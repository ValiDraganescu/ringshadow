// C4 (docs/haul-and-ferry.md section 3): a test's way aboard the walk-in
// Dropship before a flight: called at the pad its Prospector stands on, down
// with its ramp, the ramp shut from its button, the Prospector at the
// take-off button. The tests that fly (F2 and after) start here.
#pragma once

#include "Shuttle.h"
#include "WorldMap.h"

#include <optional>
#include <string>

namespace shuttletest {

using namespace ac;
using namespace ac::server;

inline Unit* drivenBy(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

/// The world stepped `seconds` (the pilot's unit as it stands).
inline void run(WorldMap& w, double seconds) {
    const int ticks = int(seconds / w.start().description().step + 0.5);
    for (int i = 0; i < ticks; i++) w.step();
}

/// Seconds from the take-off to the touch down.
inline double flightSeconds(const Ferry::Plan& plan) { return plan.lift + plan.turns + plan.dash + plan.approach; }

/// The Dropship nearest the pilot's unit in its region (nil: none).
inline Shuttle* nearestShip(WorldMap& w, PilotId p) {
    Unit* u = drivenBy(w, p);
    if (!u) return nullptr;
    Shuttle* best = nullptr;
    for (Shuttle& s : w.regionOf(p)->simulation().state.shuttles)
        if (!best || distance(s.position, u->position) < distance(best->position, u->position)) best = &s;
    return best;
}

/// The pilot's keys let go (a test's last drive may hold the walk, its
/// inputs buffered ahead): the world on until those are used up, then an
/// input just past them (the test's next input after it is dropped once).
inline void still(WorldMap& w, PilotId p) {
    for (size_t i = 0; i <= Region::maxBuffered; i++) w.step();
    PilotInput in;
    in.tick = w.regionOf(p)->lastInputTick(p) + 1;
    in.visit = w.visit(p);
    if (const Unit* u = drivenBy(w, p)) in.heading = u->heading;
    w.input(p, in);
    w.step();
}

/// The pilot's Prospector put at `local` in the frame of the nearest ship,
/// standing still.
inline void standIn(WorldMap& w, PilotId p, Vec2 local) {
    Shuttle* s = nearestShip(w, p);
    Unit* u = drivenBy(w, p);
    if (!s || !u) return;
    still(w, p);
    u->position = ShuttleBay::world(*s, local);
    w.step();
}

/// Where a Prospector stands to press the ramp button, and the take-off
/// button (the ship's frame).
inline constexpr Vec2 kAtRampButton = ShuttleBay::rampStand;
inline constexpr Vec2 kAtConsole = ShuttleBay::helmStand;

/// Call, land, in, shut, to the console. The refusal on the way, or nil:
/// ready for `WorldMap::fly`.
inline std::optional<std::string> board(WorldMap& w, PilotId p) {
    const Vec2 at = drivenBy(w, p) ? drivenBy(w, p)->position : Vec2(0, 0);
    still(w, p);
    if (Unit* u = drivenBy(w, p)) u->position = at;
    if (auto why = w.callShuttle(p)) return why;
    run(w, Ferry::descend + ShuttleBay::rampSeconds + 0.2);
    standIn(w, p, kAtRampButton);
    if (auto why = w.shuttleRamp(p, false)) return why;
    run(w, ShuttleBay::rampSeconds + 0.2);
    standIn(w, p, kAtConsole);
    return std::nullopt;
}

/// `board`, then the take-off for `region`'s `pad`.
inline std::optional<std::string> boardAndFly(WorldMap& w, PilotId p, const std::string& region, const std::string& pad) {
    if (auto why = board(w, p)) return why;
    return w.fly(p, region, pad);
}

/// At the touch down: the ramp down from its button, the Prospector out
/// behind the ship by `beyond` cells past the ramp's foot.
inline std::optional<std::string> walkOut(WorldMap& w, PilotId p, double beyond = 1.5) {
    standIn(w, p, kAtRampButton);
    if (auto why = w.shuttleRamp(p, true)) return why;
    run(w, ShuttleBay::rampSeconds + 0.2);
    standIn(w, p, Vec2(ShuttleBay::rampFoot() - beyond, 0));
    return std::nullopt;
}

} // namespace shuttletest
