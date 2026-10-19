// The canyon game (Z2, docs/canyon.md): The Fork's waves in Cold Fork. Wave 1
// comes again and again until it is beaten, beating it starts the game, the
// waves grow, a wave is beaten when its bugs are dead and fails on the ore it
// takes or its time; a failed wave brings the Matriarch, who fells every
// turret of the zone, and the zone starts over; the last wave beaten holds
// the canyon a world day; the waves wait with no pilot near; the pay goes by
// damage; the game is saved. Isolated scenes: one wave or two at a time,
// stepped for a few world minutes at most; run from unreal/server.

#include "test.h"

#include "Brood.h"
#include "GunAim.h"
#include "Progression.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

using namespace ac;
using namespace ac::server;
using State = WorldMap::CanyonState;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId a = 81;
const std::string keyA = "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf";
constexpr PilotId b = 82;
const std::string keyB = "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf";
const std::string fork = "the-fork";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// No contracts, starfalls or director's groups: only the canyon's waves.
WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.missions = false;
    p.firstStep = 0;
    p.brood = false;
    p.broodSeed = 0xca2e0ULL;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(std::lround(seconds * 60));
    for (int i = 0; i < n; i++) w.step();
}

size_t coldFork(const WorldMap& w) { return *w.regionNamed("cold-fork"); }

/// A kept pilot at level 20 with blueprints, its Prospector at region cell
/// `cell` of Cold Fork (the valley south of the canyon: out of the prize's site).
void seatIn(WorldMap& w, PilotId p, const std::string& key, Vec2 cell = Vec2(100, 52)) {
    ASSERT_TRUE(w.seat(p, key) == WorldMap::Seated::fresh);
    ProspectorProgress g;
    g.xp = Progression::xp(20);
    for (const StructureKind k : Progression::blueprintKinds()) g.learned.insert(k);
    w.setProgress(p, g);
    w.step();
    const size_t r = coldFork(w);
    ASSERT_TRUE(w.devTransfer(p, r, w.region(r).places()->ground(cell)));
    w.step();
}

const WorldMap::Canyon& theFork(const WorldMap& w) { return *w.canyon(coldFork(w), fork); }

/// The bugs of the wave out alive, and their kinds.
int64_t alive(const WorldMap& w, UnitKind kind) {
    int64_t n = 0;
    for (const Unit& u : w.region(coldFork(w)).simulation().state.units)
        if (theFork(w).bugs.count(u.id) && u.hp > 0 && u.kind == kind) n++;
    return n;
}

/// Finished guns of the realm beside The Fork's paths, past `first` free places (the dev
/// line's: built by the first pilot on line).
void turrets(WorldMap& w, int64_t gatlings, int64_t towers, int64_t from) {
    std::printf("  %s\n", w.devCanyon("canyon turrets " + std::to_string(gatlings) + " " + std::to_string(towers) + " " + std::to_string(from)).c_str());
}

int64_t zoneTurrets(const WorldMap& w) {
    const WorldMap::Canyon& c = theFork(w);
    int64_t n = 0;
    for (const Structure& s : w.region(c.region).simulation().state.structures)
        if ((s.kind == StructureKind::gatling || s.kind == StructureKind::sentinel) && s.hp > 0 &&
            distance(s.position, c.middle) <= c.reach + Rules::radius(s.kind))
            n++;
    return n;
}

int64_t prizeOre(const WorldMap& w) {
    const WorldMap::Canyon& c = theFork(w);
    const region::Built& places = *w.region(c.region).places();
    const GameState& st = w.region(c.region).simulation().state;
    int64_t n = 0;
    for (const int64_t i : places.zones[c.zone].prizeDeposits)
        for (const OreDeposit& d : st.patches)
            if (distance(d.position, places.map.deposits[size_t(i)].position) < 0.01) n += d.remaining;
    return n;
}

std::vector<net::ProgressMessage::News> canyonNews(WorldMap& w, PilotId p) {
    std::vector<net::ProgressMessage::News> out;
    for (const auto& n : w.takeNews(p))
        if (n.kind == NewsKind::canyon) out.push_back(n);
    return out;
}

/// Steps in quarter seconds until `done` (true) or `limit` world seconds.
template <class F> double stepUntil(WorldMap& w, double limit, F done) {
    const double from = w.clock();
    while (w.clock() - from < limit) {
        stepFor(w, 0.25);
        if (done()) return w.clock() - from;
    }
    return -1;
}

} // namespace

/// WAITING: wave 1 comes a minute in, a few Shardlings down one arm; with no
/// defence it gnaws the prize until its time runs out and breaks through,
/// which loses nothing yet: it comes again three minutes later, down the
/// other arm.
TEST(Canyon_waveOneRepeatsUntilBeaten) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    EXPECT_TRUE(theFork(w).state == State::waiting);
    EXPECT_TRUE(!theFork(w).out);
    const double came = stepUntil(w, WorldCanyon::firstWait + 5, [&] { return theFork(w).out; });
    ASSERT_TRUE(came > 0);
    EXPECT_EQ(theFork(w).wave, int64_t(1));
    EXPECT_EQ(alive(w, UnitKind::shardling), int64_t(4));
    const uint32_t firstTunnel = theFork(w).tunnels;
    const int64_t full = prizeOre(w);
    // It breaks through: still waiting, wave 1 again later.
    double ends = 0;
    int64_t taken = 0;
    const double broke = stepUntil(w, 200, [&] {
        if (theFork(w).out) {
            ends = theFork(w).ends;
            taken = theFork(w).taken();
        }
        return !theFork(w).out;
    });
    std::printf("  wave 1 came after %.1f s from tunnel mask %u, broke through %.1f s later, %lld ore gnawed\n", came, firstTunnel, broke,
                (long long)taken);
    ASSERT_TRUE(broke > 0);
    EXPECT_TRUE(theFork(w).state == State::waiting);
    EXPECT_TRUE(w.clock() >= ends - 0.3);
    EXPECT_TRUE(taken > 0 && taken < WorldCanyon::failOre);
    EXPECT_TRUE(prizeOre(w) < full);
    EXPECT_TRUE(canyonNews(w, a).empty());
    EXPECT_TRUE(std::abs(theFork(w).next - (w.clock() + WorldCanyon::replayEvery)) < 0.5);
    const double again = stepUntil(w, WorldCanyon::replayEvery + 5, [&] { return theFork(w).out; });
    ASSERT_TRUE(again > 0);
    EXPECT_EQ(theFork(w).wave, int64_t(1));
    EXPECT_EQ(theFork(w).replays, int64_t(2));
    EXPECT_TRUE(theFork(w).tunnels != firstTunnel);
}

/// Beating wave 1 (turrets on the North arm's rim kill it) starts the game:
/// RUNNING, wave 2 after the break, the builder paid.
TEST(Canyon_beatingWaveOneStartsTheGame) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    turrets(w, 4, 0, 0);
    ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 1));
    EXPECT_TRUE(theFork(w).state == State::waiting);
    const double beaten = stepUntil(w, 120, [&] { return !theFork(w).out; });
    std::printf("  wave 1 down in %.1f s\n", beaten);
    ASSERT_TRUE(beaten > 0);
    EXPECT_TRUE(theFork(w).state == State::running);
    EXPECT_EQ(theFork(w).wave, int64_t(2));
    EXPECT_TRUE(std::abs(theFork(w).next - (w.clock() + WorldCanyon::breakFor)) < 0.5);
    const auto news = canyonNews(w, a);
    ASSERT_TRUE(news.size() == 1);
    EXPECT_TRUE(news[0].id.rfind("wave/1/", 0) == 0);
    EXPECT_TRUE(news[0].by == "The Fork");
    EXPECT_TRUE(news[0].xp > 0 && news[0].value > 0);
    // Wave 2 after the break.
    const double next = stepUntil(w, WorldCanyon::breakFor + 2, [&] { return theFork(w).out; });
    EXPECT_TRUE(next > WorldCanyon::breakFor - 1);
    EXPECT_EQ(theFork(w).wave, int64_t(2));
}

/// The waves grow: more bugs, both arms from wave 3, Gloamwings from wave 4,
/// Lodebacks in the mini-boss waves 5 and 10, ten in all.
TEST(Canyon_wavesEscalate) {
    const auto& waves = WorldCanyon::waves();
    ASSERT_TRUE(waves.size() == 10);
    auto bugs = [](const WorldCanyon::Wave& v) {
        int64_t n = 0;
        for (const auto& g : v.groups) n += g.count + g.escort;
        return n;
    };
    auto has = [](const WorldCanyon::Wave& v, UnitKind k) {
        for (const auto& g : v.groups)
            if (g.kind == k && g.count > 0) return true;
        return false;
    };
    for (size_t i = 0; i < waves.size(); i++) {
        std::printf("  wave %zu: %lld bugs%s\n", i + 1, (long long)bugs(waves[i]), waves[i].boss ? ", mini-boss" : "");
        if (i > 0) EXPECT_TRUE(bugs(waves[i]) >= bugs(waves[i - 1]) || waves[i - 1].boss);
    }
    EXPECT_TRUE(bugs(waves[9]) > 2 * bugs(waves[0]) + 20);
    for (size_t i = 0; i < 3; i++) EXPECT_TRUE(!has(waves[i], UnitKind::gloamwing) && !has(waves[i], UnitKind::lodeback));
    EXPECT_TRUE(has(waves[3], UnitKind::gloamwing));
    EXPECT_TRUE(waves[4].boss && waves[9].boss && has(waves[4], UnitKind::lodeback) && has(waves[9], UnitKind::lodeback));
    for (size_t i = 0; i < 10; i++)
        if (i != 4 && i != 9) EXPECT_TRUE(!waves[i].boss);
    // In the world: wave 3 down both arms, wave 4's Gloamwings in the air.
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 3));
    EXPECT_TRUE(theFork(w).state == State::running);
    EXPECT_EQ(theFork(w).tunnels, uint32_t(0b11));
    EXPECT_EQ(alive(w, UnitKind::shardling), int64_t(10));
    ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 4));
    EXPECT_EQ(alive(w, UnitKind::gloamwing), int64_t(3));
    EXPECT_EQ(alive(w, UnitKind::shardling), int64_t(7));
    ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 5));
    EXPECT_EQ(alive(w, UnitKind::lodeback), int64_t(2));
    // The pay rises with the wave.
    for (int64_t n = 2; n <= 10; n++)
        EXPECT_TRUE(WorldCanyon::xp(n) > WorldCanyon::xp(n - 1) && WorldCanyon::ore(n) > WorldCanyon::ore(n - 1));
}

/// A wave fails on the ore it takes (the mini-boss's Lodebacks eat, its
/// Shardlings gnaw: the fail mark well before its time is up), or on its
/// time running out with bugs on the prize (wave 2's Shardlings gnaw too
/// slowly to reach the mark first). A RUNNING wave failing is FAILED, told to
/// every pilot.
TEST(Canyon_failsOnTheOreTakenOrTheTime) {
    {
        WorldMap w(first(), params());
        seatIn(w, a, keyA);
        // X2: a pilot off line, its ledger filling.
        const std::string keyAway = "0a0b0c0d0e0f10111213141516171819";
        ASSERT_TRUE(w.seat(89, keyAway) == WorldMap::Seated::fresh);
        w.leave(89);
        ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 5));
        int64_t taken = 0;
        double ends = 0;
        const double broke = stepUntil(w, 200, [&] {
            if (theFork(w).out) {
                taken = theFork(w).taken();
                ends = theFork(w).ends;
            }
            return !theFork(w).out;
        });
        std::printf("  wave 5 broke through after %.1f s, at %lld ore taken, %.1f s before its time\n", broke, (long long)taken,
                    ends - w.clock());
        ASSERT_TRUE(broke > 0);
        EXPECT_TRUE(taken >= WorldCanyon::failOre - 3);
        EXPECT_TRUE(w.clock() < ends);
        EXPECT_TRUE(theFork(w).state == State::failed);
        const auto news = canyonNews(w, a);
        ASSERT_TRUE(news.size() == 1);
        EXPECT_TRUE(news[0].id == "failed" && news[0].value == 5 && news[0].by == "The Fork");
        {
            const auto* rows = w.ledgerOf(keyAway);
            ASSERT_TRUE(rows != nullptr);
            bool found = false;
            for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::fallen && (row.what == "The Fork" && row.value == 5));
            EXPECT_TRUE(found);
        }
    }
    {
        WorldMap w(first(), params());
        seatIn(w, a, keyA);
        ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 2));
        int64_t taken = 0;
        double ends = 0;
        const double broke = stepUntil(w, 200, [&] {
            if (theFork(w).out) {
                taken = theFork(w).taken();
                ends = theFork(w).ends;
            }
            return !theFork(w).out;
        });
        std::printf("  wave 2 broke through after %.1f s, at %lld ore taken, its time up\n", broke, (long long)taken);
        ASSERT_TRUE(broke > 0);
        EXPECT_TRUE(taken < WorldCanyon::failOre);
        EXPECT_TRUE(w.clock() >= ends - 0.3);
        EXPECT_TRUE(theFork(w).state == State::failed);
    }
}

/// A failed wave brings the Matriarch out of its tunnel: she fells every
/// turret of the zone (and none outside it), leaves, and the zone starts
/// over at WAITING and wave 1 with its prize full again.
TEST(Canyon_theMatriarchFellsTheZonesTurretsAndItResets) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    const size_t r = coldFork(w);
    turrets(w, 3, 0, 0);
    ASSERT_TRUE(zoneTurrets(w) == 3);
    // A gun outside the zone, in the valley.
    GameState& st = w.region(r).simulation().state;
    const Vec2 outside = w.region(r).places()->ground(Vec2(70, 60));
    st.structures.push_back(Structure(st.nextID++, StructureKind::gatling, Pilot::player, outside));
    st.structures.back().hp = Rules::hp(StructureKind::gatling);
    const int64_t outsideId = st.structures.back().id;
    w.region(r).simulation().reindex();
    ASSERT_TRUE(w.canyonSend(r, fork, 2));
    ASSERT_TRUE(w.canyonFail(r, fork));
    EXPECT_TRUE(theFork(w).state == State::failed);
    ASSERT_TRUE(w.siege().has_value());
    EXPECT_TRUE(w.siege()->region == r);
    // She comes out of the North tunnel.
    const region::Zone& z = w.region(r).places()->zones[0];
    bool fromTunnel = false;
    for (const Unit& u : st.units)
        if (u.kind == UnitKind::matriarch && distance(u.position, z.entrances[0].at) < 4) fromTunnel = true;
    EXPECT_TRUE(fromTunnel);
    // The wave gnawed at the prize a little: it comes back full on the reset.
    const int64_t full = [&] {
        int64_t n = 0;
        const region::Built& places = *w.region(r).places();
        for (const int64_t i : z.prizeDeposits) n += places.map.deposits[size_t(i)].amount;
        return n;
    }();
    double felled = -1;
    const double reset = stepUntil(w, 420, [&] {
        if (felled < 0 && zoneTurrets(w) == 0) felled = w.clock();
        return theFork(w).state == State::waiting;
    });
    std::printf("  every zone turret down after %.0f s; the zone back to waiting after %.0f s\n", felled - (w.clock() - reset), reset);
    ASSERT_TRUE(reset > 0);
    EXPECT_EQ(zoneTurrets(w), int64_t(0));
    bool outsideStands = false;
    for (const Structure& s : st.structures)
        if (s.id == outsideId && s.hp > 0) outsideStands = true;
    EXPECT_TRUE(outsideStands);
    EXPECT_EQ(theFork(w).wave, int64_t(1));
    EXPECT_TRUE(!theFork(w).out);
    EXPECT_EQ(prizeOre(w), full);
    EXPECT_TRUE(!w.siege().has_value() || w.siege()->home);
}

/// The last wave beaten holds the canyon a world day (no waves, the whole
/// world told), on the world's clock even with no pilot near; then WAITING
/// again with the prize full.
TEST(Canyon_heldAfterTheLastWave) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    // X2: a pilot off line, its ledger filling.
    const std::string keyAway = "0a0b0c0d0e0f10111213141516171819";
    ASSERT_TRUE(w.seat(89, keyAway) == WorldMap::Seated::fresh);
    w.leave(89);
    const size_t r = coldFork(w);
    ASSERT_TRUE(w.canyonSend(r, fork, 10));
    EXPECT_TRUE(theFork(w).out);
    ASSERT_TRUE(w.canyonBeat(r, fork));
    EXPECT_TRUE(theFork(w).state == State::held);
    EXPECT_TRUE(std::abs(theFork(w).next - (w.clock() + w.params().dayLength)) < 0.5);
    const auto news = canyonNews(w, a);
    ASSERT_TRUE(news.size() == 1);
    EXPECT_TRUE(news[0].id == "held" && news[0].by == "The Fork");
    {
        const auto* rows = w.ledgerOf(keyAway);
        ASSERT_TRUE(rows != nullptr);
        bool found = false;
        for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::held && (row.what == "The Fork"));
        EXPECT_TRUE(found);
    }
    {
        const auto* rows = w.ledgerOf(keyAway);
        ASSERT_TRUE(rows != nullptr);
        bool found = false;
        for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::waves && (row.what == "The Fork" && row.value == 1));
        EXPECT_TRUE(found);
    }
    // No wave in a few minutes with a pilot there.
    stepFor(w, 240);
    EXPECT_TRUE(theFork(w).state == State::held && !theFork(w).out);
    // The pilots mine it; the pilot goes far away; the day passes.
    GameState& st = w.region(r).simulation().state;
    const int64_t full = prizeOre(w);
    for (OreDeposit& d : st.patches)
        if (distance(d.position, theFork(w).middle) < 8) d.remaining /= 2;
    EXPECT_TRUE(prizeOre(w) < full);
    const size_t slag = *w.regionNamed("slag-pockets");
    ASSERT_TRUE(w.devTransfer(a, slag, w.region(slag).places()->pads[0].at));
    w.step();
    w.idle(w.params().dayLength);
    stepFor(w, 1);
    EXPECT_TRUE(theFork(w).state == State::waiting);
    EXPECT_EQ(theFork(w).wave, int64_t(1));
    EXPECT_EQ(prizeOre(w), full);
}

/// With no pilot in Cold Fork or next to it the waves wait: wave 1's
/// countdown stops, a wave out stays as it was until the region sleeps, then
/// comes again whole after a break once a pilot is back; the turrets stand.
TEST(Canyon_wavesPauseWithNoPilotNear) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    const size_t r = coldFork(w);
    turrets(w, 2, 0, 0);
    const size_t slag = *w.regionNamed("slag-pockets");
    const Vec2 home = w.region(r).places()->ground(Vec2(100, 52));
    // WAITING's countdown stops while away.
    const double left = theFork(w).next - w.clock();
    ASSERT_TRUE(w.devTransfer(a, slag, w.region(slag).places()->pads[0].at));
    stepFor(w, 120);
    EXPECT_TRUE(theFork(w).paused);
    EXPECT_TRUE(!theFork(w).out);
    EXPECT_TRUE(std::abs((theFork(w).next - w.clock()) - left) < 1);
    // A wave out when the pilot goes: nothing happens to it, nor to the
    // turrets; the region sleeps two minutes later, its bugs taken away.
    ASSERT_TRUE(w.devTransfer(a, r, home));
    w.step();
    ASSERT_TRUE(w.canyonSend(r, fork, 3));
    stepFor(w, 2);
    ASSERT_TRUE(w.devTransfer(a, slag, w.region(slag).places()->pads[0].at));
    stepFor(w, WorldBrood::idleDespawn + 30);
    EXPECT_TRUE(theFork(w).state == State::running);
    EXPECT_EQ(theFork(w).wave, int64_t(3));
    EXPECT_TRUE(!theFork(w).out);
    EXPECT_EQ(zoneTurrets(w), int64_t(2));
    for (const Structure& s : w.region(r).simulation().state.structures)
        if (s.kind == StructureKind::gatling) EXPECT_TRUE(s.hp >= Rules::hp(StructureKind::gatling) - 0.01);
    // Back: wave 3 again after the break.
    ASSERT_TRUE(w.devTransfer(a, r, home));
    const double again = stepUntil(w, WorldCanyon::breakFor + 3, [&] { return theFork(w).out; });
    std::printf("  wave 3 again %.1f s after the pilot came back\n", again);
    EXPECT_TRUE(again > WorldCanyon::breakFor - 2);
    EXPECT_EQ(theFork(w).wave, int64_t(3));
}

/// A wave beaten pays its fighters by the damage they did: two pilots'
/// guns on the North arm's rim (B on line in Prospect Landing: a region has
/// one pilot driving); the one whose gun killed more gets more, and together
/// they get the wave's pay.
TEST(Canyon_rewardsByDamage) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    const size_t r = coldFork(w);
    // A's gun on the north arm's rim where its arc reaches the arm the most
    // (`rig::rimGunSpot`), B's beside it, as close as the footprints let it
    // (Z4: anywhere standable), both over the arm wave 2 comes down; B's is
    // handed to B.
    Simulation& sim = w.region(r).simulation();
    {
        const auto& places = *w.region(r).places();
        const auto rim = rig::rimGunSpot(sim, places.zones.front(), places.pads, "North tunnel");
        ASSERT_TRUE(rim.has_value());
        const Vec2 at = places.regionAt(*rim);
        std::printf("  %s\n", w.devCanyon("canyon place gatling " + std::to_string(at.x) + " " + std::to_string(at.y)).c_str());
    }
    GameState& st = sim.state;
    const Vec2 gunAt = st.structures.back().position;
    std::optional<Vec2> beside;
    // Footprints side by side (C5: 5 cells across).
    const double side = 2 * Rules::radius(StructureKind::gatling);
    for (double rad = side; rad <= side + 2 && !beside; rad += 0.5)
        for (int k = 0; k < 16 && !beside; k++) {
            const double t = k * 2 * pi / 16;
            const auto q = sim.snap(StructureKind::gatling, gunAt + Vec2(std::cos(t), std::sin(t)) * rad);
            if (q && !sim.placementRefusal(StructureKind::gatling, *q) && rig::offTheLip(sim, *q)) beside = q;
        }
    ASSERT_TRUE(beside.has_value());
    st.structures.push_back(Structure(st.nextID++, StructureKind::gatling, Pilot::player, *beside));
    st.structures.back().hp = Rules::hp(StructureKind::gatling);
    const int64_t gunB = st.structures.back().id;
    sim.reindex();
    sim.refreshNav();
    // Hand it to B: a save and a restore keep who built what.
    WorldSave s = w.save();
    for (SavedRegion& sr : s.regions)
        for (SavedBuilding& sb : sr.buildings)
            if (sr.id == "cold-fork" && sb.id == gunB) sb.builtBy = keyB;
    WorldMap v(first(), params());
    ASSERT_TRUE(v.restore(s).empty());
    ASSERT_TRUE(v.seat(a, keyA) == WorldMap::Seated::restored);
    ASSERT_TRUE(v.seat(b, keyB) == WorldMap::Seated::restored);
    v.step();
    ASSERT_TRUE(v.canyonSend(r, fork, 2));
    const double beaten = stepUntil(v, 150, [&] { return !theFork(v).out; });
    ASSERT_TRUE(beaten > 0);
    EXPECT_TRUE(theFork(v).state == State::running);
    int64_t gunA = 0;
    for (const Structure& x : v.region(r).simulation().state.structures)
        if (x.kind == StructureKind::gatling && x.id != gunB) gunA = x.id;
    const int64_t killsA = v.turretKills(r, gunA), killsB = v.turretKills(r, gunB);
    const auto na = canyonNews(v, a), nb = canyonNews(v, b);
    ASSERT_TRUE(na.size() == 1 && nb.size() == 1);
    std::printf("  kills %lld / %lld; pay %.0f XP + %lld ore / %.0f XP + %lld ore\n", (long long)killsA, (long long)killsB, na[0].xp,
                (long long)na[0].value, nb[0].xp, (long long)nb[0].value);
    EXPECT_TRUE(killsA + killsB == 6);
    if (killsA != killsB) EXPECT_TRUE((killsA > killsB) == (na[0].xp > nb[0].xp));
    const bool floored = na[0].value <= std::llround(WorldCanyon::ore(2) * WorldCanyon::floorShare) ||
                         nb[0].value <= std::llround(WorldCanyon::ore(2) * WorldCanyon::floorShare);
    if (!floored) EXPECT_TRUE(std::abs(na[0].value + nb[0].value - WorldCanyon::ore(2)) <= 1);
}

/// The game is saved: a zone's state, wave and next change come back (a
/// wave out comes again after a break), through the store's schema 20; an
/// old save has the zone waiting.
TEST(Canyon_savedAndLoaded) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    const size_t r = coldFork(w);
    ASSERT_TRUE(w.canyonSend(r, fork, 4));
    stepFor(w, 1);
    const WorldSave s = w.save();
    // Z3: three zones in the world (The Drowned Cut, The Fork, The Roost),
    // by region id; The Fork's is the second.
    ASSERT_TRUE(s.canyons.size() == 3);
    EXPECT_TRUE(s.canyons[1].region == "cold-fork" && s.canyons[1].zone == fork);
    EXPECT_EQ(s.canyons[1].state, int64_t(State::running));
    EXPECT_EQ(s.canyons[1].wave, int64_t(4));
    // Through the file.
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/canyon.sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    std::string why;
    {
        auto store = WorldStore::open(path, first().file.id, &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_TRUE(store->schemaVersion() >= 20);
        ASSERT_TRUE(store->write(s, &why).has_value());
    }
    auto store = WorldStore::open(path, first().file.id, &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->canyons == s.canyons);
    WorldMap v(first(), params());
    const auto problems = v.restore(*loaded);
    for (const std::string& p : problems) std::printf("    %s\n", p.c_str());
    EXPECT_TRUE(problems.empty());
    EXPECT_TRUE(theFork(v).state == State::running);
    EXPECT_EQ(theFork(v).wave, int64_t(4));
    EXPECT_TRUE(!theFork(v).out);
    EXPECT_TRUE(std::abs(theFork(v).next - (s.clock + WorldCanyon::breakFor)) < 0.5);
    // HELD and FAILED come back with their ends.
    WorldSave held = s;
    held.canyons[1].state = int64_t(State::held);
    held.canyons[1].wave = 10;
    held.canyons[1].next = s.clock + 500;
    WorldMap h(first(), params());
    EXPECT_TRUE(h.restore(held).empty());
    EXPECT_TRUE(theFork(h).state == State::held && std::abs(theFork(h).next - (s.clock + 500)) < 1e-6);
    // A save from before the canyon game: waiting for wave 1.
    WorldSave old = s;
    old.canyons.clear();
    WorldMap o(first(), params());
    EXPECT_TRUE(o.restore(old).empty());
    EXPECT_TRUE(theFork(o).state == State::waiting && theFork(o).wave == 1);
    EXPECT_TRUE(std::abs(theFork(o).next - (s.clock + WorldCanyon::firstWait)) < 1e-6);
}

/// C2: a wave whose time runs out with no bug on the prize (the last one
/// held off it, where no round reaches) is beaten: the prize held, the
/// survivor leaves the wave, the game runs on.
TEST(Canyon_aWaveOutOfTimeOffThePrizeIsBeaten) {
    WorldMap w(first(), params());
    seatIn(w, a, keyA);
    const size_t r = coldFork(w);
    ASSERT_TRUE(w.canyonSend(r, fork, 1));
    w.step();
    ASSERT_TRUE(theFork(w).out);
    const int64_t full = prizeOre(w);
    Simulation& sim = w.region(r).simulation();
    // Every bug of the wave dead but one, held 40 cells off the prize.
    int64_t last = 0;
    for (Unit& u : sim.state.units)
        if (theFork(w).bugs.count(u.id)) {
            if (!last) last = u.id;
            else u.hp = 0;
        }
    ASSERT_TRUE(last != 0);
    // Away from the pilot's Prospector, which would fight it.
    Vec2 me = theFork(w).middle;
    if (const auto id = w.region(r).driving(a))
        if (const auto u = sim.state.unit(*id)) me = u->position;
    const Vec2 away = normalize(theFork(w).middle - me);
    std::optional<Vec2> at;
    for (const double turn : {0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5})
        for (const double d : {30.0, 25.0, 20.0, 15.0})
            if (!at) {
                const double h = std::atan2(away.y, away.x) + turn;
                const Vec2 q = theFork(w).middle + Vec2(std::cos(h), std::sin(h)) * d;
                if (sim.nav->walkable(q) && !sim.nav->inRock(q)) at = q;
            }
    ASSERT_TRUE(at.has_value());
    const Vec2 off = *at;
    const double ends = theFork(w).ends;
    const double done = stepUntil(w, ends - w.clock() + 10, [&] {
        for (Unit& u : sim.state.units)
            if (u.id == last && theFork(w).out) u.position = off;
        return !theFork(w).out;
    });
    std::printf("  out of time %.1f s after the wave's end\n", w.clock() - ends);
    // It left alive: the wave was beaten by its time, not its deaths.
    const auto still = sim.state.unit(last);
    EXPECT_TRUE(still && still->hp > 0);
    ASSERT_TRUE(done > 0);
    EXPECT_TRUE(w.clock() >= ends - 0.3);
    EXPECT_TRUE(theFork(w).state == State::running);
    EXPECT_EQ(theFork(w).wave, int64_t(2));
    EXPECT_EQ(prizeOre(w), full);
    // The survivor is on its way: nothing keeps it lingering.
    bool lingering = false;
    for (const WorldMap::BroodGroup& g : w.broodGroups(r))
        if (std::find(g.bugs.begin(), g.bugs.end(), last) != g.bugs.end()) lingering = g.lingerUntil > w.clock() + 1;
    EXPECT_TRUE(!lingering);
}
