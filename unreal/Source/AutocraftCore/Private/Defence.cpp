// The realm's defences (Defence.h).

#include "Defence.h"

#include <cmath>
#include <string>

namespace ac {

Vec2 Defence::cornerGun(Vec2 citadel, int i) {
    const double sx = (i == 0 || i == 3) ? 1 : -1, sy = i < 2 ? 1 : -1;
    return {citadel.x + sx * cornerInset, citadel.y + sy * cornerInset};
}

double Defence::cornerAngle(int i) {
    const double sx = (i == 0 || i == 3) ? 1 : -1, sy = i < 2 ? 1 : -1;
    return std::atan2(sy, sx);
}

UnitStats Defence::apply(UnitStats w, const TurretMarks& m) {
    w.cooldown /= 1 + ratePerMark * double(m.rate);
    w.damage *= 1 + damagePerMark * double(m.damage);
    w.range += rangePerMark * double(m.range);
    if (w.airRange > 0) w.airRange += rangePerMark * double(m.range);
    return w;
}

std::optional<UnitStats> Defence::weapon(const Structure& s) {
    std::optional<UnitStats> w = Rules::turret(s.kind);
    if (!w && s.kind == StructureKind::citadel && s.marks) w = UnitStats::citadelGuns;
    if (!w) return std::nullopt;
    return s.marks ? apply(*w, *s.marks) : *w;
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
    const int64_t next = marks.of(stat) + 1;
    if (next > marksMax) return std::string(title(stat)) + " is at its last mark";
    const ShopPrice p = citadelPrice(stat, next);
    if (ore < p.ore) return "The realm wallet is short: " + std::to_string(p.ore) + " ore needed, it has " + std::to_string(ore);
    if (hydrogen < p.hydrogen)
        return "The realm wallet is short: " + std::to_string(p.hydrogen) + " MH needed, it has " + std::to_string(hydrogen);
    return std::nullopt;
}

const char* Defence::title(DefenceStat stat) {
    switch (stat) {
    case DefenceStat::rate: return "Fire rate";
    case DefenceStat::damage: return "Damage";
    case DefenceStat::range: return "Range";
    }
    return "";
}

} // namespace ac
