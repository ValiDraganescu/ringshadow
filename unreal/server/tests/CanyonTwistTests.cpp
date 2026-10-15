// The canyons after The Fork (Z3, docs/canyon.md): The Drowned Cut in Brine
// Pans (level 2), whose flooded floor slows the Brood's walkers and not its
// flyers, and The Roost in Dust Terraces (level 3), whose Gloamwings all come
// off a roost on a ledge while its walkers come out of the tunnels. Each
// validates (its paths walkable, a roost's flown), has its own wave table,
// a level-3 canyon pays more, and a frontier defender joins their waves.
// Isolated scenes, a wave at a time; run from unreal/server.

#include "test.h"

#include "Brood.h"
#include "Progression.h"
#include "RegionFile.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;
using namespace ac::region;

constexpr PilotId me = 61;
const std::string myKey = "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.brood = false;
    p.frontier = 0;
    p.broodSeed = 0x23ULL;
    return p;
}

std::string read(const std::string& path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

RegionFile parsed(const std::string& path) {
    const Parsed p = parse(read(path));
    for (const Problem& q : p.problems) std::printf("    unexpected: %s\n", q.text().c_str());
    return p.file ? *p.file : RegionFile{};
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(std::lround(seconds * 60));
    for (int i = 0; i < n; i++) w.step();
}

bool inside(Vec2 p, const std::vector<Vec2>& poly) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if ((poly[i].y > p.y) != (poly[j].y > p.y) && p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    return in;
}

/// The pilot in `region`, 8 cells south of its zone's prize (as `brood zone
/// here`).
size_t into(WorldMap& w, const std::string& zone) {
    w.seat(me, myKey);
    w.step();
    std::printf("  %s\n", w.devBrood("brood zone here " + zone).c_str());
    w.step();
    return *w.where(me);
}

} // namespace

/// Both canyons validate in their regions' files: two tunnels each, paths
/// walkable end to end (a roost's flown), spots over them, a prize richer
/// than the region's other ore; the Drowned Cut's flood inside its zone, the
/// Roost's roost on a ledge over the prize.
TEST(CanyonTwist_bothValidate) {
    for (const auto& [path, zone] : {std::pair<std::string, std::string>{"regions/brine-pans.json", "the-drowned-cut"},
                                      {"regions/dust-terraces.json", "the-roost"}}) {
        const RegionFile f = parsed(path);
        ASSERT_TRUE(f.zones.size() == size_t(1));
        const Zone& z = f.zones[0];
        EXPECT_EQ(z.id, zone);
        const Built b = build(f);
        const std::vector<Problem> problems = validate(f, b);
        for (const Problem& p : problems) std::printf("    %s\n", p.text().c_str());
        EXPECT_TRUE(problems.empty());
        EXPECT_TRUE(z.spots.size() >= 6);
        int64_t tunnels = 0;
        for (const Zone::Entrance& e : z.entrances) tunnels += e.roost ? 0 : 1;
        EXPECT_EQ(tunnels, int64_t(2));
        int64_t prizeLeast = INT64_MAX, elsewhereMost = 0;
        for (size_t i = 0; i < f.deposits.size(); i++) {
            const bool prize = std::find(z.prizeDeposits.begin(), z.prizeDeposits.end(), int64_t(i)) != z.prizeDeposits.end();
            if (prize) prizeLeast = std::min(prizeLeast, f.deposits[i].amount);
            else elsewhereMost = std::max(elsewhereMost, f.deposits[i].amount);
        }
        EXPECT_TRUE(prizeLeast > elsewhereMost);
        std::printf("  %s: %zu spots, %zu paths, flood %zu corners\n", zone.c_str(), z.spots.size(), z.paths.size(), z.flood.size());
    }
    const RegionFile brine = parsed("regions/brine-pans.json");
    EXPECT_TRUE(brine.brood == 2 && brine.zones[0].flood.size() >= 3 && brine.zones[0].floodSlow == 0.5);
    const RegionFile dust = parsed("regions/dust-terraces.json");
    EXPECT_EQ(dust.brood, int64_t(3));
    int64_t roosts = 0;
    for (const Zone::Entrance& e : dust.zones[0].entrances) roosts += e.roost ? 1 : 0;
    EXPECT_EQ(roosts, int64_t(1));
}

/// The format's rules for the twists: a flood corner outside the zone is a
/// problem; a roost on the canyon's floor (not a ledge) is one; a roost's
/// path may cross the walls (it is flown); `roost` and `flood` round-trip.
TEST(CanyonTwist_formatRules) {
    RegionFile f = parsed("regions/dust-terraces.json");
    const RegionFile again = *parse(write(f)).file;
    EXPECT_TRUE(again.zones == f.zones);
    // The roost on the basin's floor: not a ledge.
    RegionFile low = f;
    for (Zone::Entrance& e : low.zones[0].entrances)
        if (e.roost) e.at = Vec2(30, 76);
    bool flagged = false;
    for (const Problem& p : validate(low, build(low))) flagged = flagged || p.message.find("a roost stands on a ledge") != std::string::npos;
    EXPECT_TRUE(flagged);
    RegionFile brine = parsed("regions/brine-pans.json");
    EXPECT_TRUE(parse(write(brine)).file->zones == brine.zones);
    brine.zones[0].flood.push_back(Vec2(10, 90));
    flagged = false;
    for (const Problem& p : validate(brine, build(brine))) flagged = flagged || p.where.find(".flood") != std::string::npos;
    EXPECT_TRUE(flagged);
}

/// The brine: a wave's Shardlings wading in the Drowned Cut's flood are
/// slowed to half (their slow renewed every think), out of it they are not;
/// its Gloamwings pass over unslowed. Walkers take longer to the prize
/// through it than the same walk would take dry.
TEST(CanyonTwist_theBrineSlowsWalkersNotFlyers) {
    WorldMap w(first(), params());
    const size_t r = into(w, "the-drowned-cut");
    ASSERT_TRUE(w.loaded()->regions[r].file.id == std::string("brine-pans"));
    const Zone& z = w.region(r).places()->zones.at(0);
    ASSERT_TRUE(!z.flood.empty());
    ASSERT_TRUE(w.canyonSend(r, "the-drowned-cut", 8));  // Gloamwings down both arms, Shardlings down the Gull arm
    int64_t wadingSlowed = 0, wading = 0, dryFast = 0, flyersSlowed = 0, flyersOver = 0;
    for (int s = 0; s < 90 * 4; s++) {
        stepFor(w, 0.25);
        const GameState& st = w.region(r).simulation().state;
        for (const Unit& u : st.units) {
            if (!u.bug || u.hp <= 0) continue;
            const bool slowed = u.slowUntil.value_or(-1) > st.time && u.slowedTo.value_or(1) <= 0.5 + 1e-9;
            const bool wet = inside(u.position, z.flood);
            if (u.kind == UnitKind::gloamwing) {
                flyersOver += wet ? 1 : 0;
                flyersSlowed += slowed ? 1 : 0;
            } else if (wet) {
                wading += 1;
                wadingSlowed += slowed ? 1 : 0;
            } else if (!slowed) {
                dryFast += 1;
            }
        }
    }
    std::printf("  wading %lld (slowed %lld), dry and unslowed %lld, flyers over the brine %lld (slowed %lld)\n", (long long)wading,
                (long long)wadingSlowed, (long long)dryFast, (long long)flyersOver, (long long)flyersSlowed);
    EXPECT_TRUE(wading > 0);
    EXPECT_TRUE(double(wadingSlowed) >= 0.9 * double(wading));
    EXPECT_TRUE(dryFast > 0);
    EXPECT_TRUE(flyersOver > 0);
    EXPECT_EQ(flyersSlowed, int64_t(0));
}

/// The march through the brine: a Shardling pack down the Salt arm reaches
/// the prize later than the arm's dry walk at its march (the floor is
/// flooded from halfway down).
TEST(CanyonTwist_theBrineDelaysTheMarch) {
    WorldMap w(first(), params());
    const size_t r = into(w, "the-drowned-cut");
    const Zone& z = w.region(r).places()->zones.at(0);
    Vec2 prize(0, 0);
    const auto& m = w.region(r).places()->map;
    for (int64_t i : z.prizeDeposits) prize = prize + m.deposits[size_t(i)].position / double(z.prizeDeposits.size());
    double length = 0;
    const auto& line = z.paths[0].points;
    for (size_t k = 0; k + 1 < line.size(); k++) length += distance(line[k], line[k + 1]);
    const auto id = w.sendAlongPath(r, "the-drowned-cut", 0, UnitKind::shardling, 4);
    ASSERT_TRUE(id.has_value());
    const double from = w.clock();
    double arrived = -1;
    for (int s = 0; s < 120 * 4 && arrived < 0; s++) {
        stepFor(w, 0.25);
        for (const Unit& u : w.region(r).simulation().state.units)
            if (u.bug && u.bug->group == *id && u.hp > 0 && distance(u.position, prize) <= region::prizeReach) arrived = w.clock() - from;
    }
    const double dry = length / Brood::marchSpeed(UnitKind::shardling);
    std::printf("  the Salt arm: %.0f cells, dry %.1f s, through the brine %.1f s\n", length, dry, arrived);
    EXPECT_TRUE(arrived > dry);
}

/// The roost: every Gloamwing of a Roost wave comes off the roost's ledge,
/// every Shardling out of a tunnel.
TEST(CanyonTwist_flyersComeOffTheRoost) {
    WorldMap w(first(), params());
    const size_t r = into(w, "the-roost");
    ASSERT_TRUE(w.loaded()->regions[r].file.id == std::string("dust-terraces"));
    const Zone& z = w.region(r).places()->zones.at(0);
    Vec2 roost(0, 0);
    std::vector<Vec2> tunnels;
    for (const Zone::Entrance& e : z.entrances) {
        if (e.roost) roost = e.at;
        else tunnels.push_back(e.at);
    }
    ASSERT_TRUE(w.canyonSend(r, "the-roost", 3));  // Shardlings down both arms, Gloamwings off the roost
    w.step();
    int64_t flyers = 0, offRoost = 0, walkers = 0, outOfTunnels = 0;
    const WorldMap::Canyon& c = *w.canyon(r, "the-roost");
    for (const Unit& u : w.region(r).simulation().state.units) {
        if (!c.bugs.count(u.id)) continue;
        double nearTunnel = 1e9;
        for (const Vec2 t : tunnels) nearTunnel = std::min(nearTunnel, distance(u.position, t));
        if (u.kind == UnitKind::gloamwing) {
            flyers += 1;
            offRoost += distance(u.position, roost) < 4 ? 1 : 0;
        } else {
            walkers += 1;
            outOfTunnels += nearTunnel < 4 ? 1 : 0;
        }
    }
    std::printf("  wave 3: %lld Gloamwings (%lld off the roost), %lld walkers (%lld out of the tunnels)\n", (long long)flyers,
                (long long)offRoost, (long long)walkers, (long long)outOfTunnels);
    EXPECT_TRUE(flyers > 0 && offRoost == flyers);
    EXPECT_TRUE(walkers > 0 && outOfTunnels == walkers);
}

/// Each canyon has its own ten waves; the Roost's carry more Gloamwings than
/// The Fork's, the Drowned Cut's more walkers; mini-boss waves at 5 and 10;
/// a level-3 canyon pays × 1.3.
TEST(CanyonTwist_waveTablesAndPay) {
    auto count = [](const std::string& zone, bool flyers) {
        int64_t n = 0;
        for (const WorldCanyon::Wave& wv : WorldCanyon::waves(zone))
            for (const WorldCanyon::Group& g : wv.groups) n += (g.kind == UnitKind::gloamwing) == flyers ? g.count + (flyers ? 0 : g.escort) : 0;
        return n;
    };
    for (const std::string zone : {"the-fork", "the-drowned-cut", "the-roost"}) {
        EXPECT_EQ(int64_t(WorldCanyon::waves(zone).size()), WorldCanyon::count());
        EXPECT_TRUE(WorldCanyon::waves(zone)[4].boss && WorldCanyon::waves(zone)[9].boss);
        std::printf("  %s: %lld walkers, %lld flyers in its ten waves\n", zone.c_str(), (long long)count(zone, false), (long long)count(zone, true));
    }
    EXPECT_TRUE(count("the-roost", true) > count("the-fork", true));
    EXPECT_TRUE(count("the-drowned-cut", false) > count("the-fork", false));
    EXPECT_EQ(WorldCanyon::payScale(2), 1.0);
    EXPECT_EQ(WorldCanyon::payScale(3), 1.3);
}

/// Every canyon of the world plays its game: three zones in the sky, each
/// waiting, and a frontier defender in a canyon's region goes for the wave a
/// human there holds.
TEST(CanyonTwist_threeZonesAndAFrontierDefender) {
    WorldParams p = params();
    p.frontier = 5;
    WorldMap w(first(), p);
    const size_t r = into(w, "the-roost");
    w.step();
    ASSERT_TRUE(w.sky().has_value());
    EXPECT_EQ(w.canyons().size(), size_t(3));
    w.frontierRoster();
    WorldMap::FrontierPilot* d = nullptr;
    for (WorldMap::FrontierPilot& f : w.frontier())
        if (f.habit == WorldFrontier::Habit::defender) d = &f;
    ASSERT_TRUE(d != nullptr);
    d->arrived = true;
    d->region = *w.regionNamed("start");
    d->at = w.region(d->region).places()->pads[0].at;
    ASSERT_TRUE(w.canyonSend(r, "the-roost", 2));
    const FrontierPlan plan = w.frontierDecide(*d);
    std::printf("  defender: %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == FrontierPlan::Act::walk && plan.region == r);
}
