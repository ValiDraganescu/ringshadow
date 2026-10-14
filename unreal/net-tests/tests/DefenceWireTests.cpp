// The defences on the wire (D1, D2): a gatling gun is a building like any
// other; an armed Citadel carries its marks and where its four corner
// turrets point, and nothing of their targets.

#include "test.h"

#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <cmath>

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(snapshot_carries_the_citadel_turrets_and_the_gatling) {
    Snapshot s;
    Structure c(3, StructureKind::citadel, 0, Vec2(10, 10));
    c.hp = 1500;
    c.marks = TurretMarks{.rate = 2, .damage = 0, .range = 3};
    c.guns = std::array<CornerGun, 4>{};
    for (int i = 0; i < 4; i++) {
        (*c.guns)[size_t(i)].aim = -1.0 + 0.5 * i;
        (*c.guns)[size_t(i)].target = 40 + i;
        (*c.guns)[size_t(i)].cooldown = 0.1;
    }
    Structure g(4, StructureKind::gatling, 0, Vec2(20, 10));
    g.hp = 300;
    g.aim = 1.25;
    s.entities.push_back(EntityState{c});
    s.entities.push_back(EntityState{g});
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    ASSERT_TRUE(o.messages.size() == 1);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->snapshot.entities.size() == 2);
    const Structure* cb = p->snapshot.entities[0].structure();
    ASSERT_TRUE(cb && cb->marks && cb->guns);
    EXPECT_TRUE(*cb->marks == *c.marks);
    for (int i = 0; i < 4; i++) {
        // An angle comes back in [0, 2π).
        EXPECT_NEAR(std::remainder((*cb->guns)[size_t(i)].aim - (-1.0 + 0.5 * i), 2 * 3.141592653589793), 0.0, 1e-3);
        // Its targets and reloads stay on the server.
        EXPECT_TRUE(!(*cb->guns)[size_t(i)].target.has_value());
    }
    const Structure* gb = p->snapshot.entities[1].structure();
    ASSERT_TRUE(gb);
    EXPECT_TRUE(gb->kind == StructureKind::gatling);
    EXPECT_NEAR(gb->aim.value_or(0), 1.25, 1e-3);
    EXPECT_TRUE(!gb->marks.has_value());
}

/// D3: a placed turret carries its marks, air reach included; the progress
/// message the turrets' kills and who upgraded one.
TEST(turret_marks_kills_and_upgrades_on_the_wire) {
    Snapshot s;
    Structure g(4, StructureKind::gatling, 0, Vec2(20, 10));
    g.hp = 300;
    g.marks = TurretMarks{.rate = 1, .damage = 0, .range = 2, .air = 3};
    s.entities.push_back(EntityState{g});
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    ASSERT_TRUE(o.messages.size() == 1);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value());
    const Structure* gb = p->snapshot.entities[0].structure();
    ASSERT_TRUE(gb && gb->marks);
    EXPECT_TRUE(*gb->marks == *g.marks);

    ProgressMessage m;
    m.turretKills = {{4, 17}, {9, 1}};
    m.news.push_back(ProgressMessage::News{ProgressMessage::NewsKind::upgraded, "gatling/air", 0, 3, "Pilot 1A2B"});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.turretKills == m.turretKills);
    EXPECT_TRUE(back.news == m.news);
}
