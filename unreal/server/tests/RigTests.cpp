// The rig in a world (P7; WorldRig.cpp, docs/prospector-progression.md
// "Foundry and Lab"): the Foundry makes modules for ore and MH on the
// world's clock (one at a time, away or not), the Lab researches Mk II and
// Mk III first, modules are fitted at the Citadel or a Hab Dome, each one's
// stat lands in the sim, and the rig is saved. Isolated scenes on The First
// Reach; run from unreal/server.

#include "test.h"
#include "CitadelSpot.h"

#include "Rules.h"
#include "Session.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 71;
const std::string key = "00112233445566778899aabbccddeeff";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Every blueprint, no contracts (they pay MH into the wallet).
WorldParams params() {
    WorldParams p;
    p.devBlueprints = true;
    p.contracts = false;
    return p;
}

/// A pilot at `level` with these ranks and every blueprint.
ProspectorProgress at(int64_t level, const std::map<Skill, int64_t>& ranks = {}) {
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    g.ranks = ranks;
    for (const StructureKind k : Progression::blueprintKinds()) g.learned.insert(k);
    return g;
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

void wait(WorldMap& w, PilotId p, double seconds) {
    const int ticks = int(std::llround(seconds / w.start().description().step));
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        if (const Unit* u = drivenUnit(w, p)) in.heading = u->heading;
        w.input(p, in);
        w.step();
    }
}

/// The wallet set where the pilot is (the side's bank).
void fill(WorldMap& w, PilotId p, int64_t ore, int64_t mh) {
    simOf(w, p).state.players[0].ore = ore;
    simOf(w, p).state.players[0].hydrogen = mh;
    w.step();
}

/// A `kind` of the pilot's put up round its Prospector, off the pads'
/// plates, and finished at once by hand. Nil: no spot, or refused.
std::optional<int64_t> put(WorldMap& w, PilotId p, StructureKind kind, std::string* why = nullptr) {
    Simulation& sim = simOf(w, p);
    const auto& pads = first().regions[*w.where(p)].built.pads;
    const double reach = Region::buildReach(kind);
    // No room round it (C5: the 7x7s): beside the nearest open ground.
    bool room = false;
    for (int k = 0; k < 16 && !room; k++) {
        const Vec2 at = drivenUnit(w, p)->position + Vec2(std::cos(k * 2 * pi / 16), std::sin(k * 2 * pi / 16)) * reach;
        const auto spot = sim.snap(kind, at);
        room = spot && !sim.placementRefusal(kind, *spot) && !region::touchesPadPlate(pads, *spot, Rules::radius(kind)) &&
               !NavGrid::insideStructure(drivenUnit(w, p)->position, Structure(-1, kind, 0, *spot), Rules::radius(UnitKind::prospector));
    }
    if (!room)
        if (const auto spot = rig::placeableNear(sim, pads, drivenUnit(w, p)->position, kind)) {
            drivenUnit(w, p)->position = rig::weldStand(kind, *spot);
            w.step();
        }
    const Unit u = *drivenUnit(w, p);
    for (double d = reach; d <= reach + Region::buildSlack; d += 0.5)
        for (int k = 0; k < 16; k++) {
            const double h = k * 2 * pi / 16;
            const auto spot = sim.snap(kind, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            // Not over the Prospector: a big square's corner reaches inside
            // the ring (C5: the 17x17 pad at 45 degrees).
            if (!spot || distance(*spot, u.position) > reach + Region::buildSlack || sim.placementRefusal(kind, *spot) ||
                region::touchesPadPlate(pads, *spot, Rules::radius(kind)) ||
                NavGrid::insideStructure(u.position, Structure(-1, kind, 0, *spot), Rules::radius(UnitKind::prospector)))
                continue;
            const int64_t id = sim.state.nextID;
            if (const auto no = w.build(p, kind, *spot)) {
                if (why) *why = *no;
                return std::nullopt;
            }
            for (Structure& s : sim.state.structures)
                if (s.id == id) {
                    s.buildLeft = std::nullopt;
                    s.hp = sim.fullHP(s);
                }
            w.step();
            return id;
        }
    return std::nullopt;
}

const Structure* building(WorldMap& w, PilotId p, int64_t id) {
    for (const Structure& s : simOf(w, p).state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// The driven Prospector moved to `at` (ground) and told it.
void moveTo(WorldMap& w, PilotId p, Vec2 at) {
    drivenUnit(w, p)->position = at;
    w.step();
}

/// The driven Prospector stood where it unloads at building `id`.
void besides(WorldMap& w, PilotId p, int64_t id) {
    const Structure b = *building(w, p, id);
    Vec2 out = drivenUnit(w, p)->position - b.position;
    out = length(out) > 1e-6 ? normalize(out) : Vec2(1, 0);
    moveTo(w, p, b.position + out * (Rules::radius(b.kind) + 0.5));
}

/// The pilot's progress with `edit` applied (a rig set up by hand).
void edit(WorldMap& w, PilotId p, const std::function<void(Rig&)>& f) {
    ProspectorProgress g = *w.progress(p);
    f(g.rig);
    w.setProgress(p, g);
}

bool told(const std::vector<net::ProgressMessage::News>& news, net::ProgressMessage::NewsKind kind, const std::string& id,
          int64_t value) {
    for (const auto& n : news)
        if (n.kind == kind && n.id == id && n.value == value) return true;
    return false;
}

/// A level-30 pilot with a Citadel standing beside it, and its driven
/// Prospector where it unloads there; the Citadel's id.
int64_t home(WorldMap& w, const std::map<Skill, int64_t>& ranks = {}) {
    w.seat(me, key);
    w.setProgress(me, at(30, ranks));
    fill(w, me, 5000, 5000);
    const auto citadel = put(w, me, StructureKind::citadel);
    besides(w, me, *citadel);
    return *citadel;
}

} // namespace

// The Foundry makes a module: 100 ore + 50 MH and 60 s for Mk I, one at a
// time, refused when short or with no Foundry; made while the pilot is
// away, told when it comes back.
TEST(rig_make_costs_time_and_one_at_a_time) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(20));
    fill(w, me, 1000, 500);
    EXPECT_EQ(w.make(me, Module::drillHead, 1).value_or(""), std::string("Build a Foundry first"));
    ASSERT_TRUE(put(w, me, StructureKind::foundry).has_value());
    EXPECT_TRUE(w.realmFinished(StructureKind::foundry));
    EXPECT_TRUE(w.progressMessage(me)->foundry);
    EXPECT_TRUE(!w.progressMessage(me)->lab);
    fill(w, me, 50, 500);
    EXPECT_EQ(w.make(me, Module::drillHead, 1).value_or(""), std::string("Not enough ore"));
    fill(w, me, 1000, 10);
    EXPECT_EQ(w.make(me, Module::drillHead, 1).value_or(""), std::string("Not enough MH"));
    fill(w, me, 1000, 500);
    const double start = w.clock();
    ASSERT_TRUE(!w.make(me, Module::drillHead, 1));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(900));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(450));
    const auto job = w.progressMessage(me)->rig.making;
    ASSERT_TRUE(job.has_value());
    EXPECT_TRUE(job->module == Module::drillHead && job->mark == 1);
    EXPECT_NEAR(job->done, start + 60, 1e-9);
    // One at a time: nothing taken for the second.
    EXPECT_EQ(w.make(me, Module::cargoPod, 1).value_or(""), std::string("The Foundry is busy: Drill head Mk I"));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(900));
    // Not yet at 59 s.
    w.takeNews(me);
    wait(w, me, 59 - (w.clock() - start));
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 1), int64_t(0));
    // The pilot leaves; the Foundry goes on, and it is told on its return.
    w.leave(me);
    for (int i = 0; i < 60; i++) w.step();
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 1), int64_t(1));
    EXPECT_TRUE(!w.progress(me)->rig.making.has_value());
    EXPECT_TRUE(told(w.takeNews(me), net::ProgressMessage::NewsKind::made, "drillHead", 1));
    // Not fitted until fitted: the sim's drill is the hero's.
    EXPECT_EQ(simOf(w, me).rate(Stat::Drill{}, *drivenUnit(w, me)), 1.0);
    // Free again: Mk I of another.
    EXPECT_TRUE(!w.make(me, Module::cargoPod, 1));
}

// The Lab opens Mk II, then Mk III, for MH and time, one at a time; the
// Foundry makes a mark only once researched (Mk II: 300 + 200, 180 s).
TEST(rig_research_gates_the_marks) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(30));
    fill(w, me, 5000, 5000);
    ASSERT_TRUE(put(w, me, StructureKind::foundry).has_value());
    EXPECT_EQ(w.make(me, Module::plate, 2).value_or(""), std::string("Plate Mk II needs research at the Lab first"));
    EXPECT_EQ(w.research(me, Module::plate, 2).value_or(""), std::string("Build a Lab first"));
    ASSERT_TRUE(put(w, me, StructureKind::lab).has_value());
    EXPECT_TRUE(w.progressMessage(me)->lab);
    EXPECT_EQ(w.research(me, Module::plate, 1).value_or(""), std::string("Only Mk II and Mk III are researched"));
    EXPECT_EQ(w.research(me, Module::plate, 3).value_or(""), std::string("Plate Mk III needs Mk II researched first"));
    fill(w, me, 5000, 299);
    EXPECT_EQ(w.research(me, Module::plate, 2).value_or(""), std::string("Not enough MH"));
    fill(w, me, 5000, 5000);
    const double start = w.clock();
    ASSERT_TRUE(!w.research(me, Module::plate, 2));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(4700));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(5000));
    ASSERT_TRUE(w.progress(me)->rig.research.has_value());
    EXPECT_NEAR(w.progress(me)->rig.research->done, start + 600, 1e-9);
    EXPECT_EQ(w.research(me, Module::torchTip, 2).value_or(""), std::string("The Lab is busy: Plate Mk II"));
    // The Foundry works beside it.
    EXPECT_TRUE(!w.make(me, Module::plate, 1));
    // Ten minutes are long for a test: the Lab's job brought to a second
    // from now by hand; it opens Mk II on the clock.
    edit(w, me, [&](Rig& r) { r.research->done = w.clock() + 1; });
    w.takeNews(me);
    wait(w, me, 1.1);
    EXPECT_EQ(w.progress(me)->rig.researchedMark(Module::plate), int64_t(2));
    EXPECT_TRUE(told(w.takeNews(me), net::ProgressMessage::NewsKind::researched, "plate", 2));
    EXPECT_EQ(w.research(me, Module::plate, 2).value_or(""), std::string("Plate Mk II is researched already"));
    // Mk II made now (once the Foundry is free): 300 + 200.
    edit(w, me, [&](Rig& r) { r.making->done = w.clock(); });
    w.step();
    fill(w, me, 5000, 5000);
    ASSERT_TRUE(!w.make(me, Module::plate, 2));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(4700));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(4800));
    EXPECT_NEAR(w.progress(me)->rig.making->done, w.clock() + 180, 1e-9);
    // Mk III: 900 MH and 30 minutes.
    fill(w, me, 5000, 5000);
    ASSERT_TRUE(!w.research(me, Module::plate, 3));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(4100));
    EXPECT_NEAR(w.progress(me)->rig.research->done, w.clock() + 1800, 1e-9);
}

// Fitting: only at the pilot's own Citadel or Hab Dome, from the stores;
// the old module goes back to them, and 0 takes it off.
TEST(rig_fit_at_home_only) {
    WorldMap w(first(), params());
    const int64_t citadel = home(w);
    edit(w, me, [](Rig& r) {
        r.store(Module::drillHead, 1);
        r.store(Module::drillHead, 2);
    });
    // Away from home: refused.
    const Structure c = *building(w, me, citadel);
    moveTo(w, me, c.position + normalize(drivenUnit(w, me)->position - c.position) * (Rules::radius(StructureKind::citadel) + 8));
    EXPECT_EQ(w.fit(me, Module::drillHead, 1).value_or(""),
              std::string("Fit modules at your Citadel or a Hab Dome: stand where you unload"));
    // At a Hab Dome of its own.
    const auto dome = put(w, me, StructureKind::habDome);
    ASSERT_TRUE(dome.has_value());
    besides(w, me, *dome);
    EXPECT_EQ(w.fit(me, Module::drillHead, 3).value_or(""), std::string("No Drill head Mk III in your stores"));
    ASSERT_TRUE(!w.fit(me, Module::drillHead, 1));
    EXPECT_EQ(w.progress(me)->rig.fittedMark(Module::drillHead), int64_t(1));
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 1), int64_t(0));
    EXPECT_EQ(w.fit(me, Module::drillHead, 1).value_or(""), std::string("Drill head Mk I is fitted already"));
    // At the Citadel: Mk II in, Mk I back to the stores.
    besides(w, me, citadel);
    ASSERT_TRUE(!w.fit(me, Module::drillHead, 2));
    EXPECT_EQ(w.progress(me)->rig.fittedMark(Module::drillHead), int64_t(2));
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 1), int64_t(1));
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 2), int64_t(0));
    EXPECT_TRUE(w.progressMessage(me)->rig.fitted == (std::map<Module, int64_t>{{Module::drillHead, 2}}));
    // Taken off.
    ASSERT_TRUE(!w.fit(me, Module::drillHead, 0));
    EXPECT_EQ(w.progress(me)->rig.fittedMark(Module::drillHead), int64_t(0));
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 2), int64_t(1));
    EXPECT_EQ(w.fit(me, Module::drillHead, 0).value_or(""), std::string("No Drill head fitted"));
}

namespace {

/// `module` at `mark` fitted at home through the request.
void fitted(WorldMap& w, Module module, int64_t mark) {
    edit(w, me, [&](Rig& r) { r.store(module, mark); });
    ASSERT_TRUE(!w.fit(me, module, mark));
}

} // namespace

// The drill head: drill +8% at Mk I; Mk III's 25% adds to the tree's Quick
// drill (rank 3, 18%): ×1.43.
TEST(rig_drill_head_in_the_sim) {
    WorldMap w(first(), params());
    home(w);
    EXPECT_EQ(simOf(w, me).rate(Stat::Drill{}, *drivenUnit(w, me)), 1.0);
    fitted(w, Module::drillHead, 1);
    EXPECT_NEAR(simOf(w, me).rate(Stat::Drill{}, *drivenUnit(w, me)), 1.08, 1e-12);
    WorldMap v(first(), params());
    home(v, {{Skill::quickDrill, 3}});
    fitted(v, Module::drillHead, 3);
    EXPECT_NEAR(simOf(v, me).rate(Stat::Drill{}, *drivenUnit(v, me)), 1.43, 1e-12);
}

// The cargo pod: ore and MH a trip +16% at Mk II (25 → 29, 20 → 23).
TEST(rig_cargo_pod_in_the_sim) {
    WorldMap w(first(), params());
    home(w);
    const Unit u = *drivenUnit(w, me);
    EXPECT_EQ(simOf(w, me).load(u, false), Rules::heroCarry);
    fitted(w, Module::cargoPod, 2);
    EXPECT_EQ(simOf(w, me).load(*drivenUnit(w, me), false), int64_t(std::llround(Rules::heroCarry * 1.16)));
    EXPECT_EQ(simOf(w, me).load(*drivenUnit(w, me), true), int64_t(std::llround(Rules::heroHydrogenCarry * 1.16)));
}

// The plate: hp +25% of the Prospector's own at Mk III, on top of Plating.
TEST(rig_plate_in_the_sim) {
    WorldMap w(first(), params());
    home(w, {{Skill::plating, 5}});
    const double base = Rules::hp(UnitKind::prospector);
    EXPECT_EQ(simOf(w, me).maxHP(*drivenUnit(w, me)), base + 30);
    fitted(w, Module::plate, 3);
    EXPECT_EQ(simOf(w, me).maxHP(*drivenUnit(w, me)), base + 30 + std::round(0.25 * base));
}

// The torch tip: torch damage +8% and range +0.25 cells at Mk I.
TEST(rig_torch_tip_in_the_sim) {
    WorldMap w(first(), params());
    home(w);
    const Boost damage = simOf(w, me).boost(Stat::Damage{}, *drivenUnit(w, me));
    const Boost range = simOf(w, me).boost(Stat::Range{}, *drivenUnit(w, me));
    fitted(w, Module::torchTip, 1);
    EXPECT_NEAR(simOf(w, me).boost(Stat::Damage{}, *drivenUnit(w, me)).percent - damage.percent, 0.08, 1e-12);
    EXPECT_NEAR(simOf(w, me).boost(Stat::Range{}, *drivenUnit(w, me)).plus - range.plus, 0.25, 1e-12);
}

// The survey lens: sight +16% and survey chips 16% more often at Mk II.
TEST(rig_survey_lens_in_the_sim) {
    WorldMap w(first(), params());
    home(w);
    const double sight = simOf(w, me).rate(Stat::Sight{}, *drivenUnit(w, me));
    EXPECT_NEAR(w.progress(me)->chipChance(), Progression::chipChance, 1e-12);
    fitted(w, Module::surveyLens, 2);
    EXPECT_NEAR(simOf(w, me).rate(Stat::Sight{}, *drivenUnit(w, me)), sight * 1.16, 1e-12);
    EXPECT_NEAR(w.progress(me)->chipChance(), Progression::chipChance * 1.16, 1e-12);
}

// The rig is saved: the world's save and back (fitted, stores, marks, both
// jobs), and a v6 file opens as v8 with no rig.
TEST(rig_save_round_trip) {
    WorldMap w(first(), params());
    const int64_t citadel = home(w);
    ASSERT_TRUE(put(w, me, StructureKind::foundry).has_value());
    ASSERT_TRUE(put(w, me, StructureKind::lab).has_value());
    // Back home to fit (C5: the 7x7 Foundry and the Lab went up farther off).
    besides(w, me, citadel);
    edit(w, me, [](Rig& r) {
        r.store(Module::torchTip, 1, 2);
        r.store(Module::surveyLens, 1);
        r.researched[Module::cargoPod] = 2;
    });
    fitted(w, Module::cargoPod, 2);
    fill(w, me, 5000, 5000);
    ASSERT_TRUE(!w.make(me, Module::plate, 1));
    ASSERT_TRUE(!w.research(me, Module::cargoPod, 3));
    const Rig rig = w.progress(me)->rig;
    const WorldSave save = w.save();
    bool written = false;
    for (const SavedPilot& p : save.pilots)
        if (p.id == key) {
            written = true;
            EXPECT_TRUE(SessionStore::decode<Rig>(p.rig) == std::optional<Rig>(rig));
        }
    EXPECT_TRUE(written);
    WorldMap back(first(), params());
    EXPECT_TRUE(back.restore(save).empty());
    ASSERT_TRUE(back.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(back.progress(me)->rig == rig);
    // In the sim again on its return: the cargo pod.
    EXPECT_EQ(simOf(back, me).load(*drivenUnit(back, me), false), int64_t(std::llround(Rules::heroCarry * 1.16)));

    // Through the file: written as v8, and a v6 pilot row reads with no rig.
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/rig.sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    std::string why;
    const std::vector<WorldStore::Migration> v6(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 6);
    {
        auto store = WorldStore::open(path, first().file.id, &why, v6);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(6));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, contracts) VALUES ('" +
                                    key + "', 10, 3, 'start', NULL, '', '', 1, '')",
                                &why));
    }
    {
        auto store = WorldStore::open(path, first().file.id, &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), WorldStore::version());
        const auto old = store->load(&why);
        ASSERT_TRUE(old.has_value() && old->pilots.size() == 1);
        EXPECT_TRUE(old->pilots[0].rig.empty());
        WorldMap fresh(first(), params());
        EXPECT_TRUE(fresh.restore(*old).empty());
        ASSERT_TRUE(fresh.seat(me, key) == WorldMap::Seated::restored);
        EXPECT_TRUE(fresh.progress(me)->rig == Rig{});
        // This world's rig written and read back.
        ASSERT_TRUE(store->write(save, &why).has_value());
        const auto again = store->load(&why);
        ASSERT_TRUE(again.has_value());
        WorldMap last(first(), params());
        EXPECT_TRUE(last.restore(*again).empty());
        ASSERT_TRUE(last.seat(me, key) == WorldMap::Seated::restored);
        EXPECT_TRUE(last.progress(me)->rig == rig);
    }
}

// Dev and shots (`--dev-rig`): a new pilot owns a finished Citadel, Foundry
// and Lab, stands where it unloads (it fits at once), and its rig is stocked
// with a job at each.
TEST(rig_dev_flag_gives_a_home_and_a_stocked_rig) {
    WorldParams p = params();
    p.devBlueprints = false;
    p.devRig = true;
    WorldMap w(first(), p);
    w.seat(me, key);
    w.step();
    EXPECT_TRUE(w.realmFinished(StructureKind::citadel));
    EXPECT_TRUE(w.realmFinished(StructureKind::foundry));
    EXPECT_TRUE(w.realmFinished(StructureKind::lab));
    // C5: the Foundry and the Lab beyond the Citadel, away from the pad the
    // Prospector stands by; it at the Citadel's edge.
    {
        const Simulation& sim = simOf(w, me);
        const Vec2 at = drivenUnit(w, me)->position;
        double citadel = 0, foundry = 0, lab = 0;
        for (const Structure& b : sim.state.structures) {
            const double d = distance(b.position, at);
            if (b.kind == StructureKind::citadel) {
                citadel = d;
                EXPECT_NEAR(d - Rules::radius(b.kind), 0.5, 0.75);
            }
            if (b.kind == StructureKind::foundry) foundry = d;
            if (b.kind == StructureKind::lab) lab = d;
        }
        EXPECT_TRUE(foundry > citadel && lab > citadel);
    }
    const auto m = w.progressMessage(me);
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->foundry && m->lab);
    EXPECT_TRUE(m->rig.making.has_value() && m->rig.research.has_value());
    EXPECT_EQ(m->rig.fittedMark(Module::drillHead), int64_t(2));
    EXPECT_EQ(m->rig.stock(Module::drillHead, 3), int64_t(1));
    EXPECT_TRUE(!w.fit(me, Module::drillHead, 3));
    EXPECT_EQ(w.progress(me)->rig.fittedMark(Module::drillHead), int64_t(3));
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 2), int64_t(1));
}
