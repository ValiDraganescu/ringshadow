// Port of Sources/GameCore/Vision.swift: `Vision` and the Simulation's fog
// (`Simulation+Vision.members.h`).
#include "Vision.h"

#include "Commander.h"
#include "NavGrid.h"
#include "Rules.h"
#include "Simulation.h"
#include "TerrainField.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <utility>

namespace ac {

static_assert(Vision::cell == NavGrid::cell, "the fog's grid is the walking grid's");

namespace {

/// Terrain levels per map, shared by every simulation on it.
struct VisionCache {
    std::map<std::string, std::vector<int8_t>> layers;
    std::mutex lock;
    std::optional<std::vector<int8_t>> get(const std::string& k) {
        std::lock_guard<std::mutex> g(lock);
        auto it = layers.find(k);
        if (it == layers.end()) return std::nullopt;
        return it->second;
    }
    void set(const std::string& k, const std::vector<int8_t>& v) {
        std::lock_guard<std::mutex> g(lock);
        layers[k] = v;
    }
};

VisionCache& visionCache() {
    static VisionCache cache;
    return cache;
}

/// `Int8(clamping: Int(v))`.
int8_t clampedInt8(double v) {
    const int64_t i = static_cast<int64_t>(v);
    return static_cast<int8_t>(min<int64_t>(max<int64_t>(i, -128), 127));
}

/// A viewer: where, how far, from what level (nil: the air).
struct Viewer { Vec2 c; double r; std::optional<int8_t> level; };

bool hidden(Unit::Task t) {
    return t == Unit::Task::inBastion || t == Unit::Task::aboard || t == Unit::Task::inDerrick;
}

/// One memory for a team (C++, teams): what any of `members` (players, in
/// order) has scouted and remembers. Ground explored by any, each site at
/// the latest any saw it, every building any remembers (the first member's
/// copy of one several remember) and every unit (the latest sighting).
Intel teamIntel(const std::vector<Intel>& intel, const std::vector<size_t>& members) {
    Intel k = intel[members.front()];
    std::set<int64_t> buildings;
    for (const auto& b : k.buildings) buildings.insert(b.id);
    std::map<int64_t, size_t> units;
    for (size_t i = 0; i < k.units.size(); ++i) units[k.units[i].unit.id] = i;
    for (size_t m = 1; m < members.size(); ++m) {
        const Intel& o = intel[members[m]];
        if (k.explored.size() < o.explored.size()) k.explored.resize(o.explored.size(), 0);
        for (size_t w = 0; w < o.explored.size(); ++w) k.explored[w] |= o.explored[w];
        if (k.sites.size() < o.sites.size()) k.sites.resize(o.sites.size(), -std::numeric_limits<double>::infinity());
        for (size_t j = 0; j < o.sites.size(); ++j) k.sites[j] = max(k.sites[j], o.sites[j]);
        for (const auto& b : o.buildings) {
            if (buildings.insert(b.id).second) k.buildings.push_back(b);
        }
        for (const auto& seen : o.units) {
            const auto it = units.find(seen.unit.id);
            if (it == units.end()) {
                units[seen.unit.id] = k.units.size();
                k.units.push_back(seen);
            } else if (seen.at > k.units[it->second].at) {
                k.units[it->second] = seen;
            }
        }
    }
    std::sort(k.buildings.begin(), k.buildings.end(), [](const Structure& a, const Structure& b) { return a.id < b.id; });
    std::sort(k.units.begin(), k.units.end(), [](const Intel::Seen& a, const Intel::Seen& b) { return a.unit.id < b.unit.id; });
    return k;
}

} // namespace

Vision::Vision(const MapDefinition& map, const TerrainField& field, const NavGrid& nav) {
    origin = nav.origin;
    width = nav.width;
    height = nav.height;
    char key[512];
    const double mirror = map.mirrorX ? *map.mirrorX : std::numeric_limits<double>::quiet_NaN();
    std::snprintf(key, sizeof key, "vision|%s|%lld|%llu|%.17g,%.17g,%.17g,%.17g|%.17g", map.name.c_str(),
                  static_cast<long long>(map.version), static_cast<unsigned long long>(map.seed), map.bounds.minX,
                  map.bounds.minZ, map.bounds.maxX, map.bounds.maxZ, mirror);
    if (auto cached = visionCache().get(key)) {
        tier = *cached;
    } else {
        std::vector<int8_t> t(static_cast<size_t>(width * height), 0);
        const Vec2 o = origin;
        const int64_t w = width, h = height;
        const double lh = map.levelHeight;
        // Rows over the cores, as Swift has it and as `NavGrid::buildGround`
        // does: each cell is its own, each thread writes its own rows.
        int8_t* out = t.data();
        auto row = [&](int64_t z) {
            for (int64_t x = 0; x < w; ++x) {
                const Vec2 p = o + Vec2(static_cast<double>(x) + 0.5, static_cast<double>(z) + 0.5) * cell;
                out[static_cast<size_t>(z * w + x)] = clampedInt8(rounded(field.height(p) / lh));
            }
        };
        const int64_t threads = std::max<int64_t>(1, std::min<int64_t>(static_cast<int64_t>(std::thread::hardware_concurrency()), h));
        std::vector<std::thread> pool;
        for (int64_t k = 1; k < threads; k++)
            pool.emplace_back([&row, k, threads, h] {
                for (int64_t z = k; z < h; z += threads) row(z);
            });
        for (int64_t z = 0; z < h; z += threads) row(z);
        for (std::thread& th : pool) th.join();
        tier = t;
        visionCache().set(key, t);
    }
    for (const auto& d : map.doodads) {
        if (d.kind == Doodad::Kind::tower) towers.push_back(Tower{d.position, tierIn(tier, origin, width, height, d.position)});
    }
}

/// A watchtower put down in the playground (`Simulation.placeDoodad`).
void Vision::addTower(Vec2 p) {
    towers.push_back(Tower{p, tierIn(tier, origin, width, height, p)});
}

int8_t Vision::tierIn(const std::vector<int8_t>& t, Vec2 o, int64_t w, int64_t h, Vec2 p) {
    const int64_t x = static_cast<int64_t>(std::floor((p.x - o.x) / cell));
    const int64_t z = static_cast<int64_t>(std::floor((p.y - o.y) / cell));
    if (!(x >= 0 && z >= 0 && x < w && z < h)) return 0;
    return t[static_cast<size_t>(z * w + x)];
}

/// The level a viewer at `p` sees from.
int8_t Vision::tierAt(Vec2 p) const {
    const auto i = index(p);
    return i ? tier[static_cast<size_t>(*i)] : 0;
}

/// Mark what a viewer at `c` sees, `r` round, from level `level` (nil:
/// in the air, every level): `seen` (and `explored`) where it sees the
/// ground, `near` wherever it is in reach (an air unit there is seen).
void Vision::stamp(Vec2 c, double r, std::optional<int8_t> level, std::vector<uint8_t>& seen, std::vector<uint8_t>& near,
                   std::vector<uint64_t>& explored, std::optional<std::pair<double, double>> cone) const {
    const int64_t x0 = max<int64_t>(0, static_cast<int64_t>(std::floor((c.x - r - origin.x) / cell)));
    const int64_t x1 = min<int64_t>(width - 1, static_cast<int64_t>(std::floor((c.x + r - origin.x) / cell)));
    const int64_t z0 = max<int64_t>(0, static_cast<int64_t>(std::floor((c.y - r - origin.y) / cell)));
    const int64_t z1 = min<int64_t>(height - 1, static_cast<int64_t>(std::floor((c.y + r - origin.y) / cell)));
    if (!(x0 <= x1 && z0 <= z1)) return;
    const double r2 = r * r;
    const int8_t top = level ? *level : std::numeric_limits<int8_t>::max();
    const int8_t* __restrict t = tier.data();
    uint8_t* __restrict s = seen.data();
    uint8_t* __restrict n = near.data();
    uint64_t* __restrict ex = explored.data();
    // A wedge: the cells whose centre is within the half angle of the heading.
    const Vec2 ahead = cone ? Vec2(std::cos(cone->first), std::sin(cone->first)) : Vec2::zero;
    const double edge = cone ? std::cos(cone->second) : 0.0;
    for (int64_t z = z0; z <= z1; ++z) {
        const double dz = origin.y + (static_cast<double>(z) + 0.5) * cell - c.y;
        const double dz2 = dz * dz;
        if (!(dz2 <= r2)) continue;
        const int64_t row = z * width;
        if (cone) {
            for (int64_t x = x0; x <= x1; ++x) {
                const double dx = origin.x + (static_cast<double>(x) + 0.5) * cell - c.x;
                if (!(dx * dx + dz2 <= r2)) continue;
                if (dx * ahead.x + dz * ahead.y < edge * std::sqrt(dx * dx + dz2)) continue;
                const int64_t i = row + x;
                n[i] = 1;
                if (t[i] <= top && !s[i]) {
                    s[i] = 1;
                    ex[i >> 6] |= uint64_t(1) << static_cast<uint64_t>(i & 63);
                }
            }
            continue;
        }
        // The row's cells in the circle are one unbroken span: a cell's
        // `dx` grows with `x` (rounding keeps the order), so the test, which
        // grows with `|dx|`, passes from one end of the span to the other.
        // Its ends from the circle's half chord, then set right by the same
        // test, cell by cell; within them every cell passes.
        auto in = [&](int64_t x) {
            const double dx = origin.x + (static_cast<double>(x) + 0.5) * cell - c.x;
            return dx * dx + dz2 <= r2;
        };
        // The nearest cell to the centre's column and its neighbours: the
        // span, if any, holds one of them.
        const int64_t mid = min(x1, max(x0, static_cast<int64_t>(std::floor((c.x - origin.x) / cell))));
        int64_t seed = -1;
        for (const int64_t x : {mid, mid - 1, mid + 1})
            if (seed < 0 && x >= x0 && x <= x1 && in(x)) seed = x;
        if (seed < 0) continue;
        const double chord = std::sqrt(max(r2 - dz2, 0.0));
        int64_t a = min(seed, max(x0, static_cast<int64_t>(std::floor((c.x - chord - origin.x) / cell))));
        int64_t b = max(seed, min(x1, static_cast<int64_t>(std::floor((c.x + chord - origin.x) / cell))));
        if (in(a)) { while (a > x0 && in(a - 1)) --a; } else { while (!in(a)) ++a; }
        if (in(b)) { while (b < x1 && in(b + 1)) ++b; } else { while (!in(b)) --b; }
        // What the per-cell test marked: in reach, and the ground seen where
        // its level is not above the viewer's, its explored bit with it (a
        // word's bits gathered, then written once).
        int64_t i = row + a;
        const int64_t end = row + b + 1;
        std::memset(n + i, 1, static_cast<size_t>(end - i));
        while (i < end) {
            const int64_t from = i, stop = min(end, (i | 63) + 1);
            uint64_t bits = 0;
            if (!level) {
                // In the air (or the daylight): every level, so every cell
                // not seen yet is seen now. Eight at a time where they line
                // up: `seen` holds 0 or 1, so each byte's low bit, flipped,
                // says "not yet", and one multiply gathers the eight bits
                // (byte k's to bit 56 + k).
                for (; i < stop && (i & 7); ++i) bits |= uint64_t(s[i] == 0) << static_cast<uint64_t>(i & 63);
                for (; i + 8 <= stop; i += 8) {
                    uint64_t w;
                    std::memcpy(&w, s + i, sizeof w);
                    bits |= ((~w & 0x0101010101010101ull) * 0x0102040810204080ull >> 56) << static_cast<uint64_t>(i & 63);
                }
                for (; i < stop; ++i) bits |= uint64_t(s[i] == 0) << static_cast<uint64_t>(i & 63);
                std::memset(s + from, 1, static_cast<size_t>(stop - from));
            } else {
                for (; i < stop; ++i) {
                    const bool add = t[i] <= top && !s[i];
                    s[i] |= static_cast<uint8_t>(add);
                    bits |= uint64_t(add) << static_cast<uint64_t>(i & 63);
                }
            }
            ex[from >> 6] |= bits;
        }
    }
}

/// Its test (the same sums as `stamp`'s) passes at the four corner cells;
/// it grows with `|dx|` and `|dz|`, whose largest are at the corners.
bool Vision::coversGrid(Vec2 c, double r) const {
    if (!(width > 0 && height > 0)) return false;
    const double r2 = r * r;
    for (const int64_t z : {int64_t(0), height - 1}) {
        const double dz = origin.y + (static_cast<double>(z) + 0.5) * cell - c.y;
        const double dz2 = dz * dz;
        for (const int64_t x : {int64_t(0), width - 1}) {
            const double dx = origin.x + (static_cast<double>(x) + 0.5) * cell - c.x;
            if (!(dx * dx + dz2 <= r2)) return false;
        }
    }
    return true;
}

/// What player `p` sees in `state` now; what it sees joins `explored`.
/// `farther` holds the sight radius of units whose leveling picks
/// change it (Spotter), by id.
Sight Vision::sight(int64_t p, const GameState& state, std::vector<uint64_t>& explored,
                    const std::map<int64_t, double>& farther, const std::map<int64_t, double>& reveal,
                    const std::map<int64_t, double>& beams, const std::vector<std::pair<Vec2, double>>& daylight,
                    double nightShare) const {
    Sight s;
    sightInto(s, p, state, explored, farther, reveal, beams, daylight, nightShare);
    return s;
}

void Vision::sightInto(Sight& s, int64_t p, const GameState& state, std::vector<uint64_t>& explored,
                       const std::map<int64_t, double>& farther, const std::map<int64_t, double>& reveal,
                       const std::map<int64_t, double>& beams, const std::vector<std::pair<Vec2, double>>& daylight,
                       double nightShare) const {
    const int64_t cells = width * height;
    s.ids.clear();
    s.seen.assign(static_cast<size_t>(cells), 0);
    s.near.assign(static_cast<size_t>(cells), 0);
    const size_t words = static_cast<size_t>((cells + 63) / 64);
    if (explored.size() != words) explored.assign(words, 0);
    // Every viewer: where, how far, from what level (nil: the air). At
    // night everything sees closer in (`Rules::sightScale`).
    const double night = Rules::sightScale(Rules::hourAt(state.time, state.dayLength, state.heldHour), nightShare);
    std::vector<Viewer> viewers;
    std::vector<bool> holds(towers.size(), false);
    // A world region's daylight: every level, so the widest of all by day.
    for (const auto& [at, r] : daylight)
        if (r > 0) viewers.push_back(Viewer{at, r, std::nullopt});
    // A headlamp's beam: its day sight out, a wedge round its heading.
    struct Beam { Viewer v; double heading; };
    std::vector<Beam> lamps;
    // Its own and its allies' (C++: teams share their sight).
    for (const auto& b : state.structures) {
        if (!(state.allied(b.owner, p) && b.hp > 0)) continue;
        viewers.push_back(Viewer{b.position, Rules::vision(b.kind) * night, tierAt(b.position)});
    }
    for (const auto& u : state.units) {
        if (!(state.allied(u.owner, p) && u.hp > 0)) continue;
        if (hidden(u.task)) continue;
        const bool air = u.stats().air;
        auto f = farther.find(u.id);
        const double day = f != farther.end() ? f->second : Rules::vision(u.kind);
        const std::optional<int8_t> level = air ? std::nullopt : std::optional<int8_t>(tierAt(u.position));
        viewers.push_back(Viewer{u.position, day * night, level});
        if (auto b = beams.find(u.id); b != beams.end() && night < 1) lamps.push_back(Beam{Viewer{u.position, day, level}, b->second});
        if (!air) {
            for (size_t k = 0; k < towers.size(); ++k) {
                if (distance(towers[k].at, u.position) <= Rules::towerReach) holds[k] = true;
            }
        }
    }
    for (size_t k = 0; k < towers.size(); ++k) {
        if (holds[k]) viewers.push_back(Viewer{towers[k].at, Rules::towerSight * night, towers[k].tier});
    }
    // The widest first; one inside another's circle on its level adds
    // nothing (a Prospector by its Citadel).
    std::vector<size_t> order(viewers.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::sort(order.begin(), order.end(), [&viewers](size_t a, size_t b) {
        return viewers[a].r > viewers[b].r || (viewers[a].r == viewers[b].r && a < b);
    });
    std::vector<Viewer> drawn;
    // Once a circle on every level has covered every cell (by day, a pilot's
    // daylight over the whole region) the grid is all seen, near and
    // explored: the circles and beams left change nothing.
    bool whole = false;
    for (size_t k : order) {
        if (whole) break;
        const Viewer& v = viewers[k];
        bool inside = false;
        for (const auto& d : drawn) {
            if ((!d.level || d.level == v.level) && distance(d.c, v.c) + v.r <= d.r) { inside = true; break; }
        }
        if (inside) continue;
        drawn.push_back(v);
        stamp(v.c, v.r, v.level, s.seen, s.near, explored);
        whole = !v.level && coversGrid(v.c, v.r);
    }
    // The beams last: one inside a drawn circle adds nothing.
    const double half = Rules::headlampCone * pi / 180;
    for (const auto& b : lamps) {
        if (whole) break;
        bool inside = false;
        for (const auto& d : drawn) {
            if ((!d.level || d.level == b.v.level) && distance(d.c, b.v.c) + b.v.r <= d.r) { inside = true; break; }
        }
        if (!inside) stamp(b.v.c, b.v.r, b.v.level, s.seen, s.near, explored, std::make_pair(b.heading, half));
    }
    // What of the others' it sees.
    for (const auto& u : state.units) {
        if (!(state.hostile(u.owner, p) && u.hp > 0)) continue;
        if (hidden(u.task)) continue;
        if (u.burrowed()) {
            auto r = reveal.find(u.id);
            if (hides(u, p, state, r != reveal.end() ? r->second : Rules::revealRange)) continue;
        }
        const auto i = index(u.position);
        if (!i) continue;
        if (u.stats().air ? s.near[static_cast<size_t>(*i)] : s.seen[static_cast<size_t>(*i)]) s.ids.insert(u.id);
    }
    for (const auto& b : state.structures) {
        if (!(state.hostile(b.owner, p) && b.hp > 0)) continue;
        if (sees(b, s.seen)) s.ids.insert(b.id);
    }
}

/// A buried Scorpion `u` is out of sight of player `p` (and its allies).
bool Vision::hides(const Unit& u, int64_t p, const GameState& state, double range) {
    // Visible to all for `Rules.strikeReveal` s after each strike (its own
    // clock), and while it reloads.
    if (u.struckAt && state.time - *u.struckAt < Rules::strikeReveal) return false;
    if (u.cooldown.value_or(0) > 0) return false;
    for (const auto& v : state.units) {
        if (!(state.allied(v.owner, p) && v.hp > 0) || hidden(v.task)) continue;
        if (distance(v.position, u.position) <= range) return false;
    }
    for (const auto& b : state.structures) {
        if (!(state.allied(b.owner, p) && b.hp > 0)) continue;
        const double d = distance(b.position, u.position);
        if (d - Rules::radius(b.kind) <= range) return false;
        // An enemy Sentinel is the detector: its whole sight.
        if (b.kind == Structure::Kind::sentinel && b.complete() && d <= Rules::vision(b.kind)) return false;
    }
    return true;
}

/// Any cell of a building's square footprint is in `seen`.
bool Vision::sees(const Structure& b, const std::vector<uint8_t>& seen) const {
    const double r = Rules::radius(b.kind) - 0.01;
    const int64_t x0 = max<int64_t>(0, static_cast<int64_t>(std::floor((b.position.x - r - origin.x) / cell)));
    const int64_t x1 = min<int64_t>(width - 1, static_cast<int64_t>(std::floor((b.position.x + r - origin.x) / cell)));
    const int64_t z0 = max<int64_t>(0, static_cast<int64_t>(std::floor((b.position.y - r - origin.y) / cell)));
    const int64_t z1 = min<int64_t>(height - 1, static_cast<int64_t>(std::floor((b.position.y + r - origin.y) / cell)));
    if (!(x0 <= x1 && z0 <= z1)) return false;
    for (int64_t z = z0; z <= z1; ++z) {
        for (int64_t x = x0; x <= x1; ++x) {
            if (seen[static_cast<size_t>(z * width + x)]) return true;
        }
    }
    return false;
}

// (`Rules.vision`, the sight radii, are in Rules.cpp.)

// MARK: - The simulation's fog

/// Player `p` sees the unit or building `id` now (its own always; every
/// one on a map without fog).
bool Simulation::sees(int64_t p, int64_t id) const {
    if (!(p >= 0 && p < static_cast<int64_t>(sights.size()))) {
        // No fog here: everything, but a buried Scorpion (it hides on every map).
        if (const auto i = unitIndex(id)) return !hiddenFrom(state.units[static_cast<size_t>(*i)], p);
        return true;
    }
    if (sights[static_cast<size_t>(p)].ids.count(id) > 0) return true;
    const auto o = owner(id);
    return o && state.allied(*o, p);
}

/// Player `p` sees the ground at `at` now (everywhere without fog).
bool Simulation::sees(int64_t p, Vec2 at) const {
    if (!(vision && p >= 0 && p < static_cast<int64_t>(sights.size()))) return true;
    const auto i = vision->index(at);
    return i ? sights[static_cast<size_t>(p)].seen[static_cast<size_t>(*i)] != 0 : false;
}

/// What player `p` knows of the others (nil: no fog on this map).
std::optional<Intel> Simulation::intel(int64_t p) const {
    if (!(vision && state.intel && p >= 0 && p < static_cast<int64_t>(state.intel->size()))) return std::nullopt;
    return (*state.intel)[static_cast<size_t>(p)];
}

/// What player `p` sees this moment (nil: no fog on this map).
std::optional<Sight> Simulation::sight(int64_t p) const {
    if (p >= 0 && p < static_cast<int64_t>(sights.size())) return sights[static_cast<size_t>(p)];
    return std::nullopt;
}

std::optional<int64_t> Simulation::owner(int64_t id) const {
    if (const auto i = unitIndex(id)) return state.units[static_cast<size_t>(*i)].owner;
    for (const auto& s : state.structures) {
        if (s.id == id) return s.owner;
    }
    return std::nullopt;
}

/// A target `owner`'s units may pick: its own, or one of the others'
/// it sees.
std::optional<Simulation::Target> Simulation::foe(int64_t id, int64_t owner_) const {
    const auto t = target(id);
    if (!t) return std::nullopt;
    // A buried Scorpion is no target to a side that cannot see it, on a map
    // without fog too (with fog its sight leaves it out).
    if (!vision && t->kind == Unit::Kind::scorpion && !t->structure) {
        if (const auto i = unitIndex(id); i && hiddenFrom(state.units[static_cast<size_t>(*i)], owner_)) return std::nullopt;
    }
    return state.allied(t->owner, owner_) || !(owner_ >= 0 && owner_ < static_cast<int64_t>(sights.size()))
                   || sights[static_cast<size_t>(owner_)].ids.count(id) > 0
               ? t
               : std::nullopt;
}

/// A buried Scorpion `u` is hidden from player `p`: an enemy of its side's
/// that has no unit or building within `Rules.revealRange` of it (1 cell with
/// Deep burrow, once driven), no Sentinel seeing it and no strike or reload
/// to give it away (`Vision.hides`). The same on every map, with fog or not.
bool Simulation::hiddenFrom(const Unit& u, int64_t p) const {
    if (!(u.kind == Unit::Kind::scorpion && u.burrowed() && u.hp > 0 && state.hostile(u.owner, p))) return false;
    double range = Rules::revealRange;
    if (pilot) {
        const Boost r = boost(Stat::RevealRange{}, u);
        if (r != Boost::none) range = max(0.0, Rules::revealRange + r.plus);
    }
    return Vision::hides(u, p, state, range);
}

/// The buried Scorpions hidden from player `p` now, by id (the fog's sight
/// holds them out already; this is for a map without it).
std::set<int64_t> Simulation::hiddenScorpions(int64_t p) const {
    std::set<int64_t> out;
    for (const Unit& u : state.units) {
        if (u.kind == Unit::Kind::scorpion && hiddenFrom(u, p)) out.insert(u.id);
    }
    return out;
}

/// The game without the buried Scorpions player `p` cannot see (a map
/// without fog: otherwise the whole game).
GameState Simulation::withoutHidden(int64_t p) const {
    GameState s = state;
    const std::set<int64_t> hid = hiddenScorpions(p);
    if (!hid.empty()) std::erase_if(s.units, [&](const Unit& u) { return hid.count(u.id) > 0; });
    return s;
}

/// Look at once, for a game set up by hand (units moved, no step yet).
void Simulation::lookNow() {
    reindex();
    look();
}

/// Work out what every player sees, and update what each knows.
void Simulation::look() {
    static std::atomic<uint64_t> looks{0};
    lookId = ++looks;
    if (!vision) { sights.clear(); return; }
    // What each knows, taken (not copied) and put back at the end: nothing
    // in between reads `state.intel`.
    std::vector<Intel> intel_ = state.intel ? std::move(*state.intel) : std::vector<Intel>{};
    const int64_t cells = vision->width * vision->height;
    if (intel_.size() != state.players.size()) {
        // A new game: each knows where the others start, and that a
        // Citadel stands there (the start locations are known).
        intel_.clear();
        for (size_t p = 0; p < state.players.size(); ++p) {
            Intel k;
            for (const auto& s : state.structures) {
                if (state.hostile(s.owner, static_cast<int64_t>(p)) && s.kind == Structure::Kind::citadel) k.buildings.push_back(s);
            }
            k.explored.assign(static_cast<size_t>((cells + 63) / 64), 0);
            k.sites.assign(sites.size(), -std::numeric_limits<double>::infinity());
            intel_.push_back(k);
        }
    }
    // Spotter: units whose picks let them see farther; Deep burrow: a
    // Scorpion that is seen from closer in only.
    std::map<int64_t, double> farther, reveal, beams;
    if (pilot) {
        // A driven Prospector's headlamp points where the crosshair is
        // (W5: every human's).
        if (const auto i = unitIndex(pilot->unit); i && state.units[*i].kind == Unit::Kind::prospector)
            beams[pilot->unit] = pilot->heading;
        for (const Driver& d : drivers)
            if (const auto i = unitIndex(d.pilot.unit); i && state.units[*i].kind == Unit::Kind::prospector)
                beams[d.pilot.unit] = d.pilot.heading;
        for (const auto& u : state.units) {
            const Boost b = boost(Stat::Sight{}, u);
            if (b != Boost::none) farther[u.id] = b.apply(Rules::vision(u.kind));
            if (u.kind == Unit::Kind::scorpion) {
                const Boost r = boost(Stat::RevealRange{}, u);
                if (r != Boost::none) reveal[u.id] = max(0.0, Rules::revealRange + r.plus);
            }
        }
    }
    // A world region's daylight (S3): round the pilot, the whole map by
    // day, closing in through dusk to the driven unit's night sight.
    // W5: one circle round each human driving here.
    std::vector<std::pair<Vec2, double>> daylight;
    if (daylightAt) {
        const double hour = Rules::hourAt(state.time, state.dayLength, state.heldHour);
        const double span = std::hypot(static_cast<double>(vision->width), static_cast<double>(vision->height)) * Vision::cell;
        auto circle = [&](Vec2 at, double reach) {
            const double night = reach * Rules::sightScale(hour, Rules::nightSight * nightWiden);
            const double r = Rules::daylightReach(hour, night, span);
            // Through the night it is the unit's own circle: none on top.
            if (r > night + 1e-6) daylight.emplace_back(at, r);
        };
        const std::vector<int64_t> driven = drivenUnits();
        bool any = false;
        for (const int64_t id : driven)
            if (const auto i = unitIndex(id)) {
                const Unit& u = state.units[static_cast<size_t>(*i)];
                const auto f = farther.find(u.id);
                circle(u.position, f != farther.end() ? f->second : Rules::vision(u.kind));
                any = true;
            }
        if (!any) circle(*daylightAt, Rules::vision(Unit::Kind::prospector));
    }
    // The last look's sights, their arrays filled again (no new ones each
    // look).
    std::vector<Sight> seen = std::move(sights);
    sights.clear();
    seen.resize(state.players.size());
    const double now = state.time;
    // Allies see and remember alike (C++, teams): a team's sight and its
    // one memory (`teamIntel`: what any member has scouted) are worked
    // out once, by its first player, and every member takes a copy.
    std::set<int64_t> teamsDone;
    for (size_t p = 0; p < state.players.size(); ++p) {
        const int64_t pl = static_cast<int64_t>(p);
        if (!teamsDone.insert(state.team(pl)).second) continue;
        std::vector<size_t> members;
        for (size_t q = p; q < state.players.size(); ++q) {
            if (state.allied(static_cast<int64_t>(q), pl)) members.push_back(q);
        }
        const auto merging = std::chrono::steady_clock::now();
        Intel k = members.size() == 1 ? std::move(intel_[p]) : teamIntel(intel_, members);
        stepTimes.merge += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - merging).count();
        const bool pilots = state.allied(pl, Pilot::player);
        vision->sightInto(seen[p], pl, state, k.explored, farther, reveal, beams,
                          pilots ? daylight : std::vector<std::pair<Vec2, double>>{}, Rules::nightSight * (pilots ? nightWiden : 1.0));
        const Sight& s = seen[p];
        if (k.sites.size() != sites.size()) k.sites.assign(sites.size(), -std::numeric_limits<double>::infinity());
        for (size_t j = 0; j < sites.size(); ++j) {
            const auto i = vision->index(sites[j]);
            if (i && s.seen[static_cast<size_t>(*i)]) k.sites[j] = now;
        }
        // Buildings: the ones in sight as they are now; the rest as
        // last seen, unless their ground is seen empty.
        std::vector<Structure> shownNow;
        for (const auto& b : state.structures) {
            if (state.hostile(b.owner, pl) && s.ids.count(b.id)) shownNow.push_back(b);
        }
        std::set<int64_t> shownIDs;
        for (const auto& b : shownNow) shownIDs.insert(b.id);
        std::vector<Structure> buildings;
        for (const auto& b : k.buildings) {
            if (!shownIDs.count(b.id) && !vision->sees(b, s.seen)) buildings.push_back(b);
        }
        buildings.insert(buildings.end(), shownNow.begin(), shownNow.end());
        // (Swift's `sort` is not stable either; ids are unique.)
        std::sort(buildings.begin(), buildings.end(), [](const Structure& a, const Structure& b) { return a.id < b.id; });
        k.buildings = std::move(buildings);
        // Units: the same, and a unit out of sight this long is gone
        // from mind.
        std::vector<Intel::Seen> units;
        for (const auto& m : k.units) {
            if (!(!s.ids.count(m.unit.id) && now - m.at <= Rules::memory)) continue;
            const auto i = vision->index(m.unit.position);
            if (!i) { units.push_back(m); continue; }
            if (!(m.unit.stats().air ? s.near[static_cast<size_t>(*i)] : s.seen[static_cast<size_t>(*i)])) units.push_back(m);
        }
        for (const auto& u : state.units) {
            if (state.hostile(u.owner, pl) && s.ids.count(u.id)) units.push_back(Intel::Seen{u, now});
        }
        std::sort(units.begin(), units.end(), [](const Intel::Seen& a, const Intel::Seen& b) { return a.unit.id < b.unit.id; });
        k.units = std::move(units);
        for (size_t q : members) {
            if (q != p) seen[q] = s;
        }
        for (size_t q : members)
            if (q != members.back()) intel_[q] = k;
        intel_[members.back()] = std::move(k);
    }
    sights = std::move(seen);
    state.intel = std::move(intel_);
}

/// The game as player `p` knows it: its own units and buildings, the
/// others' it sees, and the rest of theirs as it last saw them. Its AI
/// plays from this (`seen(by:)`), and its humans' screens show it.
GameState Simulation::known(int64_t p) const {
    if (!(vision && state.intel && p >= 0 && p < static_cast<int64_t>(state.intel->size())
          && p < static_cast<int64_t>(sights.size()))) return withoutHidden(p);
    const std::set<int64_t>& ids = sights[static_cast<size_t>(p)].ids;
    const Intel& k = (*state.intel)[static_cast<size_t>(p)];
    GameState s = state;
    s.units.clear();
    for (const auto& u : state.units) {
        if (state.allied(u.owner, p) || ids.count(u.id)) s.units.push_back(u);
    }
    for (const auto& m : k.units) {
        if (!ids.count(m.unit.id)) s.units.push_back(m.unit);
    }
    s.structures.clear();
    for (const auto& b : state.structures) {
        if (state.allied(b.owner, p) || ids.count(b.id)) s.structures.push_back(b);
    }
    for (const auto& b : k.buildings) {
        if (!ids.count(b.id)) s.structures.push_back(b);
    }
    return s;
}

/// The game as player `p`'s humans see it on screen: as `known(by:)`,
/// but only the others' units in sight now (no memory of units, only
/// of buildings).
GameState Simulation::shown(int64_t p) const {
    if (!(vision && state.intel && p >= 0 && p < static_cast<int64_t>(state.intel->size())
          && p < static_cast<int64_t>(sights.size()))) return withoutHidden(p);
    const std::set<int64_t>& ids = sights[static_cast<size_t>(p)].ids;
    const Intel& k = (*state.intel)[static_cast<size_t>(p)];
    GameState s = state;
    s.units.clear();
    for (const auto& u : state.units) {
        if (state.allied(u.owner, p) || ids.count(u.id)) s.units.push_back(u);
    }
    s.structures.clear();
    for (const auto& b : state.structures) {
        if (state.allied(b.owner, p) || ids.count(b.id)) s.structures.push_back(b);
    }
    for (const auto& b : k.buildings) {
        if (!ids.count(b.id)) s.structures.push_back(b);
    }
    return s;
}

/// The others' units and buildings player `p` does not see now (none
/// without fog).
std::set<int64_t> Simulation::unseen(int64_t p) const {
    if (!(vision && p >= 0 && p < static_cast<int64_t>(sights.size()))) return hiddenScorpions(p);
    const std::set<int64_t>& ids = sights[static_cast<size_t>(p)].ids;
    std::set<int64_t> out;
    for (const auto& u : state.units) {
        if (state.hostile(u.owner, p) && !ids.count(u.id)) out.insert(u.id);
    }
    for (const auto& b : state.structures) {
        if (state.hostile(b.owner, p) && !ids.count(b.id)) out.insert(b.id);
    }
    if (state.intel && p < static_cast<int64_t>(state.intel->size())) {
        for (const auto& b : (*state.intel)[static_cast<size_t>(p)].buildings) {
            if (!ids.count(b.id)) out.insert(b.id);
        }
    }
    return out;
}

/// This simulation as player `p` knows the game (`known(by:)`), for its
/// AI to decide from. Orders it gives are applied to the real one.
Simulation Simulation::seen(int64_t p) const {
    if (!vision) {
        if (hiddenScorpions(p).empty()) return *this;
        Simulation v = *this;
        v.state = withoutHidden(p);
        v.reindex();
        return v;
    }
    Simulation v = *this;
    v.state = known(p);
    v.reindex();
    return v;
}

/// When player `p` last saw base site `i` (-inf: never; now on a map
/// without fog).
double Simulation::lastSeen(int64_t i, int64_t p) const {
    if (!(vision && state.intel && p >= 0 && p < static_cast<int64_t>(state.intel->size()))) return state.time;
    const Intel& k = (*state.intel)[static_cast<size_t>(p)];
    if (!(i >= 0 && i < static_cast<int64_t>(k.sites.size()))) return state.time;
    return k.sites[static_cast<size_t>(i)];
}

} // namespace ac
