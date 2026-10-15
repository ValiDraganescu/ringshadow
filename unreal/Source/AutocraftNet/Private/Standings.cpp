#include "Standings.h"

#include "AwayReport.h"

#include <algorithm>
#include <tuple>

namespace ac::net {

bool standingBefore(const WeekStanding& a, const WeekStanding& b) {
    return std::make_tuple(-a.ore, -a.bugs, -a.waves, -a.built, a.callsign) <
           std::make_tuple(-b.ore, -b.bugs, -b.waves, -b.built, b.callsign);
}

std::string ordinal(int64_t n) {
    const int64_t teen = n % 100;
    const char* tail = "th";
    if (teen < 11 || teen > 13) {
        if (n % 10 == 1) tail = "st";
        else if (n % 10 == 2) tail = "nd";
        else if (n % 10 == 3) tail = "rd";
    }
    return std::to_string(n) + tail;
}

std::string standingLine(int64_t rank, const WeekStanding& s) {
    auto count = [](int64_t n, const char* one, const char* many) { return awayNumber(n) + " " + (n == 1 ? one : many); };
    std::string lead = "no work counted";
    if (s.ore > 0) lead = awayNumber(s.ore) + " ore";
    else if (s.bugs > 0) lead = count(s.bugs, "bug", "bugs");
    else if (s.waves > 0) lead = count(s.waves, "wave held", "waves held");
    else if (s.built > 0) lead = count(s.built, "building", "buildings");
    return ordinal(rank) + " · " + lead + " this week";
}

std::optional<std::string> standingLineOf(const StandingsMessage& m, const std::string& callsign, bool frontier) {
    for (const WeekStanding& s : m.rows)
        if (s.callsign == callsign && s.frontier == frontier) return standingLine(s.rank, s);
    return std::nullopt;
}

std::string weekTopLine(const std::string& who, int64_t ore) {
    if (who.empty() || ore <= 0) return "No ore was hauled this week";
    return "This week's top hauler: " + who + ", " + awayNumber(ore) + " ore";
}

std::string weekLeft(double seconds) { return awayRan(std::max(60.0, seconds)) + " left"; }

} // namespace ac::net
