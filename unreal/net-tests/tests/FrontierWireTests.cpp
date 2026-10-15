// Frontier pilots on the wire (P1, docs/frontier.md; protocol 28): the sky
// carries where each one is (its region, place, Prospector, level and what
// it is doing; none while on its way), the progress message the roster
// (callsigns, habits, paints) and their news (the radio, the deeds).

#include "test.h"

#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(snapshot_sky_carries_the_frontier_pilots) {
    Snapshot s;
    Sky sky;
    sky.time = 90.5;
    Sky::Frontier mining;
    mining.id = 1;
    mining.region = 4;
    mining.at = Vec2(96.25, 44.5);
    mining.unit = 1234;
    mining.level = 14;
    mining.doing = FrontierDoing::mining;
    Sky::Frontier flying;
    flying.id = 5;
    flying.level = 3;
    flying.doing = FrontierDoing::flying;
    Sky::Frontier down;
    down.id = 6;
    down.region = 0;
    down.at = Vec2(0.5, 143.75);
    down.level = 100;
    down.doing = FrontierDoing::down;
    sky.frontier = {mining, flying, down};
    s.sky = sky;
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->snapshot.sky.has_value());
    EXPECT_TRUE(*p->snapshot.sky == sky);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(28));
}

TEST(progress_carries_the_frontier_roster_and_news) {
    ProgressMessage m;
    m.frontier.push_back(ProgressMessage::FrontierPilot{1, "Vesper", FrontierHabit::miner, 2});
    m.frontier.push_back(ProgressMessage::FrontierPilot{4, "Larkspur", FrontierHabit::wanderer, 7});
    using K = ProgressMessage::NewsKind;
    m.news.push_back(ProgressMessage::News{K::frontier, "radio", 0, 1, "Vesper: swarm moving on Rust Hollow. Guns up, pilots."});
    m.news.push_back(ProgressMessage::News{K::frontier, "deed", 0, 4, "Larkspur built a Hab Dome in Rust Hollow"});
    m.news.push_back(ProgressMessage::News{K::frontier, "come", 0, 4, "Larkspur landed at Tern Ridge  ·  Wanderer, level 6"});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.frontier == m.frontier);
    EXPECT_TRUE(back.news == m.news);
    EXPECT_EQ(std::string(title(FrontierHabit::defender)), std::string("Defender"));
    EXPECT_EQ(std::string(title(FrontierDoing::fighting)), std::string("fighting bugs"));
}
