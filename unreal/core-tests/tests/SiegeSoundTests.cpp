// The siege heard (A2, SiegeSound.h): the wave layer rises with a wave near
// the pilot, louder with the wave's number; the Matriarch layer comes in on
// FAILED (or with her in sight); both fade out on HELD; the radio ducks to
// the narration's floor under them and comes back over 2 s after. KSTR's
// lines: one at a time, never over a narration, paced, stale ones dropped.

#include "test.h"

#include "SiegeSound.h"

#include <cmath>

namespace {

using namespace ac;

void run(SiegeMix& m, double seconds, const std::optional<SiegeMix::Near>& n) {
    for (int i = 0; i < int(seconds * 60); i++) m.step(1.0 / 60, n);
}

SiegeMix::Near zone(int state, int64_t wave, bool out, bool matriarch = false) {
    SiegeMix::Near n;
    n.state = state;
    n.wave = wave;
    n.out = out;
    n.matriarch = matriarch;
    return n;
}

} // namespace

TEST(SiegeMix_waveLayerRisesWithTheWave) {
    SiegeMix m;
    run(m, 5, std::nullopt);
    EXPECT_EQ(m.wave, 0.0);
    EXPECT_EQ(m.radio, 1.0);
    // Wave 1 out while waiting: the layer at its low gain in 3 s.
    run(m, 1.5, zone(0, 1, true));
    EXPECT_TRUE(m.wave > 0.4 && m.wave < 0.6);
    run(m, 2, zone(0, 1, true));
    EXPECT_TRUE(std::abs(m.wave - SiegeMix::waveLow) < 1e-9);
    // Wave 10 running: full.
    run(m, 3, zone(1, 10, true));
    EXPECT_TRUE(std::abs(m.wave - 1.0) < 1e-9);
    // Between two waves, the game running: it holds.
    run(m, 2, zone(1, 6, false));
    EXPECT_TRUE(std::abs(m.wave - SiegeMix::waveTarget(zone(1, 6, false))) < 1e-9);
    // Waiting with no wave out: it fades.
    run(m, 5, zone(0, 1, false));
    EXPECT_EQ(m.wave, 0.0);
}

TEST(SiegeMix_theMatriarchLayerOnFailedFadesOnHeld) {
    SiegeMix m;
    run(m, 4, zone(1, 5, true));
    run(m, 4, zone(2, 5, false));
    EXPECT_TRUE(std::abs(m.matriarch - 1.0) < 1e-9);
    EXPECT_EQ(m.wave, 0.0);
    // Seen in the region while waiting: her layer too.
    SiegeMix s;
    run(s, 4, zone(0, 1, false, true));
    EXPECT_TRUE(s.matriarch > 0.99);
    // HELD: both go within the fade out, she still in sight or not.
    run(m, 1, zone(3, 10, false, true));
    EXPECT_TRUE(m.matriarch < 1.0 && m.matriarch > 0.0);
    run(m, SiegeMix::fadeOut, zone(3, 10, false, true));
    EXPECT_EQ(m.matriarch, 0.0);
    EXPECT_EQ(m.wave, 0.0);
}

TEST(SiegeMix_theRadioDucksUnderTheLayersAndComesBackOver2s) {
    SiegeMix m;
    run(m, 1, zone(1, 3, true));
    EXPECT_TRUE(std::abs(m.radio - MusicDuck::floorGain()) < 1e-9);
    // The canyon held: down while the layer fades out, then up over 2 s.
    double faded = 0;
    while (m.wave > 0 && faded < 10) {
        m.step(1.0 / 60, zone(3, 10, false));
        faded += 1.0 / 60;
        EXPECT_TRUE(m.radio < 0.07 || m.wave < 0.01);
    }
    EXPECT_TRUE(faded > 1 && faded <= SiegeMix::fadeOut);
    run(m, 1.0, zone(3, 10, false));
    EXPECT_TRUE(m.radio > 0.4 && m.radio < 0.65);
    run(m, 1.1, zone(3, 10, false));
    EXPECT_EQ(m.radio, 1.0);
}

TEST(VoiceQueue_oneAtATimePacedNarrationFirst) {
    VoiceQueue q;
    q.push(0, "canyon-the-fork-wave", 3.0);
    q.push(0.1, "goal-done", 2.5);
    auto a = q.next(0.2);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->key, std::string("canyon-the-fork-wave"));
    // The second waits for the first and the gap.
    EXPECT_TRUE(!q.next(3.0).has_value());
    EXPECT_TRUE(!q.next(3.7).has_value());
    auto b = q.next(3.81);
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->key, std::string("goal-done"));
    // A narration: a line waits for it, and one speaking is cut.
    q.push(10, "goal-shop", 2.0);
    q.narration(9.9, 12.0);
    EXPECT_TRUE(!q.next(12).has_value());
    EXPECT_TRUE(!q.next(22.0).has_value());
    EXPECT_TRUE(q.next(22.6).has_value());
    q.push(30, "canyon-the-fork-beaten", 3.0);
    ASSERT_TRUE(q.next(30).has_value());
    EXPECT_TRUE(q.busy(31));
    q.narration(31, 5);
    EXPECT_TRUE(!q.busy(31.1));
    // Stopped early: lines go on after the gap.
    q.push(32, "goal-merc", 2);
    q.narrationEnded(33);
    EXPECT_TRUE(!q.next(33.3).has_value());
    EXPECT_TRUE(q.next(33.7).has_value());
}

TEST(VoiceQueue_staleCapAndRepeats) {
    VoiceQueue q;
    q.narration(0, 60);
    q.push(1, "a", 1);
    q.push(1, "a", 1);
    EXPECT_EQ(q.size(), size_t(1));
    for (const char* k : {"b", "c", "d", "e"}) q.push(2, k, 1);
    EXPECT_EQ(q.size(), VoiceQueue::most);
    // All waited past `stale` behind the long narration: dropped.
    EXPECT_TRUE(!q.next(61).has_value());
    EXPECT_EQ(q.size(), size_t(0));
}
