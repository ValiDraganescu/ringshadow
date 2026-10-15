// The away report (X2, docs/mmorpg-play.md "Coming back"): every pilot off
// line keeps a ledger of what the world did meanwhile, from the server's own
// bookkeeping, never invented: its mercenaries' deliveries, the frontier
// pilots' deeds, raids on the realm's buildings, the canyons, the
// Matriarch, the realm's projects. On its return the ledger becomes the
// report its next Progress carries, once, and starts again empty.

#include "WorldMap.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>

namespace ac::server {

using json = nlohmann::json;

namespace {

/// Rows kept a pilot at most: past it the oldest of the least telling go.
constexpr size_t kLedgerMost = 120;

bool addsUp(net::AwayKind k) { return k == net::AwayKind::merc || k == net::AwayKind::waves; }

} // namespace

void WorldMap::awayNote(PilotState& p, net::AwayRow row) {
    row.at = clock_;
    if (addsUp(row.kind))
        for (net::AwayRow& r : p.ledger)
            if (r.kind == row.kind && r.who == row.who && r.what == row.what && r.where == row.where) {
                r.value += row.value;
                r.at = row.at;
                return;
            }
    p.ledger.push_back(std::move(row));
    if (p.ledger.size() > kLedgerMost) {
        // The oldest frontier deed, else the oldest row.
        auto it = std::find_if(p.ledger.begin(), p.ledger.end(), [](const net::AwayRow& r) { return r.kind == net::AwayKind::frontier; });
        p.ledger.erase(it != p.ledger.end() ? it : p.ledger.begin());
    }
}

void WorldMap::awayNoteAll(const net::AwayRow& row) {
    for (auto& [key, p] : away_) awayNote(p, row);
}

net::AwayReport WorldMap::awayReport(const PilotState& p) const {
    net::AwayReport out;
    // Off line since it left, or since the world was loaded (saved on line).
    const bool left = p.leftAt >= 0;
    const double from = left ? p.leftAt : std::max(0.0, loadedAt_);
    const Wallet realm = left ? p.realmAtLeave : loadedRealm_;
    out.ran = std::max(0.0, clock_ - from);
    out.back = clock_;
    out.realmOre = realm_.ore - realm.ore;
    out.realmHydrogen = realm_.hydrogen - realm.hydrogen;
    out.rows = p.ledger;
    return out;
}

std::optional<net::AwayReport> WorldMap::takeAway(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !it->second.awayReport) return std::nullopt;
    std::optional<net::AwayReport> out = std::move(it->second.awayReport);
    it->second.awayReport = std::nullopt;
    return out;
}

const std::vector<net::AwayRow>* WorldMap::ledgerOf(const std::string& key) const {
    const auto it = away_.find(key);
    return it == away_.end() ? nullptr : &it->second.ledger;
}

std::map<std::string, std::array<std::string, 2>> WorldMap::awaySummaries() const {
    std::map<std::string, std::array<std::string, 2>> out;
    for (const auto& [key, p] : away_)
        out[key] = net::awaySummary(awayReport(p));
    return out;
}

std::string WorldMap::encodeAway(const PilotState& p) const {
    // On line (or off since the load, nothing yet): none; a load counts it
    // off line from then.
    if (p.leftAt < 0 && p.ledger.empty()) return "";
    const bool on = p.leftAt < 0;
    const Wallet realm = on ? loadedRealm_ : p.realmAtLeave;
    json rows = json::array();
    for (const net::AwayRow& r : p.ledger)
        rows.push_back(json{{"at", r.at}, {"kind", int(r.kind)}, {"who", r.who}, {"what", r.what}, {"where", r.where}, {"value", r.value}});
    return json{{"left", on ? std::max(0.0, loadedAt_) : p.leftAt}, {"realmOre", realm.ore}, {"realmMh", realm.hydrogen}, {"rows", rows}}.dump();
}

bool WorldMap::decodeAway(const std::string& text, PilotState& p) const {
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return false;
    p.leftAt = root.value("left", -1.0);
    p.realmAtLeave = Wallet{root.value("realmOre", int64_t(0)), root.value("realmMh", int64_t(0))};
    p.ledger.clear();
    if (root.contains("rows") && root["rows"].is_array())
        for (const json& j : root["rows"]) {
            if (!j.is_object()) continue;
            const int kind = j.value("kind", -1);
            if (kind < 0 || kind > int(net::AwayKind::projectEnd)) continue;
            net::AwayRow r;
            r.at = j.value("at", 0.0);
            r.kind = net::AwayKind(kind);
            r.who = j.value("who", std::string());
            r.what = j.value("what", std::string());
            r.where = j.value("where", std::string());
            r.value = j.value("value", int64_t(0));
            p.ledger.push_back(std::move(r));
        }
    return true;
}

} // namespace ac::server
