// The world map's pads after a save and a load (the user, 2026-10-15: "I
// can't pick a landing on the map"): a pilot back at the start region sees,
// in its World message, the Landing Pad it built in a neighbour region it has
// been to (a `building` pad), the one a frontier builder put up at Prospect
// Landing too, and its regions seen still hold that neighbour; the map's rule
// (atlas::padPick) picks the neighbour's pad and refuses the region file's
// own pads with their lines. Run from unreal/server.

#include "test.h"

#include "WorldAtlas.h"
#include "WorldMap.h"

#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 51;
const std::string myKey = "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] { return *loadWorld("worlds/first.json").world; }();
    return w;
}

WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.brood = false;
    p.firstStep = 0;
    p.frontier = 5;
    return p;
}

} // namespace

TEST(MapPick_padsAndSeenSurviveASaveAndAFrontierPad) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(me, myKey) == WorldMap::Seated::fresh);
    w.step();
    const size_t start = *w.regionNamed("start"), rust = *w.regionNamed("rust-hollow");
    // To Rust Hollow and a Landing Pad there, back to the start.
    ASSERT_TRUE(w.devTransfer(me, rust, w.region(rust).places()->ground(Vec2(72, 48))));
    w.step();
    w.firstVisit(me, "rust-hollow");
    ASSERT_TRUE(w.devLandingPad(rust, w.region(rust).places()->ground(Vec2(60, 40))).has_value());
    ASSERT_TRUE(w.devTransfer(me, start, w.region(start).places()->pads[0].at + Vec2(6, 0)));
    w.step();
    // A frontier builder's pad at Prospect Landing.
    w.frontierRoster();
    WorldMap::FrontierPilot* b = nullptr;
    for (auto& f : w.frontier())
        if (f.habit == WorldFrontier::Habit::builder && !b) b = &f;
    ASSERT_TRUE(b != nullptr);
    b->arrived = true;
    b->region = start;
    // South of the start pad, where a 17 x 17 Landing Pad has room (C5).
    b->at = w.region(start).places()->ground(Vec2(76, 26));
    std::printf("  %s\n", w.devFrontier("frontier build landingPad").c_str());
    for (int i = 0; i < 180; i++) w.step();
    // Saved and loaded, the pilot back on line.
    const WorldSave s = w.save();
    w.leave(me);
    WorldMap v(first(), params());
    ASSERT_TRUE(v.restore(s).empty());
    ASSERT_TRUE(v.seat(me, myKey) == WorldMap::Seated::restored);
    v.step();
    const auto msg = v.worldMessage(me);
    ASSERT_TRUE(msg.has_value());
    const auto prog = v.progressMessage(me);
    ASSERT_TRUE(prog.has_value());
    EXPECT_TRUE(prog->regionsSeen.count("rust-hollow") == 1);
    EXPECT_EQ(msg->here, int64_t(start));
    int64_t rustPad = -1, rustOwn = -1, startRealm = 0;
    for (size_t p = 0; p < msg->regions[rust].pads.size(); p++)
        (msg->regions[rust].pads[p].building ? rustPad : rustOwn) = int64_t(p);
    for (const auto& p : msg->regions[start].pads) startRealm += p.building ? 1 : 0;
    std::printf("  Rust Hollow: realm pad %lld, own pad %lld; Prospect Landing realm pads %lld\n", (long long)rustPad, (long long)rustOwn,
                (long long)startRealm);
    ASSERT_TRUE(rustPad >= 0 && rustOwn >= 0);
    EXPECT_TRUE(startRealm >= 1);
    // The map's rule on the user's state.
    EXPECT_TRUE(net::atlas::padPick(*msg, prog->regionsSeen, int64_t(rust), rustPad).ok);
    const auto own = net::atlas::padPick(*msg, prog->regionsSeen, int64_t(rust), rustOwn);
    std::printf("  own pad: %s\n", own.why.c_str());
    EXPECT_TRUE(!own.ok && own.why.find("own pad") != std::string::npos);
    const size_t cold = *v.regionNamed("cold-fork");
    const auto none = net::atlas::padPick(*msg, prog->regionsSeen, int64_t(cold), 0);
    std::printf("  cold-fork: %s\n", none.why.c_str());
    EXPECT_TRUE(!none.ok);
}
