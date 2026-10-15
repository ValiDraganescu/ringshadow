// Realm projects (R2): see RealmProjects.h.
#include "RealmProjects.h"

#include <cmath>

namespace ac {

RealmProject::Price RealmProject::base(Kind k) {
    switch (k) {
    case Kind::beacon: return {2000, 100};
    case Kind::relay: return {2500, 150};
    case Kind::dockCrew: return {3000, 200};
    case Kind::armoury: return {4000, 300};
    }
    return {};
}

RealmProject::Price RealmProject::price(Kind k, int64_t runs) {
    const Price b = base(k);
    const double f = std::pow(growth, double(runs > 0 ? runs : 0));
    return {int64_t(std::llround(double(b.ore) * f / 50.0)) * 50, int64_t(std::llround(double(b.hydrogen) * f / 10.0)) * 10};
}

const char* RealmProject::title(Kind k) {
    switch (k) {
    case Kind::beacon: return "Beacon";
    case Kind::relay: return "Relay";
    case Kind::armoury: return "Armoury";
    case Kind::dockCrew: return "Dock Crew";
    }
    return "";
}

const char* RealmProject::slug(Kind k) {
    switch (k) {
    case Kind::beacon: return "beacon";
    case Kind::relay: return "relay";
    case Kind::armoury: return "armoury";
    case Kind::dockCrew: return "dock-crew";
    }
    return "";
}

const char* RealmProject::effect(Kind k) {
    switch (k) {
    case Kind::beacon: return "Dropship fares halved realm-wide";
    case Kind::relay: return "Every pilot's night sight half again as wide";
    case Kind::armoury: return "Every turret fights one mark up";
    case Kind::dockCrew: return "Mercenaries mine twice as fast";
    }
    return "";
}

} // namespace ac
