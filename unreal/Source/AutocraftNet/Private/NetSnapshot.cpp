#include "NetSnapshot.h"

#include "NetCodec.h"
#include "NetTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ac::net {

using namespace codec;

namespace {

/// What an item is; its key is unique within its kind.
enum class Kind : uint8_t {
    unit = 1,
    structure = 2,
    playerCore = 3,
    playerKind = 4,
    stamp = 5,
    patch = 6,
    well = 7,
    fogChunk = 8,
    site = 9,
    removed = 10,
};
constexpr int kKindBits = 4;
/// A changed position closer than this many centimetres on both axes goes as
/// the step from its baseline.
constexpr int kStepBits = 12;
constexpr int64_t kStepMax = (int64_t(1) << (kStepBits - 1)) - 1;
/// Versions of an item a reader keeps, at most.
constexpr size_t kVersions = 32;
/// Removed ids a snapshot carries, at most.
constexpr size_t kMaxRemoved = 256;
/// Acks older than this find nothing to apply to.
constexpr double kRecordSeconds = 2;
/// Nothing acked for this long: everything goes whole again.
constexpr double kResetSeconds = 1;

uint64_t keyOf(Kind k, uint64_t v) { return (uint64_t(k) << 56) | (v & ((uint64_t(1) << 56) - 1)); }
uint64_t kindKey(int64_t player, UnitKind k) { return uint64_t(player) * 16 + uint64_t(k); }
uint64_t stampKey(int64_t player, UnitKind k, size_t i) { return kindKey(player, k) * 16 + i; }

int64_t signExtend(uint64_t v, int bits) {
    const uint64_t sign = uint64_t(1) << (bits - 1);
    return static_cast<int64_t>((v ^ sign) - sign);
}

/// `n` (≤ 64) bits of `b` from bit `from`.
uint64_t bitsAt(const std::vector<uint8_t>& b, size_t from, int n) {
    uint64_t v = 0;
    int got = 0;
    while (got < n) {
        const size_t index = from / 8;
        const int offset = static_cast<int>(from % 8);
        const int k = std::min(8 - offset, n - got);
        const uint64_t byte = index < b.size() ? b[index] : 0;
        v |= ((byte >> offset) & ((uint64_t(1) << k) - 1)) << got;
        got += k;
        from += size_t(k);
    }
    return v;
}

bool rangeEqual(const std::vector<uint8_t>& a, size_t af, const std::vector<uint8_t>& b, size_t bf, size_t n) {
    while (n > 0) {
        const int k = static_cast<int>(std::min<size_t>(n, 64));
        if (bitsAt(a, af, k) != bitsAt(b, bf, k)) return false;
        af += size_t(k);
        bf += size_t(k);
        n -= size_t(k);
    }
    return true;
}

template <class C> constexpr bool isPos = requires { C::position; };

/// An item's state on the wire: its fields written whole, one after the
/// other, and where each starts. A fog chunk: its cells, a byte each.
struct ItemBits {
    std::vector<uint8_t> bytes;
    size_t bits = 0;
    std::vector<uint32_t> offsets;
    uint64_t positions = 0;
    bool operator==(const ItemBits& o) const { return bits == o.bits && bytes == o.bytes; }
};

struct Canon {
    BitWriter w;
    std::vector<uint32_t> offsets;
    uint64_t positions = 0;
    template <class C, class T> void operator()(const C& c, const T& v) {
        if constexpr (isPos<C>) positions |= uint64_t(1) << offsets.size();
        offsets.push_back(static_cast<uint32_t>(w.bitCount()));
        c.write(w, v);
    }
};

template <class T> std::shared_ptr<const ItemBits> canon(const T& x) {
    Canon c;
    fields(c, x);
    auto b = std::make_shared<ItemBits>();
    b->offsets = std::move(c.offsets);
    b->offsets.push_back(static_cast<uint32_t>(c.w.bitCount()));
    b->bits = c.w.bitCount();
    b->bytes = c.w.bytes();
    b->positions = c.positions;
    return b;
}

void writeWhole(BitWriter& w, const ItemBits& cur) { appendBits(w, cur.bytes.data(), 0, cur.bits); }

/// The fields that changed, in groups of eight: a bit for the group, then
/// a bit for each of its fields, then the changed ones.
void writeDelta(BitWriter& w, const ItemBits& cur, const ItemBits& base) {
    const size_t n = cur.offsets.size() - 1;
    for (size_t g = 0; g < n; g += 8) {
        const size_t m = std::min<size_t>(8, n - g);
        uint32_t mask = 0;
        for (size_t i = 0; i < m; i++) {
            const size_t f = g + i;
            const size_t lc = cur.offsets[f + 1] - cur.offsets[f], lb = base.offsets[f + 1] - base.offsets[f];
            if (lc != lb || !rangeEqual(cur.bytes, cur.offsets[f], base.bytes, base.offsets[f], lc)) mask |= 1u << i;
        }
        w.writeBool(mask != 0);
        if (!mask) continue;
        w.writeBits(mask, static_cast<int>(m));
        for (size_t i = 0; i < m; i++) {
            if (!(mask & (1u << i))) continue;
            const size_t f = g + i;
            if (cur.positions & (uint64_t(1) << f)) {
                const int pb = q::kPositionBits;
                const int64_t cx = signExtend(bitsAt(cur.bytes, cur.offsets[f], pb), pb);
                const int64_t cy = signExtend(bitsAt(cur.bytes, cur.offsets[f] + size_t(pb), pb), pb);
                const int64_t bx = signExtend(bitsAt(base.bytes, base.offsets[f], pb), pb);
                const int64_t by = signExtend(bitsAt(base.bytes, base.offsets[f] + size_t(pb), pb), pb);
                const int64_t dx = cx - bx, dy = cy - by;
                const bool step = std::abs(dx) <= kStepMax && std::abs(dy) <= kStepMax;
                w.writeBool(step);
                if (step) {
                    w.writeBits(static_cast<uint64_t>(dx), kStepBits);
                    w.writeBits(static_cast<uint64_t>(dy), kStepBits);
                    continue;
                }
            }
            appendBits(w, cur.bytes.data(), cur.offsets[f], cur.offsets[f + 1] - cur.offsets[f]);
        }
    }
}

struct DeltaRead {
    BitReader& r;
    size_t n;
    size_t i = 0;
    uint32_t mask = 0;
    template <class C, class T> void operator()(const C& c, T& v) {
        if (i % 8 == 0) {
            const size_t m = std::min<size_t>(8, n - i);
            mask = r.readBool() ? static_cast<uint32_t>(r.readBits(static_cast<int>(m))) : 0;
        }
        const bool changed = (mask >> (i % 8)) & 1;
        i += 1;
        if (!changed) return;
        if constexpr (isPos<C>) {
            if (r.readBool()) {
                const int64_t dx = signExtend(r.readBits(kStepBits), kStepBits);
                const int64_t dy = signExtend(r.readBits(kStepBits), kStepBits);
                v = Vec2(double(q::centimetres(v.x) + dx) / 100, double(q::centimetres(v.y) + dy) / 100);
                return;
            }
        }
        c.read(r, v);
    }
};

// MARK: - Fog

/// A cell: seen, near, explored, as bits 0, 1, 2.
std::shared_ptr<const ItemBits> fogChunk(const FogView& f, size_t chunk) {
    auto b = std::make_shared<ItemBits>();
    const size_t from = chunk * kFogChunkCells, to = std::min(f.seen.size(), from + kFogChunkCells);
    b->bytes.resize(to - from);
    for (size_t i = from; i < to; i++) {
        const bool explored = i / 64 < f.explored.size() && ((f.explored[i / 64] >> (i % 64)) & 1);
        b->bytes[i - from] = static_cast<uint8_t>((f.seen[i] ? 1 : 0) | (i < f.near.size() && f.near[i] ? 2 : 0) | (explored ? 4 : 0));
    }
    b->bits = b->bytes.size() * 8;
    return b;
}

/// The cells as runs of their difference from the baseline (or from
/// nothing): the difference in 3 bits, the run's length less one.
void writeFog(BitWriter& w, const ItemBits& cur, const ItemBits* base) {
    const size_t n = cur.bytes.size();
    writeVarint(w, n);
    size_t i = 0;
    auto diff = [&](size_t k) {
        const uint8_t b = base && k < base->bytes.size() ? base->bytes[k] : 0;
        return static_cast<uint8_t>(cur.bytes[k] ^ b);
    };
    while (i < n) {
        const uint8_t d = diff(i);
        size_t j = i + 1;
        while (j < n && diff(j) == d) j++;
        w.writeBits(d, 3);
        writeVarint(w, j - i - 1);
        i = j;
    }
}

bool readFog(BitReader& r, std::vector<uint8_t>& cells) {
    const uint64_t n = readVarint(r);
    if (n > kFogChunkCells) return false;
    cells.resize(size_t(n), 0);
    size_t i = 0;
    while (i < n && r.ok()) {
        const uint8_t d = static_cast<uint8_t>(r.readBits(3));
        const uint64_t run = readVarint(r) + 1;
        if (run > n - i) return false;
        for (size_t k = 0; k < run; k++) cells[i + k] ^= d;
        i += size_t(run);
    }
    return r.ok();
}

// MARK: - The header

void writeHeader(BitWriter& w, int64_t id, size_t part, const Snapshot& s, size_t cells, size_t sites) {
    w.writeU8(static_cast<uint8_t>(MessageType::snapshot));
    writeVarint(w, static_cast<uint64_t>(id));
    w.writeBits(part, 4);
    writeSigned(w, s.serverTick);
    writeSigned(w, s.lastInputTick);
    writeSigned(w, s.game);
    kMs.write(w, s.time);
    Opt<Var>{}.write(w, s.driven);
    Opt<Var>{}.write(w, s.winner);
    Opt<Fixed>{kMs}.write(w, s.endedAt);
    Vec<Var>{}.write(w, s.score);
    w.writeBool(s.sky.has_value());
    if (s.sky) {
        kMs.write(w, s.sky->time);
        kMs.write(w, s.sky->dayLength);
        Opt<Fixed>{kFine}.write(w, s.sky->heldHour);
        w.writeBits(s.sky->weatherSeed, 32);
        Opt<Fixed>{kFine}.write(w, s.sky->weatherHold);
        Enum<WeatherState, 2>{}.write(w, s.sky->weather);
    }
    Opt<Fixed>{kMs}.write(w, s.xp);
    writeVarint(w, s.players.size());
    w.writeBool(s.fog.has_value());
    if (s.fog) {
        writeVarint(w, cells);
        writeVarint(w, sites);
    }
}

PlayerCore coreOf(const Player& p) {
    PlayerCore c;
    c.player = p;
    c.player.pilot = std::nullopt;
    c.pilot = p.pilot.has_value();
    if (p.pilot)
        for (const auto& [k, rec] : p.pilot->kinds) c.kinds |= uint64_t(1) << static_cast<unsigned>(k);
    return c;
}

KindCore kindOf(const KindRecord& r) {
    KindCore c;
    c.record = r;
    c.record.stamps.clear();
    c.stamps = static_cast<int64_t>(r.stamps.size());
    return c;
}

template <class T> T roundTrip(const T& x) {
    const auto b = canon(x);
    BitReader r(b->bytes.data(), b->bytes.size());
    ReadAll a{r};
    T out{};
    fields(a, out);
    return out;
}

} // namespace

// MARK: - Quantised

Unit quantised(const Unit& u) { return roundTrip(u); }
Structure quantised(const Structure& b) { return roundTrip(b); }
OreDeposit quantised(const OreDeposit& p) { return roundTrip(p); }
Well quantised(const Well& w) { return roundTrip(w); }
double quantisedTime(double t) { return roundTrip(SiteTime{t}).at; }

Player quantised(const Player& p) {
    Player out = roundTrip(coreOf(p)).player;
    if (p.pilot) {
        PilotRecord rec;
        for (const auto& [k, kr] : p.pilot->kinds) {
            KindRecord q = roundTrip(kindOf(kr)).record;
            for (const Stamp& s : kr.stamps) q.stamps.push_back(roundTrip(s));
            rec.kinds[k] = q;
        }
        out.pilot = rec;
    }
    return out;
}

FogView quantised(const FogView& f) {
    FogView out;
    const size_t n = f.seen.size();
    out.seen.resize(n);
    out.near.resize(n);
    out.explored.assign((n + 63) / 64, 0);
    for (size_t i = 0; i < n; i++) {
        out.seen[i] = f.seen[i] ? 1 : 0;
        out.near[i] = i < f.near.size() && f.near[i] ? 1 : 0;
        if (i / 64 < f.explored.size() && ((f.explored[i / 64] >> (i % 64)) & 1)) out.explored[i / 64] |= uint64_t(1) << (i % 64);
    }
    for (double s : f.sites) out.sites.push_back(quantisedTime(s));
    return out;
}

// MARK: - The writer

struct SnapshotWriter::State {
    struct Known {
        uint64_t generation = 0;
        std::shared_ptr<const ItemBits> base;
        int64_t baseId = 0;
        int64_t lastSentId = 0;
    };
    struct Sent {
        uint64_t key;
        uint64_t generation;
        std::shared_ptr<const ItemBits> bits;
    };
    struct Part {
        std::vector<Sent> items;
        std::vector<std::pair<int64_t, uint64_t>> removed;
        bool acked = false;
    };
    struct Record {
        double at = 0;
        std::vector<Part> parts;
    };

    std::unordered_map<uint64_t, Known> known;
    /// Removed ids not yet acked, with the generation of their removal.
    std::map<int64_t, uint64_t> pendingRemoved;
    std::map<int64_t, Record> records;
    int64_t nextId = 1;
    uint64_t generation = 0;
    double lastAck = -1;
    int64_t game = -1;
    int64_t fullResets = 0;

    void forgetAll() {
        known.clear();
        pendingRemoved.clear();
        records.clear();
    }
};

SnapshotWriter::SnapshotWriter() : s_(std::make_unique<State>()) {}
SnapshotWriter::~SnapshotWriter() = default;
SnapshotWriter::SnapshotWriter(SnapshotWriter&&) noexcept = default;
SnapshotWriter& SnapshotWriter::operator=(SnapshotWriter&&) noexcept = default;

int64_t SnapshotWriter::fullResets() const { return s_->fullResets; }

SnapshotWriter::Output SnapshotWriter::write(const Snapshot& snap, double now, int64_t budget) {
    State& st = *s_;
    Output out;
    if (snap.game != st.game) {
        st.forgetAll();
        st.game = snap.game;
    }
    if (st.lastAck < 0) st.lastAck = now;
    if (now - st.lastAck > kResetSeconds) {
        for (auto& [key, k] : st.known) {
            k.base = nullptr;
            k.baseId = 0;
            k.lastSentId = 0;
        }
        st.records.clear();
        st.lastAck = now;
        st.fullResets += 1;
    }
    for (int64_t id : snap.removed) {
        st.known.erase(keyOf(Kind::unit, uint64_t(id)));
        st.known.erase(keyOf(Kind::structure, uint64_t(id)));
        st.pendingRemoved[id] = ++st.generation;
    }
    for (const EntityState& e : snap.entities) st.pendingRemoved.erase(e.id());

    const int64_t id = st.nextId++;
    const size_t cells = snap.fog ? snap.fog->seen.size() : 0;
    const size_t sites = snap.fog ? snap.fog->sites.size() : 0;

    // The messages: closed ones, and the one being filled.
    State::Record record;
    record.at = now;
    std::vector<BitWriter> messages;
    // What the messages before the last cost the budget.
    int64_t closedCost = 0;
    const size_t limit = kMaxUnreliableBytes * 8;
    auto cost = [](const BitWriter& w, size_t more) { return kOverhead + int64_t((w.bitCount() + more + 1 + 7) / 8); };
    auto openPart = [&]() -> bool {
        if (messages.size() >= kMaxSnapshotParts) return false;
        BitWriter w;
        writeHeader(w, id, messages.size(), snap, cells, sites);
        const int64_t closed = closedCost + (messages.empty() ? 0 : cost(messages.back(), 0));
        if (closed + cost(w, 0) > budget) return false;
        closedCost = closed;
        messages.push_back(std::move(w));
        record.parts.emplace_back();
        return true;
    };
    // Appends an item where it fits, in a new part if need be; false if the
    // budget or the parts run out.
    auto place = [&](const BitWriter& item) -> bool {
        if (messages.back().bitCount() + item.bitCount() + 1 > limit && !openPart()) return false;
        BitWriter& cur = messages.back();
        if (cur.bitCount() + item.bitCount() + 1 > limit || closedCost + cost(cur, item.bitCount()) > budget) return false;
        appendBits(cur, item.bytes().data(), 0, item.bitCount());
        return true;
    };
    if (!openPart()) return out;

    enum class Placed { written, unchanged, noRoom };
    auto consider = [&](Kind kind, uint64_t keyValue, const std::shared_ptr<const ItemBits>& bits,
                        std::optional<bool> remembered = std::nullopt) -> Placed {
        const uint64_t key = keyOf(kind, keyValue);
        auto [it, fresh] = st.known.try_emplace(key);
        State::Known& k = it->second;
        if (fresh) k.generation = ++st.generation;
        if (k.base && k.lastSentId == k.baseId && *k.base == *bits) {
            out.unchanged += 1;
            return Placed::unchanged;
        }
        BitWriter item;
        item.writeBool(true);
        item.writeBits(static_cast<uint64_t>(kind), kKindBits);
        writeVarint(item, keyValue);
        if (remembered) item.writeBool(*remembered);
        const bool delta = k.base != nullptr;
        item.writeBool(delta);
        if (delta) writeVarint(item, static_cast<uint64_t>(id - k.baseId));
        if (kind == Kind::fogChunk) writeFog(item, *bits, k.base.get());
        else if (delta) writeDelta(item, *bits, *k.base);
        else writeWhole(item, *bits);
        if (!place(item)) return Placed::noRoom;
        k.lastSentId = id;
        record.parts[messages.size() - 1].items.push_back(State::Sent{key, k.generation, bits});
        (delta ? out.deltas : out.whole) += 1;
        return Placed::written;
    };
    auto entity = [&](const EntityState& e) {
        Placed p;
        if (const Unit* u = e.unit()) p = consider(Kind::unit, uint64_t(u->id), canon(*u), e.remembered);
        else p = consider(Kind::structure, uint64_t(e.id()), canon(*e.structure()), e.remembered);
        if (p == Placed::noRoom) out.deferred.push_back(e.id());
    };

    // The driven unit and what it fights; what is gone; the players; the
    // ground in view; the fog; then the rest by distance.
    for (const EntityState& e : snap.entities)
        if (e.rank != Rank::nearby) entity(e);
    if (!st.pendingRemoved.empty()) {
        BitWriter item;
        item.writeBool(true);
        item.writeBits(static_cast<uint64_t>(Kind::removed), kKindBits);
        writeVarint(item, 0);
        std::vector<std::pair<int64_t, uint64_t>> ids;
        for (const auto& [rid, gen] : st.pendingRemoved) {
            if (ids.size() >= kMaxRemoved) break;
            ids.emplace_back(rid, gen);
        }
        writeVarint(item, ids.size());
        int64_t last = 0;
        for (const auto& [rid, gen] : ids) {
            writeSigned(item, rid - last);
            last = rid;
        }
        if (place(item)) record.parts[messages.size() - 1].removed = std::move(ids);
    }
    for (size_t p = 0; p < snap.players.size(); p++) {
        const Player& pl = snap.players[p];
        consider(Kind::playerCore, p, canon(coreOf(pl)));
        if (!pl.pilot) continue;
        for (const auto& [k, rec] : pl.pilot->kinds) {
            consider(Kind::playerKind, kindKey(int64_t(p), k), canon(kindOf(rec)));
            for (size_t i = 0; i < rec.stamps.size() && i < 16; i++)
                consider(Kind::stamp, stampKey(int64_t(p), k, i), canon(rec.stamps[i]));
        }
    }
    for (const PatchView& v : snap.patches) consider(Kind::patch, uint64_t(v.index), canon(v.patch));
    for (const WellView& v : snap.wells) consider(Kind::well, uint64_t(v.index), canon(v.well));
    if (snap.fog) {
        const size_t chunks = (cells + kFogChunkCells - 1) / kFogChunkCells;
        for (size_t c = 0; c < chunks; c++) consider(Kind::fogChunk, c, fogChunk(*snap.fog, c));
        for (size_t i = 0; i < sites; i++) consider(Kind::site, i, canon(SiteTime{snap.fog->sites[i]}));
    }
    for (const EntityState& e : snap.entities)
        if (e.rank == Rank::nearby) entity(e);

    out.id = id;
    for (BitWriter& w : messages) {
        w.writeBool(false);
        out.messages.push_back(w.bytes());
    }
    st.records[id] = std::move(record);
    while (!st.records.empty() && st.records.begin()->second.at < now - kRecordSeconds) st.records.erase(st.records.begin());
    return out;
}

void SnapshotWriter::ack(const AckMessage& m, double now) {
    State& st = *s_;
    for (const AckMessage::Received& rcv : m.received) {
        const auto rec = st.records.find(rcv.snapshot);
        if (rec == st.records.end()) continue;
        for (size_t p = 0; p < rec->second.parts.size(); p++) {
            State::Part& part = rec->second.parts[p];
            if (!(rcv.parts & (uint32_t(1) << p)) || part.acked) continue;
            part.acked = true;
            st.lastAck = now;
            for (const State::Sent& s : part.items) {
                const auto k = st.known.find(s.key);
                if (k == st.known.end() || k->second.generation != s.generation || rcv.snapshot <= k->second.baseId) continue;
                k->second.base = s.bits;
                k->second.baseId = rcv.snapshot;
            }
            for (const auto& [rid, gen] : part.removed) {
                const auto r = st.pendingRemoved.find(rid);
                if (r != st.pendingRemoved.end() && r->second == gen) st.pendingRemoved.erase(r);
            }
        }
    }
}

// MARK: - The reader

namespace {

/// Each item's versions by snapshot id, oldest first.
template <class T> struct Store {
    std::unordered_map<uint64_t, std::deque<std::pair<int64_t, T>>> items;

    const T* at(uint64_t key, int64_t id) const {
        const auto it = items.find(key);
        if (it == items.end()) return nullptr;
        for (const auto& [v, x] : it->second)
            if (v == id) return &x;
        return nullptr;
    }
    const T* latest(uint64_t key) const {
        const auto it = items.find(key);
        return it == items.end() || it->second.empty() ? nullptr : &it->second.back().second;
    }
    void put(uint64_t key, int64_t id, const T& x) {
        auto& d = items[key];
        if (!d.empty() && d.back().first == id) d.pop_back();
        d.emplace_back(id, x);
        while (d.size() > kVersions) d.pop_front();
    }
    /// The server will not name a baseline older than `id` again.
    void prune(uint64_t key, int64_t id) {
        const auto it = items.find(key);
        if (it == items.end()) return;
        while (!it->second.empty() && it->second.front().first < id) it->second.pop_front();
    }
    void erase(uint64_t key) { items.erase(key); }
};

} // namespace

struct SnapshotReader::State {
    Store<Unit> units;
    Store<Structure> structures;
    Store<PlayerCore> cores;
    Store<KindCore> kinds;
    Store<Stamp> stamps;
    Store<OreDeposit> patches;
    Store<Well> wells;
    Store<std::vector<uint8_t>> fog;
    Store<SiteTime> sites;
    FogView fogNow;
    std::deque<AckMessage::Received> received;
    int64_t game = -1;

    /// Reads an item of a struct type into its store, whole or against its
    /// baseline. False: broken, or the baseline is gone.
    template <class T> bool item(BitReader& r, Store<T>& store, uint64_t key, int64_t id, T& out) {
        const bool delta = r.readBool();
        if (delta) {
            const int64_t baseId = id - static_cast<int64_t>(readVarint(r));
            const T* base = store.at(key, baseId);
            if (!base) return false;
            out = *base;
            DeltaRead d{r, fieldCount<T>()};
            fields(d, out);
            store.prune(key, baseId);
        } else {
            out = T{};
            ReadAll a{r};
            fields(a, out);
        }
        if (!r.ok()) return false;
        store.put(key, id, out);
        return true;
    }

    std::vector<Player> players(size_t count) const {
        std::vector<Player> out(count);
        for (size_t p = 0; p < count; p++) {
            const PlayerCore* c = cores.latest(keyOf(Kind::playerCore, p));
            if (!c) continue;
            out[p] = c->player;
            if (!c->pilot) continue;
            PilotRecord rec;
            for (unsigned k = 0; k < 16; k++) {
                if (!(c->kinds & (uint64_t(1) << k))) continue;
                const UnitKind kind = static_cast<UnitKind>(k);
                KindRecord kr;
                if (const KindCore* kc = kinds.latest(keyOf(Kind::playerKind, kindKey(int64_t(p), kind)))) {
                    kr = kc->record;
                    for (int64_t i = 0; i < kc->stamps && i < 16; i++) {
                        const Stamp* s = stamps.latest(keyOf(Kind::stamp, stampKey(int64_t(p), kind, size_t(i))));
                        kr.stamps.push_back(s ? *s : Stamp{});
                    }
                }
                rec.kinds[kind] = kr;
            }
            out[p].pilot = rec;
        }
        return out;
    }
};

SnapshotReader::SnapshotReader() : s_(std::make_unique<State>()) {}
SnapshotReader::~SnapshotReader() = default;
SnapshotReader::SnapshotReader(SnapshotReader&&) noexcept = default;
SnapshotReader& SnapshotReader::operator=(SnapshotReader&&) noexcept = default;

void SnapshotReader::reset() { *s_ = State{}; }

std::optional<SnapshotPart> SnapshotReader::read(const std::vector<uint8_t>& message) {
    State& st = *s_;
    if (message.empty() || message[0] != uint8_t(MessageType::snapshot)) return std::nullopt;
    BitReader r(message.data(), message.size());
    r.readU8();
    SnapshotPart out;
    out.id = static_cast<int64_t>(readVarint(r));
    out.part = static_cast<int>(r.readBits(4));
    Snapshot& s = out.snapshot;
    s.serverTick = readSigned(r);
    s.lastInputTick = readSigned(r);
    s.game = readSigned(r);
    kMs.read(r, s.time);
    Opt<Var>{}.read(r, s.driven);
    Opt<Var>{}.read(r, s.winner);
    Opt<Fixed>{kMs}.read(r, s.endedAt);
    Vec<Var>{}.read(r, s.score);
    if (r.readBool()) {
        Sky sky;
        kMs.read(r, sky.time);
        kMs.read(r, sky.dayLength);
        Opt<Fixed>{kFine}.read(r, sky.heldHour);
        sky.weatherSeed = static_cast<uint32_t>(r.readBits(32));
        Opt<Fixed>{kFine}.read(r, sky.weatherHold);
        Enum<WeatherState, 2>{}.read(r, sky.weather);
        s.sky = sky;
    }
    Opt<Fixed>{kMs}.read(r, s.xp);
    const uint64_t playerCount = readVarint(r);
    const bool fog = r.readBool();
    uint64_t cells = 0, sites = 0;
    if (fog) {
        cells = readVarint(r);
        sites = readVarint(r);
    }
    if (!r.ok() || playerCount > 64 || cells > (uint64_t(1) << 26) || sites > 4096) return std::nullopt;
    if (s.game != st.game) {
        const int64_t game = s.game;
        reset();
        st.game = game;
    }
    if (fog) {
        st.fogNow.seen.resize(size_t(cells), 0);
        st.fogNow.near.resize(size_t(cells), 0);
        st.fogNow.explored.resize(size_t((cells + 63) / 64), 0);
        st.fogNow.sites.resize(size_t(sites), -std::numeric_limits<double>::infinity());
    }

    bool playersChanged = false, fogChanged = false;
    while (r.readBool()) {
        if (!r.ok()) return std::nullopt;
        const Kind kind = static_cast<Kind>(r.readBits(kKindBits));
        const uint64_t key = readVarint(r);
        switch (kind) {
        case Kind::removed: {
            const uint64_t n = readVarint(r);
            if (n > kMaxRemoved) return std::nullopt;
            int64_t last = 0;
            for (uint64_t i = 0; i < n; i++) {
                last += readSigned(r);
                s.removed.push_back(last);
                st.units.erase(keyOf(Kind::unit, uint64_t(last)));
                st.structures.erase(keyOf(Kind::structure, uint64_t(last)));
            }
            break;
        }
        case Kind::unit: {
            const bool remembered = r.readBool();
            Unit u;
            if (!st.item(r, st.units, keyOf(kind, key), out.id, u)) return std::nullopt;
            s.entities.push_back(EntityState{u, remembered});
            break;
        }
        case Kind::structure: {
            const bool remembered = r.readBool();
            Structure b;
            if (!st.item(r, st.structures, keyOf(kind, key), out.id, b)) return std::nullopt;
            s.entities.push_back(EntityState{b, remembered});
            break;
        }
        case Kind::playerCore: {
            PlayerCore c;
            if (!st.item(r, st.cores, keyOf(kind, key), out.id, c)) return std::nullopt;
            playersChanged = true;
            break;
        }
        case Kind::playerKind: {
            KindCore c;
            if (!st.item(r, st.kinds, keyOf(kind, key), out.id, c)) return std::nullopt;
            playersChanged = true;
            break;
        }
        case Kind::stamp: {
            Stamp x;
            if (!st.item(r, st.stamps, keyOf(kind, key), out.id, x)) return std::nullopt;
            playersChanged = true;
            break;
        }
        case Kind::patch: {
            OreDeposit p;
            if (!st.item(r, st.patches, keyOf(kind, key), out.id, p)) return std::nullopt;
            s.patches.push_back(PatchView{int64_t(key), p});
            break;
        }
        case Kind::well: {
            Well w;
            if (!st.item(r, st.wells, keyOf(kind, key), out.id, w)) return std::nullopt;
            s.wells.push_back(WellView{int64_t(key), w});
            break;
        }
        case Kind::fogChunk: {
            const uint64_t k = keyOf(kind, key);
            const bool delta = r.readBool();
            std::vector<uint8_t> chunk;
            if (delta) {
                const int64_t baseId = out.id - static_cast<int64_t>(readVarint(r));
                const std::vector<uint8_t>* base = st.fog.at(k, baseId);
                if (!base) return std::nullopt;
                chunk = *base;
                st.fog.prune(k, baseId);
            }
            if (!readFog(r, chunk)) return std::nullopt;
            st.fog.put(k, out.id, chunk);
            const size_t from = size_t(key) * kFogChunkCells;
            for (size_t i = 0; i < chunk.size() && from + i < cells; i++) {
                const size_t c = from + i;
                st.fogNow.seen[c] = chunk[i] & 1;
                st.fogNow.near[c] = (chunk[i] >> 1) & 1;
                const uint64_t bit = uint64_t(1) << (c % 64);
                if (chunk[i] & 4) st.fogNow.explored[c / 64] |= bit;
                else st.fogNow.explored[c / 64] &= ~bit;
            }
            fogChanged = true;
            break;
        }
        case Kind::site: {
            SiteTime t;
            if (!st.item(r, st.sites, keyOf(kind, key), out.id, t)) return std::nullopt;
            if (key < st.fogNow.sites.size()) st.fogNow.sites[size_t(key)] = t.at;
            fogChanged = true;
            break;
        }
        default:
            return std::nullopt;
        }
        if (!r.ok()) return std::nullopt;
    }
    if (!r.ok()) return std::nullopt;
    if (playersChanged) s.players = st.players(size_t(playerCount));
    if (fogChanged) s.fog = st.fogNow;

    // The parts read, newest snapshots first.
    auto it = std::find_if(st.received.begin(), st.received.end(),
                           [&](const AckMessage::Received& x) { return x.snapshot == out.id; });
    if (it == st.received.end()) {
        st.received.push_front(AckMessage::Received{out.id, 0});
        std::sort(st.received.begin(), st.received.end(),
                  [](const AckMessage::Received& a, const AckMessage::Received& b) { return a.snapshot > b.snapshot; });
        while (st.received.size() > AckMessage::kMaxSnapshots) st.received.pop_back();
        it = std::find_if(st.received.begin(), st.received.end(),
                          [&](const AckMessage::Received& x) { return x.snapshot == out.id; });
    }
    if (it != st.received.end() && out.part < 32) it->parts |= uint32_t(1) << out.part;
    return out;
}

AckMessage SnapshotReader::ack() const {
    AckMessage m;
    m.received.assign(s_->received.begin(), s_->received.end());
    return m;
}

} // namespace ac::net
