// Citadel missions (K2, docs/missions.md): the board's offers by tier, drawn
// by seeded dice and again at dawn; signing up at the Citadel, three at
// most, abandoning; each kind finished and paid; the board and the Journal
// saved, and an older save given a board. Isolated scenes of a few seconds
// on The First Reach; files in build/save-tests. Run from unreal/server.

#include "test.h"

#include "CitadelSpot.h"

#include "Ringshadow.h"
#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;
using CM = net::CitadelMission;
using Kind = CM::Kind;
using Tier = CM::Tier;
using State = CM::State;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId a = 93;
const std::string keyA = "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// No contracts (they pay into the wallet).
WorldParams params(uint64_t seed = 0) {
    WorldParams p;
    p.contracts = false;
    p.missionSeed = seed;
    // Hunts only where a test asks for them.
    p.huntMissions = false;
    return p;
}

Simulation& simOf(WorldMap& w, PilotId p) { return w.regionOf(p)->simulation(); }

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

int64_t inputTick = 0;

void wait(WorldMap& w, PilotId p, int ticks) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        if (const Unit* u = drivenUnit(w, p)) in.heading = u->heading;
        w.input(p, in);
        w.step();
    }
}

void fill(WorldMap& w, PilotId p, int64_t ore, int64_t mh) {
    simOf(w, p).state.players[0].ore = ore;
    simOf(w, p).state.players[0].hydrogen = mh;
    w.step();
}

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// A `kind` put up west of the Prospector standing at `stand`, welded to
/// the end. Nil: refused, or not finished.
std::optional<int64_t> weld(WorldMap& w, PilotId p, StructureKind kind, Vec2 stand) {
    Simulation& sim = simOf(w, p);
    Unit* u = drivenUnit(w, p);
    u->task = Unit::Task::idle;
    u->position = stand;
    u->heading = pi;
    const auto spot = sim.snap(kind, stand + Vec2(-1, 0) * Region::buildReach(kind));
    if (!spot) return std::nullopt;
    const int64_t id = sim.state.nextID;
    if (w.build(p, kind, *spot)) return std::nullopt;
    for (int i = 0; i < int((Rules::buildTime(kind) + 5) * 60); i++) {
        wait(w, p, 1);
        if (const Structure* s = structure(sim, id); s && s->complete()) return id;
    }
    return std::nullopt;
}

/// The driven Prospector at the building's edge, where it unloads.
void besides(WorldMap& w, PilotId p, int64_t building) {
    const Structure s = *structure(simOf(w, p), building);
    Unit* u = drivenUnit(w, p);
    const Vec2 out = normalize(u->position - s.position);
    u->position = s.position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(s.kind) + 0.4);
    u->task = Unit::Task::idle;
    w.step();
}

/// Pilot A at level 10, its Citadel welded east of the start region's ore,
/// standing where it unloads.
struct Board1 {
    explicit Board1(WorldParams p = params()) : w(first(), p) {
        w.seat(a, keyA);
        ProspectorProgress g;
        g.xp = Progression::xp(10);
        g.learned.insert(StructureKind::citadel);
        w.setProgress(a, g);
        fill(w, a, 5000, 500);
        citadel = weld(w, a, StructureKind::citadel, rig::citadelStand(w, a, w.regionOf(a)->places()->ground(Vec2(72, 48))));
        if (citadel) besides(w, a, *citadel);
    }
    WorldMap w;
    std::optional<int64_t> citadel;
};

/// A mission put on the board by hand.
CM offer(uint32_t id, Kind kind, Tier tier, const std::string& target, int64_t need) {
    CM m;
    m.id = id;
    m.kind = kind;
    m.tier = tier;
    m.key = "test";
    m.title = "Test";
    m.briefing = "A test.";
    m.target = target;
    m.region = kind == Kind::haul ? "" : target;
    m.need = need;
    m.xp = WorldMissions::xp(tier, 10);
    m.ore = WorldMissions::ore(tier, 10);
    m.hydrogen = WorldMissions::hydrogen(tier, 10);
    return m;
}

CM taken(CM m) {
    m.state = State::taken;
    return m;
}

int64_t steps(const LoadedWorld& wd, const std::string& region) {
    const WorldRegion& home = wd.regions[wd.start];
    for (const WorldRegion& g : wd.regions)
        if (g.file.id == region) return std::abs(g.column - home.column) + std::abs(g.row - home.row);
    return -1;
}

std::string savePath(const std::string& name) {
    ::mkdir("build", 0755);
    ::mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

// The numbers: a tier pays more than the one below, in XP (8, 20 and 40
// minutes of play), ore and MH; it asks more.
TEST(missions_rewards_rise_by_tier) {
    for (const int64_t level : {1, 10, 50}) {
        EXPECT_TRUE(WorldMissions::xp(Tier::easy, level) < WorldMissions::xp(Tier::hard, level));
        EXPECT_TRUE(WorldMissions::xp(Tier::hard, level) < WorldMissions::xp(Tier::perilous, level));
        EXPECT_TRUE(WorldMissions::ore(Tier::easy, level) < WorldMissions::ore(Tier::hard, level));
        EXPECT_TRUE(WorldMissions::ore(Tier::hard, level) < WorldMissions::ore(Tier::perilous, level));
        EXPECT_TRUE(WorldMissions::hydrogen(Tier::easy, level) < WorldMissions::hydrogen(Tier::hard, level));
        EXPECT_TRUE(WorldMissions::hydrogen(Tier::hard, level) < WorldMissions::hydrogen(Tier::perilous, level));
    }
    // B2 (docs/balance.md): 3, 8, 15 minutes of play and 75, 225, 500 ore.
    EXPECT_NEAR(WorldMissions::xp(Tier::easy, 1), 225, 1e-9);
    EXPECT_NEAR(WorldMissions::xp(Tier::hard, 1), 600, 1e-9);
    EXPECT_NEAR(WorldMissions::xp(Tier::perilous, 1), 1125, 1e-9);
    EXPECT_EQ(WorldMissions::ore(Tier::easy, 1), int64_t(75));
    EXPECT_EQ(WorldMissions::hydrogen(Tier::perilous, 1), int64_t(70));
    EXPECT_TRUE(WorldMissions::haulOre(Tier::easy, 1) < WorldMissions::haulOre(Tier::hard, 1));
    EXPECT_TRUE(WorldMissions::scan(Tier::easy) < WorldMissions::scan(Tier::perilous));
    EXPECT_TRUE(WorldMissions::hold(Tier::easy) < WorldMissions::hold(Tier::perilous));
    EXPECT_TRUE(WorldMissions::kills(Tier::easy) < WorldMissions::kills(Tier::perilous));
    EXPECT_TRUE(WorldMissions::tierAt(0) == Tier::easy && WorldMissions::tierAt(1) == Tier::hard &&
                WorldMissions::tierAt(2) == Tier::perilous && WorldMissions::tierAt(4) == Tier::perilous);
}

// The board: two offers a tier, drawn on the pilot's first step by the
// dice of the world, the pilot and the day; worded with no rolled numbers
// in the briefing (the narration reads it); easy ones at home, hard ones
// next door, perilous ones in the far corners; the hard and perilous ones
// pay a blueprint copy or supplies. No hunt until the Brood is in.
TEST(missions_board_by_tier_seeded) {
    Board1 s, t;
    ASSERT_TRUE(s.citadel.has_value() && t.citadel.has_value());
    const auto board = s.w.board(a);
    ASSERT_TRUE(board.size() == 3 * WorldMissions::offers);
    EXPECT_TRUE(board == t.w.board(a));
    for (size_t i = 0; i < board.size(); i++) {
        const CM& m = board[i];
        EXPECT_TRUE(m.tier == Tier(i / WorldMissions::offers));
        EXPECT_TRUE(m.state == State::offered);
        EXPECT_TRUE(m.kind != Kind::hunt);
        EXPECT_TRUE(!m.key.empty() && !m.title.empty() && !m.briefing.empty() && !m.instructions.empty());
        EXPECT_TRUE(m.instructions.find('{') == std::string::npos);
        for (const char c : m.briefing) EXPECT_TRUE(c < '0' || c > '9');
        EXPECT_TRUE(m.xp == WorldMissions::xp(m.tier, 10) && m.need >= 1);
        if (m.kind == Kind::haul) {
            EXPECT_TRUE(m.tier != Tier::perilous);
            EXPECT_TRUE(m.target == "ore" || m.target == "hydrogen");
        } else {
            EXPECT_EQ(steps(first(), m.region), int64_t(m.tier == Tier::perilous ? 2 : int(m.tier)));
        }
        EXPECT_TRUE(m.tier == Tier::easy ? m.count == 0 : m.count > 0 && (m.blueprint.has_value() != m.item.has_value()));
        std::printf("  %s [%s] %s\n", m.key.c_str(), m.title.c_str(), m.instructions.c_str());
    }
    // The two offers of a tier are of two kinds.
    for (size_t i = 0; i < board.size(); i += 2) EXPECT_TRUE(board[i].kind != board[i + 1].kind);
    // The message carries it.
    EXPECT_TRUE(s.w.progressMessage(a)->board == board);
    // Over some days every kind but the hunt comes (a haul: the realm has
    // its Citadel).
    std::set<CM::Kind> kinds;
    for (int day = 0; day < 12; day++) {
        s.w.setClock(s.w.nextDawn(s.w.clock()) + 1);
        wait(s.w, a, 1);
        for (const CM& m : s.w.board(a)) kinds.insert(m.kind);
    }
    EXPECT_TRUE((kinds == std::set<CM::Kind>{Kind::haul, Kind::survey, Kind::salvage, Kind::hold}));
    // Another seed, another board; hunts drawn once the Brood is in.
    WorldParams hunts = params(99);
    hunts.huntMissions = true;
    WorldMap h(first(), hunts);
    ASSERT_TRUE(h.join(a));
    bool hunt = false;
    for (int day = 0; day < 20 && !hunt; day++) {
        h.setClock(h.nextDawn(h.clock()) + 1);
        wait(h, a, 1);
        for (const CM& m : h.board(a)) hunt = hunt || m.kind == Kind::hunt;
    }
    EXPECT_TRUE(hunt);
}

// Refreshed at the next dawn: new offers with new numbers; a taken mission
// stays in the Journal, a done one leaves it.
TEST(missions_board_refreshes_at_dawn) {
    Board1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const auto before = w.board(a);
    ASSERT_TRUE(!w.takeMission(a, before[0].id));
    CM done = taken(offer(90, Kind::hunt, Tier::easy, "start", 3));
    done.state = State::done;
    auto journal = w.journal(a);
    journal.push_back(done);
    w.setJournal(a, journal);
    wait(w, a, 5);
    EXPECT_TRUE(w.board(a).size() == before.size() - 1);
    w.setClock(w.nextDawn(w.clock()) + 0.5);
    wait(w, a, 1);
    const auto after = w.board(a);
    ASSERT_TRUE(after.size() == 3 * WorldMissions::offers);
    for (const CM& m : after) EXPECT_TRUE(m.id > 90);
    ASSERT_TRUE(w.journal(a).size() == 1);
    EXPECT_TRUE(w.journal(a)[0].id == before[0].id && w.journal(a)[0].state == State::taken);
}

// Signing up: at a Citadel only, three at once at most, an offer
// on the board only; abandoning: a taken one, from anywhere, free; a done
// one stays.
TEST(missions_take_abandon_and_limit) {
    Board1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const auto board = w.board(a);
    EXPECT_TRUE(w.takeMission(a, 999) == std::optional<std::string>("No such mission on the board"));
    for (int i = 0; i < 3; i++) EXPECT_TRUE(!w.takeMission(a, board[size_t(i)].id).has_value());
    EXPECT_TRUE(w.takeMission(a, board[3].id) ==
                std::optional<std::string>("Your Journal holds 3 missions: finish or abandon one (L)"));
    ASSERT_TRUE(w.journal(a).size() == 3);
    for (const CM& m : w.journal(a)) EXPECT_TRUE(m.state == State::taken);
    EXPECT_TRUE(w.board(a).size() == board.size() - 3);
    EXPECT_TRUE(w.progressMessage(a)->journal == w.journal(a));
    // Abandoned from far away: free.
    drivenUnit(w, a)->position = drivenUnit(w, a)->position + Vec2(-20, 0);
    wait(w, a, 2);
    const auto wallet = *w.wallet(a);
    EXPECT_TRUE(!w.abandonMission(a, board[1].id).has_value());
    EXPECT_TRUE(w.abandonMission(a, board[1].id) == std::optional<std::string>("No such mission in your Journal"));
    EXPECT_TRUE(*w.wallet(a) == wallet);
    EXPECT_TRUE(w.journal(a).size() == 2);
    // Away from the Citadel: refused.
    EXPECT_TRUE(w.takeMission(a, board[3].id) == std::optional<std::string>("Sign up at a Citadel: stand where you unload"));
    besides(w, a, *s.citadel);
    EXPECT_TRUE(!w.takeMission(a, board[3].id).has_value());
    // A done one can't be dropped.
    auto journal = w.journal(a);
    journal[0].state = State::done;
    w.setJournal(a, journal);
    EXPECT_TRUE(w.abandonMission(a, journal[0].id) == std::optional<std::string>("That mission is done already"));
}

// A haul: ore unloaded at a Citadel counts; done, it pays its XP,
// ore and MH once, with its news, and supplies up to the stack.
TEST(missions_haul_completes_and_pays) {
    Board1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    CM m = taken(offer(50, Kind::haul, Tier::hard, "ore", 300));
    m.item = Item::repairKit;
    m.count = 2;
    w.setJournal(a, {m});
    ProspectorProgress g = *w.progress(a);
    g.items[Item::repairKit] = 4;
    w.setProgress(a, g);
    w.takeNews(a);
    const double xp = w.progress(a)->xp;
    const auto wallet = *w.wallet(a);
    drivenUnit(w, a)->carrying = 200;
    drivenUnit(w, a)->hydrogen = false;
    wait(w, a, 10);
    EXPECT_EQ(w.journal(a)[0].have, int64_t(200));
    EXPECT_TRUE(w.journal(a)[0].state == State::taken);
    drivenUnit(w, a)->carrying = 200;
    wait(w, a, 10);
    const CM done = w.journal(a)[0];
    EXPECT_TRUE(done.state == State::done && done.have == 300);
    // The unload's own XP (400 ore), the mission's.
    EXPECT_NEAR(w.progress(a)->xp - xp, 400 * WorldXp::ore + m.xp, 1e-6);
    // The ore unloaded less the Citadel's tax, and the mission's ore and MH.
    const auto now = *w.wallet(a);
    EXPECT_EQ(now.hydrogen - wallet.hydrogen, m.hydrogen);
    EXPECT_TRUE(now.ore - wallet.ore >= m.ore + 300);
    // Supplies: two kits on four, the stack is five.
    EXPECT_EQ(w.progress(a)->held(Item::repairKit), int64_t(5));
    const auto news = w.takeNews(a);
    bool told = false;
    for (const auto& n : news) told = told || (n.kind == NewsKind::mission && n.id == "50" && n.value == m.ore && n.by == m.title && !n.by.empty());
    EXPECT_TRUE(told);
    // More ore: nothing more.
    const double after = w.progress(a)->xp;
    drivenUnit(w, a)->carrying = 100;
    wait(w, a, 10);
    EXPECT_NEAR(w.progress(a)->xp - after, 100 * WorldXp::ore, 1e-6);
}

// A survey: inside its ring for its seconds (a second step outside does
// not count); pays a blueprint copy.
TEST(missions_survey_completes) {
    Board1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    CM m = taken(offer(51, Kind::survey, Tier::easy, "start", 3));
    m.at = drivenUnit(w, a)->position + Vec2(15, 0);
    m.radius = WorldMissions::surveyRadius;
    m.blueprint = StructureKind::habDome;
    m.count = 1;
    w.setJournal(a, {m});
    const int64_t copies = w.progress(a)->copies(StructureKind::habDome);
    wait(w, a, 5 * 60);
    EXPECT_EQ(w.journal(a)[0].have, int64_t(0));
    drivenUnit(w, a)->position = m.at + Vec2(WorldMissions::surveyRadius - 1, 0);
    wait(w, a, 2 * 60 + 5);
    EXPECT_EQ(w.journal(a)[0].have, int64_t(2));
    drivenUnit(w, a)->position = m.at + Vec2(WorldMissions::surveyRadius + 2, 0);
    wait(w, a, 3 * 60);
    EXPECT_EQ(w.journal(a)[0].have, int64_t(2));
    drivenUnit(w, a)->position = m.at;
    wait(w, a, 60);
    EXPECT_TRUE(w.journal(a)[0].state == State::done);
    EXPECT_EQ(w.progress(a)->copies(StructureKind::habDome), copies + 1);
}

// A hold: inside its ring only counts while the ringshadow band covers it.
TEST(missions_hold_counts_under_the_band) {
    Board1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const Vec2 at = drivenUnit(w, a)->position;
    CM m = taken(offer(52, Kind::hold, Tier::easy, "start", 3));
    m.at = at;
    m.radius = WorldMissions::holdRadius;
    w.setJournal(a, {m});
    // In the sun: nothing.
    const auto sweep = Ringshadow::next(w.clock(), w.params().dayLength);
    wait(w, a, 4 * 60);
    EXPECT_EQ(w.journal(a)[0].have, int64_t(0));
    // Deep under the band.
    const GroundRect bounds = simOf(w, a).map ? simOf(w, a).map->bounds : GroundRect{};
    double under = -1;
    for (double t = sweep.start; t + 10 < sweep.end && under < 0; t += 0.5)
        if (Ringshadow::cover(t, at, bounds, w.params().dayLength) >= 1 &&
            Ringshadow::cover(t + 10, at, bounds, w.params().dayLength) >= 1)
            under = t;
    ASSERT_TRUE(under > 0);
    w.setClock(under);
    wait(w, a, 4 * 60);
    EXPECT_TRUE(w.journal(a)[0].state == State::done);
}

// A salvage: done when its site is opened (here, by hand); a hunt: each
// Brood kill in its region counts, one elsewhere doesn't.
TEST(missions_salvage_and_hunt_complete) {
    Board1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const auto& sites = first().regions[first().start].built.sites;
    ASSERT_TRUE(!sites.empty());
    const std::string site = w.siteId(first().start, 0);
    w.setJournal(a, {taken(offer(53, Kind::salvage, Tier::easy, site, 1)), taken(offer(54, Kind::hunt, Tier::hard, "cold-fork", 2))});
    wait(w, a, 2);
    EXPECT_TRUE(w.journal(a)[0].state == State::taken);
    ProspectorProgress g = *w.progress(a);
    g.sites.insert(site);
    w.setProgress(a, g);
    wait(w, a, 1);
    EXPECT_TRUE(w.journal(a)[0].state == State::done);
    const size_t cold = *w.regionNamed("cold-fork");
    w.broodKill(a, UnitKind::prospector, first().start);
    EXPECT_EQ(w.journal(a)[1].have, int64_t(0));
    w.broodKill(a, UnitKind::prospector, cold);
    EXPECT_EQ(w.journal(a)[1].have, int64_t(1));
    const double xp = w.progress(a)->xp;
    w.broodKill(a, UnitKind::prospector, cold);
    EXPECT_TRUE(w.journal(a)[1].state == State::done);
    EXPECT_NEAR(w.progress(a)->xp - xp, WorldMissions::xp(Tier::hard, 10), 1e-6);
}

// Saved and loaded: the board, the Journal, the day and the next number
// come back, and the board is not drawn again that day.
TEST(missions_saved_and_loaded) {
    const std::string path = savePath("missions");
    std::string why;
    std::vector<CM> board, journal;
    {
        Board1 s;
        ASSERT_TRUE(s.citadel.has_value());
        WorldMap& w = s.w;
        ASSERT_TRUE(!w.takeMission(a, w.board(a)[2].id));
        auto j = w.journal(a);
        j[0].have = 1;
        w.setJournal(a, j);
        board = w.board(a);
        journal = w.journal(a);
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        EXPECT_TRUE(!store->rows("SELECT missions FROM pilots")[0][0].empty());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(w.board(a) == board);
    EXPECT_TRUE(w.journal(a) == journal);
    wait(w, a, 1);
    EXPECT_TRUE(w.board(a) == board);
}

// A save from before missions (schema 10) opens at 12: its pilot has no
// board until its first step draws one.
TEST(missions_v10_save_migrates_and_draws) {
    const std::string path = savePath("v10missions");
    std::string why;
    std::vector<WorldStore::Migration> v10;
    for (const auto& m : WorldStore::migrations())
        if (m.version <= 10) v10.push_back(m);
    {
        auto store = WorldStore::open(path, "first", &why, v10);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(10));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, stacked) VALUES ('" +
                                    keyA + "', 40, 7, 'start', NULL, '', '', 1, 1)",
                                &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->pilots.size() == 1);
    EXPECT_TRUE(loaded->pilots[0].missions.empty());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(w.board(a).empty());
    wait(w, a, 1);
    EXPECT_EQ(w.board(a).size(), size_t(3 * WorldMissions::offers));
    EXPECT_TRUE(w.journal(a).empty());
}

/// The K2 pictures' board (`missions here`, Q2): a new pilot at the dev
/// rig's home (its Citadel: haul offers too) has an easy survey in its
/// region only by its key's dice; the dev line draws it again until one is,
/// for any key.
TEST(missions_dev_here_puts_an_easy_point_offer_here) {
    int before = 0;
    for (int k = 0; k < 16; k++) {
        WorldParams p = params();
        p.devRig = true;
        WorldMap w(first(), p);
        ASSERT_TRUE(w.seat(a, "q2-key-" + std::to_string(k)) == WorldMap::Seated::fresh);
        w.step();
        const std::string here = "start";
        auto has = [&] {
            for (const CM& m : w.board(a))
                if (m.tier == Tier::easy && m.kind == Kind::survey && m.region == here) return true;
            return false;
        };
        before += has() ? 1 : 0;
        const std::string said = w.devMissions("missions here");
        EXPECT_TRUE(said.find("an easy survey in start") != std::string::npos);
        EXPECT_TRUE(has());
        EXPECT_EQ(w.board(a).size(), size_t(3 * WorldMissions::offers));
    }
    std::printf("  %d of 16 new pilots' boards had one before the dev line\n", before);
}
