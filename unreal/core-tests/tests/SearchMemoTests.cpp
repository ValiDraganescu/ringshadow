// C++ only: the walking grid keeps its searches (NavGrid::searchMemo). A
// search asked again over the same `passable` gets the cells the search
// found, so every path must be the one a grid without the memo finds, bit
// for bit, from anywhere in the start cell; a change to the grid starts the
// memo again.
#include "SimHelpers.h"

#include "Commander.h"
#include "Session.h"
#include "WindowMaps.h"

#include <cstring>

using namespace ac;
using namespace simtest;

namespace {

std::optional<Simulation> memoGame() {
    const std::string file = std::string(AC_REPO_ROOT) + "/bench/badlands-large.json";
    std::string error;
    const auto session = SessionStore::load(file, &error);
    if (!session) return std::nullopt;
    const MapDefinition m = WindowMaps::build(MapChoice{MapStyle::badlands, MapSize::large});
    return Simulation(session->state, m, Commander::all(m), true);
}

bool sameWay(const std::optional<std::vector<Vec2>>& a, const std::optional<std::vector<Vec2>>& b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a) return true;
    return a->size() == b->size() && (a->empty() || std::memcmp(a->data(), b->data(), a->size() * sizeof(Vec2)) == 0);
}

/// Pairs of open-map cells from a fixed sequence, far enough apart that the
/// searches are long ones the memo keeps.
std::vector<std::pair<Vec2, Vec2>> farPairs(const NavGrid& nav, int count) {
    uint64_t seed = 777;
    auto pick = [&] {
        while (true) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            const int64_t i = static_cast<int64_t>((seed >> 33) % uint64_t(nav.width * nav.height));
            if (nav.free(i) && nav.regions[size_t(i)] == nav.mainRegion) return nav.centre(i);
        }
    };
    std::vector<std::pair<Vec2, Vec2>> pairs;
    while (int(pairs.size()) < count) {
        const Vec2 a = pick(), b = pick();
        if (distance(a, b) > 30) pairs.push_back({a, b});
    }
    return pairs;
}

} // namespace

TEST(searchMemo_aSearchAskedAgainIsTheSearch) {
    std::optional<Simulation> game = memoGame();
    ASSERT_TRUE(game.has_value());
    for (const bool fast : {false, true}) {
        NavGrid nav = *game->nav;
        nav.fastSearch = fast;
        NavGrid plain = nav;
        plain.searchMemo = nullptr;
        int64_t kept = 0;
        for (const auto& [a, b] : farPairs(nav, 60)) {
            const auto fresh = plain.path(a, b, 0.1);
            const auto first = nav.path(a, b, 0.1);
            const int64_t before = NavGrid::searchesRun.load();
            // Again, and from elsewhere in the same cell: the same cells, straightened from there.
            const auto again = nav.path(a, b, 0.1);
            const Vec2 moved = a + Vec2(0.13, -0.11);
            const auto movedMemo = nav.path(moved, b, 0.1);
            kept += NavGrid::searchesRun.load() == before ? 1 : 0;
            EXPECT_TRUE(sameWay(fresh, first));
            EXPECT_TRUE(sameWay(fresh, again));
            EXPECT_TRUE(sameWay(plain.path(moved, b, 0.1), movedMemo));
        }
        // Most of these close more than `memoCells`: asked again, nothing is searched.
        EXPECT_TRUE(kept > 30);
        std::printf("  (%s: %lld of 60 asked again without a search)\n", fast ? "fast" : "exact", (long long)kept);
    }
}

TEST(searchMemo_aChangeToTheGridStartsItAgain) {
    std::optional<Simulation> game = memoGame();
    ASSERT_TRUE(game.has_value());
    NavGrid nav = *game->nav;
    const auto pairs = farPairs(nav, 30);
    for (const auto& [a, b] : pairs) nav.path(a, b, 0.1);
    const auto memo = nav.searchMemo;
    // A copy shares it while its grid is the same.
    NavGrid copy = nav;
    EXPECT_TRUE(copy.searchMemo == memo);
    // Every deposit mined out: the grid opens where they stood.
    GameState s = game->state;
    for (OreDeposit& d : s.patches) d.remaining = 0;
    ASSERT_TRUE(nav.setDynamic(s));
    EXPECT_TRUE(nav.searchMemo != memo);
    EXPECT_TRUE(copy.searchMemo == memo);
    NavGrid plain = nav, plainBefore = *game->nav;
    plain.searchMemo = nullptr;
    plainBefore.searchMemo = nullptr;
    int64_t differ = 0;
    for (const auto& [a, b] : pairs) {
        const auto now = nav.path(a, b, 0.1), then = copy.path(a, b, 0.1);
        EXPECT_TRUE(sameWay(plain.path(a, b, 0.1), now));
        EXPECT_TRUE(sameWay(plainBefore.path(a, b, 0.1), then));
        differ += sameWay(now, then) ? 0 : 1;
    }
    std::printf("  (%lld of 30 ways changed with the deposits gone)\n", (long long)differ);
}

TEST(searchMemo_manyQuestionsStayWithinItsBound) {
    std::optional<Simulation> game = memoGame();
    ASSERT_TRUE(game.has_value());
    NavGrid nav = *game->nav;
    NavGrid plain = nav;
    plain.searchMemo = nullptr;
    size_t most = 0, mostCells = 0;
    for (const auto& [a, b] : farPairs(nav, 600)) {
        EXPECT_TRUE(sameWay(plain.path(a, b, 0.1), nav.path(a, b, 0.1)));
        const auto [held, cells] = nav.memoHeld();
        most = std::max(most, held);
        mostCells = std::max(mostCells, cells);
    }
    // 600 different long questions: it filled and started again, never past 256 searches or 65,536 cells.
    EXPECT_TRUE(most == 256);
    EXPECT_TRUE(mostCells <= 65536);
    std::printf("  (at most %zu searches, %zu cells held)\n", most, mostCells);
}
