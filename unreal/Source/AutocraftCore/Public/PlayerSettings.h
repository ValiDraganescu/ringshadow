// The settings screen's rules (U2, docs/mmorpg-play.md "Settings"): the
// volumes' defaults and steps, the screen's modes and the resolutions it
// offers, the lag simulator's switch. Engine-free; the game keeps the
// values in `UAcSettings` (GameUserSettings.ini) and the screen is SAcMenu.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ac {

struct PlayerSettings {
    /// A first run's volumes, 0...1: the music a little under the voices and
    /// the effects, so KSTR and the narration carry over it.
    static constexpr double defaultMaster = 1.0;
    static constexpr double defaultMusic = 0.8;
    static constexpr double defaultVoice = 1.0;
    static constexpr double defaultEffects = 1.0;
    static constexpr double defaultEnvironment = 1.0;
    /// Left and Right move a volume this much (5%).
    static constexpr double volumeStep = 0.05;

    /// `v` moved `steps` steps, on the 5% grid, within 0...1.
    static double stepVolume(double v, int steps) {
        const double at = std::round(std::clamp(v, 0.0, 1.0) / volumeStep) + steps;
        return std::clamp(at * volumeStep, 0.0, 1.0);
    }
    /// What the screen shows: 0...100.
    static int percent(double v) { return int(std::lround(std::clamp(v, 0.0, 1.0) * 100)); }

    /// The lag simulator's one setting besides off: two_clients.py's
    /// (150 ms each way, up to 20 ms more, 5% lost), as `ac.MmoLag` reads it.
    static constexpr const char* lagPreset = "150 20 5";
    static std::string lagSetting(bool on) { return on ? lagPreset : ""; }
};

struct Resolution {
    int width = 0;
    int height = 0;
    bool operator==(const Resolution&) const = default;
    bool valid() const { return width > 0 && height > 0; }
};

/// How the game's window shows: a window, the whole display at its own
/// resolution (borderless, the display untouched), or the whole display
/// switched to another of its modes.
enum class ScreenMode { window, borderless, switched };

struct ScreenChoices {
    /// The smallest size offered: under it the HUD's panels overlap.
    static constexpr int minWidth = 1280;
    static constexpr int minHeight = 720;

    /// The resolutions the screen offers: the display's own modes no
    /// smaller than 1280 x 720, each once, smallest first. Full screen: up
    /// to the display's own resolution, always among them; a window: only
    /// those smaller than the display both ways (room for the menu bar and
    /// the window's title; macOS keeps a window on its screen).
    static std::vector<Resolution> list(std::vector<Resolution> supported, Resolution desktop, bool fullScreen = true) {
        std::vector<Resolution> out;
        // A display that lists no modes (seen on a Mac with two displays):
        // the common sizes, under the same rules.
        if (supported.empty()) supported = standard();
        if (desktop.valid() && fullScreen) supported.push_back(desktop);
        for (const Resolution& r : supported) {
            if (r.width < minWidth || r.height < minHeight) continue;
            if (desktop.valid() && (r.width > desktop.width || r.height > desktop.height)) continue;
            if (desktop.valid() && !fullScreen && (r.width >= desktop.width || r.height >= desktop.height)) continue;
            if (std::find(out.begin(), out.end(), r) == out.end()) out.push_back(r);
        }
        std::sort(out.begin(), out.end(), [](const Resolution& a, const Resolution& b) {
            return a.width != b.width ? a.width < b.width : a.height < b.height;
        });
        return out;
    }

    /// The common 16:9 and 16:10 sizes.
    static std::vector<Resolution> standard() {
        return {{1280, 720}, {1280, 800}, {1440, 900}, {1600, 900}, {1680, 1050}, {1920, 1080}, {1920, 1200},
                {2560, 1440}, {2560, 1600}, {3200, 1800}, {3840, 2160}, {3840, 2400}, {5120, 2880}};
    }

    /// `want`'s place in `list`; not there: the one nearest it in area; -1:
    /// the list is empty.
    static int index(const std::vector<Resolution>& list, Resolution want) {
        int best = -1;
        double bestGap = 0;
        for (int i = 0; i < int(list.size()); ++i) {
            if (list[size_t(i)] == want) return i;
            const double gap = std::abs(double(list[size_t(i)].width) * list[size_t(i)].height - double(want.width) * want.height);
            if (best < 0 || gap < bestGap) {
                best = i;
                bestGap = gap;
            }
        }
        return best;
    }

    /// The mode a choice asks for: full screen at the display's own
    /// resolution (or none chosen yet) is borderless; at another, the
    /// display switches to it.
    static ScreenMode mode(bool fullScreen, Resolution pick, Resolution desktop) {
        if (!fullScreen) return ScreenMode::window;
        return !pick.valid() || !desktop.valid() || pick == desktop ? ScreenMode::borderless : ScreenMode::switched;
    }
};

} // namespace ac
