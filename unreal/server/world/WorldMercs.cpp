// Mercenary prospectors (WorldMap.h `WorldMercs`, H2; docs/prospector-progression.md
// "Mercenaries"): hired at the realm's Citadel for a fee, paid a wage in MH
// at each world dawn, each one a Prospector of the realm that mines a
// deposit field and hauls to the realm's Citadel there, for its pilot's
// wallet, taxed like any unload, with no XP. In a day it delivers what its
// pilot mines in an hour: the server keeps the ore it has worked (`owed`)
// and lets it drill a load only once it has worked it, so the rate holds
// whatever the walk. With the regions held (S2's idle, a catch-up) the same
// work is bookkeeping: ore from its field straight to the wallet.

#include "WorldMap.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ac::server {

using json = nlohmann::json;
using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

// MARK: - The rules

int64_t WorldMercs::limit(int64_t level) { return level >= 30 ? 3 : level >= 15 ? 2 : level >= 5 ? 1 : 0; }

int64_t WorldMercs::nextLevel(int64_t held) { return held <= 0 ? 5 : held == 1 ? 15 : held == 2 ? 30 : 0; }

int64_t WorldMercs::feeOre(int64_t held) { return 500 << std::clamp<int64_t>(held, 0, 8); }

int64_t WorldMercs::feeHydrogen(int64_t held) { return 50 << std::clamp<int64_t>(held, 0, 8); }

int64_t WorldMercs::wage(int64_t level) { return 5 + std::max<int64_t>(level, 1) / 5; }

double WorldMercs::rate(int64_t level, bool dockCrew) {
    return WorldContracts::perMinute(level) * 60 / ((dockCrew ? RealmProject::dockCrewHours : hours) * 3600);
}

const std::vector<std::string>& WorldMercs::names() {
    static const std::vector<std::string> n = {"Bracken", "Cinder", "Dace",   "Ember",  "Flint",  "Garnet", "Harrow", "Ingot",
                                               "Jasper",  "Kiln",   "Larch",  "Marrow", "Nettle", "Onyx",   "Pike",   "Quarry",
                                               "Rust",    "Slate",  "Tallow", "Umber",  "Vetch",  "Wren",   "Yarrow", "Zinc"};
    return n;
}

// MARK: - Fields and depots

const std::vector<std::vector<size_t>>& WorldMap::fields(size_t r) const {
    if (fields_.size() < regions_.size()) fields_.resize(regions_.size());
    if (fields_[r]) return *fields_[r];
    // The region file's own deposits (starfall deposits come and go), joined
    // when within `fieldLink` of each other.
    const auto& patches = regions_[r]->simulation().state.patches;
    const size_t n = std::min(patches.size(), r < ownPatches_.size() ? ownPatches_[r] : patches.size());
    std::vector<int64_t> group(n, -1);
    std::vector<std::vector<size_t>> out;
    for (size_t i = 0; i < n; i++) {
        if (group[i] >= 0) continue;
        group[i] = int64_t(out.size());
        std::vector<size_t> field{i};
        for (size_t k = 0; k < field.size(); k++)
            for (size_t j = 0; j < n; j++)
                if (group[j] < 0 && distance(patches[field[k]].position, patches[j].position) <= WorldMercs::fieldLink) {
                    group[j] = group[i];
                    field.push_back(j);
                }
        std::sort(field.begin(), field.end());
        out.push_back(std::move(field));
    }
    fields_[r] = std::move(out);
    return *fields_[r];
}

Vec2 WorldMap::fieldAt(size_t r, int64_t field) const {
    const auto& f = fields(r);
    if (field < 0 || size_t(field) >= f.size() || f[size_t(field)].empty()) return Vec2(0, 0);
    const auto& patches = regions_[r]->simulation().state.patches;
    Vec2 sum(0, 0);
    for (size_t i : f[size_t(field)]) sum = sum + patches[i].position;
    return sum * (1.0 / double(f[size_t(field)].size()));
}

int64_t WorldMap::fieldOre(size_t r, int64_t field) const {
    const auto& f = fields(r);
    if (field < 0 || size_t(field) >= f.size()) return 0;
    int64_t ore = 0;
    for (size_t i : f[size_t(field)]) ore += std::max<int64_t>(0, regions_[r]->simulation().state.patches[i].remaining);
    return ore;
}

std::optional<Vec2> WorldMap::depot(size_t r, Vec2 at) const {
    // The realm's Citadels there: where the core's Prospector hauls (`bases`).
    std::optional<Vec2> best;
    for (const Structure& b : regions_[r]->simulation().bases(Pilot::player))
        if (realm(b) && (!best || distance(b.position, at) < distance(*best, at))) best = b.position;
    return best;
}

bool WorldMap::mercUnit(PilotState::Merc& m) {
    const Vec2 field = fieldAt(m.region, m.field);
    const auto home = depot(m.region, field);
    if (!home) return false;
    Region& r = *regions_[m.region];
    Simulation& sim = r.simulation();
    // At the Citadel's edge, on the field's side, on open ground.
    const Vec2 out = normalize(field - *home);
    Unit v = sim.freshUnit(kMercenaryKind, 0, Pilot::player, *home, 0);
    const Vec2 first = *home + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(StructureKind::citadel) + 1.0);
    v.position = first;
    for (double rad = 0; rad <= 6 && !sim.pilotCanStand(v, v.position); rad += 0.5)
        for (int k = 0; k < 16; k++) {
            const double a = k * 6.283185307179586 / 16;
            const Vec2 q = first + Vec2(std::cos(a), std::sin(a)) * rad;
            if (sim.pilotCanStand(v, q)) {
                v.position = q;
                break;
            }
        }
    v.heading = std::atan2(out.y, out.x);
    v.carrying = m.cargo;
    m.unit = r.admit(v);
    m.drilling = std::nullopt;
    return true;
}

// MARK: - Hiring

std::optional<std::string> WorldMap::hireMerc(PilotId pilot, bool guard) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    const int64_t level = p->progress.level();
    const int64_t held = int64_t(p->mercs.size());
    if (WorldMercs::limit(level) <= 0) return std::string("Mercenaries sign on with pilots of level 5 and up");
    if (held >= WorldMercs::limit(level)) {
        std::string why = "You hold " + std::to_string(held) + (held == 1 ? " mercenary" : " mercenaries") + ", the most at level " +
                          std::to_string(level);
        if (const int64_t next = WorldMercs::nextLevel(held)) why += ": " + std::to_string(held + 1) + " at level " + std::to_string(next);
        return why;
    }
    // R3: at any Citadel; the merc works the nearest field to that one.
    const auto citadel = citadelAt(pilot, *p);
    if (!citadel) return std::string("Hire at a Citadel: stand where you unload");
    const size_t r = citadel->first;
    const auto at = regions_[r]->simulation().state.structure(citadel->second);
    std::optional<int64_t> field;
    for (size_t f = 0; f < fields(r).size(); f++)
        if (fieldOre(r, int64_t(f)) > 0 &&
            (!field || distance(fieldAt(r, int64_t(f)), at->position) < distance(fieldAt(r, *field), at->position)))
            field = int64_t(f);
    if (!field) return std::string("No deposit field with ore near the Citadel");
    const int64_t ore = WorldMercs::feeOre(held), mh = WorldMercs::feeHydrogen(held);
    bankOut(*p, p->region);
    if (ore > p->wallet.ore) return "Not enough ore: " + std::to_string(ore) + ", you have " + std::to_string(p->wallet.ore);
    if (mh > p->wallet.hydrogen) return "Not enough MH: " + std::to_string(mh) + ", you have " + std::to_string(p->wallet.hydrogen);
    p->wallet.ore -= ore;
    p->wallet.hydrogen -= mh;
    bankIn(*p, p->region);
    PilotState::Merc m;
    m.id = p->nextMerc++;
    // A name none of its others has, by the pilot's key and the number.
    const auto& names = WorldMercs::names();
    size_t pick = (std::hash<std::string>{}(p->key) + size_t(m.id) * 7) % names.size();
    for (size_t k = 0; k < names.size(); k++, pick = (pick + 1) % names.size())
        if (std::none_of(p->mercs.begin(), p->mercs.end(), [&](const PilotState::Merc& o) { return o.name == names[pick]; })) break;
    m.name = names[pick];
    m.region = r;
    m.field = *field;
    m.guard = guard;
    mercUnit(m);
    // Its first wage at the next dawn.
    if (p->mercs.empty()) p->mercDay = contractDay(clock_);
    p->mercs.push_back(m);
    goalDid(*p, "merc");
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::dismissMerc(PilotId pilot, uint32_t merc) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    const auto it = std::find_if(p->mercs.begin(), p->mercs.end(), [&](const PilotState::Merc& m) { return m.id == merc; });
    if (it == p->mercs.end()) return std::string("No such mercenary");
    if (it->unit) regions_[mercIn(*it)]->release(*it->unit);
    p->mercs.erase(it);
    return std::nullopt;
}

std::optional<std::string> WorldMap::mercRole(PilotId pilot, uint32_t merc, bool guard) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    const auto it = std::find_if(p->mercs.begin(), p->mercs.end(), [&](const PilotState::Merc& m) { return m.id == merc; });
    if (it == p->mercs.end()) return std::string("No such mercenary");
    // A guard on a hunt finishes it; the role counts from the next call.
    it->guard = guard;
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::assignMerc(PilotId pilot, uint32_t merc, std::string_view region, int64_t field) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    const auto it = std::find_if(p->mercs.begin(), p->mercs.end(), [&](const PilotState::Merc& m) { return m.id == merc; });
    if (it == p->mercs.end()) return std::string("No such mercenary");
    const auto r = regionNamed(region);
    if (!r) return "No region " + std::string(region);
    if (field < 0 || size_t(field) >= fields(*r).size()) return std::string("No such field");
    const std::string& name = world_->regions[*r].file.name;
    if (!depot(*r, fieldAt(*r, field))) return "No realm Citadel in " + name + " to haul to";
    if (fieldOre(*r, field) <= 0) return std::string("That field is mined out");
    PilotState::Merc& m = *it;
    if (m.hunting || m.homeward || m.in) return m.name + " is away on the hunt: back at its field after";
    if (*r != m.region) {
        // Another region: there at once, at its Citadel, with its load.
        if (m.unit)
            if (const auto u = regions_[m.region]->simulation().state.unit(*m.unit)) m.cargo = u->carrying;
        if (m.unit) regions_[m.region]->release(*m.unit);
        m.unit = std::nullopt;
        m.region = *r;
        m.field = field;
        if (m.downUntil <= clock_) mercUnit(m);
    } else {
        m.field = field;
        // Off to the new field (drilling stops; what it carries goes home).
        if (m.unit)
            if (const auto i = regions_[m.region]->simulation().unitIndex(*m.unit)) {
                Simulation& sim = regions_[m.region]->simulation();
                Unit& u = sim.state.units[size_t(*i)];
                for (auto& d : sim.state.patches)
                    if (d.miner == u.id) d.miner = std::nullopt;
                u.patch = std::nullopt;
                u.goal = std::nullopt;
                u.waypoints = std::nullopt;
                u.task = u.carrying > 0 ? Unit::Task::toBase : Unit::Task::idle;
                u.timer = 1;
                m.drilling = std::nullopt;
            }
    }
    pushProgress(pilot);
    return std::nullopt;
}

std::vector<WorldMap::MercView> WorldMap::mercs(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return {};
    std::vector<MercView> out;
    for (const auto& m : it->second.mercs)
        out.push_back(MercView{m.id, m.name, m.region, m.field, m.unit, m.owed, m.downUntil, m.delivered, m.guard, m.hunting, mercIn(m)});
    return out;
}

std::vector<WorldMap::MercView> WorldMap::mercsOf(const std::string& key) const {
    const PilotState* p = nullptr;
    for (const auto& [id, q] : pilots_)
        if (q.key == key) p = &q;
    if (const auto a = away_.find(key); a != away_.end()) p = &a->second;
    if (!p) return {};
    std::vector<MercView> out;
    for (const auto& m : p->mercs)
        out.push_back(MercView{m.id, m.name, m.region, m.field, m.unit, m.owed, m.downUntil, m.delivered, m.guard, m.hunting, mercIn(m)});
    return out;
}

// MARK: - Pay

void WorldMap::credit(PilotState& p, bool online, const Wallet& w) {
    // On line, its wallet is its region's bank (`bankOut` after each step).
    const bool banked = online && !p.flight;
    if (banked) bankOut(p, p.region);
    p.wallet.ore += w.ore;
    p.wallet.hydrogen += w.hydrogen;
    if (banked) bankIn(p, p.region);
}

void WorldMap::mercDelivered(PilotState& p, bool online, PilotState::Merc& m, int64_t ore) {
    if (ore <= 0) return;
    // Taxed load by load, as its unloads are (C1).
    int64_t tax = 0;
    for (int64_t left = ore; left > 0; left -= WorldMercs::load) tax += Progression::realmTax(std::min(left, WorldMercs::load));
    realm_.ore += tax;
    p.taxed.ore += tax;
    credit(p, online, Wallet{ore - tax, 0});
    m.delivered += ore - tax;
    // S4: its hirer's week, the ore as unloaded.
    standNote(p.key, &Standing::ore, ore);
    if (!online) {
        p.mercAway += ore - tax;
        awayNote(p, net::AwayRow{0, net::AwayKind::merc, m.name, "", "", ore - tax});
    }
}

void WorldMap::mercWages(double at) {
    const int64_t day = contractDay(at);
    auto pay = [&](PilotState& p, bool online, std::vector<News>* news) {
        if (p.mercs.empty()) {
            p.mercDay = day;
            return;
        }
        while (p.mercDay < day) {
            p.mercDay += 1;
            const int64_t wage = WorldMercs::wage(p.progress.level());
            if (online && !p.flight) bankOut(p, p.region);
            for (auto it = p.mercs.begin(); it != p.mercs.end();) {
                if (p.wallet.hydrogen >= wage) {
                    p.wallet.hydrogen -= wage;
                    ++it;
                    continue;
                }
                // Unpaid: it leaves.
                if (news) news->push_back(News{NewsKind::merc, "left", 0, wage, it->name});
                if (it->unit) regions_[mercIn(*it)]->release(*it->unit);
                it = p.mercs.erase(it);
            }
            if (online && !p.flight) bankIn(p, p.region);
        }
    };
    for (auto& [id, p] : pilots_) pay(p, true, &p.news);
    for (auto& [key, p] : away_) pay(p, false, &p.news);
}

// MARK: - Live

void WorldMap::mercsStep(double dt) {
    using Task = Unit::Task;
    auto steer = [&](PilotState& p, bool online) {
        for (PilotState::Merc& m : p.mercs) {
            Simulation& sim = regions_[mercIn(m)]->simulation();
            GameState& st = sim.state;
            const auto i = m.unit ? sim.unitIndex(*m.unit) : std::nullopt;
            if (!i) {
                // Fallen (or never placed): back at its depot at once, and at
                // work after `downFor` (N4: out of the hunt, home).
                if (m.unit) {
                    m.downUntil = clock_ + WorldMercs::downFor;
                    p.news.push_back(News{NewsKind::merc, "down", 0, int64_t(WorldMercs::downFor), m.name});
                }
                m.unit = std::nullopt;
                m.in = std::nullopt;
                m.hunting = m.homeward = false;
                if (!mercUnit(m)) continue;
                continue;
            }
            // N4: away on the hunt, or on its way back.
            if (m.hunting || m.homeward || m.in) {
                m.drilling = std::nullopt;
                mercTrek(p, m);
                continue;
            }
            Unit& u = st.units[size_t(*i)];
            m.cargo = u.carrying;
            const bool base = depot(m.region, u.position).has_value();
            // The nearest deposit of its field with ore (its own first).
            const auto& field = fields(m.region);
            std::optional<size_t> target;
            if (size_t(m.field) < field.size()) {
                const auto& f = field[size_t(m.field)];
                if (u.patch && std::find(f.begin(), f.end(), size_t(*u.patch)) != f.end() && st.patches[size_t(*u.patch)].remaining > 0)
                    target = size_t(*u.patch);
                else
                    for (size_t k : f)
                        if (st.patches[k].remaining > 0 &&
                            (!target || distance(st.patches[k].position, u.position) < distance(st.patches[*target].position, u.position)))
                            target = k;
            }
            const bool waits = clock_ < m.downUntil;
            if (waits || !target || !base) {
                // No work: what it carries goes home, then it stands.
                m.drilling = std::nullopt;
                if (u.task == Task::mining || u.task == Task::waiting || u.task == Task::toPatch) {
                    for (auto& d : st.patches)
                        if (d.miner == u.id) d.miner = std::nullopt;
                    u.task = Task::idle;
                }
                if (u.task == Task::idle) u.timer = 1;
                continue;
            }
            m.owed += WorldMercs::rate(p.progress.level(), projectOn(RealmProject::Kind::dockCrew)) * dt;
            // Bound for a deposit outside its field (a new field): this one.
            const auto& f = field[size_t(m.field)];
            if ((u.task == Task::toPatch || u.task == Task::waiting || u.task == Task::mining) &&
                (!u.patch || std::find(f.begin(), f.end(), size_t(*u.patch)) == f.end())) {
                for (auto& d : st.patches)
                    if (d.miner == u.id) d.miner = std::nullopt;
                u.task = Task::idle;
                u.goal = std::nullopt;
                u.waypoints = std::nullopt;
            }
            if (u.task == Task::idle && u.carrying <= 0) {
                u.patch = int64_t(*target);
                u.task = Task::toPatch;
                u.timer = 0;
            } else if (u.task == Task::idle) {
                u.timer = 1;
            }
            if (u.task != Task::mining) {
                m.drilling = std::nullopt;
                continue;
            }
            // Drilling: the core's own timer never runs out; the load comes
            // once it is worked, and at least `miningTime` drilled.
            if (!m.drilling) m.drilling = clock_;
            u.timer = 1e9;
            if (!u.patch) continue;
            OreDeposit& d = st.patches[size_t(*u.patch)];
            const int64_t need = std::min(WorldMercs::load, std::max<int64_t>(1, d.remaining));
            if (clock_ - *m.drilling < Rules::miningTime || m.owed < double(need)) continue;
            const int64_t take = std::min(int64_t(std::floor(m.owed)), d.remaining);
            if (take <= 0) continue;
            d.remaining -= take;
            d.miner = std::nullopt;
            if (d.remaining <= 0) d.depletedAt = st.time;
            u.carrying = take;
            u.hydrogen = std::nullopt;
            u.task = Task::toBase;
            u.timer = 0;
            m.owed -= double(take);
            m.cargo = take;
            m.drilling = std::nullopt;
        }
        // Its unloads: out of the region's bank, into its pilot's wallet.
        for (PilotState::Merc& m : p.mercs) {
            if (!m.unit) continue;
            for (const GameEvent& e : regions_[mercIn(m)]->lastEvents()) {
                const auto* d = e.as<GameEvent::Deposited>();
                if (!d || d->unit != *m.unit || d->amount <= 0) continue;
                Player& side = regions_[mercIn(m)]->simulation().state.players[0];
                (d->hydrogen ? side.hydrogen : side.ore) -= d->amount;
                mercDelivered(p, online, m, d->amount);
                m.cargo = 0;
            }
        }
    };
    for (auto& [id, p] : pilots_) steer(p, true);
    for (auto& [key, p] : away_) steer(p, false);
    mercWages(clock_);
}

// MARK: - Held regions

void WorldMap::mercsBook(double from, double to) {
    auto work = [&](PilotState& p, bool online, double a, double b) {
        for (PilotState::Merc& m : p.mercs) {
            // N4: away on the hunt: no work booked.
            if (m.hunting || m.homeward || m.in) continue;
            Simulation& sim = regions_[m.region]->simulation();
            GameState& st = sim.state;
            const double start = std::max(a, m.downUntil);
            if (!(b > start)) continue;
            // Fallen and back by now: at its depot.
            if (!m.unit || !sim.unitIndex(*m.unit)) {
                m.unit = std::nullopt;
                if (!mercUnit(m)) continue;
            }
            const auto& field = fields(m.region);
            if (size_t(m.field) >= field.size() || fieldOre(m.region, m.field) <= 0) continue;
            if (!depot(m.region, fieldAt(m.region, m.field))) continue;
            // R2: the Dock Crew's share of the span at twice the rate.
            double crew = 0;
            if (const auto& dc = projects_[size_t(RealmProject::Kind::dockCrew)])
                crew = std::max(0.0, std::min(b, dc->ends) - std::max(start, dc->ends - RealmProject::days * params_.dayLength));
            m.owed += WorldMercs::rate(p.progress.level()) * (b - start - crew) + WorldMercs::rate(p.progress.level(), true) * crew;
            // Whole ore from its field, its own deposit first, then the
            // nearest to it.
            const auto i = sim.unitIndex(*m.unit);
            const Unit* u = i ? &st.units[size_t(*i)] : nullptr;
            std::vector<size_t> order = field[size_t(m.field)];
            const Vec2 from_ = u ? u->position : fieldAt(m.region, m.field);
            std::sort(order.begin(), order.end(), [&](size_t x, size_t y) {
                const bool ox = u && u->patch == int64_t(x), oy = u && u->patch == int64_t(y);
                if (ox != oy) return ox;
                return distance(st.patches[x].position, from_) < distance(st.patches[y].position, from_);
            });
            // In whole loads, as its trips are (the last of a field aside).
            const int64_t loads = int64_t(std::floor(m.owed / double(WorldMercs::load))) * WorldMercs::load;
            const int64_t left = fieldOre(m.region, m.field);
            const int64_t all = loads >= left ? std::min(int64_t(std::floor(m.owed)), left) : loads;
            int64_t taken = 0;
            for (size_t k : order) {
                OreDeposit& d = st.patches[k];
                const int64_t want = all - taken;
                if (want <= 0) break;
                const int64_t take = std::min(want, std::max<int64_t>(0, d.remaining));
                if (take <= 0) continue;
                d.remaining -= take;
                if (d.remaining <= 0) d.depletedAt = st.time;
                taken += take;
            }
            m.owed -= double(taken);
            // A field mined out: no more work owed.
            if (fieldOre(m.region, m.field) <= 0) m.owed = 0;
            mercDelivered(p, online, m, taken);
        }
    };
    double t = from;
    while (t < to) {
        // Each day apart (on a dawn itself, rounding may name that dawn).
        double end = std::min(to, nextDawn(t));
        if (!(end > t)) end = std::min(to, nextDawn(t + 1e-3));
        for (auto& [id, p] : pilots_) work(p, true, t, end);
        for (auto& [key, p] : away_) work(p, false, t, end);
        t = end;
        mercWages(t);
    }
}

// MARK: - Told

void WorldMap::mercsTold(const PilotState& p, net::ProgressMessage& m) const {
    using Merc = net::ProgressMessage::Merc;
    const int64_t level = p.progress.level();
    const int64_t held = int64_t(p.mercs.size());
    for (const auto& x : p.mercs) {
        Merc out;
        out.id = x.id;
        out.name = x.name;
        out.region = world_->regions[x.region].file.id;
        out.field = x.field;
        out.downUntil = x.downUntil > clock_ ? x.downUntil : 0;
        out.delivered = x.delivered;
        out.unit = x.unit.value_or(0);
        out.guard = x.guard;
        out.hunting = x.hunting || x.homeward || x.in.has_value();
        out.state = out.downUntil > 0                                          ? Merc::State::down
                    : !depot(x.region, fieldAt(x.region, x.field))            ? Merc::State::noDepot
                    : fieldOre(x.region, x.field) <= 0                         ? Merc::State::empty
                                                                               : Merc::State::working;
        m.mercs.push_back(std::move(out));
    }
    // The fields with ore in the regions with a realm Citadel, the pilot's
    // own region first (R3: a Citadel a region).
    std::vector<size_t> order{p.region};
    for (size_t r = 0; r < regions_.size(); r++)
        if (r != p.region) order.push_back(r);
    for (size_t r : order) {
        if (regions_[r]->simulation().bases(Pilot::player).empty()) continue;
        for (size_t f = 0; f < fields(r).size(); f++) {
            const Vec2 at = fieldAt(r, int64_t(f));
            const int64_t ore = fieldOre(r, int64_t(f));
            if (ore <= 0 || !depot(r, at)) continue;
            m.mercFields.push_back(net::ProgressMessage::MercField{world_->regions[r].file.id, world_->regions[r].file.name, int64_t(f),
                                                                   regions_[r]->places() ? regions_[r]->places()->regionAt(at) : at,
                                                                   int64_t(fields(r)[f].size()), ore / 100 * 100});
        }
    }
    m.mercLimit = WorldMercs::limit(level);
    m.mercFee = {WorldMercs::feeOre(held), WorldMercs::feeHydrogen(held)};
    m.mercWage = WorldMercs::wage(level);
    m.mercRate = WorldMercs::rate(level, projectOn(RealmProject::Kind::dockCrew)) * 3600;
    m.mercNextLevel = WorldMercs::nextLevel(m.mercLimit);
    // Every mercenary's unit in the pilot's region, any pilot's.
    if (!p.flight) {
        auto tag = [&](const PilotState& q) {
            for (const auto& x : q.mercs)
                if (x.unit && mercIn(x) == p.region) m.mercTags[*x.unit] = x.name;
        };
        for (const auto& [id, q] : pilots_) tag(q);
        for (const auto& [key, q] : away_) tag(q);
    }
}

// MARK: - Saved

std::string WorldMap::encodeMercs(const PilotState& p) const {
    if (p.mercs.empty() && p.nextMerc == 1 && p.mercAway == 0) return "";
    json list = json::array();
    for (const auto& m : p.mercs) {
        json j{{"id", m.id},       {"name", m.name},   {"region", world_->regions[m.region].file.id}, {"field", m.field},          {"owed", m.owed},
               {"down", m.downUntil}, {"cargo", m.cargo}, {"delivered", m.delivered}};
        if (m.unit) j["unit"] = *m.unit;
        // N4: its role, and the hunt it is away on.
        if (m.guard) j["guard"] = true;
        if (m.hunting) j["hunting"] = true;
        if (m.homeward) j["homeward"] = true;
        if (m.in) j["in"] = world_->regions[*m.in].file.id;
        list.push_back(j);
    }
    return json{{"day", p.mercDay}, {"next", p.nextMerc}, {"away", p.mercAway}, {"list", list}}.dump();
}

bool WorldMap::decodeMercs(const std::string& text, PilotState& p) const {
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("list") || !root["list"].is_array()) return false;
    std::vector<PilotState::Merc> out;
    for (const json& j : root["list"]) {
        if (!j.is_object()) return false;
        PilotState::Merc m;
        m.id = j.value("id", uint32_t(0));
        m.name = j.value("name", std::string());
        // The region by its id (an index before would not survive a new world file).
        const auto r = regionNamed(j.value("region", std::string()));
        if (!r) continue;
        m.region = *r;
        m.field = j.value("field", int64_t(0));
        if (j.contains("unit") && j["unit"].is_number_integer()) m.unit = j["unit"].get<int64_t>();
        m.owed = j.value("owed", 0.0);
        m.downUntil = j.value("down", 0.0);
        m.cargo = j.value("cargo", int64_t(0));
        m.delivered = j.value("delivered", int64_t(0));
        m.guard = j.value("guard", false);
        m.hunting = j.value("hunting", false);
        m.homeward = j.value("homeward", false);
        if (j.contains("in") && j["in"].is_string()) m.in = regionNamed(j["in"].get<std::string>());
        out.push_back(m);
    }
    p.mercs = std::move(out);
    p.mercDay = root.value("day", int64_t(0));
    p.nextMerc = root.value("next", uint32_t(1));
    p.mercAway = root.value("away", int64_t(0));
    return true;
}

} // namespace ac::server
