// The realm's defences (Defence.h).

#include "Defence.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ac {

Vec2 Defence::cornerGun(Vec2 citadel, int i) {
    const double sx = (i == 0 || i == 3) ? 1 : -1, sy = i < 2 ? 1 : -1;
    return {citadel.x + sx * cornerInset(), citadel.y + sy * cornerInset()};
}

double Defence::cornerAngle(int i) {
    const double sx = (i == 0 || i == 3) ? 1 : -1, sy = i < 2 ? 1 : -1;
    return std::atan2(sy, sx);
}

Vec2 Defence::gunStation(Vec2 citadel, int i) {
    const double sx = (i == 0 || i == 3) ? 1 : -1, sy = i < 2 ? 1 : -1;
    return {citadel.x + sx * stationInset(), citadel.y + sy * stationInset()};
}

Vec3 Defence::gunEye(Vec2 citadel, int i, double aim, double pitch) {
    // In the gun's own plane (along the aim, up), the eye turned by the pitch
    // about the trunnion.
    const double c = std::cos(pitch), s = std::sin(pitch);
    const double along = trunnionAlong + eyeAlong * c - eyeUp * s;
    const double up = trunnionUp + eyeAlong * s + eyeUp * c;
    const Vec2 m = cornerGun(citadel, i) + Vec2(std::cos(aim), std::sin(aim)) * along;
    return {m.x, m.y, mountHeight() + up};
}

double Defence::clampAim(int i, double aim) {
    const double rest = cornerAngle(i);
    const double off = std::remainder(aim - rest, 2 * pi);
    return std::remainder(rest + std::clamp(off, -gunArcHalf, gunArcHalf), 2 * pi);
}

UnitStats Defence::apply(UnitStats w, const TurretMarks& m) {
    w.cooldown /= 1 + ratePerMark * double(m.rate);
    w.damage *= 1 + damagePerMark * double(m.damage);
    w.range += rangePerMark * double(m.range);
    if (w.airRange > 0) w.airRange += airPerMark * double(m.air);
    return w;
}

std::optional<UnitStats> Defence::weapon(const Structure& s, int64_t bonus) {
    std::optional<UnitStats> w = Rules::turret(s.kind);
    if (!w && s.kind == StructureKind::citadel && s.marks) w = UnitStats::citadelGuns;
    if (!w) return std::nullopt;
    if (bonus <= 0) return s.marks ? apply(*w, *s.marks) : *w;
    TurretMarks m = s.marks.value_or(TurretMarks{});
    for (int64_t* k : {&m.rate, &m.damage, &m.range, &m.air}) *k = std::min<int64_t>(*k + bonus, marksMax);
    return apply(*w, m);
}

ShopPrice Defence::citadelPrice(DefenceStat, int64_t mark) {
    switch (mark) {
    case 1: return {250, 0};
    case 2: return {500, 50};
    case 3: return {1000, 150};
    default: return {};
    }
}

std::optional<std::string> Defence::citadelRefusal(DefenceStat stat, const TurretMarks& marks, int64_t ore,
                                                   int64_t hydrogen) {
    if (!takes(StructureKind::citadel, stat)) return std::string("The Citadel's turrets reach the air as far as the ground");
    const int64_t next = marks.of(stat) + 1;
    if (next > marksMax) return std::string(title(stat)) + " is at its last mark";
    const ShopPrice p = citadelPrice(stat, next);
    if (ore < p.ore) return "The realm wallet is short: " + std::to_string(p.ore) + " ore needed, it has " + std::to_string(ore);
    if (hydrogen < p.hydrogen)
        return "The realm wallet is short: " + std::to_string(p.hydrogen) + " MH needed, it has " + std::to_string(hydrogen);
    return std::nullopt;
}

bool Defence::upgradable(StructureKind kind) {
    return kind == StructureKind::sentinel || kind == StructureKind::gatling;
}

bool Defence::takes(StructureKind kind, DefenceStat stat) {
    if (stat == DefenceStat::air) return kind == StructureKind::gatling;
    return upgradable(kind) || kind == StructureKind::citadel;
}

ShopPrice Defence::turretPrice(StructureKind kind, DefenceStat stat, int64_t mark) {
    if (!upgradable(kind) || !takes(kind, stat) || mark < 1 || mark > marksMax) return {};
    static constexpr ShopPrice table[] = {{100, 50}, {300, 200}, {800, 500}};
    const ShopPrice p = table[mark - 1];
    // The AA tower is the dearer building: half as much again.
    if (kind == StructureKind::sentinel) return {p.ore * 3 / 2, p.hydrogen * 3 / 2};
    return p;
}

std::optional<std::string> Defence::turretRefusal(StructureKind kind, DefenceStat stat, const TurretMarks& marks,
                                                  int64_t ore, int64_t hydrogen) {
    if (!upgradable(kind)) return std::string("Only a Sentinel or a Gatling Gun takes marks");
    if (!takes(kind, stat)) return std::string("The Sentinel reaches the air as far as its range");
    const int64_t next = marks.of(stat) + 1;
    if (next > marksMax) return std::string(title(stat)) + " is at its last mark";
    const ShopPrice p = turretPrice(kind, stat, next);
    if (ore < p.ore) return "Your wallet is short: " + std::to_string(p.ore) + " ore needed, you have " + std::to_string(ore);
    if (hydrogen < p.hydrogen)
        return "Your wallet is short: " + std::to_string(p.hydrogen) + " MH needed, you have " + std::to_string(hydrogen);
    return std::nullopt;
}

const char* Defence::title(DefenceStat stat) {
    switch (stat) {
    case DefenceStat::rate: return "Fire rate";
    case DefenceStat::damage: return "Damage";
    case DefenceStat::range: return "Range";
    case DefenceStat::air: return "Air reach";
    }
    return "";
}

} // namespace ac
