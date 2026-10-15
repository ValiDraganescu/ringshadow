// The MMORPG HUD's two queues (U1, docs/mmorpg-play.md "The HUD"):
//   - `ToastLane`: the one toast lane, off the subject (the view's upper
//     left). At most `visible` at once, newest on top, each shown for `life`
//     seconds; more wait their turn. A toast of the same kind as one shown
//     or waiting merges into it instead of stacking: the same title again
//     refreshes it, gains add up ("+24 ORE" three times: "+72 ORE").
//   - `BannerQueue`: the one banner slot at the top (a level reached, a raid
//     warning, the Matriarch down). One at a time; the next waits for it to
//     go; held (its clock stopped) while a screen covers the view.
// Engine-free, on the HUD's clock (real seconds).
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace ac {

struct ToastLane {
    /// Toasts on screen at once.
    static constexpr size_t visible = 3;
    /// Seconds each is shown (the last `fade` fading out).
    static constexpr double life = 4.0;
    static constexpr double fade = 0.5;
    /// More waiting than this: the oldest waiting one goes.
    static constexpr size_t maxWaiting = 6;

    struct Toast {
        std::string title;
        std::string sub;
        /// What merges: the same key, one toast (empty: the title).
        std::string key;
        /// The caller's colour, as it gave it.
        std::array<float, 4> accent{1, 1, 1, 1};
        /// A gain ("+24 ORE"): its amount, its unit and the realm's share
        /// of it (0: none). Not a gain: `unit` empty.
        int64_t amount = 0;
        int64_t tax = 0;
        std::string unit;
        /// How many came together in it.
        int64_t count = 1;
        /// When it took its place in the lane (unset: still waiting).
        std::optional<double> shownAt;
    };

    /// Shown, newest first.
    std::vector<Toast> shown;
    /// Waiting their turn, oldest first.
    std::deque<Toast> waiting;

    /// A toast at `now`: merged into one of its kind shown (refreshed and
    /// back on top) or waiting; else it waits for a place (at once when one
    /// is free).
    void push(Toast t, double now) {
        if (t.key.empty()) t.key = t.title;
        if (Toast* same = find(t.key)) {
            merge(*same, t);
            if (same->shownAt) {
                same->shownAt = now;
                // Back on top: newest first.
                const auto it = std::find_if(shown.begin(), shown.end(), [&](const Toast& x) { return &x == same; });
                std::rotate(shown.begin(), it, it + 1);
            }
            step(now);
            return;
        }
        t.shownAt.reset();
        waiting.push_back(std::move(t));
        while (waiting.size() > maxWaiting) waiting.pop_front();
        step(now);
    }

    /// A gain: `amount` of `unit` ("ORE", "MH") reached the wallet, `tax`
    /// went to the realm. Gains of one unit add up while one is up.
    void gain(const std::string& unit, int64_t amount, int64_t tax, const std::array<float, 4>& accent, double now) {
        Toast t;
        t.key = "gain/" + unit;
        t.unit = unit;
        t.amount = amount;
        t.tax = tax;
        t.accent = accent;
        words(t);
        push(std::move(t), now);
    }

    /// The clock moved to `now`: the ones over their life go, the waiting
    /// take the places freed.
    void step(double now) {
        shown.erase(std::remove_if(shown.begin(), shown.end(), [&](const Toast& t) { return now - *t.shownAt >= life; }), shown.end());
        while (shown.size() < visible && !waiting.empty()) {
            Toast t = std::move(waiting.front());
            waiting.pop_front();
            t.shownAt = now;
            shown.insert(shown.begin(), std::move(t));
        }
    }

    /// How opaque `t` is at `now`: in over 0.2 s, out over `fade`.
    static double alpha(const Toast& t, double now) {
        if (!t.shownAt) return 0;
        const double age = now - *t.shownAt;
        if (age < 0 || age >= life) return 0;
        if (age < 0.2) return age / 0.2;
        if (age > life - fade) return (life - age) / fade;
        return 1;
    }

    bool empty() const { return shown.empty() && waiting.empty(); }

private:
    Toast* find(const std::string& key) {
        for (Toast& t : shown)
            if (t.key == key) return &t;
        for (Toast& t : waiting)
            if (t.key == key) return &t;
        return nullptr;
    }

    static void merge(Toast& into, const Toast& t) {
        into.count += 1;
        into.accent = t.accent;
        if (!into.unit.empty() && into.unit == t.unit) {
            into.amount += t.amount;
            into.tax += t.tax;
            words(into);
            return;
        }
        into.title = t.title;
        into.sub = t.sub;
    }

    /// A gain's words: "+72 ORE", "9 TO THE REALM".
    static void words(Toast& t) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "+%lld %s", (long long)t.amount, t.unit.c_str());
        t.title = buf;
        if (t.tax > 0) {
            std::snprintf(buf, sizeof buf, "%lld TO THE REALM", (long long)t.tax);
            t.sub = buf;
        } else {
            t.sub.clear();
        }
    }
};

struct BannerQueue {
    /// In (a scale-in), held, out (a fade), seconds.
    static constexpr double in = 0.18;
    static constexpr double hold = 3.6;
    static constexpr double out = 0.6;
    /// More waiting than this: the oldest waiting one goes.
    static constexpr size_t maxWaiting = 4;

    struct Banner {
        std::string title;
        std::string sub;
        /// A third line ("TIER 2 OPEN"), empty: none.
        std::string gate;
        std::array<float, 4> accent{1, 1, 1, 1};
    };

    std::optional<Banner> current;
    std::deque<Banner> waiting;
    /// How long the current one has been up (its clock stops while held).
    double age = 0;

    /// A banner at `now`: up at the next step when the slot is free, else it waits.
    /// One with the current or a waiting one's title replaces it (a second
    /// level reached while the first shows: one banner, the newer words).
    void push(Banner b, double now) {
        if (current && current->title == b.title) {
            *current = std::move(b);
            return;
        }
        for (Banner& w : waiting)
            if (w.title == b.title) {
                w = std::move(b);
                return;
            }
        waiting.push_back(std::move(b));
        while (waiting.size() > maxWaiting) waiting.pop_front();
        // Up at the next step (not while a screen is open).
        (void)now;
    }

    /// The clock moved to `now`; `held`: a screen covers the view (the
    /// current one waits there, its clock stopped).
    void step(double now, bool held) {
        const double dt = last ? std::max(0.0, now - *last) : 0.0;
        last = now;
        if (current && !held) age += dt;
        if (current && age >= in + hold + out) current.reset();
        if (!current && !waiting.empty() && !held) {
            current = std::move(waiting.front());
            waiting.pop_front();
            age = 0;
        }
    }

    /// The current one's opacity and scale (in: 1.15 to 1).
    double alpha() const {
        if (!current) return 0;
        if (age < in) return age / in;
        if (age > in + hold) return std::max(0.0, 1 - (age - in - hold) / out);
        return 1;
    }
    double scale() const { return current && age < in ? 1.15 + (1 - 1.15) * (age / in) : 1.0; }

private:
    std::optional<double> last;
};

} // namespace ac
