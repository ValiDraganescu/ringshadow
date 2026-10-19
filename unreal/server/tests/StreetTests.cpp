// The dev line `street` (C5, WorldStreet.cpp, docs/building-sizes.md), the
// mmosize pictures' staging: one kind ahead of the pilot, or every kind down
// a street out of a Citadel's door. Isolated scenes; run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "WorldMap.h"

#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 93;
const std::string keyA = "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf";

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
    p.starfalls = false;
    p.firstStep = 0;
    p.brood = false;
    return p;
}

int count(const Simulation& sim, StructureKind kind) {
    int n = 0;
    for (const Structure& s : sim.state.structures) n += s.kind == kind && s.complete() ? 1 : 0;
    return n;
}

} // namespace

/// `street all`: a Citadel and one of each kind a pilot builds, the street
/// on both sides of the road; a second line takes the first's away.
TEST(street_allSetsOutEveryKindOnBothSides) {
    WorldMap w(first(), params());
    w.seat(a, keyA);
    w.step();
    const std::string said = w.devStreet("street all");
    std::printf("  %s\n", said.c_str());
    ASSERT_TRUE(said.rfind("dev: street all: ", 0) == 0);
    Simulation& sim = w.regionOf(a)->simulation();
    for (const StructureKind k : {StructureKind::habDome, StructureKind::garrison, StructureKind::bastion, StructureKind::foundry,
                                  StructureKind::spacedock, StructureKind::lab, StructureKind::sentinel, StructureKind::gatling,
                                  StructureKind::landingPad})
        EXPECT_EQ(count(sim, k), 1);
    const Structure* citadel = nullptr;
    for (const Structure& s : sim.state.structures)
        if (s.kind == StructureKind::citadel) citadel = &s;
    ASSERT_TRUE(citadel != nullptr);
    int left = 0, right = 0;
    for (const Structure& s : sim.state.structures) {
        if (s.kind == StructureKind::citadel || s.kind == StructureKind::derrick) continue;
        (s.position.x < citadel->position.x ? left : right) += 1;
    }
    EXPECT_TRUE(left >= 3 && right >= 3);
    const size_t before = sim.state.structures.size();
    w.devStreet("street all");
    EXPECT_EQ(sim.state.structures.size(), before);
}
