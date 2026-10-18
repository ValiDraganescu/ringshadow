// The Brood in the core (docs/creatures.md): each bug's step by its orders
// (`Bug`), and the Brood's numbers (Brood.h). The server's brood director
// decides the orders: which site, which prey, which raid, when to leave.

#include "Brood.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ac {

double Brood::wanderSpeed(UnitKind k) {
    switch (k) {
    case UnitKind::shardling: return 1.6;
    case UnitKind::gloamwing: return 2.0;
    case UnitKind::lodeback: return 1.8;
    case UnitKind::matriarch: return 0.8;
    default: return Rules::speed(k);
    }
}

double Brood::marchSpeed(UnitKind k) {
    switch (k) {
    // A pack on its way trots: faster than its wander, slower than its chase.
    case UnitKind::shardling: return 2.8;
    case UnitKind::gloamwing: return 5.0;
    case UnitKind::lodeback: return 1.8;
    case UnitKind::matriarch: return 0.8;
    default: return Rules::speed(k);
    }
}

double Brood::buildingDamage(UnitKind k) {
    switch (k) {
    case UnitKind::shardling: return 3;
    case UnitKind::gloamwing: return 6;
    case UnitKind::matriarch: return 60;
    default: return 0;
    }
}

double Brood::xp(UnitKind k) {
    switch (k) {
    case UnitKind::shardling: return 20;
    case UnitKind::gloamwing: return 30;
    case UnitKind::lodeback: return 50;
    case UnitKind::matriarch: return 2500;
    default: return 0;
    }
}

bool Unit::airborne() const {
    if (!stats().air) return false;
    return !(bug && bug->diveFrom);
}

namespace {

double paceFor(const Unit& u, double speed) { return std::clamp(speed / std::max(0.01, Rules::speed(u.kind)), 0.05, 1.0); }

} // namespace

Vec2 Simulation::bugWanderSpot(const Unit& u) const {
    const Bug& b = *u.bug;
    for (uint64_t k = 0; k < 6; k++) {
        const double a = roll(u.id, 101 + k) * 2 * pi;
        const double r = std::sqrt(roll(u.id, 201 + k)) * b.siteRadius * 0.8;
        const Vec2 p = b.site + Vec2(std::cos(a), std::sin(a)) * r;
        if (u.stats().air || !nav || (nav->walkable(p) && !nav->inRock(p))) return p;
    }
    return b.site;
}

void Simulation::bugClimb(Unit& u, double dt) const {
    Bug& b = *u.bug;
    double want = 0;
    if (u.stats().air) want = b.state == Bug::State::dead ? 0 : b.diveFrom ? Brood::diveHeight : Brood::hoverHeight;
    const double step = Brood::climbRate * dt;
    b.height = b.height < want ? std::min(want, b.height + step) : std::max(want, b.height - step);
}

bool Simulation::bugAttack(Unit& u, int64_t id, double dt) {
    Bug& b = *u.bug;
    const auto t = target(id);
    if (!t || !state.hostile(t->owner, u.owner)) return false;
    const UnitStats& w = u.stats();
    if (t->structure && !(Brood::buildingDamage(u.kind) > 0)) return false;
    // A ground biter can't reach a flyer; a Gloamwing spits at the ground.
    if (!t->structure && t->air && !w.hitsAir) return false;
    u.target = id;
    const bool spitter = u.kind == Unit::Kind::gloamwing;
    const double reach = w.range;
    const double d = gap(u, *t);
    // The dive (docs/creatures.md, "Gloamwing"): at a unit it swoops low
    // for the windup, spits, and stays low a moment before it climbs.
    if (spitter && b.diveFrom) {
        const double since = state.time - *b.diveFrom;
        turn(u, t->position, dt, 8);
        b.state = Bug::State::dive;
        if (since >= Brood::diveWindup && !u.cooldown) {
            if (d <= reach + 1) {
                hit(id, Rules::damage(w, t->armored, t->light, t->armor), false, &u);
                b.strikes = (b.strikes + 1) % 16;
            }
            u.cooldown = w.cooldown;
        }
        if (since >= Brood::diveWindup + Brood::diveAfter) b.diveFrom = std::nullopt;
        return true;
    }
    if (d > reach) {
        b.pace = 1;
        b.state = Bug::State::move;
        if (spitter) {
            move(u, t->position, reach + t->radius + w.radius - 0.2, dt);
        } else {
            // To its edge: a building's or a deposit's middle is solid ground.
            Vec2 off = u.position - t->position;
            if (!(length(off) > 1e-6)) off = Vec2(1, 0);
            travel(u, t->position + normalize(off) * (t->radius + w.radius + reach * 0.5), 0.1, dt);
        }
        return true;
    }
    u.goal = std::nullopt;
    u.waypoints = std::nullopt;
    const bool facing = turn(u, t->position, dt, spitter ? 8 : 10);
    b.state = Bug::State::attack;
    if (u.cooldown || !facing) return true;
    if (spitter && !t->structure) {
        b.diveFrom = state.time;
        b.state = Bug::State::dive;
        return true;
    }
    const double dmg = t->structure ? std::max(0.5, Brood::buildingDamage(u.kind) - 1)
                                    : Rules::damage(w, t->armored, t->light, t->armor);
    hit(id, dmg, false, &u);
    b.strikes = (b.strikes + 1) % 16;
    u.cooldown = w.cooldown;
    return true;
}

// MARK: - The Matriarch (N3)

std::vector<int64_t> Simulation::slamStrikes(const Unit& u) const {
    std::vector<int64_t> out;
    const double reach = Matriarch::slamReach + Rules::radius(u.kind);
    auto inFront = [&](Vec2 at, double radius) {
        const Vec2 d = at - u.position;
        const double far = length(d);
        if (far - radius > reach) return false;
        // Within her edge's reach, and in front of her (one standing under
        // her forelimbs counts whatever its bearing).
        if (far <= radius + 0.5) return true;
        return std::abs(std::remainder(std::atan2(d.y, d.x) - u.heading, 2 * pi)) <= Matriarch::slamArc;
    };
    for (const Unit& o : state.units) {
        if (o.id == u.id || !(o.hp > 0) || o.airborne() || !state.hostile(o.owner, u.owner)) continue;
        if (o.task == Unit::Task::aboard || o.task == Unit::Task::inBastion || o.task == Unit::Task::inDerrick) continue;
        // Sheltered in the Citadel (C1): she strikes the Citadel, not it.
        if (sheltered(o)) continue;
        if (inFront(o.position, Rules::radius(o.kind))) out.push_back(o.id);
    }
    for (const Structure& s : state.structures)
        if (s.hp > 0 && state.hostile(s.owner, u.owner) && inFront(s.position, Rules::radius(s.kind))) out.push_back(s.id);
    return out;
}

void Simulation::stepPuddles(double dt) {
    std::erase_if(puddles, [&](const Puddle& p) { return state.time >= p.until; });
    for (const Puddle& p : puddles)
        for (const Unit& o : state.units) {
            if (!(o.hp > 0) || o.airborne() || o.bug || !state.hostile(o.owner, p.owner)) continue;
            if (distance(o.position, p.at) <= Matriarch::puddleRadius + Rules::radius(o.kind)) hit(o.id, Matriarch::puddleDps * dt);
        }
}

void Simulation::stepMatriarch(Unit& u, double dt) {
    Bug& b = *u.bug;
    const double now = state.time;
    if (u.cooldown) {
        *u.cooldown -= dt;
        if (*u.cooldown <= 0) u.cooldown = std::nullopt;
    }
    u.target = std::nullopt;
    b.diveFrom = std::nullopt;
    // She mends once nothing has hit her for a while.
    const double full = Rules::hp(u.kind);
    if (u.hp < full && (!u.hitAt || now - *u.hitAt >= Matriarch::mendAfter)) u.hp = std::min(full, u.hp + Matriarch::mendRate * dt);
    // The slam crashes down: every ground foe in front of her now.
    if (b.slamAt && now >= *b.slamAt - 1e-9) {
        b.slamAt = std::nullopt;
        for (const int64_t id : slamStrikes(u))
            if (const auto t = target(id))
                hit(id, std::max(0.5, (t->structure ? Matriarch::slamBuilding : Matriarch::slamUnit) - t->armor), false, &u);
    }
    const std::optional<int64_t> foe = b.prey ? b.prey : b.raid;
    std::optional<Target> t;
    if (foe) {
        t = target(*foe);
        if (t && !state.hostile(t->owner, u.owner)) t = std::nullopt;
    }
    if (t) u.target = *foe;
    // Her forelimbs are up, or she rears in a roar: she only turns.
    if (b.slamAt || now < b.roarUntil) {
        b.state = b.slamAt ? Bug::State::attack : Bug::State::roar;
        if (t) turn(u, t->position, dt, 1.5);
        u.goal = std::nullopt;
        u.waypoints = std::nullopt;
        return;
    }
    if (t) {
        const bool facing = turn(u, t->position, dt, 1.5);
        const double d = gap(u, *t);
        // The slam, when a ground foe stands in front of her.
        if (!u.cooldown && facing && !slamStrikes(u).empty()) {
            b.slamAt = now + Matriarch::slamWindup;
            b.strikes = (b.strikes + 1) % 16;
            b.sac = Matriarch::struckSlam;
            b.state = Bug::State::attack;
            u.cooldown = Matriarch::slamEvery;
            u.goal = std::nullopt;
            u.waypoints = std::nullopt;
            return;
        }
        // The spit, at ground or air within its range.
        if (now >= b.spitReady && facing && d <= Matriarch::spitRange) {
            hit(*foe, std::max(0.5, (t->structure ? Matriarch::spitBuilding : Matriarch::spitUnit) - t->armor), false, &u);
            // The puddle where it splashed: under the foe, or at a building's edge.
            Vec2 at = t->position;
            if (t->structure) {
                Vec2 off = u.position - t->position;
                if (!(length(off) > 1e-6)) off = Vec2(1, 0);
                at = t->position + normalize(off) * t->radius;
            }
            puddles.push_back({at, now + Matriarch::puddleFor, u.owner});
            b.strikes = (b.strikes + 1) % 16;
            b.sac = Matriarch::struckSpit;
            b.spitReady = now + Matriarch::spitEvery;
            b.state = Bug::State::attack;
            return;
        }
        // Closer, but never past her tether.
        Vec2 want = t->position;
        if (distance(want, b.site) > b.tether) want = b.site + normalize(want - b.site) * b.tether;
        if (d > Matriarch::slamReach * 0.6 && distance(u.position, want) > 0.4) {
            b.state = Bug::State::move;
            b.pace = 1;
            travel(u, want, 0.3, dt);
        } else {
            b.state = Bug::State::attack;
            u.goal = std::nullopt;
            u.waypoints = std::nullopt;
        }
        return;
    }
    // Nothing to fight: home to her nest's centre (or the siege's place).
    if (distance(u.position, b.site) > 1.0) {
        b.state = Bug::State::move;
        b.pace = 1;
        travel(u, b.site, 0.6, dt);
    } else {
        b.state = Bug::State::idle;
        u.goal = std::nullopt;
        u.waypoints = std::nullopt;
    }
}

bool Simulation::bugOnPath(Unit& u, double dt) {
    Bug& b = *u.bug;
    if (b.leaving || b.leg >= b.path.size()) return false;
    const Vec2 to = b.path[b.leg];
    // Z4: pilots build anywhere in a canyon. A walker routes round their
    // buildings while a way to its next point is open; when they wall it
    // off, it bites through the nearest building of its foes.
    if (nav && !u.stats().air) {
        if (b.breach) {
            const auto t = target(*b.breach);
            if (!t || !t->structure) b.breach = std::nullopt;
        }
        if (state.time >= b.breachCheck) {
            b.breachCheck = state.time + Brood::breachEvery;
            b.breach = std::nullopt;
            const auto k = nav->index(u.position);
            if (k && !nav->reachable(*k, to, Brood::pathReach, false)) {
                double best = std::numeric_limits<double>::infinity();
                for (const Structure& s : state.structures)
                    if (s.hp > 0 && state.hostile(s.owner, u.owner) && distance(s.position, u.position) < best) {
                        best = distance(s.position, u.position);
                        b.breach = s.id;
                    }
            }
        }
        if (b.breach && bugAttack(u, *b.breach, dt)) return true;
    }
    b.state = Bug::State::move;
    b.pace = paceFor(u, Brood::marchSpeed(u.kind));
    if (travel(u, to, 0.5, dt) || distance(u.position, to) <= Brood::pathReach) {
        b.leg++;
        b.coming = true;
    }
    bugClimb(u, dt);
    return true;
}

void Simulation::stepBug(Unit& u, double dt, std::vector<GameEvent>&) {
    if (!u.bug) u.bug = Bug{};
    if (u.kind == Unit::Kind::matriarch) {
        stepMatriarch(u, dt);
        return;
    }
    Bug& b = *u.bug;
    const double now = state.time;
    if (u.cooldown) {
        *u.cooldown -= dt;
        if (*u.cooldown <= 0) u.cooldown = std::nullopt;
    }
    u.target = std::nullopt;
    const bool lodeback = u.kind == Unit::Kind::lodeback;
    const double inSite = distance(u.position, b.site);
    const bool hitLately = u.hitAt && now - *u.hitAt < Brood::stillHit;

    // Mends to full back in its site, unhit for a while.
    if (inSite <= b.siteRadius && !b.prey && !hitLately && !b.coming && !b.leaving) {
        if (b.settledAt < 0) b.settledAt = now;
        if (now - b.settledAt >= Brood::mendAfter && (!u.hitAt || now - *u.hitAt >= Brood::mendAfter)) u.hp = Rules::hp(u.kind);
    } else {
        b.settledAt = -1;
    }

    // Scared (a starfall's impact): away from it, flat out.
    if (now < b.scaredUntil) {
        b.diveFrom = std::nullopt;
        b.state = Bug::State::flee;
        b.pace = 1;
        Vec2 away = u.position - b.scaredFrom;
        if (!(length(away) > 1e-6)) away = Vec2(1, 0);
        travel(u, u.position + normalize(away) * 6, 0.5, dt);
        bugClimb(u, dt);
        return;
    }

    if (lodeback) {
        // Hit: it stops eating and flees at once with what it has.
        if (b.hitBy && b.fleeFrom < 0 && hitLately) {
            b.fleeFrom = now;
            b.leaving = true;
            b.deposit = std::nullopt;
        }
        if (b.fleeFrom >= 0) {
            // Cornered: it has fled a while and is still being hit, or it
            // can't get away. It turns and bites until it can run again.
            const bool stuck = !u.moving.value_or(true) && u.goal;
            const bool cornered = hitLately && (now - b.fleeFrom >= Brood::corneredAfter || stuck);
            if (cornered && b.hitBy && bugAttack(u, *b.hitBy, dt)) return;
            b.state = Bug::State::flee;
            b.pace = paceFor(u, Brood::fleeSpeed);
            travel(u, b.exit, 0.5, dt);
            return;
        }
        if (b.leaving) {
            b.state = Bug::State::move;
            b.pace = paceFor(u, Brood::wanderSpeed(u.kind));
            travel(u, b.exit, 0.5, dt);
            return;
        }
        if (bugOnPath(u, dt)) return;
        if (b.deposit && *b.deposit >= 0 && *b.deposit < static_cast<int64_t>(state.patches.size())) {
            OreDeposit& p = state.patches[size_t(*b.deposit)];
            const double reach = Brood::eatReach + Rules::radius(u.kind);
            if (distance(u.position, p.position) > reach + 0.3) {
                b.state = Bug::State::move;
                b.pace = paceFor(u, Brood::marchSpeed(u.kind));
                Vec2 off = u.position - p.position;
                if (!(length(off) > 1e-6)) off = Vec2(1, 0);
                if (travel(u, p.position + normalize(off) * reach, 0.2, dt) && distance(u.position, p.position) > reach + 1.5) {
                    // Can't get there: it gives up and leaves.
                    b.leaving = true;
                }
                return;
            }
            turn(u, p.position, dt, 3);
            if (b.ateFrom < 0) b.ateFrom = now;
            b.ate = b.deposit;
            b.state = Bug::State::eat;
            const double before = std::floor(b.sac);
            double next = std::min(b.sac + Brood::eatRate * dt, Brood::sacFull);
            if (next > Brood::sacFull - 1e-6) next = Brood::sacFull;
            const double after = std::floor(next);
            const int64_t take = std::min<int64_t>(static_cast<int64_t>(after - before), std::max<int64_t>(0, p.remaining));
            if (after > before && take <= 0) {
                b.leaving = true;  // the deposit is empty
            } else {
                b.sac = next;
                p.remaining -= take;
                if (p.remaining <= 0 && !p.depletedAt) p.depletedAt = now;
            }
            if (b.sac >= Brood::sacFull - 1e-9 || now - b.ateFrom >= Brood::eatFor) b.leaving = true;
            if (b.leaving) b.deposit = std::nullopt;
            return;
        }
        b.leaving = true;
        return;
    }

    // The hunt: its prey while it stays within the leash.
    if (b.prey) {
        const auto t = target(*b.prey);
        const double bound = b.siteRadius + b.leash;
        if (t && distance(u.position, b.site) <= bound && distance(t->position, b.site) <= bound + 1 && bugAttack(u, *b.prey, dt)) {
            bugClimb(u, dt);
            return;
        }
        b.prey = std::nullopt;
        b.diveFrom = std::nullopt;
    }
    // The raid: at its building until it falls.
    if (b.raid) {
        if (bugAttack(u, *b.raid, dt)) {
            bugClimb(u, dt);
            return;
        }
        b.raid = std::nullopt;
    }
    b.diveFrom = std::nullopt;
    if (bugOnPath(u, dt)) return;
    if (b.leaving) {
        b.state = Bug::State::move;
        b.pace = paceFor(u, Brood::marchSpeed(u.kind));
        travel(u, b.exit, 0.5, dt);
        bugClimb(u, dt);
        return;
    }
    if (b.coming || inSite > b.siteRadius) {
        b.state = Bug::State::move;
        b.pace = paceFor(u, b.coming ? Brood::marchSpeed(u.kind) : Brood::wanderSpeed(u.kind) * 1.5);
        const bool there = travel(u, b.site, b.siteRadius * 0.5, dt);
        if (there || inSite <= b.siteRadius * 0.6) {
            b.coming = false;
            b.wander = std::nullopt;
            b.wanderAt = now + Brood::wanderMin * roll(u.id, 7);
        }
        bugClimb(u, dt);
        return;
    }
    // Lingering: a slow wander from spot to spot, a pause at each.
    if (now >= b.wanderAt) {
        b.wander = bugWanderSpot(u);
        b.wanderAt = now + Brood::wanderMin + (Brood::wanderMax - Brood::wanderMin) * roll(u.id, 9);
    }
    if (b.wander && distance(u.position, *b.wander) > 0.3) {
        b.state = Bug::State::move;
        b.pace = paceFor(u, Brood::wanderSpeed(u.kind));
        if (travel(u, *b.wander, 0.25, dt)) b.wander = std::nullopt;
    } else {
        b.wander = std::nullopt;
        b.state = Bug::State::idle;
    }
    bugClimb(u, dt);
}

} // namespace ac
