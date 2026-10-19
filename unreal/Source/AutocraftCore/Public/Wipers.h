// The visor's wiper (docs/weather.md, "Rain on the visor and the wipers"):
// one blade on a pivot below the view's bottom centre, sweeping an arc out
// from its park on the left, over the top to the right and back. Automatic
// on the rain rate fed to the glass: off, intermittent, steady, fast; a
// sweep early when the water on the glass covers too much of it; two last
// sweeps when the rain stops, then the park.
//
// Also the glass's water: how much rain has landed on it (heavier rain
// lands disproportionately more), drying slowly when none lands, kept as a
// short history so the visor material can tell, for any spot, how much has
// landed since the blade last crossed it (`water`, `lastWipe`).
//
// Presentation only: the sim never reads it. Times are seconds on one
// clock of the caller's (the world's time); angles radians from straight up,
// positive to the right.
#pragma once

#include "AutocraftCoreApi.h"

#include <array>
#include <cstdint>
#include <limits>

namespace ac {

enum class WiperMode : uint8_t { off, intermittent, steady, fast };

/// One sweep: out from the park to the far stop, and back.
struct WiperSweep {
    /// Sweeps started before this one (its number).
    int64_t number = -1;
    double start = 0;
    /// Seconds each way.
    double each = 0;
    /// The glass nearly dry as it began (the rubber squeaks).
    bool dry = false;
    /// The glass's whole water (`Wipers::all`) as it began and as it ended
    /// (unset until then), to tell how much landed since.
    double allAtStart = 0, allAtEnd = -1;
    double end() const { return start + 2 * each; }
};

class AUTOCRAFTCORE_API Wipers {
public:
    // The modes on the rain rate (0-1): off under 0.15, intermittent under
    // 0.45, steady under 0.8, fast from there.
    static constexpr double IntermittentFrom = 0.15;
    static constexpr double SteadyFrom = 0.45;
    static constexpr double FastFrom = 0.8;
    /// From one sweep's start to the next: intermittent and steady (fast
    /// goes on without a pause, out and back in `2 * FastEach`).
    static constexpr double IntermittentEvery = 5.0;
    static constexpr double SteadyEvery = 1.8;
    /// A sweep's time each way: the slow speed (intermittent, steady, the
    /// last sweeps) and the fast one.
    static constexpr double SlowEach = 0.7;
    static constexpr double FastEach = 0.4;

    /// The arc: parked lying flat to the left (out of sight below the
    /// view), out to lying flat on the right.
    static constexpr double ParkAngle = -1.5707963267948966;
    static constexpr double FarAngle = 1.5707963267948966;

    /// Water on the glass, in rain-seconds (the rate times seconds), each
    /// second's rain weighted by `1 + HeavyLands * rate^2`: a storm lands
    /// three times what its rate says, so the glass builds up between even
    /// fast sweeps.
    static constexpr double HeavyLands = 2.0;
    /// Water drying off the glass when none lands (time constant, s).
    static constexpr double DrySeconds = 180.0;
    /// How much of the glass the water covers: `1 - exp(-water /
    /// CoverWater)`; half at 6 (rain at 0.5 for 8 s, a storm for 2 s).
    static constexpr double CoverWater = 8.66;
    /// The blade starts a sweep early when the glass in its middle is this
    /// covered.
    static constexpr double SweepEarlyAt = 0.5;
    /// The last sweeps when the rain stops.
    static constexpr int FinalSweeps = 2;
    /// A sweep squeaks (dry) when, as it begins, the rain landing is under
    /// `DryRain` and the glass under `DryCover` covered.
    static constexpr double DryRain = 0.05;
    static constexpr double DryCover = 0.1;
    /// The thin smear the blade leaves behind dries in this long (s).
    static constexpr double SmearSeconds = 0.4;

    /// The ages (s) at which `history` reports the water landed lately:
    /// the material reads how hard it rained a few seconds ago from them.
    static constexpr std::array<double, 4> HistoryAges{0.5, 1, 2, 4};
    /// Never wiped (outside the arc, or before the first sweep).
    static constexpr double Never = -std::numeric_limits<double>::infinity();

    static WiperMode modeFor(double rate);
    /// Start to start (s); 0 when off.
    static double cadenceOf(WiperMode mode);
    /// Seconds each way.
    static double eachOf(WiperMode mode);
    /// The blade's angle `t` seconds into a sweep of `each` seconds a way
    /// (the park before and after): eased like a crank, slow at the stops.
    static double sweepAngle(double t, double each);
    /// How far out (0 park - 1 far stop) the ease puts angle `a`, as the
    /// fraction of the way-out time (clamped to the arc).
    static double reachTime(double a);
    /// `1 + HeavyLands * rate^2`.
    static double landing(double rate);
    static double coverOf(double water);

    /// Advance to `now` with the rain rate fed to the glass (0 when it is
    /// sheltered). Time never runs backwards; a first call only starts.
    void update(double now, double rate);

    WiperMode mode() const { return mode_; }
    /// The blade's angle now (`ParkAngle` when parked).
    double angle() const;
    /// Which way the blade moves: 1 out, -1 back, 0 parked.
    int moving() const;
    bool parked() const { return moving() == 0; }
    /// Last sweeps still to come before the park (0 when not stopping).
    int finalsLeft() const { return finals_; }
    /// The latest sweep and the one before it (`number` -1: none).
    const WiperSweep& latest() const { return sweeps_[0]; }
    const WiperSweep& previous() const { return sweeps_[1]; }
    /// Passes started (two a sweep, out and back): the sound plays one a pass.
    int64_t passes() const { return passes_; }
    /// The latest pass's sweep was dry (the squeak).
    bool dry() const { return sweeps_[0].dry; }

    /// When the blade last crossed angle `a`, at or before now (`Never`).
    double lastWipe(double a) const;
    /// Water landed (rain-seconds, dried) since the sweep `s` began, and
    /// since it ended (or since now, while it runs: 0).
    double waterSinceStart(const WiperSweep& s) const;
    double waterSinceEnd(const WiperSweep& s) const;
    /// Water landed at angle `a` since the blade last crossed it (`all`
    /// if it never did): the landing through the sweep taken as even.
    double waterSinceWipe(double a) const;
    /// Water landed (rain-seconds, dried) in the last `age` seconds (at
    /// most `HistoryAges.back()` back).
    double water(double age) const;
    /// Water on the glass in the middle of the arc (since the blade last
    /// crossed straight up), and how much of the glass it covers.
    double middleWater() const { return waterSinceWipe(0.0); }
    double coverage() const { return coverOf(middleWater()); }
    /// `water` at `HistoryAges`.
    std::array<double, HistoryAges.size()> history() const;
    /// All the water that ever landed, dried: what a spot the blade never
    /// reaches holds.
    double all() const { return all_; }
    double now() const { return now_; }
    /// The rain rate fed last.
    double rate() const { return rate_; }

private:
    /// Water landed per slice of `SliceSeconds`, newest first, each dried
    /// to now (a few seconds of them); `all_` the whole, dried.
    static constexpr double SliceSeconds = 0.125;
    static constexpr int Slices = 48;
    std::array<double, Slices> slices_{};
    /// The slice now filling: its start time.
    double sliceStart_ = 0;
    double all_ = 0;
    double now_ = 0;
    double rate_ = 0;
    bool started_ = false;
    WiperMode mode_ = WiperMode::off;
    std::array<WiperSweep, 2> sweeps_{};
    int64_t count_ = 0;
    int64_t passes_ = 0;
    int finals_ = 0;

    void startSweep(double at, double each);
    bool sweeping() const;
};

} // namespace ac
