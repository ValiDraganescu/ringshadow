// The walk-in Dropship on the server (C4, docs/haul-and-ferry.md section 3;
// WorldMap.h): called down onto a realm Landing Pad, its ramp worked from
// inside, launched from its console (`WorldMap::fly`, WorldMap.cpp), moved
// with its riders into the target region at the dash's end, and forgotten
// once it has lifted away empty. The ship itself, its stages and its walk are
// the core's (`Shuttle`, Simulation+Shuttle.cpp).

#include "WorldMap.h"

#include "Shuttle.h"
#include "WorldAtlas.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ac::server {

Vec2 WorldMap::groundIn(size_t from, Vec2 g, size_t to) const {
    if (!world_ || from == to) return g;
    net::WorldMessage m;
    for (const WorldRegion& r : world_->regions) {
        m.columns = std::max(m.columns, r.column + 1);
        m.rows = std::max(m.rows, r.row + 1);
        net::WorldMessage::Region out;
        out.id = r.file.id;
        out.size = r.file.size;
        out.column = r.column;
        out.row = r.row;
        m.regions.push_back(out);
    }
    const std::vector<net::atlas::Place> places = net::atlas::layout(m, {});
    const net::atlas::Place* a = net::atlas::placeOf(places, int64_t(from));
    const net::atlas::Place* b = net::atlas::placeOf(places, int64_t(to));
    if (!a || !b) return g;
    return b->toGround(a->fromGround(g));
}

Shuttle* WorldMap::shipOf(const ShuttleTrip& t) {
    if (t.region >= regions_.size()) return nullptr;
    return regions_[t.region]->simulation().shuttle(t.id);
}

const WorldMap::ShuttleTrip* WorldMap::riderTrip(const PilotState& p) const {
    if (p.flight || !p.unit || p.region >= regions_.size()) return nullptr;
    for (const ShuttleTrip& t : trips_) {
        if (!t.flying || !t.to || t.region != p.region) continue;
        const Shuttle* s = regions_[t.region]->simulation().shuttle(t.id);
        if (s && s->rides(*p.unit)) return &t;
    }
    return nullptr;
}

std::optional<int64_t> WorldMap::shuttleAt(size_t r, int64_t pad) const {
    for (const ShuttleTrip& t : trips_) {
        if (t.region != r || t.pad != pad || t.flying) continue;
        const Shuttle* s = regions_[r]->simulation().shuttle(t.id);
        if (s && s->stage != Shuttle::Stage::depart) return t.id;
    }
    return std::nullopt;
}

std::optional<std::pair<size_t, int64_t>> WorldMap::shuttleOf(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_ || it->second.flight) return std::nullopt;
    const PilotState& p = it->second;
    const Simulation& sim = regions_[p.region]->simulation();
    if (sim.state.shuttles.empty()) return std::nullopt;
    const auto unit = regions_[p.region]->driving(pilot);
    const auto u = unit ? sim.state.unit(*unit) : std::nullopt;
    if (!u) return std::nullopt;
    const Shuttle* s = sim.aboard(*u);
    if (!s) return std::nullopt;
    return std::pair{p.region, s->id};
}

bool WorldMap::riding(PilotId pilot) const {
    const auto at = shuttleOf(pilot);
    if (!at) return false;
    const Shuttle* s = regions_[at->first]->simulation().shuttle(at->second);
    return s && s->flying();
}

std::optional<std::string> WorldMap::callShuttle(PilotId pilot) {
    if (!world_) return "not a world";
    PilotState* p = pilotState(pilot);
    if (!p) return "not joined";
    if (p->flight || riding(pilot)) return "In flight";
    Region& here = *regions_[p->region];
    Simulation& sim = here.simulation();
    const auto unit = here.driving(pilot);
    const auto u = unit ? sim.state.unit(*unit) : std::nullopt;
    if (!u || !(u->hp > 0)) return "Nothing driven";
    // On a Landing Pad here: within Pad link's reach of its centre (a
    // Spacedock: also within its edge and `buildingPadReach`), as F2.
    const double reach = padReach(*p);
    std::optional<LandingPad> pad;
    double best = std::numeric_limits<double>::infinity();
    for (const LandingPad& o : landingPads()) {
        if (o.region != p->region) continue;
        const double d = distance(o.at, u->position);
        const double within = o.kind == StructureKind::spacedock ? std::max(reach, Rules::radius(o.kind) + WorldRoles::buildingPadReach) : reach;
        if (d <= within && d < best) {
            best = d;
            pad = o;
        }
    }
    if (!pad) return std::string("Not at a Landing Pad: stand on one to call the Dropship");
    if (const auto id = shuttleAt(p->region, pad->building)) {
        const Shuttle* s = sim.shuttle(*id);
        return std::string(s && s->stage == Shuttle::Stage::descend ? "The Dropship is on its way" : "The Dropship is here");
    }
    // Its ramp toward the caller: the nose points from the caller over the
    // pad (on the pad's middle: the way the caller looks, turned round);
    // turned a quarter at a time until its hull and ramp lie inside the
    // region's ground, when one way does.
    const Vec2 d = pad->at - u->position;
    const double toward = length(d) > 0.5 ? std::atan2(d.y, d.x) : u->heading + pi;
    double heading = toward;
    if (sim.map)
        for (int k = 0; k < 4; k++) {
            Shuttle probe;
            probe.position = pad->at;
            probe.heading = toward + k * pi / 2;
            const GroundRect b = sim.map->bounds;
            bool inside = true;
            for (const Vec2 c : {Vec2(ShuttleBay::rampFoot(), ShuttleBay::hullHalfWide), Vec2(ShuttleBay::rampFoot(), -ShuttleBay::hullHalfWide),
                                 Vec2(ShuttleBay::hullFront, ShuttleBay::hullHalfWide), Vec2(ShuttleBay::hullFront, -ShuttleBay::hullHalfWide)}) {
                const Vec2 q = ShuttleBay::world(probe, c);
                inside = inside && q.x > b.minX + 1 && q.x < b.maxX - 1 && q.y > b.minZ + 1 && q.y < b.maxZ - 1;
            }
            if (inside) {
                heading = probe.heading;
                break;
            }
        }
    ShuttleTrip t;
    t.region = p->region;
    t.id = sim.callShuttle(pad->at, heading);
    t.pad = pad->building;
    t.padKind = pad->kind;
    t.padName = pad->name;
    trips_.push_back(t);
    return std::nullopt;
}

std::optional<std::string> WorldMap::shuttleRamp(PilotId pilot, bool open) {
    if (!world_) return "not a world";
    PilotState* p = pilotState(pilot);
    if (!p || p->flight) return "Not in a world";
    Simulation& sim = regions_[p->region]->simulation();
    const auto unit = regions_[p->region]->driving(pilot);
    const auto u = unit ? sim.state.unit(*unit) : std::nullopt;
    if (!u || !(u->hp > 0)) return "Nothing driven";
    const Shuttle* s = sim.aboard(*u);
    if (!s) return std::string("Not in a Dropship");
    return sim.ramp(u->id, s->id, open);
}

std::optional<std::string> WorldMap::launch(ShuttleTrip& t, int64_t unit, const LandingPad& target, const Ferry::Plan& plan) {
    Region& here = *regions_[t.region];
    Simulation& sim = here.simulation();
    // The target pad on this region's ground (the world's grid), its height
    // its own ground's.
    const Vec2 there = groundIn(target.region, target.at, t.region);
    const double z = regions_[target.region]->simulation().groundZ(target.at);
    if (auto why = sim.takeOff(unit, t.id, plan, Vec3(there.x, there.y, z), target.region == t.region)) return why;
    t.to = target.region;
    t.toPad = target.name;
    t.toBuilding = target.building;
    t.toAt = target.at;
    t.switched = target.region == t.region;
    t.flying = true;
    t.humans.clear();
    const Shuttle* lifted = sim.shuttle(t.id);
    for (const auto& [id, q] : pilots_)
        if (lifted && q.region == t.region && !q.flight)
            if (const auto d = here.driving(id); d && lifted->rides(*d)) {
                t.humans.push_back(id);
                // Its World message again: where it flies.
                moves_.push_back(Moved{id, std::nullopt});
            }
    return std::nullopt;
}

void WorldMap::shuttleSwitch(ShuttleTrip& t) {
    const size_t from = t.region, to = *t.to;
    Region& a = *regions_[from];
    Region& b = *regions_[to];
    Simulation& sb = b.simulation();
    const Shuttle ship = *shipOf(t);
    // The frame on the target pad, its nose along the way; the flight's
    // ends on the target region's ground.
    Shuttle there = ship;
    there.id = sb.state.nextShuttle++;
    there.position = t.toAt;
    there.heading = shuttleLandingHeading(ship);
    there.ground = sb.groundZ(t.toAt);
    const Vec2 shift = groundIn(from, Vec2(0, 0), to);
    there.from = ship.from + Vec3(shift.x, shift.y, 0);
    there.to = Vec3(t.toAt.x, t.toAt.y, there.ground);
    there.riders.clear();
    sb.state.shuttles.push_back(there);
    const double turn = there.heading - ship.heading;
    // A Mule's rider goes with its Mule (`arriveMule`), not on its own.
    std::set<int64_t> onMules;
    for (const auto& [pid, q] : pilots_)
        if (!q.flight && q.region == from && q.unit && drivesMule(pid, q)) onMules.insert(*q.unit);
    for (const int64_t id : ship.riders) {
        if (onMules.count(id)) continue;
        const auto u = a.simulation().state.unit(id);
        if (!u) continue;
        const Vec2 at = ShuttleBay::world(there, ShuttleBay::local(ship, u->position));
        std::optional<int64_t> now;
        // A human's: the pilot goes with it, its inputs walking on.
        for (auto& [pid, q] : pilots_) {
            if (q.flight || q.region != from || a.driving(pid) != id) continue;
            // Driving its Mule: the Mule and its rider together (the
            // Prospector's clocks go along), as at an edge.
            const bool mule = drivesMule(pid, q) && q.unit;
            const auto clocks = takeClocks(from, mule ? *q.unit : id);
            std::optional<Unit> rider = mule ? a.release(*q.unit) : std::nullopt;
            Unit leaving = *a.release(id);
            leaving.heading += turn;
            if (rider) rider->heading += turn;
            const int64_t lastInput = a.lastInputTick(pid);
            const std::optional<Region::Inputs> inputs = a.inputs(pid);
            q.record = a.levels(pid);
            a.leave(pid);
            if (a.pilots().empty()) bankIn(PilotState{}, from);
            if (rider) {
                arriveMule(pid, q, to, leaving, *rider, at, lastInput, true);
                if (Shuttle* s = sb.shuttle(there.id); s && q.unit) s->riders.push_back(*q.unit);
            } else {
                arrive(pid, q, to, leaving, at, lastInput, true);
            }
            q.walkedIn = true;
            if (inputs) b.carryOn(pid, *inputs);
            if (q.unit) giveClocks(to, *q.unit, clocks);
            regionChanged(pid, q, to);
            now = rider && q.mule ? q.mule->unit : q.unit;
            break;
        }
        // A frontier pilot's: its pilot into the region with it.
        if (!now)
            for (FrontierPilot& f : frontier_) {
                if (f.region != from || f.unit != id) continue;
                Unit leaving = *a.release(id);
                leaving.position = at;
                leaving.heading += turn;
                f.region = to;
                f.unit = b.admit(leaving);
                f.at = at;
                now = f.unit;
                break;
            }
        // Anything else aboard (a parked Mule, the Prospector of a pilot gone
        // off line): over as it stands, an away pilot's still its own.
        if (!now) {
            Unit leaving = *a.release(id);
            leaving.position = at;
            leaving.heading += turn;
            now = b.admit(leaving);
            for (auto& [key, q] : away_)
                if (q.region == from && q.unit == id) {
                    q.region = to;
                    q.unit = now;
                }
            // A parked Mule stays its owner's.
            auto own = [&](PilotState& q) {
                if (q.mule && q.mule->region == from && q.mule->unit == id) {
                    q.mule->region = to;
                    q.mule->unit = now;
                }
            };
            for (auto& [pid, q] : pilots_) own(q);
            for (auto& [key, q] : away_) own(q);
        }
        if (Shuttle* s = sb.shuttle(there.id)) s->riders.push_back(*now);
    }
    std::erase_if(a.simulation().state.shuttles, [&](const Shuttle& s) { return s.id == ship.id; });
    t.region = to;
    t.id = there.id;
    t.switched = true;
}

void WorldMap::shuttlesStep() {
    for (size_t i = 0; i < trips_.size();) {
        ShuttleTrip& t = trips_[i];
        Shuttle* s = shipOf(t);
        if (!s) {
            trips_.erase(trips_.begin() + std::ptrdiff_t(i));
            continue;
        }
        // The dash's end: into the target region, riders and all.
        if (t.flying && !t.switched && t.to && s->stage == Shuttle::Stage::approach) {
            shuttleSwitch(t);
            s = shipOf(t);
            if (!s) continue;
        }
        // The touch down: a first landing (and a first visit) for each pilot
        // aboard; the ship stands on the target pad now.
        if (t.flying && s->grounded()) {
            t.flying = false;
            const std::string region = world_->regions[t.region].file.id;
            for (const PilotId h : t.humans) {
                const auto it = pilots_.find(h);
                if (it == pilots_.end() || it->second.region != t.region) continue;
                firstLanding(h, region + "/" + t.toPad);
                firstVisit(h, region);
                moves_.push_back(Moved{h, std::nullopt});
            }
            t.pad = t.toBuilding;
            t.padName = t.toPad;
            for (const LandingPad& o : landingPads())
                if (o.region == t.region && o.building == t.pad) t.padKind = o.kind;
            t.to = std::nullopt;
            t.humans.clear();
        }
        i++;
    }
}

// MARK: - The frontier pilots fly it (docs/frontier.md, "Dropship")

bool WorldMap::frontierCall(FrontierPilot& f, const LandingPad& from, size_t to) {
    std::optional<LandingPad> there;
    for (const LandingPad& l : landingPads())
        if (l.region == to && !there) there = l;
    if (!there || to == f.region) return false;
    const int64_t steps = ::ac::Flight::steps(world_->regions[f.region].column, world_->regions[f.region].row, world_->regions[to].column,
                                              world_->regions[to].row);
    const int64_t price = ::ac::Flight::price(steps, from.kind == StructureKind::spacedock, false, projectOn(RealmProject::Kind::beacon));
    if (::ac::Flight::refusal(price, f.wallet.hydrogen)) return false;
    const Unit* u = frontierUnitOf(f);
    if (!u) return false;
    Simulation& sim = regions_[f.region]->simulation();
    std::optional<int64_t> ship = shuttleAt(f.region, from.building);
    if (!ship) {
        const Vec2 d = from.at - u->position;
        ShuttleTrip t;
        t.region = f.region;
        t.id = sim.callShuttle(from.at, length(d) > 0.5 ? std::atan2(d.y, d.x) : u->heading + pi);
        t.pad = from.building;
        t.padKind = from.kind;
        t.padName = from.name;
        trips_.push_back(t);
        ship = t.id;
    }
    f.ride = FrontierPilot::Ride{*ship, *there, price, FrontierPilot::Ride::Step::called, 0};
    f.doing = WorldFrontier::Doing::walking;
    return true;
}

bool WorldMap::frontierRide(FrontierPilot& f, double dt) {
    using Step = FrontierPilot::Ride::Step;
    using B = ShuttleBay;
    FrontierPilot::Ride& r = *f.ride;
    Simulation& sim = regions_[f.region]->simulation();
    Unit* u = frontierUnitOf(f);
    // In flight the ship is the one it rides (its id changes with the
    // region switch); else the one it called or landed in.
    const Shuttle* s = !u ? nullptr : r.step == Step::flying ? sim.aboard(*u) : sim.shuttle(r.ship);
    r.since += dt;
    if (!u || !s || (r.step < Step::flying && r.since > 60)) {
        f.ride = std::nullopt;
        return false;
    }
    r.ship = s->id;
    // The server's hand on it: none of its own errands meanwhile.
    u->task = Unit::Task::idle;
    u->goal = std::nullopt;
    u->waypoints = std::nullopt;
    u->mission = std::nullopt;
    u->order = std::nullopt;
    f.doing = r.step == Step::flying ? WorldFrontier::Doing::flying : WorldFrontier::Doing::walking;
    // A walk straight to `local` in the ship's frame at a Prospector's
    // pace: true when there.
    auto walkTo = [&](Vec2 local) {
        const Vec2 goal = B::world(*s, local);
        const Vec2 d = goal - u->position;
        const double left = length(d), stride = Rules::prospectorSpeed * dt;
        if (left <= stride) {
            u->position = goal;
            return true;
        }
        u->position += d / left * stride;
        u->heading = std::atan2(d.y, d.x);
        return false;
    };
    auto next = [&](Step step) {
        r.step = step;
        r.since = 0;
    };
    // Where it stands to press each (in the bay, clear of the opening).
    const Vec2 rampButton(-1.7, -0.4), console(B::takeOffButton.x - 0.9, 0);
    switch (r.step) {
    case Step::called:
        if (s->grounded() && s->ramp.open >= B::rampDown) next(Step::walkIn);
        break;
    case Step::walkIn: {
        // Round the hull to behind the ramp's foot, up it, to the ramp
        // button.
        const Vec2 l = B::local(*s, u->position);
        const double foot = B::rampFoot() - 1.2;
        if (B::inBay(l) || B::onRamp(l)) {
            if (walkTo(rampButton)) next(Step::shut);
        } else if (l.x > foot) {
            walkTo(Vec2(foot, (l.y < 0 ? -1 : 1) * (B::hullHalfWide + 1)));
        } else {
            walkTo(Vec2(foot, 0));
        }
        break;
    }
    case Step::shut:
        // Pressed until nobody stands on the ramp, then to the console.
        if (s->ramp.opening) sim.ramp(u->id, s->id, false);
        else if (s->ramp.open <= 0) next(Step::console);
        break;
    case Step::console: {
        if (!walkTo(console)) break;
        const auto trip = std::find_if(trips_.begin(), trips_.end(), [&](const ShuttleTrip& t) { return t.region == f.region && t.id == s->id; });
        if (trip == trips_.end() || ::ac::Flight::refusal(r.price, f.wallet.hydrogen)) {
            f.ride = std::nullopt;
            return false;
        }
        const int64_t steps = ::ac::Flight::steps(world_->regions[f.region].column, world_->regions[f.region].row,
                                                  world_->regions[r.target.region].column, world_->regions[r.target.region].row);
        if (launch(*trip, u->id, r.target, Ferry::plan(steps))) break;
        f.wallet.hydrogen -= r.price;
        next(Step::flying);
        break;
    }
    case Step::flying:
        // The switch moved it (`shuttleSwitch`); down on the target pad, out.
        if (s->grounded()) next(Step::open);
        break;
    case Step::open:
        if (!walkTo(rampButton)) break;
        if (!s->ramp.opening) sim.ramp(u->id, s->id, true);
        if (s->ramp.open >= B::rampDown) next(Step::walkOut);
        break;
    case Step::walkOut:
        if (!walkTo(Vec2(B::rampFoot() - 2, 0))) break;
        f.ride = std::nullopt;
        f.at = u->position;
        f.progress.regions.insert(world_->regions[f.region].file.id);
        f.doing = WorldFrontier::Doing::coming;
        f.thought = -1e9;
        return false;
    }
    return true;
}

} // namespace ac::server
