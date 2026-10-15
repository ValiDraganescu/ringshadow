// Realm standings (S4, docs/mmorpg-play.md "Standings"; Standings.h): every
// pilot's work over the realm week (seven world days), humans and frontier
// pilots alike, counted where the server already sees it, never invented:
// ore unloaded (workAfter, frontierUnload; a mercenary's deliveries to its
// hirer, mercDelivered), bugs felled (broodKill, frontierHit), canyon waves
// beaten that it fought (waveBeaten's pay split), its buildings finished
// (realmNews, frontierEvents). At the week's end the top hauler is told to
// every pilot (on line: news; off line: a row in its away ledger) and the
// counts start again.

#include "WorldMap.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ac::server {

using json = nlohmann::json;

namespace {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

} // namespace

void WorldMap::standNote(const std::string& key, int64_t Standing::*stat, int64_t n) {
    if (!world_ || key.empty() || n <= 0) return;
    standings_[key].*stat += n;
}

std::string WorldMap::standingName(const std::string& key) const {
    if (frontierKey(key))
        if (const FrontierPilot* f = frontierByKey(key)) return f->callsign;
    return callsign(key);
}

std::optional<net::StandingsMessage> WorldMap::standingsMessage(PilotId pilot) const {
    const auto me = pilots_.find(pilot);
    if (!world_ || me == pilots_.end()) return std::nullopt;
    const std::string& mine = me->second.key;
    std::vector<std::pair<net::WeekStanding, std::string>> all;
    bool mineIn = false;
    for (const auto& [key, s] : standings_) {
        if (s == Standing{} && key != mine) continue;
        all.push_back({net::WeekStanding{standingName(key), frontierKey(key), s.ore, s.bugs, s.waves, s.built, 0}, key});
        mineIn = mineIn || key == mine;
    }
    // Its own row always, nothing counted yet or not.
    if (!mineIn && !mine.empty()) all.push_back({net::WeekStanding{standingName(mine), false, 0, 0, 0, 0, 0}, mine});
    std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return net::standingBefore(a.first, b.first); });
    net::StandingsMessage m;
    m.weekStart = weekStart_;
    m.weekEnds = weekEnds();
    for (size_t i = 0; i < all.size(); i++) {
        all[i].first.rank = int64_t(i) + 1;
        const bool own = all[i].second == mine;
        if (i >= net::kStandingsMost && !own) continue;
        if (own) m.you = int64_t(m.rows.size());
        m.rows.push_back(all[i].first);
    }
    return m;
}

std::string WorldMap::weekEnd() {
    // The top hauler: the most ore (ties as the table orders them).
    std::optional<net::WeekStanding> top;
    for (const auto& [key, s] : standings_) {
        if (s.ore <= 0) continue;
        const net::WeekStanding row{standingName(key), frontierKey(key), s.ore, s.bugs, s.waves, s.built, 1};
        if (!top || net::standingBefore(row, *top)) top = row;
    }
    const std::string who = top ? top->callsign : std::string();
    const int64_t ore = top ? top->ore : 0;
    for (auto& [id, p] : pilots_) p.news.push_back(News{NewsKind::week, top ? "top" : "none", 0, ore, who});
    awayNoteAll(net::AwayRow{0, net::AwayKind::week, who, "", "", ore});
    standings_.clear();
    return net::weekTopLine(who, ore);
}

void WorldMap::standingsStep() {
    if (!world_ || !(weekLength() > 0) || clock_ < weekEnds()) return;
    // Told once, however many weeks the clock passed (a world down long).
    weekEnd();
    weekStart_ += weekLength() * std::floor((clock_ - weekStart_) / weekLength());
}

std::string WorldMap::weekEndNow() {
    if (!world_) return "";
    const std::string said = weekEnd();
    weekStart_ = clock_;
    return said;
}

std::string WorldMap::devWeek(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_) return "dev: week: not a world";
    if (what == "end") {
        const std::string said = weekEndNow();
        return "dev: week: ended, the next from " + std::to_string(int64_t(weekStart_)) + " s: " + said;
    }
    if (what == "left") {
        double s = 0;
        if (!(in >> s) || s < 0) return "dev: week: `week left S`";
        weekStart_ = clock_ + s - weekLength();
        return "dev: week: ends in " + std::to_string(int64_t(s)) + " s";
    }
    if (what == "list") {
        std::string out = "dev: week: " + std::to_string(standings_.size()) + " pilots, " + net::weekLeft(weekEnds() - clock_);
        for (const auto& [key, s] : standings_)
            out += "; " + standingName(key) + " " + std::to_string(s.ore) + " ore " + std::to_string(s.bugs) + " bugs " +
                   std::to_string(s.waves) + " waves " + std::to_string(s.built) + " built";
        return out;
    }
    return "dev: week: `week end|left S|list`";
}

std::string WorldMap::encodeStandings() const {
    json rows = json::object();
    for (const auto& [key, s] : standings_)
        if (!(s == Standing{})) rows[key] = json::array({s.ore, s.bugs, s.waves, s.built});
    return json{{"start", weekStart_}, {"pilots", rows}}.dump();
}

bool WorldMap::decodeStandings(const std::string& text) {
    standings_.clear();
    // A save from before (schema 23 and under): the week starts at the load.
    if (text.empty()) {
        weekStart_ = clock_;
        return true;
    }
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        weekStart_ = clock_;
        return false;
    }
    weekStart_ = root.value("start", clock_);
    if (root.contains("pilots") && root["pilots"].is_object())
        for (const auto& [key, j] : root["pilots"].items()) {
            if (!j.is_array() || j.size() != 4) continue;
            Standing s;
            s.ore = j[0].is_number_integer() ? j[0].get<int64_t>() : 0;
            s.bugs = j[1].is_number_integer() ? j[1].get<int64_t>() : 0;
            s.waves = j[2].is_number_integer() ? j[2].get<int64_t>() : 0;
            s.built = j[3].is_number_integer() ? j[3].get<int64_t>() : 0;
            standings_[key] = s;
        }
    return true;
}

} // namespace ac::server
