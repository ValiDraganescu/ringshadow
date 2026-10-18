// The region and its snapshots (world/World.h). Isolated scenes: a few
// seconds at most, never a game played out.

#include "test.h"

#include "Rules.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 7;

/// A small open map; the AIs and the fog as asked.
RegionConfig scene(bool ai, bool fog) {
    RegionConfig c;
    c.map = MapChoice{MapStyle::openField, MapSize::small, 2};
    c.ai = ai;
    c.fog = fog;
    return c;
}

const Unit* find(const Simulation& sim, int64_t id) {
    const auto i = sim.unitIndex(id);
    return i ? &sim.state.units[static_cast<size_t>(*i)] : nullptr;
}

int64_t firstProspector(const Simulation& sim, int64_t owner) {
    for (const Unit& u : sim.state.units)
        if (u.owner == owner && u.kind == Unit::Kind::prospector) return u.id;
    return -1;
}

Vec2 citadelOf(const Simulation& sim, int64_t owner) {
    for (const Structure& s : sim.state.structures)
        if (s.owner == owner && s.kind == Structure::Kind::citadel) return s.position;
    return Vec2(0, 0);
}

/// A unit put down by hand, full health.
int64_t put(Simulation& sim, Unit::Kind kind, int64_t owner, Vec2 at, double heading = 0) {
    Unit u(sim.state.nextID, kind, owner, at, heading, Unit::Task::idle);
    sim.state.nextID += 1;
    u.hp = Rules::hp(kind);
    sim.state.units.push_back(u);
    sim.reindex();
    return u.id;
}

bool has(const Snapshot& s, int64_t id) {
    return std::any_of(s.entities.begin(), s.entities.end(), [&](const EntityState& e) { return e.id() == id; });
}

} // namespace

TEST(region_describes_its_map) {
    Region r(scene(false, true));
    const MapDefinition map = WindowMaps::build(MapChoice{MapStyle::openField, MapSize::small, 2});
    EXPECT_EQ(r.description().mapName, map.name);
    EXPECT_EQ(r.description().mapVersion, map.version);
    EXPECT_EQ(r.description().seed, map.seed);
    EXPECT_TRUE(r.description().map == (MapChoice{MapStyle::openField, MapSize::small, 2}));
    EXPECT_EQ(r.description().step, 1.0 / 60);
}

TEST(region_pilot_drives_like_the_sim) {
    // The AIs on: the region's step is the game's step, inputs and all.
    Region r(scene(true, true));
    ASSERT_TRUE(r.join(me));
    Simulation direct = r.simulation();
    const int64_t id = firstProspector(direct, 0);
    ASSERT_TRUE(id >= 0);
    const Vec2 start = find(direct, id)->position;
    const double heading = find(direct, id)->heading;
    ASSERT_TRUE(r.take(me, id));
    ASSERT_TRUE(direct.take(id));
    EXPECT_TRUE(r.driving(me) == std::optional<int64_t>(id));
    for (int64_t k = 1; k <= 120; ++k) {
        PilotInput in;
        in.tick = k;
        in.walk = Vec2(1, 0);
        in.heading = heading;
        EXPECT_TRUE(r.input(me, in));
        r.step();
        direct.pilot->walk = Vec2(1, 0);
        direct.pilot->heading = heading;
        direct.step(1.0 / 60);
    }
    const Unit* a = find(r.simulation(), id);
    const Unit* b = find(direct, id);
    ASSERT_TRUE(a && b);
    EXPECT_EQ(a->position.x, b->position.x);
    EXPECT_EQ(a->position.y, b->position.y);
    EXPECT_EQ(a->heading, b->heading);
    EXPECT_TRUE(distance(a->position, start) > 1.0);
    EXPECT_EQ(r.tick(), int64_t(120));
    EXPECT_EQ(r.simulation().state.time, direct.state.time);
}

TEST(region_hand_back_returns_to_ai) {
    Region r(scene(true, true));
    r.join(me);
    const int64_t id = firstProspector(r.simulation(), 0);
    ASSERT_TRUE(r.take(me, id));
    for (int64_t k = 1; k <= 30; ++k) {
        PilotInput in;
        in.tick = k;
        in.walk = Vec2(1, 0);
        in.heading = find(r.simulation(), id)->heading;
        r.input(me, in);
        r.step();
    }
    EXPECT_TRUE(r.simulation().piloted(id));
    r.handBack(me);
    EXPECT_TRUE(!r.driving(me));
    EXPECT_TRUE(!r.simulation().pilot);
    EXPECT_TRUE(!r.simulation().piloted(id));
    // Its AI puts it back to work.
    for (int k = 0; k < 180; ++k) r.step();
    const Unit* u = find(r.simulation(), id);
    ASSERT_TRUE(u);
    EXPECT_TRUE(u->task != Unit::Task::idle);
    // The inputs that keep coming no longer move it: nobody drives.
    EXPECT_TRUE(!r.simulation().pilot);
}

TEST(region_take_refuses_others_units_and_a_driven_one) {
    Region r(scene(false, true));
    r.join(me);
    r.join(me + 1);
    EXPECT_TRUE(!r.take(me, firstProspector(r.simulation(), 1)));
    EXPECT_TRUE(!r.take(99, firstProspector(r.simulation(), 0)));
    const int64_t mine = firstProspector(r.simulation(), 0);
    ASSERT_TRUE(r.take(me, mine));
    // W5: a second pilot drives a unit of its own, never one a human drives.
    EXPECT_TRUE(!r.take(me + 1, mine));
    const int64_t other = put(r.simulation(), Unit::Kind::ranger, 0, citadelOf(r.simulation(), 0) + Vec2(4, 4));
    EXPECT_TRUE(r.take(me + 1, other));
    EXPECT_TRUE(r.simulation().piloted(mine) && r.simulation().piloted(other));
    // Leaving hands the unit back; the other pilot drives on, now the active one.
    r.leave(me);
    EXPECT_TRUE(!r.simulation().piloted(mine));
    ASSERT_TRUE(r.simulation().pilot.has_value());
    EXPECT_EQ(r.simulation().pilot->unit, other);
    EXPECT_TRUE(r.simulation().drivers.empty());
    EXPECT_EQ(*r.driving(me + 1), other);
}

TEST(region_last_input_tick) {
    Region r(scene(false, true));
    r.join(me);
    const int64_t id = firstProspector(r.simulation(), 0);
    ASSERT_TRUE(r.take(me, id));
    const double heading = find(r.simulation(), id)->heading;
    auto at = [&](int64_t t) {
        PilotInput in;
        in.tick = t;
        in.walk = Vec2(1, 0);
        in.heading = heading;
        return in;
    };
    EXPECT_EQ(r.lastInputTick(me), int64_t(-1));
    for (int64_t t = 1; t <= 5; ++t) EXPECT_TRUE(r.input(me, at(t)));
    for (int k = 0; k < 3; ++k) r.step();
    EXPECT_EQ(r.lastInputTick(me), int64_t(3));
    EXPECT_EQ(r.snapshot(me).lastInputTick, int64_t(3));
    // Repeats and old ones are dropped.
    EXPECT_TRUE(!r.input(me, at(2)));
    EXPECT_TRUE(!r.input(me, at(3)));
    EXPECT_TRUE(!r.input(me, at(5)));
    // Out of order, applied in order, one a tick.
    EXPECT_TRUE(r.input(me, at(7)));
    EXPECT_TRUE(r.input(me, at(6)));
    r.step();
    EXPECT_EQ(r.lastInputTick(me), int64_t(4));
    r.step();
    r.step();
    EXPECT_EQ(r.lastInputTick(me), int64_t(6));
    r.step();
    EXPECT_EQ(r.lastInputTick(me), int64_t(7));
    // None waiting: the last one holds (it keeps walking), the count stays.
    const Vec2 before = find(r.simulation(), id)->position;
    r.step();
    EXPECT_EQ(r.lastInputTick(me), int64_t(7));
    EXPECT_EQ(r.snapshot(me).serverTick, int64_t(8));
    EXPECT_TRUE(distance(find(r.simulation(), id)->position, before) > 1e-4);
    // A client far ahead: past `maxBuffered` waiting, the oldest are used up.
    for (int64_t t = 10; t < 50; ++t) r.input(me, at(t));
    EXPECT_EQ(r.lastInputTick(me), int64_t(49 - static_cast<int64_t>(Region::maxBuffered)));
    r.step();
    EXPECT_EQ(r.lastInputTick(me), int64_t(50 - static_cast<int64_t>(Region::maxBuffered)));
}

TEST(region_input_is_clamped) {
    Region r(scene(false, true));
    r.join(me);
    const int64_t id = firstProspector(r.simulation(), 0);
    ASSERT_TRUE(r.take(me, id));
    PilotInput in;
    in.tick = 1;
    in.walk = Vec2(50, -std::numeric_limits<double>::infinity());
    in.heading = std::nan("");
    r.input(me, in);
    const double heading = find(r.simulation(), id)->heading;
    r.step();
    EXPECT_EQ(r.simulation().pilot->walk.x, 1.0);
    EXPECT_EQ(r.simulation().pilot->walk.y, 0.0);
    EXPECT_EQ(r.simulation().pilot->heading, heading);
}

TEST(snapshot_fog_never_shows_hidden_enemies) {
    Region r(scene(false, true));
    r.join(me);
    Simulation& sim = r.simulation();
    // One enemy across the map, out of sight; one at blue's door, in sight.
    const int64_t far = put(sim, Unit::Kind::ranger, 1, citadelOf(sim, 1) + Vec2(0, 4));
    const int64_t near = put(sim, Unit::Kind::ranger, 1, citadelOf(sim, 0) + Vec2(4, 0));
    for (int k = 0; k < 12; ++k) r.step();
    bool sawNear = false;
    for (int k = 0; k < 20; ++k) {
        const Snapshot s = r.snapshot(me);
        for (const EntityState& e : s.entities) {
            if (const Unit* u = e.unit()) EXPECT_TRUE(u->owner == 0 || r.simulation().sees(0, u->id));
            if (const Structure* b = e.structure(); b && b->owner != 0) EXPECT_TRUE(e.remembered || r.simulation().sees(0, b->id));
            EXPECT_TRUE(e.id() != far);
        }
        sawNear = sawNear || has(s, near);
        for (const PatchView& p : s.patches) EXPECT_TRUE(r.simulation().sees(0, p.patch.position));
        ASSERT_TRUE(s.fog != nullptr);
        EXPECT_EQ(s.fog->seen.size(), r.simulation().sights[0].seen.size());
        // The other side's money is not told.
        EXPECT_EQ(s.players[1].ore, Player().ore);
        r.step();
    }
    EXPECT_TRUE(sawNear);
    // Its death far away is not told either; ours is.
    EXPECT_TRUE(!mayKnow(r.simulation(), 0, GameEvent::Died{far, Unit::Kind::ranger, 1, citadelOf(sim, 1)}));
    EXPECT_TRUE(mayKnow(r.simulation(), 0, GameEvent::Died{123456, Unit::Kind::ranger, 0, citadelOf(sim, 1)}));
}

// docs/perf.md "Server" round 13: the side's fog is made once a look and
// shared by its snapshots. Two snapshots between looks share one; one held
// across the next look still reads the fog it had, byte for byte, while the
// next is the fog the look left.
TEST(snapshot_fog_held_across_a_look_keeps_its_fog) {
    Region r(scene(false, true));
    r.join(me);
    Simulation& sim = r.simulation();
    const int64_t id = firstProspector(sim, 0);
    ASSERT_TRUE(r.take(me, id));
    for (int k = 0; k < 12; ++k) r.step();
    const Snapshot held = r.snapshot(me);
    ASSERT_TRUE(held.fog != nullptr);
    const Snapshot twin = r.snapshot(me);
    EXPECT_TRUE(twin.fog.get() == held.fog.get());
    const std::vector<uint8_t> seen = held.fog->seen, near = held.fog->near;
    const std::vector<uint64_t> explored = held.fog->explored;
    // The unit carried across the map, and a look there.
    Unit* u = &sim.state.units[*sim.unitIndex(id)];
    u->position = citadelOf(sim, 1) + Vec2(-6, 0);
    const uint64_t look = sim.lookId;
    for (int k = 0; k < 12 && sim.lookId == look; ++k) r.step();
    ASSERT_TRUE(sim.lookId != look);
    const Snapshot next = r.snapshot(me);
    ASSERT_TRUE(next.fog != nullptr);
    EXPECT_TRUE(next.fog.get() != held.fog.get());
    EXPECT_TRUE(held.fog->seen == seen && held.fog->near == near && held.fog->explored == explored);
    EXPECT_TRUE(next.fog->seen == sim.sights[0].seen && next.fog->near == sim.sights[0].near);
    EXPECT_TRUE(next.fog->explored == (*sim.state.intel)[0].explored);
    EXPECT_TRUE(next.fog->seen != seen);
}

TEST(snapshot_enemy_plans_left_out) {
    Region r(scene(false, false));
    r.join(me);
    Simulation& sim = r.simulation();
    const int64_t foe = put(sim, Unit::Kind::ranger, 1, citadelOf(sim, 0) + Vec2(6, 0));
    sim.state.units[static_cast<size_t>(*sim.unitIndex(foe))].goal = Vec2(1, 2);
    sim.state.units[static_cast<size_t>(*sim.unitIndex(foe))].mission = Mission(Mission::Raid{Vec2(3, 4)});
    const Snapshot s = r.snapshot(me);
    bool found = false;
    for (const EntityState& e : s.entities) {
        if (e.id() != foe) continue;
        found = true;
        EXPECT_TRUE(!e.unit()->goal);
        EXPECT_TRUE(!e.unit()->mission);
    }
    EXPECT_TRUE(found);
}

TEST(snapshot_priority_order) {
    Region r(scene(false, false));
    r.join(me);
    Simulation& sim = r.simulation();
    const Vec2 home = citadelOf(sim, 0) + Vec2(8, 0);
    const int64_t driven = put(sim, Unit::Kind::ranger, 0, home);
    const int64_t shooter = put(sim, Unit::Kind::ranger, 1, home + Vec2(9, 0));
    const int64_t mark = put(sim, Unit::Kind::ranger, 1, home + Vec2(0, 7));
    put(sim, Unit::Kind::ranger, 0, home + Vec2(1, 1));
    put(sim, Unit::Kind::ranger, 1, home + Vec2(2, -1));
    ASSERT_TRUE(r.take(me, driven));
    sim.state.units[static_cast<size_t>(*sim.unitIndex(shooter))].target = driven;
    sim.state.units[static_cast<size_t>(*sim.unitIndex(driven))].target = mark;
    const Snapshot s = r.snapshot(me);
    ASSERT_TRUE(s.entities.size() >= 5);
    EXPECT_EQ(s.entities[0].id(), driven);
    EXPECT_TRUE(s.entities[0].rank == Rank::driven);
    // What it fights, nearer first, then the rest by distance.
    EXPECT_EQ(s.entities[1].id(), mark);
    EXPECT_EQ(s.entities[2].id(), shooter);
    EXPECT_TRUE(s.entities[1].rank == Rank::engaged && s.entities[2].rank == Rank::engaged);
    for (size_t i = 3; i < s.entities.size(); ++i) {
        EXPECT_TRUE(s.entities[i].rank == Rank::nearby);
        if (i > 3) EXPECT_TRUE(s.entities[i - 1].distance <= s.entities[i].distance);
    }
    EXPECT_TRUE(s.driven == std::optional<int64_t>(driven));
}

TEST(snapshot_far_things_less_often) {
    Region r(scene(false, false));
    r.join(me);
    Simulation& sim = r.simulation();
    const Vec2 home = citadelOf(sim, 0);
    const int64_t driven = put(sim, Unit::Kind::ranger, 0, home + Vec2(6, 0));
    ASSERT_TRUE(r.take(me, driven));
    const Vec2 at = home + Vec2(6, 0);
    // Toward the map's middle, where there is ground.
    const Vec2 way = normalize(citadelOf(sim, 1) - home);
    const int64_t near = put(sim, Unit::Kind::ranger, 0, at + way * 10);
    const int64_t mid = put(sim, Unit::Kind::ranger, 0, at + way * 30);
    const int64_t far = put(sim, Unit::Kind::ranger, 0, at + way * 60);
    InterestParams p;
    p.nearRadius = 24;
    p.midRadius = 48;
    p.midEvery = 2;
    p.farEvery = 4;
    p.maxEntities = 1000;
    std::map<int64_t, int> sent;
    for (int k = 0; k < 16; ++k) {
        const Snapshot s = r.snapshot(me, p);
        for (int64_t id : {driven, near, mid, far}) sent[id] += has(s, id) ? 1 : 0;
    }
    EXPECT_EQ(sent[driven], 16);
    EXPECT_EQ(sent[near], 16);
    EXPECT_EQ(sent[mid], 8);
    EXPECT_EQ(sent[far], 4);
}

TEST(snapshot_budget_starves_none) {
    Region r(scene(false, false));
    r.join(me);
    Simulation& sim = r.simulation();
    const Vec2 home = citadelOf(sim, 0);
    const int64_t driven = put(sim, Unit::Kind::ranger, 0, home + Vec2(6, 0));
    ASSERT_TRUE(r.take(me, driven));
    const Vec2 way = normalize(citadelOf(sim, 1) - home);
    std::vector<int64_t> crowd;
    for (int i = 0; i < 40; ++i) crowd.push_back(put(sim, Unit::Kind::ranger, 0, home + Vec2(6, 0) + way * (2 + i * 0.5)));
    InterestParams p;
    p.nearRadius = 1000;  // all due every time: only the budget decides
    p.maxEntities = 6;
    std::map<int64_t, int> last;
    int worst = 0;
    const int rounds = 60;
    for (int k = 0; k < rounds; ++k) {
        const Snapshot s = r.snapshot(me, p);
        EXPECT_EQ(static_cast<int64_t>(s.entities.size()), p.maxEntities);
        EXPECT_EQ(s.entities[0].id(), driven);
        for (const EntityState& e : s.entities) {
            const auto it = last.find(e.id());
            const int since = it == last.end() ? k + 1 : k - it->second;
            worst = std::max(worst, since);
            last[e.id()] = k;
        }
    }
    // Every one of the crowd went out, none waited long, and the nearest
    // still go out more often than the farthest.
    for (int64_t id : crowd) EXPECT_TRUE(last.count(id) > 0);
    const int total = static_cast<int>(sim.state.units.size() + sim.state.structures.size());
    EXPECT_TRUE(worst <= total);
    // A second pilot on the same scene, counting how often each goes out.
    r.join(me + 1);
    std::map<int64_t, int> count;
    for (int k = 0; k < rounds; ++k)
        for (const EntityState& e : r.snapshot(me + 1, p).entities) count[e.id()] += 1;
    EXPECT_TRUE(count[crowd.front()] > count[crowd.back()]);
    EXPECT_TRUE(count[crowd.back()] > 0);
}

TEST(snapshot_tells_what_is_gone) {
    Region r(scene(false, false));
    r.join(me);
    Simulation& sim = r.simulation();
    const int64_t id = put(sim, Unit::Kind::ranger, 0, citadelOf(sim, 0) + Vec2(6, 0));
    EXPECT_TRUE(has(r.snapshot(me), id));
    sim.state.units.erase(sim.state.units.begin() + *sim.unitIndex(id));
    sim.reindex();
    const Snapshot s = r.snapshot(me);
    EXPECT_TRUE(!has(s, id));
    EXPECT_TRUE(std::find(s.removed.begin(), s.removed.end(), id) != s.removed.end());
    EXPECT_TRUE(r.snapshot(me).removed.empty());
}

TEST(snapshot_events_since_the_last) {
    Region r(scene(false, true));
    r.join(me);
    Simulation& sim = r.simulation();
    // A Ranger of blue's next to a red one at blue's door: they fight.
    const Vec2 door = citadelOf(sim, 0) + Vec2(6, 0);
    put(sim, Unit::Kind::ranger, 0, door);
    put(sim, Unit::Kind::ranger, 1, door + Vec2(3, 0));
    for (int k = 0; k < 120; ++k) r.step();
    const Snapshot s = r.snapshot(me);
    const bool shots = std::any_of(s.events.begin(), s.events.end(), [](const GameEvent& e) { return e.is<GameEvent::Shot>(); });
    EXPECT_TRUE(shots);
    EXPECT_TRUE(r.snapshot(me).events.empty());
}
