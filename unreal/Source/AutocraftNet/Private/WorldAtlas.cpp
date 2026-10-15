#include "WorldAtlas.h"

#include "Pilot.h"
#include "Simulation.h"
#include "WindowMaps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace ac::net::atlas {

namespace {

/// "0:42", "3:05", "1:02:09".
std::string clock(double seconds) {
    const int64_t s = std::max<int64_t>(0, int64_t(std::ceil(seconds)));
    char out[32];
    if (s >= 3600) std::snprintf(out, sizeof out, "%lld:%02lld:%02lld", (long long)(s / 3600), (long long)(s / 60 % 60), (long long)(s % 60));
    else std::snprintf(out, sizeof out, "%lld:%02lld", (long long)(s / 60), (long long)(s % 60));
    return out;
}

std::string siteKind(SiteKind k) {
    switch (k) {
    case SiteKind::crate: return "Crate";
    case SiteKind::wreck: return "Wreck";
    case SiteKind::cache: return "Cache";
    case SiteKind::vault: return "Vault";
    }
    return "Site";
}

std::string tierName(CitadelMission::Tier t) {
    switch (t) {
    case CitadelMission::Tier::easy: return "Easy";
    case CitadelMission::Tier::hard: return "Hard";
    case CitadelMission::Tier::perilous: return "Perilous";
    }
    return "";
}

std::string missionKind(CitadelMission::Kind k) {
    switch (k) {
    case CitadelMission::Kind::haul: return "haul";
    case CitadelMission::Kind::survey: return "survey";
    case CitadelMission::Kind::salvage: return "salvage";
    case CitadelMission::Kind::hold: return "hold";
    case CitadelMission::Kind::hunt: return "hunt";
    }
    return "";
}

std::string mercState(const ProgressMessage::Merc& m, double now) {
    switch (m.state) {
    case ProgressMessage::Merc::State::working: return "Working its field";
    case ProgressMessage::Merc::State::down: return "Down: back at work in " + clock(m.downUntil - now);
    case ProgressMessage::Merc::State::empty: return "Its field is mined out";
    case ProgressMessage::Merc::State::noDepot: return "No realm Citadel to haul to";
    }
    return "";
}

/// "3 Shardlings, 2 Lodebacks".
std::string broodCount(const std::map<UnitKind, int64_t>& kinds) {
    std::string out;
    for (const auto& [kind, n] : kinds) {
        if (!out.empty()) out += ", ";
        out += std::to_string(n) + " " + Simulation::title(kind) + (n == 1 ? "" : "s");
    }
    return out;
}

const ProgressMessage::MercField* fieldOf(const ProgressMessage& p, const std::string& region, int64_t field) {
    for (const ProgressMessage::MercField& f : p.mercFields)
        if (f.region == region && f.field == field) return &f;
    return nullptr;
}

} // namespace

std::string thousands(int64_t n) {
    std::string digits = std::to_string(n < 0 ? -n : n);
    std::string out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return n < 0 ? "-" + out : out;
}

// MARK: - The regions

std::vector<Place> layout(const WorldMessage& world, const std::set<std::string>& seen) {
    // Each column as wide as its widest region, each row as deep as its deepest.
    std::vector<double> widths(size_t(std::max<int64_t>(world.columns, 0)), 0.0);
    std::vector<double> depths(size_t(std::max<int64_t>(world.rows, 0)), 0.0);
    for (const WorldMessage::Region& r : world.regions) {
        if (r.column < 0 || r.row < 0) continue;
        if (size_t(r.column) >= widths.size()) widths.resize(size_t(r.column) + 1, 0.0);
        if (size_t(r.row) >= depths.size()) depths.resize(size_t(r.row) + 1, 0.0);
        const MapCells c = cells(r.size);
        widths[size_t(r.column)] = std::max(widths[size_t(r.column)], c.width);
        depths[size_t(r.row)] = std::max(depths[size_t(r.row)], c.depth);
    }
    std::vector<Place> out;
    for (size_t i = 0; i < world.regions.size(); ++i) {
        const WorldMessage::Region& r = world.regions[i];
        if (r.column < 0 || r.row < 0) continue;
        Place p;
        p.index = int64_t(i);
        p.id = r.id;
        p.name = r.name;
        for (int64_t c = 0; c < r.column; ++c) p.x += widths[size_t(c)];
        for (int64_t c = 0; c < r.row; ++c) p.y += depths[size_t(c)];
        const MapCells c = cells(r.size);
        p.width = c.width;
        p.depth = c.depth;
        p.seen = seen.count(r.id) > 0 || int64_t(i) == world.here;
        out.push_back(std::move(p));
    }
    return out;
}

Bounds bounds(const std::vector<Place>& places) {
    if (places.empty()) return Bounds{};
    Bounds b{places[0].x, places[0].y, places[0].x + places[0].width, places[0].y + places[0].depth};
    for (const Place& p : places) {
        b.minX = std::min(b.minX, p.x);
        b.minY = std::min(b.minY, p.y);
        b.maxX = std::max(b.maxX, p.x + p.width);
        b.maxY = std::max(b.maxY, p.y + p.depth);
    }
    return b;
}

const Place* placeAt(const std::vector<Place>& places, Vec2 world) {
    for (const Place& p : places)
        if (p.contains(world)) return &p;
    return nullptr;
}

const Place* placeOf(const std::vector<Place>& places, int64_t index) {
    for (const Place& p : places)
        if (p.index == index) return &p;
    return nullptr;
}

const Place* placeOf(const std::vector<Place>& places, const std::string& id) {
    for (const Place& p : places)
        if (p.id == id) return &p;
    return nullptr;
}

// MARK: - The view

Vec2 View::toScreen(Vec2 world, Vec2 screen) const {
    return Vec2((world.x - centre.x) * scale + screen.x / 2, (world.y - centre.y) * scale + screen.y / 2);
}

Vec2 View::toWorld(Vec2 point, Vec2 screen) const {
    return Vec2((point.x - screen.x / 2) / scale + centre.x, (point.y - screen.y / 2) / scale + centre.y);
}

void View::zoomAt(Vec2 point, Vec2 screen, double factor, double floor) {
    const Vec2 under = toWorld(point, screen);
    scale = std::clamp(scale * factor, std::max(minScale, std::min(floor, maxScale)), maxScale);
    // The same world point under the pointer.
    centre = Vec2(under.x - (point.x - screen.x / 2) / scale, under.y - (point.y - screen.y / 2) / scale);
}

void View::pan(Vec2 points) {
    centre = Vec2(centre.x - points.x / scale, centre.y - points.y / scale);
}

void View::clamp(const Bounds& world) {
    centre = Vec2(std::clamp(centre.x, world.minX, world.maxX), std::clamp(centre.y, world.minY, world.maxY));
}

View View::fit(const Bounds& world, Vec2 screen, double margin) {
    View v;
    v.centre = world.centre();
    const double w = std::max(1.0, world.width()), d = std::max(1.0, world.depth());
    v.scale = std::clamp(std::min((screen.x - 2 * margin) / w, (screen.y - 2 * margin) / d), minScale, maxScale);
    return v;
}

// MARK: - The items

std::vector<Item> items(const Input& in, const std::vector<Place>& places) {
    std::vector<Item> out;
    if (!in.world) return out;
    const WorldMessage& world = *in.world;
    const ProgressMessage* progress = in.progress;
    const double now = in.sky ? in.sky->time : 0.0;
    const Place* here = placeOf(places, world.here);
    const GameState* state = in.here && here ? &in.here->state : nullptr;

    // The pilot's own mercenaries' units, and every mercenary's in its region.
    std::set<int64_t> ownMercUnits;
    if (progress)
        for (const ProgressMessage::Merc& m : progress->mercs)
            if (m.unit != 0) ownMercUnits.insert(m.unit);

    // P1: the frontier pilots' units (drawn as theirs, below).
    std::set<int64_t> frontierUnits;
    if (in.sky)
        for (const Sky::Frontier& f : in.sky->frontier)
            if (f.unit != 0 && f.region == world.here) frontierUnits.insert(f.unit);

    // The units in the pilot's region: the pilot, other pilots, mercenaries,
    // the Brood.
    std::vector<const Unit*> brood;
    if (state) {
        for (const Unit& u : state->units) {
            if (u.brood()) {
                brood.push_back(&u);
                continue;
            }
            if (frontierUnits.count(u.id)) continue;
            if (u.owner != Pilot::player || u.kind != UnitKind::prospector) continue;
            Item it;
            it.region = here->index;
            it.at = here->fromGround(u.position);
            it.ref = u.id;
            it.heading = u.heading;
            const std::string load = u.carrying > 0 ? "Carrying " + thousands(u.carrying) + (u.hydrogen ? " MH" : " ore") : "Carrying nothing";
            if (in.driven && u.id == *in.driven) {
                it.kind = Kind::you;
                it.title = progress && !progress->callsign.empty() ? "You · " + progress->callsign : "You";
                it.lines = {"Your Prospector", load};
                it.own = true;
            } else if (progress && progress->mercTags.count(u.id)) {
                it.kind = Kind::merc;
                it.own = ownMercUnits.count(u.id) > 0;
                it.title = "MERC · " + progress->mercTags.at(u.id);
                if (it.own) {
                    for (const ProgressMessage::Merc& m : progress->mercs) {
                        if (m.unit != u.id) continue;
                        const ProgressMessage::MercField* f = fieldOf(*progress, m.region, m.field);
                        it.lines = {"Your mercenary", mercState(m, now),
                                    "Field " + std::to_string(m.field + 1) + (f ? " in " + f->regionName : ""),
                                    thousands(m.delivered) + " ore delivered"};
                    }
                } else {
                    it.lines = {"Another pilot's mercenary"};
                }
                it.lines.push_back(load);
            } else {
                it.kind = Kind::pilot;
                it.title = "Pilot";
                it.lines = {"Another pilot's Prospector", load};
            }
            out.push_back(std::move(it));
        }
    }
    // The pilot's mercenaries elsewhere (or with no unit now): at their field.
    if (progress) {
        for (const ProgressMessage::Merc& m : progress->mercs) {
            if (m.unit != 0 && state && std::any_of(state->units.begin(), state->units.end(), [&](const Unit& u) { return u.id == m.unit; }))
                continue;
            const Place* p = placeOf(places, m.region);
            if (!p) continue;
            const ProgressMessage::MercField* f = fieldOf(*progress, m.region, m.field);
            Item it;
            it.kind = Kind::merc;
            it.region = p->index;
            it.at = f ? p->fromCells(f->at) : p->centre();
            it.own = true;
            it.ref = m.unit;
            it.title = "MERC · " + m.name;
            it.lines = {"Your mercenary", mercState(m, now), "Field " + std::to_string(m.field + 1) + " in " + p->name,
                        thousands(m.delivered) + " ore delivered"};
            out.push_back(std::move(it));
        }
    }
    // P1: the frontier pilots, wherever they are (on their way: not shown);
    // in the pilot's region at their unit's place in the mirror.
    if (in.sky)
        for (const Sky::Frontier& f : in.sky->frontier) {
            if (!f.region) continue;
            const Place* p = placeOf(places, *f.region);
            if (!p) continue;
            const ProgressMessage::FrontierPilot* who = nullptr;
            if (progress)
                for (const ProgressMessage::FrontierPilot& x : progress->frontier)
                    if (x.id == f.id) who = &x;
            Item it;
            it.kind = Kind::frontier;
            it.region = p->index;
            it.at = p->fromCells(f.at);
            it.ref = f.unit;
            if (state && f.unit != 0 && *f.region == world.here)
                for (const Unit& u : state->units)
                    if (u.id == f.unit) {
                        it.at = p->fromGround(u.position);
                        it.heading = u.heading;
                    }
            std::string name = who ? who->callsign : "Pilot";
            it.key = name;
            for (char& c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
            it.title = name + " · LV " + std::to_string(f.level);
            std::string doing = title(f.doing);
            if (!doing.empty()) doing[0] = char(std::toupper(static_cast<unsigned char>(doing[0])));
            it.lines = {std::string("Frontier pilot · ") + (who ? title(who->habit) : "") , doing + " in " + p->name};
            it.paint = who ? who->paint : 2;
            out.push_back(std::move(it));
        }
    // The Brood in groups.
    if (!brood.empty()) {
        std::vector<Vec2> at;
        for (const Unit* u : brood) at.push_back(u->position);
        for (const std::vector<size_t>& g : groups(at, 8)) {
            Vec2 sum(0, 0);
            std::map<UnitKind, int64_t> kinds;
            for (size_t i : g) {
                sum = sum + at[i];
                kinds[brood[i]->kind] += 1;
            }
            const Vec2 mid(sum.x / double(g.size()), sum.y / double(g.size()));
            double spread = 0;
            for (size_t i : g) spread = std::max(spread, distance(at[i], mid));
            Item it;
            it.kind = Kind::bugs;
            it.region = here->index;
            it.at = here->fromGround(mid);
            it.radius = spread;
            it.size = double(g.size());
            it.title = g.size() == 1 ? "Brood · 1 bug" : "Brood · " + std::to_string(g.size()) + " bugs";
            it.lines = {broodCount(kinds)};
            out.push_back(std::move(it));
        }
    }
    // The realm's buildings in the pilot's region.
    if (state) {
        for (const Structure& s : state->structures) {
            if (s.owner != Pilot::player) continue;
            Item it;
            it.kind = Kind::building;
            it.region = here->index;
            it.at = here->fromGround(s.position);
            it.radius = Rules::radius(s.kind);
            it.ref = s.id;
            it.building = s.kind;
            it.size = s.complete() ? 1.0 : s.progress();
            it.title = Simulation::title(s.kind);
            std::string by = "Built by the realm";
            if (progress) {
                if (const auto b = progress->builders.find(s.id); b != progress->builders.end()) {
                    it.own = b->second == progress->callsign;
                    by = it.own ? "Built by you" : "Built by " + b->second;
                }
            }
            it.lines.push_back(by);
            if (!s.complete()) it.lines.push_back("Under construction · " + std::to_string(int(std::floor(s.progress() * 100))) + "%");
            it.lines.push_back("HP " + thousands(int64_t(std::llround(s.hp))) + " / " + thousands(int64_t(std::llround(Rules::hp(s.kind)))));
            out.push_back(std::move(it));
        }
    }
    // Starfalls: the incoming ones and their craters.
    std::set<int64_t> craterPatches;
    if (in.sky) {
        for (const Sky::Starfall& f : in.sky->starfalls) {
            const Place* p = placeOf(places, f.region);
            if (!p) continue;
            Item it;
            it.region = p->index;
            it.at = p->fromGround(f.at);
            it.ref = f.id;
            if (f.impact > now) {
                it.kind = Kind::starfall;
                it.title = "Starfall incoming";
                it.lines = {"Lands in " + clock(f.impact - now), "Fresh ore where it falls"};
            } else {
                it.kind = Kind::crater;
                it.radius = 3;
                it.title = "Starfall crater";
                if (f.patch && state && p == here && *f.patch >= 0 && size_t(*f.patch) < state->patches.size()) {
                    const OreDeposit& d = state->patches[size_t(*f.patch)];
                    craterPatches.insert(*f.patch);
                    it.lines = {thousands(d.remaining) + " ore left"};
                    it.size = d.initial > 0 ? double(d.remaining) / double(d.initial) : 0;
                } else {
                    it.lines = {"Its ore: go and see"};
                }
            }
            out.push_back(std::move(it));
        }
    }
    // The deposits and wells of the pilot's region.
    if (state) {
        for (size_t i = 0; i < state->patches.size(); ++i) {
            if (craterPatches.count(int64_t(i))) continue;
            const OreDeposit& d = state->patches[i];
            Item it;
            it.kind = Kind::deposit;
            it.region = here->index;
            it.at = here->fromGround(d.position);
            it.radius = 1;
            it.ref = d.id;
            it.size = d.initial > 0 ? std::clamp(double(d.remaining) / double(d.initial), 0.0, 1.0) : 0;
            it.title = "Ore deposit";
            if (d.remaining <= 0) it.lines.push_back("Mined out: it grows back");
            else it.lines.push_back(thousands(d.remaining) + " of " + thousands(d.initial) + " ore left");
            // A Lodeback at it eats it.
            for (const Unit* b : brood) {
                if (b->kind != UnitKind::lodeback) continue;
                if ((b->patch && *b->patch == d.id) || distance(b->position, d.position) <= 3) {
                    it.flag = true;
                    it.lines.push_back("Being eaten by a Lodeback");
                    break;
                }
            }
            if (d.miner) it.lines.push_back("Being drilled");
            out.push_back(std::move(it));
        }
        if (state->wells) {
            for (const Well& w : *state->wells) {
                Item it;
                it.kind = Kind::well;
                it.region = here->index;
                it.at = here->fromGround(w.position);
                it.radius = 1.2;
                it.title = "MH well";
                it.lines = {thousands(w.remaining) + " MH left", "A Derrick draws it"};
                out.push_back(std::move(it));
            }
        }
    }
    // The blueprint sites the pilot knows of.
    for (const KnownSite& s : in.sites) {
        const Place* p = placeOf(places, s.region);
        if (!p) continue;
        Item it;
        it.kind = Kind::site;
        it.region = p->index;
        it.at = p->fromCells(s.at);
        it.key = s.region + "/" + s.name;
        it.flag = s.found;
        it.title = s.name;
        it.lines = {siteKind(s.kind) + " · " + Simulation::title(s.blueprint) + " blueprint", s.found ? "Opened" : "Not opened yet"};
        out.push_back(std::move(it));
    }
    // The pads.
    for (const Place& p : places) {
        const WorldMessage::Region& r = world.regions[size_t(p.index)];
        for (const WorldMessage::Pad& q : r.pads) {
            Item it;
            it.kind = Kind::pad;
            it.region = p.index;
            it.at = p.fromCells(q.at);
            it.radius = 3;
            it.key = q.name;
            it.own = q.building.has_value();
            it.building = q.building;
            it.title = q.name;
            // F2: the realm's Landing Pads (and the Spacedock) the Dropship
            // flies between; a region file's pad is where new pilots come down.
            it.lines = {!it.own                                  ? std::string("Where new pilots come down")
                        : *q.building == StructureKind::spacedock ? std::string("The big pad: flights from it cost half")
                                                                  : std::string("The realm's: the Dropship lands here"),
                        p.name};
            out.push_back(std::move(it));
        }
    }
    // The Journal's missions with a place.
    if (progress) {
        for (const CitadelMission& m : progress->journal) {
            if (m.state != CitadelMission::State::taken || m.region.empty()) continue;
            const Place* p = placeOf(places, m.region);
            if (!p) continue;
            Item it;
            it.kind = Kind::mission;
            it.region = p->index;
            it.at = m.kind == CitadelMission::Kind::hunt ? p->centre() : p->fromGround(m.at);
            it.radius = m.radius;
            it.ref = m.id;
            it.tier = m.tier;
            it.title = m.title;
            it.lines = {tierName(m.tier) + " " + missionKind(m.kind) + " mission · " + p->name};
            if (!m.instructions.empty()) it.lines.push_back(m.instructions);
            out.push_back(std::move(it));
        }
    }
    // KSTR's places.
    for (const ContractMark& c : in.contracts) {
        const Place* p = placeOf(places, c.region);
        if (!p) continue;
        Item it;
        it.kind = Kind::contract;
        it.region = p->index;
        it.at = c.at ? p->fromCells(*c.at) : p->centre();
        it.flag = c.at.has_value();
        it.title = "KSTR · " + c.title;
        it.lines = {"Contract · " + p->name};
        out.push_back(std::move(it));
    }
    // The canyon zones' games.
    for (const ZoneMark& z : in.zones) {
        const Place* p = placeOf(places, z.region);
        if (!p) continue;
        Item it;
        it.kind = Kind::zone;
        it.region = p->index;
        it.at = p->fromCells(z.at);
        it.radius = z.radius;
        it.flag = z.alert;
        it.title = z.title;
        it.lines = z.lines;
        out.push_back(std::move(it));
    }
    std::stable_sort(out.begin(), out.end(), [](const Item& a, const Item& b) { return a.kind < b.kind; });
    return out;
}

PadPick padPick(const WorldMessage& world, const std::set<std::string>& seen, int64_t region, int64_t pad) {
    if (region < 0 || region >= int64_t(world.regions.size())) return {false, "No such region"};
    const WorldMessage::Region& r = world.regions[size_t(region)];
    if (pad < 0 || pad >= int64_t(r.pads.size())) return {false, "No such pad"};
    // No Landing Pad of the realm anywhere but here: nothing to fly to yet.
    bool elsewhere = false;
    for (size_t k = 0; k < world.regions.size(); k++)
        if (int64_t(k) != world.here)
            for (const WorldMessage::Pad& q : world.regions[k].pads) elsewhere = elsewhere || q.building.has_value();
    if (!elsewhere) return {false, onlyHere};
    if (region != world.here && !seen.count(r.id)) return {false, "Not discovered yet: " + r.name + ". Go there on foot first"};
    if (r.pads[size_t(pad)].building) return {true, ""};
    const bool realm = std::any_of(r.pads.begin(), r.pads.end(), [](const WorldMessage::Pad& q) { return q.building.has_value(); });
    if (!realm) return {false, "No Landing Pad in " + r.name + ": build one there first"};
    return {false, "That is " + r.name + "'s own pad: the Dropship lands only on a Landing Pad"};
}

std::optional<size_t> pick(const std::vector<Item>& items, const View& view, Vec2 screen, Vec2 point, double reach) {
    std::optional<size_t> best;
    double bestEdge = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        const double d = distance(view.toScreen(it.at, screen), point);
        // The icon's edge or the ground's, whichever is wider.
        const double edge = d - std::max(it.radius * view.scale, 0.0);
        if (edge > reach) continue;
        const double e = std::max(edge, 0.0);
        if (!best || e < bestEdge - 2 || (std::abs(e - bestEdge) <= 2 && it.kind < items[*best].kind)) {
            best = i;
            bestEdge = e;
        }
    }
    return best;
}

std::vector<std::vector<size_t>> groups(const std::vector<Vec2>& at, double link) {
    std::vector<int64_t> group(at.size(), -1);
    std::vector<std::vector<size_t>> out;
    for (size_t i = 0; i < at.size(); ++i) {
        if (group[i] >= 0) continue;
        group[i] = int64_t(out.size());
        std::vector<size_t> members{i};
        for (size_t k = 0; k < members.size(); ++k) {
            for (size_t j = 0; j < at.size(); ++j) {
                if (group[j] >= 0 || distance(at[members[k]], at[j]) > link) continue;
                group[j] = group[i];
                members.push_back(j);
            }
        }
        out.push_back(std::move(members));
    }
    return out;
}

std::vector<Vec2> band(const Place& place, const Ringshadow::Band& b) {
    // The region's rectangle in its ground, clipped to trail ≤ dot(p, dir) ≤ lead.
    const double hw = place.width / 2, hd = place.depth / 2;
    std::vector<Vec2> poly{Vec2(-hw, -hd), Vec2(hw, -hd), Vec2(hw, hd), Vec2(-hw, hd)};
    auto clip = [&](double sign, double limit) {
        // Keep sign * dot(p, dir) <= sign * limit.
        std::vector<Vec2> next;
        for (size_t i = 0; i < poly.size(); ++i) {
            const Vec2 a = poly[i], c = poly[(i + 1) % poly.size()];
            const double da = sign * (dot(a, b.dir) - limit), dc = sign * (dot(c, b.dir) - limit);
            if (da <= 0) next.push_back(a);
            if ((da < 0) != (dc < 0) && da != dc) next.push_back(a + (c - a) * (da / (da - dc)));
        }
        poly = std::move(next);
    };
    const double hi = std::max(b.lead, b.trail), lo = std::min(b.lead, b.trail);
    clip(1, hi);
    if (!poly.empty()) clip(-1, lo);
    for (Vec2& p : poly) p = place.fromGround(p);
    return poly;
}

} // namespace ac::net::atlas
