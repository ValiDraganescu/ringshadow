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

/// The Dropship's flight (F2): it comes down onto the Landing Pad, the
/// Prospector walks up its ramp, it lifts off, banks through a couple of
/// turns, dashes across the sky, and comes in over the target pad. The
/// pilot is aboard (out of every region, untouchable) from the call to the
/// touch down. The server moves the pilot into the target region when the
/// approach starts (`switchAt`), and puts the Prospector on the target pad
/// at `total`; the client steps it out after (`stepOut`).
struct Ferry {
    /// The Dropship coming down onto the pad.
    static constexpr double descend = 3.0;
    /// The Prospector walking up the ramp, the ramp closing.
    static constexpr double board = 1.5;
    /// Up off the pad, climbing.
    static constexpr double lift = 2.5;
    /// The banking turns, the giant swinging through the view.
    static constexpr double turns = 3.0;
    /// The dash across the sky: a base and a share a grid step, at most
    /// `dashMost`.
    static constexpr double dashBase = 2.0;
    static constexpr double dashStep = 1.0;
    static constexpr double dashMost = 6.0;
    /// Slowing, coming down onto the target pad.
    static constexpr double approach = 4.0;
    /// After the touch down: the ramp down, the Prospector walking out
    /// (the client's; control is back at its end).
    static constexpr double stepOut = 1.5;

    struct Plan {
        double descend = Ferry::descend;
        double board = Ferry::board;
        double lift = Ferry::lift;
        double turns = Ferry::turns;
        double dash = dashBase;
        double approach = Ferry::approach;
        /// Seconds from the call to each stage's start.
        double boardAt() const { return descend; }
        double liftAt() const { return descend + board; }
        double turnsAt() const { return liftAt() + lift; }
        double dashAt() const { return turnsAt() + turns; }
        double approachAt() const { return dashAt() + dash; }
        /// The touch down.
        double total() const { return approachAt() + approach; }
        bool operator==(const Plan&) const = default;
    };

    /// The plan of a flight `steps` grid steps long; Short hop's share off
    /// the dash (`shortHop`: −0.2 a rank, as `SkillStat::flightTime`), and
    /// Ringwalker's `faster` (0.3: every stage 30% faster).
    static Plan plan(int64_t steps, double shortHop = 0, double faster = 0) {
        Plan p;
        p.dash = std::min(dashMost, dashBase + dashStep * double(steps > 0 ? steps : 0)) * std::max(0.0, 1 + shortHop);
        if (faster > 0) {
            const double k = 1 / (1 + faster);
            p.descend *= k;
            p.board *= k;
            p.lift *= k;
            p.turns *= k;
            p.dash *= k;
            p.approach *= k;
        }
        return p;
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
