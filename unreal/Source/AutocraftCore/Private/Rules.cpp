// Port of `Rules` of Sources/GameCore/State.swift and Vision.swift.
#include "Rules.h"

#include <algorithm>
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
    }
    return 0;
}

int64_t Rules::supply(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 15;
    case StructureKind::habDome: return 8;
    default: return 0;
    }
}

/// Half the footprint side: 5x5 Citadel; 3x3 Garrison, Bastion,
/// Derrick, Foundry, Spacedock and Landing Pad; 2x2 Hab Dome, Lab and Sentinel.
double Rules::radius(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 2.5;
    case StructureKind::habDome: case StructureKind::lab: case StructureKind::sentinel: case StructureKind::gatling: return 1;
    default: return 1.5;
    }
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
    default: return 9;
    }
}

} // namespace ac
