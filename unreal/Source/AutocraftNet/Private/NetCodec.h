// The protocol's value codecs and the field lists of the core's types
// (NetProtocol.h, NetSnapshot.h). A codec writes one value in its quantised
// form and reads it back; a field list walks a struct's fields in a fixed
// order with a visitor, the same list for writing, reading and deltas.
#pragma once

#include "NetProtocol.h"

#include "Rules.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <type_traits>
#include <vector>

namespace ac::net::codec {

/// The bits that hold the values 0..n-1.
constexpr int bitsFor(size_t n) {
    int bits = 0;
    while ((size_t(1) << bits) < n) bits++;
    return bits;
}
/// The wire's unit and structure kinds take their widths from the counts: a new
/// kind can widen them, which changes the bytes (bump kProtocolVersion).
/// `kUnit…`, not `kKind…`: NetSnapshot.cpp's kKindBits is the item kind's width.
constexpr size_t kUnitKindCount = EnumInfo<UnitKind>::names.size();
constexpr int kUnitKindBits = bitsFor(kUnitKindCount);
static_assert((size_t(1) << kUnitKindBits) >= kUnitKindCount);
static_assert(kUnitKindCount <= 64, "the driven-kinds mask is one uint64_t");
constexpr size_t kStructureKindCount = EnumInfo<StructureKind>::names.size();
constexpr int kStructureKindBits = bitsFor(kStructureKindCount);
static_assert((size_t(1) << kStructureKindBits) >= kStructureKindCount);
/// C2: the Target Dummy is the 12th kind; the wire's 4 bits hold 16.
static_assert(kStructureKindBits == 4 && kStructureKindCount >= 12);
/// A unit's task takes 5 bits (C1: `atGun` is the 19th).
constexpr int kTaskBits = 5;
static_assert(EnumInfo<Unit::Task>::names.size() <= (size_t(1) << kTaskBits));

/// A double as a signed integer of 1/`scale` steps (non-finite: 0).
int64_t fixed(double v, double scale);

struct Var {
    void write(BitWriter& w, int64_t v) const { writeSigned(w, v); }
    void read(BitReader& r, int64_t& v) const { v = readSigned(r); }
};
struct Bool {
    void write(BitWriter& w, bool v) const { w.writeBool(v); }
    void read(BitReader& r, bool& v) const { v = r.readBool(); }
};
template <class E, int Bits> struct Enum {
    void write(BitWriter& w, E v) const { w.writeBits(static_cast<uint64_t>(v), Bits); }
    void read(BitReader& r, E& v) const { v = static_cast<E>(r.readBits(Bits)); }
};
/// Positions: centimetres, `q::kPositionBits` a coordinate, two's complement.
struct Pos {
    static constexpr bool position = true;
    void write(BitWriter& w, Vec2 v) const;
    void read(BitReader& r, Vec2& v) const;
};
/// A round's point (C2): `q::kPointSteps` a cell, x and y in
/// `q::kPointBits`, z in `q::kPointHeightBits`.
struct Point {
    void write(BitWriter& w, Vec3 v) const;
    void read(BitReader& r, Vec3& v) const;
};
/// A round's velocity (C2): `q::kVelocitySteps` a cell/s, `q::kVelocityBits`
/// an axis.
struct Velocity {
    void write(BitWriter& w, Vec3 v) const;
    void read(BitReader& r, Vec3& v) const;
};
/// 0…1 in 255ths (C2: a gun's heat, its barrels' spin).
struct Share {
    void write(BitWriter& w, double v) const;
    void read(BitReader& r, double& v) const;
};
struct Angle {
    void write(BitWriter& w, double v) const { w.writeU16(q::angle(v)); }
    void read(BitReader& r, double& v) const { v = q::angle(r.readU16()); }
};
/// A double in 1/`scale` steps, as a signed varint.
struct Fixed {
    double scale = 1000;
    void write(BitWriter& w, double v) const { writeSigned(w, fixed(v, scale)); }
    void read(BitReader& r, double& v) const { v = double(readSigned(r)) / scale; }
};
/// Game times and durations: milliseconds.
inline constexpr Fixed kMs{1000};
/// Lengths: centimetres.
inline constexpr Fixed kCm{100};
/// Shares and small angles: 1/10000.
inline constexpr Fixed kFine{10000};
/// A time that may be ±infinity (a site never seen), in milliseconds.
struct Time {
    void write(BitWriter& w, double v) const;
    void read(BitReader& r, double& v) const;
};
/// Hit points and shields: `q::kHealthBits` over 0…range, else exact.
struct Health {
    double range = 1;
    void write(BitWriter& w, double v) const;
    void read(BitReader& r, double& v) const;
};
template <class C> struct Opt {
    C c{};
    template <class T> void write(BitWriter& w, const std::optional<T>& v) const {
        w.writeBool(v.has_value());
        if (v) c.write(w, *v);
    }
    template <class T> void read(BitReader& r, std::optional<T>& v) const {
        if (r.readBool()) {
            T x{};
            c.read(r, x);
            v = std::move(x);
        } else {
            v = std::nullopt;
        }
    }
};
template <class C> struct Vec {
    C c{};
    template <class T> void write(BitWriter& w, const std::vector<T>& v) const {
        writeVarint(w, v.size());
        for (const T& x : v) c.write(w, x);
    }
    template <class T> void read(BitReader& r, std::vector<T>& v) const {
        const uint64_t n = readVarint(r);
        v.clear();
        // A length past what is left is a broken message.
        if (n > r.bitsLeft()) {
            r.readBits(64);
            r.readBits(64);
            return;
        }
        v.resize(static_cast<size_t>(n));
        for (T& x : v) c.read(r, x);
    }
};
/// A set of upgrades: one bit each.
struct Upgrades {
    void write(BitWriter& w, const std::set<Upgrade>& v) const;
    void read(BitReader& r, std::set<Upgrade>& v) const;
};
/// A struct written with its own field list (`fields(v, x)`), whole.
template <class T> struct Whole {
    void write(BitWriter& w, const T& v) const;
    void read(BitReader& r, T& v) const;
};

// MARK: - Field lists

struct WriteAll {
    BitWriter& w;
    template <class C, class T> void operator()(const C& c, const T& v) { c.write(w, v); }
};
struct ReadAll {
    BitReader& r;
    template <class C, class T> void operator()(const C& c, T& v) { c.read(r, v); }
};

template <class V, class X> void buildOrderFields(V& v, X& x) {
    v(Enum<StructureKind, kStructureKindBits>{}, x.kind);
    v(Pos{}, x.position);
}
template <class V> void fields(V& v, const BuildOrder& x) { buildOrderFields<V, const BuildOrder>(v, x); }
template <class V> void fields(V& v, BuildOrder& x) { buildOrderFields<V, BuildOrder>(v, x); }

template <class V, class X> void rushFields(V& v, X& x) {
    v(kMs, x.seconds);
    v(kFine, x.speed);
    v(kFine, x.fireRate);
}
template <class V> void fields(V& v, const Rush& x) { rushFields<V, const Rush>(v, x); }
template <class V> void fields(V& v, Rush& x) { rushFields<V, Rush>(v, x); }

template <class V, class X> void burningFields(V& v, X& x) {
    v(kMs, x.until);
    v(Fixed{100}, x.perSecond);
    v(Var{}, x.by);
}
template <class V> void fields(V& v, const Burning& x) { burningFields<V, const Burning>(v, x); }
template <class V> void fields(V& v, Burning& x) { burningFields<V, Burning>(v, x); }

/// A unit's errand: its case, then the case's fields.
struct MissionCodec {
    void write(BitWriter& w, const Mission& m) const;
    void read(BitReader& r, Mission& m) const;
};

/// A bug of the Brood (docs/creatures.md, "On the wire"): only what the
/// client draws and plays, never its orders. The state (3 bits), its height
/// (6 bits, 0.1 cells), its strikes (4 bits) and its sac (4 bits, 0..15 of
/// `Brood::sacFull`).
struct BugCodec {
    void write(BitWriter& w, const Bug& b) const;
    void read(BitReader& r, Bug& b) const;
};
/// A defence's marks (D1, D3): 0 to 3 each, 2 bits.
struct MarksCodec {
    void write(BitWriter& w, const TurretMarks& m) const;
    void read(BitReader& r, TurretMarks& m) const;
};
/// A look's pitch (C1): `q::kPitchBits` over ±90°.
struct Pitch {
    void write(BitWriter& w, double v) const { w.writeBits(q::pitch(v), q::kPitchBits); }
    void read(BitReader& r, double& v) const { v = q::pitch(uint8_t(r.readBits(q::kPitchBits))); }
};
/// An armed Citadel's corner turrets (D1): where each points (C1: its yaw,
/// its pitch) and who mans it, and (C2) its heat, whether it is overheated
/// and its barrels' spin; their targets, reloads and spread stay on the
/// server.
struct GunsCodec {
    void write(BitWriter& w, const std::array<CornerGun, 4>& g) const {
        for (const CornerGun& x : g) {
            Angle{}.write(w, x.aim);
            Pitch{}.write(w, x.pitch);
            Opt<Var>{}.write(w, x.gunner);
            Share{}.write(w, x.heat);
            w.writeBool(x.overheated);
            Share{}.write(w, x.spin);
        }
    }
    void read(BitReader& r, std::array<CornerGun, 4>& g) const {
        for (CornerGun& x : g) {
            x = CornerGun{};
            Angle{}.read(r, x.aim);
            Pitch{}.read(r, x.pitch);
            Opt<Var>{}.read(r, x.gunner);
            Share{}.read(r, x.heat);
            x.overheated = r.readBool();
            Share{}.read(r, x.spin);
        }
    }
};
/// A Citadel's door (C1): how far open in 255ths, and the way it moves.
struct DoorCodec {
    void write(BitWriter& w, const Door& d) const {
        const double c = std::isfinite(d.open) ? std::clamp(d.open, 0.0, 1.0) : 0.0;
        w.writeU8(static_cast<uint8_t>(std::llround(c * 255)));
        w.writeBool(d.opening);
    }
    void read(BitReader& r, Door& d) const {
        d = Door{};
        d.open = double(r.readU8()) / 255;
        d.opening = r.readBool();
    }
};

/// Every field of a unit, in wire order. A new field of `Unit` goes here.
template <class V, class X> void unitFields(V& v, X& u) {
    v(Var{}, u.id);
    v(Enum<UnitKind, kUnitKindBits>{}, u.kind);
    v(Var{}, u.owner);
    v(Pos{}, u.position);
    v(Health{2 * Rules::hp(u.kind)}, u.hp);
    v(Angle{}, u.heading);
    v(Enum<Unit::Task, kTaskBits>{}, u.task);
    v(Opt<Var>{}, u.patch);
    v(kMs, u.timer);
    v(Var{}, u.carrying);
    v(kCm, u.stride);
    v(Opt<Whole<BuildOrder>>{}, u.order);
    v(Opt<Var>{}, u.structure);
    v(Opt<Pos>{}, u.goal);
    v(Opt<Vec<Pos>>{}, u.waypoints);
    v(Opt<Var>{}, u.target);
    v(Opt<Fixed>{kMs}, u.cooldown);
    v(Opt<Bool>{}, u.moving);
    v(Opt<Bool>{}, u.hydrogen);
    v(Opt<Fixed>{kMs}, u.hitAt);
    v(Opt<Fixed>{kMs}, u.slowUntil);
    v(Opt<Fixed>{kFine}, u.slowedTo);
    v(Opt<Fixed>{kFine}, u.anchor);
    v(Opt<Bool>{}, u.anchored);
    v(Opt<Angle>{}, u.aim);
    v(Opt<Pos>{}, u.jumpFrom);
    v(Opt<Pos>{}, u.jumpTo);
    v(Opt<Fixed>{Fixed{100}}, u.energy);
    v(Opt<Vec<Var>>{}, u.cargo);
    v(Opt<MissionCodec>{}, u.mission);
    v(Opt<Bool>{}, u.autoFollow);
    v(Opt<Fixed>{kMs}, u.burstFrom);
    v(Opt<Fixed>{kMs}, u.firedAt);
    v(Opt<Fixed>{kFine}, u.spread);
    v(Opt<Fixed>{kMs}, u.railCooldown);
    v(Opt<Pos>{}, u.shotFrom);
    v(Opt<Health>{Health{Rules::hp(u.kind)}}, u.shield);
    v(Opt<Health>{Health{Rules::hp(u.kind)}}, u.bonusHP);
    v(Opt<Pos>{}, u.boardedAt);
    v(Opt<Whole<Rush>>{}, u.rush);
    v(Opt<Fixed>{kMs}, u.rushUntil);
    v(Opt<Whole<Burning>>{}, u.burning);
    v(Opt<Var>{}, u.lockTarget);
    v(Opt<Fixed>{kMs}, u.lockFrom);
    v(Opt<Fixed>{kMs}, u.lockAt);
    v(Opt<Fixed>{kMs}, u.stompReady);
    v(Opt<Fixed>{kMs}, u.stompedAt);
    v(Opt<Fixed>{kMs}, u.struckAt);
    v(Opt<BugCodec>{}, u.bug);
}
template <class V> void fields(V& v, const Unit& x) { unitFields<V, const Unit>(v, x); }
template <class V> void fields(V& v, Unit& x) { unitFields<V, Unit>(v, x); }

template <class V, class X> void structureFields(V& v, X& b) {
    v(Var{}, b.id);
    v(Enum<StructureKind, kStructureKindBits>{}, b.kind);
    v(Var{}, b.owner);
    v(Pos{}, b.position);
    v(Health{2 * Rules::hp(b.kind)}, b.hp);
    v(Opt<Fixed>{kMs}, b.buildLeft);
    v(Opt<Var>{}, b.builder);
    v(Opt<Fixed>{kMs}, b.training);
    v(Opt<Vec<Enum<UnitKind, kUnitKindBits>>>{}, b.line);
    v(Opt<Vec<Var>>{}, b.crew);
    v(Opt<Var>{}, b.addon);
    v(Opt<Var>{}, b.parent);
    v(Opt<Enum<Upgrade, 2>>{}, b.research);
    v(Opt<Fixed>{kMs}, b.researchLeft);
    v(Opt<Var>{}, b.target);
    v(Opt<Fixed>{kMs}, b.cooldown);
    v(Opt<Angle>{}, b.aim);
    v(Opt<MarksCodec>{}, b.marks);
    v(Opt<GunsCodec>{}, b.guns);
    v(Opt<DoorCodec>{}, b.door);
    // C2: a dummy's hits since it stood and when it stands again; a
    // gatling's heat, overheat and spin (its spread stays on the server).
    v(Var{}, b.hits);
    v(Opt<Fixed>{kMs}, b.downUntil);
    v(Share{}, b.heat);
    v(Bool{}, b.overheated);
    v(Share{}, b.spin);
}
template <class V> void fields(V& v, const Structure& x) { structureFields<V, const Structure>(v, x); }
template <class V> void fields(V& v, Structure& x) { structureFields<V, Structure>(v, x); }

template <class V, class X> void patchFields(V& v, X& p) {
    v(Var{}, p.id);
    v(Pos{}, p.position);
    v(Angle{}, p.angle);
    v(Var{}, p.initial);
    v(Var{}, p.remaining);
    v(Opt<Fixed>{kMs}, p.depletedAt);
    v(Opt<Var>{}, p.miner);
}
template <class V> void fields(V& v, const OreDeposit& x) { patchFields<V, const OreDeposit>(v, x); }
template <class V> void fields(V& v, OreDeposit& x) { patchFields<V, OreDeposit>(v, x); }

template <class V, class X> void wellFields(V& v, X& w) {
    v(Pos{}, w.position);
    v(Var{}, w.remaining);
    v(Opt<Var>{}, w.harvester);
}
template <class V> void fields(V& v, const Well& x) { wellFields<V, const Well>(v, x); }
template <class V> void fields(V& v, Well& x) { wellFields<V, Well>(v, x); }

/// A request's what: its case and its kind.
struct RequestWhat {
    void write(BitWriter& w, const Request::What& x) const;
    void read(BitReader& r, Request::What& x) const;
};

template <class V, class X> void requestFields(V& v, X& q) {
    v(Var{}, q.id);
    v(RequestWhat{}, q.what);
    v(Var{}, q.count);
    v(Bool{}, q.repeats);
    v(Opt<Var>{}, q.at);
    v(Opt<Var>{}, q.site);
}
template <class V> void fields(V& v, const Request& x) { requestFields<V, const Request>(v, x); }
template <class V> void fields(V& v, Request& x) { requestFields<V, Request>(v, x); }

template <class V, class X> void objectiveFields(V& v, X& o) {
    v(Var{}, o.id);
    v(Enum<Objective::Kind, 2>{}, o.kind);
    v(Pos{}, o.at);
    v(Opt<Var>{}, o.structure);
}
template <class V> void fields(V& v, const Objective& x) { objectiveFields<V, const Objective>(v, x); }
template <class V> void fields(V& v, Objective& x) { objectiveFields<V, Objective>(v, x); }

template <class V, class X> void directivesFields(V& v, X& d) {
    v(Vec<Whole<Request>>{}, d.queue);
    v(Var{}, d.keepOre);
    v(Var{}, d.keepHydrogen);
    v(Enum<Harvest, 2>{}, d.harvest);
    v(Vec<Whole<Objective>>{}, d.objectives);
    v(Enum<Stance, 2>{}, d.stance);
    v(Var{}, d.nextID);
}
template <class V> void fields(V& v, const Directives& x) { directivesFields<V, const Directives>(v, x); }
template <class V> void fields(V& v, Directives& x) { directivesFields<V, Directives>(v, x); }

/// A player but its pilot record (sent as items of their own, NetSnapshot.h):
/// the record's kinds go as a bit mask.
struct PlayerCore {
    Player player;
    uint64_t kinds = 0;
    bool pilot = false;
};
template <class V, class X> void playerFields(V& v, X& c) {
    auto& p = c.player;
    v(Var{}, p.ore);
    v(Var{}, p.hydrogen);
    v(Var{}, p.supplyUsed);
    v(Var{}, p.supplyCap);
    v(Var{}, p.totalMined);
    v(Opt<Pos>{}, p.attack);
    v(Bool{}, p.retreating);
    v(Opt<Fixed>{kMs}, p.retreatedAt);
    v(Opt<Bool>{}, p.defending);
    v(Opt<Var>{}, p.reinforce);
    v(Var{}, p.waves);
    v(kFine, p.aggression);
    v(Opt<Fixed>{kFine}, p.greed);
    v(Opt<Var>{}, p.garrisonPerBase);
    v(Opt<Var>{}, p.comets);
    v(Opt<Var>{}, p.start);
    v(Opt<Enum<Player::Style, 2>>{}, p.style);
    v(Opt<Var>{}, p.fireflies);
    v(Opt<Bool>{}, p.drops);
    v(Opt<Upgrades>{}, p.upgrades);
    v(Opt<Whole<Directives>>{}, p.directives);
    v(Opt<Var>{}, p.unitsLost);
    v(Opt<Var>{}, p.team);
    v(Bool{}, c.pilot);
    v(Enum<uint64_t, int(kUnitKindCount)>{}, c.kinds);
}
template <class V> void fields(V& v, const PlayerCore& x) { playerFields<V, const PlayerCore>(v, x); }
template <class V> void fields(V& v, PlayerCore& x) { playerFields<V, PlayerCore>(v, x); }

/// A kind's record but its stamps (items of their own): their count.
struct KindCore {
    KindRecord record;
    int64_t stamps = 0;
};
template <class V, class X> void kindFields(V& v, X& c) {
    auto& k = c.record;
    v(kMs, k.xp);
    v(Vec<Enum<Perk, 8>>{}, k.picks);
    v(Var{}, k.count);
    v(Opt<Fixed>{kMs}, k.burstUntil);
    v(Opt<Fixed>{kMs}, k.burstReady);
    v(Opt<Fixed>{kMs}, k.healedAt);
    auto& t = k.tally;
    v(kMs, t.seconds);
    v(Var{}, t.takeovers);
    v(kCm, t.unitDamage);
    v(kCm, t.buildingDamage);
    v(Var{}, t.kills);
    v(Var{}, t.razed);
    v(kCm, t.damageTaken);
    v(kCm, t.shieldAbsorbed);
    v(Var{}, t.deaths);
    v(Var{}, t.ore);
    v(Var{}, t.hydrogen);
    v(kCm, t.built);
    v(kCm, t.repaired);
    v(kCm, t.healed);
    v(Var{}, t.carried);
    v(kCm, t.burn);
    v(Var{}, t.blastHits);
    v(Var{}, t.airKills);
    v(Var{}, t.stomps);
    v(Var{}, t.stompHits);
    v(Var{}, t.strikes);
    v(Var{}, t.strikeHits);
    v(kMs, t.hiddenSeconds);
    v(Var{}, c.stamps);
}
template <class V> void fields(V& v, const KindCore& x) { kindFields<V, const KindCore>(v, x); }
template <class V> void fields(V& v, KindCore& x) { kindFields<V, KindCore>(v, x); }

template <class V, class X> void standingFields(V& v, X& s) {
    v(Var{}, s.army);
    v(Var{}, s.workers);
    v(Var{}, s.ore);
    v(Var{}, s.hydrogen);
    v(Var{}, s.supplyUsed);
    v(Var{}, s.supplyCap);
    v(Var{}, s.buildings);
    v(Var{}, s.mined);
}
template <class V> void fields(V& v, const Standing& x) { standingFields<V, const Standing>(v, x); }
template <class V> void fields(V& v, Standing& x) { standingFields<V, Standing>(v, x); }

template <class V, class X> void stampFields(V& v, X& s) {
    v(Var{}, s.level);
    v(kMs, s.at);
    v(kMs, s.driven);
    v(Vec<Whole<Standing>>{}, s.standing);
    v(Opt<Fixed>{kMs}, s.picked);
}
template <class V> void fields(V& v, const Stamp& x) { stampFields<V, const Stamp>(v, x); }
template <class V> void fields(V& v, Stamp& x) { stampFields<V, Stamp>(v, x); }

/// A base site's last-seen time, as an item of its own.
struct SiteTime {
    double at = 0;
};
template <class V> void fields(V& v, const SiteTime& x) { v(Time{}, x.at); }
template <class V> void fields(V& v, SiteTime& x) { v(Time{}, x.at); }

template <class T> void Whole<T>::write(BitWriter& w, const T& v) const {
    WriteAll a{w};
    fields(a, v);
}
template <class T> void Whole<T>::read(BitReader& r, T& v) const {
    ReadAll a{r};
    fields(a, v);
}

struct Count {
    size_t n = 0;
    template <class C, class X> void operator()(const C&, X&) { n += 1; }
};

/// The fields of a type, counted.
template <class T> size_t fieldCount() {
    static const size_t n = [] {
        Count c;
        T x{};
        fields(c, x);
        return c.n;
    }();
    return n;
}

} // namespace ac::net::codec
