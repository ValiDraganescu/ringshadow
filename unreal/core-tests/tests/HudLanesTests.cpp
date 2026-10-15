// The MMORPG HUD's toast lane and banner slot (U1; HudLanes.h): three toasts
// at most, newest on top, the rest waiting; the same kind merged; gains
// added up; one banner at a time, the next queued, held under a screen.
#include "HudLanes.h"
#include "test.h"

using namespace ac;

namespace {

ToastLane::Toast toast(const std::string& title, const std::string& sub = "") {
    ToastLane::Toast t;
    t.title = title;
    t.sub = sub;
    return t;
}

BannerQueue::Banner banner(const std::string& title) {
    BannerQueue::Banner b;
    b.title = title;
    return b;
}

} // namespace

/// Three at most on screen, the newest on top; a fourth waits for a place
/// and takes it when the oldest goes, with its own full life.
TEST(ToastLane_threeAtMostNewestOnTop) {
    ToastLane lane;
    lane.push(toast("A"), 0);
    lane.push(toast("B"), 0.1);
    lane.push(toast("C"), 0.2);
    lane.push(toast("D"), 0.3);
    ASSERT_TRUE(lane.shown.size() == size_t(3));
    EXPECT_TRUE(lane.shown[0].title == "C" && lane.shown[1].title == "B" && lane.shown[2].title == "A");
    ASSERT_TRUE(lane.waiting.size() == size_t(1));
    EXPECT_TRUE(!lane.waiting[0].shownAt);
    // A goes at 4 s; D takes the place, on top, for 4 s from then.
    lane.step(ToastLane::life);
    ASSERT_TRUE(lane.shown.size() == size_t(3));
    EXPECT_TRUE(lane.shown[0].title == "D");
    EXPECT_NEAR(*lane.shown[0].shownAt, ToastLane::life, 1e-12);
    EXPECT_TRUE(lane.waiting.empty());
    lane.step(ToastLane::life + 0.15);
    EXPECT_EQ(lane.shown.size(), size_t(2));
    lane.step(2 * ToastLane::life + 0.01);
    EXPECT_TRUE(lane.empty());
}

/// The same title again: one toast, refreshed and back on top, its words
/// the newer ones; nothing stacks.
TEST(ToastLane_sameKindMerges) {
    ToastLane lane;
    lane.push(toast("SURVEY CHIP", "A site marked in Cold Fork"), 0);
    lane.push(toast("REGION DISCOVERED"), 1);
    lane.push(toast("SURVEY CHIP", "A site marked in Brine Pans"), 2);
    ASSERT_TRUE(lane.shown.size() == size_t(2));
    EXPECT_TRUE(lane.shown[0].title == "SURVEY CHIP");
    EXPECT_TRUE(lane.shown[0].sub == "A site marked in Brine Pans");
    EXPECT_EQ(lane.shown[0].count, int64_t(2));
    EXPECT_NEAR(*lane.shown[0].shownAt, 2.0, 1e-12);
    // Its life runs from the refresh.
    lane.step(5);
    ASSERT_TRUE(lane.shown.size() == size_t(1));
    EXPECT_TRUE(lane.shown[0].title == "SURVEY CHIP");
    // A waiting one merges where it waits.
    ToastLane full;
    for (const char* t : {"A", "B", "C", "D"}) full.push(toast(t), 0);
    full.push(toast("D", "again"), 0.5);
    EXPECT_EQ(full.waiting.size(), size_t(1));
    EXPECT_TRUE(full.waiting[0].sub == "again" && full.waiting[0].count == 2);
}

/// Gains add up while one is up: "+24 ORE" three times is "+72 ORE", the
/// realm's share with it; MH is its own toast.
TEST(ToastLane_gainsAddUp) {
    ToastLane lane;
    const std::array<float, 4> violet{0.78f, 0.66f, 1, 1};
    lane.gain("ORE", 24, 3, violet, 0);
    lane.gain("ORE", 24, 3, violet, 1);
    lane.gain("ORE", 24, 3, violet, 2);
    ASSERT_TRUE(lane.shown.size() == size_t(1));
    EXPECT_TRUE(lane.shown[0].title == "+72 ORE");
    EXPECT_TRUE(lane.shown[0].sub == "9 TO THE REALM");
    EXPECT_EQ(lane.shown[0].count, int64_t(3));
    lane.gain("MH", 20, 0, violet, 2.5);
    ASSERT_TRUE(lane.shown.size() == size_t(2));
    EXPECT_TRUE(lane.shown[0].title == "+20 MH" && lane.shown[0].sub.empty());
    // Once it went, the next gain starts again from its own amount.
    lane.step(10);
    lane.gain("ORE", 24, 0, violet, 10);
    EXPECT_TRUE(lane.shown[0].title == "+24 ORE");
}

/// A busy moment (an unload, a mission done, a goal done, a contract done at
/// once): three on screen, the fourth waits; a flood keeps only the newest
/// waiting.
TEST(ToastLane_busyMomentQueues) {
    ToastLane lane;
    lane.gain("ORE", 40, 4, {1, 1, 1, 1}, 0);
    lane.push(toast("MISSION DONE: SALVAGE RUN"), 0);
    lane.push(toast("GOAL DONE: STOCK UP"), 0);
    lane.push(toast("CONTRACT DONE: ORE RUN"), 0);
    EXPECT_EQ(lane.shown.size(), size_t(3));
    EXPECT_EQ(lane.waiting.size(), size_t(1));
    EXPECT_TRUE(lane.waiting[0].title == "CONTRACT DONE: ORE RUN");
    for (int i = 0; i < 20; i++) lane.push(toast("T" + std::to_string(i)), 0.1);
    EXPECT_EQ(lane.waiting.size(), ToastLane::maxWaiting);
    EXPECT_TRUE(lane.waiting.back().title == "T19");
    // Faded in, held, faded out.
    const ToastLane::Toast& t = lane.shown[0];
    EXPECT_NEAR(ToastLane::alpha(t, 0.1), 0.5, 1e-9);
    EXPECT_NEAR(ToastLane::alpha(t, 2), 1.0, 1e-12);
    EXPECT_TRUE(ToastLane::alpha(t, ToastLane::life - 0.1) < 0.5);
    EXPECT_NEAR(ToastLane::alpha(t, ToastLane::life), 0.0, 1e-12);
}

/// One banner at a time: a second waits for the first to go, then has its
/// whole run.
TEST(BannerQueue_oneAtATime) {
    BannerQueue q;
    q.push(banner("PROSPECTOR  LEVEL 5"), 0);
    q.step(0, false);
    q.push(banner("BROOD SWARM"), 0.5);
    q.step(0.5, false);
    ASSERT_TRUE(q.current.has_value());
    EXPECT_TRUE(q.current->title == "PROSPECTOR  LEVEL 5");
    EXPECT_EQ(q.waiting.size(), size_t(1));
    const double run = BannerQueue::in + BannerQueue::hold + BannerQueue::out;
    q.step(run - 0.01, false);
    EXPECT_TRUE(q.current->title == "PROSPECTOR  LEVEL 5");
    EXPECT_TRUE(q.alpha() > 0 && q.alpha() < 0.1);
    q.step(run, false);
    ASSERT_TRUE(q.current.has_value());
    EXPECT_TRUE(q.current->title == "BROOD SWARM");
    EXPECT_NEAR(q.age, 0, 1e-12);
    EXPECT_NEAR(q.scale(), 1.15, 1e-12);
    q.step(2 * run, false);
    EXPECT_TRUE(!q.current.has_value());
}

/// The same banner again replaces its words (levels 5 then 6 while it
/// shows: one banner); under a screen it waits, its clock stopped.
TEST(BannerQueue_mergesAndHolds) {
    BannerQueue q;
    q.push(banner("BROOD SWARM"), 0);
    q.step(0, false);
    BannerQueue::Banner again = banner("BROOD SWARM");
    again.sub = "heading for the Hab Dome";
    q.push(again, 1);
    EXPECT_TRUE(q.waiting.empty());
    EXPECT_TRUE(q.current->sub == "heading for the Hab Dome");
    q.step(1, false);
    // A screen opens for 20 s: still there after it closes.
    q.step(21, true);
    EXPECT_TRUE(q.current.has_value());
    EXPECT_NEAR(q.age, 1.0, 1e-9);
    // Nothing comes up under a screen.
    BannerQueue empty;
    empty.push(banner("THE MATRIARCH IS DOWN"), 0);
    empty.step(0, true);
    EXPECT_TRUE(!empty.current.has_value());
    empty.step(0.1, false);
    EXPECT_TRUE(empty.current.has_value());
}
