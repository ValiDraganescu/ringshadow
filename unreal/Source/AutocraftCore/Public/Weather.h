// The weather (docs/weather.md): sun, broken cloud, rain and storms on the
// game clock. Presentation only: the sim never reads it.
#pragma once

#include "AutocraftCoreApi.h"

#include <cstdint>
#include <vector>

namespace ac {

enum class WeatherState : uint8_t { clear, cloudy, rain, storm };

/// One spell of a state, game seconds [start, end).
struct WeatherSpell {
    WeatherState state = WeatherState::clear;
    double start = 0, end = 0;
};

/// A lightning strike: when it flashes, how far (km) and in which
/// direction (radians, 0 = +x, counter-clockwise), how strong (0-1), and
/// when its thunder arrives (`time + distance / 0.343 km/s`).
struct LightningStrike {
    double time = 0, distanceKm = 0, azimuth = 0, power = 0;
    double thunderTime() const { return time + distanceKm / 0.343; }
};

/// The weather at a moment. Every quantity is continuous in time.
struct WeatherSample {
    /// Cloud cover (0 clear sky - 1 overcast), the material's coverage.
    double cover = 0.35;
    /// Rain rate, 0 dry - 1 the heaviest.
    double rain = 0;
    /// How dark it is under the cloud (0 none - 1 a storm's).
    double gloom = 0;
    /// Storminess: dark towers, gusts, lightning (0-1).
    double storm = 0;
    /// Wind speed, m/s, and how far the air has moved since the start (km).
    double wind = 12, windKm = 0;
    /// How wet the ground is (0 dry - 1 soaked): lags the rain.
    double wetness = 0;
    /// Lightning light now (0 none - about 1 a close strike).
    double flash = 0;
    /// The spell now and how far into its change from the last (0-1).
    WeatherState state = WeatherState::clear;
    double change = 1;
};

/// The weather as a pure function of game time and a seed: the same seed
/// and time give the same sample, whatever was asked before (the
/// integrated parts are rebuilt from checkpoints when time goes back).
class AUTOCRAFTCORE_API Weather {
public:
    /// The change from one spell to the next, game seconds.
    static constexpr double ChangeSeconds = 45;
    /// The first spell is clear and lasts at least this long.
    static constexpr double FirstClearSeconds = 300;

    explicit Weather(uint64_t seed);

    uint64_t seed() const { return seed_; }

    /// Hold the weather at a state (0 clear .. 3 storm; fractions blend
    /// between neighbours); negative follows the schedule.
    void hold(double state);
    double held() const { return held_; }

    WeatherSample sample(double time);
    /// The spell running at `time` (the schedule, ignoring `hold`).
    WeatherSpell spellAt(double time);
    /// Strikes flashing in [t0, t1) (the schedule's storminess, or the held one).
    std::vector<LightningStrike> strikes(double t0, double t1);
    /// Strikes whose thunder arrives in [t0, t1).
    std::vector<LightningStrike> thunder(double t0, double t1);

    /// A strike's flicker `age` seconds after it (0 before and after a
    /// second): the stroke, a dip, the return stroke, a fading glow. The
    /// flash and the drawn bolt both follow it.
    static double flicker(double age);

    /// Each state's targets (docs/weather.md's table).
    static double coverOf(WeatherState s);
    static double rainOf(WeatherState s);
    static double gloomOf(WeatherState s);
    static double stormOf(WeatherState s);
    static double windOf(WeatherState s);

private:
    struct Targets { double cover, rain, gloom, storm, wind; };
    struct Checkpoint { double time, windKm, wetness; };

    uint64_t seed_;
    double held_ = -1;
    std::vector<WeatherSpell> spells_;
    uint64_t spellRng_;
    /// Every 60 s of integration (wind offset, wetness), from 0; `last_`
    /// is the latest whole second reached.
    std::vector<Checkpoint> checkpoints_;
    Checkpoint last_{0, 0, 0};

    void extendTo(double time);
    static Targets targetsOf(WeatherState s);
    /// The eased quantities (no integration) at `time`.
    Targets eased(double time);
    /// Advances `c` to `time` in steps of at most 1 s.
    void integrate(Checkpoint& c, double time);
    Checkpoint integratedAt(double time);
    double flashAt(double time);
    void strikesIn(int64_t slot0, int64_t slot1, std::vector<LightningStrike>& out);
};

} // namespace ac
