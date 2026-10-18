// docs/perf.md "Server" round 25: in a snapshot round each entity's wire
// bits are made once for every pilot sent the same value
// (`WorldMap::beginSnapshots`, `EntityState::canon`). Every pilot is sent
// the bytes it is sent without the round, snapshot after snapshot: pilots
// driving at the start, more in the nest among the Matriarch's bugs (sent
// scrubbed), one of them driving off.

#include "test.h"

#include "NetSnapshot.h"
#include "WorldMap.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::server;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

} // namespace

TEST(snapshotRound_everyPilotIsSentTheBytesItIsSentWithout) {
    WorldParams p;
    p.heldHour = 12;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    WorldMap shared(first(), p), alone(first(), p);
    constexpr int pilots = 6;
    for (WorldMap* w : {&shared, &alone})
        for (int k = 0; k < pilots; k++) {
            const PilotId id = PilotId(k + 1);
            w->join(id);
            if (k >= 3) {
                const std::optional<size_t> nest = w->nestRegion();
                ASSERT_TRUE(nest.has_value());
                w->devTransfer(id, *nest, w->region(*nest).places()->pads[0].at);
            }
        }
    std::vector<net::SnapshotWriter> a(pilots), b(pilots);
    double clock = 100;
    int64_t snapshots = 0, sharedBits = 0, differ = 0;
    for (int t = 0; t < 900; t++) {
        for (WorldMap* w : {&shared, &alone}) {
            for (int k = 0; k < pilots; k++) {
                PilotInput in;
                in.tick = t + 1;
                in.visit = w->visit(PilotId(k + 1));
                // Pilot 2 drives straight off after a while; the rest circle.
                in.walk = k == 1 && t > 300 ? Vec2(1, 0.2) : Vec2(1, 0);
                in.heading = k == 1 && t > 300 ? 0.2 : double(t) / 60 * 0.4 + double(k);
                w->input(PilotId(k + 1), in);
            }
            w->step();
        }
        clock += 1.0 / 60;
        if (t % 3) continue;
        shared.beginSnapshots();
        for (int k = 0; k < pilots; k++) {
            const PilotId id = PilotId(k + 1);
            const net::Snapshot sa = shared.snapshot(id), sb = alone.snapshot(id);
            for (const auto& e : sa.entities) sharedBits += e.canon ? 1 : 0;
            const auto oa = a[size_t(k)].write(sa, clock, 1 << 20), ob = b[size_t(k)].write(sb, clock, 1 << 20);
            differ += oa.messages == ob.messages ? 0 : 1;
            snapshots += 1;
            for (auto* pair : {&oa, &ob}) {
                net::AckMessage ack;
                ack.received.push_back({pair->id, pair->messages.size() >= 32 ? ~0u : (uint32_t(1) << pair->messages.size()) - 1});
                (pair == &oa ? a : b)[size_t(k)].ack(ack, clock);
            }
        }
        shared.endSnapshots();
    }
    EXPECT_EQ(differ, int64_t(0));
    EXPECT_EQ(snapshots, int64_t(300 * pilots));
    EXPECT_TRUE(sharedBits > 1000);
    std::printf("  (%lld snapshots, %lld entities with shared bits)\n", (long long)snapshots, (long long)sharedBits);
}
