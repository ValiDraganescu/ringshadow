// The Brood on the wire (docs/creatures.md, "On the wire"): a bug is a unit
// with its state, height, strikes and sac, and nothing of its orders; the
// sky carries the raids' warnings and the Matriarch's roars.

#include "test.h"

#include "Brood.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <vector>

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(snapshot_carries_a_bug_and_not_its_orders) {
    Snapshot s;
    Unit u(7, UnitKind::gloamwing, 1, Vec2(3.5, -2.25), 0.5, Unit::Task::idle);
    Bug b;
    b.state = Bug::State::dive;
    b.height = 0.6;
    b.strikes = 13;
    b.sac = 0;
    b.prey = 4;
    b.site = Vec2(40, 40);
    b.leaving = true;
    u.bug = b;
    u.target = 4;
    Unit l(8, UnitKind::lodeback, 1, Vec2(-1, 1), 0, Unit::Task::idle);
    Bug lb;
    lb.state = Bug::State::eat;
    lb.sac = 40;
    l.bug = lb;
    s.entities.push_back(EntityState{u});
    s.entities.push_back(EntityState{l});
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    ASSERT_TRUE(o.messages.size() == 1);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->snapshot.entities.size() == 2);
    const Unit* g = p->snapshot.entities[0].unit();
    ASSERT_TRUE(g && g->bug.has_value());
    EXPECT_TRUE(g->kind == UnitKind::gloamwing);
    EXPECT_TRUE(g->bug->state == Bug::State::dive);
    EXPECT_NEAR(g->bug->height, 0.6, 1e-9);
    EXPECT_EQ(g->bug->strikes, int64_t(13));
    EXPECT_EQ(g->target.value_or(-1), int64_t(4));
    // Its orders stay on the server.
    EXPECT_TRUE(!g->bug->prey.has_value());
    EXPECT_TRUE(!g->bug->leaving);
    const Unit* lo = p->snapshot.entities[1].unit();
    ASSERT_TRUE(lo && lo->bug.has_value());
    EXPECT_TRUE(lo->bug->state == Bug::State::eat);
    // 4 bits: 40 of 60 is 10 of 15.
    EXPECT_NEAR(lo->bug->sac, 40.0, 1e-9);
    // A bug costs few bytes past a unit's own: kind, owner, place, hp,
    // heading, and its 17 bits.
    EXPECT_TRUE(o.messages[0].size() < 60);
}

TEST(snapshot_sky_carries_the_raids_and_roars) {
    Snapshot s;
    Sky sky;
    sky.time = 812.5;
    sky.raids.push_back(Sky::Raid{5, 4, 1203, StructureKind::habDome, Vec2(12.5, -3.25), Vec2(-40, 10.5), 830.25});
    sky.roars.push_back(Sky::Roar{2, 8, Vec2(60, 20), 811});
    s.sky = sky;
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->snapshot.sky.has_value());
    EXPECT_TRUE(*p->snapshot.sky == sky);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(15));
}

/// The Matriarch's news (N3): her fall to every pilot, each fighter's share
/// and her return; a news kind past the last (P1's frontier) is refused.
TEST(progress_carries_the_matriarchs_news) {
    ProgressMessage m;
    using K = ProgressMessage::NewsKind;
    m.news.push_back(ProgressMessage::News{K::matriarch, "down", 0, 3, "Pilot 1A2B"});
    m.news.push_back(ProgressMessage::News{K::matriarch, "share/620/spacedock", 0, 1240, ""});
    m.news.push_back(ProgressMessage::News{K::matriarch, "back", 0, 0, ""});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.news == m.news);
    m.news.front().kind = K(uint8_t(K::frontier) + 1);
    EXPECT_TRUE(!decode(encode(m), back));
}

/// The Matriarch's last strike rides in her sac's 4 bits: the client tells
/// her slam from her spit (N3).
TEST(snapshot_tells_the_matriarchs_slam_from_her_spit) {
    for (const double struck : {Matriarch::struckSlam, Matriarch::struckSpit}) {
        Snapshot s;
        Unit q(9, UnitKind::matriarch, 1, Vec2(10, 10), 0, Unit::Task::idle);
        Bug b;
        b.state = Bug::State::attack;
        b.strikes = 3;
        b.sac = struck;
        q.bug = b;
        s.entities.push_back(EntityState{q});
        SnapshotWriter w;
        SnapshotReader r;
        const auto o = w.write(s, 0, 1 << 20);
        ASSERT_TRUE(o.messages.size() == 1);
        const auto p = r.read(o.messages[0]);
        ASSERT_TRUE(p.has_value());
        const Unit* back = p->snapshot.entities[0].unit();
        ASSERT_TRUE(back && back->bug.has_value());
        EXPECT_EQ(Matriarch::slammed(back->bug->sac), struck == Matriarch::struckSlam);
        EXPECT_EQ(Matriarch::spat(back->bug->sac), struck == Matriarch::struckSpit);
    }
}
