// A flight's price in Metallic Hydrogen (docs/prospector-progression.md,
// "MH sinks"; F1): a base for the lift-off and a share for each step across
// the world's grid (the regions' columns and rows, as the World message
// and the world file have them). The server takes it when the pilot calls
// the Dropship (`WorldMap::fly`); the client's flight menu shows it beside
// each destination and refuses with the same words before it asks. F2: half
// from a Spacedock, nothing with Ringwalker.
//
// The Dropship's flight (F2, docs/mmorpg-m8.md; `Ferry`): its stages'
// seconds, the server's clock for the switch into the target region and the
// touch down, the client's for the picture.
#pragma once

#include "Ferry.h"

#include <algorithm>
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

    /// F2: what a flight `steps` long costs from a Spacedock (`spacedock`:
    /// half, rounded up) or with Ringwalker (`ringwalker`: nothing). R2:
    /// while the realm's Beacon burns (`beacon`) half again, so a quarter
    /// from a Spacedock, rounded up.
    static int64_t price(int64_t steps, bool spacedock, bool ringwalker, bool beacon = false) {
        if (ringwalker) return 0;
        const int64_t whole = hydrogen(steps);
        const int64_t by = (spacedock ? 2 : 1) * (beacon ? 2 : 1);
        return (whole + by - 1) / by;
    }

    /// Why a wallet of `have` MH can't pay `price` (none: it can).
    static std::optional<std::string> refusal(int64_t price, int64_t have) {
        if (have >= price) return std::nullopt;
        return "Not enough MH to fly: " + std::to_string(price) + " MH, you have " + std::to_string(have > 0 ? have : 0);
    }
};

/// A jet-pack jump's price in MH (docs/prospector-progression.md, "MH
/// sinks"; F1b): the skill tree's Cliff hop burns it on each take-off, the
/// carried MH first, then the wallet. Jump jets' third rank halves it. Only
/// the MMORPG's Prospector pays: the strategy game's jumpers jump free.
struct JetJump {
    static constexpr int64_t hydrogen = 2;
    /// What a jump costs with Jump jets at `jumpJetsRank`.
    static int64_t price(int64_t jumpJetsRank) { return jumpJetsRank >= 3 ? 1 : hydrogen; }
    /// Why `have` MH can't pay `price` (none: it can).
    static std::optional<std::string> refusal(int64_t price, int64_t have) {
        if (have >= price) return std::nullopt;
        return "Not enough MH to jet: " + std::to_string(price) + " MH, you have " + std::to_string(have > 0 ? have : 0);
    }
};

} // namespace ac
