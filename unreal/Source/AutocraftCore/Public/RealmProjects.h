// Realm projects (R2, docs/mmorpg-play.md "Realm projects"): what the realm
// wallet buys that the whole realm feels. Four kinds, each one world day
// long, started from the Citadel's REALM tab by any human pilot when the
// realm wallet covers the price (the wallet pays, not the pilot); one of each
// kind at a time, several kinds together. The server owns them and applies
// their effects; the client shows them from the Progress message.
#pragma once

#include "AutocraftCoreApi.h"

#include <cstdint>

namespace ac {

struct AUTOCRAFTCORE_API RealmProject {
    enum class Kind : uint8_t { beacon, relay, armoury, dockCrew };
    static constexpr int count = 4;
    /// A project lasts this many world days.
    static constexpr double days = 1;
    /// Each repeat of the same kind costs a fifth more.
    static constexpr double growth = 1.2;

    struct Price {
        int64_t ore = 0;
        int64_t hydrogen = 0;
        bool operator==(const Price&) const = default;
    };
    /// The first one's price: 2 to 4 hours of one mining pilot's tax
    /// (docs/balance.md: 936 to 1,218 ore an hour), and some MH.
    static Price base(Kind k);
    /// After `runs` of that kind: × `growth` each, ore to 50, MH to 10.
    static Price price(Kind k, int64_t runs);

    static const char* title(Kind k);
    /// "beacon", "relay", "armoury", "dock-crew" (saves, news, sound keys).
    static const char* slug(Kind k);
    /// What it does, a few words for the tab.
    static const char* effect(Kind k);

    /// Beacon: Dropship fares divided by this realm-wide (a Spacedock's half
    /// on top: a quarter).
    static constexpr int64_t fareDivisor = 2;
    /// Relay: every pilot's night sight widened by half (the headlamp's
    /// wedge untouched).
    static constexpr double relaySight = 1.5;
    /// Armoury: every turret of the realm fights as this many marks up in
    /// each stat (to the marks' cap).
    static constexpr int64_t armouryMarks = 1;
    /// Dock Crew: the mercenaries work the pace model's hour over this many
    /// hours (1/12 of the pilot's rate, not 1/24).
    static constexpr double dockCrewHours = 12;
};

} // namespace ac
