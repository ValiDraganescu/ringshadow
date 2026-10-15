// `balancebench [MINUTES]`: what a pilot keeps an hour (B2, docs/balance.md),
// against the pace model (docs/prospector-progression.md: a level-1 driven
// Prospector that only mines earns 100 XP a minute, 6,000 an hour; skills,
// a Derrick and routes add 2% a level). The first world with the dev rig (a
// Citadel at the start), one pilot at level 1, 5, 10 and 20, its Prospector
// driven by a plain hand: to the deposit nearest the Citadel with ore, along
// the walking grid's path (E1: a vein's mounds stand close), the drill held
// to a full load, back to the Citadel's edge to unload, again. Two builds a level: no points spent, and every
// point in the drill and the road (Quick drill, Big haul, Light frame, Deep
// bore, Ore sled, Dense pack, in that order, as the tree lets them). MINUTES
// of game time a run (default 10), counted an hour: ore unloaded (the whole
// load), what the pilot keeps (less the realm's 10%), the tax, the XP.
// Isolated: no Brood, contracts, missions, starfalls, frontier pilots or
// mercenaries. Built and run by `make bench` (-O3). E1: the start region's
// veins run dry in a long run; part 4 mines 90 minutes in 10-minute slices
// (the pilot waits at the Citadel while nothing has grown back).

#include "WorldMap.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

constexpr PilotId me = 1;
/// A deposit worth the walk: a load or more (E1).
constexpr int64_t kLoadWorth = 20;
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

enum class Build { none, average, economy };
const char* named(Build b) { return b == Build::none ? "none" : b == Build::average ? "average" : "economy"; }

Out run(const LoadedWorld& world, int64_t level, Build build, double minutes, std::vector<double>* slices = nullptr) {
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
    if (build != Build::none) {
        // Every point (economy), or half of them (average: the pace model's
        // "average build", the rest elsewhere), into the drill and the road.
        const int64_t points = level - 1;
        int64_t spend = build == Build::economy ? points : points / 2;
        const Skill order[] = {Skill::quickDrill, Skill::bigHaul, Skill::lightFrame, Skill::deepBore, Skill::oreSled, Skill::densePack};
        for (bool spent = true; spent && spend > 0;) {
            spent = false;
            for (const Skill sk : order)
                if (!w.spend(me, sk)) {
                    spent = true;
                    spend--;
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
    // Each trip: the deposit nearest the Citadel with a load's worth of ore
    // (a pilot does not walk for the little that has grown back) and a way
    // to it (one the hand got stuck at left alone for a minute).
    std::map<size_t, int> stuckAt;
    int tick = 0;
    auto pick = [&]() -> std::optional<size_t> {
        std::optional<size_t> best;
        for (size_t k = 0; k < sim.state.patches.size(); k++) {
            const OreDeposit& d = sim.state.patches[k];
            if (d.remaining < kLoadWorth) continue;
            if (const auto st = stuckAt.find(k); st != stuckAt.end() && tick - st->second < 60 * 60) continue;
            if (!sim.nav->path(base + normalize(d.position - base) * (edgeAt + 0.1), d.position, Rules::mineRange)) continue;
            if (!best || distance(d.position, base) < distance(sim.state.patches[*best].position, base)) best = k;
        }
        return best;
    };
    std::optional<size_t> dep = pick();
    if (!dep) {
        std::printf("  no deposit with a way to it\n");
        return {};
    }
    // The way there along the walking grid, asked again every half second.
    std::vector<Vec2> way;
    int wayAt = -1000;
    Vec2 wayTo(1e9, 1e9);
    auto steer = [&](const Unit& v, Vec2 to, double stopAt, int now) {
        if (now - wayAt >= 30 || distance(to, wayTo) > 0.01) {
            way = sim.nav->path(v.position, to, stopAt).value_or(std::vector<Vec2>{to});
            wayAt = now;
            wayTo = to;
        }
        while (way.size() > 1 && distance(way.front(), v.position) < 0.6) way.erase(way.begin());
        return way.empty() ? to : way.front();
    };
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
    Vec2 lastAt(1e9, 1e9);
    int stillSince = 0;
    for (int i = 0; i < ticks; i++) {
        tick = i;
        u = driven(w);
        if (!u) break;
        // No headway and no drilling for 3 s: another deposit.
        if (distance(u->position, lastAt) > 0.3 || u->task == Unit::Task::mining) {
            lastAt = u->position;
            stillSince = i;
        } else if (i - stillSince > 3 * 60) {
            // Loaded: ask the way home again; else leave that deposit be.
            if (u->carrying > 0 && u->task != Unit::Task::mining) wayAt = -1000;
            else if (dep) {
                stuckAt[*dep] = i;
                dep = std::nullopt;
            }
            stillSince = i;
        }
        PilotInput in;
        in.tick = ++t;
        in.visit = w.visit(me);
        const bool full = u->carrying > 0 && u->task != Unit::Task::mining;
        // E1: every 10 game minutes, the ore unloaded so far.
        if (slices && i > 0 && i % (10 * 60 * 60) == 0)
            slices->push_back(double(w.wallet(me)->ore - wallet0.ore + w.realmWallet().ore - realm0.ore));
        if (!full && (!dep || sim.state.patches[*dep].remaining <= 0 || (u->task != Unit::Task::mining && sim.state.patches[*dep].remaining < kLoadWorth))) {
            dep = pick();
            // Mined dry: the pilot waits for the veins to grow back.
            if (!dep) {
                w.input(me, PilotInput{});
                w.step();
                continue;
            }
        }
        const Vec2 ore = sim.state.patches[*dep].position;
        const auto target = sim.pilotTarget(*u);
        const bool atOre = !full && target && target->as<PilotTarget::Patch>();
        // Facing the ore once there (the drill takes what it faces).
        const Vec2 to = full ? steer(*u, base, edgeAt, i) : atOre ? ore : steer(*u, ore, Rules::mineRange, i);
        in.heading = std::atan2(to.y - u->position.y, to.x - u->position.x);
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
    if (slices && std::getenv("B2DEBUG"))
        for (const OreDeposit& d : sim.state.patches)
            std::printf("  mound (%.1f, %.1f): %lld of %lld left, %.1f from the Citadel, way %d, from the unit %d\n", d.position.x, d.position.y,
                        (long long)d.remaining, (long long)d.initial, distance(d.position, base),
                        int(sim.nav->path(base + normalize(d.position - base) * (edgeAt + 0.1), d.position, Rules::mineRange).has_value()),
                        int(u && sim.nav->path(u->position, d.position, Rules::mineRange).has_value()));
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
struct Cycle {
    bool held = false;
    double xp = 0, ore = 0, mh = 0, waves = 0, all = 0;
};

Cycle canyon(const LoadedWorld& world, int64_t level, int64_t gatlings, int64_t towers) {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.brood = false;
    p.frontier = 0;
    p.firstStep = 0;
    WorldMap w(world, p);
    if (w.seat(me, key) != WorldMap::Seated::fresh) return {};
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    for (const StructureKind k : Progression::blueprintKinds()) g.giveBlueprint(k, 9);
    w.setProgress(me, g);
    w.step();
    const size_t r = *w.regionNamed("cold-fork");
    if (!w.devTransfer(me, r, w.region(r).places()->ground(Vec2(100, 52)))) return {};
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
            return Cycle{c.state == WorldMap::CanyonState::held, sumXp, sumOre, sumMh, fight / 60, all / 60};
        }
    }
    std::printf("  not held in 90 min\n");
    return {};
}

} // namespace

int main(int argc, char** argv) {
    const double minutes = argc > 1 ? std::atof(argv[1]) : 10;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    // 1. Mining: what a pilot unloads and keeps an hour.
    std::printf("Mining, %.0f game minutes a run, counted an hour (target: 6,000 at level 1, +2%% a level, all work):\n", minutes);
    std::printf("level  build     target/h  ore/h   kept/h  tax/h  xp/h    trips/h  walk\n");
    const int64_t levels[] = {1, 5, 10, 15, 20};
    std::map<int64_t, double> mined;
    for (const int64_t level : levels)
        for (const Build b : {Build::none, Build::average, Build::economy}) {
            const Out o = run(*load.world, level, b, minutes);
            if (b == Build::average) mined[level] = o.xp;
            const double target = 6000 * (1 + 0.02 * double(level - 1));
            std::printf("%5lld  %-8s  %7.0f  %6.0f  %6.0f  %5.0f  %6.0f  %6.0f  %4.1f\n", (long long)level, named(b), target, o.ore, o.kept,
                        o.tax, o.xp, o.trips, o.walk);
        }

    // 4. (E1) Scarcity: the start region's veins mined by a pilot alone for
    // 90 minutes, ore unloaded a 10-minute slice: they run dry, then only
    // their regrowth (a tenth of their ore a world day) comes in.
    int64_t startOre = 0;
    for (const WorldRegion& g : load.world->regions)
        if (g.file.id == "start")
            for (const region::Deposit& d : g.file.deposits) startOre += d.amount;
    std::printf("\nScarcity (E1): the start region's %lld ore, 90 game minutes, ore unloaded each 10 minutes:\n", (long long)startOre);
    for (const auto [level, b] : {std::pair{int64_t(1), Build::none}, std::pair{int64_t(20), Build::economy}}) {
        std::vector<double> slices;
        run(*load.world, level, b, 90, &slices);
        std::printf("level %2lld %-8s", (long long)level, named(b));
        double last = 0;
        for (const double s : slices) {
            std::printf(" %5.0f", s - last);
            last = s;
        }
        std::printf("  (%.0f in all)\n", last);
    }

    // 2. A held canyon cycle at level 15, the pilot's guns on The Fork.
    // The guns alone don't hold waves 9 and 10 (the pilot fights too): the
    // best cycle reached counts, its XP over its whole time (a low mark:
    // wave 10's double pay never comes in).
    Cycle held;
    for (const auto [g, t] : {std::pair{8, 3}, std::pair{10, 4}, std::pair{12, 5}}) {
        std::printf("\nThe Fork, pilot level 15, %d Gatling Guns and %d AA towers:\n", g, t);
        const Cycle c = canyon(*load.world, 15, g, t);
        if (c.xp > held.xp) held = c;
        if (c.held) break;
    }

    // 3. The all-in pilot: an hour of play is three quarters work (the pace
    // model), 45 minutes. The board's six offers a world day (40 minutes:
    // 9 an hour), each taking about 1, 2.5 and 5 minutes (easy, hard,
    // perilous: the walk to its place and its ask); from level 10 one
    // canyon cycle in 70 minutes (its waves and breaks; then the 40-minute
    // hold, when the pilot mines); the rest of the work mining at the
    // average build. XP only (the levels).
    const double missionMin[3] = {1, 2.5, 5};
    auto mining = [&](int64_t level) {
        // Between the measured levels, in a straight line.
        auto hi = mined.lower_bound(level);
        if (hi == mined.end()) return mined.rbegin()->second;
        if (hi->first == level || hi == mined.begin()) return hi->second;
        auto lo = std::prev(hi);
        const double t = double(level - lo->first) / double(hi->first - lo->first);
        return lo->second + (hi->second - lo->second) * t;
    };
    struct Hour { double xp, mine, board, canyon, boardMin, canyonMin; };
    auto allIn = [&](int64_t level) {
        Hour h{};
        h.boardMin = 0;
        double boardXp = 0;
        for (int t = 0; t < 3; t++) {
            boardXp += 2 * WorldMissions::xp(net::CitadelMission::Tier(t), level);
            h.boardMin += 2 * missionMin[t];
        }
        // An hour: 1.5 world days of the board.
        boardXp *= 60.0 / 40;
        h.boardMin *= 60.0 / 40;
        if (level >= 10 && held.xp > 0) {
            h.canyon = held.xp * 60 / 70;
            h.canyonMin = held.all * 60 / 70;
        }
        const double work = 45;
        const double share = std::min(1.0, (work - h.canyonMin) / h.boardMin);
        h.board = boardXp * share;
        h.boardMin *= share;
        const double mineMin = std::max(0.0, work - h.canyonMin - h.boardMin);
        h.mine = mining(level) * mineMin / 60;
        h.xp = h.mine + h.board + h.canyon;
        return h;
    };
    std::printf("\nThe all-in pilot, an hour (45 minutes of work), XP; the curve is the pace model's:\n");
    std::printf("level  curve/h  mining  board  canyon  all-in  x curve   (board %%, canyon %% of the work)\n");
    for (const int64_t level : {1, 5, 10, 15, 20}) {
        const Hour h = allIn(level);
        const double curve = WorldContracts::perMinute(level) * 60;
        std::printf("%5lld  %7.0f  %6.0f  %5.0f  %6.0f  %6.0f  %6.2f    (%.0f, %.0f)\n", (long long)level, curve, h.mine, h.board, h.canyon, h.xp,
                    h.xp / curve, h.boardMin / 45 * 100, h.canyonMin / 45 * 100);
    }
    // Level 1 to 10 at the all-in rate, a minute at a time.
    double xp = 0, at = 0;
    while (xp < Progression::xp(10) && at < 600) {
        xp += allIn(Progression::level(xp)).xp / 60;
        at += 1;
    }
    double pure = 0, atPure = 0;
    while (pure < Progression::xp(10) && atPure < 600) {
        pure += WorldContracts::perMinute(Progression::level(pure));
        atPure += 1;
    }
    std::printf("Level 1 to 10: %.0f minutes all-in, %.0f on the pace model's curve\n", at, atPure);
    return 0;
}
