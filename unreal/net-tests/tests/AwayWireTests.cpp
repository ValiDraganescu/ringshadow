// The away report (X2, docs/mmorpg-play.md "Coming back") on the wire and
// in words: the Progress message's `away`, its lines as the home screen,
// the Journal and KSTR take them, and the summary file beside the save.

#include "test.h"

#include "AwayReport.h"
#include "NetPacket.h"
#include "NetProtocol.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

using namespace ac;
using namespace ac::net;

AwayReport sample() {
    AwayReport r;
    r.ran = 6 * 3600 + 12 * 60 + 30;
    r.back = 1300 + 2 * 3600 + 10 * 60;
    r.realmOre = 410;
    r.realmHydrogen = -60;
    r.rows.push_back(AwayRow{120, AwayKind::frontier, "Sorrel", "Sorrel built a Derrick in Cold Fork", "", 0});
    r.rows.push_back(AwayRow{300, AwayKind::waves, "", "The Fork", "", 4});
    r.rows.push_back(AwayRow{900, AwayKind::merc, "Slate", "", "", 1240});
    r.rows.push_back(AwayRow{950, AwayKind::held, "", "The Fork", "", 2});
    r.rows.push_back(AwayRow{1000, AwayKind::raid, "", "Hab Dome", "Rust Hollow", 0});
    r.rows.push_back(AwayRow{1100, AwayKind::projectEnd, "", "The Relay", "", 0});
    return r;
}

} // namespace

TEST(progress_carries_the_away_report_once) {
    ProgressMessage m;
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(!back.away.has_value());
    m.away = sample();
    ASSERT_TRUE(decode(encode(m), back));
    ASSERT_TRUE(back.away.has_value());
    EXPECT_TRUE(*back.away == *m.away);
    // A kind past the last is refused.
    m.away->rows[0].kind = AwayKind(uint8_t(AwayKind::hunt) + 1);
    EXPECT_TRUE(!decode(encode(m), back));
    EXPECT_TRUE(kProtocolVersion >= uint16_t(35));
}

TEST(away_lines_say_what_the_rows_hold) {
    EXPECT_EQ(awayNumber(1240), std::string("1,240"));
    EXPECT_EQ(awayNumber(-1234567), std::string("-1,234,567"));
    EXPECT_EQ(awayRan(6 * 3600 + 12 * 60 + 30), std::string("6 h 12 m"));
    EXPECT_EQ(awayRan(14 * 60), std::string("14 m"));
    EXPECT_EQ(awayRan(20), std::string("under a minute"));
    EXPECT_EQ(awayRan(2 * 86400 + 3 * 3600), std::string("2 d 3 h"));
    const AwayReport r = sample();
    EXPECT_EQ(awayLine(r.rows[2]), std::string("Slate delivered 1,240 ore"));
    EXPECT_EQ(awayWhen(r, r.rows[5]), std::string("2 h 13 m ago"));
    EXPECT_EQ(awayLine(r.rows[4]), std::string("A raid hit the Hab Dome in Rust Hollow"));
    EXPECT_EQ(awayLine(AwayRow{0, AwayKind::fell, "", "Hab Dome", "Rust Hollow", 0}), std::string("A raid took the Hab Dome in Rust Hollow"));
    EXPECT_EQ(awayLine(r.rows[1]), std::string("The Fork: 4 waves beaten"));
    EXPECT_EQ(awayLine(AwayRow{0, AwayKind::queenDown, "Pilot 1A2B", "", "", 3}), std::string("The Matriarch went down · the hardest blows: Pilot 1A2B"));
    EXPECT_EQ(awayLine(AwayRow{0, AwayKind::projectStart, "Pilot 1A2B", "the Beacon", "", 0}), std::string("Pilot 1A2B started the Beacon"));
    // The home screen's two lines: the most telling three, then the count.
    const auto two = awaySummary(r);
    EXPECT_EQ(two[0], std::string("Your world ran 6 h 12 m."));
    EXPECT_EQ(two[1], std::string("Slate delivered 1,240 ore · The Fork held · Sorrel built a Derrick in Cold Fork · 3 more"));
    EXPECT_TRUE(awaySummary(AwayReport{})[0].empty());
    // KSTR's welcome: a loss first, then a canyon held, then the deliveries.
    EXPECT_EQ(awayVoice(r), std::string("welcome-held"));
    AwayReport lost = r;
    lost.rows.push_back(AwayRow{0, AwayKind::fell, "", "Derrick", "Cold Fork", 0});
    EXPECT_EQ(awayVoice(lost), std::string("welcome-brood"));
    AwayReport paid;
    paid.rows.push_back(r.rows[2]);
    EXPECT_EQ(awayVoice(paid), std::string("welcome-mercs"));
    EXPECT_TRUE(awayVoice(AwayReport{}).empty());
}

TEST(away_file_holds_each_pilots_two_lines) {
    EXPECT_EQ(awayFilePath("mmo/first.sqlite"), std::string("mmo/first.away"));
    EXPECT_EQ(awayFilePath("mmo/first"), std::string("mmo/first.away"));
    AwaySummaries all;
    all["0123456789abcdef0123456789abcdef"] = awaySummary(sample());
    all["fedcba9876543210fedcba9876543210"] = {"Your world ran 14 m.", "Tabs\tand\nnew lines go"};
    const AwaySummaries back = decodeAwayFile(encodeAwayFile(all));
    ASSERT_TRUE(back.size() == size_t(2));
    EXPECT_TRUE(back.at("0123456789abcdef0123456789abcdef") == all.at("0123456789abcdef0123456789abcdef"));
    EXPECT_EQ(back.at("fedcba9876543210fedcba9876543210")[1], std::string("Tabs and new lines go"));
    mkdir("build", 0755);
    const std::string path = "build/away-test.away";
    ASSERT_TRUE(writeAwayFile(path, all));
    const auto mine = readAwayFile(path, "0123456789abcdef0123456789abcdef");
    ASSERT_TRUE(mine.has_value());
    EXPECT_TRUE(*mine == all.at("0123456789abcdef0123456789abcdef"));
    EXPECT_TRUE(!readAwayFile(path, "00000000000000000000000000000000").has_value());
    EXPECT_TRUE(!readAwayFile("build/no-such.away", "0123456789abcdef0123456789abcdef").has_value());
}
