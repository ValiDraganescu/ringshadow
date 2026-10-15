// The away report (X2, docs/mmorpg-play.md "Coming back"): what happened in
// the world while a pilot was off line, from the server's own bookkeeping
// (the ledger, `WorldLedger.cpp`), never invented. The server keeps a row
// for each thing it saw; the Progress message carries the report once, on
// the pilot's first Progress after it joins; a file beside the save carries
// its two summary lines to the home screen. The wording is here, one for the
// server's file and the client's Journal page. Engine-free.
#pragma once

#include "AutocraftNetApi.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ac::net {

enum class AwayKind : uint8_t {
    /// A mercenary's deliveries (`who` its name, `value` the ore it brought).
    merc,
    /// A frontier pilot's deed (`what` the line as KSTR said it).
    frontier,
    /// A raid reached a realm building (`what` its kind's title, `where`).
    raid,
    /// A raid took a realm building down.
    fell,
    /// A canyon held (`what` its name).
    held,
    /// A canyon fell to the Brood.
    fallen,
    /// Waves beaten at a canyon (`value` how many).
    waves,
    /// The Matriarch went down (`who` the hardest blows, if any).
    queenDown,
    /// The Matriarch is back.
    queenBack,
    /// A realm project started (`what` its title, `who` by whom).
    projectStart,
    /// A realm project ran its course.
    projectEnd,
    /// S4: the realm week ended (`who` its top hauler, `value` the ore).
    week,
    /// N4: a hunt ended (`what` "won" or "failed", `who` its caller,
    /// `where` the nest's region, `value` the party that went).
    hunt,
};

struct AwayRow {
    /// World clock seconds when it happened (its last time, for a row that
    /// adds up: deliveries, waves).
    double at = 0;
    AwayKind kind = AwayKind::merc;
    std::string who;
    std::string what;
    /// The region's name.
    std::string where;
    int64_t value = 0;
    bool operator==(const AwayRow&) const = default;
};

struct AwayReport {
    /// World seconds since the pilot left; the world's clock at its return.
    double ran = 0;
    double back = 0;
    /// The realm wallet's change meanwhile.
    int64_t realmOre = 0;
    int64_t realmHydrogen = 0;
    std::vector<AwayRow> rows;
    bool operator==(const AwayReport&) const = default;
};

/// "Slate delivered 1,240 ore", "Sorrel built a Derrick in Cold Fork",
/// "The Fork held", "A raid took the Hab Dome in Rust Hollow".
AUTOCRAFTNET_API std::string awayLine(const AwayRow& row);
/// When a row happened, before the return: "2 h 10 m ago".
AUTOCRAFTNET_API std::string awayWhen(const AwayReport& report, const AwayRow& row);
/// "6 h 12 m", "14 m", "under a minute".
AUTOCRAFTNET_API std::string awayRan(double seconds);
/// 1,240 (thousands with commas).
AUTOCRAFTNET_API std::string awayNumber(int64_t n);
/// The rows in the order they are told: the most telling first (deliveries,
/// a canyon held or fallen, a building lost, the Matriarch, then the rest),
/// the biggest and then the latest first within a kind.
AUTOCRAFTNET_API std::vector<AwayRow> awayOrdered(const std::vector<AwayRow>& rows);
/// The home screen's two lines: "Your world ran 6 h 12 m." and the first
/// `most` rows joined by " · ", with " · N more" past them (empty when
/// nothing happened).
AUTOCRAFTNET_API std::array<std::string, 2> awaySummary(const AwayReport& report, size_t most = 3);
/// KSTR's welcome line for the report, by what is in it (a key of the
/// kstrnews manifest): "welcome-mercs" (deliveries), "welcome-held" (a
/// canyon held, no building lost), "welcome-brood" (a building or a canyon
/// lost, or the Matriarch back); empty when none of those happened.
AUTOCRAFTNET_API std::string awayVoice(const AwayReport& report);

/// The summary file beside a save (`mmo/first.sqlite`: `mmo/first.away`),
/// written by the server on its idle ticks: every pilot off line's two home
/// screen lines, read by the game's home screen with no connection.
using AwaySummaries = std::map<std::string, std::array<std::string, 2>>;
AUTOCRAFTNET_API std::string awayFilePath(const std::string& save);
/// One `key<TAB>line<TAB>line` row a pilot (tabs and line ends in a line
/// become spaces).
AUTOCRAFTNET_API std::string encodeAwayFile(const AwaySummaries& summaries);
AUTOCRAFTNET_API AwaySummaries decodeAwayFile(const std::string& text);
/// Written aside and moved in, readable only by its owner. False when it
/// could not be.
AUTOCRAFTNET_API bool writeAwayFile(const std::string& path, const AwaySummaries& summaries);
AUTOCRAFTNET_API std::optional<std::array<std::string, 2>> readAwayFile(const std::string& path, const std::string& key);

} // namespace ac::net
