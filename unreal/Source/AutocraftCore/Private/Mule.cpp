#include "Mule.h"

#include <algorithm>
#include <cmath>

namespace ac {

Vec2 Mule::point(const Unit& m, Vec2 local) {
    const double c = std::cos(m.heading), s = std::sin(m.heading);
    return m.position + Vec2(local.x * c - local.y * s, local.x * s + local.y * c);
}

int64_t Mule::room(const Unit& m) {
    const int64_t held = m.haul ? m.haul->total() : 0;
    return std::max<int64_t>(0, capacity - held);
}

Vec2 Mule::door(Vec2 foundry) { return foundry + Vec2(0, Rules::radius(StructureKind::foundry) + 0.2 + length / 2); }

std::string Mule::bookedToast(int64_t ore, int64_t hydrogen) {
    std::string out = "Booked";
    std::string sep = " · ";
    if (ore > 0) {
        out += sep + "+" + std::to_string(ore) + " ore";
        sep = ", ";
    }
    if (hydrogen > 0) out += sep + "+" + std::to_string(hydrogen) + " MH";
    return out;
}

} // namespace ac
