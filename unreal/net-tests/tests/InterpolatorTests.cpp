// net::Interpolator: others drawn 100 ms in the past, blended between the
// snapshots around that moment; the server's clock learned from the ticks.

#include "test.h"

#include "NetInterpolator.h"

#include <vector>

using namespace ac;

namespace {

Unit at(int64_t id, double x, double heading) {
    Unit u;
    u.id = id;
    u.position = Vec2(x, 0);
    u.heading = heading;
    u.stride = x;
    return u;
}

} // namespace

TEST(interpolator_blends_between_the_two_snapshots_around_the_moment) {
    net::Interpolator in;
    in.record(60, {at(1, 0, 0), at(2, 10, 0)});
    in.record(63, {at(1, 3, 1), at(2, 10, 0)});
    std::vector<Unit> drawn = {at(1, 3, 1), at(2, 10, 0)};
    in.apply(61.5, drawn, std::nullopt);
    EXPECT_NEAR(drawn[0].position.x, 1.5, 1e-9);
    EXPECT_NEAR(drawn[0].heading, 0.5, 1e-9);
    EXPECT_NEAR(drawn[0].stride, 1.5, 1e-9);
    EXPECT_NEAR(drawn[1].position.x, 10, 1e-9);
}

TEST(interpolator_leaves_the_driven_unit_and_never_runs_ahead) {
    net::Interpolator in;
    in.record(60, {at(1, 0, 0)});
    in.record(63, {at(1, 3, 0)});
    std::vector<Unit> drawn = {at(1, 3, 0)};
    // Past the newest: the newest, not a guess beyond it.
    in.apply(70, drawn, std::nullopt);
    EXPECT_NEAR(drawn[0].position.x, 3, 1e-9);
    // Before the oldest: the oldest.
    in.apply(50, drawn, std::nullopt);
    EXPECT_NEAR(drawn[0].position.x, 0, 1e-9);
    // The driven unit is as the newest snapshot has it.
    drawn = {at(1, 3, 0)};
    in.apply(61, drawn, int64_t(1));
    EXPECT_NEAR(drawn[0].position.x, 3, 1e-9);
}

TEST(interpolator_turns_the_short_way_and_keeps_new_units_where_they_are) {
    net::Interpolator in;
    in.record(60, {at(1, 0, 3.0)});
    in.record(63, {at(1, 0, -3.0), at(2, 5, 0)});
    std::vector<Unit> drawn = {at(1, 0, -3.0), at(2, 5, 0), at(3, 7, 0)};
    in.apply(61.5, drawn, std::nullopt);
    // 3.0 to -3.0 is 0.28 across π, not 6 the long way round.
    EXPECT_TRUE(std::abs(std::remainder(drawn[0].heading - 3.14159265358979, 2 * 3.14159265358979)) < 0.01);
    EXPECT_NEAR(drawn[1].position.x, 5, 1e-9);
    EXPECT_NEAR(drawn[2].position.x, 7, 1e-9);
}

TEST(interpolator_draws_100_ms_behind_the_servers_clock) {
    net::Interpolator in;
    EXPECT_TRUE(!in.renderTick(5).has_value());
    // Snapshots every 3 ticks, 50 ms on the way, a jittered one late.
    double now = 10;
    for (int64_t tick = 600; tick <= 900; tick += 3) {
        const double late = tick % 30 == 0 ? 0.03 : 0;
        in.heard(tick, now + 0.05 + late);
        now += 3.0 / 60;
    }
    // At the moment tick 900 arrived (on time), the server is at 900 and
    // the moment drawn is 6 ticks before it, less the way's 3.
    const double drawn = *in.renderTick(now - 3.0 / 60 + 0.05);
    EXPECT_NEAR(drawn, 894, 0.6);
    // Records older than a second go.
    for (int64_t tick = 0; tick < 300; tick += 3) in.record(tick, {at(1, double(tick), 0)});
    EXPECT_TRUE(in.records() <= 21);
}
