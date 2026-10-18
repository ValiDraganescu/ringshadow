// Port of `Rules` of Sources/GameCore/State.swift and Vision.swift.
#include "Rules.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace ac {

double Rules::flight(UnitKind kind, bool anchored, double d) {
    // An anchored kind that does not bury (a Longbow) fires its shock
    // cannon (`UnitStats::anchor`); unanchored, a Longbow's 90 mm guns and an
    // Atlas's shells hit at once.
    const UnitStats& k = stats(kind);
    const UnitStats& w = anchored && k.anchors && !k.burrows ? UnitStats::anchor : k;
    double t = w.shotFlightBase;
    if (w.shotFlightPerCell != 0) t += d * w.shotFlightPerCell;
    if (w.shotSpeed > 0) t += d / w.shotSpeed;
    return t;
}

int64_t Rules::cost(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 400; case StructureKind::habDome: return 100;
    case StructureKind::garrison: return 150; case StructureKind::bastion: return 100;
    case StructureKind::derrick: return 75; case StructureKind::foundry: case StructureKind::spacedock: return 150;
    case StructureKind::lab: return 50; case StructureKind::sentinel: return 100;
    case StructureKind::gatling: return 100; case StructureKind::landingPad: return 50;
    case StructureKind::dummy: return 10;
    }
    return 0;
}

int64_t Rules::hydrogenCost(StructureKind k) {
    switch (k) {
    case StructureKind::foundry: case StructureKind::spacedock: return 100;
    case StructureKind::lab: return 25;
    default: return 0;
    }
}

double Rules::buildTime(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 71; case StructureKind::habDome: return 21;
    case StructureKind::garrison: return 46; case StructureKind::bastion: return 29;
    case StructureKind::derrick: return 21; case StructureKind::foundry: return 43;
    case StructureKind::spacedock: return 36; case StructureKind::lab: return 18; case StructureKind::sentinel: return 18;
    case StructureKind::gatling: return 18; case StructureKind::landingPad: return 15;
    case StructureKind::dummy: return 2;
    }
    return 0;
}

Vec3 Rules::roundVelocity(double yaw, double pitch) {
    const double c = std::cos(pitch);
    return {std::cos(yaw) * c * roundSpeed, std::sin(yaw) * c * roundSpeed, std::sin(pitch) * roundSpeed};
}

std::optional<double> Rules::ballisticPitch(double ahead, double rise) {
    // tan θ = (v² − √(v⁴ − g(g x² + 2 y v²))) / (g x), the lower root.
    const double v2 = roundSpeed * roundSpeed, g = roundGravity;
    if (!(ahead > 1e-9)) return rise >= 0 ? pi / 2 : -pi / 2;
    const double disc = v2 * v2 - g * (g * ahead * ahead + 2 * rise * v2);
    if (disc < 0) return std::nullopt;
    return std::atan((v2 - std::sqrt(disc)) / (g * ahead));
}

int64_t Rules::supply(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 15;
    case StructureKind::habDome: return 8;
    default: return 0;
    }
}

namespace {
// Read on every region's thread, written only at a game's start.
std::atomic<bool> hall{true};
} // namespace

void Rules::setCitadelHall(bool on) { hall.store(on, std::memory_order_relaxed); }
bool Rules::citadelHall() { return hall.load(std::memory_order_relaxed); }

/// Half the footprint side: 12x12 Citadel with the hall (5x5 without);
/// 3x3 Garrison, Bastion, Derrick, Foundry, Spacedock and Landing Pad; 2x2
/// Hab Dome, Lab and Sentinel; 1x1 Target Dummy.
double Rules::radius(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return citadelHall() ? 6.0 : 2.5;
    case StructureKind::dummy: return 0.5;
    case StructureKind::habDome: case StructureKind::lab: case StructureKind::sentinel: case StructureKind::gatling: return 1;
    default: return 1.5;
    }
}

double Rules::height(StructureKind k) {
    // GameScene.makeBuilding (GameScene.swift:363); Models.labHeight 1.9.
    switch (k) {
    // The hall's: the central tower's beacon top (c1-model, citadel.py).
    case StructureKind::citadel: return citadelHall() ? 12.3 : 4.6;
    case StructureKind::habDome: return 1.2;
    case StructureKind::garrison: return 2.5;
    case StructureKind::bastion: return 1.4;
    case StructureKind::derrick: return 2.5;
    case StructureKind::foundry: return 2.4;
    case StructureKind::spacedock: return 3.2;
    case StructureKind::lab: return 1.9;
    case StructureKind::sentinel: return 1.7;
    case StructureKind::gatling: return 1.3;
    case StructureKind::landingPad: return 0.18;
    // The post and its plate (dummy.py).
    case StructureKind::dummy: return 1.8;
    }
    return 2.0;
}

bool Footprint::blocks(Vec2 local, bool doorOpen, double grow) const {
    for (const Slab& w : walls) {
        // A slab off the ground (the lintel over the door) is overhead.
        if (w.z0 <= 0 && w.contains(local, grow)) return true;
    }
    return door && !doorOpen && door->contains(local, grow);
}

bool Footprint::inside(Vec2 local) const { return interior && interior->contains(local); }

const Footprint& Rules::footprint(StructureKind k) {
    // docs/citadel-interior.md section 2. With the hall, the Citadel's walls
    // 1 thick round a 10x10 hall 6 high, the roof slab 6 to 7, the doorway
    // in the +y face 3 wide and 3 tall under a lintel. Every other kind (and
    // the Citadel without the hall): its whole square, as tall as its body.
    using Table = std::array<Footprint, size_t(StructureKind::dummy) + 1>;
    auto make = [](bool withHall) {
        Table t;
        for (size_t i = 0; i < t.size(); i++) {
            const StructureKind kind = StructureKind(i);
            Footprint& f = t[i];
            if (kind != StructureKind::citadel || !withHall) {
                f.radius = kind == StructureKind::citadel ? 2.5 : radius(kind);
                const double z1 = kind == StructureKind::citadel ? 4.6 : height(kind);
                f.walls = {Slab{{-f.radius, -f.radius}, {f.radius, f.radius}, 0, z1}};
                continue;
            }
            f.radius = 6.0;
            f.walls = {
                Slab{{-6, 5}, {-1.5, 6}, 0, 7},  // +y, left of the door
                Slab{{1.5, 5}, {6, 6}, 0, 7},    // +y, right of the door
                Slab{{-1.5, 5}, {1.5, 6}, 3, 7}, // the lintel
                Slab{{-6, -6}, {6, -5}, 0, 7},   // -y
                Slab{{-6, -5}, {-5, 5}, 0, 7},   // -x
                Slab{{5, -5}, {6, 5}, 0, 7},     // +x
            };
            f.door = Slab{{-1.5, 5}, {1.5, 6}, 0, 3};
            f.interior = Slab{{-5, -5}, {5, 5}, 0, 6};
            f.roof = Slab{{-5, -5}, {5, 5}, 6, 7};
        }
        return t;
    };
    static const Table withHall = make(true), without = make(false);
    return (citadelHall() ? withHall : without)[size_t(k)];
}

/// A building needs one of these finished first (the tech tree).
std::optional<StructureKind> Rules::requires_(StructureKind k) {
    switch (k) {
    case StructureKind::garrison: return StructureKind::habDome;
    case StructureKind::bastion: case StructureKind::foundry: case StructureKind::sentinel: return StructureKind::garrison;
    case StructureKind::spacedock: return StructureKind::foundry;
    default: return std::nullopt;
    }
}

/// The units a building trains, the ones needing a Lab included.
/// In kind order (`UnitStats::trainedBy`).
std::vector<UnitKind> Rules::trains(StructureKind k) {
    std::vector<UnitKind> out;
    for (size_t i = 0; i < UnitStats::byKind.size(); i++) {
        if (UnitStats::byKind[i].trainedBy == k) out.push_back(static_cast<UnitKind>(i));
    }
    return out;
}

/// The unit a building trains when none is named (the first it trains).
std::optional<UnitKind> Rules::produces(StructureKind k) {
    auto t = trains(k);
    if (t.empty()) return std::nullopt;
    return t.front();
}

int64_t Rules::hpShown(double hp, double max) {
    if (!(hp > 0)) return 0;
    const double top = std::max(1.0, std::floor(max));
    return int64_t(std::clamp(std::round(hp), 1.0, top));
}

double Rules::hp(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 1500; case StructureKind::habDome: return 400;
    case StructureKind::garrison: return 1000; case StructureKind::bastion: return 400;
    case StructureKind::derrick: return 500; case StructureKind::foundry: return 1250;
    case StructureKind::spacedock: return 1300; case StructureKind::lab: return 400; case StructureKind::sentinel: return 250;
    case StructureKind::gatling: return 300; case StructureKind::landingPad: return 400;
    case StructureKind::dummy: return 150;
    }
    return 0;
}

/// The weapon of a building that shoots by itself (a Sentinel), if any.
std::optional<UnitStats> Rules::turret(StructureKind k) {
    if (k == StructureKind::sentinel) return UnitStats::sentinel;
    if (k == StructureKind::gatling) return UnitStats::gatling;
    return std::nullopt;
}

/// Damage a Ranger shot does after armour: buildings have 1 armour,
/// and a hit always does at least 0.5.
double Rules::damage(bool toStructure) { return max(0.5, rangerDamage - (toStructure ? 1 : 0)); }

/// Damage one attack of `attacker` does to a target with these traits:
/// per hit, the base plus any bonus against its attribute, less its
/// armour (at least 0.5), times the hits.
double Rules::damage(UnitKind attacker, bool anchored, bool armored, bool light, double armor) {
    return damage(anchored ? UnitStats::anchor : stats(attacker), armored, light, armor);
}

/// The same for weapon `a`.
double Rules::damage(const UnitStats& a, bool armored, bool light, double armor) {
    const double bonus = a.bonusVsArmored > 0 && armored ? a.bonusVsArmored : a.bonusVsLight > 0 && light ? a.bonusVsLight : 0;
    return static_cast<double>(a.hits) * max(0.5, a.damage + bonus - armor);
}

double Rules::hourAt(double time, double day, std::optional<double> held) {
    if (held && *held >= 0) return std::fmod(*held, 24.0);
    return std::fmod(startHour + time / max(1.0, day) * 24.0, 24.0);
}

double Rules::sunHeight(double hour) {
    // The sun's arc (`Daylight.sun(hour:)`): from sunrise over the noon
    // point (-60, 110, -55, SceneKit axes, Y up) to sunset by day, under
    // the ground by night. Its height is the noon point's, by the cosine.
    const double noonY = 110.0 / std::sqrt(60.0 * 60.0 + 110.0 * 110.0 + 55.0 * 55.0);
    const double day = sunset - sunrise;
    const double a = hour >= sunrise && hour <= sunset ? (hour - sunrise) / day * pi - pi / 2
                                                       : std::fmod(hour - sunset + 24.0, 24.0) / (24.0 - day) * pi + pi / 2;
    return std::asin(max(-1.0, min(1.0, std::cos(a) * noonY))) * 180.0 / pi;
}

double Rules::dark(double hour) {
    const double t = max(0.0, min(1.0, (sunHeight(hour) + 6.0) / 16.0));
    return 1.0 - t * t * (3.0 - 2.0 * t);
}

double Rules::daylightReach(double hour, double night, double span) {
    if (!(night > 0) || span <= night) return night;
    return night * std::pow(span / night, 1.0 - dark(hour));
}

double Rules::vision(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: case StructureKind::sentinel: return 11;
    case StructureKind::bastion: return 10;
    // A practice target sees nothing.
    case StructureKind::dummy: return 0;
    default: return 9;
    }
}

} // namespace ac
