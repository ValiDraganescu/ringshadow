// Repairs cost materials (docs/balance.md "Repairs"): mending a building's
// whole bar costs `Rules::repairShare` of its build price, ore and MH, paid
// in whole units as the debt crosses them, from the repairer's bank.
// Isolated scenes of one building and a Prospector or two.
#include "test.h"
#include "SimHelpers.h"

#include "Simulation.h"

#include <cmath>

namespace {

using namespace ac;
using namespace simtest;

struct Mend {
    Simulation sim;
    int64_t building;
    int64_t worker;
};

/// Blue alone with `ore` and `hydrogen`, a finished `kind` at half its bar,
/// a Prospector beside it.
Mend halfBar(Structure::Kind kind, int64_t ore, int64_t hydrogen) {
    GameState s = bare(1, 0, {}, {}, 1);
    s.players[0].ore = ore;
    s.players[0].hydrogen = hydrogen;
    const Vec2 at(40, 40);
    const int64_t b = addStructure(s, kind, at, 0);
    s.structures.back().hp = Rules::hp(kind) / 2;
    const int64_t w = addUnit(s, Unit::Kind::prospector, at + Vec2(Rules::radius(kind) + 0.6, 0), 0);
    return Mend{Simulation(s, std::nullopt), b, w};
}

const Structure& building(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures) if (s.id == id) return s;
    static const Structure none;
    return none;
}

/// The ore and MH the run's Repaired events paid for `id`.
std::pair<int64_t, int64_t> paid(const std::vector<GameEvent>& events, int64_t id) {
    std::pair<int64_t, int64_t> sum{0, 0};
    for (const GameEvent& e : events)
        if (const auto r = e.as<GameEvent::Repaired>(); r && r->structure == id) {
            sum.first += r->ore;
            sum.second += r->hydrogen;
        }
    return sum;
}

} // namespace

/// Half the bar mended costs a quarter of the build price, exactly: what
/// was paid plus what is still owed under one unit. A Citadel (400 ore:
/// 100 paid, nothing owed) and a Foundry (150 ore, 100 MH: 37 ore paid and
/// 0.5 owed, 25 MH paid).
TEST(repair_costsHalfTheBuildPricePerFullBar) {
    for (const Structure::Kind kind : {Structure::Kind::citadel, Structure::Kind::foundry}) {
        Mend m = halfBar(kind, 10000, 10000);
        ASSERT_TRUE(m.sim.issue(Command::Repair{m.worker, m.building}));
        const auto events = run(m.sim, Rules::buildTime(kind) * 0.5 / Rules::heroWork + 30);
        const Structure& s = building(m.sim, m.building);
        EXPECT_EQ(s.hp, Rules::hp(kind));
        const double ore = 0.25 * double(Rules::cost(kind)), hydrogen = 0.25 * double(Rules::hydrogenCost(kind));
        const int64_t oreSpent = 10000 - m.sim.state.players[0].ore, hydrogenSpent = 10000 - m.sim.state.players[0].hydrogen;
        EXPECT_EQ(oreSpent, int64_t(std::floor(ore)));
        EXPECT_EQ(hydrogenSpent, int64_t(std::floor(hydrogen)));
        EXPECT_TRUE(std::abs(double(oreSpent) + s.repairOwed.ore - ore) < 1e-9);
        EXPECT_TRUE(std::abs(double(hydrogenSpent) + s.repairOwed.hydrogen - hydrogen) < 1e-9);
        // Every unit paid was announced, and none twice.
        EXPECT_TRUE(paid(events, m.building) == std::make_pair(oreSpent, hydrogenSpent));
    }
    EXPECT_EQ(Simulation::repairCost(Structure::Kind::citadel, Rules::hp(Structure::Kind::citadel) / 2, false).first, 100.0);
}

/// The side short of the next unit: the repair stops where the money ran
/// out, the Prospector leaves it, and a new order is refused with the
/// reason, as a build is.
TEST(repair_stopsWhenTheSideCannotPay) {
    // A Citadel needs 100 ore for its half bar; blue has 3.
    Mend m = halfBar(Structure::Kind::citadel, 3, 0);
    ASSERT_TRUE(m.sim.issue(Command::Repair{m.worker, m.building}));
    const auto events = run(m.sim, 60);
    const Structure& s = building(m.sim, m.building);
    const double full = Rules::hp(Structure::Kind::citadel);
    EXPECT_EQ(m.sim.state.players[0].ore, int64_t(0));
    EXPECT_TRUE(paid(events, m.building) == std::make_pair(int64_t(3), int64_t(0)));
    // Mended as far as 3 ore (and the fourth's start) go: 3 / 400 / 0.5 of the bar.
    const double reach = full / 2 + 4.0 / 400 / Rules::repairShare * full;
    EXPECT_TRUE(s.hp > full / 2 + 3.0 / 400 / Rules::repairShare * full - 1e-6);
    EXPECT_TRUE(s.hp < reach);
    EXPECT_TRUE(m.sim.state.unit(m.worker)->task != Unit::Task::repairing);
    const auto why = m.sim.repairRefusal(*m.sim.state.unit(m.worker), s);
    EXPECT_TRUE(why == std::optional<std::string>("No ore to repair"));
    EXPECT_TRUE(!m.sim.issue(Command::Repair{m.worker, m.building}));
    // MH short likewise.
    Mend f = halfBar(Structure::Kind::foundry, 1000, 0);
    EXPECT_TRUE(f.sim.repairRefusal(*f.sim.state.unit(f.worker), building(f.sim, f.building))
                == std::optional<std::string>("No MH to repair"));
    EXPECT_TRUE(!f.sim.issue(Command::Repair{f.worker, f.building}));
}

/// Two humans driving: the second pilot mending the realm's Citadel pays
/// from its own bank, not the first's (whose bank is the side's while it
/// is the active one), whoever owns the building.
TEST(repair_paidByTheRepairersBank) {
    GameState s = bare(1, 0, {}, {}, 1);
    const Vec2 at(40, 40);
    const int64_t citadel = addStructure(s, Structure::Kind::citadel, at, 0);
    s.structures.back().hp = Rules::hp(Structure::Kind::citadel) / 2;
    const double reach = Rules::radius(Structure::Kind::citadel) + 0.6;
    const int64_t first = addUnit(s, Unit::Kind::prospector, at + Vec2(-12, 0), 0);
    const int64_t second = addUnit(s, Unit::Kind::prospector, at + Vec2(reach, 0), 0, Unit::Task::idle, pi);
    Simulation sim(s, std::nullopt);
    ASSERT_TRUE(sim.drive(first));
    ASSERT_TRUE(sim.drive(second));
    sim.setBankFor(first, 1000, 0);
    sim.setBankFor(second, 500, 0);
    ASSERT_TRUE(sim.pilotFor(second) != nullptr);
    Pilot& p = *sim.pilotFor(second);
    p.heading = pi;  // at the Citadel, west of it
    p.mend = true;
    p.hold = true;
    const auto events = run(sim, 10);
    const int64_t spent = 500 - sim.bankFor(second).first;
    EXPECT_TRUE(spent > 0);
    EXPECT_TRUE(paid(events, citadel) == std::make_pair(spent, int64_t(0)));
    for (const GameEvent& e : events)
        if (const auto r = e.as<GameEvent::Repaired>()) EXPECT_EQ(r->by, second);
    EXPECT_EQ(sim.bankFor(first).first, int64_t(1000));
    EXPECT_TRUE(building(sim, citadel).hp > Rules::hp(Structure::Kind::citadel) / 2);
}

/// A world's region (the server's sim, `xpScale` < 1): a repairer no human
/// drives (a frontier pilot's Prospector) never pays from the side's bank,
/// which is a human's; the server's `repairPayer` charges it, and its "no"
/// stops the repair. Without one the mend is free.
TEST(repair_aWorldsOtherRepairersPayThroughTheServer) {
    Mend m = halfBar(Structure::Kind::citadel, 50, 0);
    m.sim.xpScale = 0;
    ASSERT_TRUE(m.sim.issue(Command::Repair{m.worker, m.building}));
    run(m.sim, 5);
    EXPECT_EQ(m.sim.state.players[0].ore, int64_t(50));
    EXPECT_TRUE(building(m.sim, m.building).hp > Rules::hp(Structure::Kind::citadel) / 2);

    Mend w = halfBar(Structure::Kind::citadel, 50, 0);
    w.sim.xpScale = 0;
    int64_t charged = 0;
    w.sim.repairPayer = [&](const Unit& by, const Structure&, int64_t ore, int64_t) {
        if (by.id != w.worker || charged + ore > 2) return false;
        charged += ore;
        return true;
    };
    ASSERT_TRUE(w.sim.issue(Command::Repair{w.worker, w.building}));
    run(w.sim, 60);
    EXPECT_EQ(charged, int64_t(2));
    EXPECT_EQ(w.sim.state.players[0].ore, int64_t(50));
    EXPECT_TRUE(w.sim.state.unit(w.worker)->task != Unit::Task::repairing);
    // Stopped short of the full bar where the payer said no.
    EXPECT_TRUE(building(w.sim, w.building).hp < Rules::hp(Structure::Kind::citadel));
}
