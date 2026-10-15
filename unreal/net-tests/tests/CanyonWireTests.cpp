// The canyon game on the wire (Z2, docs/canyon.md): the sky carries each
// zone's game (its state, wave, bugs left, clocks, the prize taken against
// the fail mark, the tunnels), and the progress message the canyon's news.

#include "test.h"

#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(snapshot_sky_carries_the_canyon_zones) {
    Snapshot s;
    Sky sky;
    sky.time = 1200.5;
    Sky::Zone running;
    running.region = 1;
    running.zone = 0;
    running.state = Sky::Zone::State::running;
    running.wave = 5;
    running.waves = 10;
    running.left = 11;
    running.ends = 1290.25;
    running.taken = 137;
    running.mark = 300;
    running.tunnels = 0b11;
    running.boss = true;
    Sky::Zone held;
    held.region = 7;
    held.zone = 2;
    held.state = Sky::Zone::State::held;
    held.wave = 10;
    held.waves = 10;
    held.next = 3600.75;
    held.paused = true;
    sky.zones = {running, held};
    s.sky = sky;
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->snapshot.sky.has_value());
    EXPECT_TRUE(*p->snapshot.sky == sky);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(27));
}

TEST(progress_carries_the_canyon_news) {
    ProgressMessage m;
    using K = ProgressMessage::NewsKind;
    m.news.push_back(ProgressMessage::News{K::canyon, "wave/3/120", 640, 450, "The Fork"});
    m.news.push_back(ProgressMessage::News{K::canyon, "wave/10/800/spacedock", 4000, 3000, "The Fork"});
    m.news.push_back(ProgressMessage::News{K::canyon, "held", 0, 2, "The Fork"});
    m.news.push_back(ProgressMessage::News{K::canyon, "failed", 0, 6, "The Fork"});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.news == m.news);
}
