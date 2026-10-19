// Rounds and practice (Simulation+Citadel.members.h; C2,
// docs/guns-and-practice.md sections 1 and 3): the minigun class's real
// projectiles, the barrels and heat that fire them, the gatling gun's
// ballistic aim, the Target Dummy that falls and stands again.

#include "Simulation.h"

#include "Defence.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <limits>

namespace ac {

namespace {

/// Where segment `p0 + s·d` (s in 0…1) first enters an upright cylinder
/// round `c` of radius `r` from `z0` to `z1`, or nothing.
std::optional<double> enterCylinder(Vec3 p0, Vec3 d, Vec2 c, double r, double z0, double z1) {
    double lo = 0, hi = 1;
    const double fx = p0.x - c.x, fy = p0.y - c.y;
    const double a = d.x * d.x + d.y * d.y, b = 2 * (fx * d.x + fy * d.y), k = fx * fx + fy * fy - r * r;
    if (a < 1e-12) {
        if (k > 0) return std::nullopt;
    } else {
        const double disc = b * b - 4 * a * k;
        if (disc < 0) return std::nullopt;
        const double q = std::sqrt(disc);
        lo = ac::max(lo, (-b - q) / (2 * a));
        hi = ac::min(hi, (-b + q) / (2 * a));
    }
    if (std::abs(d.z) < 1e-12) {
        if (p0.z < z0 || p0.z > z1) return std::nullopt;
    } else {
        double s0 = (z0 - p0.z) / d.z, s1 = (z1 - p0.z) / d.z;
        if (s0 > s1) std::swap(s0, s1);
        lo = ac::max(lo, s0);
        hi = ac::min(hi, s1);
    }
    if (lo > hi) return std::nullopt;
    return lo;
}

/// Where segment `p0 + s·d` first enters the box `lo`…`hi`, or nothing.
std::optional<double> enterBox(Vec3 p0, Vec3 d, Vec3 lo, Vec3 hi) {
    double s0 = 0, s1 = 1;
    for (int k = 0; k < 3; k++) {
        if (std::abs(d[k]) < 1e-12) {
            if (p0[k] < lo[k] || p0[k] > hi[k]) return std::nullopt;
            continue;
        }
        double a = (lo[k] - p0[k]) / d[k], b = (hi[k] - p0[k]) / d[k];
        if (a > b) std::swap(a, b);
        s0 = ac::max(s0, a);
        s1 = ac::min(s1, b);
        if (s0 > s1) return std::nullopt;
    }
    return s0;
}

/// The sweep's buckets, cells on a side.
constexpr double kBucket = 4.0;
/// The ground is sampled this often along a round's path, cells.
constexpr double kGroundStep = 0.5;

} // namespace

double Simulation::groundZ(Vec2 p) const { return field ? field->height(p) : 0; }

double Simulation::groundTop(Vec2 lo, Vec2 hi) const {
    if (!field || !map) return 0;
    const GroundRect& b = map->bounds;
    const int64_t cols = int64_t((b.maxX - b.minX) / groundTile) + 1, rows = int64_t((b.maxZ - b.minZ) / groundTile) + 1;
    if (cols <= 0 || rows <= 0 || cols * rows > (1 << 22)) return std::numeric_limits<double>::infinity();
    if (groundTops.size() != size_t(cols * rows)) groundTops.assign(size_t(cols * rows), std::numeric_limits<double>::quiet_NaN());
    auto cx = [&](double x) { return std::clamp<int64_t>(int64_t(std::floor((x - b.minX) / groundTile)), 0, cols - 1); };
    auto cy = [&](double y) { return std::clamp<int64_t>(int64_t(std::floor((y - b.minZ) / groundTile)), 0, rows - 1); };
    double top = -std::numeric_limits<double>::infinity();
    for (int64_t y = cy(lo.y); y <= cy(hi.y); y++)
        for (int64_t x = cx(lo.x); x <= cx(hi.x); x++) {
            double& t = groundTops[size_t(y * cols + x)];
            if (std::isnan(t)) {
                t = -std::numeric_limits<double>::infinity();
                const int n = int(groundTile);
                for (int j = 0; j <= n; j++)
                    for (int i = 0; i <= n; i++)
                        t = ac::max(t, field->height(Vec2(b.minX + (double(x) + double(i) / n) * groundTile,
                                                          b.minZ + (double(y) + double(j) / n) * groundTile)));
                t += groundSlack;
            }
            top = ac::max(top, t);
        }
    return top;
}

bool Simulation::targetable(const Unit& u, int64_t side) const { return state.hostile(u.owner, side); }

bool Simulation::targetable(const Structure& s, int64_t side) const {
    if (s.kind == StructureKind::dummy) return s.complete() && !s.downUntil && s.hp > 0;
    return state.hostile(s.owner, side);
}

void Simulation::minigun(double& cooldown, double& heat, bool& overheated, double& spin, double& bloom,
                         const UnitStats& w, bool trigger, double dt, std::vector<double>& late) const {
    late.clear();
    heat = ac::max(0.0, heat - Rules::gunCooling * dt);
    if (overheated && heat <= Rules::gunHeatResume) overheated = false;
    if (w.coneRecovery > 0) bloom *= std::pow(0.1, dt / w.coneRecovery);
    const bool firing = trigger && !overheated;
    spin = firing ? ac::min(1.0, spin + dt / Rules::gunSpinUp) : ac::max(0.0, spin - dt / Rules::gunSpinUp);
    if (!firing || !(spin > 0)) {
        // The next pull fires at once.
        cooldown = 0;
        return;
    }
    // The rate ramps with the barrels: this step's share of a round at the
    // rate it has now. A round leaves each time a whole one is due, at its
    // own moment inside the step (`late` seconds before its end).
    const double rate = spin / w.cooldown;
    double due = dt * rate;
    while (due >= cooldown - 1e-12) {
        due -= cooldown;
        late.push_back(due / rate);
        cooldown = 1;
        heat += Rules::gunHeatPerRound;
        if (heat >= 1) {
            heat = 1;
            overheated = true;
            cooldown = 0;
            return;
        }
    }
    cooldown -= due;
}

void Simulation::fireRound(const Structure& s, Vec3 muzzle, double yaw, double pitch, double cone, double damage,
                           double late, std::vector<GameEvent>& events) {
    // The cone roll: uniform over the disc of the cone, seeded by the
    // shooter, the step's time and the round, so a scene replays the same.
    const int64_t id = state.nextRound++;
    const uint64_t salt = uint64_t(id) * 2;
    const double a = 2 * pi * roll(s.id, salt), r = cone * std::sqrt(roll(s.id, salt + 1));
    const double p = pitch + r * std::sin(a);
    const double y = yaw + r * std::cos(a) / ac::max(0.2, std::cos(pitch));
    const Vec3 v = Rules::roundVelocity(y, p);
    state.rounds.push_back(Round{id, s.id, s.owner, muzzle, v, damage, state.time - late});
    events.push_back(GameEvent::Fired{s.id, id, muzzle, v});
}

void Simulation::stepRounds(double dt, std::vector<GameEvent>& events) {
    if (state.rounds.empty()) return;
    // Candidates by bucket (`kBucket` cells), built once a step: every unit
    // that can be struck, every building, in each bucket its footprint
    // touches. A round asks only the buckets its path crosses.
    struct Body { Vec2 c; double r; double z0, z1; size_t index; bool structure; };
    std::vector<Body> bodies;
    bodies.reserve(state.units.size() + state.structures.size());
    for (size_t i = 0; i < state.units.size(); i++) {
        const Unit& u = state.units[i];
        if (!(u.hp > 0) || u.task == Unit::Task::aboard || u.task == Unit::Task::inBastion || u.task == Unit::Task::inDerrick
            || u.task == Unit::Task::atGun) {
            continue;
        }
        // Its body about where it is now (`Unit::bodyHeight`): a flyer at
        // its height, diving or not, never below the ground.
        const UnitStats& st = u.stats();
        const double h = st.height > 0 ? st.height : 1.5;
        const double g = groundZ(u.position);
        const double mid = g + u.bodyHeight();
        bodies.push_back({u.position, st.radius, ac::max(g, mid - h / 2), mid + h / 2, i, false});
    }
    for (size_t i = 0; i < state.structures.size(); i++) {
        const Structure& s = state.structures[i];
        // A dummy lying down lets rounds pass over it.
        if (!(s.hp > 0) || s.downUntil) continue;
        const double g = groundZ(s.position);
        bodies.push_back({s.position, Rules::radius(s.kind), g, g + Rules::height(s.kind), i, true});
    }
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool first = true;
    auto grow = [&](double x0, double y0, double x1, double y1) {
        if (first) { minX = x0; minY = y0; maxX = x1; maxY = y1; first = false; return; }
        minX = ac::min(minX, x0); minY = ac::min(minY, y0); maxX = ac::max(maxX, x1); maxY = ac::max(maxY, y1);
    };
    for (const Body& b : bodies) grow(b.c.x - b.r, b.c.y - b.r, b.c.x + b.r, b.c.y + b.r);
    for (const Round& r : state.rounds) grow(r.position.x, r.position.y, r.position.x, r.position.y);
    const int64_t cols = ac::min<int64_t>(int64_t((maxX - minX) / kBucket) + 1, 1024);
    const int64_t rows = ac::min<int64_t>(int64_t((maxY - minY) / kBucket) + 1, 1024);
    auto col = [&](double x) { return ac::min<int64_t>(ac::max<int64_t>(int64_t(std::floor((x - minX) / kBucket)), 0), cols - 1); };
    auto row = [&](double y) { return ac::min<int64_t>(ac::max<int64_t>(int64_t(std::floor((y - minY) / kBucket)), 0), rows - 1); };
    // Counted, then filled: bucket `c` holds `bucketed[start[c]…start[c+1])`.
    std::vector<uint32_t> start(size_t(cols * rows + 1), 0);
    for (const Body& b : bodies)
        for (int64_t y = row(b.c.y - b.r); y <= row(b.c.y + b.r); y++)
            for (int64_t x = col(b.c.x - b.r); x <= col(b.c.x + b.r); x++) start[size_t(y * cols + x) + 1]++;
    for (size_t k = 1; k < start.size(); k++) start[k] += start[k - 1];
    std::vector<uint32_t> bucketed(start.back());
    std::vector<uint32_t> fill(start.begin(), start.end() - 1);
    for (uint32_t n = 0; n < bodies.size(); n++) {
        const Body& b = bodies[n];
        for (int64_t y = row(b.c.y - b.r); y <= row(b.c.y + b.r); y++)
            for (int64_t x = col(b.c.x - b.r); x <= col(b.c.x + b.r); x++) bucketed[fill[size_t(y * cols + x)]++] = n;
    }
    // Each body is tested once a round, however many buckets it is in.
    std::vector<uint32_t> seen(bodies.size(), 0);
    uint32_t stamp = 0;

    // The shooters' buildings, by id, for the grace over their own ground.
    std::unordered_map<int64_t, size_t> own;
    for (const Round& r : state.rounds)
        if (state.time - dt - r.born < Rules::roundSelfGrace) own.emplace(r.shooter, SIZE_MAX);
    if (!own.empty())
        for (size_t i = 0; i < state.structures.size(); i++)
            if (auto it = own.find(state.structures[i].id); it != own.end()) it->second = i;

    std::vector<Round> kept;
    kept.reserve(state.rounds.size());
    for (Round r : state.rounds) {
        const double t0 = ac::max(r.born, state.time - dt);
        const double t1 = ac::min(state.time, r.born + Rules::roundLife);
        const double h = t1 - t0;
        // (Fired at the very end of this step: it flies from the next.)
        if (h < 0) continue;
        const Vec3 p0 = r.position;
        const Vec3 p1 = Rules::roundAt(p0, r.velocity, h);
        const Vec3 d = p1 - p0;
        // The first thing on the path: the ground, a unit or a building.
        double best = 2;
        GameEvent::Impact::What what = GameEvent::Impact::What::ground;
        std::optional<size_t> victim;
        bool structure = false;
        const bool grace = t0 - r.born < Rules::roundSelfGrace;
        // In its grace, the ground under the shooter's own footprint lets it by.
        std::optional<std::pair<Vec2, double>> home;
        if (grace)
            if (auto it = own.find(r.shooter); it != own.end() && it->second != SIZE_MAX) {
                const Structure& o = state.structures[it->second];
                home = std::pair{o.position, Rules::radius(o.kind)};
            }
        auto under = [&](Vec3 q) {
            if (home && std::abs(q.x - home->first.x) <= home->second && std::abs(q.y - home->first.y) <= home->second) return false;
            return q.z < groundZ(Vec2(q.x, q.y));
        };
        if (ac::min(p0.z, p1.z) <= groundTop(Vec2(ac::min(p0.x, p1.x), ac::min(p0.y, p1.y)), Vec2(ac::max(p0.x, p1.x), ac::max(p0.y, p1.y)))) {
            const double along = std::sqrt(d.x * d.x + d.y * d.y);
            const int n = ac::max(1, int(std::ceil(along / kGroundStep)));
            if (under(p0)) {
                best = 0;
            } else {
                for (int k = 1; k <= n; k++) {
                    const double s = double(k) / n;
                    if (!under(p0 + d * s)) continue;
                    // Between the last sample above and this one below.
                    double lo = double(k - 1) / n, hi = s;
                    for (int b = 0; b < 8; b++) {
                        const double m = (lo + hi) / 2;
                        (under(p0 + d * m) ? hi : lo) = m;
                    }
                    best = hi;
                    break;
                }
            }
        }
        const double rr = Rules::roundRadius;
        stamp++;
        for (int64_t y = row(ac::min(p0.y, p1.y) - rr); y <= row(ac::max(p0.y, p1.y) + rr); y++)
            for (int64_t x = col(ac::min(p0.x, p1.x) - rr); x <= col(ac::max(p0.x, p1.x) + rr); x++) {
                const size_t cell = size_t(y * cols + x);
                for (uint32_t k = start[cell]; k < start[cell + 1]; k++) {
                    const uint32_t n = bucketed[k];
                    if (seen[n] == stamp) continue;
                    seen[n] = stamp;
                    const Body& b = bodies[n];
                    std::optional<double> s;
                    if (!b.structure) {
                        s = enterCylinder(p0, d, b.c, b.r + rr, b.z0 - rr, b.z1 + rr);
                        // Sheltered in the hall (C1): nothing reaches it.
                        if (s && *s < best && sheltered(state.units[b.index])) s = std::nullopt;
                    } else {
                        const Structure& o = state.structures[b.index];
                        if (grace && o.id == r.shooter) continue;
                        const Footprint& f = Rules::footprint(o.kind);
                        if (o.complete() && (f.interior || f.door)) {
                            // A hall Citadel: its walls, its door while shut, its roof.
                            auto slab = [&](const Slab& l) {
                                const auto e = enterBox(p0, d, Vec3(o.position.x + l.min.x - rr, o.position.y + l.min.y - rr, b.z0 + l.z0 - rr),
                                                        Vec3(o.position.x + l.max.x + rr, o.position.y + l.max.y + rr, b.z0 + l.z1 + rr));
                                if (e && (!s || *e < *s)) s = e;
                            };
                            for (const Slab& l : f.walls) slab(l);
                            if (f.door && !doorOpen(o)) slab(*f.door);
                            if (f.roof) slab(*f.roof);
                        } else {
                            // Its bounding square by its body height (a
                            // scaffold by what has risen of it).
                            const double top = b.z0 + (b.z1 - b.z0) * ac::max(0.1, o.progress());
                            s = enterBox(p0, d, Vec3(b.c.x - b.r - rr, b.c.y - b.r - rr, b.z0 - rr), Vec3(b.c.x + b.r + rr, b.c.y + b.r + rr, top + rr));
                        }
                    }
                    if (s && *s < best) {
                        best = *s;
                        victim = b.index;
                        structure = b.structure;
                        what = structure ? GameEvent::Impact::What::structure : GameEvent::Impact::What::unit;
                    }
                }
            }
        if (best <= 1) {
            const Vec3 at = p0 + d * best;
            int64_t id = 0;
            if (victim) {
                id = structure ? state.structures[*victim].id : state.units[*victim].id;
                const bool aimed = structure ? targetable(state.structures[*victim], r.owner) : targetable(state.units[*victim], r.owner);
                // A friend's stops the round, unhurt (no friendly fire).
                if (aimed) strike(r, id);
            }
            events.push_back(GameEvent::Impact{r.id, at, what, id});
            continue;
        }
        r.position = p1;
        r.velocity.z -= Rules::roundGravity * h;
        // Gone with age, or out of the region.
        if (t1 >= r.born + Rules::roundLife) continue;
        if (map && (p1.x < map->bounds.minX || p1.x > map->bounds.maxX || p1.y < map->bounds.minZ || p1.y > map->bounds.maxZ)) continue;
        kept.push_back(r);
    }
    state.rounds = std::move(kept);
}

void Simulation::strike(const Round& r, int64_t id) {
    const auto t = target(id);
    if (!t) return;
    // The round's damage less the armour, at least half a point, as a gun
    // hit (`Rules::damage`).
    const double amount = ac::max(0.5, r.damage - t->armor);
    const auto bug = state.unit(id);
    const double had = bug && bug->bug ? bug->hp : 0;
    hit(id, amount);
    if (bug && bug->bug) {
        // The director pays the gun's side and turns the group on it.
        if (const auto after = state.unit(id)) {
            bugHits.push_back({r.shooter, id, bug->kind, ac::max(0.0, had - ac::max(0.0, after->hp)), had > 0 && after->hp <= 0});
            if (auto it = unitIndexByID.find(id); it != unitIndexByID.end()) {
                if (auto& b = state.units[size_t(it->second)].bug) b->hitBy = r.shooter;
            }
        }
    }
}

double Simulation::aimHeight(int64_t id) const {
    if (const auto u = state.unit(id)) return groundZ(u->position) + u->bodyHeight();
    if (const auto o = state.structure(id)) return groundZ(o->position) + Rules::height(o->kind) / 2;
    return 0;
}

std::optional<double> Simulation::gatlingPitch(const Structure& s, Vec2 at, double z, double radius) const {
    const double base = groundZ(s.position);
    const double yaw = std::atan2(at.y - s.position.y, at.x - s.position.x);
    // Solved from the barrels' pivot: the muzzle lies on the barrels' line
    // through it, so a bug at the gun's foot (nearer than the muzzle's
    // reach) is aimed at as well as one far out.
    const Vec2 pivot = s.position + Vec2(std::cos(yaw), std::sin(yaw)) * Defence::gatlingTrunnion;
    auto p = Rules::ballisticPitch(distance(pivot, at), z - (base + Defence::gatlingPivot));
    if (!p || *p < Defence::gatlingPitchMin || *p > Defence::gatlingPitchMax) return std::nullopt;
    // The arc must clear the ground up to the target's edge: a gun on a
    // ledge does not fire into its own lip at what is below it.
    Vec3 m = Defence::gatlingMuzzle(s.position, yaw, *p);
    m.z += base;
    const Vec3 v = Rules::roundVelocity(yaw, *p);
    const double along = std::sqrt(v.x * v.x + v.y * v.y);
    const double ahead = distance(Vec2(m.x, m.y), at) - radius;
    for (double d = kGroundStep; d < ahead; d += kGroundStep) {
        const Vec3 q = Rules::roundAt(m, v, d / along);
        if (q.z < groundZ(Vec2(q.x, q.y)) + Rules::roundRadius) return std::nullopt;
    }
    return p;
}

void Simulation::stepGatling(Structure& s, const UnitStats& w, double dt, std::vector<GameEvent>& events) {
    const double edge = Rules::radius(s.kind);
    // Only a target its rounds can reach over the ground between.
    const std::function<bool(const Target&, int64_t)> clear = [&](const Target& t, int64_t id) {
        return gatlingPitch(s, t.position, aimHeight(id), t.radius).has_value();
    };
    std::optional<int64_t> target = gunTarget(s, s.position, edge, w, s.target, &clear);
    s.target = target;
    double aim = s.aim.value_or(pi / 2);
    double cooldown = s.cooldown.value_or(0);
    bool trigger = false;
    // Nothing to shoot: the barrels come back to level.
    const double turn_ = turretTurnRate * dt;
    double pitch = s.pitch > 0 ? ac::max(0.0, s.pitch - turn_) : ac::min(0.0, s.pitch + turn_);
    const double base = groundZ(s.position);
    if (target) {
        if (const auto t = foe(*target, s.owner)) {
            // Lead: where the target will be when a round gets there (twice
            // round: the flight time from the led point).
            Vec2 v{0, 0};
            const double z = aimHeight(*target);
            if (const auto u = state.unit(*target)) {
                if (u->walking()) v = Vec2(std::cos(u->heading), std::sin(u->heading)) * (Rules::speed(u->kind) * (u->bug ? u->bug->pace : 1));
            }
            Vec2 at = t->position;
            for (int k = 0; k < 2; k++) {
                const double flight = distance(s.position, at) / Rules::roundSpeed;
                at = t->position + v * flight;
            }
            const double want = std::atan2(at.y - s.position.y, at.x - s.position.x);
            const double diff = std::remainder(want - aim, 2 * pi);
            aim = std::remainder(aim + ac::max(-turn_, ac::min(turn_, diff)), 2 * pi);
            if (const auto p = gatlingPitch(s, at, z, t->radius)) {
                pitch = *p;
                trigger = std::abs(std::remainder(want - aim, 2 * pi)) < gatlingAimGate;
            }
        }
    }
    std::vector<double> late;
    minigun(cooldown, s.heat, s.overheated, s.spin, s.bloom, w, trigger, dt, late);
    if (!late.empty()) {
        Vec3 m = Defence::gatlingMuzzle(s.position, aim, pitch);
        m.z += base;
        for (double l : late) {
            fireRound(s, m, aim, pitch, w.coneStand + s.bloom, w.damage, l, events);
            s.bloom += w.coneFire;
        }
    }
    s.aim = aim;
    s.pitch = pitch;
    s.cooldown = cooldown;
}

void Simulation::stepDummies() {
    for (Structure& s : state.structures) {
        if (s.kind != StructureKind::dummy || !s.downUntil || state.time < *s.downUntil - 1e-9) continue;
        s.downUntil = std::nullopt;
        s.hp = fullHP(s);
        s.hits = 0;
    }
}

} // namespace ac
