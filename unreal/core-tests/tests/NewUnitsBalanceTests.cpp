// Balance duels for the Peregrine, the Atlas and the Scorpion: the claims
// of docs/new-units.md ("The counter loop", the Peregrine's numbers,
// "Balance checks"), each a small staged fight on a map-less field (no fog,
// straight flight, a buried Scorpion still hidden), never a game: no AI, no
// economy. "Equal cost" counts ore and Metallic Hydrogen together.
#include "SimHelpers.h"

using namespace ac;
using simtest::bare;
using simtest::shots;

namespace {

/// Two players, nothing on the field, no map.
GameState empty() { return bare(2, 600, {}, {}, 1); }

/// A unit at `p`, idle or on attack-move toward its side's point; a
/// Scorpion holds its spot (a raid order on it), and buries there.
int64_t add(GameState& s, Unit::Kind kind, Vec2 p, int64_t owner, double heading = 0, bool attack = false) {
    const int64_t id = s.nextID;
    Unit u(id, kind, owner, p, heading, attack ? Unit::Task::attackMove : Unit::Task::idle);
    if (kind == Unit::Kind::longbow) { u.anchor = 0; u.anchored = false; u.aim = heading; }
    if (kind == Unit::Kind::hailstorm || kind == Unit::Kind::firefly || kind == Unit::Kind::atlas) u.aim = heading;
    if (kind == Unit::Kind::scorpion) { u.anchor = 0; u.anchored = false; u.mission = Mission::Raid{p}; }
    if (kind == Unit::Kind::dropship) { u.energy = 0; u.cargo = std::vector<int64_t>{}; }
    s.units.push_back(u);
    s.nextID += 1;
    return id;
}

int64_t addBuried(GameState& s, Vec2 p, int64_t owner) {
    const int64_t id = add(s, Unit::Kind::scorpion, p, owner);
    s.units.back().anchor = 1;
    s.units.back().anchored = true;
    return id;
}

/// `n` units of a kind in a column at x, 1.9 cells apart, facing -x.
std::vector<int64_t> line(GameState& s, Unit::Kind kind, int n, double x, int64_t owner, bool attack = false) {
    std::vector<int64_t> ids;
    for (int i = 0; i < n; i++) ids.push_back(add(s, kind, Vec2(x, (i - (n - 1) / 2.0) * 1.9), owner, pi, attack));
    return ids;
}

/// `n` units packed 0.8 cells apart (edges touching) from `at`.
std::vector<int64_t> clump(GameState& s, Unit::Kind kind, int n, Vec2 at, int64_t owner) {
    std::vector<int64_t> ids;
    for (int i = 0; i < n; i++) ids.push_back(add(s, kind, at + Vec2((i % 2) * 0.8, (i / 2) * 0.8), owner, pi));
    return ids;
}

int64_t worth(Unit::Kind k) { return Rules::cost(k) + Rules::hydrogenCost(k); }

double life(const Simulation& sim, int64_t id) {
    if (auto u = sim.state.unit(id)) return u->hp;
    if (auto b = sim.state.structure(id)) return b->hp;
    return 0;
}

/// How a fight ended: when one side was gone (nil: neither within the
/// limit), what each side has left, and the shots each fired.
struct Outcome {
    std::optional<double> over;
    int64_t a = 0, b = 0;
    double hpA = 0, hpB = 0;
    int64_t shotsA = 0, shotsB = 0;
};

Outcome fight(Simulation& sim, const std::vector<int64_t>& A, const std::vector<int64_t>& B, double limit) {
    Outcome o;
    const double start = sim.state.time;
    for (int64_t k = 0; k < int64_t(limit * 30); k++) {
        const auto events = sim.step(1.0 / 30);
        for (auto id : A) o.shotsA += shots(events, id);
        for (auto id : B) o.shotsB += shots(events, id);
        int64_t a = 0, b = 0;
        for (auto id : A) a += life(sim, id) > 0;
        for (auto id : B) b += life(sim, id) > 0;
        if (a == 0 || b == 0) { o.over = sim.state.time - start; break; }
    }
    for (auto id : A) { o.a += life(sim, id) > 0; o.hpA += life(sim, id); }
    for (auto id : B) { o.b += life(sim, id) > 0; o.hpB += life(sim, id); }
    return o;
}

/// Seconds `n` of `kind` take to shoot down a Peregrine parked 4 cells off
/// (it cannot shoot back at the ground).
double downPeregrine(Unit::Kind kind, int n) {
    GameState s = empty();
    const int64_t p = add(s, Unit::Kind::peregrine, Vec2::zero, 0);
    std::vector<int64_t> B;
    for (int i = 0; i < n; i++) B.push_back(add(s, kind, Vec2(4, i * 0.8), 1, pi));
    Simulation sim(s);
    const Outcome o = fight(sim, {p}, B, 30);
    EXPECT_EQ(o.b, int64_t(n));
    return o.over.value_or(1e9);
}

} // namespace

// MARK: - Peregrine

/// "One Peregrine beats one Kestrel in about 5.5 s and costs 70% as
/// much": 2 × 16 every 1.25 s against the rail gun's 34 every 3.2 s (no
/// armoured bonus on a light flyer). Measured: 5.47 s, 12 hp left.
TEST(balance_aPeregrineBeatsAKestrelInAboutFiveAndAHalfSeconds) {
    EXPECT_NEAR(double(worth(Unit::Kind::peregrine)) / double(worth(Unit::Kind::kestrel)), 0.7, 1e-9);
    GameState s = empty();
    const int64_t p = add(s, Unit::Kind::peregrine, Vec2::zero, 0);
    const int64_t k = add(s, Unit::Kind::kestrel, Vec2(6, 0), 1, pi);
    Simulation sim(s);
    const Outcome o = fight(sim, {p}, {k}, 20);
    ASSERT_TRUE(o.over.has_value());
    EXPECT_TRUE(o.a == 1 && o.b == 0);
    EXPECT_NEAR(*o.over, 5.5, 0.5);
    EXPECT_TRUE(o.hpA < Rules::hp(Unit::Kind::peregrine)); // the rail gun answered
}

/// "Peregrine beats Dropship": an unarmed Dropship is down in about 5 s
/// (measured 5.4).
TEST(balance_aPeregrineBeatsADropship) {
    GameState s = empty();
    const int64_t p = add(s, Unit::Kind::peregrine, Vec2::zero, 0);
    const int64_t d = add(s, Unit::Kind::dropship, Vec2(6, 0), 1, pi);
    Simulation sim(s);
    const Outcome o = fight(sim, {p}, {d}, 20);
    ASSERT_TRUE(o.over.has_value());
    EXPECT_TRUE(o.a == 1 && o.b == 0);
    EXPECT_TRUE(*o.over < 6.5);
    EXPECT_EQ(o.hpA, Rules::hp(Unit::Kind::peregrine));
}

/// "Rangers (about 10 a second each) and Sentinels (24 a second) shoot it
/// down fast". One Ranger: about 8 s for its 80 hp (measured 8.27); three,
/// cheaper than the Peregrine, under 3.5 s (2.57); a Sentinel under 4 s
/// (3.37).
TEST(balance_rangersAndSentinelsShootAPeregrineDownFast) {
    const double one = downPeregrine(Unit::Kind::ranger, 1);
    EXPECT_NEAR(Rules::hp(Unit::Kind::peregrine) / one, 10, 1.0);
    EXPECT_TRUE(3 * worth(Unit::Kind::ranger) <= worth(Unit::Kind::peregrine));
    EXPECT_TRUE(downPeregrine(Unit::Kind::ranger, 3) < 3.5);
    GameState s = empty();
    const int64_t p = add(s, Unit::Kind::peregrine, Vec2::zero, 0);
    const int64_t tower = simtest::addStructure(s, Structure::Kind::sentinel, Vec2(5, 0), 1);
    Simulation sim(s);
    const Outcome o = fight(sim, {p}, {tower}, 20);
    ASSERT_TRUE(o.over.has_value());
    EXPECT_TRUE(o.a == 0 && *o.over < 4);
}

/// "The Hailstorm's bonus does not apply to it, so flak alone is a weak
/// answer": 12 a hit at the Peregrine against 20 at a Kestrel, and one
/// Hailstorm (the Peregrine's own cost) takes over twice as long as three
/// Rangers (cheaper still) and longer than a Sentinel (measured 6.53 s).
TEST(balance_flakAloneIsAWeakAnswerToAPeregrine) {
    const auto& flak = Rules::stats(Unit::Kind::hailstorm);
    EXPECT_EQ(Rules::damage(flak, false, true, 0), 12.0);
    EXPECT_EQ(Rules::damage(flak, true, false, 0), 20.0);
    EXPECT_EQ(worth(Unit::Kind::hailstorm), worth(Unit::Kind::peregrine));
    const double flakTime = downPeregrine(Unit::Kind::hailstorm, 1);
    EXPECT_TRUE(flakTime < 30); // it does get there, slowly
    EXPECT_TRUE(flakTime > 2 * downPeregrine(Unit::Kind::ranger, 3));
    EXPECT_TRUE(flakTime > 4); // a Sentinel needs under 4
}

// MARK: - Atlas

/// "Kestrel beats Atlas": two Kestrels (the Atlas's cost) bring it down in
/// about 16 s (measured 15.6), and it never fires at them.
TEST(balance_kestrelsBeatAnAtlas) {
    EXPECT_EQ(2 * worth(Unit::Kind::kestrel), worth(Unit::Kind::atlas));
    GameState s = empty();
    const std::vector<int64_t> A{add(s, Unit::Kind::kestrel, Vec2::zero, 0), add(s, Unit::Kind::kestrel, Vec2(0, 1.5), 0)};
    const int64_t atlas = add(s, Unit::Kind::atlas, Vec2(5, 0), 1, pi);
    Simulation sim(s);
    const Outcome o = fight(sim, A, {atlas}, 30);
    ASSERT_TRUE(o.over.has_value());
    EXPECT_TRUE(o.b == 0 && o.a == 2);
    EXPECT_NEAR(*o.over, 15.6, 2);
    EXPECT_EQ(o.shotsB, int64_t(0));
    EXPECT_EQ(o.hpA, 2 * Rules::hp(Unit::Kind::kestrel));
}

namespace {

/// An Atlas against `n` of `kind`: `approach` stages both sides 16 cells
/// apart on attack-move, else the enemy stands 6 cells off, in range.
Outcome atlasAgainst(Unit::Kind kind, int n, bool approach) {
    GameState s = empty();
    s.players[0].attack = Vec2(20, 0);
    s.players[1].attack = Vec2::zero;
    const int64_t atlas = add(s, Unit::Kind::atlas, Vec2::zero, 0, 0, approach);
    const auto B = line(s, kind, n, approach ? 16 : 6, 1, approach);
    if (kind == Unit::Kind::scorpion) for (auto& u : s.units) if (u.kind == kind) u.mission = std::nullopt;
    Simulation sim(s);
    return fight(sim, {atlas}, B, 60);
}

void expectAtlasWins(Unit::Kind kind, int n, bool approach) {
    const Outcome o = atlasAgainst(kind, n, approach);
    EXPECT_TRUE(o.over.has_value());
    EXPECT_TRUE(o.a == 1 && o.b == 0);
}

} // namespace

/// "Atlas beats Longbow": one, and two (more than its cost), whether they
/// stand in range or walk in. Measured: one in 4.0 s (432 hp left), two in
/// 10.0 s (160 left); walking in, 6.8 s (364) and 8.8 s (160).
TEST(balance_anAtlasBeatsLongbows) {
    EXPECT_TRUE(2 * worth(Unit::Kind::longbow) >= worth(Unit::Kind::atlas));
    for (const bool approach : {false, true}) {
        expectAtlasWins(Unit::Kind::longbow, 1, approach);
        expectAtlasWins(Unit::Kind::longbow, 2, approach);
    }
}

/// "Atlas beats Juggernaut": one, either way, and four (its cost) that walk
/// into its range, where its 7 cells outreach their 6 (measured 11.4 s,
/// 122 hp left). Four already in range when the fight starts win instead
/// (open question in the doc's "Balance checks"), so that is not asserted.
TEST(balance_anAtlasBeatsJuggernauts) {
    EXPECT_EQ(4 * worth(Unit::Kind::juggernaut), worth(Unit::Kind::atlas));
    expectAtlasWins(Unit::Kind::juggernaut, 1, false);
    expectAtlasWins(Unit::Kind::juggernaut, 1, true);
    expectAtlasWins(Unit::Kind::juggernaut, 4, true);
}

/// "Atlas beats Firefly": one, and five (its cost), either way (measured:
/// five in 18.2 s with 320 hp left, walking in 10.9 s with 410).
TEST(balance_anAtlasBeatsFireflies) {
    EXPECT_EQ(5 * worth(Unit::Kind::firefly), worth(Unit::Kind::atlas));
    for (const bool approach : {false, true}) {
        expectAtlasWins(Unit::Kind::firefly, 1, approach);
        expectAtlasWins(Unit::Kind::firefly, 5, approach);
    }
}

/// "Atlas beats a Scorpion (when seen)": Scorpions on the surface in its
/// sight, one or five (its cost), die without hurting it.
TEST(balance_anAtlasBeatsASeenScorpion) {
    EXPECT_EQ(5 * worth(Unit::Kind::scorpion), worth(Unit::Kind::atlas));
    for (const bool approach : {false, true}) {
        for (const int n : {1, 5}) {
            const Outcome o = atlasAgainst(Unit::Kind::scorpion, n, approach);
            EXPECT_TRUE(o.over.has_value() && o.a == 1 && o.b == 0);
            EXPECT_EQ(o.hpA, Rules::hp(Unit::Kind::atlas));
        }
    }
}

// MARK: - Scorpion

/// "A Scorpion beats clumped light units, Prospectors": one sting and its
/// splash kill two Rangers or two Prospectors standing together (its cost
/// in Rangers), and it lives.
TEST(balance_aScorpionBeatsClumpedLightUnits) {
    EXPECT_EQ(2 * worth(Unit::Kind::ranger), worth(Unit::Kind::scorpion));
    for (const auto kind : {Unit::Kind::ranger, Unit::Kind::prospector}) {
        GameState s = empty();
        const int64_t c = addBuried(s, Vec2::zero, 0);
        const auto B = clump(s, kind, 2, Vec2(4, 0), 1);
        Simulation sim(s);
        const Outcome o = fight(sim, {c}, B, 10);
        ASSERT_TRUE(o.over.has_value());
        EXPECT_TRUE(o.a == 1 && o.b == 0);
        EXPECT_EQ(o.shotsA, int64_t(1));
        EXPECT_TRUE(*o.over < 2);
    }
}

/// "... and careless flyers": a Dropship or a Peregrine that hangs about
/// in its reach (the stung one comes to sit right above it) is down by the
/// second sting; a Kestrel shoots one lone Scorpion dead between stings,
/// but two (less than the Kestrel's cost) kill it with their first.
TEST(balance_aScorpionBeatsCarelessFlyers) {
    for (const auto kind : {Unit::Kind::dropship, Unit::Kind::peregrine}) {
        GameState s = empty();
        const int64_t c = addBuried(s, Vec2::zero, 0);
        const int64_t f = add(s, kind, Vec2(4, 0), 1, pi);
        Simulation sim(s);
        const Outcome o = fight(sim, {c}, {f}, 40);
        ASSERT_TRUE(o.over.has_value());
        EXPECT_TRUE(o.a == 1 && o.b == 0);
        EXPECT_EQ(o.shotsA, int64_t(2));
        EXPECT_NEAR(*o.over, 1 + Rules::stats(Unit::Kind::scorpion).cooldown + 1.2, 1);
    }
    EXPECT_TRUE(2 * worth(Unit::Kind::scorpion) < worth(Unit::Kind::kestrel));
    GameState s = empty();
    const std::vector<int64_t> A{addBuried(s, Vec2::zero, 0), addBuried(s, Vec2(0, 1.2), 0)};
    const int64_t k = add(s, Unit::Kind::kestrel, Vec2(4, 0.6), 1, pi);
    Simulation sim(s);
    const Outcome o = fight(sim, A, {k}, 10);
    ASSERT_TRUE(o.over.has_value());
    EXPECT_TRUE(o.a == 2 && o.b == 0 && *o.over < 2);
}

/// "... beaten by anything that spots it first": with a Sentinel's sight
/// on it, a Longbow walking in shoots it from beyond the sting's reach and
/// is never stung; without one it is stung (89) before it finds it. Four
/// Rangers' reach is the sting's, so it stings them first either way and
/// kills at least three (open question in the doc's "Balance checks"; with
/// the longer sight of docs/shooter-feel.md the Sentinel's lets them walk
/// in clumped, and the sting takes all four).
TEST(balance_aScorpionLosesToWhatSpotsItFirst) {
    for (const bool tower : {false, true}) {
        GameState s = empty();
        s.players[1].attack = Vec2(-20, 0);
        const int64_t c = addBuried(s, Vec2::zero, 0);
        if (tower) simtest::addStructure(s, Structure::Kind::sentinel, Vec2(9, 4), 1);
        const int64_t l = add(s, Unit::Kind::longbow, Vec2(14, 0), 1, pi, true);
        Simulation sim(s);
        const Outcome o = fight(sim, {c}, {l}, 30);
        ASSERT_TRUE(o.over.has_value());
        EXPECT_TRUE(o.a == 0 && o.b == 1);
        EXPECT_EQ(o.shotsA, int64_t(tower ? 0 : 1));
        if (tower) EXPECT_EQ(o.hpB, Rules::hp(Unit::Kind::longbow));
    }
    for (const bool tower : {false, true}) {
        GameState s = empty();
        s.players[1].attack = Vec2(-20, 0);
        const int64_t c = addBuried(s, Vec2::zero, 0);
        if (tower) simtest::addStructure(s, Structure::Kind::sentinel, Vec2(9, 4), 1);
        std::vector<int64_t> B;
        for (int i = 0; i < 4; i++) B.push_back(add(s, Unit::Kind::ranger, Vec2(12 + (i % 2) * 0.8, (i / 2) * 0.8), 1, pi, true));
        Simulation sim(s);
        const Outcome o = fight(sim, {c}, B, 30);
        ASSERT_TRUE(o.over.has_value());
        EXPECT_TRUE(o.shotsA >= 1);
        EXPECT_TRUE(o.b <= 1);
    }
}

// MARK: - Overlap

/// A flyer right above a ground unit overlaps it (edge to edge below 0)
/// and is still in reach of a weapon with no minimum range. Before
/// `Simulation::gap` stopped at 0, nothing on the ground ever picked it: a
/// Peregrine that chased an unseen sting to its spot sat over the Scorpion
/// for good, and neither shot at the other.
TEST(balance_antiAirShootsAFlyerRightAboveIt) {
    for (const auto gun : {Unit::Kind::ranger, Unit::Kind::hailstorm, Unit::Kind::scorpion}) {
        GameState s = empty();
        const int64_t g = gun == Unit::Kind::scorpion ? addBuried(s, Vec2::zero, 0) : add(s, gun, Vec2::zero, 0);
        const int64_t f = add(s, Unit::Kind::dropship, Vec2(0.3, 0), 1);
        Simulation sim(s);
        const auto events = simtest::run(sim, 2);
        EXPECT_TRUE(shots(events, g) > 0);
        EXPECT_TRUE(life(sim, f) < Rules::hp(Unit::Kind::dropship));
    }
}
