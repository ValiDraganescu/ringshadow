// Two humans on the wire (W5, docs/mmorpg-play.md "Two pilots"; protocol
// 30): the sky carries every human pilot on line, its callsign, region and
// the Prospector it drives, and while it is aboard the Dropship its flight,
// for the others to watch it come down, lift and go.

#include "test.h"

#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(snapshot_sky_carries_the_human_pilots_and_their_flights) {
    Snapshot s;
    Sky sky;
    sky.time = 120.25;
    Sky::Human driving{"Pilot 443E", 2, 812, std::nullopt};
    Sky::Human aboard{"Pilot 2B3C", 0, 0, std::nullopt};
    Sky::Human::Ferry f;
    f.boarded = Vec2(70.5, 41.25);
    f.to = 3;
    f.at = Vec2(12.75, 96.0);
    f.plan = Ferry::plan(2);
    f.elapsed = 4.25;
    aboard.ferry = f;
    sky.pilots = {driving, aboard};
    s.sky = sky;
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->snapshot.sky.has_value());
    EXPECT_TRUE(*p->snapshot.sky == sky);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(30));
}
