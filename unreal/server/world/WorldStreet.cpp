// The buildings' sizes in pictures (C5, docs/building-sizes.md): the dev
// line `street` sets finished buildings of the realm out for the client's
// mmosize flow (AcMmoSizeShots.cpp), one kind beside the pilot's Prospector
// or the whole row beside the realm's Citadel.

#include "WorldMap.h"

#include "Mule.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ac::server {

namespace {

/// The kinds a pilot builds, as the dev line names them.
const std::vector<std::pair<std::string, StructureKind>>& streetKinds() {
    static const std::vector<std::pair<std::string, StructureKind>> kinds = {
        {"habdome", StructureKind::habDome},   {"garrison", StructureKind::garrison}, {"bastion", StructureKind::bastion},
        {"foundry", StructureKind::foundry},   {"spacedock", StructureKind::spacedock}, {"lab", StructureKind::lab},
        {"sentinel", StructureKind::sentinel}, {"gatling", StructureKind::gatling},   {"pad", StructureKind::landingPad},
        {"derrick", StructureKind::derrick},
    };
    return kinds;
}

} // namespace

std::string WorldMap::devStreet(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: street: no world, or no pilot";
    auto& [pilot, p] = *devPilot();
    if (p.flight) return "dev: street: in flight";
    const size_t r = p.region;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const auto me = p.unit ? sim.unitIndex(*p.unit) : std::nullopt;
    if (!me) return "dev: street: the pilot has no Prospector here";

    // The ones an earlier line set out go first (not for `street door`).
    for (const int64_t id : what == "door" ? std::vector<int64_t>{} : devStreet_) {
        builtBy_[r].erase(id);
        st.structures.erase(std::remove_if(st.structures.begin(), st.structures.end(), [&](const Structure& s) { return s.id == id; }),
                            st.structures.end());
    }
    if (what != "door") devStreet_.clear();
    sim.reindex();
    sim.refreshNav();

    // A finished `kind` of the realm at `spot`, built by the pilot.
    auto put = [&](StructureKind kind, Vec2 spot) -> const Structure& {
        Structure s(st.nextID++, kind, Pilot::player, spot);
        s.hp = Rules::hp(kind);
        st.structures.push_back(s);
        builtBy_[r][s.id] = p.key;
        devStreet_.push_back(s.id);
        sim.reindex();
        sim.refreshNav();
        return st.structures.back();
    };
    auto fits = [&](StructureKind kind, Vec2 spot) { return !sim.placementRefusal(kind, spot); };
    // The nearest free well to `from` with MH left (a Derrick stands on one).
    auto freeWell = [&](Vec2 from) -> std::optional<Vec2> {
        std::optional<Vec2> best;
        if (!st.wells) return best;
        for (const Well& w : *st.wells)
            if (w.remaining > 0 && fits(StructureKind::derrick, w.position) && (!best || distance(w.position, from) < distance(*best, from)))
                best = w.position;
        return best;
    };
    auto standAt = [&](Vec2 at, double heading) {
        Unit& u = st.units[size_t(*me)];
        u.position = at;
        u.heading = heading;
        u.task = Unit::Task::idle;
        if (Pilot* driver = sim.pilotFor(u.id)) driver->heading = u.heading;
        sim.reindex();
    };
    auto walkable = [&](Vec2 q) { return sim.nav && sim.nav->walkable(q) && !sim.nav->inRock(q); };
    char out[256];

    if (what == "all") {
        // The street: a Citadel of the realm on open ground near the pilot
        // and every kind 2 cells apart down both sides of a road 8 cells
        // wide out of its door (+y), each on the side with the shorter row
        // so far; the Prospector before the door looking down the street.
        const Vec2 at = st.units[size_t(*me)].position;
        const double cr = Rules::radius(StructureKind::citadel);
        constexpr double road = 4;
        // Where each kind goes down the street from a Citadel at `c` (none
        // placed): `tight` keeps them 2 to 4 cells apart, else up to 30.
        auto layout = [&](Vec2 c, bool tight) {
            std::vector<std::pair<StructureKind, Vec2>> spots;
            double cursor[2] = {cr + 3, cr + 3};
            for (const auto& [name, kind] : streetKinds()) {
                if (kind == StructureKind::derrick) continue;
                const double kr = Rules::radius(kind);
                bool done = false;
                for (int pass = 0; pass < 2 && !done; pass++) {
                    const int side = (cursor[0] <= cursor[1]) == (pass == 0) ? 0 : 1;
                    const double sign = side == 0 ? 1 : -1;
                    for (double k = cursor[side] + kr; k <= cursor[side] + kr + (tight ? 2 : 30) && !done; k += 1)
                        for (const double off : {0.0, 1.0, 2.0, 4.0}) {
                            const auto spot = sim.snap(kind, c + Vec2(sign * (road + kr + off), k));
                            if (!spot || !fits(kind, *spot)) continue;
                            spots.emplace_back(kind, *spot);
                            cursor[side] = spot->y - c.y + kr + 2;
                            done = true;
                            break;
                        }
                }
            }
            return spots;
        };
        // Open ground near the pilot that takes the Citadel and the whole
        // street, the street shortest (both sides of the road filled).
        const size_t want = streetKinds().size() - 1;
        std::optional<Vec2> best;
        size_t bestCount = 0;
        double bestFar = 1e9;
        int tried = 0;
        const GroundRect bounds = sim.map ? sim.map->bounds : GroundRect{};
        std::vector<Vec2> candidates;
        for (double x = bounds.minX + cr + 2; x <= bounds.maxX - cr - 2; x += 4)
            for (double y = bounds.minZ + cr + 2; y <= bounds.maxZ - cr - 2; y += 4)
                if (distance(Vec2(x, y), at) < 120) candidates.push_back(Vec2(x, y));
        std::sort(candidates.begin(), candidates.end(), [&](Vec2 a, Vec2 b) { return distance(a, at) < distance(b, at); });
        for (const Vec2 q : candidates) {
            const auto c = sim.snap(StructureKind::citadel, q);
            if (!c || !fits(StructureKind::citadel, *c) || !walkable(*c + Vec2(2.5, cr + 3))) continue;
            const auto spots = layout(*c, true);
            double far = 0;
            for (const auto& [kind, spot] : spots) far = std::max(far, spot.y - c->y + Rules::radius(kind));
            if (spots.size() > bestCount || (spots.size() == bestCount && far < bestFar)) {
                bestCount = spots.size();
                bestFar = far;
                best = c;
            }
            // The nearest 1500 spots that take it all (a second or two).
            if (bestCount >= want && ++tried >= 1500) break;
        }
        if (!best) return "dev: street all: no open ground for a Citadel near the pilot";
        const Structure& citadel = put(StructureKind::citadel, *best);
        const int64_t cid = citadel.id;
        const Vec2 c = *best;
        armCitadels();
        int placed = 0;
        std::string where;
        for (const auto& [kind, spot] : layout(c, bestCount >= want)) {
            put(kind, spot);
            placed += 1;
            for (const auto& [name, k] : streetKinds())
                if (k == kind) {
                    char spotAt[64];
                    std::snprintf(spotAt, sizeof spotAt, " %s (%.1f, %.1f)", name.c_str(), spot.x - c.x, spot.y - c.y);
                    where += spotAt;
                }
        }
        if (const auto w = freeWell(c); w && distance(*w, c) < 60) {
            put(StructureKind::derrick, *w);
            placed += 1;
        }
        standAt(c + Vec2(2.5, cr + 3), pi / 2);
        const Vec2 g = regions_[r]->places()->regionAt(c);
        std::snprintf(out, sizeof out, "dev: street all: %d placed beside citadel %lld at (%.1f, %.1f); from its middle:", placed,
                      (long long)cid, g.x, g.y);
        return out + where;
    }

    if (what == "door") {
        // The pilot's Mule at the door of the Foundry an earlier line set
        // out, facing out of it, as one rolls out (its door opens).
        const Structure* f = nullptr;
        for (const Structure& b : st.structures)
            if (b.kind == StructureKind::foundry && std::find(devStreet_.begin(), devStreet_.end(), b.id) != devStreet_.end()) f = &b;
        if (!f) return "dev: street door: no Foundry set out (street foundry first)";
        const Vec2 at = Mule::door(f->position);
        std::optional<int64_t> id = p.mule && p.mule->region == r ? p.mule->unit : std::nullopt;
        if (!id || !sim.unitIndex(*id)) {
            id = sim.rollOutAt(at, Pilot::player);
            PilotState::OwnMule own;
            own.region = r;
            own.unit = *id;
            p.mule = own;
        }
        if (const auto i = sim.unitIndex(*id)) {
            st.units[size_t(*i)].position = at;
            st.units[size_t(*i)].heading = Mule::doorHeading;
        }
        sim.reindex();
        return "dev: street door: mule " + std::to_string(*id);
    }

    // One kind: ahead of the pilot on clear ground (a Derrick on the
    // nearest free well), the Prospector at its front corner, facing out.
    std::optional<StructureKind> kind;
    for (const auto& [name, k] : streetKinds())
        if (name == what) kind = k;
    if (!kind) return "dev: street: '" + what + "' is not all|door|habdome|garrison|bastion|foundry|spacedock|lab|sentinel|gatling|pad|derrick";
    const Vec2 at = st.units[size_t(*me)].position;
    const double heading = st.units[size_t(*me)].heading;
    const double kr = Rules::radius(*kind);
    std::optional<Vec2> spot;
    if (*kind == StructureKind::derrick) {
        spot = freeWell(at);
    } else {
        for (double d = kr + 4; d <= kr + 60 && !spot; d += 1)
            for (const double turn : {0.0, 0.4, -0.4, 0.8, -0.8, 1.2, -1.2, 1.8, -1.8, 2.6, -2.6}) {
                const auto q = sim.snap(*kind, at + Vec2(std::cos(heading + turn), std::sin(heading + turn)) * d);
                if (!q || !fits(*kind, *q)) continue;
                // Room for the Prospector at its front corner, and open ground
                // out to where the pictures' camera stands before that corner
                // (mmosize: up to 24 cells off along (0.6, 0.8)).
                if (!walkable(*q + Vec2(kr + 1.0, kr - 0.5)) || !walkable(*q + Vec2(0, kr + 4))) continue;
                bool open = true;
                for (double k = kr + 2; k <= 24 && open; k += 1.5) open = walkable(*q + Vec2(0.6, 0.8) * k);
                if (!open) continue;
                spot = q;
                break;
            }
    }
    if (!spot) return "dev: street " + what + ": no clear ground";
    const Structure& s = put(*kind, *spot);
    const int64_t id = s.id;
    Vec2 stand = *spot + Vec2(kr + 1.0, kr - 0.5);
    if (!walkable(stand)) stand = *spot + Vec2(-(kr + 1.0), kr - 0.5);
    standAt(stand, pi / 3);
    const Vec2 g = regions_[r]->places()->regionAt(*spot);
    std::snprintf(out, sizeof out, "dev: street %s: %lld at (%.1f, %.1f) in %s", what.c_str(), (long long)id, g.x, g.y,
                  world_->regions[r].file.id.c_str());
    return out;
}

} // namespace ac::server
