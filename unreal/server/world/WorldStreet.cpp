// The buildings' sizes in pictures (C5, docs/building-sizes.md): the dev
// line `street` sets finished buildings of the realm out for the client's
// mmosize flow (AcMmoSizeShots.cpp), one kind beside the pilot's Prospector
// or the whole row beside the realm's Citadel.

#include "WorldMap.h"

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

    // The ones an earlier line set out go first.
    for (const int64_t id : devStreet_) {
        builtBy_[r].erase(id);
        st.structures.erase(std::remove_if(st.structures.begin(), st.structures.end(), [&](const Structure& s) { return s.id == id; }),
                            st.structures.end());
    }
    devStreet_.clear();
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
        // The row: every kind beside the realm's Citadel nearest the pilot,
        // along the first of its sides with room, 2 cells apart, the
        // Prospector before the Citadel's door looking down the row.
        const Vec2 at = st.units[size_t(*me)].position;
        std::optional<Structure> c;
        for (const Structure& b : st.structures)
            if (b.owner == Pilot::player && b.complete() && b.kind == StructureKind::citadel && (!c || distance(b.position, at) < distance(c->position, at)))
                c = b;
        if (!c) return "dev: street all: no Citadel of the realm here";
        const double cr = Rules::radius(StructureKind::citadel);
        // Each kind on the side of the Citadel with the shorter row so far
        // (both sides, so the street stays in one picture).
        int placed = 0;
        std::string missed;
        double cursor[2] = {cr + 2, cr + 2};
        for (const auto& [name, kind] : streetKinds()) {
            if (kind == StructureKind::derrick) continue;
            const double kr = Rules::radius(kind);
            bool done = false;
            for (int pass = 0; pass < 2 && !done; pass++) {
                const int side = (cursor[0] <= cursor[1]) == (pass == 0) ? 0 : 1;
                const Vec2 along = side == 0 ? Vec2(1, 0) : Vec2(-1, 0);
                for (double k = cursor[side] + kr; k <= cursor[side] + kr + 30 && !done; k += 1)
                    // The row's buildings line up on their fronts with the Citadel's.
                    for (const double off : {0.0, -1.0, 1.0, -2.0, 2.0, -4.0, 4.0}) {
                        const auto spot = sim.snap(kind, c->position + along * k + Vec2(0, cr - kr + off));
                        if (!spot || !fits(kind, *spot)) continue;
                        put(kind, *spot);
                        cursor[side] = std::abs(spot->x - c->position.x) + kr + 2;
                        placed += 1;
                        done = true;
                        break;
                    }
            }
            if (!done) missed += " " + name;
        }
        if (const auto w = freeWell(c->position); w && distance(*w, c->position) < 60) {
            put(StructureKind::derrick, *w);
            placed += 1;
        }
        const Vec2 stand = c->position + Vec2(2.5, cr + 3);
        if (walkable(stand)) standAt(stand, pi / 2);
        std::snprintf(out, sizeof out, "dev: street all: %d placed beside citadel %lld%s%s", placed, (long long)c->id,
                      missed.empty() ? "" : "; no room for", missed.c_str());
        return out;
    }

    // One kind: ahead of the pilot on clear ground (a Derrick on the
    // nearest free well), the Prospector at its front corner, facing out.
    std::optional<StructureKind> kind;
    for (const auto& [name, k] : streetKinds())
        if (name == what) kind = k;
    if (!kind) return "dev: street: '" + what + "' is not all|habdome|garrison|bastion|foundry|spacedock|lab|sentinel|gatling|pad|derrick";
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
