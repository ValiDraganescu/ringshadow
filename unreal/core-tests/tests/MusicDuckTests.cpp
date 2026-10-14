// The radio ducking under a mission narration (A1; MusicDuck.h): its gain
// over time, from a narration's start to the music back at full.
#include "MusicDuck.h"
#include "test.h"

using namespace ac;

/// At rest the music plays at full; a narration takes it to about −24 dB
/// in 0.3 s, holds it there for the narration's length, and brings it back
/// over 1 s.
TEST(MusicDuck_downHoldUp) {
    MusicDuck d;
    EXPECT_NEAR(d.gain(0), 1.0, 1e-12);
    d.start(10, 5);
    EXPECT_NEAR(d.gain(10), 1.0, 1e-12);
    EXPECT_TRUE(d.gain(10.15) < 1.0 && d.gain(10.15) > MusicDuck::floorGain());
    EXPECT_NEAR(d.gain(10.3), MusicDuck::floorGain(), 1e-12);
    EXPECT_NEAR(20 * std::log10(d.gain(12)), -24.0, 1e-9);
    EXPECT_TRUE(d.active(14.9));
    EXPECT_NEAR(d.gain(14.99), MusicDuck::floorGain(), 1e-12);
    // Played out at 15: back up over a second, never past full.
    EXPECT_TRUE(!d.active(15.0));
    EXPECT_TRUE(d.gain(15.5) > MusicDuck::floorGain() && d.gain(15.5) < 1.0);
    EXPECT_NEAR(d.gain(16.0), 1.0, 1e-12);
    EXPECT_NEAR(d.gain(100), 1.0, 1e-12);
    // It only falls, then only rises: no step.
    double last = 2;
    for (double t = 10; t <= 15; t += 0.01) {
        EXPECT_TRUE(d.gain(t) <= last + 1e-12);
        last = d.gain(t);
    }
    for (double t = 15; t <= 16.5; t += 0.01) {
        EXPECT_TRUE(d.gain(t) >= last - 1e-12);
        last = d.gain(t);
    }
}

/// Stopped early (the Journal closed, another sound cut it): back up from
/// there over a second.
TEST(MusicDuck_stoppedEarly) {
    MusicDuck d;
    d.start(0, 30);
    d.end(4);
    EXPECT_TRUE(!d.active(4.5));
    EXPECT_NEAR(d.gain(4), MusicDuck::floorGain(), 1e-12);
    EXPECT_TRUE(d.gain(4.5) > MusicDuck::floorGain());
    EXPECT_NEAR(d.gain(5), 1.0, 1e-12);
    // Stopped during the fade down: it comes back from where it was.
    MusicDuck e;
    e.start(0, 30);
    const double mid = e.gain(0.1);
    e.end(0.1);
    EXPECT_NEAR(e.gain(0.1), mid, 1e-12);
    EXPECT_NEAR(e.gain(1.1), 1.0, 1e-12);
}

/// A second narration while the first plays, or while the music comes back
/// up, keeps it down (no bump up between them).
TEST(MusicDuck_secondNarrationKeepsItDown) {
    MusicDuck d;
    d.start(0, 5);
    d.start(3, 5);
    EXPECT_NEAR(d.gain(3.1), MusicDuck::floorGain(), 1e-12);
    EXPECT_TRUE(d.active(7.9));
    EXPECT_NEAR(d.gain(7.9), MusicDuck::floorGain(), 1e-12);
    // On the way back up at 8.5, a third: down again from there.
    const double up = d.gain(8.5);
    d.start(8.5, 2);
    EXPECT_NEAR(d.gain(8.5), up, 1e-12);
    EXPECT_NEAR(d.gain(8.8), MusicDuck::floorGain(), 1e-12);
    EXPECT_NEAR(d.gain(12), 1.0, 1e-12);
}
