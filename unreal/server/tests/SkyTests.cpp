// The world's clock and weather (docs/mmorpg-m6.md, C1): one clock per
// world, the same hour in every region, sight closing in at the world's
// night, the dev holds, the clock saved and resumed, and the sky in the
// snapshots a client gets. Isolated scenes of a few seconds; run from
// unreal/server (as `make test` does).

#include "test.h"

#include "HeadlessClient.h"
#include "Server.h"
#include "WorldMap.h"
#include "WorldStore.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 7;

const LoadedWorld& firstWorld() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Cells side 0 sees in the pilot's region now.
size_t seenCells(const WorldMap& w, PilotId p) {
    const auto r = w.where(p);
    const auto sight = w.region(*r).simulation().sight(0);
    size_t n = 0;
    if (sight)
        for (uint8_t c : sight->seen) n += c ? 1 : 0;
    return n;
}

} // namespace

TEST(sky_world_clock_runs_a_tick_a_step_from_the_morning) {
    WorldMap w(firstWorld());
    EXPECT_EQ(w.clock(), 0.0);
    EXPECT_NEAR(w.hour(), WorldClock::startHour, 1e-9);
    const double step = w.region(0).description().step;
    for (int i = 0; i < 600; i++) w.step();
    EXPECT_NEAR(w.clock(), 600 * step, 1e-6);
    EXPECT_NEAR(w.hour(), WorldClock::hourAt(600 * step), 1e-9);
    EXPECT_TRUE(w.hour() > WorldClock::startHour);
    // The sky says the same, with the world's day and seed.
    const auto sky = w.sky();
    ASSERT_TRUE(sky.has_value());
    EXPECT_NEAR(sky->time, w.clock(), 1e-12);
    EXPECT_EQ(sky->dayLength, WorldClock::dayLength);
    EXPECT_EQ(sky->weatherSeed, WorldClock::weatherSeed(firstWorld().file.id));
    EXPECT_TRUE(!sky->heldHour.has_value());
}

TEST(sky_every_region_is_at_the_worlds_hour) {
    WorldMap w(firstWorld());
    ASSERT_TRUE(w.regionCount() > 1);
    w.setClock(WorldClock::timeOf(21.0));
    for (int i = 0; i < 30; i++) w.step();
    for (size_t r = 0; r < w.regionCount(); r++) {
        const GameState& st = w.region(r).simulation().state;
        ASSERT_TRUE(st.heldHour.has_value());
        EXPECT_NEAR(*st.heldHour, w.hour(), 1e-12);
        // Its own clock does not decide the hour: the core reads the held one.
        EXPECT_NEAR(Rules::hourAt(st.time, st.dayLength, st.heldHour), w.hour(), 1e-12);
    }
}

TEST(sky_night_closes_a_regions_sight_at_the_worlds_night) {
    WorldMap w(firstWorld());
    ASSERT_TRUE(w.join(me));
    w.setClock(WorldClock::timeOf(12.0));
    for (int i = 0; i < 10; i++) w.step();
    const size_t day = seenCells(w, me);
    EXPECT_NEAR(Rules::sightScale(w.hour()), 1.0, 1e-9);
    // The world's night (not the region's own clock): the same spot sees
    // much less, the headlamp's beam aside.
    w.setClock(WorldClock::timeOf(1.0, w.clock()));
    for (int i = 0; i < 10; i++) w.step();
    const size_t night = seenCells(w, me);
    EXPECT_NEAR(Rules::sightScale(w.hour()), Rules::nightSight, 1e-6);
    EXPECT_TRUE(day > 0);
    EXPECT_TRUE(night < day * 6 / 10);
    // Morning again: the sight comes back.
    w.setClock(WorldClock::timeOf(12.0, w.clock()));
    for (int i = 0; i < 10; i++) w.step();
    EXPECT_EQ(seenCells(w, me), day);
}

TEST(sky_held_hour_and_weather) {
    WorldParams p;
    p.heldHour = 22.0;
    p.weatherHold = 3.0;
    WorldMap w(firstWorld(), p);
    for (int i = 0; i < 120; i++) w.step();
    EXPECT_NEAR(w.hour(), 22.0, 1e-12);
    EXPECT_TRUE(w.clock() > 1.9);
    EXPECT_TRUE(w.weather().state == WeatherState::storm || w.weather().storm > 0.99);
    EXPECT_NEAR(w.weather().rain, 1.0, 1e-9);
    const auto sky = w.sky();
    ASSERT_TRUE(sky.has_value());
    EXPECT_NEAR(*sky->heldHour, 22.0, 1e-12);
    EXPECT_NEAR(*sky->weatherHold, 3.0, 1e-12);
    // A client sampling the same seed, time and hold has the same weather.
    Weather mine(sky->weatherSeed);
    mine.hold(*sky->weatherHold);
    const WeatherSample s = mine.sample(sky->time);
    EXPECT_NEAR(s.rain, w.weather().rain, 1e-12);
    EXPECT_NEAR(s.cover, w.weather().cover, 1e-12);
    EXPECT_NEAR(s.windKm, w.weather().windKm, 1e-12);
}

TEST(sky_weather_follows_the_schedule_on_the_worlds_clock) {
    WorldMap w(firstWorld());
    Weather same(WorldClock::weatherSeed(firstWorld().file.id));
    // The first spell is clear; later the schedule turns, the same for the
    // server and anyone with the seed.
    EXPECT_TRUE(w.weather().state == WeatherState::clear);
    for (double t : {0.0, 400.0, 1500.0, 5000.0}) {
        w.setClock(t);
        EXPECT_TRUE(w.sky()->weather == same.sample(t).state);
        EXPECT_NEAR(w.weather().cover, same.sample(t).cover, 1e-12);
    }
}

TEST(sky_clock_saved_and_resumed) {
    WorldMap w(firstWorld());
    w.setClock(WorldClock::timeOf(19.0));
    for (int i = 0; i < 60; i++) w.step();
    const double clock = w.clock();
    const double hour = w.hour();
    const WorldSave saved = w.save();
    EXPECT_NEAR(saved.clock, clock, 1e-12);
    // Through the file.
    mkdir("build/sky-tests", 0755);
    const std::string path = "build/sky-tests/clock.sqlite";
    unlink(path.c_str());
    unlink((path + "-wal").c_str());
    unlink((path + "-shm").c_str());
    std::string why;
    WorldSave back;
    {
        auto store = WorldStore::open(path, firstWorld().file.id, &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(saved, &why).has_value());
    }
    {
        auto store = WorldStore::open(path, firstWorld().file.id, &why);
        ASSERT_TRUE(store != nullptr);
        const auto loaded = store->load(&why);
        ASSERT_TRUE(loaded.has_value());
        back = *loaded;
    }
    EXPECT_NEAR(back.clock, clock, 1e-9);
    // A new server on the save goes on at that hour, and on from there.
    WorldMap again(firstWorld());
    EXPECT_TRUE(again.restore(back).empty());
    EXPECT_NEAR(again.clock(), clock, 1e-9);
    EXPECT_NEAR(again.hour(), hour, 1e-9);
    for (size_t r = 0; r < again.regionCount(); r++) EXPECT_NEAR(*again.region(r).simulation().state.heldHour, hour, 1e-9);
    again.step();
    EXPECT_TRUE(again.clock() > clock);
}

TEST(sky_one_generated_region_has_none) {
    // M1's one region (`--map`) keeps its own clock: no sky in its snapshots.
    RegionConfig c;
    c.map = MapChoice{MapStyle::openField, MapSize::small, 2};
    WorldMap w(c);
    EXPECT_TRUE(!w.sky().has_value());
    ASSERT_TRUE(w.join(me));
    w.step();
    EXPECT_TRUE(!w.snapshot(me).sky.has_value());
    EXPECT_TRUE(!w.region(0).simulation().state.heldHour.has_value());
}

TEST(sky_snapshots_bring_the_hour_and_the_weather_to_the_client) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = firstWorld();
    o.worldParams.weatherHold = 2.0;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    server.world().setClock(WorldClock::timeOf(20.5));
    HeadlessClient client(clientPort, nettest::clientConfig());
    double now = 100;
    client.connect(now);
    client.join(*net::pilotKeyFromHex("00112233445566778899aabbccddeeff"));
    bool ok = false;
    for (int i = 0; i < 10 * 60 && !ok; i++) {
        now += 1.0 / 60;
        server.tick(now);
        client.tick(now);
        ok = client.mirror() && client.mirror()->sky().has_value() && client.mirror()->driven();
    }
    ASSERT_TRUE(ok);
    const net::Sky sky = *client.mirror()->sky();
    // A snapshot or two behind the server, never ahead.
    EXPECT_TRUE(sky.time <= server.world().clock());
    EXPECT_TRUE(server.world().clock() - sky.time < 0.2);
    EXPECT_NEAR(sky.hour(), WorldClock::hourAt(sky.time), 1e-9);
    EXPECT_TRUE(sky.hour() > 20.4 && sky.hour() < 21.0);
    EXPECT_EQ(sky.weatherSeed, WorldClock::weatherSeed(firstWorld().file.id));
    ASSERT_TRUE(sky.weatherHold.has_value());
    EXPECT_NEAR(*sky.weatherHold, 2.0, 1e-9);
    EXPECT_TRUE(sky.weather == WeatherState::rain);
    // The mirror's sight at night is the server's hour.
    EXPECT_NEAR(*client.mirror()->simulation().state.heldHour, sky.hour(), 1e-9);
}
