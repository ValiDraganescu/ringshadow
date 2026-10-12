// Night closes sight in: the day's clock (`Rules::hourAt`, the one the sky
// runs on) and how far everything sees at night (`Rules::sightScale`).
// Hand-built positions and a fraction of a second of simulation each.
#include "VisionHelpers.h"

#include "NavGrid.h"
#include "Rules.h"

#include <cmath>

using namespace ac;
using visiontest::add;
using visiontest::homeMap;

namespace {

/// The game time `hour` falls on in a game's first day.
double timeOf(double hour) { return std::fmod(hour - Rules::startHour + 24.0, 24.0) / 24.0 * Rules::dayLength; }

/// A walkable spot 30 cells or more from every building.
Vec2 openGround(const Simulation& sim) {
    for (double y = 4; y < 200; y += 2) {
        for (double x = 4; x < 300; x += 2) {
            const Vec2 p(x, y);
            if (!sim.nav->walkable(p) || !sim.nav->walkable(p + Vec2(8, 0))) continue;
            bool clear = true;
            for (const auto& b : sim.state.structures) clear = clear && distance(b.position, p) > 30;
            if (clear) return p;
        }
    }
    return Vec2::zero;
}

/// Does a blue Dropship (it sees every level; 11 cells by day) see a red
/// Prospector 8 cells off at `hour`?
bool seesAt(double hour) {
    GameState s = GameState::new_(homeMap());
    s.units.clear();
    const Simulation probe(s, homeMap());
    const Vec2 at = openGround(probe);
    EXPECT_TRUE(at != Vec2::zero);
    s.time = timeOf(hour);
    add(s, Unit::Kind::dropship, at, 0);
    const int64_t red = add(s, Unit::Kind::prospector, at + Vec2(8, 0), 1);
    Simulation sim(s, homeMap());
    for (int k = 0; k < 6; ++k) sim.step(1.0 / 30);
    return sim.sees(0, red);
}

} // namespace

/// A game starts at 08:00 and a day lasts `dayLength`; a held hour holds.
TEST(night_theClock) {
    EXPECT_NEAR(Rules::hourAt(0), 8.0, 1e-9);
    EXPECT_NEAR(Rules::hourAt(Rules::dayLength / 2), 20.0, 1e-9);
    EXPECT_NEAR(Rules::hourAt(Rules::dayLength), 8.0, 1e-9);
    EXPECT_NEAR(Rules::hourAt(100, 1200), 10.0, 1e-9);
    EXPECT_NEAR(Rules::hourAt(100, 600, 21.0), 21.0, 1e-9);
}

/// Full sight with the sun up, half at night, between at dusk.
TEST(night_sightHalvesAtNight) {
    EXPECT_NEAR(Rules::sightScale(12.0), 1.0, 1e-9);
    EXPECT_NEAR(Rules::sightScale(0.0), Rules::nightSight, 1e-9);
    const double dusk = Rules::sightScale(Rules::sunset);
    EXPECT_TRUE(dusk > Rules::nightSight && dusk < 1.0);
    EXPECT_TRUE(Rules::sunHeight(12.0) > 30 && Rules::sunHeight(0.0) < -30);
}

/// What is seen at noon 8 cells off is lost at midnight.
TEST(night_aUnitSeesLessAtNight) {
    EXPECT_TRUE(seesAt(12.0));
    EXPECT_TRUE(!seesAt(0.0));
}

/// A held hour holds the sight too (`ac.Hour` for shots).
TEST(night_aHeldHourHoldsTheSight) {
    GameState s;
    s.time = timeOf(12.0);
    s.heldHour = 0.0;
    EXPECT_NEAR(Rules::sightScale(Rules::hourAt(s.time, s.dayLength, s.heldHour)), Rules::nightSight, 1e-9);
}
