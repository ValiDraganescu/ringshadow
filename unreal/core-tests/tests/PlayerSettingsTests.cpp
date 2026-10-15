// The settings screen's rules (U2, PlayerSettings.h): a first run's volumes,
// the 5% steps, the resolutions offered, the screen's mode, the lag switch.
#include "PlayerSettings.h"
#include "test.h"

using namespace ac;

/// A first run: the music at 80, the voices and the effects at 100.
TEST(PlayerSettings_defaults) {
    EXPECT_EQ(PlayerSettings::percent(PlayerSettings::defaultMusic), 80);
    EXPECT_EQ(PlayerSettings::percent(PlayerSettings::defaultVoice), 100);
    EXPECT_EQ(PlayerSettings::percent(PlayerSettings::defaultEffects), 100);
    EXPECT_EQ(PlayerSettings::percent(PlayerSettings::defaultMaster), 100);
    EXPECT_EQ(PlayerSettings::lagSetting(false), std::string(""));
}

/// Left and Right: 5% a step, on the grid, never past 0 or 100.
TEST(PlayerSettings_volumeSteps) {
    EXPECT_NEAR(PlayerSettings::stepVolume(0.8, -1), 0.75, 1e-12);
    EXPECT_NEAR(PlayerSettings::stepVolume(0.8, 1), 0.85, 1e-12);
    // A dragged value off the grid snaps to it, then steps.
    EXPECT_NEAR(PlayerSettings::stepVolume(0.512, 1), 0.55, 1e-12);
    EXPECT_NEAR(PlayerSettings::stepVolume(1.0, 1), 1.0, 1e-12);
    EXPECT_NEAR(PlayerSettings::stepVolume(0.0, -1), 0.0, 1e-12);
    EXPECT_NEAR(PlayerSettings::stepVolume(0.02, -1), 0.0, 1e-12);
    EXPECT_EQ(PlayerSettings::percent(0.555), 56);
    EXPECT_EQ(PlayerSettings::percent(1.4), 100);
}

/// The display's modes from 1280 x 720 up to its own size, each once,
/// smallest first, its own always there.
TEST(PlayerSettings_resolutionList) {
    const Resolution desktop{3456, 2234};
    const std::vector<Resolution> modes = {{1920, 1200}, {640, 480}, {1280, 800}, {5120, 2880}, {1920, 1200},
                                           {1024, 768},  {2560, 1600}, {1280, 720}, {1600, 900}};
    const std::vector<Resolution> list = ScreenChoices::list(modes, desktop);
    const std::vector<Resolution> want = {{1280, 720}, {1280, 800}, {1600, 900}, {1920, 1200}, {2560, 1600}, {3456, 2234}};
    EXPECT_TRUE(list == want);
    // No modes read: the common sizes that fit, and the display's own.
    const std::vector<Resolution> common = ScreenChoices::list({}, Resolution{1728, 1085});
    const std::vector<Resolution> commonWant = {{1280, 720}, {1280, 800}, {1440, 900}, {1600, 900}, {1680, 1050}, {1728, 1085}};
    EXPECT_TRUE(common == commonWant);
    EXPECT_EQ(ScreenChoices::list({}, Resolution{1728, 1085}, false).size(), size_t(5));
    // No desktop known: the modes as they come, filtered and sorted.
    EXPECT_EQ(ScreenChoices::list(modes, Resolution{}).size(), size_t(6));
    // A window: only the modes smaller than the display both ways.
    const std::vector<Resolution> window = ScreenChoices::list(modes, Resolution{2560, 1440}, false);
    const std::vector<Resolution> fits = {{1280, 720}, {1280, 800}, {1600, 900}, {1920, 1200}};
    EXPECT_TRUE(window == fits);
}

/// A saved size finds its place, or the one nearest it.
TEST(PlayerSettings_resolutionIndex) {
    const std::vector<Resolution> list = {{1280, 720}, {1920, 1200}, {2560, 1600}};
    EXPECT_EQ(ScreenChoices::index(list, Resolution{1920, 1200}), 1);
    EXPECT_EQ(ScreenChoices::index(list, Resolution{2400, 1500}), 2);
    EXPECT_EQ(ScreenChoices::index(list, Resolution{800, 600}), 0);
    EXPECT_EQ(ScreenChoices::index({}, Resolution{800, 600}), -1);
}

/// Full screen at the display's own size (or none saved) leaves the display
/// alone; another size switches it; a window is a window.
TEST(PlayerSettings_screenMode) {
    const Resolution desktop{3456, 2234};
    EXPECT_TRUE(ScreenChoices::mode(true, desktop, desktop) == ScreenMode::borderless);
    EXPECT_TRUE(ScreenChoices::mode(true, Resolution{}, desktop) == ScreenMode::borderless);
    EXPECT_TRUE(ScreenChoices::mode(true, Resolution{1920, 1200}, desktop) == ScreenMode::switched);
    EXPECT_TRUE(ScreenChoices::mode(false, Resolution{1920, 1200}, desktop) == ScreenMode::window);
    EXPECT_TRUE(ScreenChoices::mode(false, desktop, desktop) == ScreenMode::window);
}

/// The lag switch on: two_clients.py's preset, as ac.MmoLag reads it.
TEST(PlayerSettings_lagPreset) {
    EXPECT_EQ(PlayerSettings::lagSetting(true), std::string("150 20 5"));
}
