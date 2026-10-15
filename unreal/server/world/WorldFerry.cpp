// The Dropship's dev lines (F2, docs/mmorpg-m8.md; WorldMap.h): Landing Pads
// put up finished, the regions seen, the pilot taken to a region, MH in the
// wallet. For the pictures (-AcMmoShotsF2) and the tests; the flight itself
// is `WorldMap::fly` and `landings` (WorldMap.cpp).

#include "WorldMap.h"

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
    std::string word, what, arg;
    in >> word >> what >> arg;
    if (!world_ || pilots_.empty()) return "dev: ferry: no world, or no pilot";
    auto& [pilot, p] = *pilots_.begin();
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
            p.progress.regions.insert(arg);
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
        for (const LandingPad& o : landingPads())
            if (o.building == *id && o.region == r) return "dev: ferry: " + o.name + " in " + world_->regions[r].file.id;
        return "dev: ferry: a Landing Pad in " + world_->regions[r].file.id;
    }
    return "dev: ferry: pad [here|REGION] | seen | go REGION | mh N";
}

} // namespace ac::server
