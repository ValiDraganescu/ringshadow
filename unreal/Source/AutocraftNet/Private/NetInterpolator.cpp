#include "NetInterpolator.h"

#include <cmath>

namespace ac::net {

namespace {

double blendAngle(double a, double b, double t) {
    const double d = std::remainder(b - a, 2 * 3.14159265358979323846);
    return a + d * t;
}

} // namespace

void Interpolator::heard(int64_t serverTick, double now) {
    const double sample = double(serverTick) * step - now;
    if (!offset_ || std::abs(sample - *offset_) > 0.5) {
        offset_ = sample;
        return;
    }
    // The earliest arrivals tell the clock best (the least delay on the
    // way): it follows them up quickly and drifts down slowly.
    const double k = sample > *offset_ ? 0.25 : 0.02;
    *offset_ += (sample - *offset_) * k;
}

std::optional<double> Interpolator::renderTick(double now) const {
    if (!offset_) return std::nullopt;
    return (now + *offset_ - delay) / step;
}

void Interpolator::record(int64_t serverTick, const std::vector<Unit>& units) {
    if (!frames_.empty() && serverTick < frames_.back().tick) return;  // late: it is in the past already
    if (frames_.empty() || frames_.back().tick != serverTick) {
        frames_.push_back(Frame{serverTick, {}});
    }
    Frame& f = frames_.back();
    f.poses.clear();
    f.poses.reserve(units.size());
    for (const Unit& u : units) f.poses[u.id] = Pose{u.position, u.heading, u.aim, u.stride};
    const int64_t oldest = serverTick - int64_t(std::ceil(keep / step));
    while (frames_.size() > 2 && frames_.front().tick < oldest) frames_.pop_front();
}

void Interpolator::apply(double tick, std::vector<Unit>& units, std::optional<int64_t> skip) const {
    if (frames_.empty()) return;
    // The records either side of `tick`.
    const Frame* a = &frames_.front();
    const Frame* b = &frames_.front();
    double t = 0;
    if (tick >= double(frames_.back().tick)) {
        a = b = &frames_.back();
    } else if (tick > double(frames_.front().tick)) {
        for (size_t i = 1; i < frames_.size(); i++) {
            if (double(frames_[i].tick) >= tick) {
                a = &frames_[i - 1];
                b = &frames_[i];
                t = (tick - double(a->tick)) / double(b->tick - a->tick);
                break;
            }
        }
    }
    for (Unit& u : units) {
        if (skip && u.id == *skip) continue;
        const auto ia = a->poses.find(u.id);
        const auto ib = b->poses.find(u.id);
        const Pose* pa = ia == a->poses.end() ? nullptr : &ia->second;
        const Pose* pb = ib == b->poses.end() ? nullptr : &ib->second;
        if (!pa && !pb) continue;  // newer than both: as the newest snapshot has it
        if (!pa) pa = pb;
        if (!pb) pb = pa;
        u.position = pa->position + (pb->position - pa->position) * t;
        u.heading = blendAngle(pa->heading, pb->heading, t);
        if (pa->aim && pb->aim) u.aim = blendAngle(*pa->aim, *pb->aim, t);
        else u.aim = t < 0.5 ? pa->aim : pb->aim;
        u.stride = pa->stride + (pb->stride - pa->stride) * t;
    }
}

void Interpolator::reset() {
    frames_.clear();
    offset_.reset();
}

} // namespace ac::net
