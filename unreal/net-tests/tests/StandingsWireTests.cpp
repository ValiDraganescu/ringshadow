// The realm week's standings (S4, docs/mmorpg-play.md "Standings") on the
// wire and in words: the Standings message, the order, the map tooltip's
// line, the week's end as KSTR's toast and the away report say it.

#include "test.h"

#include "AwayReport.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "Standings.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::net;

StandingsMessage sample() {
    StandingsMessage m;
    m.weekStart = 16800;
    m.weekEnds = 16800 + 7 * 2400;
    m.rows.push_back(WeekStanding{"Vesper", true, 18400, 12, 0, 2, 1});
    m.rows.push_back(WeekStanding{"Pilot 1A2B", false, 4200, 31, 3, 1, 2});
    m.rows.push_back(WeekStanding{"Sorrel", true, 0, 40, 4, 0, 3});
    m.you = 1;
    return m;
}

} // namespace

TEST(standings_travel_in_a_message_of_their_own) {
    const StandingsMessage m = sample();
    const auto bytes = encode(m);
    EXPECT_TRUE(messageType(bytes) == MessageType::standings);
    StandingsMessage back;
    ASSERT_TRUE(decode(bytes, back));
    EXPECT_TRUE(back == m);
    // None: an empty table, nobody's row.
    ASSERT_TRUE(decode(encode(StandingsMessage{}), back));
    EXPECT_TRUE(back.rows.empty() && !back.you.has_value());
    // The viewer's row past the table is refused.
    StandingsMessage bad = m;
    bad.you = 3;
    EXPECT_TRUE(!decode(encode(bad), back));
    // Not a Progress.
    ProgressMessage p;
    EXPECT_TRUE(!decode(encode(m), p));
    EXPECT_EQ(kProtocolVersion, uint16_t(33));
}

TEST(standings_order_and_words) {
    std::vector<WeekStanding> rows = {{"B", false, 100, 1, 0, 0, 0}, {"A", false, 100, 1, 0, 0, 0}, {"C", true, 100, 5, 0, 0, 0},
                                      {"D", false, 900, 0, 0, 0, 0}};
    std::sort(rows.begin(), rows.end(), standingBefore);
    EXPECT_EQ(rows[0].callsign, std::string("D"));
    EXPECT_EQ(rows[1].callsign, std::string("C"));
    EXPECT_EQ(rows[2].callsign, std::string("A"));
    EXPECT_EQ(ordinal(1), std::string("1st"));
    EXPECT_EQ(ordinal(2), std::string("2nd"));
    EXPECT_EQ(ordinal(3), std::string("3rd"));
    EXPECT_EQ(ordinal(4), std::string("4th"));
    EXPECT_EQ(ordinal(11), std::string("11th"));
    EXPECT_EQ(ordinal(12), std::string("12th"));
    EXPECT_EQ(ordinal(22), std::string("22nd"));
    const StandingsMessage m = sample();
    EXPECT_EQ(standingLine(3, m.rows[1]), std::string("3rd · 4,200 ore this week"));
    EXPECT_EQ(standingLine(5, m.rows[2]), std::string("5th · 40 bugs this week"));
    EXPECT_EQ(standingLine(6, WeekStanding{"X", false, 0, 0, 1, 0, 0}), std::string("6th · 1 wave held this week"));
    EXPECT_EQ(standingLine(8, WeekStanding{}), std::string("8th · no work counted this week"));
    EXPECT_EQ(weekTopLine("Vesper", 18400), std::string("This week's top hauler: Vesper, 18,400 ore"));
    EXPECT_EQ(weekTopLine("", 0), std::string("No ore was hauled this week"));
    EXPECT_EQ(weekLeft(3 * 86400 + 4 * 3600 + 5), std::string("3 d 4 h left"));
    EXPECT_EQ(weekLeft(10), std::string("1 m left"));
    // The away report's row says the same.
    EXPECT_EQ(awayLine(AwayRow{0, AwayKind::week, "Vesper", "", "", 18400}), std::string("This week's top hauler: Vesper, 18,400 ore"));
    ProgressMessage p;
    p.away = AwayReport{};
    p.away->rows.push_back(AwayRow{5, AwayKind::week, "Vesper", "", "", 18400});
    p.news.push_back(ProgressMessage::News{ProgressMessage::NewsKind::week, "top", 0, 18400, "Vesper"});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(p), back));
    EXPECT_TRUE(back.away == p.away && back.news == p.news);
}
