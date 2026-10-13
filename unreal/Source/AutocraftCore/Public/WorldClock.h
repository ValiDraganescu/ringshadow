// The MMORPG world's clock (docs/mmorpg-m6.md, C1): one per world, kept by
// the server and saved with the world, the same hour in every region. A
// world's day is longer than a game's (`Rules::dayLength`) and its light
// longer than its night: the sun is up (`Rules::sunrise` to `Rules::sunset`)
// for `lightShare` of the day, the night runs through the rest faster. The
// sun's arc, the dark and the sight at night are the game's own
// (`Rules::sunHeight`, `dark`, `sightScale`), read at this clock's hour.
// The server and the client both read it: the server holds each region's
// hour at it (`GameState::heldHour`), the client's sky draws it.
#pragma once

#include "AutocraftCoreApi.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace ac {

struct AUTOCRAFTCORE_API WorldClock {
    /// One day and night, seconds of real time: 40 minutes, about 30 of
    /// them light and 10 night.
    static constexpr double dayLength = 2400.0;
    /// The share of the day from sunrise to sunset.
    static constexpr double lightShare = 0.75;
    /// A new world starts in the morning.
    static constexpr double startHour = 8.0;

    /// The hour (0-24) `time` seconds into a world whose day is `day`
    /// seconds long; `held` (0-24) holds it.
    static double hourAt(double time, double day = dayLength, std::optional<double> held = std::nullopt);
    /// The first world time at or after `from` whose hour is `hour`.
    static double timeOf(double hour, double from = 0, double day = dayLength);

    /// The weather's seed for a world (its id hashed): every server of the
    /// world, and every restart, has the same weather. 32 bits (it goes on
    /// the wire whole).
    static uint32_t weatherSeed(std::string_view worldId);
};

} // namespace ac
