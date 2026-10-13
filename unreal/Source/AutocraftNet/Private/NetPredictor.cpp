#include "NetPredictor.h"

#include <algorithm>
#include <cmath>

namespace ac::net {

namespace {

/// As the server takes an input (`server::Region::input`).
double clampUnit(double v) { return std::isfinite(v) ? std::clamp(v, -1.0, 1.0) : 0.0; }

} // namespace

void Predictor::reset() {
    unit_ = std::nullopt;
    pending_.clear();
    seenSnapshot_ = -1;
    offset_ = Vec2(0, 0);
    turn_ = 0;
}

void Predictor::advance(Mirror& mirror, Unit& u, const PilotInput& in) const {
    Simulation& sim = mirror.simulation();
    Pilot p(u.id, std::isfinite(in.heading) ? in.heading : u.heading);
    p.walk = Vec2(clampUnit(in.walk.x), clampUnit(in.walk.y));
    // The server's step for a driven unit (`Simulation::stepPiloted`),
    // its facing and walking.
    u.goal = std::nullopt;
    u.waypoints = std::nullopt;
    sim.pilotFace(u, p, settings_.step);
    sim.pilotWalk(u, p.walk, settings_.step);
}

void Predictor::input(Mirror& mirror, const PilotInput& in) {
    pending_.push_back(in);
    while (pending_.size() > settings_.keep) pending_.pop_front();
    if (unit_) advance(mirror, *unit_, in);
}

void Predictor::reconcile(Mirror& mirror) {
    const std::optional<int64_t> driven = mirror.driven();
    const Unit* truth = driven ? mirror.unit(*driven) : nullptr;
    if (!truth) {
        unit_ = std::nullopt;
        offset_ = Vec2(0, 0);
        turn_ = 0;
        return;
    }
    const bool fresh = !unit_ || unit_->id != *driven;
    if (!fresh && mirror.snapshotId() == seenSnapshot_) return;
    seenSnapshot_ = mirror.snapshotId();
    // What the server used is done with.
    const int64_t used = mirror.lastInputTick();
    while (!pending_.empty() && pending_.front().tick <= used) pending_.pop_front();
    Unit next = *truth;
    for (const PilotInput& in : pending_) advance(mirror, next, in);
    if (fresh) {
        unit_ = next;
        offset_ = Vec2(0, 0);
        turn_ = 0;
        return;
    }
    // Where it is drawn stays where it was; the offset eases it over.
    const double error = distance(unit_->position, next.position);
    if (error > 1e-9) {
        stats_.corrections += 1;
        stats_.errorSum += error;
        stats_.errorMax = std::max(stats_.errorMax, error);
    }
    const Vec2 offset = unit_->position + offset_ - next.position;
    const double turn = std::remainder(unit_->heading + turn_ - next.heading, 2 * pi);
    const bool jump = length(offset) > settings_.snap;
    if (jump) stats_.snaps += 1;
    offset_ = jump ? Vec2(0, 0) : offset;
    turn_ = std::abs(turn) > settings_.snapTurn ? 0 : turn;
    unit_ = next;
}

void Predictor::ease(double dt) {
    if (!(dt > 0)) return;
    const double keep = std::exp(-dt / settings_.ease);
    const Vec2 before = offset_;
    offset_ *= keep;
    turn_ *= keep;
    if (length(offset_) < 1e-4) offset_ = Vec2(0, 0);
    if (std::abs(turn_) < 1e-4) turn_ = 0;
    if (unit_) {
        stats_.easedSum += distance(before, offset_);
        stats_.frames += 1;
    }
}

std::optional<Unit> Predictor::shown() const {
    if (!unit_) return std::nullopt;
    Unit u = *unit_;
    u.position += offset_;
    u.heading = std::remainder(u.heading + turn_, 2 * pi);
    return u;
}

void Predictor::draw(Mirror& mirror) const {
    const std::optional<Unit> u = shown();
    if (!u) return;
    Simulation& sim = mirror.simulation();
    if (const std::optional<int64_t> i = sim.unitIndex(u->id)) sim.state.units[size_t(*i)] = *u;
}

} // namespace ac::net
