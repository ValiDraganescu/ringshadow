// The wire's kind widths come from the kind counts (NetCodec.h,
// docs/kind-registry.md "Net: widths from the count"): every unit kind goes
// through each place a kind is written (a unit, a training line, the driven
// kinds' mask, a request, an event) and comes back as itself.

#include "test.h"

#include "NetCodec.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <map>
#include <vector>

namespace {

using namespace ac;
using namespace ac::net;

std::vector<SnapshotPart> roundTrip(const Snapshot& s) {
    SnapshotWriter w;
    SnapshotReader r;
    std::vector<SnapshotPart> out;
    for (const auto& m : w.write(s, 0, 1 << 20).messages)
        if (auto p = r.read(m)) out.push_back(*p);
    return out;
}

} // namespace

TEST(kind_widths_cover_the_counts) {
    using namespace ac::net::codec;
    EXPECT_EQ(bitsFor(1), 0);
    EXPECT_EQ(bitsFor(2), 1);
    EXPECT_EQ(bitsFor(16), 4);
    EXPECT_EQ(bitsFor(17), 5);
    EXPECT_EQ(kUnitKindCount, allCases<UnitKind>().size());
    EXPECT_EQ(kStructureKindCount, allCases<StructureKind>().size());
    // Wide enough, and no wider.
    EXPECT_TRUE((size_t(1) << kUnitKindBits) >= kUnitKindCount);
    EXPECT_TRUE((size_t(1) << (kUnitKindBits - 1)) < kUnitKindCount);
    EXPECT_TRUE((size_t(1) << kStructureKindBits) >= kStructureKindCount);
    EXPECT_TRUE((size_t(1) << (kStructureKindBits - 1)) < kStructureKindCount);
    // Today's counts give protocol 35's widths; a kind past them widens the wire.
    if (kUnitKindBits != 4 || kStructureKindBits != 4) EXPECT_TRUE(kProtocolVersion > uint16_t(35));
}

TEST(every_unit_kind_round_trips_through_the_wire) {
    const auto kinds = allCases<UnitKind>();
    Snapshot s;
    Structure b(100, StructureKind::garrison, 0, Vec2(10, 10), 4.5, 12);
    b.line = std::vector<UnitKind>(kinds.begin(), kinds.end());
    s.entities.push_back(EntityState{b});
    Player p;
    PilotRecord rec;
    Directives d;
    int64_t id = 1;
    for (UnitKind k : kinds) {
        s.entities.push_back(EntityState{Unit(id, k, 0, Vec2(double(id), 2), 1.0, Unit::Task::idle, 0, 0, 0, 0)});
        KindRecord kr;
        kr.count = id;
        rec.kinds[k] = kr;
        Request q;
        q.id = id;
        q.what = Request::What(Request::What::Unit{k});
        d.queue.push_back(q);
        id++;
    }
    p.pilot = rec;
    p.directives = d;
    s.players = {p};

    std::map<int64_t, EntityState> got;
    std::vector<Player> players;
    for (const SnapshotPart& part : roundTrip(s)) {
        for (const EntityState& e : part.snapshot.entities) got[e.id()] = e;
        if (!part.snapshot.players.empty()) players = part.snapshot.players;
    }
    ASSERT_TRUE(got.size() == kinds.size() + 1);
    for (int64_t i = 1; i < id; i++) {
        ASSERT_TRUE(got[i].unit() != nullptr);
        EXPECT_TRUE(got[i].unit()->kind == kinds[size_t(i - 1)]);
        EXPECT_TRUE(*got[i].unit() == quantised(*s.entities[size_t(i)].unit()));
    }
    ASSERT_TRUE(got[100].structure() != nullptr);
    EXPECT_TRUE(got[100].structure()->line == b.line);
    ASSERT_TRUE(players.size() == 1);
    ASSERT_TRUE(players[0].pilot.has_value());
    EXPECT_EQ(players[0].pilot->kinds.size(), kinds.size());
    for (UnitKind k : kinds) EXPECT_EQ(players[0].pilot->kinds.at(k).count, rec.kinds.at(k).count);
    EXPECT_TRUE(players[0] == quantised(p));

    std::vector<GameEvent> events;
    for (UnitKind k : kinds) events.push_back(GameEvent(GameEvent::Trained{7, k, Vec2(1, 2)}));
    const auto messages = encodeEvents(9, events);
    ASSERT_TRUE(messages.size() == 1);
    EventMessage m;
    ASSERT_TRUE(decode(messages[0], m));
    ASSERT_TRUE(m.events.size() == events.size());
    for (size_t i = 0; i < events.size(); i++) EXPECT_TRUE(m.events[i] == events[i]);
}

TEST(snapshot_with_the_last_kind_round_trips) {
    const UnitKind last = allCases<UnitKind>().back();
    EXPECT_TRUE(last == UnitKind::matriarch);
    Snapshot s;
    Unit u(1, last, 1, Vec2(30.25, -12.5), 2.0, Unit::Task::attacking, 4, 1.25, 5, 17.5);
    u.hp = Rules::hp(last) * 0.75;
    s.entities.push_back(EntityState{u, false, Rank::driven});
    s.driven = 1;
    const auto parts = roundTrip(s);
    ASSERT_TRUE(parts.size() == 1);
    ASSERT_TRUE(parts[0].snapshot.entities.size() == 1);
    ASSERT_TRUE(parts[0].snapshot.entities[0].unit() != nullptr);
    EXPECT_TRUE(*parts[0].snapshot.entities[0].unit() == quantised(u));
}
