// The client's prediction of the machine it drives (docs/networking.md,
// "Prediction of the driven machine"). Each input the client sends moves its
// copy of the driven unit at once, with the core's own pilot code (facing
// and walking: `Simulation::pilotFace`, `pilotWalk`), and waits until the
// server says it used it. Each snapshot puts the copy back where the server
// had the unit after the last input it used (`Mirror::lastInputTick`) and
// runs the inputs it has not used yet again. What that moves the drawn unit
// is not shown as a jump: it is carried as an offset that eases away over a
// few frames, unless it is past `snap` (a teleport, a Dropship's drop).
//
// Only walking and facing are predicted: busy work (drilling, a jump, a
// Derrick), the keys and what they do come from the server's snapshots.
//
// In a world, a region change (a new visit: `PilotInput::visit`, the
// mirror's `visitOf(game)`) starts it over: the server drops the inputs of
// an earlier visit, so they are not run again on the new region's unit.
#pragma once

#include "AutocraftNetApi.h"
#include "NetMirror.h"
#include "NetProtocol.h"

#include <cstdint>
#include <deque>
#include <optional>

namespace ac::net {

struct PredictorSettings {
    /// The server's step, seconds (one input each).
    double step = 1.0 / 60;
    /// The eased offset's time constant, seconds: about a third left after
    /// one, nearly none after three.
    double ease = 0.05;
    /// A correction farther than this, in cells, is not eased: a teleport.
    double snap = 1.5;
    /// The same for the heading, radians.
    double snapTurn = 1.0;
    /// Inputs kept, at most (two seconds of them).
    size_t keep = 120;
};

class AUTOCRAFTNET_API Predictor {
public:
    explicit Predictor(const PredictorSettings& settings = {}) : settings_(settings) {}

    /// Forgets the unit and the inputs.
    void reset();

    /// An input just sent (stamped with its tick): kept until the server
    /// used it, and applied to the driven unit at once.
    void input(Mirror& mirror, const PilotInput& in);
    /// After the snapshots of a frame are in: the driven unit as the server
    /// had it after its last input used, the inputs after it run again. A
    /// new driven unit starts from the server's; none stops the prediction.
    void reconcile(Mirror& mirror);
    /// One frame of `dt` seconds: the offset eases away.
    void ease(double dt);
    /// Writes the drawn unit (the prediction and the offset) over the
    /// mirror's copy in its state, which the game draws. The next snapshot
    /// writes the server's back.
    void draw(Mirror& mirror) const;

    /// The predicted unit (nil when nothing is driven).
    const Unit* predicted() const { return unit_ ? &*unit_ : nullptr; }
    /// As drawn: the prediction and what is left of the offset.
    std::optional<Unit> shown() const;
    /// Inputs sent and not yet used by the server.
    size_t pending() const { return pending_.size(); }

    struct Stats {
        /// Snapshots that corrected the prediction, and how far they moved
        /// it (cells): their sum and the largest.
        int64_t corrections = 0;
        double errorSum = 0;
        double errorMax = 0;
        /// Corrections past `snap`, drawn as jumps.
        int64_t snaps = 0;
        /// How far the eased offset moved the drawn unit, all frames
        /// summed (cells), and the frames.
        double easedSum = 0;
        int64_t frames = 0;
    };
    const Stats& stats() const { return stats_; }
    void clearStats() { stats_ = {}; }

private:
    void advance(Mirror& mirror, Unit& u, const PilotInput& in) const;

    PredictorSettings settings_;
    std::optional<Unit> unit_;
    /// The visit `unit_` is of, and of the newest input.
    int64_t unitVisit_ = 0;
    int64_t visit_ = 0;
    std::deque<PilotInput> pending_;
    int64_t seenSnapshot_ = -1;
    Vec2 offset_{0, 0};
    double turn_ = 0;
    Stats stats_;
};

} // namespace ac::net
