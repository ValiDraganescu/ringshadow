// Realm standings (S4, docs/mmorpg-play.md "Standings"): each pilot's work
// over the realm week (seven world days), humans and frontier pilots alike,
// from the server's own bookkeeping (WorldStandings.cpp): ore delivered,
// bugs felled, canyon waves held, buildings raised. The server sends each
// pilot the table in a Standings message of its own when it changed; the
// REALM tab lists it, the map's tooltips take a pilot's rank from it. The
// wording is here, one for the server's news and the game. Engine-free.
#pragma once

#include "AutocraftNetApi.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac::net {

/// One pilot's week: its callsign ("Vesper", "Pilot 1A2B"), a frontier
/// pilot or a human, and its four counts.
struct WeekStanding {
    std::string callsign;
    bool frontier = false;
    /// Ore unloaded at the realm's buildings (its mercenaries' deliveries
    /// counted to it), before the tax.
    int64_t ore = 0;
    /// Bugs of the Brood its Prospector or its defences killed.
    int64_t bugs = 0;
    /// Canyon waves beaten that it fought (paid a share).
    int64_t waves = 0;
    /// Buildings of its own finished.
    int64_t built = 0;
    /// Its place in the week's order (`standingBefore`), 1 the first.
    int64_t rank = 0;
    bool operator==(const WeekStanding&) const = default;
};

/// Server → client, reliable: the week's standings as one pilot sees them.
struct StandingsMessage {
    /// The week's start and end on the world clock (`Sky::time`).
    double weekStart = 0;
    double weekEnds = 0;
    /// Ranked (`standingsRank`): every pilot with a count this week, at most
    /// `kStandingsMost`, and the viewer's own always.
    std::vector<WeekStanding> rows;
    /// The viewer's row in `rows` (nil: none, a pilot never kept).
    std::optional<int64_t> you;
    bool operator==(const StandingsMessage&) const = default;
};

/// The most rows a Standings message carries (the viewer's past them).
inline constexpr size_t kStandingsMost = 64;
/// The realm week, world days.
inline constexpr int64_t kWeekDays = 7;

/// Most ore first, then bugs, waves, buildings, then by callsign.
AUTOCRAFTNET_API bool standingBefore(const WeekStanding& a, const WeekStanding& b);
/// "1st", "2nd", "3rd", "4th", "11th", "22nd".
AUTOCRAFTNET_API std::string ordinal(int64_t n);
/// The map tooltip's line for a pilot ranked `rank` (1 the first): its rank
/// and its leading stat, ore when it hauled any, else the first count it
/// has: "3rd · 4,200 ore this week", "5th · 12 bugs this week", "8th · no
/// work counted this week".
AUTOCRAFTNET_API std::string standingLine(int64_t rank, const WeekStanding& s);
/// The tooltip line of the pilot called `callsign` (a frontier pilot or a
/// human) in `m` (nil: not in it).
AUTOCRAFTNET_API std::optional<std::string> standingLineOf(const StandingsMessage& m, const std::string& callsign, bool frontier);
/// The week's end, said by the toast and the away report: "This week's top
/// hauler: Vesper, 18,400 ore"; nobody hauled: "No ore was hauled this
/// week".
AUTOCRAFTNET_API std::string weekTopLine(const std::string& who, int64_t ore);
/// The week's time left, world seconds: "3 d 4 h left", "2 h 10 m left".
AUTOCRAFTNET_API std::string weekLeft(double seconds);

} // namespace ac::net
