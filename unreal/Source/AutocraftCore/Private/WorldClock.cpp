// The MMORPG world's clock (WorldClock.h).

#include "WorldClock.h"

#include "Rules.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {

constexpr double lightHours = Rules::sunset - Rules::sunrise;

/// How far through the day `hour` is (0-1), counted from sunrise.
double phaseOf(double hour) {
    const double h = std::fmod(hour - Rules::sunrise + 48.0, 24.0);
    return h <= lightHours ? h / lightHours * WorldClock::lightShare
                           : WorldClock::lightShare + (h - lightHours) / (24.0 - lightHours) * (1.0 - WorldClock::lightShare);
}

double hourOf(double phase) {
    const double h = phase <= WorldClock::lightShare
                         ? phase / WorldClock::lightShare * lightHours
                         : lightHours + (phase - WorldClock::lightShare) / (1.0 - WorldClock::lightShare) * (24.0 - lightHours);
    return std::fmod(Rules::sunrise + h, 24.0);
}

} // namespace

double WorldClock::hourAt(double time, double day, std::optional<double> held) {
    if (held && *held >= 0) return std::fmod(*held, 24.0);
    day = std::max(1.0, day);
    double phase = std::fmod(phaseOf(startHour) + time / day, 1.0);
    if (phase < 0) phase += 1.0;
    return hourOf(phase);
}

double WorldClock::timeOf(double hour, double from, double day) {
    day = std::max(1.0, day);
    double d = phaseOf(hour) - std::fmod(phaseOf(startHour) + from / day, 1.0);
    if (d < 0) d += 1.0;
    return from + d * day;
}

uint32_t WorldClock::weatherSeed(std::string_view worldId) {
    // FNV-1a.
    uint64_t h = 1469598103934665603ull;
    for (const char c : worldId) {
        h ^= uint8_t(c);
        h *= 1099511628211ull;
    }
    return uint32_t(h ^ (h >> 32));
}

} // namespace ac
