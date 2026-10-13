#include "NetProtocol.h"

#include "NetCodec.h"
#include "NetTypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ac::net {

namespace {

constexpr double kTurn = 6.283185307179586;
/// The largest integer a double holds exactly.
constexpr double kExact = 9007199254740992.0;

int64_t signExtend(uint64_t v, int bits) {
    const uint64_t sign = uint64_t(1) << (bits - 1);
    return static_cast<int64_t>((v ^ sign) - sign);
}

void writeDouble(BitWriter& w, double v) {
    uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof bits);
    w.writeU64(bits);
}

double readDouble(BitReader& r) {
    const uint64_t bits = r.readU64();
    double v = 0;
    std::memcpy(&v, &bits, sizeof v);
    return v;
}

void writeString(BitWriter& w, const std::string& s) {
    writeVarint(w, s.size());
    for (char c : s) w.writeU8(static_cast<uint8_t>(c));
}

bool readString(BitReader& r, std::string& s) {
    const uint64_t n = readVarint(r);
    if (n * 8 > r.bitsLeft()) return false;
    s.resize(static_cast<size_t>(n));
    for (char& c : s) c = static_cast<char>(r.readU8());
    return r.ok();
}

BitWriter start(MessageType t) {
    BitWriter w;
    w.writeU8(static_cast<uint8_t>(t));
    return w;
}

/// A reader past the type byte, or false when the type is not `t`.
bool open(const std::vector<uint8_t>& bytes, MessageType t, BitReader& r) {
    if (bytes.empty() || bytes[0] != static_cast<uint8_t>(t)) return false;
    r.readU8();
    return true;
}

void writeInput(BitWriter& w, const PilotInput& in) {
    const PilotInput qi = quantised(in);
    writeSigned(w, std::llround(qi.walk.x * q::kWalkSteps));
    writeSigned(w, std::llround(qi.walk.y * q::kWalkSteps));
    w.writeU16(q::angle(qi.heading));
    w.writeBool(in.act);
    w.writeBool(in.ability);
    w.writeBool(in.hold);
    w.writeBool(in.mend);
    w.writeBool(in.crosshair.has_value());
    if (in.crosshair) {
        const auto* on = in.crosshair->as<Pilot::Crosshair::On>();
        w.writeBool(on != nullptr);
        if (on) writeSigned(w, on->_0);
    }
}

void readInput(BitReader& r, PilotInput& in) {
    in.walk.x = double(readSigned(r)) / q::kWalkSteps;
    in.walk.y = double(readSigned(r)) / q::kWalkSteps;
    in.heading = q::angle(r.readU16());
    in.act = r.readBool();
    in.ability = r.readBool();
    in.hold = r.readBool();
    in.mend = r.readBool();
    if (r.readBool()) {
        if (r.readBool()) {
            in.crosshair = Pilot::Crosshair(Pilot::Crosshair::On{readSigned(r)});
        } else {
            in.crosshair = Pilot::Crosshair(Pilot::Crosshair::Clear{});
        }
    } else {
        in.crosshair = std::nullopt;
    }
}

} // namespace

// MARK: - Bits

void writeVarint(BitWriter& w, uint64_t v) {
    while (v >= 0x80) {
        w.writeU8(static_cast<uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    w.writeU8(static_cast<uint8_t>(v));
}

uint64_t readVarint(BitReader& r) {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
        const uint8_t b = r.readU8();
        v |= uint64_t(b & 0x7F) << shift;
        if (!(b & 0x80) || !r.ok()) return v;
    }
    return v;
}

void writeSigned(BitWriter& w, int64_t v) {
    writeVarint(w, (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63));
}

int64_t readSigned(BitReader& r) {
    const uint64_t z = readVarint(r);
    return static_cast<int64_t>(z >> 1) ^ -static_cast<int64_t>(z & 1);
}

void appendBits(BitWriter& w, const uint8_t* src, size_t from, size_t bits) {
    // Byte by byte from the source's bit offset.
    size_t at = from;
    while (bits > 0) {
        const size_t index = at / 8;
        const int offset = static_cast<int>(at % 8);
        const int n = static_cast<int>(std::min<size_t>(bits, size_t(8 - offset)));
        w.writeBits(uint64_t(src[index] >> offset), n);
        at += size_t(n);
        bits -= size_t(n);
    }
}

std::optional<MessageType> messageType(const std::vector<uint8_t>& message) {
    if (message.empty()) return std::nullopt;
    const uint8_t t = message[0];
    if (t < uint8_t(MessageType::welcome) || t > uint8_t(MessageType::world)) return std::nullopt;
    return static_cast<MessageType>(t);
}

// MARK: - Quantising

namespace q {

int64_t centimetres(double v) { return codec::fixed(v, 100); }

uint16_t angle(double radians) {
    if (!std::isfinite(radians)) return 0;
    double turns = radians / kTurn;
    turns -= std::floor(turns);
    return static_cast<uint16_t>(static_cast<uint32_t>(std::floor(turns * 65536.0 + 0.5)) & 0xFFFF);
}

double angle(uint16_t v) { return double(v) * (kTurn / 65536.0); }

} // namespace q

PilotInput quantised(const PilotInput& in) {
    PilotInput out = in;
    auto axis = [](double v) {
        const double c = std::isfinite(v) ? std::clamp(v, -1.0, 1.0) : 0.0;
        return double(std::llround(c * q::kWalkSteps)) / q::kWalkSteps;
    };
    out.walk = Vec2(axis(in.walk.x), axis(in.walk.y));
    out.heading = q::angle(q::angle(in.heading));
    return out;
}

// MARK: - Codecs

namespace codec {

int64_t fixed(double v, double scale) {
    if (!std::isfinite(v)) return 0;
    const double x = std::clamp(v * scale, -kExact, kExact);
    return static_cast<int64_t>(std::llround(x));
}

void Pos::write(BitWriter& w, Vec2 v) const {
    const int64_t lim = (int64_t(1) << (q::kPositionBits - 1)) - 1;
    w.writeBits(static_cast<uint64_t>(std::clamp(q::centimetres(v.x), -lim, lim)), q::kPositionBits);
    w.writeBits(static_cast<uint64_t>(std::clamp(q::centimetres(v.y), -lim, lim)), q::kPositionBits);
}

void Pos::read(BitReader& r, Vec2& v) const {
    const int64_t x = signExtend(r.readBits(q::kPositionBits), q::kPositionBits);
    const int64_t y = signExtend(r.readBits(q::kPositionBits), q::kPositionBits);
    v = Vec2(double(x) / 100, double(y) / 100);
}

void Time::write(BitWriter& w, double v) const {
    // 0: −∞, 1: +∞, else the milliseconds after 2.
    if (std::isinf(v)) {
        writeVarint(w, v < 0 ? 0 : 1);
        return;
    }
    writeVarint(w, 2);
    writeSigned(w, fixed(v, 1000));
}

void Time::read(BitReader& r, double& v) const {
    const uint64_t tag = readVarint(r);
    if (tag == 0) v = -std::numeric_limits<double>::infinity();
    else if (tag == 1) v = std::numeric_limits<double>::infinity();
    else v = double(readSigned(r)) / 1000;
}

void Health::write(BitWriter& w, double v) const {
    const double steps = double((1 << q::kHealthBits) - 1);
    if (std::isfinite(v) && v >= 0 && v <= range && range > 0) {
        w.writeBool(false);
        w.writeBits(static_cast<uint64_t>(std::llround(v / range * steps)), q::kHealthBits);
    } else {
        w.writeBool(true);
        writeDouble(w, v);
    }
}

void Health::read(BitReader& r, double& v) const {
    const double steps = double((1 << q::kHealthBits) - 1);
    if (r.readBool()) {
        v = readDouble(r);
    } else {
        v = double(r.readBits(q::kHealthBits)) * range / steps;
    }
}

void Upgrades::write(BitWriter& w, const std::set<Upgrade>& v) const {
    uint64_t bits = 0;
    for (Upgrade u : v) bits |= uint64_t(1) << static_cast<unsigned>(u);
    w.writeBits(bits, 4);
}

void Upgrades::read(BitReader& r, std::set<Upgrade>& v) const {
    const uint64_t bits = r.readBits(4);
    v.clear();
    for (unsigned i = 0; i < 4; i++)
        if (bits & (uint64_t(1) << i)) v.insert(static_cast<Upgrade>(i));
}

void MissionCodec::write(BitWriter& w, const Mission& m) const {
    w.writeBits(m.index(), 3);
    std::visit(
        [&](const auto& c) {
            using C = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<C, Mission::Follow>) {
                writeSigned(w, c.unit);
            } else if constexpr (std::is_same_v<C, Mission::Assault> || std::is_same_v<C, Mission::Hold> ||
                                 std::is_same_v<C, Mission::Regroup>) {
                writeSigned(w, c.objective);
                Pos{}.write(w, c.at);
            } else {
                Pos{}.write(w, c.at);
            }
        },
        m.value);
}

void MissionCodec::read(BitReader& r, Mission& m) const {
    const uint64_t i = r.readBits(3);
    auto at = [&] {
        Vec2 v;
        Pos{}.read(r, v);
        return v;
    };
    switch (i) {
    case 0: m = Mission(Mission::Raid{at()}); break;
    case 1: m = Mission(Mission::Drop{at()}); break;
    case 2: m = Mission(Mission::FallBack{at()}); break;
    case 3: m = Mission(Mission::Hunt{at()}); break;
    case 4: { const int64_t o = readSigned(r); m = Mission(Mission::Assault{o, at()}); break; }
    case 5: { const int64_t o = readSigned(r); m = Mission(Mission::Hold{o, at()}); break; }
    case 6: { const int64_t o = readSigned(r); m = Mission(Mission::Regroup{o, at()}); break; }
    default: m = Mission(Mission::Follow{readSigned(r)}); break;
    }
}

void RequestWhat::write(BitWriter& w, const Request::What& x) const {
    w.writeBits(x.index(), 2);
    if (const auto* u = x.as<Request::What::Unit>()) w.writeBits(static_cast<uint64_t>(u->kind), 4);
    if (const auto* b = x.as<Request::What::Building>()) w.writeBits(static_cast<uint64_t>(b->kind), 4);
    if (const auto* g = x.as<Request::What::Upgrade>()) w.writeBits(static_cast<uint64_t>(g->upgrade), 2);
}

void RequestWhat::read(BitReader& r, Request::What& x) const {
    const uint64_t i = r.readBits(2);
    if (i == 0) x = Request::What(Request::What::Unit{static_cast<UnitKind>(r.readBits(4))});
    else if (i == 1) x = Request::What(Request::What::Building{static_cast<StructureKind>(r.readBits(4))});
    else x = Request::What(Request::What::Upgrade{static_cast<Upgrade>(r.readBits(2))});
}

} // namespace codec

// MARK: - Events

void writeEvent(BitWriter& w, const GameEvent& e) {
    using codec::Pos;
    const Pos pos;
    auto id = [&](int64_t v) { writeSigned(w, v); };
    w.writeBits(e.index(), 5);
    std::visit(
        [&](const auto& x) {
            using E = std::decay_t<decltype(x)>;
            using G = GameEvent;
            auto unitKind = [&](UnitKind k) { w.writeBits(uint64_t(k), 4); };
            auto structureKind = [&](StructureKind k) { w.writeBits(uint64_t(k), 4); };
            if constexpr (std::is_same_v<E, G::Deposited>) { id(x.unit); id(x.amount); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::PatchDepleted> || std::is_same_v<E, G::PatchRegrown>) { id(x.patch); }
            else if constexpr (std::is_same_v<E, G::Trained>) { id(x.unit); unitKind(x.kind); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::ConstructionStarted> || std::is_same_v<E, G::Constructed>) { id(x.structure); structureKind(x.kind); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::Shot>) { id(x.unit); id(x.target); id(x.victim); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::Missed>) { id(x.unit); pos.write(w, x.at); w.writeU16(q::angle(x.yaw)); w.writeU16(q::angle(x.pitch)); }
            else if constexpr (std::is_same_v<E, G::Landed>) { id(x.unit); id(x.target); }
            else if constexpr (std::is_same_v<E, G::Died> || std::is_same_v<E, G::Stuck> || std::is_same_v<E, G::Trapped>) { id(x.unit); unitKind(x.kind); id(x.owner); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::Destroyed>) { id(x.structure); structureKind(x.kind); id(x.owner); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::Victory>) { id(x.winner); }
            else if constexpr (std::is_same_v<E, G::Researched>) { w.writeBits(uint64_t(x.upgrade), 2); id(x.owner); pos.write(w, x.at); }
            else if constexpr (std::is_same_v<E, G::LeveledUp>) { unitKind(x.kind); id(x.level); }
            else if constexpr (std::is_same_v<E, G::Blast> || std::is_same_v<E, G::Stomp>) { id(x.unit); pos.write(w, x.at); codec::kCm.write(w, x.radius); }
            else static_assert(sizeof(E) == 0, "every event has its encoding");
        },
        e.value);
}

bool readEvent(BitReader& r, GameEvent& e) {
    using codec::Pos;
    const Pos pos;
    auto id = [&] { return readSigned(r); };
    auto at = [&] { Vec2 v; pos.read(r, v); return v; };
    auto unitKind = [&] { return static_cast<UnitKind>(r.readBits(4)); };
    auto structureKind = [&] { return static_cast<StructureKind>(r.readBits(4)); };
    auto radius = [&] { double v = 0; codec::kCm.read(r, v); return v; };
    switch (r.readBits(5)) {
    case 0: { auto u = id(); auto a = id(); e = GameEvent(GameEvent::Deposited{u, a, at()}); break; }
    case 1: e = GameEvent(GameEvent::PatchDepleted{id()}); break;
    case 2: e = GameEvent(GameEvent::PatchRegrown{id()}); break;
    case 3: { auto u = id(); auto k = unitKind(); e = GameEvent(GameEvent::Trained{u, k, at()}); break; }
    case 4: { auto s = id(); auto k = structureKind(); e = GameEvent(GameEvent::ConstructionStarted{s, k, at()}); break; }
    case 5: { auto s = id(); auto k = structureKind(); e = GameEvent(GameEvent::Constructed{s, k, at()}); break; }
    case 6: { auto u = id(); auto t = id(); auto v = id(); e = GameEvent(GameEvent::Shot{u, t, v, at()}); break; }
    case 7: {
        auto u = id();
        auto a = at();
        const double yaw = q::angle(r.readU16());
        const double pitch = q::angle(r.readU16());
        e = GameEvent(GameEvent::Missed{u, a, yaw, pitch});
        break;
    }
    case 8: { auto u = id(); auto t = id(); e = GameEvent(GameEvent::Landed{u, t}); break; }
    case 9: { auto u = id(); auto k = unitKind(); auto o = id(); e = GameEvent(GameEvent::Died{u, k, o, at()}); break; }
    case 10: { auto s = id(); auto k = structureKind(); auto o = id(); e = GameEvent(GameEvent::Destroyed{s, k, o, at()}); break; }
    case 11: e = GameEvent(GameEvent::Victory{id()}); break;
    case 12: { auto g = static_cast<Upgrade>(r.readBits(2)); auto o = id(); e = GameEvent(GameEvent::Researched{g, o, at()}); break; }
    case 13: { auto u = id(); auto k = unitKind(); auto o = id(); e = GameEvent(GameEvent::Stuck{u, k, o, at()}); break; }
    case 14: { auto u = id(); auto k = unitKind(); auto o = id(); e = GameEvent(GameEvent::Trapped{u, k, o, at()}); break; }
    case 15: { auto k = unitKind(); auto l = id(); e = GameEvent(GameEvent::LeveledUp{k, l}); break; }
    case 16: { auto u = id(); auto a = at(); e = GameEvent(GameEvent::Blast{u, a, radius()}); break; }
    case 17: { auto u = id(); auto a = at(); e = GameEvent(GameEvent::Stomp{u, a, radius()}); break; }
    default: return false;
    }
    return r.ok();
}

// MARK: - Messages

void writeRegion(BitWriter& w, const RegionDescription& d) {
    w.writeBits(uint64_t(d.map.style), 4);
    w.writeBits(uint64_t(d.map.size), 4);
    writeVarint(w, static_cast<uint64_t>(d.map.players));
    writeString(w, d.regionFile);
    w.writeBool(d.wild);
    writeString(w, d.mapName);
    writeSigned(w, d.mapVersion);
    w.writeU64(d.seed);
    w.writeBool(d.fog);
    writeDouble(w, d.step);
}

bool readRegion(BitReader& r, RegionDescription& d) {
    d.map.style = static_cast<MapStyle>(r.readBits(4));
    d.map.size = static_cast<MapSize>(r.readBits(4));
    d.map.players = static_cast<int64_t>(readVarint(r));
    if (!readString(r, d.regionFile)) return false;
    d.wild = r.readBool();
    if (!readString(r, d.mapName)) return false;
    d.mapVersion = readSigned(r);
    d.seed = r.readU64();
    d.fog = r.readBool();
    d.step = readDouble(r);
    return r.ok() && size_t(d.map.style) < 4 && size_t(d.map.size) < 4;
}

std::vector<uint8_t> encode(const Welcome& m) {
    BitWriter w = start(MessageType::welcome);
    writeVarint(w, m.pilot);
    writeRegion(w, m.region);
    writeSigned(w, m.serverTick);
    writeDouble(w, m.time);
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, Welcome& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::welcome, r)) return false;
    m.pilot = readVarint(r);
    if (!readRegion(r, m.region)) return false;
    m.serverTick = readSigned(r);
    m.time = readDouble(r);
    return r.ok();
}

std::vector<uint8_t> encode(const RegionChange& m) {
    BitWriter w = start(MessageType::regionChange);
    writeSigned(w, m.visit);
    writeRegion(w, m.region);
    writeString(w, m.regionId);
    w.writeBool(m.driven.has_value());
    if (m.driven) writeSigned(w, *m.driven);
    writeSigned(w, m.serverTick);
    writeDouble(w, m.time);
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, RegionChange& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::regionChange, r)) return false;
    m.visit = readSigned(r);
    if (!readRegion(r, m.region) || !readString(r, m.regionId)) return false;
    m.driven = std::nullopt;
    if (r.readBool()) m.driven = readSigned(r);
    m.serverTick = readSigned(r);
    m.time = readDouble(r);
    return r.ok();
}

std::vector<uint8_t> encode(const WorldMessage& m) {
    BitWriter w = start(MessageType::world);
    writeVarint(w, static_cast<uint64_t>(m.columns));
    writeVarint(w, static_cast<uint64_t>(m.rows));
    writeVarint(w, m.regions.size());
    for (const WorldMessage::Region& g : m.regions) {
        writeString(w, g.id);
        writeString(w, g.name);
        w.writeBits(uint64_t(g.style), 4);
        w.writeBits(uint64_t(g.size), 4);
        writeVarint(w, static_cast<uint64_t>(g.column));
        writeVarint(w, static_cast<uint64_t>(g.row));
        writeVarint(w, g.pads.size());
        for (const WorldMessage::Pad& p : g.pads) {
            writeString(w, p.name);
            writeDouble(w, p.at.x);
            writeDouble(w, p.at.y);
        }
    }
    writeVarint(w, static_cast<uint64_t>(m.here));
    w.writeBool(m.flyingTo.has_value());
    if (m.flyingTo) writeVarint(w, static_cast<uint64_t>(*m.flyingTo));
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, WorldMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::world, r)) return false;
    m.columns = static_cast<int64_t>(readVarint(r));
    m.rows = static_cast<int64_t>(readVarint(r));
    const uint64_t n = readVarint(r);
    if (n > 4096 || m.columns > 4096 || m.rows > 4096) return false;
    m.regions.assign(size_t(n), {});
    for (WorldMessage::Region& g : m.regions) {
        if (!readString(r, g.id) || !readString(r, g.name)) return false;
        g.style = static_cast<MapStyle>(r.readBits(4));
        g.size = static_cast<MapSize>(r.readBits(4));
        if (size_t(g.style) >= 4 || size_t(g.size) >= 4) return false;
        g.column = static_cast<int64_t>(readVarint(r));
        g.row = static_cast<int64_t>(readVarint(r));
        const uint64_t pads = readVarint(r);
        if (pads > 256 || !r.ok()) return false;
        g.pads.assign(size_t(pads), {});
        for (WorldMessage::Pad& p : g.pads) {
            if (!readString(r, p.name)) return false;
            p.at.x = readDouble(r);
            p.at.y = readDouble(r);
        }
    }
    m.here = static_cast<int64_t>(readVarint(r));
    m.flyingTo = std::nullopt;
    if (r.readBool()) m.flyingTo = static_cast<int64_t>(readVarint(r));
    return r.ok();
}

std::vector<uint8_t> encode(const InputMessage& m) {
    BitWriter w = start(MessageType::input);
    const size_t n = std::min(m.inputs.size(), InputMessage::kMaxInputs);
    w.writeBits(n, 3);
    // The newest tick whole, the older ones as steps back from the one before.
    for (size_t i = 0; i < n; i++) {
        if (i == 0) writeSigned(w, m.inputs[0].tick);
        else writeSigned(w, m.inputs[i - 1].tick - m.inputs[i].tick);
        writeInput(w, m.inputs[i]);
    }
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, InputMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::input, r)) return false;
    const size_t n = size_t(r.readBits(3));
    if (n > InputMessage::kMaxInputs) return false;
    m.inputs.assign(n, PilotInput{});
    for (size_t i = 0; i < n; i++) {
        const int64_t t = readSigned(r);
        m.inputs[i].tick = i == 0 ? t : m.inputs[i - 1].tick - t;
        readInput(r, m.inputs[i]);
    }
    return r.ok();
}

std::vector<uint8_t> encode(const AckMessage& m) {
    BitWriter w = start(MessageType::ack);
    const size_t n = std::min(m.received.size(), AckMessage::kMaxSnapshots);
    w.writeBits(n, 3);
    for (size_t i = 0; i < n; i++) {
        if (i == 0) writeSigned(w, m.received[0].snapshot);
        else writeSigned(w, m.received[i - 1].snapshot - m.received[i].snapshot);
        writeVarint(w, m.received[i].parts);
    }
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, AckMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::ack, r)) return false;
    const size_t n = size_t(r.readBits(3));
    if (n > AckMessage::kMaxSnapshots) return false;
    m.received.assign(n, AckMessage::Received{});
    for (size_t i = 0; i < n; i++) {
        const int64_t s = readSigned(r);
        m.received[i].snapshot = i == 0 ? s : m.received[i - 1].snapshot - s;
        m.received[i].parts = static_cast<uint32_t>(readVarint(r));
    }
    return r.ok();
}

std::vector<uint8_t> encode(const RequestMessage& m) {
    BitWriter w = start(MessageType::request);
    writeVarint(w, m.id);
    w.writeBits(m.what.index(), 4);
    if (const auto* t = std::get_if<RequestMessage::Take>(&m.what)) writeSigned(w, t->unit);
    if (const auto* f = std::get_if<RequestMessage::Fly>(&m.what)) {
        writeString(w, f->region);
        writeString(w, f->pad);
    }
    if (const auto* b = std::get_if<RequestMessage::Build>(&m.what)) {
        w.writeBits(uint64_t(b->kind), 4);
        codec::Pos{}.write(w, b->at);
    }
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, RequestMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::request, r)) return false;
    m.id = static_cast<uint32_t>(readVarint(r));
    switch (r.readBits(4)) {
    case 0: m.what = RequestMessage::Take{readSigned(r)}; break;
    case 1: m.what = RequestMessage::HandBack{}; break;
    case 2: {
        RequestMessage::Fly f;
        if (!readString(r, f.region) || !readString(r, f.pad)) return false;
        m.what = f;
        break;
    }
    case 3: {
        RequestMessage::Build b;
        const uint64_t kind = r.readBits(4);
        if (kind >= uint64_t(allCases<StructureKind>().size())) return false;
        b.kind = static_cast<StructureKind>(kind);
        codec::Pos{}.read(r, b.at);
        m.what = b;
        break;
    }
    default: return false;
    }
    return r.ok();
}

std::vector<uint8_t> encode(const ReplyMessage& m) {
    BitWriter w = start(MessageType::reply);
    writeVarint(w, m.request);
    w.writeBool(m.done);
    writeString(w, m.reason);
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, ReplyMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::reply, r)) return false;
    m.request = static_cast<uint32_t>(readVarint(r));
    m.done = r.readBool();
    return readString(r, m.reason) && r.ok();
}

std::vector<std::vector<uint8_t>> encodeEvents(int64_t serverTick, const std::vector<GameEvent>& events) {
    std::vector<std::vector<uint8_t>> out;
    size_t i = 0;
    while (i < events.size()) {
        // Count-prefixed: as many as fit, each event at most ~40 bytes.
        BitWriter body;
        size_t n = 0;
        while (i + n < events.size() && body.byteCount() + 64 < kMaxReliableBytes - 16) {
            writeEvent(body, events[i + n]);
            n += 1;
        }
        BitWriter w = start(MessageType::event);
        writeSigned(w, serverTick);
        writeVarint(w, n);
        appendBits(w, body.bytes().data(), 0, body.bitCount());
        out.push_back(w.bytes());
        i += n;
    }
    return out;
}

bool decode(const std::vector<uint8_t>& bytes, EventMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::event, r)) return false;
    m.serverTick = readSigned(r);
    const uint64_t n = readVarint(r);
    if (n > r.bitsLeft()) return false;
    m.events.assign(size_t(n), GameEvent{});
    for (GameEvent& e : m.events)
        if (!readEvent(r, e)) return false;
    return r.ok();
}

std::vector<region::Place> padsOnGround(const WorldMessage::Region& r, const GroundRect& bounds) {
    std::vector<region::Place> out;
    for (const WorldMessage::Pad& p : r.pads) out.push_back(region::Place{p.name, Vec2(p.at.x + bounds.minX, p.at.y + bounds.minZ)});
    return out;
}

} // namespace ac::net
