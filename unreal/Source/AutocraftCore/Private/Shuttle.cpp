// The walk-in Dropship's shape and pose (C4, Shuttle.h).

#include "Shuttle.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ac {

namespace {

constexpr double kUp = 1;

Vec3 up(double z) { return Vec3(0, 0, z * kUp); }
Vec3 flat(Vec3 v) { return Vec3(v.x, v.y, 0); }
double len(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
Vec3 unit(Vec3 v) {
    const double l = len(v);
    return l > 1e-12 ? v / l : Vec3(1, 0, 0);
}

/// A cubic Hermite curve from `p0` (tangent `m0`) to `p1` (tangent `m1`),
/// `u` 0..1; tangents per unit of `u`.
Vec3 hermite(Vec3 p0, Vec3 m0, Vec3 p1, Vec3 m1, double u) {
    const double u2 = u * u, u3 = u2 * u;
    return p0 * (2 * u3 - 3 * u2 + 1) + m0 * (u3 - 2 * u2 + u) + p1 * (-2 * u3 + 3 * u2) + m1 * (u3 - u2);
}

double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

double wrap(double a) {
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}

/// The flight's way (as F2's cinematic flight drew it, in cells with z up):
/// from the boarding pad `a` (the ship's heading `g` there) to the target
/// pad `b`. `t`: seconds since the lift began.
struct Way {
    Vec3 a, b, g;
    Ferry::Plan plan;

    Vec3 way() const {
        const Vec3 d = flat(b - a);
        return len(d) > 30 ? unit(d) : g;
    }
    Vec3 side() const {
        const Vec3 w = way();
        return Vec3(-w.y, w.x, 0);
    }
    double liftEnd() const { return plan.lift; }
    double turnsEnd() const { return plan.lift + plan.turns; }
    double dashEnd() const { return turnsEnd() + plan.dash; }
    double end() const { return dashEnd() + plan.approach; }

    void turnPoints(Vec3 p[4]) const {
        const Vec3 w = way();
        p[0] = a + up(10);
        p[1] = p[0] + up(5) + g * 18;
        p[3] = p[0] + up(22) + g * 20 + w * 26;
        p[2] = p[3] - w * 16 - up(4);
    }
    Vec3 approachFrom() const { return b - way() * 60 + up(40); }
    Vec3 dashOut() const {
        Vec3 p[4];
        turnPoints(p);
        return (p[3] - p[2]) * (3.0 / plan.turns * plan.dash);
    }
    Vec3 dashIn() const {
        Vec3 p[4];
        turnPoints(p);
        return way() * (len(p[3] - approachFrom()) * 0.9) - up(4);
    }

    Vec3 pos(double t) const {
        if (t < liftEnd()) {
            const double u = clamp01(t / plan.lift);
            return a + up(10) * (u * u);
        }
        if (t < turnsEnd()) {
            Vec3 p[4];
            turnPoints(p);
            const double u = (t - liftEnd()) / plan.turns, v = 1 - u;
            const Vec3 bez = p[0] * (v * v * v) + p[1] * (3 * v * v * u) + p[2] * (3 * v * u * u) + p[3] * (u * u * u);
            // Two banks: a weave across the way, none at either end.
            return bez + side() * (7 * std::sin(2 * pi * u) * 4 * u * v);
        }
        if (t < dashEnd()) {
            Vec3 p[4];
            turnPoints(p);
            return hermite(p[3], dashOut(), approachFrom(), dashIn(), (t - turnsEnd()) / plan.dash);
        }
        const double u = clamp01((t - dashEnd()) / plan.approach);
        // In at the dash's last speed, slowing to settle on the pad.
        return hermite(approachFrom(), dashIn() / plan.dash * plan.approach, b, up(-2), u);
    }

    /// Its turn along the way: the flat velocity's yaw (the way's at a
    /// standstill), the nose up or down with half the climb, banked by how
    /// fast the way turns.
    ShuttlePose pose(double t, double heading) const {
        ShuttlePose out;
        out.at = pos(t);
        if (t < liftEnd()) {
            out.yaw = heading;
            return out;
        }
        const double h = 0.05;
        auto velocity = [&](double q) { return pos(std::min(q + h, end() - 0.3)) - pos(std::min(q - h, end() - 0.4)); };
        const Vec3 wv = way();
        const double wayYaw = std::atan2(wv.y, wv.x);
        auto yawAt = [&](double q) {
            const Vec3 v = velocity(q);
            return len(flat(v)) > 0.02 ? std::atan2(v.y, v.x) : wayYaw;
        };
        const Vec3 v = velocity(t);
        const double f = len(flat(v));
        // The touch down: its nose on the way it lands along.
        out.yaw = t >= dashEnd() ? wayYaw : f > 0.02 ? std::atan2(v.y, v.x) : wayYaw;
        out.pitch = f > 0.02 ? std::max(-15.0, std::min(18.0, std::atan2(v.z, f) * 180 / pi * 0.5)) * pi / 180 : 0;
        const double rate = wrap(yawAt(t + 0.15) - yawAt(t - 0.15)) / 0.3;
        out.roll = std::max(-40.0 * pi / 180, std::min(40.0 * pi / 180, rate * 0.6));
        return out;
    }
};

Way wayOf(const Shuttle& s) {
    return Way{s.from, s.to, Vec3(std::cos(s.fromHeading), std::sin(s.fromHeading), 0), s.plan};
}

Block box(double x0, double x1, double y0, double y1, double z0, double z1) {
    return Block{Vec2((x0 + x1) / 2, (y0 + y1) / 2), Vec2((x1 - x0) / 2, (y1 - y0) / 2), 0, z0, z1};
}

/// The solids a walker meets (the shut ramp last).
const std::array<Block, 6>& solids() {
    using B = ShuttleBay;
    static const std::array<Block, 6> all{
        // The sides (the walls, the jambs, the nacelles) and the nose.
        box(B::hullRear, B::hullFront, B::halfWide, B::hullHalfWide, 0, B::roofTop),
        box(B::hullRear, B::hullFront, -B::hullHalfWide, -B::halfWide, 0, B::roofTop),
        box(B::noseFrom, B::hullFront, -B::halfWide, B::halfWide, 0, B::roofTop),
        // The console under the windshield, the ramp post in its niche.
        box(B::front, 2.68, -0.96, 0.96, B::deck, 1.44),
        box(-2.42, -2.10, -1.10, -0.88, B::deck, 1.78),
        // The ramp shut in the opening.
        box(-2.70, B::rear, -B::halfWide, B::halfWide, 0, B::ceiling),
    };
    return all;
}

} // namespace

double ShuttleBay::rampFoot() { return rear - std::sqrt(rampLength * rampLength - deck * deck); }

Vec2 ShuttleBay::local(const Shuttle& s, Vec2 p) {
    const Vec2 d = p - s.position;
    const double c = std::cos(s.heading), n = std::sin(s.heading);
    return Vec2(d.x * c + d.y * n, -d.x * n + d.y * c);
}

Vec2 ShuttleBay::world(const Shuttle& s, Vec2 l) {
    const double c = std::cos(s.heading), n = std::sin(s.heading);
    return s.position + Vec2(l.x * c - l.y * n, l.x * n + l.y * c);
}

bool ShuttleBay::inBay(Vec2 l, double grow) {
    return l.x > rear + grow && l.x < front - grow && std::abs(l.y) < halfWide - grow;
}

bool ShuttleBay::onRamp(Vec2 l, double grow) {
    return l.x > rampFoot() - grow && l.x <= rear + grow && std::abs(l.y) < halfWide + grow;
}

bool ShuttleBay::covers(Vec2 l, double grow) {
    const bool hull = l.x > hullRear - grow && l.x < hullFront + grow && std::abs(l.y) < hullHalfWide + grow;
    return hull || onRamp(l, grow);
}

double ShuttleBay::floor(Vec2 l, double rampOpen) {
    if (inBay(l)) return deck;
    if (!onRamp(l) || l.x > rear) return 0;
    // The ramp turns 104.5° about its hinge from upright to lying down: its
    // slope below the level, and the height over the ground at `l`.
    const double down = std::asin(deck / rampLength);
    const double angle = (pi / 2) - clamp01(rampOpen) * (pi / 2 + down);
    if (angle >= 0) return 0;
    return std::max(0.0, deck + (rear - l.x) * std::tan(angle));
}

std::optional<Block> ShuttleBay::solidAt(Vec2 l, double grow, bool rampIsDown) {
    const std::array<Block, 6>& all = solids();
    for (size_t i = 0; i < all.size(); i++) {
        if (i == all.size() - 1 && rampIsDown) break;
        if (all[i].contains(l, grow)) return all[i];
    }
    return std::nullopt;
}

const Footprint& ShuttleBay::footprint() {
    static const Footprint f = [] {
        Footprint out;
        out.radius = std::max({hullFront, -hullRear, hullHalfWide});
        const std::array<Block, 6>& all = solids();
        for (size_t i = 0; i < 3; i++) out.walls.push_back(all[i].bounds());
        // The deck under the bay and over the ramp's hinge, the ceiling.
        out.walls.push_back(Slab{Vec2(rear, -halfWide), Vec2(noseFrom, halfWide), 0.25, deck});
        out.walls.push_back(Slab{Vec2(hullRear, -halfWide), Vec2(noseFrom, halfWide), ceiling, roofTop});
        out.door = Slab{Vec2(-2.70, -0.98), Vec2(rear, 0.98), 0.52, 2.48};
        out.interior = Slab{Vec2(rear, -halfWide), Vec2(front, halfWide), deck, ceiling};
        out.roof = Slab{Vec2(rear, -halfWide), Vec2(front, halfWide), ceiling, roofTop};
        out.furniture.push_back(all[3]);
        out.furniture.push_back(all[4]);
        return out;
    }();
    return f;
}

double shuttleStageSeconds(const Shuttle& s, Shuttle::Stage stage) {
    switch (stage) {
    case Shuttle::Stage::descend: return Ferry::descend;
    case Shuttle::Stage::ground: return 0;
    case Shuttle::Stage::lift: return s.plan.lift;
    case Shuttle::Stage::turns: return s.plan.turns;
    case Shuttle::Stage::dash: return s.plan.dash;
    case Shuttle::Stage::approach: return s.plan.approach;
    case Shuttle::Stage::depart: return ShuttleBay::departSeconds;
    }
    return 0;
}

double shuttleLandingHeading(const Shuttle& s) {
    const Vec3 w = wayOf(s).way();
    return std::atan2(w.y, w.x);
}

ShuttlePose shuttlePose(const Shuttle& s) { return shuttlePose(s, s.stage, s.clock); }

ShuttlePose shuttlePose(const Shuttle& s, Shuttle::Stage stage, double clock) {
    const Vec3 here(s.position.x, s.position.y, s.ground);
    const Vec3 nose(std::cos(s.heading), std::sin(s.heading), 0);
    ShuttlePose out;
    out.at = here;
    out.yaw = s.heading;
    switch (stage) {
    case Shuttle::Stage::descend: {
        // In from high behind the pad, slowing, down on its retro jets.
        const double u = clamp01(clock / Ferry::descend);
        const Vec3 from = here - nose * 45 + up(32);
        out.at = hermite(from, (here - from) * 1.4 + up(4), here, up(-1.5), u);
        return out;
    }
    case Shuttle::Stage::ground: return out;
    case Shuttle::Stage::depart: {
        // Up off the pad and away along its nose, the nose lifting.
        const double u = clamp01(clock / ShuttleBay::departSeconds);
        out.at = here + up(30 * u * u) + nose * (45 * u * u * u);
        out.pitch = 10 * pi / 180 * std::min(1.0, u * 3);
        return out;
    }
    default: break;
    }
    const Way w = wayOf(s);
    double t = clock;
    if (stage >= Shuttle::Stage::turns) t += s.plan.lift;
    if (stage >= Shuttle::Stage::dash) t += s.plan.turns;
    if (stage >= Shuttle::Stage::approach) t += s.plan.dash;
    return w.pose(t, s.fromHeading);
}

} // namespace ac
