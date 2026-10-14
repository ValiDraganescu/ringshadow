// A pad flight's price in Metallic Hydrogen (docs/prospector-progression.md,
// "MH sinks"; F1): a base for the lift-off and a share for each step across
// the world's grid (the regions' columns and rows, as the World message
// and the world file have them). The server takes it at lift-off
// (`WorldMap::fly`); the client's flight menu shows it beside each
// destination and refuses with the same words before it asks.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace ac {

struct Flight {
    /// MH for any flight, a hop on the same region's ground included.
    static constexpr int64_t baseHydrogen = 10;
    /// MH more for each step across the grid (north, south, east or west).
    static constexpr int64_t stepHydrogen = 5;

    /// Grid steps between two regions: columns plus rows apart.
    static int64_t steps(int64_t fromColumn, int64_t fromRow, int64_t toColumn, int64_t toRow) {
        return std::llabs(toColumn - fromColumn) + std::llabs(toRow - fromRow);
    }

    /// The price of a flight `steps` grid steps long.
    static int64_t hydrogen(int64_t steps) { return baseHydrogen + stepHydrogen * (steps > 0 ? steps : 0); }

    /// Why a wallet of `have` MH can't pay `price` (none: it can).
    static std::optional<std::string> refusal(int64_t price, int64_t have) {
        if (have >= price) return std::nullopt;
        return "Not enough MH to fly: " + std::to_string(price) + " MH, you have " + std::to_string(have > 0 ? have : 0);
    }
};

} // namespace ac
