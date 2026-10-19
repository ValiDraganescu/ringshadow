#include "NetProtocol.h"

#include "NetCodec.h"

#include "Brood.h"
#include "NetSocket.h"
#include "NetTypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

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
    w.writeBits(q::pitch(qi.pitch), q::kPitchBits);
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
    w.writeBits(uint64_t(std::clamp<int64_t>(in.slot, 0, Progression::slotCount)), 2);
}

void readInput(BitReader& r, PilotInput& in) {
    in.walk.x = double(readSigned(r)) / q::kWalkSteps;
    in.walk.y = double(readSigned(r)) / q::kWalkSteps;
    in.heading = q::angle(r.readU16());
    in.pitch = q::pitch(uint8_t(r.readBits(q::kPitchBits)));
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
    in.slot = int64_t(r.readBits(2));
}

void writeSkill(BitWriter& w, Skill s) { writeVarint(w, uint64_t(s)); }

bool readSkill(BitReader& r, Skill& s) {
    const uint64_t v = readVarint(r);
    if (v >= uint64_t(allCases<Skill>().size())) return false;
    s = static_cast<Skill>(v);
    return true;
}

/// A rig module and a mark (P7): 3 bits, then the mark (0 to 3) in 2.
void writeModule(BitWriter& w, Module m, int64_t mark) {
    w.writeBits(uint64_t(m), 3);
    w.writeBits(uint64_t(std::clamp<int64_t>(mark, 0, 3)), 2);
}

bool readModule(BitReader& r, Module& m, int64_t& mark) {
    const uint64_t v = r.readBits(3);
    if (v >= uint64_t(allCases<Module>().size())) return false;
    m = static_cast<Module>(v);
    mark = int64_t(r.readBits(2));
    return true;
}

void writeJob(BitWriter& w, const std::optional<ModuleJob>& j) {
    w.writeBool(j.has_value());
    if (!j) return;
    writeModule(w, j->module, j->mark);
    codec::kMs.write(w, j->done);
}

bool readJob(BitReader& r, std::optional<ModuleJob>& j) {
    j.reset();
    if (!r.readBool()) return true;
    ModuleJob x;
    if (!readModule(r, x.module, x.mark)) return false;
    codec::kMs.read(r, x.done);
    j = x;
    return true;
}

void writeRig(BitWriter& w, const Rig& rig) {
    writeVarint(w, rig.fitted.size());
    for (const auto& [m, mark] : rig.fitted) writeModule(w, m, mark);
    writeVarint(w, rig.stores.size());
    for (const ModuleStock& s : rig.stores) {
        writeModule(w, s.module, s.mark);
        writeVarint(w, uint64_t(std::max<int64_t>(0, s.count)));
    }
    writeVarint(w, rig.researched.size());
    for (const auto& [m, mark] : rig.researched) writeModule(w, m, mark);
    writeJob(w, rig.making);
    writeJob(w, rig.research);
}

bool readRig(BitReader& r, Rig& rig) {
    rig = Rig{};
    const uint64_t fitted = readVarint(r);
    if (fitted > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < fitted; i++) {
        Module m{};
        int64_t mark = 0;
        if (!readModule(r, m, mark)) return false;
        rig.fitted[m] = mark;
    }
    const uint64_t stores = readVarint(r);
    if (stores > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < stores; i++) {
        ModuleStock s;
        if (!readModule(r, s.module, s.mark)) return false;
        s.count = int64_t(readVarint(r));
        rig.stores.push_back(s);
    }
    const uint64_t researched = readVarint(r);
    if (researched > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < researched; i++) {
        Module m{};
        int64_t mark = 0;
        if (!readModule(r, m, mark)) return false;
        rig.researched[m] = mark;
    }
    return readJob(r, rig.making) && readJob(r, rig.research);
}

void writeStrings(BitWriter& w, const std::set<std::string>& set) {
    writeVarint(w, set.size());
    for (const std::string& s : set) writeString(w, s);
}

bool readStrings(BitReader& r, std::set<std::string>& set) {
    const uint64_t n = readVarint(r);
    if (n > r.bitsLeft()) return false;
    set.clear();
    for (uint64_t i = 0; i < n; i++) {
        std::string s;
        if (!readString(r, s)) return false;
        set.insert(s);
    }
    return true;
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
    // Up to 56 bits at a time from the source's bit offset, reading only the
    // bytes they lie in.
    size_t at = from;
    while (bits > 0) {
        const int n = static_cast<int>(std::min<size_t>(bits, 56));
        const size_t index = at / 8;
        const int offset = static_cast<int>(at % 8);
        uint64_t v = 0;
        for (int k = 0; k < (offset + n + 7) / 8; k++) v |= uint64_t(src[index + size_t(k)]) << (8 * k);
        w.writeBits(v >> offset, n);
        at += size_t(n);
        bits -= size_t(n);
    }
}

std::optional<MessageType> messageType(const std::vector<uint8_t>& message) {
    if (message.empty()) return std::nullopt;
    const uint8_t t = message[0];
    if (t < uint8_t(MessageType::welcome) || t > uint8_t(MessageType::hunt)) return std::nullopt;
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

uint8_t pitch(double radians) {
    if (!std::isfinite(radians)) return uint8_t(kPitchSteps);
    const double c = std::clamp(radians / (kTurn / 4), -1.0, 1.0);
    return static_cast<uint8_t>(std::llround(c * double(kPitchSteps)) + kPitchSteps);
}

double pitch(uint8_t v) {
    const int64_t steps = std::min<int64_t>(v, 2 * kPitchSteps) - kPitchSteps;
    return double(steps) / double(kPitchSteps) * (kTurn / 4);
}

} // namespace q

PilotInput quantised(const PilotInput& in) {
    PilotInput out = in;
    auto axis = [](double v) {
        const double c = std::isfinite(v) ? std::clamp(v, -1.0, 1.0) : 0.0;
        return double(std::llround(c * q::kWalkSteps)) / q::kWalkSteps;
    };
    out.walk = Vec2(axis(in.walk.x), axis(in.walk.y));
    out.heading = q::angle(q::angle(in.heading));
    out.pitch = q::pitch(q::pitch(in.pitch));
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

namespace {

/// `v` in 1/`steps`, clamped to `bits` signed bits.
void writeClamped(BitWriter& w, double v, int64_t steps, int bits) {
    const int64_t lim = (int64_t(1) << (bits - 1)) - 1;
    w.writeBits(static_cast<uint64_t>(std::clamp(fixed(v, double(steps)), -lim, lim)), bits);
}

double readClamped(BitReader& r, int64_t steps, int bits) { return double(signExtend(r.readBits(bits), bits)) / double(steps); }

} // namespace

void Point::write(BitWriter& w, Vec3 v) const {
    writeClamped(w, v.x, q::kPointSteps, q::kPointBits);
    writeClamped(w, v.y, q::kPointSteps, q::kPointBits);
    writeClamped(w, v.z, q::kPointSteps, q::kPointHeightBits);
}

void Point::read(BitReader& r, Vec3& v) const {
    v.x = readClamped(r, q::kPointSteps, q::kPointBits);
    v.y = readClamped(r, q::kPointSteps, q::kPointBits);
    v.z = readClamped(r, q::kPointSteps, q::kPointHeightBits);
}

void Velocity::write(BitWriter& w, Vec3 v) const {
    for (int i = 0; i < 3; i++) writeClamped(w, v[i], q::kVelocitySteps, q::kVelocityBits);
}

void Velocity::read(BitReader& r, Vec3& v) const {
    for (int i = 0; i < 3; i++) v[i] = readClamped(r, q::kVelocitySteps, q::kVelocityBits);
}

void Share::write(BitWriter& w, double v) const {
    const double c = std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.0;
    w.writeBits(static_cast<uint64_t>(std::llround(c * 255)), q::kShareBits);
}

void Share::read(BitReader& r, double& v) const { v = double(r.readBits(q::kShareBits)) / 255; }

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

void BugCodec::write(BitWriter& w, const Bug& b) const {
    w.writeBits(static_cast<uint64_t>(b.state), 3);
    w.writeBits(static_cast<uint64_t>(std::clamp<int64_t>(std::llround(b.height * 10), 0, 63)), 6);
    w.writeBits(static_cast<uint64_t>(b.strikes & 15), 4);
    w.writeBits(static_cast<uint64_t>(std::clamp<int64_t>(std::llround(b.sac / Brood::sacFull * 15), 0, 15)), 4);
}

void BugCodec::read(BitReader& r, Bug& b) const {
    b = Bug{};
    b.state = static_cast<Bug::State>(r.readBits(3));
    b.height = double(r.readBits(6)) / 10;
    b.strikes = int64_t(r.readBits(4));
    b.sac = double(r.readBits(4)) / 15 * Brood::sacFull;
}

void MarksCodec::write(BitWriter& w, const TurretMarks& m) const {
    for (int64_t x : {m.rate, m.damage, m.range, m.air}) w.writeBits(static_cast<uint64_t>(std::clamp<int64_t>(x, 0, 3)), 2);
}

void MarksCodec::read(BitReader& r, TurretMarks& m) const {
    m.rate = int64_t(r.readBits(2));
    m.damage = int64_t(r.readBits(2));
    m.range = int64_t(r.readBits(2));
    m.air = int64_t(r.readBits(2));
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
    if (const auto* u = x.as<Request::What::Unit>()) w.writeBits(static_cast<uint64_t>(u->kind), codec::kUnitKindBits);
    if (const auto* b = x.as<Request::What::Building>()) w.writeBits(static_cast<uint64_t>(b->kind), codec::kStructureKindBits);
    if (const auto* g = x.as<Request::What::Upgrade>()) w.writeBits(static_cast<uint64_t>(g->upgrade), 2);
}

void RequestWhat::read(BitReader& r, Request::What& x) const {
    const uint64_t i = r.readBits(2);
    if (i == 0) x = Request::What(Request::What::Unit{static_cast<UnitKind>(r.readBits(codec::kUnitKindBits))});
    else if (i == 1) x = Request::What(Request::What::Building{static_cast<StructureKind>(r.readBits(codec::kStructureKindBits))});
    else x = Request::What(Request::What::Upgrade{static_cast<Upgrade>(r.readBits(2))});
}

} // namespace codec

// MARK: - Events

void writeEvent(BitWriter& w, const GameEvent& e) {
    using codec::Pos;
    const Pos pos;
    const codec::Point point;
    const codec::Velocity velocity;
    auto id = [&](int64_t v) { writeSigned(w, v); };
    w.writeBits(e.index(), 5);
    std::visit(
        [&](const auto& x) {
            using E = std::decay_t<decltype(x)>;
            using G = GameEvent;
            auto unitKind = [&](UnitKind k) { w.writeBits(uint64_t(k), codec::kUnitKindBits); };
            auto structureKind = [&](StructureKind k) { w.writeBits(uint64_t(k), codec::kStructureKindBits); };
            if constexpr (std::is_same_v<E, G::Deposited>) { id(x.unit); id(x.amount); pos.write(w, x.at); id(x.tax); w.writeBool(x.hydrogen); }
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
            else if constexpr (std::is_same_v<E, G::Fired>) { id(x.shooter); id(x.round); point.write(w, x.muzzle); velocity.write(w, x.velocity); }
            else if constexpr (std::is_same_v<E, G::Impact>) { id(x.round); point.write(w, x.at); w.writeBits(uint64_t(x.what), 2); id(x.victim); }
            else if constexpr (std::is_same_v<E, G::DummyDown>) { id(x.id); id(x.hits); }
            else if constexpr (std::is_same_v<E, G::Repaired>) { id(x.structure); id(x.by); id(x.ore); id(x.hydrogen); }
            else if constexpr (std::is_same_v<E, G::Demolished>) { id(x.structure); id(x.ore); id(x.hydrogen); }
            else if constexpr (std::is_same_v<E, G::Booked>) { id(x.unit); id(x.ore); id(x.hydrogen); pos.write(w, x.at); id(x.taxOre); id(x.taxHydrogen); }
            else static_assert(sizeof(E) == 0, "every event has its encoding");
        },
        e.value);
}

bool readEvent(BitReader& r, GameEvent& e) {
    using codec::Pos;
    const Pos pos;
    auto point = [&] { Vec3 v; codec::Point{}.read(r, v); return v; };
    auto id = [&] { return readSigned(r); };
    auto at = [&] { Vec2 v; pos.read(r, v); return v; };
    auto unitKind = [&] { return static_cast<UnitKind>(r.readBits(codec::kUnitKindBits)); };
    auto structureKind = [&] { return static_cast<StructureKind>(r.readBits(codec::kStructureKindBits)); };
    auto radius = [&] { double v = 0; codec::kCm.read(r, v); return v; };
    switch (r.readBits(5)) {
    case 0: { auto u = id(); auto a = id(); auto p = at(); auto t = id(); e = GameEvent(GameEvent::Deposited{u, a, p, t, r.readBool()}); break; }
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
    case 18: {
        auto s = id();
        auto n = id();
        auto m = point();
        Vec3 v;
        codec::Velocity{}.read(r, v);
        e = GameEvent(GameEvent::Fired{s, n, m, v});
        break;
    }
    case 19: {
        auto n = id();
        auto a = point();
        const uint64_t what = r.readBits(2);
        if (what > uint64_t(GameEvent::Impact::What::structure)) return false;
        e = GameEvent(GameEvent::Impact{n, a, static_cast<GameEvent::Impact::What>(what), id()});
        break;
    }
    case 20: { auto d = id(); e = GameEvent(GameEvent::DummyDown{d, id()}); break; }
    case 21: {
        const auto s = id();
        const auto by = id();
        const auto ore = id();
        e = GameEvent(GameEvent::Repaired{s, by, ore, id()});
        break;
    }
    case 22: {
        const auto s = id();
        const auto ore = id();
        e = GameEvent(GameEvent::Demolished{s, ore, id()});
        break;
    }
    case 23: {
        const auto u = id();
        const auto ore = id();
        const auto mh = id();
        const auto a = at();
        const auto taxOre = id();
        e = GameEvent(GameEvent::Booked{u, ore, mh, a, taxOre, id()});
        break;
    }
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
        w.writeBits(uint64_t(g.ground), 4);
        writeVarint(w, static_cast<uint64_t>(g.column));
        writeVarint(w, static_cast<uint64_t>(g.row));
        writeVarint(w, g.pads.size());
        for (const WorldMessage::Pad& p : g.pads) {
            writeString(w, p.name);
            writeDouble(w, p.at.x);
            writeDouble(w, p.at.y);
            w.writeBool(p.building.has_value());
            if (p.building) w.writeBits(uint64_t(*p.building), codec::kStructureKindBits);
        }
        w.writeBool(g.nest.has_value());
        if (g.nest) {
            writeDouble(w, g.nest->x);
            writeDouble(w, g.nest->y);
        }
    }
    writeVarint(w, static_cast<uint64_t>(m.here));
    w.writeBool(m.flyingTo.has_value());
    if (m.flyingTo) writeVarint(w, static_cast<uint64_t>(*m.flyingTo));
    w.writeBool(m.ferry.has_value());
    if (m.ferry) {
        const WorldMessage::Ferry& f = *m.ferry;
        writeVarint(w, static_cast<uint64_t>(f.from));
        writeDouble(w, f.boarded.x);
        writeDouble(w, f.boarded.y);
        writeString(w, f.pad);
        writeDouble(w, f.at.x);
        writeDouble(w, f.at.y);
        for (const double s : {f.plan.descend, f.plan.board, f.plan.lift, f.plan.turns, f.plan.dash, f.plan.approach, f.elapsed})
            writeDouble(w, s);
    }
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
        g.ground = static_cast<region::Ground>(r.readBits(4));
        if (size_t(g.style) >= 4 || size_t(g.size) >= 4 || size_t(g.ground) >= allCases<region::Ground>().size()) return false;
        g.column = static_cast<int64_t>(readVarint(r));
        g.row = static_cast<int64_t>(readVarint(r));
        const uint64_t pads = readVarint(r);
        if (pads > 256 || !r.ok()) return false;
        g.pads.assign(size_t(pads), {});
        for (WorldMessage::Pad& p : g.pads) {
            if (!readString(r, p.name)) return false;
            p.at.x = readDouble(r);
            p.at.y = readDouble(r);
            if (r.readBool()) {
                const uint64_t k = r.readBits(codec::kStructureKindBits);
                if (k >= uint64_t(allCases<StructureKind>().size())) return false;
                p.building = static_cast<StructureKind>(k);
            }
        }
        g.nest = std::nullopt;
        if (r.readBool()) {
            const double x = readDouble(r);
            g.nest = Vec2(x, readDouble(r));
        }
    }
    m.here = static_cast<int64_t>(readVarint(r));
    m.flyingTo = std::nullopt;
    if (r.readBool()) m.flyingTo = static_cast<int64_t>(readVarint(r));
    m.ferry = std::nullopt;
    if (r.readBool()) {
        WorldMessage::Ferry f;
        f.from = static_cast<int64_t>(readVarint(r));
        f.boarded.x = readDouble(r);
        f.boarded.y = readDouble(r);
        if (!readString(r, f.pad)) return false;
        f.at.x = readDouble(r);
        f.at.y = readDouble(r);
        for (double* s : {&f.plan.descend, &f.plan.board, &f.plan.lift, &f.plan.turns, &f.plan.dash, &f.plan.approach, &f.elapsed})
            *s = readDouble(r);
        m.ferry = f;
    }
    return r.ok();
}

std::vector<uint8_t> encode(const InputMessage& m) {
    BitWriter w = start(MessageType::input);
    const size_t n = std::min(m.inputs.size(), InputMessage::kMaxInputs);
    w.writeBits(n, 3);
    // The newest tick and visit whole, the older ones as steps back from
    // the one before (a visit's step is nearly always 0).
    for (size_t i = 0; i < n; i++) {
        if (i == 0) {
            writeSigned(w, m.inputs[0].tick);
            writeSigned(w, m.inputs[0].visit);
        } else {
            writeSigned(w, m.inputs[i - 1].tick - m.inputs[i].tick);
            writeSigned(w, m.inputs[i - 1].visit - m.inputs[i].visit);
        }
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
        const int64_t v = readSigned(r);
        m.inputs[i].visit = i == 0 ? v : m.inputs[i - 1].visit - v;
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

namespace {

/// A Citadel mission (K2).
void writeMission(BitWriter& w, const CitadelMission& x) {
    writeVarint(w, x.id);
    w.writeBits(uint64_t(x.kind), 3);
    w.writeBits(uint64_t(x.tier), 2);
    w.writeBits(uint64_t(x.state), 2);
    writeString(w, x.key);
    writeString(w, x.title);
    writeString(w, x.briefing);
    writeString(w, x.instructions);
    writeString(w, x.target);
    writeString(w, x.region);
    codec::Fixed{1000}.write(w, x.at.x);
    codec::Fixed{1000}.write(w, x.at.y);
    codec::Fixed{1000}.write(w, x.radius);
    writeSigned(w, x.need);
    writeSigned(w, x.have);
    codec::Fixed{1000}.write(w, x.xp);
    writeSigned(w, x.ore);
    writeSigned(w, x.hydrogen);
    w.writeBool(x.blueprint.has_value());
    if (x.blueprint) w.writeBits(uint64_t(*x.blueprint), codec::kStructureKindBits);
    w.writeBool(x.item.has_value());
    if (x.item) w.writeBits(uint64_t(*x.item), 3);
    writeSigned(w, x.count);
}

bool readMission(BitReader& r, CitadelMission& x) {
    x.id = static_cast<uint32_t>(readVarint(r));
    const uint64_t kind = r.readBits(3), tier = r.readBits(2), state = r.readBits(2);
    if (kind > uint64_t(CitadelMission::Kind::hunt) || tier > uint64_t(CitadelMission::Tier::perilous) || state > uint64_t(CitadelMission::State::done))
        return false;
    x.kind = static_cast<CitadelMission::Kind>(kind);
    x.tier = static_cast<CitadelMission::Tier>(tier);
    x.state = static_cast<CitadelMission::State>(state);
    if (!readString(r, x.key) || !readString(r, x.title) || !readString(r, x.briefing) || !readString(r, x.instructions) ||
        !readString(r, x.target) || !readString(r, x.region))
        return false;
    codec::Fixed{1000}.read(r, x.at.x);
    codec::Fixed{1000}.read(r, x.at.y);
    codec::Fixed{1000}.read(r, x.radius);
    x.need = readSigned(r);
    x.have = readSigned(r);
    codec::Fixed{1000}.read(r, x.xp);
    x.ore = readSigned(r);
    x.hydrogen = readSigned(r);
    if (r.readBool()) {
        const uint64_t k = r.readBits(codec::kStructureKindBits);
        if (k >= uint64_t(allCases<StructureKind>().size())) return false;
        x.blueprint = static_cast<StructureKind>(k);
    }
    if (r.readBool()) {
        const uint64_t i = r.readBits(3);
        if (i >= uint64_t(allCases<Item>().size())) return false;
        x.item = static_cast<Item>(i);
    }
    x.count = readSigned(r);
    return true;
}

} // namespace

// The request's index takes 5 bits.
static_assert(std::variant_size_v<decltype(RequestMessage::what)> <= 32);

std::vector<uint8_t> encode(const RequestMessage& m) {
    BitWriter w = start(MessageType::request);
    writeVarint(w, m.id);
    w.writeBits(m.what.index(), 5);
    if (const auto* t = std::get_if<RequestMessage::Take>(&m.what)) writeSigned(w, t->unit);
    if (const auto* f = std::get_if<RequestMessage::Fly>(&m.what)) {
        writeString(w, f->region);
        writeString(w, f->pad);
    }
    if (const auto* b = std::get_if<RequestMessage::Build>(&m.what)) {
        w.writeBits(uint64_t(b->kind), codec::kStructureKindBits);
        codec::Pos{}.write(w, b->at);
    }
    if (const auto* p = std::get_if<RequestMessage::Pick>(&m.what)) {
        writeVarint(w, uint64_t(p->kind));
        writeVarint(w, uint64_t(p->perk));
    }
    if (const auto* p = std::get_if<RequestMessage::Spend>(&m.what)) writeSkill(w, p->skill);
    if (const auto* p = std::get_if<RequestMessage::Undo>(&m.what)) writeSkill(w, p->skill);
    if (const auto* p = std::get_if<RequestMessage::Slot>(&m.what)) {
        writeVarint(w, uint64_t(std::clamp<int64_t>(p->index, 0, 255)));
        w.writeBool(p->skill.has_value());
        if (p->skill) writeSkill(w, *p->skill);
    }
    if (const auto* p = std::get_if<RequestMessage::Abandon>(&m.what)) writeVarint(w, p->contract);
    if (const auto* p = std::get_if<RequestMessage::Make>(&m.what)) writeModule(w, p->module, p->mark);
    if (const auto* p = std::get_if<RequestMessage::Research>(&m.what)) writeModule(w, p->module, p->mark);
    if (const auto* p = std::get_if<RequestMessage::Fit>(&m.what)) writeModule(w, p->module, p->mark);
    if (const auto* p = std::get_if<RequestMessage::Donate>(&m.what)) {
        writeSigned(w, p->ore);
        writeSigned(w, p->hydrogen);
    }
    if (const auto* p = std::get_if<RequestMessage::Buy>(&m.what)) {
        w.writeBool(p->blueprint.has_value());
        if (p->blueprint) w.writeBits(uint64_t(*p->blueprint), codec::kStructureKindBits);
        else w.writeBits(uint64_t(p->item), 3);
    }
    if (const auto* p = std::get_if<RequestMessage::Use>(&m.what)) w.writeBits(uint64_t(p->item), 3);
    if (const auto* p = std::get_if<RequestMessage::MissionAsk>(&m.what)) {
        writeVarint(w, p->mission);
        w.writeBool(p->take);
    }
    if (const auto* p = std::get_if<RequestMessage::UpgradeCitadel>(&m.what)) w.writeBits(uint64_t(p->stat), 2);
    if (const auto* p = std::get_if<RequestMessage::Merc>(&m.what)) {
        w.writeBits(uint64_t(p->act), 2);
        writeVarint(w, p->merc);
        writeString(w, p->region);
        writeSigned(w, p->field);
        w.writeBool(p->guard);
    }
    if (const auto* p = std::get_if<RequestMessage::UpgradeTurret>(&m.what)) {
        writeSigned(w, p->turret);
        w.writeBits(uint64_t(p->stat), 2);
    }
    if (const auto* p = std::get_if<RequestMessage::StartProject>(&m.what)) w.writeBits(uint64_t(p->kind), 3);
    if (const auto* p = std::get_if<RequestMessage::Door>(&m.what)) w.writeBool(p->open);
    // A gun 0 to 3, or -1 (leave it): 3 bits, the gun + 1.
    if (const auto* p = std::get_if<RequestMessage::Man>(&m.what))
        w.writeBits(uint64_t(std::clamp<int>(p->gun, -1, 3) + 1), 3);
    if (const auto* p = std::get_if<RequestMessage::Learn>(&m.what)) w.writeBits(uint64_t(p->kind), codec::kStructureKindBits);
    if (const auto* p = std::get_if<RequestMessage::Demolish>(&m.what)) writeSigned(w, p->structure);
    if (const auto* p = std::get_if<RequestMessage::BuyMule>(&m.what)) writeSigned(w, p->foundry);
    if (const auto* p = std::get_if<RequestMessage::MuleFill>(&m.what)) writeSigned(w, p->mule);
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, RequestMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::request, r)) return false;
    m.id = static_cast<uint32_t>(readVarint(r));
    // An item (C2), in 3 bits.
    auto readItem = [&](Item& item) {
        const uint64_t i = r.readBits(3);
        if (i >= uint64_t(allCases<Item>().size())) return false;
        item = static_cast<Item>(i);
        return true;
    };
    switch (r.readBits(5)) {
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
        const uint64_t kind = r.readBits(codec::kStructureKindBits);
        if (kind >= uint64_t(allCases<StructureKind>().size())) return false;
        b.kind = static_cast<StructureKind>(kind);
        codec::Pos{}.read(r, b.at);
        m.what = b;
        break;
    }
    case 4: {
        const uint64_t kind = readVarint(r);
        const uint64_t perk = readVarint(r);
        if (kind >= uint64_t(allCases<UnitKind>().size()) || perk >= uint64_t(allCases<Perk>().size())) return false;
        m.what = RequestMessage::Pick{static_cast<UnitKind>(kind), static_cast<Perk>(perk)};
        break;
    }
    case 5: {
        RequestMessage::Spend p;
        if (!readSkill(r, p.skill)) return false;
        m.what = p;
        break;
    }
    case 6: {
        RequestMessage::Undo p;
        if (!readSkill(r, p.skill)) return false;
        m.what = p;
        break;
    }
    case 7: m.what = RequestMessage::Respec{}; break;
    case 8: {
        RequestMessage::Slot p;
        p.index = int64_t(readVarint(r));
        if (r.readBool()) {
            Skill s{};
            if (!readSkill(r, s)) return false;
            p.skill = s;
        }
        m.what = p;
        break;
    }
    case 9: m.what = RequestMessage::Abandon{static_cast<uint32_t>(readVarint(r))}; break;
    case 10: {
        RequestMessage::Make p;
        if (!readModule(r, p.module, p.mark)) return false;
        m.what = p;
        break;
    }
    case 11: {
        RequestMessage::Research p;
        if (!readModule(r, p.module, p.mark)) return false;
        m.what = p;
        break;
    }
    case 12: {
        RequestMessage::Fit p;
        if (!readModule(r, p.module, p.mark)) return false;
        m.what = p;
        break;
    }
    case 13: {
        RequestMessage::Donate p;
        p.ore = readSigned(r);
        p.hydrogen = readSigned(r);
        m.what = p;
        break;
    }
    case 14: {
        RequestMessage::Buy p;
        if (r.readBool()) {
            const uint64_t kind = r.readBits(codec::kStructureKindBits);
            if (kind >= uint64_t(allCases<StructureKind>().size())) return false;
            p.blueprint = static_cast<StructureKind>(kind);
        } else if (!readItem(p.item)) {
            return false;
        }
        m.what = p;
        break;
    }
    case 15: {
        RequestMessage::Use p;
        if (!readItem(p.item)) return false;
        m.what = p;
        break;
    }
    case 16: {
        RequestMessage::MissionAsk p;
        p.mission = static_cast<uint32_t>(readVarint(r));
        p.take = r.readBool();
        m.what = p;
        break;
    }
    case 17: {
        RequestMessage::UpgradeCitadel p;
        const uint64_t stat = r.readBits(2);
        if (stat >= uint64_t(allCases<DefenceStat>().size())) return false;
        p.stat = static_cast<DefenceStat>(stat);
        m.what = p;
        break;
    }
    case 18: {
        RequestMessage::Merc p;
        const uint64_t act = r.readBits(2);
        if (act > uint64_t(RequestMessage::Merc::Act::role)) return false;
        p.act = static_cast<RequestMessage::Merc::Act>(act);
        p.merc = static_cast<uint32_t>(readVarint(r));
        if (!readString(r, p.region)) return false;
        p.field = readSigned(r);
        p.guard = r.readBool();
        m.what = p;
        break;
    }
    case 19: {
        RequestMessage::UpgradeTurret p;
        p.turret = readSigned(r);
        p.stat = static_cast<DefenceStat>(r.readBits(2));
        m.what = p;
        break;
    }
    case 20: {
        RequestMessage::StartProject p;
        p.kind = uint8_t(r.readBits(3));
        m.what = p;
        break;
    }
    case 21: m.what = RequestMessage::CallHunt{}; break;
    case 22: m.what = RequestMessage::Door{r.readBool()}; break;
    case 23: {
        const uint64_t g = r.readBits(3);
        if (g > 4) return false;
        m.what = RequestMessage::Man{int8_t(int(g) - 1)};
        break;
    }
    case 24: {
        const uint64_t kind = r.readBits(codec::kStructureKindBits);
        if (kind >= codec::kStructureKindCount) return false;
        m.what = RequestMessage::Learn{static_cast<StructureKind>(kind)};
        break;
    }
    case 25: m.what = RequestMessage::Demolish{readSigned(r)}; break;
    case 26: m.what = RequestMessage::BuyMule{readSigned(r)}; break;
    case 27: m.what = RequestMessage::MuleFill{readSigned(r)}; break;
    default: return false;
    }
    return r.ok();
}

std::vector<uint8_t> encode(const ProgressMessage& m) {
    BitWriter w = start(MessageType::progress);
    codec::Fixed{1000}.write(w, m.xp);
    writeVarint(w, uint64_t(std::max<int64_t>(0, m.level)));
    writeSigned(w, m.points);
    writeVarint(w, m.ranks.size());
    for (const auto& [skill, rank] : m.ranks) {
        writeSkill(w, skill);
        writeSigned(w, rank);
    }
    for (const auto& slot : m.slots) {
        w.writeBool(slot.has_value());
        if (slot) writeSkill(w, *slot);
    }
    writeVarint(w, m.ready.size());
    for (const auto& [skill, at] : m.ready) {
        writeSkill(w, skill);
        codec::kMs.write(w, at);
    }
    writeVarint(w, m.undo.size());
    for (const auto& u : m.undo) {
        writeSkill(w, u.skill);
        codec::kMs.write(w, u.until);
    }
    writeSigned(w, m.respecPrice);
    w.writeBool(m.respecFree);
    writeVarint(w, m.blueprints.size());
    for (const auto& [k, n] : m.blueprints) {
        w.writeBits(uint64_t(k), codec::kStructureKindBits);
        writeSigned(w, n);
    }
    writeStrings(w, m.sitesKnown);
    writeStrings(w, m.sitesFound);
    w.writeBool(m.opening.has_value());
    if (m.opening) {
        writeString(w, m.opening->site);
        codec::kMs.write(w, m.opening->since);
        codec::kMs.write(w, m.opening->needs);
        writeString(w, m.opening->refusal);
    }
    writeVarint(w, m.contracts.size());
    for (const Contract& c : m.contracts) {
        writeVarint(w, c.id);
        w.writeBits(uint64_t(c.kind), 3);
        w.writeBits(uint64_t(c.state), 2);
        w.writeBits(uint64_t(c.step), 3);
        writeString(w, c.title);
        writeString(w, c.pitch);
        writeString(w, c.target);
        writeString(w, c.region);
        writeSigned(w, c.need);
        writeSigned(w, c.have);
        codec::Fixed{1000}.write(w, c.xp);
        writeSigned(w, c.hydrogen);
    }
    codec::kMs.write(w, m.contractsDawn);
    w.writeBool(m.goal.has_value());
    if (m.goal) {
        writeString(w, m.goal->key);
        writeString(w, m.goal->title);
        writeString(w, m.goal->pitch);
        writeString(w, m.goal->why);
        writeString(w, m.goal->hint);
        w.writeBool(m.goal->marked);
        writeString(w, m.goal->region);
        codec::Fixed{1000}.write(w, m.goal->at.x);
        codec::Fixed{1000}.write(w, m.goal->at.y);
        writeSigned(w, m.goal->level);
        codec::Fixed{1000}.write(w, m.goal->xp);
        writeSigned(w, m.goal->hydrogen);
    }
    w.writeBool(m.respawn.has_value());
    if (m.respawn) {
        codec::kMs.write(w, m.respawn->at);
        writeString(w, m.respawn->region);
        w.writeBool(m.respawn->home.has_value());
        if (m.respawn->home) w.writeBits(uint64_t(*m.respawn->home), codec::kStructureKindBits);
        writeSigned(w, m.respawn->cargo);
        w.writeBool(m.respawn->hydrogen);
    }
    writeRig(w, m.rig);
    w.writeBool(m.foundry);
    w.writeBool(m.lab);
    writeVarint(w, m.realm.size());
    for (const auto& [kind, n] : m.realm) {
        w.writeBits(uint64_t(kind), codec::kStructureKindBits);
        writeSigned(w, n);
    }
    writeVarint(w, m.builders.size());
    for (const auto& [id, by] : m.builders) {
        writeSigned(w, id);
        writeString(w, by);
    }
    writeString(w, m.callsign);
    writeVarint(w, m.turretKills.size());
    for (const auto& [id, n] : m.turretKills) {
        writeSigned(w, id);
        writeSigned(w, n);
    }
    for (const auto* p : {&m.realmWallet, &m.taxed, &m.donated}) {
        writeSigned(w, p->ore);
        writeSigned(w, p->hydrogen);
    }
    writeSigned(w, m.taxPercent);
    // C2: the kinds learned, one bit each.
    for (size_t k = 0; k < codec::kStructureKindCount; k++) w.writeBool(m.learned.count(static_cast<StructureKind>(k)) > 0);
    writeVarint(w, m.items.size());
    for (const auto& [i, n] : m.items) {
        w.writeBits(uint64_t(i), 3);
        writeSigned(w, n);
    }
    writeVarint(w, m.itemReady.size());
    for (const auto& [i, at] : m.itemReady) {
        w.writeBits(uint64_t(i), 3);
        codec::kMs.write(w, at);
    }
    codec::kMs.write(w, m.shieldUntil);
    codec::MarksCodec{}.write(w, m.citadelMarks);
    w.writeBool(m.flare.has_value());
    if (m.flare) {
        writeString(w, m.flare->region);
        codec::Fixed{1000}.write(w, m.flare->at.x);
        codec::Fixed{1000}.write(w, m.flare->at.y);
        codec::Fixed{1000}.write(w, m.flare->radius);
        codec::kMs.write(w, m.flare->until);
    }
    for (const auto* list : {&m.board, &m.journal}) {
        writeVarint(w, list->size());
        for (const CitadelMission& x : *list) writeMission(w, x);
    }
    writeVarint(w, m.mercs.size());
    for (const auto& x : m.mercs) {
        writeVarint(w, x.id);
        writeString(w, x.name);
        w.writeBits(uint64_t(x.state), 2);
        writeString(w, x.region);
        writeSigned(w, x.field);
        codec::kMs.write(w, x.downUntil);
        writeSigned(w, x.delivered);
        writeSigned(w, x.unit);
        w.writeBool(x.guard);
        w.writeBool(x.hunting);
    }
    writeVarint(w, m.mercFields.size());
    for (const auto& f : m.mercFields) {
        writeString(w, f.region);
        writeString(w, f.regionName);
        writeSigned(w, f.field);
        codec::Fixed{1000}.write(w, f.at.x);
        codec::Fixed{1000}.write(w, f.at.y);
        writeSigned(w, f.deposits);
        writeSigned(w, f.ore);
    }
    writeSigned(w, m.mercLimit);
    writeSigned(w, m.mercFee.ore);
    writeSigned(w, m.mercFee.hydrogen);
    writeSigned(w, m.mercWage);
    codec::Fixed{1000}.write(w, m.mercRate);
    writeSigned(w, m.mercNextLevel);
    writeVarint(w, m.mercTags.size());
    for (const auto& [id, name] : m.mercTags) {
        writeSigned(w, id);
        writeString(w, name);
    }
    writeStrings(w, m.regionsSeen);
    writeVarint(w, m.frontier.size());
    for (const auto& f : m.frontier) {
        writeVarint(w, f.id);
        writeString(w, f.callsign);
        w.writeBits(uint64_t(f.habit), 2);
        w.writeBits(uint64_t(f.paint), 3);
    }
    writeVarint(w, m.projects.size());
    for (const auto& p : m.projects) {
        writeSigned(w, p.runs);
        writeDouble(w, p.ends);
        writeString(w, p.by);
    }
    w.writeBool(m.away.has_value());
    if (m.away) {
        writeDouble(w, m.away->ran);
        writeDouble(w, m.away->back);
        writeSigned(w, m.away->realmOre);
        writeSigned(w, m.away->realmHydrogen);
        writeVarint(w, m.away->rows.size());
        for (const AwayRow& a : m.away->rows) {
            writeDouble(w, a.at);
            w.writeBits(uint64_t(a.kind), 4);
            writeString(w, a.who);
            writeString(w, a.what);
            writeString(w, a.where);
            writeSigned(w, a.value);
        }
    }
    w.writeBool(m.mule.has_value());
    if (m.mule) {
        writeString(w, m.mule->region);
        writeSigned(w, m.mule->unit);
        writeSigned(w, m.mule->foundry);
        writeDouble(w, m.mule->readyAt);
    }
    writeVarint(w, m.news.size());
    for (const auto& n : m.news) {
        w.writeBits(uint64_t(n.kind), 5);  // 5 bits since N3: the sixteenth kind filled 4
        writeString(w, n.id);
        codec::Fixed{1000}.write(w, n.xp);
        writeSigned(w, n.value);
        writeString(w, n.by);
    }
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, ProgressMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::progress, r)) return false;
    m = ProgressMessage{};
    codec::Fixed{1000}.read(r, m.xp);
    m.level = int64_t(readVarint(r));
    m.points = readSigned(r);
    const uint64_t ranks = readVarint(r);
    if (ranks > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < ranks; i++) {
        Skill s{};
        if (!readSkill(r, s)) return false;
        m.ranks[s] = readSigned(r);
    }
    for (auto& slot : m.slots) {
        if (!r.readBool()) continue;
        Skill s{};
        if (!readSkill(r, s)) return false;
        slot = s;
    }
    const uint64_t ready = readVarint(r);
    if (ready > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < ready; i++) {
        Skill s{};
        if (!readSkill(r, s)) return false;
        codec::kMs.read(r, m.ready[s]);
    }
    const uint64_t undo = readVarint(r);
    if (undo > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < undo; i++) {
        ProgressMessage::Undoable u;
        if (!readSkill(r, u.skill)) return false;
        codec::kMs.read(r, u.until);
        m.undo.push_back(u);
    }
    m.respecPrice = readSigned(r);
    m.respecFree = r.readBool();
    const uint64_t blueprints = readVarint(r);
    if (blueprints > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < blueprints; i++) {
        const uint64_t k = r.readBits(codec::kStructureKindBits);
        if (k >= uint64_t(allCases<StructureKind>().size())) return false;
        m.blueprints[static_cast<StructureKind>(k)] = readSigned(r);
    }
    if (!readStrings(r, m.sitesKnown) || !readStrings(r, m.sitesFound)) return false;
    if (r.readBool()) {
        ProgressMessage::Opening o;
        if (!readString(r, o.site)) return false;
        codec::kMs.read(r, o.since);
        codec::kMs.read(r, o.needs);
        if (!readString(r, o.refusal)) return false;
        m.opening = o;
    }
    const uint64_t contracts = readVarint(r);
    if (contracts > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < contracts; i++) {
        Contract c;
        c.id = static_cast<uint32_t>(readVarint(r));
        const uint64_t kind = r.readBits(3);
        const uint64_t state = r.readBits(2);
        c.step = static_cast<uint8_t>(r.readBits(3));
        if (kind > uint64_t(Contract::Kind::skill) || state > uint64_t(Contract::State::abandoned)) return false;
        c.kind = static_cast<Contract::Kind>(kind);
        c.state = static_cast<Contract::State>(state);
        if (!readString(r, c.title) || !readString(r, c.pitch) || !readString(r, c.target) || !readString(r, c.region))
            return false;
        c.need = readSigned(r);
        c.have = readSigned(r);
        codec::Fixed{1000}.read(r, c.xp);
        c.hydrogen = readSigned(r);
        m.contracts.push_back(std::move(c));
    }
    codec::kMs.read(r, m.contractsDawn);
    if (r.readBool()) {
        Goal g;
        if (!readString(r, g.key) || !readString(r, g.title) || !readString(r, g.pitch) || !readString(r, g.why) ||
            !readString(r, g.hint))
            return false;
        g.marked = r.readBool();
        if (!readString(r, g.region)) return false;
        codec::Fixed{1000}.read(r, g.at.x);
        codec::Fixed{1000}.read(r, g.at.y);
        g.level = readSigned(r);
        codec::Fixed{1000}.read(r, g.xp);
        g.hydrogen = readSigned(r);
        m.goal = std::move(g);
    }
    if (r.readBool()) {
        ProgressMessage::Respawn s;
        codec::kMs.read(r, s.at);
        if (!readString(r, s.region)) return false;
        if (r.readBool()) {
            const uint64_t k = r.readBits(codec::kStructureKindBits);
            if (k >= uint64_t(allCases<StructureKind>().size())) return false;
            s.home = static_cast<StructureKind>(k);
        }
        s.cargo = readSigned(r);
        s.hydrogen = r.readBool();
        m.respawn = s;
    }
    if (!readRig(r, m.rig)) return false;
    m.foundry = r.readBool();
    m.lab = r.readBool();
    const uint64_t realm = readVarint(r);
    if (realm > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < realm; i++) {
        const uint64_t k = r.readBits(codec::kStructureKindBits);
        if (k >= uint64_t(allCases<StructureKind>().size())) return false;
        m.realm[static_cast<StructureKind>(k)] = readSigned(r);
    }
    const uint64_t builders = readVarint(r);
    if (builders > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < builders; i++) {
        const int64_t id = readSigned(r);
        if (!readString(r, m.builders[id])) return false;
    }
    if (!readString(r, m.callsign)) return false;
    const uint64_t kills = readVarint(r);
    if (kills > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < kills; i++) {
        const int64_t id = readSigned(r);
        m.turretKills[id] = readSigned(r);
    }
    for (auto* p : {&m.realmWallet, &m.taxed, &m.donated}) {
        p->ore = readSigned(r);
        p->hydrogen = readSigned(r);
    }
    m.taxPercent = readSigned(r);
    for (size_t k = 0; k < codec::kStructureKindCount; k++)
        if (r.readBool()) m.learned.insert(static_cast<StructureKind>(k));
    auto readItem = [&](Item& item) {
        const uint64_t i = r.readBits(3);
        if (i >= uint64_t(allCases<Item>().size())) return false;
        item = static_cast<Item>(i);
        return true;
    };
    const uint64_t items = readVarint(r);
    if (items > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < items; i++) {
        Item item{};
        if (!readItem(item)) return false;
        m.items[item] = readSigned(r);
    }
    const uint64_t itemReady = readVarint(r);
    if (itemReady > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < itemReady; i++) {
        Item item{};
        if (!readItem(item)) return false;
        codec::kMs.read(r, m.itemReady[item]);
    }
    codec::kMs.read(r, m.shieldUntil);
    codec::MarksCodec{}.read(r, m.citadelMarks);
    if (r.readBool()) {
        ProgressMessage::Flare f;
        if (!readString(r, f.region)) return false;
        codec::Fixed{1000}.read(r, f.at.x);
        codec::Fixed{1000}.read(r, f.at.y);
        codec::Fixed{1000}.read(r, f.radius);
        codec::kMs.read(r, f.until);
        m.flare = f;
    }
    for (auto* list : {&m.board, &m.journal}) {
        const uint64_t n = readVarint(r);
        if (n > r.bitsLeft()) return false;
        for (uint64_t i = 0; i < n; i++) {
            CitadelMission x;
            if (!readMission(r, x)) return false;
            list->push_back(std::move(x));
        }
    }
    const uint64_t mercs = readVarint(r);
    if (mercs > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < mercs; i++) {
        ProgressMessage::Merc x;
        x.id = static_cast<uint32_t>(readVarint(r));
        if (!readString(r, x.name)) return false;
        const uint64_t state = r.readBits(2);
        x.state = static_cast<ProgressMessage::Merc::State>(state);
        if (!readString(r, x.region)) return false;
        x.field = readSigned(r);
        codec::kMs.read(r, x.downUntil);
        x.delivered = readSigned(r);
        x.unit = readSigned(r);
        x.guard = r.readBool();
        x.hunting = r.readBool();
        m.mercs.push_back(std::move(x));
    }
    const uint64_t fields = readVarint(r);
    if (fields > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < fields; i++) {
        ProgressMessage::MercField f;
        if (!readString(r, f.region) || !readString(r, f.regionName)) return false;
        f.field = readSigned(r);
        codec::Fixed{1000}.read(r, f.at.x);
        codec::Fixed{1000}.read(r, f.at.y);
        f.deposits = readSigned(r);
        f.ore = readSigned(r);
        m.mercFields.push_back(std::move(f));
    }
    m.mercLimit = readSigned(r);
    m.mercFee.ore = readSigned(r);
    m.mercFee.hydrogen = readSigned(r);
    m.mercWage = readSigned(r);
    codec::Fixed{1000}.read(r, m.mercRate);
    m.mercNextLevel = readSigned(r);
    const uint64_t tags = readVarint(r);
    if (tags > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < tags; i++) {
        const int64_t id = readSigned(r);
        if (!readString(r, m.mercTags[id])) return false;
    }
    if (!readStrings(r, m.regionsSeen)) return false;
    const uint64_t pilots = readVarint(r);
    if (pilots > 64 || !r.ok()) return false;
    m.frontier.assign(size_t(pilots), {});
    for (auto& f : m.frontier) {
        f.id = uint32_t(readVarint(r));
        if (!readString(r, f.callsign)) return false;
        f.habit = static_cast<FrontierHabit>(r.readBits(2));
        f.paint = int32_t(r.readBits(3));
    }
    const uint64_t projects = readVarint(r);
    if (projects > 8 || !r.ok()) return false;
    m.projects.assign(size_t(projects), {});
    for (auto& p : m.projects) {
        p.runs = readSigned(r);
        p.ends = readDouble(r);
        if (!readString(r, p.by)) return false;
    }
    if (r.readBool()) {
        AwayReport a;
        a.ran = readDouble(r);
        a.back = readDouble(r);
        a.realmOre = readSigned(r);
        a.realmHydrogen = readSigned(r);
        const uint64_t rows = readVarint(r);
        if (rows > 256 || !r.ok()) return false;
        a.rows.assign(size_t(rows), {});
        for (AwayRow& row : a.rows) {
            row.at = readDouble(r);
            const uint64_t kind = r.readBits(4);
            if (kind > uint64_t(AwayKind::hunt)) return false;
            row.kind = static_cast<AwayKind>(kind);
            if (!readString(r, row.who) || !readString(r, row.what) || !readString(r, row.where)) return false;
            row.value = readSigned(r);
        }
        m.away = std::move(a);
    }
    if (r.readBool()) {
        ProgressMessage::Mule mu;
        if (!readString(r, mu.region)) return false;
        mu.unit = readSigned(r);
        mu.foundry = readSigned(r);
        mu.readyAt = readDouble(r);
        m.mule = std::move(mu);
    }
    const uint64_t news = readVarint(r);
    if (news > r.bitsLeft()) return false;
    for (uint64_t i = 0; i < news; i++) {
        ProgressMessage::News n;
        const uint64_t kind = r.readBits(5);
        if (kind > uint64_t(ProgressMessage::NewsKind::hunt)) return false;
        n.kind = static_cast<ProgressMessage::NewsKind>(kind);
        if (!readString(r, n.id)) return false;
        codec::Fixed{1000}.read(r, n.xp);
        n.value = readSigned(r);
        if (!readString(r, n.by)) return false;
        m.news.push_back(n);
    }
    return r.ok();
}

std::vector<uint8_t> encode(const StandingsMessage& m) {
    BitWriter w = start(MessageType::standings);
    writeDouble(w, m.weekStart);
    writeDouble(w, m.weekEnds);
    writeVarint(w, m.rows.size());
    for (const WeekStanding& s : m.rows) {
        writeString(w, s.callsign);
        w.writeBool(s.frontier);
        writeSigned(w, s.ore);
        writeSigned(w, s.bugs);
        writeSigned(w, s.waves);
        writeSigned(w, s.built);
        writeVarint(w, uint64_t(std::max<int64_t>(0, s.rank)));
    }
    w.writeBool(m.you.has_value());
    if (m.you) writeVarint(w, uint64_t(*m.you));
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, StandingsMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::standings, r)) return false;
    m = StandingsMessage{};
    m.weekStart = readDouble(r);
    m.weekEnds = readDouble(r);
    const uint64_t rows = readVarint(r);
    // The viewer's own past the most.
    if (rows > kStandingsMost + 1 || !r.ok()) return false;
    m.rows.assign(size_t(rows), {});
    for (WeekStanding& s : m.rows) {
        if (!readString(r, s.callsign)) return false;
        s.frontier = r.readBool();
        s.ore = readSigned(r);
        s.bugs = readSigned(r);
        s.waves = readSigned(r);
        s.built = readSigned(r);
        s.rank = int64_t(readVarint(r));
    }
    if (r.readBool()) {
        const uint64_t you = readVarint(r);
        if (you >= rows) return false;
        m.you = int64_t(you);
    }
    return r.ok();
}

std::vector<uint8_t> encode(const HuntMessage& m) {
    BitWriter w = start(MessageType::hunt);
    w.writeBits(uint64_t(m.state), 3);
    writeString(w, m.caller);
    w.writeBool(m.region.has_value());
    if (m.region) writeVarint(w, uint64_t(std::max<int64_t>(0, *m.region)));
    writeString(w, m.regionName);
    for (const double v : {m.x, m.z, m.ring, m.calledAt, m.gatherEnds, m.chargedAt, m.failsAt, m.endedAt, m.nextCall}) writeDouble(w, v);
    writeString(w, m.refusal);
    writeVarint(w, m.party.size());
    for (const HuntHunter& h : m.party) {
        w.writeBits(uint64_t(h.kind), 2);
        writeString(w, h.name);
        writeString(w, h.owner);
        writeSigned(w, h.paint);
        w.writeBool(h.region.has_value());
        if (h.region) writeVarint(w, uint64_t(std::max<int64_t>(0, *h.region)));
        writeDouble(w, h.x);
        writeDouble(w, h.z);
        w.writeBool(h.there);
        w.writeBool(h.down);
    }
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, HuntMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::hunt, r)) return false;
    m = HuntMessage{};
    const uint64_t state = r.readBits(3);
    if (state > uint64_t(HuntMessage::State::failed)) return false;
    m.state = static_cast<HuntMessage::State>(state);
    if (!readString(r, m.caller)) return false;
    if (r.readBool()) m.region = int64_t(readVarint(r));
    if (!readString(r, m.regionName)) return false;
    for (double* v : {&m.x, &m.z, &m.ring, &m.calledAt, &m.gatherEnds, &m.chargedAt, &m.failsAt, &m.endedAt, &m.nextCall}) *v = readDouble(r);
    if (!readString(r, m.refusal)) return false;
    const uint64_t party = readVarint(r);
    if (party > kHuntPartyMost || !r.ok()) return false;
    m.party.assign(size_t(party), {});
    for (HuntHunter& h : m.party) {
        const uint64_t kind = r.readBits(2);
        if (kind > uint64_t(HuntHunter::Kind::human)) return false;
        h.kind = static_cast<HuntHunter::Kind>(kind);
        if (!readString(r, h.name) || !readString(r, h.owner)) return false;
        h.paint = int32_t(readSigned(r));
        if (r.readBool()) h.region = int64_t(readVarint(r));
        h.x = readDouble(r);
        h.z = readDouble(r);
        h.there = r.readBool();
        h.down = r.readBool();
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

std::vector<uint8_t> encode(const JoinMessage& m) {
    BitWriter w = start(MessageType::join);
    for (uint8_t b : m.pilot) w.writeU8(b);
    return w.bytes();
}

bool decode(const std::vector<uint8_t>& bytes, JoinMessage& m) {
    BitReader r(bytes.data(), bytes.size());
    if (!open(bytes, MessageType::join, r)) return false;
    for (uint8_t& b : m.pilot) b = r.readU8();
    return r.ok();
}

std::string pilotKeyHex(const PilotKey& key) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (uint8_t b : key) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 15]);
    }
    return out;
}

std::string pilotKeyShort(const PilotKey& key) { return pilotKeyHex(key).substr(0, 8); }

std::optional<PilotKey> pilotKeyFromHex(std::string_view hex) {
    if (hex.size() != 32) return std::nullopt;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    PilotKey out{};
    for (size_t i = 0; i < 16; i++) {
        const int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        out[i] = uint8_t(hi * 16 + lo);
    }
    return out;
}

PilotKey newPilotKey() {
    PilotKey out{};
    if (!secureRandom(out.data(), out.size())) out.fill(0);
    return out;
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

const char* title(FrontierHabit h) {
    switch (h) {
    case FrontierHabit::miner: return "Miner";
    case FrontierHabit::builder: return "Builder";
    case FrontierHabit::defender: return "Defender";
    case FrontierHabit::wanderer: return "Wanderer";
    }
    return "";
}

const char* title(FrontierDoing d) {
    switch (d) {
    case FrontierDoing::coming: return "on the way in";
    case FrontierDoing::mining: return "mining";
    case FrontierDoing::hauling: return "hauling ore";
    case FrontierDoing::pumping: return "pumping MH";
    case FrontierDoing::buying: return "at the Citadel shop";
    case FrontierDoing::building: return "building";
    case FrontierDoing::patrolling: return "on patrol";
    case FrontierDoing::fighting: return "fighting bugs";
    case FrontierDoing::repairing: return "mending a building";
    case FrontierDoing::walking: return "on the road";
    case FrontierDoing::flying: return "aboard the Dropship";
    case FrontierDoing::opening: return "opening a site";
    case FrontierDoing::resting: return "resting";
    case FrontierDoing::down: return "down, coming back";
    }
    return "";
}

} // namespace ac::net
