// The strategy game's leveling on the server (docs/mmorpg-m3.md, S3;
// docs/leveling.md): off a world (the M1 region, `--map`) the pilot's levels
// and picks are its own record (`Region::levels`, `WorldMap::levels`), in
// the simulation only while it drives; a Pick applies on the server, an
// invalid one is refused with its reason. In a world the cards are off: its
// Prospector climbs the skill tree (ProgressTests.cpp). Isolated scenes of a
// few seconds; run from unreal/server.

#include "test.h"

#include "Leveling.h"
#include "Rules.h"
#include "WorldMap.h"

#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 11;
constexpr PilotId other = 12;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// A Prospector record at `level` (its XP exactly there) with `picks`.
PilotRecord record(int64_t level, const std::vector<Perk>& picks) {
    PilotRecord r;
    KindRecord& k = r.kinds[UnitKind::prospector];
    k.xp = double(Leveling::xp(level));
    k.picks = picks;
    return r;
}

/// The M1 region with no AI: the pilots' side only.
RegionConfig quiet() {
    RegionConfig c;
    c.map = MapChoice{MapStyle::openField, MapSize::small, 2};
    c.ai = false;
    return c;
}

/// A Prospector of the pilots' side.
std::vector<int64_t> prospectors(const Region& r) {
    std::vector<int64_t> out;
    for (const Unit& u : r.simulation().state.units)
        if (u.owner == Pilot::player && u.kind == UnitKind::prospector) out.push_back(u.id);
    return out;
}

} // namespace

// Off a world: a pilot at level 4 picks lowest level first, the server
// refuses what is not on offer with its reason, and Big haul shows (the
// driven Prospector's load is 35 ore). Each pilot's record is its own.
TEST(levels_m1_picks_apply_on_the_server) {
    WorldMap w(quiet());
    ASSERT_TRUE(w.join(me));
    ASSERT_TRUE(w.join(other));
    const auto ours = prospectors(w.start());
    ASSERT_TRUE(!ours.empty());
    ASSERT_TRUE(w.start().take(me, ours[0]));
    w.setLevels(me, record(4, {}));
    EXPECT_EQ(w.start().simulation().levels().level(UnitKind::prospector), int64_t(4));
    // Refused: level 4's card while level 2's waits, another kind's card,
    // a card named for the wrong kind.
    EXPECT_TRUE(w.pick(me, UnitKind::prospector, Perk::prospectorBigHaul) == std::optional<std::string>("Not on offer"));
    EXPECT_TRUE(w.pick(me, UnitKind::ranger, Perk::rangerDrillSergeant) == std::optional<std::string>("No pick waiting"));
    EXPECT_TRUE(w.pick(me, UnitKind::ranger, Perk::prospectorQuickDrill) == std::optional<std::string>("Not a card of that kind"));
    EXPECT_TRUE(w.levels(me)->picks(UnitKind::prospector).empty());
    // Kept, lowest level first.
    EXPECT_TRUE(!w.pick(me, UnitKind::prospector, Perk::prospectorQuickDrill));
    EXPECT_TRUE(!w.pick(me, UnitKind::prospector, Perk::prospectorRigMaster));
    ASSERT_TRUE(w.start().simulation().state.unit(ours[0]).has_value());
    EXPECT_EQ(w.start().simulation().load(*w.start().simulation().state.unit(ours[0]), false), int64_t(25));
    EXPECT_TRUE(!w.pick(me, UnitKind::prospector, Perk::prospectorBigHaul));
    EXPECT_EQ(w.start().simulation().load(*w.start().simulation().state.unit(ours[0]), false), int64_t(35));
    const std::vector<Perk> three{Perk::prospectorQuickDrill, Perk::prospectorRigMaster, Perk::prospectorBigHaul};
    EXPECT_TRUE(w.levels(me)->picks(UnitKind::prospector) == three);
    EXPECT_TRUE(w.pick(me, UnitKind::prospector, Perk::prospectorFastWelder) == std::optional<std::string>("No pick waiting"));
    // The other pilot, seated and not driving, has none of them.
    EXPECT_TRUE(!w.levels(other).has_value());
    EXPECT_TRUE(w.pick(other, UnitKind::prospector, Perk::prospectorQuickDrill) == std::optional<std::string>("No pick waiting"));
    // No skill tree off a world.
    EXPECT_TRUE(!w.progress(me).has_value());
    EXPECT_TRUE(w.spend(me, Skill::quickDrill) == std::optional<std::string>("Not in a world"));
}

// In a world the cards are off: a Pick is refused, the strategy game's
// leveling earns nothing (`kWorldXpScale`), so no level-up and no cards.
TEST(levels_world_has_no_cards) {
    WorldMap w(first());
    w.join(me);
    Simulation& sim = w.start().simulation();
    EXPECT_EQ(sim.xpScale, kWorldXpScale);
    const auto id = w.start().driving(me);
    ASSERT_TRUE(id.has_value());
    sim.earn(5000, *sim.state.unit(*id));
    w.step();
    EXPECT_TRUE(!w.levels(me) || w.levels(me)->level(UnitKind::prospector) == 1);
    for (const GameEvent& e : w.snapshot(me).events) EXPECT_TRUE(!e.is<GameEvent::LeveledUp>());
    EXPECT_TRUE(w.pick(me, UnitKind::prospector, Perk::prospectorQuickDrill) == std::optional<std::string>(kWorldPickRefusal));
}
