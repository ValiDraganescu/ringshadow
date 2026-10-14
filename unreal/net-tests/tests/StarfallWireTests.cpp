// Starfall on the wire (docs/weather.md "Starfall", E2): the world's sky
// carries the starfalls told to every pilot, in every snapshot part.

#include "test.h"

#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <vector>

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(snapshot_sky_carries_the_starfalls) {
    Snapshot s;
    for (int64_t i = 0; i < 300; i++) {
        Unit u(i + 1, UnitKind::prospector, 0, Vec2(double(i % 40) - 20, double(i / 40) - 4), 1.0, Unit::Task::idle);
        s.entities.push_back(EntityState{u});
    }
    Sky sky;
    sky.time = 4321.5;
    sky.weatherSeed = 77;
    // One coming (told, not landed), one landed with its deposit at index 43.
    sky.starfalls.push_back(Sky::Starfall{3, 1, Vec2(-31.25, 17.5), 4370.25, 291.5, std::nullopt});
    sky.starfalls.push_back(Sky::Starfall{2, 0, Vec2(12.75, -40.0), 3100.0, 255.125, 43});
    s.sky = sky;
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    EXPECT_TRUE(o.messages.size() > 1);
    for (const auto& m : o.messages) {
        const auto p = r.read(m);
        ASSERT_TRUE(p.has_value());
        ASSERT_TRUE(p->snapshot.sky.has_value());
        EXPECT_TRUE(*p->snapshot.sky == sky);
    }
    // None: an empty list, and broken bytes are refused.
    sky.starfalls.clear();
    s.sky = sky;
    SnapshotWriter w2;
    SnapshotReader r2;
    const auto o2 = w2.write(s, 0, 1 << 20);
    const auto p2 = r2.read(o2.messages[0]);
    ASSERT_TRUE(p2.has_value());
    EXPECT_TRUE(p2->snapshot.sky->starfalls.empty());
    // Protocol 9 brought them.
    EXPECT_TRUE(kProtocolVersion >= uint16_t(9));
}
