// `balancebench [MINUTES]`: what a pilot keeps an hour (B2, docs/balance.md),
// against the pace model (docs/prospector-progression.md: a level-1 driven
// Prospector that only mines earns 100 XP a minute, 6,000 an hour; skills,
// a Derrick and routes add 2% a level). The first world with the dev rig (a
// Citadel at the start), one pilot at level 1, 5, 10 and 20, its Prospector
// driven by a plain hand: to the deposit nearest the Citadel it can walk to
// in a straight line, the drill held to a full load, back to the Citadel's
// edge to unload, again. Two builds a level: no points spent, and every
// point in the drill and the road (Quick drill, Big haul, Light frame, Deep
// bore, Ore sled, Dense pack, in that order, as the tree lets them). MINUTES
// of game time a run (default 10), counted an hour: ore unloaded (the whole
// load), what the pilot keeps (less the realm's 10%), the tax, the XP.
// Isolated: no Brood, contracts, missions, starfalls, frontier pilots or
// mercenaries. Built and run by `make bench` (-O3).

#include "WorldMap.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

constexpr PilotId me = 1;
const std::string key = "b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2";

struct Out {
    double ore = 0, kept = 0, tax = 0, xp = 0, trips = 0;
    double walk = 0;
};

Unit* driven(WorldMap& w) {
    Region* r = w.regionOf(me);
    const auto id = r ? r->driving(me) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

/// A straight walk from `a` to `b` on open ground.
bool clear(const Simulation& sim, Vec2 a, Vec2 b) {
    const double d = distance(a, b);
    for (double t = 0; t <= d; t += 0.25)
        if (!sim.nav->walkable(a + (b - a) * (t / d))) return false;
    return true;
}

Out run(const LoadedWorld& world, int64_t level, bool economy, double minutes) {
    WorldParams p;
    p.firstStep = 0;
    p.devXp = Progression::xp(level);
    p.brood = false;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.canyons = false;
    p.frontier = 0;
    WorldMap w(world, p);
    if (w.seat(me, key) != WorldMap::Seated::fresh) {
        std::printf("  not seated\n");
        return {};
    }
    for (int i = 0; i < 60; i++) w.step();
    if (economy) {
        const Skill order[] = {Skill::quickDrill, Skill::bigHaul, Skill::lightFrame, Skill::deepBore, Skill::oreSled, Skill::densePack};
        for (bool spent = true; spent;) {
            spent = false;
            for (const Skill s : order)
                if (!w.spend(me, s)) {
                    spent = true;
                    break;
                }
        }
    }
    if (const auto pr = w.progress(me); std::getenv("B2DEBUG") && pr) {
        int64_t ranks = 0;
        for (const auto& [sk, n] : pr->ranks) ranks += n;
        std::printf("  level %lld, xp %.0f, ranks %lld\n", (long long)Progression::level(pr->xp), pr->xp, (long long)ranks);
    }
    Simulation& sim = w.regionOf(me)->simulation();
    Unit* u = driven(w);
    if (!u) {
        std::printf("  no driven unit\n");
        return {};
    }
    // A finished Citadel east of the start's ore field (where the first ten
    // minutes' test builds it), as a pilot's own first one.
    const Vec2 want = w.regionOf(me)->places()->ground(Vec2(72, 48));
    std::optional<Vec2> spot;
    for (double rad = 0; rad <= 6 && !spot; rad += 0.5)
        for (int k = 0; k < 16 && !spot; k++) {
            const double a = k * 2 * pi / 16;
            const auto s = sim.snap(StructureKind::citadel, want + Vec2(std::cos(a), std::sin(a)) * rad);
            if (s && !sim.placementRefusal(StructureKind::citadel, *s)) spot = *s;
        }
    if (!spot) {
        std::printf("  no room for the Citadel\n");
        return {};
    }
    Structure c(sim.state.nextID++, StructureKind::citadel, Pilot::player, *spot);
    c.hp = Rules::hp(StructureKind::citadel);
    sim.state.structures.push_back(c);
    sim.reindex();
    sim.refreshNav();
    const Vec2 base = *spot;
    const double edgeAt = Rules::radius(StructureKind::citadel) + 0.5;
    // Each trip: the deposit nearest the Citadel with ore and a straight walk.
    auto pick = [&]() -> std::optional<size_t> {
        std::optional<size_t> best;
        for (size_t k = 0; k < sim.state.patches.size(); k++) {
            const OreDeposit& d = sim.state.patches[k];
            if (d.remaining <= 0) continue;
            const Vec2 dir = normalize(d.position - base);
            if (!clear(sim, base + dir * (edgeAt + 0.1), d.position - dir * 1.4)) continue;
            if (!best || distance(d.position, base) < distance(sim.state.patches[*best].position, base)) best = k;
        }
        return best;
    };
    std::optional<size_t> dep = pick();
    if (!dep) {
        std::printf("  no deposit with a straight walk\n");
        return {};
    }
    u->position = base + normalize(sim.state.patches[*dep].position - base) * edgeAt;
    u->carrying = 0;
    u->task = Unit::Task::idle;
    const auto wallet0 = *w.wallet(me);
    const auto realm0 = w.realmWallet();
    const double xp0 = w.progress(me) ? w.progress(me)->xp : 0;
    Out out;
    double walks = 0, legs = 0;
    const int ticks = int(minutes * 60 * 60);
    int64_t t = 0;
    for (int i = 0; i < ticks; i++) {
        u = driven(w);
        if (!u) break;
        PilotInput in;
        in.tick = ++t;
        in.visit = w.visit(me);
        const bool full = u->carrying > 0 && u->task != Unit::Task::mining;
        if (!full && (!dep || sim.state.patches[*dep].remaining <= 0)) {
            dep = pick();
            if (!dep) break;
        }
        const Vec2 ore = sim.state.patches[*dep].position;
        const Vec2 to = full ? base : ore;
        in.heading = std::atan2(to.y - u->position.y, to.x - u->position.x);
        const auto target = sim.pilotTarget(*u);
        const bool atOre = !full && target && target->as<PilotTarget::Patch>();
        in.walk = atOre || u->task == Unit::Task::mining ? Vec2(0, 0) : Vec2(1, 0);
        in.act = atOre && u->task != Unit::Task::mining;
        in.hold = !full;
        const int64_t carried = u->carrying;
        w.input(me, in);
        w.step();
        if (Unit* v = driven(w); v && carried > 0 && v->carrying == 0) {
            out.trips += 1;
            walks += distance(sim.state.patches[*dep].position, base) - edgeAt;
            legs += 1;
        }
    }
    const double hours = minutes / 60;
    const auto wallet1 = *w.wallet(me);
    const auto realm1 = w.realmWallet();
    out.kept = double(wallet1.ore - wallet0.ore) / hours;
    out.tax = double(realm1.ore - realm0.ore) / hours;
    out.ore = out.kept + out.tax;
    out.xp = ((w.progress(me) ? w.progress(me)->xp : 0) - xp0) / hours;
    out.trips /= hours;
    out.walk = legs > 0 ? walks / legs : 0;
    return out;
}


/// The Fork's ten waves, from WAITING to HELD, at pilot level `level`:
/// `gatlings` Gatling Guns and `towers` AA towers of the pilot's on the
/// zone's build spots (`canyon turrets`), the pilot standing south of the
/// canyon. Each wave's time out and its pay to the pilot (XP, ore, MH).
void canyon(const LoadedWorld& world, int64_t level, int64_t gatlings, int64_t towers) {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.brood = false;
    p.frontier = 0;
    p.firstStep = 0;
    WorldMap w(world, p);
    if (w.seat(me, key) != WorldMap::Seated::fresh) return;
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    for (const StructureKind k : Progression::blueprintKinds()) g.giveBlueprint(k, 9);
    w.setProgress(me, g);
    w.step();
    const size_t r = *w.regionNamed("cold-fork");
    if (!w.devTransfer(me, r, w.region(r).places()->ground(Vec2(100, 52)))) return;
    w.step();
    const std::string zone = w.region(r).places()->zones.front().id;
    std::printf("  %s\n", w.devCanyon("canyon turrets " + std::to_string(gatlings) + " " + std::to_string(towers) + " 0").c_str());
    const double start = w.clock();
    double out = -1, sumXp = 0, sumOre = 0, sumMh = 0, fight = 0;
    int64_t wave = 0;
    auto purse = [&] {
        const auto wl = *w.wallet(me);
        const auto pr = w.progress(me);
        return std::array<double, 3>{pr ? pr->xp : 0, double(wl.ore), double(wl.hydrogen)};
    };
    auto before = purse();
    std::printf("  wave  bugs  out s   xp     ore    MH     xp-eq/min out   (pace %.0f/min)\n", WorldContracts::perMinute(level));
    for (int i = 0; i < 90 * 60 * 60; i++) {
        w.step();
        const WorldMap::Canyon& c = *w.canyon(r, zone);
        if (c.out && out < 0) {
            out = w.clock();
            wave = c.wave;
            before = purse();
        }
        if (!c.out && out >= 0) {
            // Paid on the step it ends.
            const auto after = purse();
            const double t = w.clock() - out;
            const double xp = after[0] - before[0], ore = after[1] - before[1], mh = after[2] - before[2];
            const bool beaten = c.wave > wave || c.state == WorldMap::CanyonState::held;
            std::printf("  %4lld  %4zu  %5.0f  %5.0f  %5.0f  %5.0f  %8.0f  %s\n", (long long)wave, c.bugs.size(), t, xp, ore, mh,
                        (xp + ore + 2 * mh) / (t / 60), beaten ? "" : "(broke through or replayed)");
            sumXp += xp;
            sumOre += ore;
            sumMh += mh;
            fight += t;
            out = -1;
        }
        if (c.state == WorldMap::CanyonState::held || c.state == WorldMap::CanyonState::failed) {
            const double all = w.clock() - start;
            std::printf("  %s after %.1f min (%.1f min of waves out): %.0f XP, %.0f ore, %.0f MH = %.0f XP-eq, %.0f a minute of the run; "
                        "the pace %.0f a minute\n",
                        c.state == WorldMap::CanyonState::held ? "HELD" : "FAILED", all / 60, fight / 60, sumXp, sumOre, sumMh,
                        sumXp + sumOre + 2 * sumMh, (sumXp + sumOre + 2 * sumMh) / (all / 60), WorldContracts::perMinute(level));
            return;
        }
    }
    std::printf("  not held in 90 min\n");
}

} // namespace

int main(int argc, char** argv) {
    const double minutes = argc > 1 ? std::atof(argv[1]) : 10;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    std::printf("level  build     target/h  ore/h   kept/h  tax/h  xp/h    trips/h  walk\n");
    for (const int64_t level : {1, 5, 10, 20})
        for (const bool economy : {false, true}) {
            const Out o = run(*load.world, level, economy, minutes);
            const double target = 6000 * (1 + 0.02 * double(level - 1));
            std::printf("%5lld  %-8s  %7.0f  %6.0f  %6.0f  %5.0f  %6.0f  %6.0f  %4.1f\n", (long long)level, economy ? "economy" : "none",
                        target, o.ore, o.kept, o.tax, o.xp, o.trips, o.walk);
        }
    for (const auto [g, t] : {std::pair{8, 3}, std::pair{10, 4}}) {
        std::printf("\nThe Fork, pilot level 15, %d Gatling Guns and %d AA towers:\n", g, t);
        canyon(*load.world, 15, g, t);
    }
    return 0;
}
