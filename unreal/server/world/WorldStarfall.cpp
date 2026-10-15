// Starfall on the server (docs/weather.md "Starfall"; docs/mmorpg-m7.md E2,
// m10.md E1): the schedule's falls told a minute before their impacts, each
// placed then on open ground of a region weighted toward the pilots, never
// where its blast would reach the home's ground; at the impact the blast
// (what it falls on destroyed, the damage falling off to its edge) and a
// deposit of 100 to 1,000 ore in the crater, first come first served, that
// never grows back; gone from the sky's list once its crater has faded and
// its deposit is empty.

#include "WorldMap.h"

#include "NavGrid.h"
#include "Rules.h"
#include "Starfall.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ac::server {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr region::Edge kFallEdges[] = {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west};

/// The crater keeps this far from the map's edge, cells.
constexpr double kEdgeMargin = 12;
/// And from what stands on the ground: beyond its own radius, cells.
constexpr double kPadGap = region::padReach + 3;
constexpr double kSiteGap = 4;
constexpr double kOreGap = 2.5;
constexpr double kWellGap = 3.5;
/// Another fall's crater in the same region.
constexpr double kFallGap = 2 * Starfall::craterRadius + 6;
/// A forced fall near a pilot: this far from its Prospector, cells.
constexpr double kNearFrom = 40, kNearTo = 58;
/// Tries at a spot before giving up on a region.
constexpr int kTries = 300;

double uniform(std::mt19937_64& dice, double lo, double hi) {
    return lo + (hi - lo) * double(dice() >> 11) * (1.0 / 9007199254740992.0);
}

} // namespace

uint64_t WorldMap::starfallSeed() const {
    if (params_.starfallSeed) return params_.starfallSeed;
    return world_ ? uint64_t(WorldClock::weatherSeed(world_->file.id)) ^ 0x57a2fa115eedULL : 0;
}

bool WorldMap::craterFits(size_t r, Vec2 at) const {
    if (r >= regions_.size()) return false;
    const Region& reg = *regions_[r];
    const Simulation& sim = reg.simulation();
    const NavGrid* nav = sim.navGrid();
    if (!nav || !sim.map || !reg.places()) return false;
    const GroundRect& b = sim.map->bounds;
    if (at.x < b.minX + kEdgeMargin || at.x > b.maxX - kEdgeMargin || at.y < b.minZ + kEdgeMargin || at.y > b.maxZ - kEdgeMargin)
        return false;
    // Open ground under the whole crater: no cliff, no rock.
    const double R = Starfall::craterRadius;
    for (int k = 0; k < 17; k++) {
        const double a = double(k) * kPi / 8;
        const Vec2 q = k == 16 ? at : at + Vec2(std::cos(a), std::sin(a)) * (k % 2 ? R : R * 0.5);
        if (!nav->standable(q) || nav->inRock(q)) return false;
    }
    // Joined to the region's pads: a pilot walks to it.
    const auto cell = nav->index(at);
    if (!cell || !nav->free(*cell)) return false;
    bool joined = false;
    for (const region::Place& p : reg.places()->pads)
        if (const auto c = nav->index(p.at); c && nav->regions[size_t(*c)] == nav->regions[size_t(*cell)]) joined = true;
    if (!joined) return false;
    // Clear of what stands there.
    const auto near = [&](Vec2 p, double gap) { return length(p - at) < R + gap; };
    for (const region::Place& p : reg.places()->pads)
        if (near(p.at, kPadGap)) return false;
    for (const region::Site& s : reg.places()->sites)
        if (near(s.at, kSiteGap)) return false;
    const GameState& st = sim.state;
    for (const OreDeposit& d : st.patches)
        if (near(d.position, kOreGap)) return false;
    if (st.wells)
        for (const Well& w : *st.wells)
            if (near(w.position, kWellGap)) return false;
    for (const Fall& f : falls_)
        if (f.region == r && length(f.at - at) < kFallGap) return false;
    return !reachesHome(r, at);
}

bool WorldMap::reachesHome(size_t r, Vec2 at) const {
    if (r != start_ || r >= regions_.size()) return false;
    const Region& reg = *regions_[r];
    const double reach = Starfall::blastRadius(reg.places() ? reg.places()->extent() : Vec2(0, 0)) + Starfall::homeSpared;
    bool citadel = false;
    for (const Structure& s : reg.simulation().state.structures) {
        if (s.kind != StructureKind::citadel) continue;
        citadel = true;
        if (length(s.position - at) < Rules::radius(s.kind) + reach) return true;
    }
    // No Citadel yet: where a new pilot lands and the home will rise.
    if (!citadel && reg.places())
        for (const region::Place& p : reg.places()->pads)
            if (length(p.at - at) < reach) return true;
    return false;
}

void WorldMap::blast(const Fall& fall) {
    Simulation& sim = regions_[fall.region]->simulation();
    GameState& st = sim.state;
    const auto& places = regions_[fall.region]->places();
    const double R = Starfall::blastRadius(places ? places->extent() : Vec2(0, 0));
    if (!(R > 0)) return;
    for (Structure& s : st.structures) {
        if (s.hp <= 0) continue;
        const double share = Starfall::blastShare(std::max(0.0, length(s.position - fall.at) - Rules::radius(s.kind)), R);
        if (share >= 1) s.hp = 0;
        else if (share > 0) s.hp -= share * Rules::hp(s.kind);
    }
    for (Unit& u : st.units) {
        if (u.hp <= 0) continue;
        const double share = Starfall::blastShare(std::max(0.0, length(u.position - fall.at) - Rules::radius(u.kind)), R);
        if (share >= 1) u.hp = 0;
        else if (share > 0) u.hp -= share * sim.maxHP(u);
    }
}

std::optional<Vec2> WorldMap::craterSpot(size_t r, std::mt19937_64& dice, std::optional<Vec2> near, double heading) const {
    const Simulation& sim = regions_[r]->simulation();
    if (!sim.map) return std::nullopt;
    if (near) {
        // Ahead of the Prospector first, then anywhere round it; in a
        // straight walk from it.
        const NavGrid* nav = sim.navGrid();
        for (int k = 0; k < kTries; k++) {
            const double spread = k < kTries / 2 ? 50 * kPi / 180 : kPi;
            const double a = heading + uniform(dice, -spread, spread);
            const Vec2 p = *near + Vec2(std::cos(a), std::sin(a)) * uniform(dice, kNearFrom, kNearTo);
            if (craterFits(r, p) && (!nav || nav->clear(*near, p))) return p;
        }
        return std::nullopt;
    }
    const GroundRect b = sim.map->bounds.inset(kEdgeMargin);
    for (int k = 0; k < kTries; k++) {
        const Vec2 p(uniform(dice, b.minX, b.maxX), uniform(dice, b.minZ, b.maxZ));
        if (craterFits(r, p)) return p;
    }
    return std::nullopt;
}

size_t WorldMap::fallRegion(std::mt19937_64& dice) const {
    std::vector<double> weight(regions_.size(), 1.0);
    for (const auto& [id, p] : pilots_) {
        if (p.flight) continue;
        weight[p.region] += 8;
        for (const region::Edge e : kFallEdges)
            if (const auto n = neighbour(p.region, e)) weight[*n] += 3;
    }
    double total = 0;
    for (const double w : weight) total += w;
    double x = uniform(dice, 0, total);
    for (size_t i = 0; i < weight.size(); i++) {
        if (x < weight[i]) return i;
        x -= weight[i];
    }
    return weight.size() - 1;
}

std::optional<WorldMap::Fall> WorldMap::forceStarfall(double in, std::optional<size_t> region, std::optional<PilotId> near) {
    if (!world_) return std::nullopt;
    std::mt19937_64 dice(Starfall::mix(starfallSeed() ^ uint64_t(clock_ * 1000) ^ (uint64_t(nextFall_) << 40)));
    size_t r = 0;
    std::optional<Vec2> at;
    double from = Starfall::bearingFrom(dice());
    if (near) {
        const auto it = pilots_.find(*near);
        if (it == pilots_.end() || it->second.flight) return std::nullopt;
        r = it->second.region;
        const auto unit = regions_[r]->driving(*near);
        const auto u = unit ? regions_[r]->simulation().state.unit(*unit) : std::nullopt;
        if (!u) return std::nullopt;
        at = craterSpot(r, dice, u->position, u->heading);
        // Seen coming from one side, so the streak crosses the pilot's sky.
        if (at) from = Starfall::bearing(*at, u->position) + 180 + (dice() % 2 ? 1 : -1) * uniform(dice, 70, 90);
    } else {
        r = region ? *region : fallRegion(dice);
        if (r >= regions_.size()) return std::nullopt;
        at = craterSpot(r, dice);
        // No open ground there (and none asked for): the next regions round.
        for (size_t k = 1; !at && !region && k < regions_.size(); k++) {
            r = (r + 1) % regions_.size();
            at = craterSpot(r, dice);
        }
    }
    if (!at) return std::nullopt;
    const int64_t ore = Starfall::oreFrom(dice());
    Fall f{nextFall_++, r, *at, clock_ + std::max(0.0, in), std::fmod(from + 360, 360.0), std::nullopt, ore, ore};
    falls_.push_back(f);
    sky_.starfalls = skyFalls();
    return f;
}

void WorldMap::landFall(Fall& fall, int64_t remaining, std::optional<int64_t> id) {
    Simulation& sim = regions_[fall.region]->simulation();
    GameState& st = sim.state;
    int64_t next = 1;
    for (const OreDeposit& d : st.patches) next = std::max(next, d.id + 1);
    OreDeposit d;
    d.id = id.value_or(next);
    d.position = fall.at;
    d.angle = std::fmod(double(Starfall::mix(uint64_t(d.id) ^ uint64_t(fall.id)) >> 11) * (1.0 / 9007199254740992.0), 1.0) * 2 * kPi;
    d.initial = fall.ore;
    d.remaining = std::max<int64_t>(0, remaining);
    // Empty: it never grows back (`Simulation::regrow`).
    if (d.remaining <= 0) d.depletedAt = std::numeric_limits<double>::infinity();
    // The blast throws whatever stands in the deposit's way clear of it.
    for (Unit& u : st.units) {
        if (!NavGrid::insidePatch(u.position, d, Rules::radius(u.kind) + 0.3)) continue;
        Vec2 out = u.position - d.position;
        out = length(out) > 1e-6 ? out / length(out) : Vec2(1, 0);
        u.position = d.position + out * (Starfall::craterRadius - 0.5);
    }
    fall.patch = int64_t(st.patches.size());
    fall.left = d.remaining;
    st.patches.push_back(d);
    sim.refreshNav();
}

void WorldMap::starfall() {
    if (!world_) return;
    const double now = clock_;
    // The schedule: each fall told `warning` before its impact, placed then.
    // A jump of the clock (a save loaded, a dev move) tells none of those
    // it skipped.
    if (params_.starfalls && fallsRead_ && now >= *fallsRead_ && now - *fallsRead_ < 1.0) {
        const uint64_t seed = starfallSeed();
        for (double t = Starfall::next(seed, *fallsRead_ + Starfall::warning, params_.dayLength); t <= now + Starfall::warning;
             t = Starfall::next(seed, t, params_.dayLength)) {
            std::mt19937_64 dice(Starfall::mix(seed ^ uint64_t(t * 1000)));
            size_t r = fallRegion(dice);
            std::optional<Vec2> at = craterSpot(r, dice);
            // No open ground there: the next regions round.
            for (size_t k = 1; !at && k < regions_.size(); k++) {
                r = (r + 1) % regions_.size();
                at = craterSpot(r, dice);
            }
            if (!at) continue;
            const int64_t ore = Starfall::oreFrom(dice());
            falls_.push_back(Fall{nextFall_++, r, *at, t, Starfall::bearingFrom(dice()), std::nullopt, ore, ore});
        }
    }
    fallsRead_ = now;
    // Dev: one near the first pilot on line that drives.
    if (params_.devStarfall && !devFallDone_)
        for (const auto& [id, p] : pilots_)
            if (!p.flight && regions_[p.region]->driving(id)) {
                devFallDone_ = true;
                forceStarfall(*params_.devStarfall, std::nullopt, id);
                break;
            }
    for (Fall& f : falls_) {
        if (!f.patch) {
            if (now >= f.impact) {
                blast(f);
                landFall(f, f.ore);
            }
            continue;
        }
        GameState& st = regions_[f.region]->simulation().state;
        if (size_t(*f.patch) >= st.patches.size()) continue;
        OreDeposit& d = st.patches[size_t(*f.patch)];
        // Drilled since the last step: the pilot whose Prospector drills
        // there gets the share of XP over the plain (the plain comes when
        // the load is unloaded).
        if (d.remaining < f.left) {
            const double drilled = double(f.left - d.remaining);
            PilotState* best = nullptr;
            double bestDistance = Rules::mineRange + 3;
            for (auto& [id, p] : pilots_) {
                if (p.flight || p.region != f.region) continue;
                const auto unit = regions_[p.region]->driving(id);
                const auto u = unit ? st.unit(*unit) : std::nullopt;
                if (!u) continue;
                const double dist = length(u->position - d.position);
                if (dist < bestDistance) {
                    bestDistance = dist;
                    best = &p;
                }
            }
            if (best) {
                const int64_t level = best->progress.level();
                earn(*best, (Starfall::xp - 1) * drilled * WorldXp::ore);
                for (auto& [id, p] : pilots_)
                    if (&p == best && p.progress.level() != level) pushProgress(id);
            }
        }
        f.left = d.remaining;
        if (d.remaining <= 0) d.depletedAt = std::numeric_limits<double>::infinity();
    }
    // Gone once the crater has faded and the deposit is empty.
    std::erase_if(falls_, [&](const Fall& f) {
        if (now < f.impact + Starfall::craterLife || !f.patch) return false;
        const GameState& st = regions_[f.region]->simulation().state;
        return size_t(*f.patch) >= st.patches.size() || st.patches[size_t(*f.patch)].remaining <= 0;
    });
    sky_.starfalls = skyFalls();
}

std::vector<net::Sky::Starfall> WorldMap::skyFalls() const {
    std::vector<net::Sky::Starfall> out;
    for (const Fall& f : falls_) out.push_back(net::Sky::Starfall{f.id, int64_t(f.region), f.at, f.impact, f.from, f.patch});
    return out;
}

} // namespace ac::server
