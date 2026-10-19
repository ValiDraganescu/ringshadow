// The walk-in Dropship in the simulation (C4, docs/haul-and-ferry.md
// section 3; Simulation+Shuttle.members.h): its stages, its ramp, the walk
// in its bay, the riders carried, the buttons' refusals.

#include "Mule.h"
#include "Shuttle.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {

bool onFoot(const Unit& u) { return u.hp > 0 && !u.airborne(); }

/// The circles a walker's body stands on at `p`, each of radius `r`: its
/// middle, and a hauler's two ends (the Mule, `Mule::length` long along its
/// heading), so its nose and tail meet the bay's walls too.
std::vector<Vec2> bodyAt(const Unit& u, Vec2 p, double r) {
    if (!u.stats().hauler) return {p};
    const double off = std::max(0.0, Mule::length / 2 - r);
    const Vec2 d(std::cos(u.heading), std::sin(u.heading));
    return {p, p + d * off, p - d * off};
}

/// `u`'s body seen from above in the frame of `s`: a hauler's box, else its
/// walk's circle as a square.
Block bodyIn(const Shuttle& s, const Unit& u) {
    const Vec2 half = u.stats().hauler ? Vec2(Mule::length / 2, Mule::width / 2) : Vec2(1, 1) * (Rules::radius(u.kind) * 0.8);
    return Block{ShuttleBay::local(s, u.position), half, u.heading - s.heading};
}

/// A button at `b` (the ship's frame) is within reach of `u`: from its
/// body's nearest point.
bool bodyReaches(const Shuttle& s, const Unit& u, Vec3 b) {
    const Block body = bodyIn(s, u);
    return length(body.nearest(Vec2(b.x, b.y)) - Vec2(b.x, b.y)) <= ShuttleBay::reach;
}

/// `u`'s body reaches over the ramp's strip, behind the opening.
bool bodyOverRamp(const Shuttle& s, const Unit& u) {
    const Block body = bodyIn(s, u);
    const Slab box = body.bounds();
    return box.min.x < ShuttleBay::rear - 0.05 && box.max.x > ShuttleBay::rampFoot() && box.min.y < ShuttleBay::halfWide &&
           box.max.y > -ShuttleBay::halfWide;
}

} // namespace

int64_t Simulation::callShuttle(Vec2 at, double heading) {
    Shuttle s;
    s.id = state.nextShuttle++;
    s.position = at;
    s.heading = heading;
    s.ground = groundZ(at);
    s.stage = Shuttle::Stage::descend;
    s.from = Vec3(at.x, at.y, s.ground);
    s.to = s.from;
    s.fromHeading = heading;
    state.shuttles.push_back(s);
    return s.id;
}

Shuttle* Simulation::shuttle(int64_t id) {
    for (Shuttle& s : state.shuttles)
        if (s.id == id) return &s;
    return nullptr;
}

const Shuttle* Simulation::shuttle(int64_t id) const {
    for (const Shuttle& s : state.shuttles)
        if (s.id == id) return &s;
    return nullptr;
}

const Shuttle* Simulation::aboard(const Unit& u) const {
    for (const Shuttle& s : state.shuttles) {
        if (s.flying()) {
            if (s.rides(u.id)) return &s;
            continue;
        }
        if (!s.grounded() || u.airborne()) continue;
        const Vec2 l = ShuttleBay::local(s, u.position);
        if (ShuttleBay::inBay(l) || ShuttleBay::onRamp(l)) return &s;
    }
    return nullptr;
}

std::optional<std::string> Simulation::rampRefusal(const Shuttle& s, const Unit& u, bool open) const {
    if (s.flying()) return std::string("Wait for the landing");
    if (!s.grounded()) return std::string("No ramp to work here");
    const Vec2 l = ShuttleBay::local(s, u.position);
    if (!ShuttleBay::inBay(l) || !bodyReaches(s, u, ShuttleBay::rampButton)) return std::string("Too far from the ramp button");
    if (open) return std::nullopt;
    // Anyone whose body (its walk's circle, a Mule's box) reaches past the
    // opening's edge over the ramp's strip.
    for (const Unit& v : state.units)
        if (onFoot(v) && bodyOverRamp(s, v)) return std::string("Someone is on the ramp");
    return std::nullopt;
}

std::optional<std::string> Simulation::ramp(int64_t unit, int64_t id, bool open) {
    const auto i = unitIndex(unit);
    if (!i) return std::string("No such unit");
    Shuttle* s = shuttle(id);
    if (!s) return std::string("No Dropship here");
    if (auto why = rampRefusal(*s, state.units[size_t(*i)], open)) return why;
    s->ramp.opening = open;
    // Shut by hand: it stays (nobody leaves by it); open again, it may.
    s->leaving = false;
    return std::nullopt;
}

std::optional<std::string> Simulation::takeOffRefusal(const Shuttle& s, const Unit& u) const {
    if (s.flying()) return std::string("In flight");
    if (!s.grounded()) return std::string("Wait for the landing");
    const Vec2 l = ShuttleBay::local(s, u.position);
    if (!ShuttleBay::inBay(l) || !bodyReaches(s, u, ShuttleBay::takeOffButton)) return std::string("Too far from the take-off button");
    if (s.ramp.open > 0 || s.ramp.opening) return std::string("Close the ramp first");
    return std::nullopt;
}

std::optional<std::string> Simulation::takeOff(int64_t unit, int64_t id, const Ferry::Plan& plan, Vec3 to, bool hop) {
    const auto i = unitIndex(unit);
    if (!i) return std::string("No such unit");
    Shuttle* s = shuttle(id);
    if (!s) return std::string("No Dropship here");
    if (auto why = takeOffRefusal(*s, state.units[size_t(*i)])) return why;
    s->riders.clear();
    for (const Unit& v : state.units)
        if (onFoot(v) && ShuttleBay::inBay(ShuttleBay::local(*s, v.position))) s->riders.push_back(v.id);
    s->stage = Shuttle::Stage::lift;
    s->clock = 0;
    s->plan = plan;
    s->from = Vec3(s->position.x, s->position.y, s->ground);
    s->fromHeading = s->heading;
    s->to = to;
    s->hop = hop;
    s->idle = 0;
    s->leaving = false;
    return std::nullopt;
}

void Simulation::moveShuttle(Shuttle& s, Vec2 at, double heading, double ground) {
    std::vector<std::pair<size_t, Vec2>> carried;
    for (const int64_t id : s.riders)
        if (const auto i = unitIndex(id)) carried.push_back({size_t(*i), ShuttleBay::local(s, state.units[size_t(*i)].position)});
    const double turn = heading - s.heading;
    s.position = at;
    s.heading = heading;
    s.ground = ground;
    for (const auto& [i, l] : carried) {
        Unit& u = state.units[i];
        u.position = ShuttleBay::world(s, l);
        u.heading += turn;
        u.goal = std::nullopt;
        u.waypoints = std::nullopt;
    }
}

void Simulation::clearShuttlePad(const Shuttle& s) {
    using B = ShuttleBay;
    for (Unit& u : state.units) {
        if (!onFoot(u) || s.rides(u.id)) continue;
        const double r = Rules::radius(u.kind) + 0.05;
        const Vec2 l = B::local(s, u.position);
        if (!(l.x > B::hullRear - r && l.x < B::hullFront + r && std::abs(l.y) < B::hullHalfWide + r)) continue;
        // Out by the nearest side of the hull.
        const double side = B::hullHalfWide + r - std::abs(l.y);
        const double back = l.x - (B::hullRear - r);
        const double ahead = B::hullFront + r - l.x;
        Vec2 out = l;
        if (side <= back && side <= ahead) out.y = (l.y < 0 ? -1 : 1) * (B::hullHalfWide + r);
        else if (back <= ahead) out.x = B::hullRear - r;
        else out.x = B::hullFront + r;
        u.position = B::world(s, out);
        u.goal = std::nullopt;
        u.waypoints = std::nullopt;
    }
}

void Simulation::stepShuttles(double dt) {
    using B = ShuttleBay;
    for (Shuttle& s : state.shuttles) {
        s.clock += dt;
        switch (s.stage) {
        case Shuttle::Stage::descend:
            if (s.clock < Ferry::descend) break;
            // The touch down: the pad cleared, the ramp lowering.
            s.stage = Shuttle::Stage::ground;
            s.clock = 0;
            s.ramp = Door{0, true};
            clearShuttlePad(s);
            break;
        case Shuttle::Stage::ground: {
            const double target = s.ramp.opening ? 1.0 : 0.0;
            const double step = dt / B::rampSeconds;
            s.ramp.open = s.ramp.open < target ? std::min(target, s.ramp.open + step) : std::max(target, s.ramp.open - step);
            bool someone = false;
            for (const Unit& u : state.units) {
                if (!onFoot(u)) continue;
                const Vec2 l = B::local(s, u.position);
                if (B::inBay(l)) s.boarded = true;
                if (B::inBay(l) || B::onRamp(l)) someone = true;
            }
            if (someone) {
                s.idle = 0;
                // Someone came while it was shutting to leave: it waits.
                if (s.leaving) {
                    s.leaving = false;
                    s.ramp.opening = true;
                }
                break;
            }
            s.idle += dt;
            if (!s.leaving && s.idle >= (s.boarded ? B::leaveSeconds : B::waitSeconds)) {
                s.leaving = true;
                s.ramp.opening = false;
            }
            if ((s.leaving || s.idle >= B::leaveSeconds) && s.ramp.open <= 0 && !s.ramp.opening) {
                s.stage = Shuttle::Stage::depart;
                s.clock = 0;
            }
            break;
        }
        case Shuttle::Stage::lift:
        case Shuttle::Stage::turns:
        case Shuttle::Stage::dash: {
            const double end = shuttleStageSeconds(s, s.stage);
            if (s.clock < end) break;
            s.clock -= end;
            s.stage = Shuttle::Stage(uint8_t(s.stage) + 1);
            // A hop on this region's ground: the frame to the target pad as
            // the approach starts (the server moves it between regions).
            if (s.stage == Shuttle::Stage::approach && s.hop) moveShuttle(s, Vec2(s.to.x, s.to.y), shuttleLandingHeading(s), s.to.z);
            break;
        }
        case Shuttle::Stage::approach:
            if (s.clock < s.plan.approach) break;
            // The touch down on the target pad, the ramp shut.
            s.stage = Shuttle::Stage::ground;
            s.clock = 0;
            s.ramp = Door{0, false};
            clearShuttlePad(s);
            s.riders.clear();
            s.boarded = true;
            s.idle = 0;
            break;
        case Shuttle::Stage::depart: break;
        }
    }
    std::erase_if(state.shuttles, [](const Shuttle& s) { return s.stage == Shuttle::Stage::depart && s.clock >= ShuttleBay::departSeconds; });
}

Simulation::ShuttleWalk Simulation::shuttleWalk(const Unit& u, Vec2 p, double r) const {
    using B = ShuttleBay;
    for (const Shuttle& s : state.shuttles) {
        const std::vector<Vec2> body = bodyAt(u, p, r);
        if (s.flying()) {
            if (!s.rides(u.id)) continue;
            // A rider walks only in the bay, whatever lies under the pad.
            for (const Vec2 q : body)
                if (B::solidAt(B::local(s, q), r, false)) return ShuttleWalk::blocked;
            return B::inBay(B::local(s, p)) ? ShuttleWalk::deck : ShuttleWalk::blocked;
        }
        if (!s.grounded()) continue;
        const Vec2 l = B::local(s, p);
        if (std::none_of(body.begin(), body.end(), [&](Vec2 q) { return B::covers(B::local(s, q), r); })) continue;
        const bool down = s.ramp.open >= B::rampDown;
        for (const Vec2 q : body)
            if (B::solidAt(B::local(s, q), r, down)) return ShuttleWalk::blocked;
        if (B::inBay(l) || (down && B::onRamp(l))) return ShuttleWalk::deck;
    }
    return ShuttleWalk::none;
}

std::optional<Vec2> Simulation::shuttleContact(const Unit& u, Vec2 from, Vec2 to, double r) const {
    using B = ShuttleBay;
    for (const Shuttle& s : state.shuttles) {
        if (s.flying() ? !s.rides(u.id) : !s.grounded()) continue;
        const bool down = !s.flying() && s.ramp.open >= B::rampDown;
        for (const Vec2 q : bodyAt(u, to, r))
            if (const auto b = B::solidAt(B::local(s, q), r, down)) return B::world(s, b->nearest(B::local(s, from + (q - to))));
    }
    return std::nullopt;
}

} // namespace ac
