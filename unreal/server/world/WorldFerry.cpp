// The Dropship's dev lines (F2, docs/mmorpg-m8.md; WorldMap.h): Landing Pads
// put up finished, the regions seen, the pilot taken to a region, MH in the
// wallet. For the pictures (-AcMmoShotsF2) and the tests; the flight itself
// is `WorldMap::fly` and `landings` (WorldMap.cpp).

#include "WorldMap.h"

#include "Mule.h"
#include "Shuttle.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ac::server {

std::optional<int64_t> WorldMap::devLandingPad(size_t r, Vec2 near, std::optional<double> heading) {
    if (!world_ || r >= regions_.size()) return std::nullopt;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const double radius = Rules::radius(StructureKind::landingPad);
    // On open ground at `near` (ahead along `heading`, or round it), clear of
    // the buildings and the region's pad plates.
    for (double d = 0; d <= 14; d += 1)
        for (int k = 0; k < (d == 0 ? 1 : 12); k++) {
            const double a = heading ? *heading + (k % 2 ? 1 : -1) * 0.35 * double((k + 1) / 2) : k * 2 * pi / 12;
            const Vec2 spot = sim.snap(StructureKind::landingPad, near + Vec2(std::cos(a), std::sin(a)) * d).value_or(near);
            if (sim.placementRefusal(StructureKind::landingPad, spot)) continue;
            if (region::touchesPadPlate(world_->regions[r].built.pads, spot, radius)) continue;
            st.structures.push_back(Structure(st.nextID++, StructureKind::landingPad, Pilot::player, spot));
            st.structures.back().hp = Rules::hp(StructureKind::landingPad);
            sim.reindex();
            sim.refreshNav();
            return st.structures.back().id;
        }
    return std::nullopt;
}

std::string WorldMap::devFerry(const std::string& line) {
    std::istringstream in(line);
    std::string word, what, arg, more;
    in >> word >> what >> arg >> more;
    if (!world_ || pilots_.empty()) return "dev: ferry: no world, or no pilot";
    auto& [pilot, p] = *devPilot();
    if (what == "seen") {
        for (const WorldRegion& g : world_->regions) p.progress.regions.insert(g.file.id);
        pushProgress(pilot);
        moves_.push_back(Moved{pilot, std::nullopt});
        return "dev: ferry: every region seen";
    }
    if (what == "mh") {
        bankOut(p, p.region);
        p.wallet.hydrogen = arg.empty() ? 200 : std::stoll(arg);
        bankIn(p, p.region);
        pushProgress(pilot);
        return "dev: ferry: the wallet holds " + std::to_string(p.wallet.hydrogen) + " MH";
    }
    // C4: `ferry call`: the Dropship called at the pad under the pilot;
    // `ferry board`: one on that pad at once (called, down, its ramp shut),
    // the pilot's Prospector at its take-off button; `ferry ramp open|shut`:
    // its ramp from the button (the Prospector put there first).
    // C4: `ferry pads`: the rig for a flight by hand (the mmoferry flow): a
    // Landing Pad under the pilot, one in a neighbouring region (seen), MH
    // for the fare. The reply names the target region and pad.
    if (what == "pads") {
        const std::string here = devFerry("ferry pad here");
        if (here.rfind("dev: ferry: ", 0) != 0) return here;
        std::optional<size_t> next;
        for (const region::Edge e : {region::Edge::east, region::Edge::south, region::Edge::west, region::Edge::north})
            if (!next) next = neighbour(p.region, e);
        if (!next) return "dev: ferry pads: no region next door";
        const std::string& id = world_->regions[*next].file.id;
        const std::string there = devFerry("ferry pad " + id);
        if (there.rfind("dev: ferry: ", 0) != 0) return there;
        devFerry("ferry mh 200");
        // "dev: ferry: Landing Pad 1 in cold-fork"
        const std::string pad = there.substr(std::string("dev: ferry: ").size(), there.rfind(" in ") - std::string("dev: ferry: ").size());
        return "dev: ferry: pads ready, fly to " + id + " " + pad;
    }
    // `ferry mule`: the pilot's Mule (one made as `mule here` makes it, when
    // it has none) on the grounded Dropship's axis, its middle 3.5 cells
    // behind the ramp's foot, facing the ship; the Prospector on foot beside
    // its perch, not on it (the game asks the take itself, as E at the
    // perch does); the ramp as it is (the mmoferrymule flow drives it up).
    if (what == "mule") {
        if (!p.mule) {
            const std::string made = devMule("mule here");
            if (made.rfind("dev: mule here: mule ", 0) != 0) return "dev: ferry mule: " + made;
        }
        if (drivesMule(pilot, p))
            if (const auto why = handBack(pilot)) return "dev: ferry mule: " + *why;
        const auto own = mule(pilot);
        if (!own || own->first != p.region) return "dev: ferry mule: its Mule is not in this region";
        Simulation& sim = regions_[p.region]->simulation();
        const auto mi = sim.unitIndex(own->second);
        const auto ui = p.unit ? sim.unitIndex(*p.unit) : std::nullopt;
        if (!mi || !ui) return "dev: ferry mule: no Mule, or no Prospector here";
        const Shuttle* ship = nullptr;
        for (const Shuttle& s : sim.state.shuttles)
            if (s.grounded() && (!ship || distance(s.position, sim.state.units[size_t(*mi)].position) <
                                              distance(ship->position, sim.state.units[size_t(*mi)].position)))
                ship = &s;
        if (!ship) return "dev: ferry mule: no Dropship on a pad here";
        Unit& m = sim.state.units[size_t(*mi)];
        m.position = ShuttleBay::world(*ship, Vec2(ShuttleBay::rampFoot() - 2.0, 0));
        m.heading = ship->heading;
        // Beside the perch on the Mule's left, clear of its box, within reach.
        Unit& u = sim.state.units[size_t(*ui)];
        u.position = Mule::point(m, Vec2(Mule::perchLocal.x, Mule::width / 2 + Rules::radius(u.kind) + 0.1));
        u.heading = m.heading - pi / 2;
        u.goal = std::nullopt;
        u.waypoints = std::nullopt;
        return "dev: ferry: the Mule behind the ramp, its Prospector beside the perch, mule " + std::to_string(own->second);
    }
    if (what == "call") {
        const auto why = callShuttle(pilot);
        return why ? "dev: ferry call: " + *why : "dev: ferry: the Dropship is called";
    }
    if (what == "board" || what == "ramp") {
        Simulation& sim = regions_[p.region]->simulation();
        const auto unit = regions_[p.region]->driving(pilot);
        const auto i = unit ? sim.unitIndex(*unit) : std::nullopt;
        if (!i) return "dev: ferry " + what + ": the pilot drives nothing";
        Shuttle* ship = nullptr;
        for (Shuttle& s : sim.state.shuttles)
            if (!ship || distance(s.position, sim.state.units[size_t(*i)].position) < distance(ship->position, sim.state.units[size_t(*i)].position))
                ship = &s;
        if (what == "board" && !ship) {
            if (const auto why = callShuttle(pilot)) return "dev: ferry board: " + *why;
            ship = &sim.state.shuttles.back();
        }
        if (!ship) return "dev: ferry ramp: no Dropship here";
        Unit& u = sim.state.units[size_t(*i)];
        if (what == "board") {
            ship->stage = Shuttle::Stage::ground;
            ship->clock = 0;
            ship->ramp = Door{0, false};
            u.position = ShuttleBay::world(*ship, ShuttleBay::helmStand);
            u.heading = ship->heading;
            return "dev: ferry: aboard the Dropship at its take-off button";
        }
        u.position = ShuttleBay::world(*ship, ShuttleBay::rampStand);
        const auto why = sim.ramp(u.id, ship->id, arg != "shut");
        return why ? "dev: ferry ramp: " + *why : "dev: ferry: the ramp " + std::string(arg != "shut" ? "lowers" : "shuts");
    }
    if (what == "go") {
        const auto r = regionNamed(arg);
        if (!r) return "dev: ferry go: no region " + arg;
        if (!devTransfer(pilot, *r, world_->regions[*r].built.pads[0].at + Vec2(0, 6)))
            return "dev: ferry go: not taken to " + arg;
        return "dev: ferry: taken to " + arg;
    }
    if (what == "pad") {
        // `ferry pad`: 6 cells ahead of the Prospector; `ferry pad here`:
        // under it (or the nearest room round it), the Prospector moved onto
        // it; `ferry pad REGION`: by that region's file pad (and the region
        // seen).
        std::optional<int64_t> id;
        size_t r = p.region;
        if (arg.empty() || arg == "here") {
            const auto u = p.unit ? regions_[r]->simulation().state.unit(*p.unit) : std::nullopt;
            if (!u) return "dev: ferry pad: the pilot drives nothing";
            id = arg == "here" ? devLandingPad(r, u->position)
                               : devLandingPad(r, u->position + Vec2(std::cos(u->heading), std::sin(u->heading)) * 6, u->heading);
        } else {
            const auto there = regionNamed(arg);
            if (!there) return "dev: ferry pad: no region " + arg;
            r = *there;
            // `ferry pad REGION unseen`: the region left unseen (the flight
            // menu's picture: a Landing Pad it can't fly to).
            if (more != "unseen") p.progress.regions.insert(arg);
            const Vec2 pad = world_->regions[r].built.pads[0].at;
            id = devLandingPad(r, pad + Vec2(0, region::padReach + 5), std::nullopt);
        }
        if (!id) return "dev: ferry pad: no room";
        builtBy_[r][*id] = p.key;
        if (arg == "here") {
            GameState& st = regions_[r]->simulation().state;
            const auto b = std::find_if(st.structures.begin(), st.structures.end(), [&](const Structure& s) { return s.id == *id; });
            for (Unit& u : st.units)
                if (p.unit && u.id == *p.unit && b != st.structures.end()) u.position = b->position;
        }
        landingPadsChanged();
        pushProgress(pilot);
        for (const LandingPad& o : landingPads())
            if (o.building == *id && o.region == r) return "dev: ferry: " + o.name + " in " + world_->regions[r].file.id;
        return "dev: ferry: a Landing Pad in " + world_->regions[r].file.id;
    }
    return "dev: ferry: pad [here|REGION [unseen]] | seen | go REGION | mh N | pads | call | board | ramp open|shut | mule";
}

} // namespace ac::server
