// The hunt (N4, docs/mmorpg-play.md "The hunt") on the wire and in words:
// the Hunt message, the CallHunt request, a mercenary's guard role, the
// hunt's news and its away row.

#include "test.h"

#include "AwayReport.h"
#include "Hunt.h"
#include "NetPacket.h"
#include "NetProtocol.h"

#include <string>

namespace {

using namespace ac;
using namespace ac::net;

HuntMessage sample() {
    HuntMessage m;
    m.state = HuntMessage::State::gathering;
    m.caller = "Pilot 1A2B";
    m.region = 4;
    m.regionName = "Slag Pockets";
    m.x = 120.5;
    m.z = 88.25;
    m.ring = 30;
    m.calledAt = 1000;
    m.gatherEnds = 1120;
    m.failsAt = 1600;
    m.nextCall = 1000 + 2400;
    m.refusal = "A hunt is on";
    m.party.push_back(HuntHunter{HuntHunter::Kind::frontier, "Sorrel", "", 3, 4, 100, 80, true, false});
    m.party.push_back(HuntHunter{HuntHunter::Kind::merc, "Slate", "Pilot 1A2B", 0, std::nullopt, 0, 0, false, false});
    m.party.push_back(HuntHunter{HuntHunter::Kind::frontier, "Vesper", "", 5, 2, 10, 12, false, true});
    return m;
}

} // namespace

TEST(hunt_travels_in_a_message_of_its_own) {
    const HuntMessage m = sample();
    const auto bytes = encode(m);
    EXPECT_TRUE(messageType(bytes) == MessageType::hunt);
    HuntMessage back;
    ASSERT_TRUE(decode(bytes, back));
    EXPECT_TRUE(back == m);
    ASSERT_TRUE(decode(encode(HuntMessage{}), back));
    EXPECT_TRUE(back.state == HuntMessage::State::none && back.party.empty() && !back.region);
    // Not a Standings, not a Progress.
    StandingsMessage s;
    EXPECT_TRUE(!decode(bytes, s));
    ProgressMessage p;
    EXPECT_TRUE(!decode(bytes, p));
    EXPECT_EQ(kProtocolVersion, uint16_t(35));
}

TEST(hunt_call_and_a_mercenarys_role_requests) {
    RequestMessage call;
    call.id = 9;
    call.what = RequestMessage::CallHunt{};
    RequestMessage back;
    ASSERT_TRUE(decode(encode(call), back));
    EXPECT_TRUE(back == call);
    for (const auto& ask : {RequestMessage::Merc{RequestMessage::Merc::Act::hire, 0, "", 0, true},
                            RequestMessage::Merc{RequestMessage::Merc::Act::role, 3, "", 0, true},
                            RequestMessage::Merc{RequestMessage::Merc::Act::role, 3, "", 0, false}}) {
        RequestMessage m;
        m.id = 4;
        m.what = ask;
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
    // The Progress message's mercenary carries its role and its hunt.
    ProgressMessage p;
    ProgressMessage::Merc x;
    x.id = 1;
    x.name = "Slate";
    x.region = "start";
    x.guard = true;
    x.hunting = true;
    p.mercs.push_back(x);
    p.news.push_back(ProgressMessage::News{ProgressMessage::NewsKind::hunt, "called/Slag Pockets", 0, 0, "Pilot 1A2B"});
    ProgressMessage pb;
    ASSERT_TRUE(decode(encode(p), pb));
    EXPECT_TRUE(pb.mercs == p.mercs && pb.news == p.news);
}

TEST(hunt_words) {
    HuntMessage m = sample();
    EXPECT_EQ(huntThere(m), int64_t(1));
    EXPECT_EQ(huntLine(m, 1040), std::string("Gathering at the ring in Slag Pockets: 1 of 3 there, 1 m 20 s left"));
    EXPECT_EQ(huntLine(m, 1130), std::string("Gathering at the ring in Slag Pockets: 1 of 3 there, waiting for Pilot 1A2B at the ring"));
    m.state = HuntMessage::State::won;
    EXPECT_EQ(huntLine(m, 0), std::string("She fell to the hunt Pilot 1A2B called"));
    m.state = HuntMessage::State::none;
    m.refusal = "";
    EXPECT_EQ(huntLine(m, 0), std::string("Not called today: any pilot may call it"));
    EXPECT_EQ(huntNewsTitle("called/Slag Pockets"), std::string("THE HUNT IS CALLED"));
    EXPECT_EQ(huntNewsLine("called/Slag Pockets", "Pilot 1A2B", 0),
              std::string("Pilot 1A2B called it: defenders and guards to the ring in Slag Pockets"));
    EXPECT_EQ(huntNewsLine("charge/Slag Pockets", "", 4), std::string("4 hunters go in at Slag Pockets"));
    EXPECT_EQ(huntNewsLine("won/Slag Pockets", "Pilot 1A2B", 3), std::string("The Matriarch is down, Pilot 1A2B hardest"));
    EXPECT_EQ(huntNewsLine("failed/Slag Pockets", "", 0), std::string("Ten minutes and she stands"));
    EXPECT_EQ(awayLine(AwayRow{0, AwayKind::hunt, "Pilot 1A2B", "won", "Slag Pockets", 4}),
              std::string("Pilot 1A2B's hunt in Slag Pockets brought her down"));
    EXPECT_EQ(awayLine(AwayRow{0, AwayKind::hunt, "Pilot 1A2B", "failed", "Slag Pockets", 4}),
              std::string("Pilot 1A2B's hunt in Slag Pockets failed: she stands"));
    // Through the ledger's wire.
    ProgressMessage p;
    p.away = AwayReport{};
    p.away->rows.push_back(AwayRow{5, AwayKind::hunt, "Pilot 1A2B", "won", "Slag Pockets", 4});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(p), back));
    EXPECT_TRUE(back.away == p.away);
}
