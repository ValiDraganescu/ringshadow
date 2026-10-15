// The canyon game (Z2, docs/canyon.md): each canyon zone of a region file
// (Z1, docs/regions.md "zones") plays waves of the Brood down its paths to its
// prize. Its states:
//
//   WAITING  wave 1 comes again every few world minutes while a pilot is in
//            the region or next to it (a few Shardlings down one arm, the arm
//            turning each time) until the pilots beat it: that starts the game.
//   RUNNING  wave n of 10, growing in count and mix, a break between two;
//            each wave beaten pays its fighters by the damage they did.
//   FAILED   a wave broke through (it took the fail mark of the prize's ore,
//            or its time ran out with bugs on the prize): the Matriarch comes
//            out of a tunnel with an escort and fells every turret of the zone,
//            then the zone starts over at WAITING, the prize full again.
//   HELD     the last wave beaten: no waves for a world day, the prize the
//            pilots' to mine; then WAITING again, the prize full.
//
// The waves wait while no pilot is near (the server runs on with no one
// there, S2: it must not lose turrets to waves nobody watches); FAILED's and
// HELD's ends run on the world's clock. Bugs of a wave are a group like any
// other (`sendAlongPath`): the core steps them, the director turns them on a
// pilot in their site or a defence that shoots them; only what makes a wave
// (its bugs, the ore they take, who fought them) is kept here. The sky
// carries each zone's game to every pilot (`Sky::zones`).

#include "WorldMap.h"

#include "Brood.h"
#include "TerrainField.h"
#include "Progression.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ac::server {

namespace {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;
using State = WorldMap::CanyonState;
using G = WorldCanyon::Group;

constexpr double kFar = 1e18;

/// Z4, for the dev line and the shots (pilots build anywhere): places a
/// `kind` of the realm's guns may go in zone `z`, beside its paths and clear
/// of the lanes: along each path every 3 cells from the prize out, 6 to 7.5
/// cells to either side of it (the rims), else 3 to 4.5 (the floor's edge),
/// level and free under the ordinary
/// build rules, in the zone's outline, off the pads' plates; the paths taken
/// in turn, so guns spread over every arm.
std::vector<Vec2> gunPlaces(const Simulation& sim, const region::Zone& z, const std::vector<region::Place>& pads, StructureKind kind) {
    std::vector<std::vector<Vec2>> per(z.paths.size());
    auto lane = [&](Vec2 q) {
        double d = kFar;
        for (const region::Zone::Path& p : z.paths)
            for (size_t k = 0; k + 1 < p.points.size(); k++) {
                const Vec2 a = p.points[k], e = p.points[k + 1] - a;
                const double len2 = dot(e, e);
                const double t = len2 > 0 ? std::clamp(dot(q - a, e) / len2, 0.0, 1.0) : 0.0;
                d = std::min(d, distance(q, a + e * t));
            }
        return d;
    };
    for (size_t i = 0; i < z.paths.size(); i++) {
        const std::vector<Vec2>& pts = z.paths[i].points;
        // From the prize end back toward the mouth.
        for (size_t k = pts.size(); k-- > 1;) {
            const Vec2 a = pts[k], e = pts[k - 1] - a;
            const double len = length(e);
            if (len <= 0) continue;
            const Vec2 dir = e / len, side(-dir.y, dir.x);
            for (double t = 0; t < len; t += 3)
                for (const double off : {6.0, -6.0, 7.5, -7.5, 4.5, -4.5, 3.0, -3.0}) {
                    const auto at = sim.snap(kind, a + dir * t + side * off);
                    if (!at || signedDistance(*at, z.outline) > 0 || lane(*at) < 2) continue;
                    if (sim.placementRefusal(kind, *at) || region::touchesPadPlate(pads, *at, Rules::radius(kind))) continue;
                    per[i].push_back(*at);
                }
        }
    }
    std::vector<Vec2> out;
    for (size_t k = 0;; k++) {
        bool any = false;
        for (const std::vector<Vec2>& v : per)
            if (k < v.size()) {
                any = true;
                out.push_back(v[k]);
            }
        if (!any) break;
    }
    return out;
}

/// A turret the Matriarch fells: the pilots' placed guns (as her siege).
bool turret(StructureKind k) { return k == StructureKind::sentinel || k == StructureKind::gatling; }

/// The rare blueprints the last wave pays: the three with the highest floors
/// (the Matriarch's).
std::vector<StructureKind> rareBlueprints() {
    std::vector<StructureKind> k;
    for (const StructureKind b : Progression::blueprintKinds())
        if (b != StructureKind::citadel) k.push_back(b);
    std::stable_sort(k.begin(), k.end(), [](StructureKind a, StructureKind b) {
        return Progression::blueprintFloor(a) > Progression::blueprintFloor(b);
    });
    if (k.size() > 3) k.resize(3);
    return k;
}

/// Ground point `p` inside polygon `poly` (even-odd).
bool inPolygon(Vec2 p, const std::vector<Vec2>& poly) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if ((poly[i].y > p.y) != (poly[j].y > p.y) && p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            in = !in;
    return in;
}

double pathLength(Vec2 mouth, const std::vector<Vec2>& points) {
    if (points.empty()) return 0;
    double length = distance(mouth, points.front());
    for (size_t k = 0; k + 1 < points.size(); k++) length += distance(points[k], points[k + 1]);
    return length;
}

} // namespace

// MARK: - The numbers

const std::vector<WorldCanyon::Wave>& WorldCanyon::waves() {
    // Path 0 and 1: a zone's first two paths (The Fork: the North arm, the
    // East arm). Shardlings first, Gloamwings from wave 4 for the AA towers,
    // Lodebacks (the ore eaters) from the mini-boss wave 5, both arms at once
    // from wave 3.
    static const std::vector<Wave> w = {
        {{G{0, UnitKind::shardling, 4, 0}}},
        {{G{0, UnitKind::shardling, 6, 0}}},
        {{G{0, UnitKind::shardling, 5, 0}, G{1, UnitKind::shardling, 5, 0}}},
        {{G{1, UnitKind::shardling, 7, 0}, G{1, UnitKind::gloamwing, 3, 0}}},
        {{G{0, UnitKind::lodeback, 2, 6}, G{1, UnitKind::gloamwing, 4, 0}}, true},
        {{G{0, UnitKind::shardling, 8, 0}, G{0, UnitKind::gloamwing, 4, 0}, G{1, UnitKind::shardling, 4, 0}}},
        {{G{0, UnitKind::shardling, 7, 0}, G{1, UnitKind::shardling, 7, 0}, G{1, UnitKind::lodeback, 1, 3}}},
        {{G{0, UnitKind::gloamwing, 5, 0}, G{1, UnitKind::gloamwing, 5, 0}, G{1, UnitKind::shardling, 8, 0}}},
        {{G{0, UnitKind::shardling, 10, 0}, G{1, UnitKind::lodeback, 2, 4}, G{1, UnitKind::gloamwing, 5, 0}}},
        {{G{0, UnitKind::lodeback, 3, 10}, G{1, UnitKind::lodeback, 3, 10}, G{0, UnitKind::gloamwing, 4, 0}, G{1, UnitKind::gloamwing, 4, 0}},
         true},
    };
    return w;
}

const std::vector<WorldCanyon::Wave>& WorldCanyon::waves(std::string_view zone) {
    // Z3. The Drowned Cut (Brine Pans, level 2): path 0 the Salt arm (long,
    // through the brine), path 1 the Gull arm. Waders slow in the brine, so
    // more walkers and earlier Lodebacks; its flyers skim the water.
    static const std::vector<Wave> drowned = {
        {{G{0, UnitKind::shardling, 4, 0}}},
        {{G{0, UnitKind::shardling, 8, 0}}},
        {{G{0, UnitKind::shardling, 5, 0}, G{1, UnitKind::shardling, 5, 0}}},
        {{G{1, UnitKind::shardling, 6, 0}, G{0, UnitKind::gloamwing, 3, 0}}},
        {{G{0, UnitKind::lodeback, 2, 6}, G{1, UnitKind::gloamwing, 3, 0}}, true},
        {{G{0, UnitKind::shardling, 10, 0}, G{1, UnitKind::shardling, 6, 0}}},
        {{G{1, UnitKind::lodeback, 1, 4}, G{0, UnitKind::shardling, 8, 0}, G{0, UnitKind::gloamwing, 4, 0}}},
        {{G{0, UnitKind::gloamwing, 6, 0}, G{1, UnitKind::gloamwing, 6, 0}, G{1, UnitKind::shardling, 8, 0}}},
        {{G{0, UnitKind::shardling, 12, 0}, G{1, UnitKind::lodeback, 2, 5}, G{1, UnitKind::gloamwing, 4, 0}}},
        {{G{0, UnitKind::lodeback, 3, 12}, G{1, UnitKind::lodeback, 3, 10}, G{0, UnitKind::gloamwing, 6, 0}}, true},
    };
    // The Roost (Dust Terraces, level 3): path 0 the Ochre arm, path 1 the
    // Dust arm; every Gloamwing comes off the roost's ledge. More of all.
    static const std::vector<Wave> roost = {
        {{G{0, UnitKind::shardling, 5, 0}}},
        {{G{1, UnitKind::shardling, 6, 0}, G{0, UnitKind::gloamwing, 3, 0}}},
        {{G{0, UnitKind::shardling, 6, 0}, G{1, UnitKind::shardling, 6, 0}, G{0, UnitKind::gloamwing, 4, 0}}},
        {{G{1, UnitKind::shardling, 8, 0}, G{0, UnitKind::gloamwing, 6, 0}}},
        {{G{0, UnitKind::lodeback, 2, 8}, G{0, UnitKind::gloamwing, 6, 0}}, true},
        {{G{0, UnitKind::shardling, 10, 0}, G{1, UnitKind::shardling, 6, 0}, G{0, UnitKind::gloamwing, 6, 0}}},
        {{G{0, UnitKind::shardling, 8, 0}, G{1, UnitKind::lodeback, 1, 5}, G{0, UnitKind::gloamwing, 8, 0}}},
        {{G{0, UnitKind::gloamwing, 10, 0}, G{1, UnitKind::shardling, 10, 0}}},
        {{G{0, UnitKind::shardling, 12, 0}, G{1, UnitKind::lodeback, 2, 6}, G{0, UnitKind::gloamwing, 8, 0}}},
        {{G{0, UnitKind::lodeback, 3, 12}, G{1, UnitKind::lodeback, 3, 12}, G{0, UnitKind::gloamwing, 12, 0}}, true},
    };
    if (zone == "the-drowned-cut") return drowned;
    if (zone == "the-roost") return roost;
    return waves();
}

double WorldCanyon::payScale(int64_t broodLevel) { return broodLevel >= 3 ? 1.3 : 1.0; }

// B2 (docs/balance.md): 0.4 of Z2's 200, 150 and 40 a wave, so a held
// canyon and the board bring an active pilot to about twice the mining curve.
double WorldCanyon::xp(int64_t wave) { return 80.0 * double(wave); }
int64_t WorldCanyon::ore(int64_t wave) { return 60 * wave; }
int64_t WorldCanyon::hydrogen(int64_t wave) { return 16 * wave; }

int64_t WorldMap::Canyon::taken() const {
    int64_t t = int64_t(std::floor(gnawed)) + carried;
    for (const auto& [bug, sac] : sacs) t += int64_t(std::floor(sac));
    return t;
}

// MARK: - Reading

const WorldMap::Canyon* WorldMap::canyon(size_t r, std::string_view zone) const {
    for (const Canyon& c : canyons_)
        if (c.region == r && c.id == zone) return &c;
    return nullptr;
}

WorldMap::Canyon* WorldMap::canyonOf(size_t r, std::string_view zone) {
    for (Canyon& c : canyons_)
        if (c.region == r && c.id == zone) return &c;
    return nullptr;
}

bool WorldMap::pilotNear(size_t r) const {
    for (const auto& [pilot, p] : pilots_) {
        if (p.flight) continue;
        if (p.region == r) return true;
        for (const region::Edge e : {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west})
            if (neighbour(r, e) == p.region) return true;
    }
    return false;
}

std::vector<net::Sky::Zone> WorldMap::skyZones() const {
    std::vector<net::Sky::Zone> out;
    for (const Canyon& c : canyons_) {
        net::Sky::Zone z;
        z.region = int64_t(c.region);
        z.zone = int64_t(c.zone);
        z.state = c.state;
        z.wave = c.wave;
        z.waves = WorldCanyon::count();
        z.left = c.out ? c.left : 0;
        const bool counting = c.state == State::failed || c.state == State::held || (!c.out && !c.paused);
        if (counting) z.next = c.next;
        if (c.out) z.ends = c.ends;
        z.taken = c.out ? c.taken() : 0;
        z.mark = WorldCanyon::failOre;
        z.tunnels = c.out ? c.tunnels : 0;
        z.paused = c.paused;
        z.boss = c.wave >= 1 && c.wave <= WorldCanyon::count() && WorldCanyon::waves(c.id)[size_t(c.wave - 1)].boss;
        out.push_back(z);
    }
    return out;
}

// MARK: - Each step

void WorldMap::canyonHit(size_t r, const Simulation::BugHit& h) {
    for (Canyon& c : canyons_) {
        if (c.region != r || !c.bugs.count(h.bug) || !(h.hp > 0)) continue;
        // A pilot's Prospector, or a defence its builder placed (while on line).
        std::optional<PilotId> who;
        for (const auto& [pilot, p] : pilots_)
            if (!p.flight && p.region == r && p.unit == h.shooter) who = pilot;
        if (!who && r < builtBy_.size())
            if (const auto by = builtBy_[r].find(h.shooter); by != builtBy_[r].end())
                for (const auto& [pilot, p] : pilots_)
                    if (!p.key.empty() && p.key == by->second) who = pilot;
        if (who) c.damage[*who] += h.hp;
    }
}

void WorldMap::canyonsStep() {
    if (!world_ || canyons_.empty()) return;
    if (clock_ - canyonThought_ < WorldCanyon::think - 1e-9) return;
    const double dt = canyonThought_ < -1e8 ? 0 : clock_ - canyonThought_;
    canyonThought_ = clock_;
    if (brood_.size() < regions_.size()) brood_.resize(regions_.size());
    for (Canyon& c : canyons_) canyonThink(c, dt);
    sky_.zones = skyZones();
}

void WorldMap::canyonThink(Canyon& c, double dt) {
    Simulation& sim = regions_[c.region]->simulation();
    GameState& st = sim.state;
    // Z3: a flooded floor slows the Brood's walkers wading through it (its
    // flyers pass over), renewed every think.
    if (const region::Zone& z = regions_[c.region]->places()->zones[c.zone]; !z.flood.empty() && sim.broodSide)
        for (Unit& u : st.units)
            if (u.owner == *sim.broodSide && u.hp > 0 && u.kind != UnitKind::gloamwing && inPolygon(u.position, z.flood)) {
                u.slowUntil = st.time + WorldCanyon::floodHold;
                u.slowedTo = std::min(u.slowedTo.value_or(1.0), z.floodSlow);
            }
    const bool near = pilotNear(c.region);
    c.paused = !near;
    switch (c.state) {
    case State::held:
        // A world day of peace, on the world's clock; then the prize is full
        // again and wave 1 waits.
        if (clock_ >= c.next) resetCanyon(c);
        return;
    case State::failed:
        // The siege over: she went home with every turret down, or she was
        // felled out there (the turrets left stand). Without her (she could
        // not come, the region slept) the zone's turrets fall at FAILED's end.
        if (c.siege && !(siege_ && siege_->region == c.region)) {
            if (!nest_.downAt) fellZoneTurrets(c);
            resetCanyon(c);
        } else if (clock_ >= c.next) {
            fellZoneTurrets(c);
            resetCanyon(c);
        }
        return;
    case State::waiting:
    case State::running: break;
    }
    if (!c.out) {
        // The next wave's countdown runs only with a pilot near.
        if (!near) {
            c.next += dt;
            return;
        }
        if (clock_ < c.next) return;
        if (c.state == State::waiting) c.wave = 1;
        if (!sendWave(c)) c.next = clock_ + WorldCanyon::replayEvery;
        return;
    }
    // A wave out. No pilot near: it waits; once the region slept (its bugs
    // taken away) it comes again whole after a break when a pilot is back.
    if (!near) {
        c.ends += dt;
        const bool any = std::any_of(c.bugs.begin(), c.bugs.end(), [&](int64_t id) { return st.unit(id).has_value(); });
        if (!any) {
            c.out = false;
            c.bugs.clear();
            c.killed.clear();
            c.sacs.clear();
            c.left = 0;
            c.next = clock_ + (c.state == State::waiting ? WorldCanyon::firstWait : WorldCanyon::breakFor);
        }
        return;
    }
    const region::Built& places = *regions_[c.region]->places();
    const region::Zone& z = places.zones[c.zone];
    std::vector<size_t> prize;
    for (const int64_t i : z.prizeDeposits) {
        const Vec2 at = places.map.deposits[size_t(i)].position;
        for (size_t k = 0; k < st.patches.size(); k++)
            if (distance(st.patches[k].position, at) < 0.01) prize.push_back(k);
    }
    int64_t alive = 0, onPrize = 0;
    std::vector<int64_t> gone;
    for (const int64_t id : c.bugs) {
        const auto u = st.unit(id);
        if (!u) {
            // Gone without dying: a Lodeback away out of sight with its sac.
            if (!c.killed.count(id))
                if (const auto s = c.sacs.find(id); s != c.sacs.end()) c.carried += int64_t(std::floor(s->second));
            c.sacs.erase(id);
            gone.push_back(id);
            continue;
        }
        if (!(u->hp > 0) || (u->bug && u->bug->state == Bug::State::dead)) {
            // Killed: a Lodeback's sac goes back into the deposit (the director).
            c.killed.insert(id);
            c.sacs.erase(id);
            continue;
        }
        alive += 1;
        if (u->kind == UnitKind::lodeback && u->bug) c.sacs[id] = u->bug->sac;
        if (distance(u->position, c.middle) > WorldCanyon::onPrize) continue;
        onPrize += 1;
        // A Shardling on the prize with nothing to fight gnaws its ore.
        if (u->kind != UnitKind::shardling || !u->bug || u->bug->prey || u->bug->raid) continue;
        std::optional<size_t> dep;
        for (const size_t k : prize)
            if (st.patches[k].remaining > 0 && (!dep || distance(st.patches[k].position, u->position) < distance(st.patches[*dep].position, u->position)))
                dep = k;
        if (!dep) continue;
        const int64_t before = int64_t(std::floor(c.gnawed));
        c.gnawed += WorldCanyon::gnaw * dt;
        OreDeposit& d = st.patches[*dep];
        d.remaining = std::max<int64_t>(0, d.remaining - (int64_t(std::floor(c.gnawed)) - before));
        if (d.remaining <= 0 && !d.depletedAt) d.depletedAt = st.time;
    }
    for (const int64_t id : gone) c.bugs.erase(id);
    c.left = alive;
    if (c.taken() >= WorldCanyon::failOre || (clock_ >= c.ends && onPrize > 0)) waveFailed(c);
    else if (alive == 0) waveBeaten(c);
}

// MARK: - Waves

bool WorldMap::sendWave(Canyon& c) {
    if (!regions_[c.region]->simulation().broodSide) return false;
    const region::Built& places = *regions_[c.region]->places();
    const region::Zone& z = places.zones[c.zone];
    if (z.paths.empty() || c.wave < 1 || c.wave > WorldCanyon::count()) return false;
    const WorldCanyon::Wave& wave = WorldCanyon::waves(c.id)[size_t(c.wave - 1)];
    c.bugs.clear();
    c.killed.clear();
    c.sacs.clear();
    c.gnawed = 0;
    c.carried = 0;
    c.tunnels = 0;
    c.damage.clear();
    c.frontierDamage.clear();
    // Wave 1 waiting turns from arm to arm with each replay.
    const size_t turn = c.state == State::waiting ? size_t(c.replays) : 0;
    double longest = 0;
    // Z3: a roost's paths are the flyers' (every Gloamwing comes off it);
    // the walkers take the tunnels' paths.
    std::vector<size_t> walk, fly;
    for (size_t k = 0; k < z.paths.size(); k++) {
        const auto e = std::find_if(z.entrances.begin(), z.entrances.end(), [&](const region::Zone::Entrance& x) { return x.name == z.paths[k].from; });
        (e != z.entrances.end() && e->roost ? fly : walk).push_back(k);
    }
    if (walk.empty()) walk = fly;
    for (const WorldCanyon::Group& g : wave.groups) {
        const bool flyer = g.kind == UnitKind::gloamwing;
        const std::vector<size_t>& ways = flyer && !fly.empty() ? fly : walk;
        const size_t path = ways[(g.path + turn) % ways.size()];
        auto send = [&](UnitKind kind, int64_t count, size_t along) {
            const region::Zone::Path& way = z.paths[along];
            const auto mouth = std::find_if(z.entrances.begin(), z.entrances.end(), [&](const region::Zone::Entrance& e) { return e.name == way.from; });
            if (mouth == z.entrances.end() || count <= 0) return;
            const size_t path = along;
            const auto id = sendAlongPath(c.region, c.id, path, kind, count);
            if (!id) return;
            for (BroodGroup& grp : brood_[c.region].groups)
                if (grp.id == *id) {
                    // A wave never leaves on its own: it is killed, eats its
                    // fill, or breaks through.
                    grp.lingerUntil = kFar;
                    for (const int64_t b : grp.bugs) c.bugs.insert(b);
                }
            longest = std::max(longest, pathLength(mouth->at, way.points) / Brood::marchSpeed(kind));
            c.tunnels |= 1u << uint32_t(mouth - z.entrances.begin());
        };
        send(g.kind, g.count, path);
        // An escort walks: a flyer's from the walkers' first path.
        send(UnitKind::shardling, g.escort, flyer ? walk[(g.path + turn) % walk.size()] : path);
    }
    if (c.bugs.empty()) return false;
    if (c.state == State::waiting) c.replays += 1;
    c.out = true;
    c.left = int64_t(c.bugs.size());
    c.ends = clock_ + longest + WorldCanyon::hold;
    return true;
}

void WorldMap::waveBeaten(Canyon& c) {
    // Its fighters' pay, by the damage each did (at least a floor share).
    const bool last = c.wave >= WorldCanyon::count();
    awayNoteAll(net::AwayRow{0, net::AwayKind::waves, "", c.name, "", 1});
    double total = 0;
    for (const auto& [pilot, d] : c.damage) total += d;
    // P1: frontier pilots fought it too, paid by the same split.
    for (const auto& [id, d] : c.frontierDamage) total += d;
    const std::vector<StructureKind> rare = rareBlueprints();
    // Z3: a level-3 region's canyon pays more.
    const double bonus = (last ? WorldCanyon::lastBonus : 1) * WorldCanyon::payScale(world_->regions[c.region].file.brood);
    int64_t fighters = 0;
    for (const auto& [pilot, d] : c.damage) {
        PilotState* p = pilotState(pilot);
        if (!p || !(d > 0)) continue;
        fighters += 1;
        // S4: a wave held, to each who fought it.
        standNote(p->key, &Standing::waves, 1);
        const double share = std::max(d / std::max(1.0, total), WorldCanyon::floorShare);
        const double xp = std::round(WorldCanyon::xp(c.wave) * bonus * share);
        const int64_t ore = std::llround(double(WorldCanyon::ore(c.wave)) * bonus * share);
        const int64_t mh = std::llround(double(WorldCanyon::hydrogen(c.wave)) * bonus * share);
        if (!p->flight) bankOut(*p, p->region);
        p->wallet.ore += ore;
        p->wallet.hydrogen += mh;
        if (!p->flight) bankIn(*p, p->region);
        earn(*p, xp);
        std::string id = "wave/" + std::to_string(c.wave) + "/" + std::to_string(mh);
        if (last && !rare.empty()) {
            const StructureKind bp = rare[size_t(broodDice_() % rare.size())];
            p->progress.giveBlueprint(bp, 1);
            id += "/" + std::string(rawValue(bp));
        }
        p->news.push_back(News{NewsKind::canyon, id, xp, ore, c.name});
        pushProgress(pilot);
    }
    for (const auto& [id, d] : c.frontierDamage) {
        FrontierPilot* f = frontierPilot(id);
        if (!f || !(d > 0)) continue;
        fighters += 1;
        standNote(f->key, &Standing::waves, 1);
        const double share = std::max(d / std::max(1.0, total), WorldCanyon::floorShare);
        f->wallet.ore += std::llround(double(WorldCanyon::ore(c.wave)) * bonus * share);
        f->wallet.hydrogen += std::llround(double(WorldCanyon::hydrogen(c.wave)) * bonus * share);
        frontierEarn(*f, std::round(WorldCanyon::xp(c.wave) * bonus * share));
        if (last && !rare.empty()) f->progress.giveBlueprint(rare[size_t(broodDice_() % rare.size())], 1);
    }
    c.out = false;
    c.bugs.clear();
    c.killed.clear();
    c.sacs.clear();
    c.left = 0;
    c.damage.clear();
    c.frontierDamage.clear();
    if (last) {
        // The canyon held: a world day of peace; the whole world hears it.
        c.state = State::held;
        c.next = clock_ + WorldCanyon::heldDays * params_.dayLength;
        for (auto& [pilot, p] : pilots_) p.news.push_back(News{NewsKind::canyon, "held", 0, fighters, c.name});
        awayNoteAll(net::AwayRow{0, net::AwayKind::held, "", c.name, "", fighters});
        return;
    }
    // Wave 1 beaten while waiting starts the game.
    c.state = State::running;
    c.wave += 1;
    c.next = clock_ + WorldCanyon::breakFor;
}

void WorldMap::releaseWave(Canyon& c) {
    if (c.region < brood_.size())
        for (BroodGroup& g : brood_[c.region].groups)
            if (std::any_of(g.bugs.begin(), g.bugs.end(), [&](int64_t b) { return c.bugs.count(b) > 0; }))
                g.lingerUntil = std::min(g.lingerUntil, clock_ + 30);
    c.out = false;
    c.bugs.clear();
    c.killed.clear();
    c.sacs.clear();
    c.left = 0;
}

void WorldMap::waveFailed(Canyon& c) {
    // What the wave ate is gone; its bugs linger a while and leave.
    releaseWave(c);
    c.damage.clear();
    c.frontierDamage.clear();
    if (c.state == State::waiting) {
        // Wave 1 comes again; nothing to lose yet.
        c.next = clock_ + WorldCanyon::replayEvery;
        return;
    }
    const int64_t wave = c.wave;
    // She is down (N3): nobody comes for the turrets; the zone starts over.
    if (nest_.downAt || !nestRegion()) {
        for (auto& [pilot, p] : pilots_) p.news.push_back(News{NewsKind::canyon, "spared", 0, wave, c.name});
        resetCanyon(c);
        return;
    }
    for (auto& [pilot, p] : pilots_) p.news.push_back(News{NewsKind::canyon, "failed", 0, wave, c.name});
    awayNoteAll(net::AwayRow{0, net::AwayKind::fallen, "", c.name, "", wave});
    c.state = State::failed;
    c.next = clock_ + WorldCanyon::failedMax;
    // She comes out of the tunnel the wave came from (the first of them).
    const region::Zone& z = regions_[c.region]->places()->zones[c.zone];
    std::optional<Vec2> from;
    for (size_t k = 0; k < z.entrances.size(); k++)
        if (c.tunnels & (1u << uint32_t(k))) {
            from = z.entrances[k].at;
            break;
        }
    if (!from && !z.entrances.empty()) from = z.entrances.front().at;
    c.siege = matriarchSiege(c.region, c.middle, c.reach, WorldCanyon::siegeEscort, from);
}

void WorldMap::resetCanyon(Canyon& c) {
    if (c.out) releaseWave(c);
    refillPrize(c);
    c.state = State::waiting;
    c.wave = 1;
    c.siege = false;
    c.damage.clear();
    c.frontierDamage.clear();
    c.next = clock_ + WorldCanyon::replayEvery;
}

void WorldMap::refillPrize(const Canyon& c) {
    const region::Built& places = *regions_[c.region]->places();
    const region::Zone& z = places.zones[c.zone];
    GameState& st = regions_[c.region]->simulation().state;
    for (const int64_t i : z.prizeDeposits) {
        const Vec2 at = places.map.deposits[size_t(i)].position;
        for (OreDeposit& d : st.patches)
            if (distance(d.position, at) < 0.01) {
                d.remaining = std::max(d.remaining, d.initial);
                d.depletedAt = std::nullopt;
            }
    }
    if (st.wells)
        for (const int64_t i : z.prizeWells) {
            const WellSpec& spec = places.map.wells[size_t(i)];
            for (Well& w : *st.wells)
                if (distance(w.position, spec.position) < 0.01) w.remaining = std::max(w.remaining, spec.amount);
        }
}

void WorldMap::fellZoneTurrets(const Canyon& c) {
    for (Structure& b : regions_[c.region]->simulation().state.structures)
        if (realm(b) && turret(b.kind) && b.hp > 0 && distance(b.position, c.middle) <= c.reach + Rules::radius(b.kind)) b.hp = 0;
}

// MARK: - Tests, dev and shots

namespace {

/// Tests, dev and shots: the wave out of `c` goes, its bugs taken away at once.
void takeWaveAway(WorldMap::Canyon& c, GameState& st, std::vector<WorldMap::BroodGroup>& groups) {
    const std::set<int64_t> bugs = c.bugs;
    std::erase_if(st.units, [&](const Unit& u) { return bugs.count(u.id) > 0; });
    for (WorldMap::BroodGroup& g : groups) std::erase_if(g.bugs, [&](int64_t b) { return bugs.count(b) > 0; });
    std::erase_if(groups, [](const WorldMap::BroodGroup& g) { return g.bugs.empty(); });
}

} // namespace

bool WorldMap::canyonSend(size_t r, std::string_view zone, int64_t wave) {
    Canyon* c = canyonOf(r, zone);
    if (!c || wave < 1 || wave > WorldCanyon::count()) return false;
    if (c->out) {
        takeWaveAway(*c, regions_[r]->simulation().state, brood_[r].groups);
        regions_[r]->simulation().reindex();
        releaseWave(*c);
    }
    if (c->state == State::failed || c->state == State::held) c->state = State::waiting;
    if (wave > 1) c->state = State::running;
    c->wave = wave;
    return sendWave(*c);
}

bool WorldMap::canyonFail(size_t r, std::string_view zone) {
    Canyon* c = canyonOf(r, zone);
    if (!c || !c->out) return false;
    waveFailed(*c);
    return true;
}

bool WorldMap::canyonBeat(size_t r, std::string_view zone) {
    Canyon* c = canyonOf(r, zone);
    if (!c || c->state == State::failed || c->state == State::held) return false;
    // Every bug of the wave out dies where it stands.
    for (Unit& u : regions_[r]->simulation().state.units)
        if (c->bugs.count(u.id) && u.hp > 0) u.hp = 0;
    waveBeaten(*c);
    return true;
}

void WorldMap::canyonReset(size_t r, std::string_view zone) {
    if (Canyon* c = canyonOf(r, zone)) {
        if (c->out && r < brood_.size()) {
            takeWaveAway(*c, regions_[r]->simulation().state, brood_[r].groups);
            regions_[r]->simulation().reindex();
        }
        resetCanyon(*c);
        c->replays = 0;
        c->next = clock_ + WorldCanyon::firstWait;
    }
}

std::string WorldMap::devCanyon(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: canyon: no world, or no pilot";
    const auto& [pilot, p] = *pilots_.begin();
    const size_t r = p.region;
    const auto& places = regions_[r]->places();
    if (!places || places->zones.empty()) return "dev: canyon: no zone in " + world_->regions[r].file.id;
    const region::Zone& z = places->zones.front();
    Canyon* c = canyonOf(r, z.id);
    if (!c) return "dev: canyon: the canyon game is off";
    const std::string zone = z.id;
    if (what == "send") {
        int64_t wave = 1;
        in >> wave;
        return canyonSend(r, zone, wave) ? "dev: canyon: wave " + std::to_string(wave) + " out in " + zone : "dev: canyon send: refused";
    }
    if (what == "fail") return canyonFail(r, zone) ? "dev: canyon: the wave broke through" : "dev: canyon fail: no wave out";
    if (what == "beat") return canyonBeat(r, zone) ? "dev: canyon: the wave beaten" : "dev: canyon beat: refused";
    if (what == "hold") {
        // The last wave beaten (out or not).
        if (c->state == State::failed || c->state == State::held) c->state = State::running;
        c->wave = WorldCanyon::count();
        return canyonBeat(r, zone) ? "dev: canyon: " + zone + " held" : "dev: canyon hold: refused";
    }
    if (what == "reset") {
        canyonReset(r, zone);
        return "dev: canyon: " + zone + " reset";
    }
    if (what == "break") {
        double s = 0;
        in >> s;
        if (c->out) return "dev: canyon break: a wave is out";
        c->next = clock_ + std::max(0.0, s);
        return "dev: canyon: the next wave in " + std::to_string(int(s)) + " s";
    }
    if (what == "turrets") {
        // Finished guns of the realm beside the zone's paths (Z4: no build
        // spots; `gunPlaces`), built by the pilot: the first FIRST places
        // passed over.
        int64_t gatlings = 0, towers = 0, first = 0;
        in >> gatlings >> towers >> first;
        Simulation& sim = regions_[r]->simulation();
        GameState& st = sim.state;
        int64_t placed = 0, skipped = 0;
        for (int64_t k = 0; k < gatlings + towers; k++) {
            const StructureKind kind = k < gatlings ? StructureKind::gatling : StructureKind::sentinel;
            std::optional<Vec2> at;
            for (const Vec2 q : gunPlaces(sim, z, places->pads, kind)) {
                bool clear = true;
                for (const Structure& b : st.structures)
                    if (std::abs(b.position.x - q.x) < Rules::radius(b.kind) + Rules::radius(kind) &&
                        std::abs(b.position.y - q.y) < Rules::radius(b.kind) + Rules::radius(kind))
                        clear = false;
                if (!clear) continue;
                if (skipped < first) {
                    skipped++;
                    continue;
                }
                at = q;
                break;
            }
            if (!at) break;
            st.structures.push_back(Structure(st.nextID++, kind, Pilot::player, *at));
            st.structures.back().hp = Rules::hp(kind);
            builtBy_[r][st.structures.back().id] = p.key;
            placed += 1;
            sim.reindex();
            sim.refreshNav();
        }
        return "dev: canyon: " + std::to_string(placed) + " turrets beside " + zone + "'s paths";
    }
    if (what == "place") {
        // `place gatling|sentinel X Y [X Y …]`: finished guns of the realm at
        // those region cells, built by the pilot (Z4: anywhere standable,
        // under the ordinary build rules; a refused place is passed over).
        std::string kindName;
        in >> kindName;
        const StructureKind kind = kindName == "sentinel" ? StructureKind::sentinel : StructureKind::gatling;
        Simulation& sim = regions_[r]->simulation();
        GameState& st = sim.state;
        int64_t placed = 0, refused = 0;
        double x = 0, y = 0;
        while (in >> x >> y) {
            // Snapped as a pilot's placement is; nudged a cell at most, as a
            // pilot would, when the snapped cell is refused.
            const Vec2 want = places->ground(Vec2(x, y));
            std::optional<Vec2> at;
            double best = kFar;
            for (const double dx : {0.0, 1.0, -1.0})
                for (const double dy : {0.0, 1.0, -1.0}) {
                    const auto q = sim.snap(kind, want + Vec2(dx, dy));
                    if (!q || distance(*q, want) >= best || sim.placementRefusal(kind, *q) ||
                        region::touchesPadPlate(places->pads, *q, Rules::radius(kind)))
                        continue;
                    best = distance(*q, want);
                    at = q;
                }
            if (!at) {
                refused += 1;
                continue;
            }
            st.structures.push_back(Structure(st.nextID++, kind, Pilot::player, *at));
            st.structures.back().hp = Rules::hp(kind);
            builtBy_[r][st.structures.back().id] = p.key;
            placed += 1;
            sim.reindex();
            sim.refreshNav();
        }
        return "dev: canyon: " + std::to_string(placed) + " " + kindName + " placed, " + std::to_string(refused) + " refused";
    }
    return "dev: canyon: '" + what + "' is not send N|fail|beat|hold|reset|break S|turrets G [A [FIRST]]|place KIND X Y…";
}

} // namespace ac::server
