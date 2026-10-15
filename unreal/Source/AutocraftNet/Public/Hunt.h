// The hunt (N4, docs/mmorpg-play.md "The hunt"): the Matriarch as the
// realm's raid. A human calls THE HUNT on the Citadel's REALM tab (free, once
// a world day realm-wide); the frontier defenders and the guard mercenaries
// walk (or fly the Dropship) to the ring round her nest and hold there; at
// two minutes, or when the caller steps inside the ring, whichever is later,
// they go in together. The server sends each pilot the hunt in a Hunt message
// of its own when it changed (WorldHunt.cpp); the REALM tab, the map and the
// toasts read it. The wording is here, one for the server and the game.
// Engine-free.
#pragma once

#include "AutocraftNetApi.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ac::net {

/// One of the hunt's party: a frontier defender, a guard mercenary (`owner`
/// its hirer's callsign), or a human in the fight.
struct HuntHunter {
    enum class Kind : uint8_t { frontier, merc, human };
    Kind kind = Kind::frontier;
    std::string name;
    std::string owner;
    /// A frontier pilot's paint (0 for the others).
    int32_t paint = 0;
    /// Where it is: the region (index in the world; nil on its way through
    /// an opening or aboard the Dropship) and its point in the region's
    /// frame (`Built::regionAt`).
    std::optional<int64_t> region;
    double x = 0;
    double z = 0;
    /// At the ring (or inside it, in the assault); down (its Prospector fell).
    bool there = false;
    bool down = false;
    bool operator==(const HuntHunter&) const = default;
};

/// Server → client, reliable: the realm's hunt as one pilot sees it.
struct HuntMessage {
    /// none: no hunt on (`refusal` says whether one can be called);
    /// gathering: the party walks to the ring; assault: they went in;
    /// won, failed: the last hunt's end, told until the next call.
    enum class State : uint8_t { none, gathering, assault, won, failed };
    State state = State::none;
    /// Who called it (a human's callsign).
    std::string caller;
    /// Her nest: the region (index; nil: the world has none), its name, her
    /// point in the region's frame, the ring's radius (cells).
    std::optional<int64_t> region;
    std::string regionName;
    double x = 0;
    double z = 0;
    double ring = 30;
    /// World clock: called, the gathering's end (two minutes after), the
    /// charge (0: not yet), the hunt's end by time (ten minutes after the
    /// call), its end (won or failed; 0: on).
    double calledAt = 0;
    double gatherEnds = 0;
    double chargedAt = 0;
    double failsAt = 0;
    double endedAt = 0;
    /// When the next call opens (world clock; 0: open now).
    double nextCall = 0;
    /// Why this pilot can't call now ("" it can): "She is down: back in 2 d
    /// 4 h", "A hunt is on", "The day's hunt was called: next in 6 h 10 m".
    std::string refusal;
    std::vector<HuntHunter> party;
    bool operator==(const HuntMessage&) const = default;
};

/// The most of a party a Hunt message carries.
inline constexpr size_t kHuntPartyMost = 64;

/// The REALM tab's state line: "Not called today", "Gathering at the ring
/// in Slag Pockets: 3 of 5 there, 1 m 20 s left", "The assault on Slag
/// Pockets", "She fell to the hunt called by Pilot 1A2B", "The hunt failed".
AUTOCRAFTNET_API std::string huntLine(const HuntMessage& m, double now);
/// How many of the party are at the ring (and not down).
AUTOCRAFTNET_API int64_t huntThere(const HuntMessage& m);
/// The toast's lines by the news kind `hunt`'s id: "called" (`by` the
/// caller, `where` the nest's region), "gathered", "charge", "won",
/// "failed".
AUTOCRAFTNET_API std::string huntNewsTitle(const std::string& id);
AUTOCRAFTNET_API std::string huntNewsLine(const std::string& id, const std::string& by, int64_t value);

} // namespace ac::net
