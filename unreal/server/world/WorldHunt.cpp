// The hunt (N4, WorldMap.h `Hunt`, docs/mmorpg-play.md "The hunt"): the
// Matriarch as the realm's raid. A human calls THE HUNT at the realm's
// Citadel (the REALM tab), free, once a world day realm-wide, never while she
// is down (N3: back in three world days), out on a siege, or a hunt is on.
// The party is chosen at the call: every frontier defender come, and every
// guard mercenary of any pilot, on line or off. Each takes a place on the
// ring round her nest (`WorldHunt::ring` cells, on the side of the region's
// pad) and goes there by the realm's rules: a frontier defender walks
// through the openings or flies the Dropship between the realm's pads,
// paying its fare (its plan, `huntPlan`); a guard walks (`mercTrek`). One
// who can't reach comes late or not at all. At `WorldHunt::gather` seconds,
// or when the caller steps inside the ring if later, they go in together.
// The fight is N3's: her pay by the damage done and her return stand. Her
// fall wins it (`queenDown` calls `huntEnd`); the party and the caller down
// in the assault, or `WorldHunt::lasts` seconds after the call, fail it.

#include "WorldMap.h"

#include "Flight.h"
#include "NavGrid.h"
#include "Rules.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ac::server {

using json = nlohmann::json;

namespace {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;
using State = WorldMap::Hunt::State;
using Act = FrontierPlan::Act;

constexpr const char* mercPrefix = "merc:";

std::string mercKey(const std::string& hirer, uint32_t id) { return mercPrefix + hirer + ":" + std::to_string(id); }

region::Edge across(region::Edge e) {
    switch (e) {
    case region::Edge::north: return region::Edge::south;
    case region::Edge::south: return region::Edge::north;
    case region::Edge::east: return region::Edge::west;
    case region::Edge::west: return region::Edge::east;
    }
    return e;
}

} // namespace

// MARK: - Reading

const WorldMap::Hunt::Member* WorldMap::huntMember(const std::string& key) const {
    for (const Hunt::Member& m : hunt_.party)
        if (m.key == key) return &m;
    return nullptr;
}

std::optional<std::string> WorldMap::huntRefusal(const std::string& key) const {
    if (!world_) return std::string("Not in a world");
    if (frontierKey(key)) return std::string("Only a pilot of the realm calls the hunt");
    const auto nr = nestRegion();
    if (!nr) return std::string("No nest in this world");
    if (nest_.downAt) return "She is down: back in " + net::awayRan(nest_.backAt.value_or(clock_) - clock_);
    if (siege_) return std::string("She is out on a siege");
    if (huntOn()) return std::string("A hunt is on");
    if (hunt_.day == contractDay(clock_)) return "The day's hunt was called: the next at dawn, in " + net::awayRan(nextDawn(clock_) - clock_);
    return std::nullopt;
}

Vec2 WorldMap::farSide(size_t r, size_t next, Vec2 out) const {
    const region::Built& here = world_->regions[r].built;
    const region::Built& there = world_->regions[next].built;
    const Vec2 local = here.regionAt(out);
    Vec2 in = local;
    for (const region::Edge e : {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west})
        if (neighbour(r, e) == next) {
            const double along = e == region::Edge::north || e == region::Edge::south ? local.x : local.y;
            in = there.edgePoint(across(e), along, params_.arriveDepth + 1);
        }
    return there.ground(in);
}

// MARK: - The call

std::optional<std::string> WorldMap::callHunt(PilotId pilot) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (p->key.empty()) return std::string("Only a pilot of the realm calls the hunt");
    if (auto why = huntRefusal(p->key)) return why;
    if (!atRealm(pilot, *p, {StructureKind::citadel})) return std::string("Call the hunt at a Citadel: stand where you unload");
    huntStart(p->key);
    return std::nullopt;
}

void WorldMap::huntStart(const std::string& key) {
    const size_t r = *nestRegion();
    const Vec2 home = *nestAt(r);
    Hunt h;
    h.state = State::gathering;
    h.callerKey = key;
    h.caller = callsign(key);
    h.calledAt = clock_;
    h.day = contractDay(clock_);
    h.region = r;
    h.at = home;
    // The party: the frontier defenders come, the guard mercenaries of
    // every pilot (on line or off).
    for (const FrontierPilot& f : frontier_)
        if (f.arrived && f.habit == WorldFrontier::Habit::defender) {
            Hunt::Member m;
            m.key = f.key;
            m.downAtCall = f.downUntil;
            h.party.push_back(m);
        }
    auto guards = [&](const PilotState& q) {
        for (const PilotState::Merc& m : q.mercs)
            if (m.guard && !q.key.empty()) h.party.push_back(Hunt::Member{mercKey(q.key, m.id)});
    };
    for (const auto& [id, q] : pilots_) guards(q);
    for (const auto& [k, q] : away_) guards(q);
    // Their places on the ring, `spacing` apart, centred on the side of the
    // region's pad (where a pilot comes in by air).
    const region::Built& b = world_->regions[r].built;
    Vec2 toward = b.extent() * 0.5;
    if (!b.pads.empty()) toward = b.pads.front().at;
    double base = std::atan2(toward.y - home.y, toward.x - home.x);
    if (distance(toward, home) < 1) base = 0;
    const double step = WorldHunt::spacing / WorldHunt::ring;
    const Simulation& sim = regions_[r]->simulation();
    for (size_t i = 0; i < h.party.size(); i++) {
        const double k = double(i) - double(h.party.size() - 1) / 2;
        const double a = base + k * step;
        Vec2 at = home + Vec2(std::cos(a), std::sin(a)) * WorldHunt::ring;
        if (sim.nav) at = sim.nav->nearestFree(at).value_or(at);
        h.party[i].slot = at;
    }
    hunt_ = std::move(h);
    // The guards leave their fields.
    auto answer = [&](PilotState& q) {
        for (PilotState::Merc& m : q.mercs)
            if (m.guard) {
                m.hunting = true;
                m.homeward = false;
            }
    };
    for (auto& [id, q] : pilots_) answer(q);
    for (auto& [k, q] : away_) answer(q);
    // The defenders think again at once.
    for (FrontierPilot& f : frontier_)
        if (huntMember(f.key)) f.thought = -1e9;
    const std::string where = world_->regions[r].file.name;
    for (auto& [id, q] : pilots_) q.news.push_back(News{NewsKind::hunt, "called/" + where, 0, int64_t(hunt_.party.size()), hunt_.caller});
}

// MARK: - Each step

void WorldMap::huntStep() {
    if (!world_ || !huntOn()) return;
    Hunt& h = hunt_;
    const std::string where = world_->regions[h.region].file.name;
    const Simulation& sim = regions_[h.region]->simulation();
    // The party: where each is, at its place (in the assault: inside the
    // ring), fallen in the hunt.
    for (Hunt::Member& m : h.party) {
        if (m.fell) continue;
        std::optional<std::pair<size_t, Vec2>> at;
        if (frontierKey(m.key)) {
            const FrontierPilot* f = frontierByKey(m.key);
            if (!f) {
                m.fell = true;
                continue;
            }
            if (f->downUntil > std::max(m.downAtCall, h.calledAt)) {
                m.fell = true;
                continue;
            }
            if (f->arrived && !f->travelTo && clock_ >= f->downUntil) at = std::pair{f->region, frontierAt(*f)};
        } else {
            // "merc:<hirer>:<id>": its hirer's mercenary, answering still.
            const size_t colon = m.key.rfind(':');
            const std::string hirer = m.key.substr(std::string(mercPrefix).size(), colon - std::string(mercPrefix).size());
            const uint32_t id = uint32_t(std::strtoul(m.key.c_str() + colon + 1, nullptr, 10));
            const PilotState* q = nullptr;
            for (const auto& [pid, x] : pilots_)
                if (x.key == hirer) q = &x;
            if (const auto a = away_.find(hirer); !q && a != away_.end()) q = &a->second;
            const PilotState::Merc* merc = nullptr;
            if (q)
                for (const PilotState::Merc& x : q->mercs)
                    if (x.id == id) merc = &x;
            if (!merc || !merc->hunting) {
                m.fell = true;
                continue;
            }
            if (merc->unit)
                if (const auto u = regions_[mercIn(*merc)]->simulation().state.unit(*merc->unit)) at = std::pair{mercIn(*merc), u->position};
        }
        const bool near = at && at->first == h.region &&
                          (h.state == State::assault ? distance(at->second, h.at) <= WorldHunt::ring + WorldHunt::there
                                                     : distance(at->second, m.slot) <= WorldHunt::there);
        if (near && !m.there) m.reached = clock_;
        m.there = near || (m.there && h.state == State::assault && at && at->first == h.region);
    }
    int64_t there = 0, standing = 0;
    for (const Hunt::Member& m : h.party) {
        there += m.there && !m.fell ? 1 : 0;
        standing += m.fell ? 0 : 1;
    }
    // The caller: on line, its Prospector up, inside the ring.
    const PilotState* caller = nullptr;
    for (const auto& [id, q] : pilots_)
        if (q.key == h.callerKey) caller = &q;
    bool callerIn = false, callerUp = caller && !caller->down && !caller->flight;
    if (callerUp && caller->region == h.region && caller->unit)
        if (const auto u = sim.state.unit(*caller->unit); u && u->hp > 0) callerIn = distance(u->position, h.at) <= WorldHunt::ring;
    const bool time = clock_ >= h.calledAt + WorldHunt::gather;
    if (h.state == State::gathering) {
        // Gathered: every one of the party standing is there, or the time.
        if (!h.gatheredTold && !h.party.empty() && ((standing > 0 && there == standing) || time)) {
            h.gatheredTold = true;
            for (auto& [id, q] : pilots_) q.news.push_back(News{NewsKind::hunt, "gathered/" + where, 0, there, h.caller});
        }
        // The charge: the gathering's time over, the caller inside the ring.
        if (time && callerIn) {
            h.state = State::assault;
            h.chargedAt = clock_;
            for (auto& [id, q] : pilots_) q.news.push_back(News{NewsKind::hunt, "charge/" + where, 0, there, h.caller});
            for (FrontierPilot& f : frontier_)
                if (huntMember(f.key)) f.thought = -1e9;
        }
    } else if (!callerUp && there == 0) {
        // The party that went in, and its caller, all down.
        huntEnd(false, true, "", 0);
        return;
    }
    if (clock_ >= h.calledAt + WorldHunt::lasts) huntEnd(false, false, "", 0);
}

void WorldMap::huntEnd(bool won, bool fellParty, const std::string& top, int64_t fighters) {
    if (!huntOn()) return;
    Hunt& h = hunt_;
    h.state = won ? State::won : State::failed;
    h.endedAt = clock_;
    const std::string where = world_->regions[h.region].file.name;
    int64_t went = 0;
    for (const Hunt::Member& m : h.party) went += m.reached >= 0 ? 1 : 0;
    for (auto& [id, q] : pilots_)
        q.news.push_back(won ? News{NewsKind::hunt, "won/" + where, 0, fighters, top}
                             : News{NewsKind::hunt, "failed/" + where, 0, fellParty ? 1 : 0, h.caller});
    awayNoteAll(net::AwayRow{0, net::AwayKind::hunt, h.caller, won ? "won" : "failed", where, went});
    // The guards go back to their fields; the defenders to their own work.
    auto home = [&](PilotState& q) {
        for (PilotState::Merc& m : q.mercs)
            if (m.hunting) {
                m.hunting = false;
                m.homeward = m.in.has_value() || mercIn(m) != m.region;
                if (!m.homeward && m.unit)
                    if (const auto i = regions_[m.region]->simulation().unitIndex(*m.unit)) {
                        Unit& u = regions_[m.region]->simulation().state.units[size_t(*i)];
                        u.mission = std::nullopt;
                        u.task = Unit::Task::idle;
                        u.timer = 1;
                    }
            }
    };
    for (auto& [id, q] : pilots_) home(q);
    for (auto& [key, q] : away_) home(q);
    for (FrontierPilot& f : frontier_)
        if (huntMember(f.key)) f.thought = -1e9;
}

// MARK: - The party's ways there

std::optional<FrontierPlan> WorldMap::huntPlan(const FrontierPilot& f) const {
    if (!huntOn()) return std::nullopt;
    const Hunt::Member* m = huntMember(f.key);
    if (!m || m->fell) return std::nullopt;
    const Hunt& h = hunt_;
    FrontierPlan p;
    p.region = h.region;
    const std::string where = world_->regions[h.region].file.name;
    if (f.region != h.region) {
        // By Dropship when both ends have a pad, two regions or more away,
        // and the wallet the fare (as a wanderer flies); else on foot.
        bool padHere = false, padThere = false;
        for (const LandingPad& l : landingPads()) {
            padHere = padHere || l.region == f.region;
            padThere = padThere || l.region == h.region;
        }
        const int64_t steps = std::abs(world_->regions[f.region].column - world_->regions[h.region].column) +
                              std::abs(world_->regions[f.region].row - world_->regions[h.region].row);
        if (padHere && padThere && steps >= 2 &&
            f.wallet.hydrogen >= ::ac::Flight::price(steps, false, false, projectOn(RealmProject::Kind::beacon)) + WorldFrontier::reserveHydrogen) {
            p.act = Act::fly;
            p.why = "flying to the hunt at " + where;
            return p;
        }
        // On foot, through the openings; none: it can't come.
        if (!frontierStepToward(f.region, h.region, frontierAt(f))) return std::nullopt;
        p.act = Act::walk;
        p.why = "to the hunt at " + where;
        return p;
    }
    if (h.state == State::assault)
        if (nest_.queen)
            if (const auto q = regions_[h.region]->simulation().state.unit(*nest_.queen); q && q->hp > 0) {
                p.act = Act::fight;
                p.target = q->id;
                p.at = q->position;
                p.why = "in the hunt's assault";
                return p;
            }
    // Holding the ring: bugs that come at it, fought.
    if (const auto foe = frontierFoe(f)) {
        p.act = Act::fight;
        p.target = *foe;
        p.at = regions_[h.region]->simulation().state.unit(*foe)->position;
        p.why = "holding the hunt's ring against the Brood";
        return p;
    }
    p.act = Act::patrol;
    p.at = m->slot;
    p.why = "holding at the hunt's ring";
    return p;
}

void WorldMap::mercTrek(PilotState& p, PilotState::Merc& m) {
    (void)p;
    const size_t r = mercIn(m);
    Simulation& sim = regions_[r]->simulation();
    const auto i = m.unit ? sim.unitIndex(*m.unit) : std::nullopt;
    if (!i) return;
    Unit& u = sim.state.units[size_t(*i)];
    // Where to: its place on the ring (in the assault, her), or its field.
    const Hunt::Member* me = m.hunting ? huntMember(mercKey(p.key, m.id)) : nullptr;
    if (m.hunting && (!huntOn() || !me)) {
        m.hunting = false;
        m.homeward = m.in.has_value();
    }
    const size_t to = m.hunting ? hunt_.region : m.region;
    auto errand = [&](Vec2 at) {
        if (u.task == Unit::Task::errand && u.mission && u.mission->is<Mission::Raid>() && distance(u.mission->as<Mission::Raid>()->at, at) <= 0.5)
            return;
        for (auto& d : sim.state.patches)
            if (d.miner == u.id) d.miner = std::nullopt;
        u.patch = std::nullopt;
        u.task = Unit::Task::errand;
        u.mission = Mission{Mission::Raid{at}};
    };
    auto hold = [&] {
        if (u.task == Unit::Task::errand || u.task == Unit::Task::mining || u.task == Unit::Task::toPatch || u.task == Unit::Task::waiting) {
            for (auto& d : sim.state.patches)
                if (d.miner == u.id) d.miner = std::nullopt;
            u.task = Unit::Task::idle;
            u.mission = std::nullopt;
        }
        if (u.task == Unit::Task::idle) u.timer = 1;
    };
    if (r != to) {
        const auto step = frontierStepToward(r, to, u.position);
        if (!step) {
            // No way on foot: it can't answer (or, homeward, it stays).
            if (m.hunting) {
                m.hunting = false;
                m.homeward = m.in.has_value();
            }
            hold();
            return;
        }
        if (distance(u.position, step->second) > WorldFrontier::edgeReach) {
            errand(step->second);
            return;
        }
        // Through the opening: its unit, with its load and its wounds, at
        // the far side.
        Unit v = u;
        const Vec2 in = farSide(r, step->first, step->second);
        regions_[r]->release(u.id);
        Simulation& next = regions_[step->first]->simulation();
        Unit w = next.freshUnit(kMercenaryKind, 0, Pilot::player, in, 0);
        w.position = openGround(next, w, in, 6);
        w.hp = std::min(w.hp, v.hp);
        w.carrying = v.carrying;
        w.hydrogen = v.hydrogen;
        w.heading = v.heading;
        w.timer = 1;
        m.unit = regions_[step->first]->admit(w);
        m.in = step->first == m.region && !m.hunting ? std::nullopt : std::optional<size_t>(step->first);
        return;
    }
    if (!m.hunting) {
        // Home: back to its field's work.
        m.in = std::nullopt;
        m.homeward = false;
        hold();
        return;
    }
    if (hunt_.state == State::assault && nest_.queen)
        if (const auto q = sim.state.unit(*nest_.queen); q && q->hp > 0) {
            for (auto& d : sim.state.patches)
                if (d.miner == u.id) d.miner = std::nullopt;
            u.patch = std::nullopt;
            u.task = Unit::Task::errand;
            u.mission = Mission{Mission::Hunt{q->position}};
            return;
        }
    // Holding the ring: a bug that comes at it, fought.
    if (sim.broodSide) {
        std::optional<Vec2> foe;
        double best = WorldHunt::guard;
        for (const Unit& x : sim.state.units)
            if (x.owner == *sim.broodSide && x.hp > 0 && x.kind != UnitKind::matriarch && distance(x.position, u.position) <= best) {
                best = distance(x.position, u.position);
                foe = x.position;
            }
        if (foe) {
            u.patch = std::nullopt;
            u.task = Unit::Task::errand;
            u.mission = Mission{Mission::Hunt{*foe}};
            return;
        }
    }
    if (distance(u.position, me->slot) > 1.5) errand(me->slot);
    else hold();
}

// MARK: - Told

std::optional<net::HuntMessage> WorldMap::huntMessage(PilotId pilot) const {
    const auto me = pilots_.find(pilot);
    if (!world_ || me == pilots_.end()) return std::nullopt;
    net::HuntMessage m;
    m.state = hunt_.state;
    m.caller = hunt_.caller;
    const auto nr = huntOn() || hunt_.state != State::none ? std::optional<size_t>(hunt_.region) : nestRegion();
    if (nr) {
        m.region = int64_t(*nr);
        m.regionName = world_->regions[*nr].file.name;
        const Vec2 at = huntOn() || hunt_.state != State::none ? hunt_.at : nestAt(*nr).value_or(Vec2(0, 0));
        const Vec2 local = world_->regions[*nr].built.regionAt(at);
        m.x = local.x;
        m.z = local.y;
    }
    m.ring = WorldHunt::ring;
    m.calledAt = hunt_.calledAt;
    m.gatherEnds = hunt_.calledAt + WorldHunt::gather;
    m.chargedAt = hunt_.chargedAt;
    m.failsAt = hunt_.calledAt + WorldHunt::lasts;
    m.endedAt = hunt_.endedAt;
    m.nextCall = hunt_.day == contractDay(clock_) ? nextDawn(clock_) : 0;
    m.refusal = huntRefusal(me->second.key).value_or("");
    if (!huntOn()) return m;
    // The caller first, then the party.
    auto place = [&](net::HuntHunter& x, size_t r, Vec2 at) {
        x.region = int64_t(r);
        const Vec2 local = world_->regions[r].built.regionAt(at);
        x.x = std::round(local.x * 2) / 2;
        x.z = std::round(local.y * 2) / 2;
    };
    for (const auto& [id, q] : pilots_) {
        if (q.key != hunt_.callerKey) continue;
        net::HuntHunter x;
        x.kind = net::HuntHunter::Kind::human;
        x.name = hunt_.caller;
        x.down = q.down.has_value();
        if (!q.flight && q.unit)
            if (const auto u = regions_[q.region]->simulation().state.unit(*q.unit)) {
                place(x, q.region, u->position);
                x.there = q.region == hunt_.region && distance(u->position, hunt_.at) <= WorldHunt::ring;
            }
        m.party.push_back(x);
    }
    for (const Hunt::Member& h : hunt_.party) {
        if (m.party.size() >= net::kHuntPartyMost) break;
        net::HuntHunter x;
        x.there = h.there;
        x.down = h.fell;
        if (frontierKey(h.key)) {
            const FrontierPilot* f = frontierByKey(h.key);
            if (!f) continue;
            x.kind = net::HuntHunter::Kind::frontier;
            x.name = f->callsign;
            x.paint = f->paint;
            if (f->arrived && !f->travelTo) place(x, f->region, frontierAt(*f));
        } else {
            const size_t colon = h.key.rfind(':');
            const std::string hirer = h.key.substr(std::string(mercPrefix).size(), colon - std::string(mercPrefix).size());
            const uint32_t id = uint32_t(std::strtoul(h.key.c_str() + colon + 1, nullptr, 10));
            x.kind = net::HuntHunter::Kind::merc;
            x.owner = callsign(hirer);
            const PilotState* q = nullptr;
            for (const auto& [pid, y] : pilots_)
                if (y.key == hirer) q = &y;
            if (const auto a = away_.find(hirer); !q && a != away_.end()) q = &a->second;
            if (q)
                for (const PilotState::Merc& y : q->mercs)
                    if (y.id == id) {
                        x.name = y.name;
                        if (y.unit)
                            if (const auto u = regions_[mercIn(y)]->simulation().state.unit(*y.unit)) place(x, mercIn(y), u->position);
                    }
            if (x.name.empty()) x.name = "Mercenary";
        }
        m.party.push_back(x);
    }
    return m;
}

// MARK: - Saved

std::string WorldMap::encodeHunt() const {
    if (hunt_ == Hunt{}) return "";
    json party = json::array();
    for (const Hunt::Member& m : hunt_.party)
        party.push_back(json{{"key", m.key}, {"slot", json::array({m.slot.x, m.slot.y})}, {"there", m.there}, {"reached", m.reached},
                             {"fell", m.fell}, {"downAtCall", m.downAtCall}});
    json root{{"state", int(hunt_.state)},  {"callerKey", hunt_.callerKey}, {"caller", hunt_.caller},
              {"called", hunt_.calledAt},   {"charged", hunt_.chargedAt},   {"ended", hunt_.endedAt},
              {"at", json::array({hunt_.at.x, hunt_.at.y})}, {"told", hunt_.gatheredTold}, {"party", party}};
    if (hunt_.day != INT64_MIN) root["day"] = hunt_.day;
    if (world_ && hunt_.region < world_->regions.size()) root["region"] = world_->regions[hunt_.region].file.id;
    return root.dump();
}

bool WorldMap::decodeHunt(const std::string& text) {
    hunt_ = Hunt{};
    // A save from before (schema 24 and under): the day's call is open.
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return false;
    const int state = root.value("state", 0);
    if (state < 0 || state > int(State::failed)) return false;
    Hunt h;
    h.state = State(state);
    h.callerKey = root.value("callerKey", std::string());
    h.caller = root.value("caller", std::string());
    h.calledAt = root.value("called", 0.0);
    h.chargedAt = root.value("charged", 0.0);
    h.endedAt = root.value("ended", 0.0);
    h.gatheredTold = root.value("told", false);
    if (root.contains("day") && root["day"].is_number_integer()) h.day = root["day"].get<int64_t>();
    if (root.contains("at") && root["at"].is_array() && root["at"].size() == 2) h.at = Vec2(root["at"][0].get<double>(), root["at"][1].get<double>());
    const auto r = regionNamed(root.value("region", std::string()));
    if (r) h.region = *r;
    else if (h.state == State::gathering || h.state == State::assault) h.state = State::failed;
    if (root.contains("party") && root["party"].is_array())
        for (const json& j : root["party"]) {
            if (!j.is_object()) continue;
            Hunt::Member m;
            m.key = j.value("key", std::string());
            if (j.contains("slot") && j["slot"].is_array() && j["slot"].size() == 2) m.slot = Vec2(j["slot"][0].get<double>(), j["slot"][1].get<double>());
            m.there = j.value("there", false);
            m.reached = j.value("reached", -1.0);
            m.fell = j.value("fell", false);
            m.downAtCall = j.value("downAtCall", 0.0);
            h.party.push_back(m);
        }
    hunt_ = std::move(h);
    return true;
}

// MARK: - Dev and shots

std::string WorldMap::devHunt(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: hunt: no world, or no pilot";
    auto& p = pilots_.begin()->second;
    if (what == "call") {
        // As the pilot's call at the Citadel, wherever it stands.
        // The Citadel's place is the call's only rule a dev line skips.
        if (auto why = huntRefusal(p.key)) return "dev: hunt: refused: " + *why;
        huntStart(p.key);
        return "dev: hunt: called by " + hunt_.caller + ", " + std::to_string(hunt_.party.size()) + " in the party";
    }
    if (what == "gathered") {
        if (!huntOn()) return "dev: hunt: none on";
        hunt_.calledAt = std::min(hunt_.calledAt, clock_ - WorldHunt::gather);
        huntStep();
        return "dev: hunt: the gathering's time is over";
    }
    if (what == "fail") {
        if (!huntOn()) return "dev: hunt: none on";
        huntEnd(false, false, "", 0);
        return "dev: hunt: failed";
    }
    if (what == "open") {
        hunt_.day = INT64_MIN;
        return "dev: hunt: the day's call is open";
    }
    if (what == "list") {
        std::string out = "dev: hunt: state " + std::to_string(int(hunt_.state)) + ", caller " + hunt_.caller + ", " +
                          std::to_string(hunt_.party.size()) + " in the party";
        for (const Hunt::Member& m : hunt_.party)
            out += "; " + m.key + (m.there ? " there" : "") + (m.fell ? " fell" : "");
        return out;
    }
    return "dev: hunt: `hunt call|gathered|fail|open|list`";
}

} // namespace ac::server
