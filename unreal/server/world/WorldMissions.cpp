// Citadel missions (WorldMap.h `WorldMissions`, K2; docs/missions.md): a
// board of offers per pilot at the realm's Citadel, two a tier (easy, hard,
// perilous), drawn by the server's dice per pilot and world day; the pilot
// signs up for up to three at once and follows them in its Journal. Each
// mission comes from a template: its title, the briefing its narration
// reads (no rolled numbers in it: the narration is recorded once per
// template), its instructions with the numbers and names, its place on the
// map. Counted from the work the server already sees (unloads, a ring
// stood in, the sweep, a site opened, bugs killed), paid in XP, ore and MH,
// and at hard and perilous a blueprint copy or supplies.

#include "WorldMap.h"

#include "Ringshadow.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <cmath>

namespace ac::server {

using Mission = net::CitadelMission;
using Kind = net::CitadelMission::Kind;
using Tier = net::CitadelMission::Tier;
using State = net::CitadelMission::State;
using json = nlohmann::json;

namespace {

double scale(int64_t level) { return 1 + 0.02 * double(std::max<int64_t>(1, level) - 1); }

int64_t roundTo(double x, int64_t step) { return std::max<int64_t>(step, int64_t(std::llround(x / double(step))) * step); }

uint64_t mix(uint64_t x) {
    // splitmix64's finaliser.
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

uint64_t hashText(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : s) h = (h ^ uint8_t(c)) * 0x100000001b3ULL;
    return h;
}

std::string fill(std::string text, int64_t n, const std::string& where, const std::string& what) {
    auto put = [&](const std::string& key, const std::string& value) {
        for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size()))
            text.replace(at, key.size(), value);
    };
    put("{n}", std::to_string(n));
    put("{where}", where);
    put("{what}", what);
    return text;
}

} // namespace

// MARK: - The templates

/// A mission's template (docs/missions.md, "Templates"): `key` names its
/// narration (`Sounds/missions/<key>.wav`); the briefing is what the
/// narrator reads, word for word; `{n}`, `{where}` and `{what}` fill the
/// instructions only.
struct MissionTemplate {
    const char* key;
    Kind kind;
    Tier tier;
    /// haul: MH, not ore.
    bool hydrogen;
    const char* title;
    const char* briefing;
    const char* instructions;
};

const std::vector<MissionTemplate>& missionTemplates() {
    static const std::vector<MissionTemplate> all{
        // The Citadel's mission officer: adventurous.
        {"haul-ore-easy", Kind::haul, Tier::easy, false, "Fill the Stacks",
         "Mission officer here. The realm's furnaces are running cold, and a cold furnace makes for a grumpy Citadel. "
         "Get your drill in the dirt, fill your bins with Stardust ore, and bring it home. Simple work. Honest work. Off you go.",
         "Unload {n} ore at the realm's buildings (the Citadel, a Hab Dome)."},
        {"haul-ore-hard", Kind::haul, Tier::hard, false, "The Long Haul",
         "Mission officer here. The realm is building big, and big means hungry. We need a mountain of ore: more than one "
         "trip, more than ten. Pace yourself, keep your treads clean, and keep those bins coming. The Citadel remembers who fed it.",
         "Unload {n} ore at the realm's buildings (the Citadel, a Hab Dome)."},
        {"haul-mh-easy", Kind::haul, Tier::easy, true, "Silver in the Tanks",
         "Mission officer here. The pads are thirsty and the night freighters won't wait. Fill up at a Derrick and bring "
         "the metallic hydrogen home. Gently, pilot. That silver is worth more than the both of us.",
         "Unload {n} MH at the realm's buildings."},
        {"haul-mh-hard", Kind::haul, Tier::hard, true, "Quicksilver Convoy",
         "Mission officer here. Every tank in the realm is running low, and the long flights won't fly on hope. Work the "
         "Derrick hard, haul the silver home trip after trip, and don't you dare spill a drop.",
         "Unload {n} MH at the realm's buildings."},
        {"survey-easy", Kind::survey, Tier::easy, false, "Mark the Ground",
         "Mission officer here. Geology wants fresh readings, and you have the best seat in the realm. Drive out to the "
         "marker, sit tight, and let the scanner sing. Open ground, clear sky. Enjoy the view on the way.",
         "Drive to the marker in {where} and stay inside its ring for {n} s while the scanner runs."},
        {"survey-hard", Kind::survey, Tier::hard, false, "Beyond the Ridge",
         "Mission officer here. There is ground past our borders nobody has read in years. Cross into the next region, "
         "find the marker, and hold your scanner steady. New ground, new ore, new stories. Bring them all back.",
         "Drive to the marker in {where} and stay inside its ring for {n} s while the scanner runs."},
        {"salvage-easy", Kind::salvage, Tier::easy, false, "Finders, Keepers",
         "Mission officer here. Something is buried out there with plans inside, and plans are worth more than ore. Find "
         "the site, crack it open, and it's yours. The realm thanks you, and so does your build menu.",
         "Open the {what} in {where}: hold the act key at it."},
        {"salvage-hard", Kind::salvage, Tier::hard, false, "The Wreck Beyond",
         "Mission officer here. A ship went down past our borders, and its holds were never opened. Cross over, find it, "
         "and pry it open. Whatever is inside, you carry home. Fortune favours the curious, pilot.",
         "Open the {what} in {where}: hold the act key at it."},
        // The Citadel's warden: grave, fearless.
        {"hold-easy", Kind::hold, Tier::easy, false, "Stand in the Shadow",
         "This is the Warden. When the ring's shadow crosses the land, the ground goes cold and the dark comes alive. Go "
         "to the marker. Stand your ground while the band passes over. Do not flinch. The realm holds because pilots hold.",
         "Stay inside the ring in {where} for {n} s while the ringshadow band covers it."},
        {"hold-hard", Kind::hold, Tier::hard, false, "Hold the Line",
         "This is the Warden. The shadow sweeps the border lands today, and something always comes with it. Take the "
         "marker beyond our walls and hold it until the band has passed. Steady breath. Steady hands. We stand.",
         "Stay inside the ring in {where} for {n} s while the ringshadow band covers it."},
        {"hunt-easy", Kind::hunt, Tier::easy, false, "Clear the Vermin",
         "This is the Warden. The Brood is chewing at the edges of the realm again. Small, fast, and hungry. Go out and "
         "put some of them down. Strike clean, keep moving, and remember: you outlast them.",
         "Kill {n} of the Brood in {where}."},
        {"hunt-hard", Kind::hunt, Tier::hard, false, "Thin the Swarm",
         "This is the Warden. The Brood is gathering beyond our borders, more of them every night. Cross over and break "
         "them before they break us. There is no glory in it. Only the work. Do it well.",
         "Kill {n} of the Brood in {where}."},
        // KSTR's night dispatcher on a bad line: afraid.
        {"survey-perilous", Kind::survey, Tier::perilous, false, "Readings from the Dark Country",
         "KSTR dispatch to pilot, do you copy? Command wants a scan from the far corner. Out there. Brood country. The "
         "last crew that went... look, just get in, run the scanner, and get out. Don't stop. Don't look at the lights. "
         "Please come back.",
         "Drive to the marker in {where} and stay inside its ring for {n} s while the scanner runs. Bugs nest there."},
        {"salvage-perilous", Kind::salvage, Tier::perilous, false, "Dead Pilot's Cargo",
         "Dispatch to pilot. There's a site out in bug country, sealed tight. Somebody died guarding it, and whatever "
         "killed them is still out there. Nesting. If you open it, open it fast. And keep your engine running.",
         "Open the {what} in {where}: hold the act key at it. Bugs nest there."},
        {"hold-perilous", Kind::hold, Tier::perilous, false, "Under the Dark Band",
         "Dispatch... pilot, they want someone to hold a point in the far country while the shadow passes. In the dark. "
         "With them out there. I'll keep the channel open the whole time. You just stay in the ring. Don't run. Running "
         "draws them.",
         "Stay inside the ring in {where} for {n} s while the ringshadow band covers it. Bugs nest there."},
        {"hunt-perilous", Kind::hunt, Tier::perilous, false, "Into the Nest",
         "Dispatch to pilot. Oh, no. They want a cull. In the far country, where the big ones are, where the Matriarch "
         "sings. Pilot, if it goes bad, you turn around. You hear me? Nobody's ore is worth your hide out there.",
         "Kill {n} of the Brood in {where}. The Matriarch's brood is thick there."},
    };
    return all;
}

// MARK: - The numbers

double WorldMissions::minutes(Tier tier) {
    switch (tier) {
    case Tier::easy: return 8;
    case Tier::hard: return 20;
    case Tier::perilous: return 40;
    }
    return 8;
}

double WorldMissions::xp(Tier tier, int64_t level) { return double(roundTo(WorldContracts::perMinute(level) * minutes(tier), 5)); }

int64_t WorldMissions::ore(Tier tier, int64_t level) {
    static const double base[] = {150, 450, 1000};
    return roundTo(base[size_t(tier)] * scale(level), 50);
}

int64_t WorldMissions::hydrogen(Tier tier, int64_t level) {
    static const double base[] = {10, 30, 60}, per[] = {2, 4, 8};
    return roundTo(base[size_t(tier)] + per[size_t(tier)] * double(std::max<int64_t>(1, level)), 5);
}

int64_t WorldMissions::haulOre(Tier tier, int64_t level) { return roundTo((tier == Tier::easy ? 200 : 800) * scale(level), 50); }

int64_t WorldMissions::haulHydrogen(Tier tier, int64_t level) {
    return roundTo((tier == Tier::easy ? 40 : 150) * scale(level), 10);
}

int64_t WorldMissions::scan(Tier tier) { return tier == Tier::easy ? 10 : tier == Tier::hard ? 15 : 20; }

int64_t WorldMissions::hold(Tier tier) { return tier == Tier::easy ? 15 : tier == Tier::hard ? 20 : 25; }

int64_t WorldMissions::kills(Tier tier) { return tier == Tier::easy ? 3 : tier == Tier::hard ? 6 : 12; }

Tier WorldMissions::tierAt(int64_t steps) { return steps <= 0 ? Tier::easy : steps == 1 ? Tier::hard : Tier::perilous; }

// MARK: - Drawing

double WorldMap::sweepCover(size_t r, Vec2 at) const {
    const Simulation& sim = regions_[r]->simulation();
    return sim.map ? Ringshadow::cover(clock_, at, sim.map->bounds, params_.dayLength) : 0;
}

void WorldMap::drawBoard(PilotId pilot, PilotState& p, int64_t day) {
    const uint64_t seed = params_.missionSeed ? params_.missionSeed
                                              : uint64_t(WorldClock::weatherSeed(world_->file.id)) ^ 0x3155104b0a2dULL;
    const std::string who = p.key.empty() ? "pilot-" + std::to_string(pilot) : p.key;
    std::mt19937_64 dice(mix(seed ^ mix(hashText(who)) ^ mix(uint64_t(day) * 0x9e3779b97f4a7c15ULL)));
    p.board.clear();
    for (const Tier tier : {Tier::easy, Tier::hard, Tier::perilous}) {
        std::vector<Kind> taken;
        for (size_t i = 0; i < WorldMissions::offers; i++) {
            auto m = drawMission(p, dice, tier, taken);
            if (!m) break;
            m->id = p.nextMission++;
            taken.push_back(m->kind);
            p.board.push_back(std::move(*m));
        }
    }
    // A done mission leaves the Journal with the old board.
    std::erase_if(p.journal, [](const Mission& m) { return m.state == State::done; });
    p.boardDay = day;
}

std::optional<Mission> WorldMap::drawMission(const PilotState& p, std::mt19937_64& dice, Tier tier, const std::vector<Kind>& taken) {
    const int64_t level = p.progress.level();
    auto roll = [&](size_t n) { return n ? size_t(std::uniform_int_distribution<size_t>(0, n - 1)(dice)) : size_t(0); };
    const WorldRegion& home = world_->regions[start_];
    // The tier's regions by grid steps from the start (a world without
    // them: the nearest tier below that has some).
    std::vector<size_t> pool;
    for (int t = int(tier); t >= 0 && pool.empty(); t--)
        for (size_t r = 0; r < world_->regions.size(); r++) {
            const WorldRegion& g = world_->regions[r];
            const int64_t steps = std::abs(g.column - home.column) + std::abs(g.row - home.row);
            if (WorldMissions::tierAt(steps) == Tier(t)) pool.push_back(r);
        }
    // Sites it hasn't opened (and hasn't taken already) that it can reach
    // there (a vault only Cliff hop reaches: with Cliff hop).
    std::vector<std::pair<size_t, size_t>> sites;
    if (siteReach_.size() != regions_.size()) siteReach_.resize(regions_.size());
    const bool hop = p.progress.rank(Skill::cliffHop) > 0;
    for (const size_t r : pool) {
        const region::Built& built = world_->regions[r].built;
        if (!built.sites.empty() && !siteReach_[r]) siteReach_[r] = region::siteReach(built);
        for (size_t i = 0; i < built.sites.size(); i++) {
            const region::SiteReach how = (*siteReach_[r])[i];
            const bool can = how == region::SiteReach::walk || how == region::SiteReach::ramp ||
                             (hop && how == region::SiteReach::cliffHop);
            const std::string id = siteId(r, i);
            const bool mine = std::any_of(p.journal.begin(), p.journal.end(), [&](const Mission& m) { return m.target == id; }) ||
                              std::any_of(p.board.begin(), p.board.end(), [&](const Mission& m) { return m.target == id; });
            if (can && !mine && !p.progress.sites.count(id)) sites.push_back({r, i});
        }
    }
    const bool haul = tier != Tier::perilous && (realmFinished(StructureKind::citadel) || realmFinished(StructureKind::habDome));

    struct Option {
        Kind kind;
        int weight;
    };
    std::vector<Option> options;
    if (haul) options.push_back({Kind::haul, 2});
    if (!pool.empty()) options.push_back({Kind::survey, 3});
    if (!sites.empty()) options.push_back({Kind::salvage, 3});
    if (!pool.empty()) options.push_back({Kind::hold, 2});
    if (params_.huntMissions && !pool.empty()) options.push_back({Kind::hunt, 3});
    std::vector<Option> fresh;
    for (const Option& o : options)
        if (std::find(taken.begin(), taken.end(), o.kind) == taken.end()) fresh.push_back(o);
    const std::vector<Option>& from = fresh.empty() ? options : fresh;
    if (from.empty()) return std::nullopt;
    int total = 0;
    for (const Option& o : from) total += o.weight;
    int pick = int(roll(size_t(total)));
    Kind kind = from.front().kind;
    for (const Option& o : from) {
        if (pick < o.weight) {
            kind = o.kind;
            break;
        }
        pick -= o.weight;
    }

    Mission m;
    m.kind = kind;
    m.tier = tier;
    m.xp = WorldMissions::xp(tier, level);
    m.ore = WorldMissions::ore(tier, level);
    m.hydrogen = WorldMissions::hydrogen(tier, level);
    // Hard: one blueprint copy or two supplies; perilous: two copies or
    // three supplies. A blueprint the pilot could build within ten levels.
    if (tier != Tier::easy) {
        std::vector<StructureKind> kinds;
        for (const StructureKind k : Progression::blueprintKinds())
            if (k != StructureKind::citadel && Progression::blueprintFloor(k) > 0 && Progression::blueprintFloor(k) <= level + 10)
                kinds.push_back(k);
        if (!kinds.empty() && roll(2) == 0) {
            m.blueprint = kinds[roll(kinds.size())];
            m.count = tier == Tier::hard ? 1 : 2;
        } else {
            const auto& items = Shop::items();
            m.item = items[roll(items.size())].item;
            m.count = tier == Tier::hard ? 2 : 3;
        }
    }
    std::string where, what;
    bool hydrogen = false;
    switch (kind) {
    case Kind::haul: {
        hydrogen = realmFinished(StructureKind::derrick) && roll(3) == 0;
        m.target = hydrogen ? "hydrogen" : "ore";
        m.need = hydrogen ? WorldMissions::haulHydrogen(tier, level) : WorldMissions::haulOre(tier, level);
        // The marker: the realm's Citadel, where it unloads.
        if (const auto c = realmCitadel()) {
            m.region = world_->regions[c->first].file.id;
            for (const Structure& s : regions_[c->first]->simulation().state.structures)
                if (s.id == c->second) m.at = s.position;
        }
        break;
    }
    case Kind::survey:
    case Kind::hold: {
        const size_t r = pool[roll(pool.size())];
        const WorldRegion& g = world_->regions[r];
        const Simulation& sim = regions_[r]->simulation();
        // A survey reads a deposit, a well or a pad; a hold stands on a
        // deposit (the band's ore). In the start region, one at least 30
        // cells from the start pad.
        std::vector<Vec2> spots;
        for (const OreDeposit& d : sim.state.patches) spots.push_back(d.position);
        if (kind == Kind::survey) {
            if (sim.state.wells)
                for (const Well& w : *sim.state.wells) spots.push_back(w.position);
            for (const region::Place& pad : g.built.pads) spots.push_back(pad.at);
        }
        if (r == start_) {
            const Vec2 pad = g.built.pads[world_->startPad].at;
            std::vector<Vec2> far;
            for (const Vec2 s : spots)
                if (distance(s, pad) >= 30) far.push_back(s);
            if (!far.empty()) spots = far;
        }
        m.target = m.region = g.file.id;
        m.at = spots.empty() ? Vec2(0, 0) : spots[roll(spots.size())];
        m.radius = kind == Kind::survey ? WorldMissions::surveyRadius : WorldMissions::holdRadius;
        m.need = kind == Kind::survey ? WorldMissions::scan(tier) : WorldMissions::hold(tier);
        where = g.file.name;
        break;
    }
    case Kind::salvage: {
        const auto [r, i] = sites[roll(sites.size())];
        const WorldRegion& g = world_->regions[r];
        m.target = siteId(r, i);
        m.region = g.file.id;
        m.at = g.built.sites[i].at;
        m.radius = WorldSites::reach;
        m.need = 1;
        where = g.file.name;
        what = g.built.sites[i].name;
        break;
    }
    case Kind::hunt: {
        const size_t r = pool[roll(pool.size())];
        const WorldRegion& g = world_->regions[r];
        m.target = m.region = g.file.id;
        m.need = WorldMissions::kills(tier);
        where = g.file.name;
        break;
    }
    }
    // Its template: of its kind and tier (a haul: of ore or MH).
    for (const MissionTemplate& t : missionTemplates()) {
        if (t.kind != kind || t.tier != tier || (kind == Kind::haul && t.hydrogen != hydrogen)) continue;
        m.key = t.key;
        m.title = t.title;
        m.briefing = t.briefing;
        m.instructions = fill(t.instructions, m.need, where, what);
        break;
    }
    return m;
}

// MARK: - Each step

void WorldMap::missions() {
    if (!params_.missions || !world_) return;
    const int64_t day = contractDay(clock_);
    const double dt = regions_[0]->description().step;
    for (auto& [pilot, p] : pilots_) {
        if (!p.boardDay || *p.boardDay != day) drawBoard(pilot, p, day);
        // A site opened (anywhere, any way) finishes its salvage.
        for (Mission& m : p.journal)
            if (m.state == State::taken && m.kind == Kind::salvage && p.progress.sites.count(m.target)) {
                m.have = m.need;
                payMission(pilot, p, m);
            }
        if (p.flight || p.down) continue;
        Region& here = *regions_[p.region];
        const auto unit = here.driving(pilot);
        const auto u = unit ? here.simulation().state.unit(*unit) : std::nullopt;
        if (!u) continue;
        const std::string& id = world_->regions[p.region].file.id;
        std::vector<std::pair<Kind, std::string>> work;
        for (Mission& m : p.journal) {
            if (m.state != State::taken || (m.kind != Kind::survey && m.kind != Kind::hold) || m.region != id) continue;
            if (distance(u->position, m.at) > m.radius) continue;
            if (m.kind == Kind::hold && sweepCover(p.region, u->position) < Ringshadow::bonusCover) continue;
            double& s = p.missionSeconds[m.id];
            s += dt;
            if (s < 1) continue;
            s -= 1;
            m.have = std::min(m.need, m.have + 1);
            if (m.have >= m.need) {
                p.missionSeconds.erase(m.id);
                payMission(pilot, p, m);
            }
        }
    }
}

void WorldMap::missionWork(PilotId pilot, PilotState& p, Kind kind, const std::string& target, int64_t amount) {
    if (amount <= 0) return;
    for (Mission& m : p.journal) {
        if (m.state != State::taken || m.kind != kind || m.target != target) continue;
        m.have = std::min(m.need, m.have + amount);
        if (m.have >= m.need) payMission(pilot, p, m);
    }
}

void WorldMap::payMission(PilotId pilot, PilotState& p, Mission& m) {
    // Done: paid now; it leaves the Journal when the board is next drawn.
    m.state = State::done;
    p.news.push_back(net::ProgressMessage::News{net::ProgressMessage::NewsKind::mission, std::to_string(m.id), m.xp, m.ore, ""});
    earn(p, m.xp);
    if (!p.flight) bankOut(p, p.region);
    p.wallet.ore += m.ore;
    p.wallet.hydrogen += m.hydrogen;
    if (!p.flight) bankIn(p, p.region);
    if (m.blueprint) p.progress.giveBlueprint(*m.blueprint, m.count);
    // Supplies up to the stack (`ItemInfo::stack`).
    if (m.item) {
        const int64_t now = p.progress.held(*m.item);
        const int64_t to = std::max(now, std::min(Shop::item(*m.item).stack, now + m.count));
        if (to > 0) p.progress.items[*m.item] = to;
    }
    pushProgress(pilot);
}

void WorldMap::broodKill(PilotId pilot, UnitKind, size_t region) {
    PilotState* p = pilotState(pilot);
    if (!p || !world_ || region >= world_->regions.size()) return;
    missionWork(pilot, *p, Kind::hunt, world_->regions[region].file.id, 1);
}

// MARK: - Asked by the pilot, told to it

std::vector<Mission> WorldMap::board(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() || !world_ ? std::vector<Mission>{} : it->second.board;
}

std::vector<Mission> WorldMap::journal(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() || !world_ ? std::vector<Mission>{} : it->second.journal;
}

void WorldMap::setBoard(PilotId pilot, const std::vector<Mission>& offers) {
    PilotState* p = pilotState(pilot);
    if (!p) return;
    p->board = offers;
    p->boardDay = contractDay(clock_);
    for (const Mission& m : offers) p->nextMission = std::max(p->nextMission, m.id + 1);
}

void WorldMap::setJournal(PilotId pilot, const std::vector<Mission>& missions) {
    PilotState* p = pilotState(pilot);
    if (!p) return;
    p->journal = missions;
    for (const Mission& m : missions) p->nextMission = std::max(p->nextMission, m.id + 1);
}

std::optional<std::string> WorldMap::takeMission(PilotId pilot, uint32_t mission) {
    PilotState* p = pilotState(pilot);
    if (!p || !world_) return "Not in a world";
    const auto it = std::find_if(p->board.begin(), p->board.end(), [&](const Mission& m) { return m.id == mission; });
    if (it == p->board.end()) return std::string("No such mission on the board");
    const auto taken = std::count_if(p->journal.begin(), p->journal.end(), [](const Mission& m) { return m.state == State::taken; });
    if (size_t(taken) >= WorldMissions::journal)
        return "Your Journal holds " + std::to_string(WorldMissions::journal) + " missions: finish or abandon one (L)";
    if (!atRealm(pilot, *p, {StructureKind::citadel})) return std::string("Sign up at the realm's Citadel: stand where you unload");
    Mission m = *it;
    m.state = State::taken;
    m.have = 0;
    p->board.erase(it);
    // Its place goes on the pilot's map: a salvage's site is known now.
    if (m.kind == Kind::salvage) p->progress.known.insert(m.target);
    p->journal.push_back(std::move(m));
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::abandonMission(PilotId pilot, uint32_t mission) {
    PilotState* p = pilotState(pilot);
    if (!p || !world_) return "Not in a world";
    const auto it = std::find_if(p->journal.begin(), p->journal.end(), [&](const Mission& m) { return m.id == mission; });
    if (it == p->journal.end()) return std::string("No such mission in your Journal");
    if (it->state == State::done) return std::string("That mission is done already");
    p->missionSeconds.erase(it->id);
    p->journal.erase(it);
    return std::nullopt;
}

// MARK: - Saved

namespace {

json encodeMission(const Mission& m) {
    json j{{"id", m.id},
           {"kind", int(m.kind)},
           {"tier", int(m.tier)},
           {"state", int(m.state)},
           {"key", m.key},
           {"title", m.title},
           {"briefing", m.briefing},
           {"instructions", m.instructions},
           {"target", m.target},
           {"region", m.region},
           {"x", m.at.x},
           {"z", m.at.y},
           {"radius", m.radius},
           {"need", m.need},
           {"have", m.have},
           {"xp", m.xp},
           {"ore", m.ore},
           {"hydrogen", m.hydrogen},
           {"count", m.count}};
    if (m.blueprint) j["blueprint"] = std::string(rawValue(*m.blueprint));
    if (m.item) j["item"] = std::string(rawValue(*m.item));
    return j;
}

bool decodeMission(const json& j, Mission& m) {
    if (!j.is_object()) return false;
    const int kind = j.value("kind", -1), tier = j.value("tier", -1), state = j.value("state", -1);
    if (kind < 0 || kind > int(Kind::hunt) || tier < 0 || tier > int(Tier::perilous) || state < 0 || state > int(State::done))
        return false;
    m.id = j.value("id", uint32_t(0));
    m.kind = Kind(kind);
    m.tier = Tier(tier);
    m.state = State(state);
    m.key = j.value("key", std::string());
    m.title = j.value("title", std::string());
    m.briefing = j.value("briefing", std::string());
    m.instructions = j.value("instructions", std::string());
    m.target = j.value("target", std::string());
    m.region = j.value("region", std::string());
    m.at = Vec2(j.value("x", 0.0), j.value("z", 0.0));
    m.radius = j.value("radius", 0.0);
    m.need = j.value("need", int64_t(1));
    m.have = j.value("have", int64_t(0));
    m.xp = j.value("xp", 0.0);
    m.ore = j.value("ore", int64_t(0));
    m.hydrogen = j.value("hydrogen", int64_t(0));
    m.count = j.value("count", int64_t(0));
    if (j.contains("blueprint")) {
        const auto k = parse<StructureKind>(j.value("blueprint", std::string()));
        if (!k) return false;
        m.blueprint = *k;
    }
    if (j.contains("item")) {
        const auto i = parse<Item>(j.value("item", std::string()));
        if (!i) return false;
        m.item = *i;
    }
    return true;
}

} // namespace

std::string WorldMap::encodeMissions(const PilotState& p) {
    if (!p.boardDay && p.board.empty() && p.journal.empty()) return "";
    json board = json::array(), journal = json::array();
    for (const Mission& m : p.board) board.push_back(encodeMission(m));
    for (const Mission& m : p.journal) journal.push_back(encodeMission(m));
    json root{{"next", p.nextMission}, {"board", board}, {"journal", journal}};
    if (p.boardDay) root["day"] = *p.boardDay;
    return root.dump();
}

bool WorldMap::decodeMissions(const std::string& text, PilotState& p) {
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return false;
    std::vector<Mission> board, journal;
    for (const auto& [name, out] : {std::pair{"board", &board}, std::pair{"journal", &journal}}) {
        if (!root.contains(name) || !root[name].is_array()) return false;
        for (const json& j : root[name]) {
            Mission m;
            if (!decodeMission(j, m)) return false;
            out->push_back(std::move(m));
        }
    }
    p.board = std::move(board);
    p.journal = std::move(journal);
    p.nextMission = root.value("next", uint32_t(1));
    p.boardDay = root.contains("day") && root["day"].is_number_integer() ? std::optional<int64_t>(root["day"].get<int64_t>())
                                                                         : std::nullopt;
    return true;
}

} // namespace ac::server
