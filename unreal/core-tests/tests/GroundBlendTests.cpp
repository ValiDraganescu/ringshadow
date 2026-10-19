// ac::groundblend (GroundBlend.h): where ground palettes meet. One truth on
// both sides of an edge and round a corner, nothing far inside, the whole
// neighbour far past, a bounded wander, the same field for the same ids.
#include "test.h"

#include "GroundBlend.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace ac::groundblend;

namespace {
    /// The First Reach: 3 × 3 medium regions (144 × 96), world-grid cells.
    constexpr float W = 144, D = 96;
    const char* const Ids[3][3] = {{"ashen-shelf", "cold-fork", "glass-flats"},
                                   {"rust-hollow", "start", "tern-ridge"},
                                   {"dust-terraces", "brine-pans", "slag-pockets"}};
    const int Dc[9] = {0, 0, 1, 0, -1, 1, 1, -1, -1};
    const int Dr[9] = {0, -1, 0, 1, 0, -1, 1, 1, -1};

    std::string_view idAt(int c, int r) { return c < 0 || c > 2 || r < 0 || r > 2 ? std::string_view() : Ids[r][c]; }

    Frame frameAt(int c, int r) {
        std::array<std::string_view, 9> ids;
        for (int k = 0; k < 9; k++) ids[size_t(k)] = idAt(c + Dc[k], r + Dr[k]);
        return frame(c * W, r * D, c * W + W, r * D + D, ids);
    }

    /// Cells from (x, y) to region (c, r)'s rectangle (0 inside).
    float outside(int c, int r, float x, float y) {
        const float ox = std::max({c * W - x, 0.0f, x - (c * W + W)}), oy = std::max({r * D - y, 0.0f, y - (r * D + D)});
        return std::sqrt(ox * ox + oy * oy);
    }

    /// A little seeded walk over the world, the same each run.
    struct Points {
        uint64_t s = 0x9e3779b97f4a7c15ull;
        float next(float lo, float hi) {
            s ^= s >> 12;
            s ^= s << 25;
            s ^= s >> 27;
            return lo + (hi - lo) * float(double((s * 0x2545f4914f6cdd1dull) >> 11) / double(1ull << 53));
        }
    };
}

/// Far inside a region (past `reach` from every edge) the field is nothing:
/// its own palette, exactly.
TEST(groundblend_nothing_far_inside) {
    const Frame f = frameAt(1, 1);
    Points p;
    int n = 0;
    for (int i = 0; i < 4000; i++) {
        const float x = p.next(W, 2 * W), y = p.next(D, 2 * D);
        if (x - W <= reach() || 2 * W - x <= reach() || y - D <= reach() || 2 * D - y <= reach()) continue;
        const Field fl = field(f, x, y);
        EXPECT_EQ(fl.a, 0.0f);
        EXPECT_EQ(fl.b, 0.0f);
        EXPECT_EQ(fl.weights()[own], 1.0f);
        n++;
    }
    EXPECT_TRUE(n > 200);
}

/// Far past an edge (past `reach`) the neighbour's palette is whole; at the
/// line's other side nothing of it.
TEST(groundblend_whole_far_outside) {
    Points p;
    for (int i = 0; i < 2000; i++) {
        const float x = p.next(-500, 500), y = p.next(-500, 500), t = p.next(-60, 60);
        const uint32_t a = uint32_t(i) * 7919u, b = uint32_t(i) * 104729u;
        const float d = p.next(reach(), reach() + 50);
        EXPECT_EQ(line(d, x, y, t, 0, a, b), 1.0f);
        EXPECT_EQ(line(-d, x, y, t, 0, a, b), 0.0f);
    }
    // Start's east neighbour, well into Tern Ridge from Start's frame.
    const Frame f = frameAt(1, 1);
    EXPECT_EQ(field(f, 2 * W + reach() + 1, 1.5f * D).a, 1.0f);
    EXPECT_EQ(field(f, 2 * W + reach() + 1, 1.5f * D).weights()[east], 1.0f);
}

/// One truth: at every point near two or more regions, each region's frame
/// gives every region the same weight (an edge, and the corners where four
/// meet, the diagonal one too), and the weights sum to 1.
TEST(groundblend_one_truth_across_edges_and_corners) {
    Points p;
    double worst = 0;
    int shared = 0, corners = 0;
    for (int i = 0; i < 60000; i++) {
        const float x = p.next(0, 3 * W), y = p.next(0, 3 * D);
        const int c = std::min(2, int(x / W)), r = std::min(2, int(y / D));
        const std::array<float, 9> mine = field(frameAt(c, r), x, y).weights();
        float sum = 0;
        for (float v : mine) sum += v;
        EXPECT_NEAR(sum, 1.0, 1e-6);
        // The weight each region round has in this frame.
        const auto weightOf = [&](int cc, int rr, const std::array<float, 9>& w, int fc, int fr) {
            for (int k = 0; k < 9; k++)
                if (fc + Dc[k] == cc && fr + Dr[k] == rr) return w[size_t(k)];
            return 0.0f;
        };
        bool corner = false;
        for (int k = 1; k < 9; k++) {
            const int cc = c + Dc[k], rr = r + Dr[k];
            if (idAt(cc, rr).empty() || outside(cc, rr, x, y) > reach()) continue;
            const std::array<float, 9> theirs = field(frameAt(cc, rr), x, y).weights();
            for (int q = 0; q < 9; q++) {
                const int qc = c + Dc[q], qr = r + Dr[q];
                if (idAt(qc, qr).empty()) continue;
                worst = std::max(worst, double(std::fabs(weightOf(qc, qr, mine, c, r) - weightOf(qc, qr, theirs, cc, rr))));
            }
            shared++;
            corner = corner || k >= northEast;
        }
        corners += corner && mine[size_t(northEast)] + mine[size_t(southEast)] + mine[size_t(southWest)] + mine[size_t(northWest)] > 0.01f;
    }
    EXPECT_TRUE(worst < 1e-6);
    EXPECT_TRUE(shared > 10000);
    EXPECT_TRUE(corners > 100);
}

/// The wander stays within its bound and does wander: along an edge the
/// line where half the ground is the neighbour's moves by several cells.
TEST(groundblend_bounded_wander) {
    Points p;
    float lo = 1, hi = -1;
    for (int i = 0; i < 20000; i++) {
        const float v = wander(p.next(-2000, 2000), p.next(-2000, 2000), uint32_t(i) & 0xffffff);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    EXPECT_TRUE(lo >= -1.0f && hi <= 1.0f);
    EXPECT_TRUE(lo < -0.6f && hi > 0.6f);
    // Start | Tern Ridge, a vertical line at x = 288: the half-way mark of
    // the neighbour's share across the band, every 8 cells along it.
    const uint32_t s = seed("start", "tern-ridge");
    float mid[12];
    for (int j = 0; j < 12; j++) {
        const float y = 100 + 8.0f * float(j);
        mid[j] = 0;
        for (float d = -reach(); d <= reach(); d += 0.25f) {
            float share = 0;
            for (int k = 0; k < 16; k++) share += line(d, 2 * W + d, y + float(k) * 0.5f, y, 0, s, s);
            if (share / 16 >= 0.5f) {
                mid[j] = d;
                break;
            }
        }
        EXPECT_TRUE(std::fabs(mid[j]) <= wanderCells + bandCells * 0.5f);
    }
    EXPECT_TRUE(*std::max_element(mid, mid + 12) - *std::min_element(mid, mid + 12) > 4.0f);
}

/// Across the band the neighbour's share follows the place in it (the
/// patches' noise is near even): half in the middle, none at its near end,
/// all at its far end, rising evenly between the soft rims' reach
/// (`(place - soft) / (1 - 2 soft)`).
TEST(groundblend_patches_follow_the_band) {
    Points p;
    int hist[10] = {};
    const int N = 40000;
    for (int i = 0; i < N; i++) {
        const float m = patch(p.next(-3000, 3000), p.next(-3000, 3000), 12345);
        EXPECT_TRUE(m >= 0.0f && m <= 1.0f);
        hist[std::min(9, int(m * 10))]++;
    }
    for (int k = 0; k < 10; k++) EXPECT_TRUE(hist[k] > N / 25 && hist[k] < N / 6);
    for (const float place : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        double share = 0;
        for (int k = 0; k < 1000; k++) share += plusOf((place - 0.5f) * bandCells, 0, (float(k) + 0.5f) / 1000);
        const float even = std::clamp((place - patchSoft) / (1.0f - 2.0f * patchSoft), 0.0f, 1.0f);
        EXPECT_NEAR(share / 1000, even, 0.01);
    }
    EXPECT_EQ(plusOf(-bandCells * 0.5f, 0, 0), 0.0f);
    EXPECT_EQ(plusOf(bandCells * 0.5f, 0, 1), 1.0f);
}

/// The same ids, the same field, every visit; other ids, another field.
/// The seed is 24 bits (a float parameter holds it exactly) and knows which
/// side is which.
TEST(groundblend_deterministic_by_seed) {
    EXPECT_EQ(seed("start", "tern-ridge"), seed("start", "tern-ridge"));
    EXPECT_TRUE(seed("start", "tern-ridge") != seed("tern-ridge", "start"));
    EXPECT_TRUE(seed("start", "tern-ridge") != seed("start", "brine-pans"));
    EXPECT_TRUE(seed("start", "") != seed("", "start"));
    for (const char* a : {"", "start", "a-very-long-region-id-indeed"}) EXPECT_TRUE(seed(a, "x") < (1u << 24));
    const Frame f1 = frameAt(1, 1), f2 = frameAt(1, 1);
    int differ = 0;
    Points p;
    for (int i = 0; i < 500; i++) {
        const float x = p.next(W - 20, W + 20), y = p.next(D, 2 * D);
        EXPECT_EQ(field(f1, x, y).a, field(f2, x, y).a);
        const uint32_t s1 = seed("rust-hollow", "start"), s2 = seed("rust-hollow", "starts");
        differ += line(x - W, x, y, y, 0, s1, s1) != line(x - W, x, y, y, 0, s2, s2);
    }
    EXPECT_TRUE(differ > 100);
}

/// No region beyond an edge: no blend there (the world's rim keeps its own
/// palette to the border scenery).
TEST(groundblend_none_at_the_rim) {
    const Frame f = frameAt(0, 0);  // Ashen Shelf: nothing north or west.
    EXPECT_TRUE(!f.has[0] && f.has[1] && f.has[2] && !f.has[3]);
    Points p;
    for (int i = 0; i < 2000; i++) {
        const float x = p.next(-20, W * 0.5f), y = p.next(-20, D * 0.5f);
        const Field fl = field(f, x, y);
        EXPECT_EQ(fl.a, 0.0f);
        EXPECT_EQ(fl.b, 0.0f);
    }
}

/// The scenery's draw takes each palette as often as it weighs there.
TEST(groundblend_pick_follows_the_weights) {
    const Frame f = frameAt(1, 1);
    Points p;
    int tried = 0;
    for (int i = 0; i < 400 && tried < 40; i++) {
        const float x = p.next(2 * W - 20, 2 * W), y = p.next(D + 30, 2 * D - 30);
        const std::array<float, 9> w = field(f, x, y).weights();
        if (w[own] < 0.2f || w[own] > 0.8f) continue;
        tried++;
        int east = 0;
        for (int k = 0; k < 2000; k++) east += pick(f, x, y, (double(k) + 0.5) / 2000) == ac::groundblend::east;
        EXPECT_NEAR(east / 2000.0, w[ac::groundblend::east], 0.002);
    }
    EXPECT_TRUE(tried > 5);
    EXPECT_EQ(pick(f, 1.5f * W, 1.5f * D, 0.999), int(own));
}
