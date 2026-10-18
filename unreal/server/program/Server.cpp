#include "Server.h"

#include "AwayReport.h"
#include "NetProtocol.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sys/stat.h>

namespace ac::server {

namespace {

net::ServerConfig endpointConfig(const ServerOptions& o) {
    net::ServerConfig c;
    c.secret = o.secret;
    c.token = o.token;
    c.connection = o.connection;
    return c;
}

std::string format(const char* f, double a) {
    char b[64];
    std::snprintf(b, sizeof b, f, a);
    return b;
}

WorldMap makeWorld(const ServerOptions& o) {
    return o.world ? WorldMap(*o.world, o.worldParams, o.region) : WorldMap(o.region);
}

} // namespace

Server::Server(net::PacketIo& io, const ServerOptions& options)
    : options_(options), world_(makeWorld(options)), endpoint_(io, endpointConfig(options)) {
    // The save in place before the first tick.
    if (options_.saved) {
        for (const std::string& p : world_.restore(*options_.saved)) log("save: " + p);
        log("save: " + std::to_string(options_.saved->regions.size()) + " regions, " +
            std::to_string(options_.saved->pilots.size()) + " pilots loaded");
        // What each region holds (a world that comes up empty on screen is
        // told apart from one that came up empty here).
        if (world_.isWorld())
            for (size_t r = 0; r < world_.loaded()->regions.size(); r++)
                log("save: " + world_.loaded()->regions[r].file.id + ": " +
                    std::to_string(world_.region(r).simulation().state.structures.size()) + " buildings, " +
                    std::to_string(world_.region(r).simulation().state.units.size()) + " units");
        // The falls kept in the save: known, not told again as new.
        if (world_.isWorld()) {
            for (const WorldMap::Fall& f : world_.starfalls()) starfalls_[f.id] = f.patch.has_value();
            log("save: " + std::to_string(starfalls_.size()) + " starfalls kept (their craters and deposits)");
        }
        // S2: the time it was down, caught up (capped).
        if (const double savedAt = options_.saved->savedAt; savedAt > 0 && world_.isWorld()) {
            const double down = wallNow() - savedAt + std::max(0.0, options_.devDown);
            const auto t0 = std::chrono::steady_clock::now();
            caughtUp_ = world_.catchUp(down);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            if (caughtUp_ > 0)
                log("catch-up: down " + format("%.0f", down) + " s, the world moved on " + format("%.0f", caughtUp_) + " s (cap " +
                    format("%.0f", world_.params().catchUpCap) + " s), clock " + format("%.1f", world_.clock()) + " s, in " +
                    format("%.1f", ms) + " ms");
        }
        options_.saved = std::nullopt;
    }
}

double Server::wallNow() const {
    if (options_.wallClock) return options_.wallClock();
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void Server::idle(double now) {
    const auto t0 = std::chrono::steady_clock::now();
    if (!idling_) {
        idling_ = true;
        idleFrom_ = now;
        log("idle: no pilot on line, the world's clock runs on at " + format("%.0f", options_.idleEvery > 0 ? 1 / options_.idleEvery : 0) +
            " Hz, the regions wait");
    }
    endpoint_.update(now);
    for (net::Event e; endpoint_.poll(e);) handle(e, now);
    if (now > idleFrom_) {
        world_.idle(now - idleFrom_);
        idleFrom_ = now;
        logStarfalls();
        writeAway();
    }
    endpoint_.flush(now);
    if (savedAt_ < 0) savedAt_ = now;
    if (options_.store && now - savedAt_ >= options_.idleSaveEvery) {
        savedAt_ = now;
        save();
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    idleSum_ += ms;
    idleMax_ = std::max(idleMax_, ms);
    idles_ += 1;
    if (idleStatsFrom_ < 0) idleStatsFrom_ = now;
    if (options_.idleStatsEvery > 0 && now - idleStatsFrom_ >= options_.idleStatsEvery) idleStatsLine(now);
}

void Server::idleStatsLine(double now) {
    log("idle stats: " + std::to_string(idles_) + " calls, mean " + format("%.2f", idles_ ? idleSum_ / double(idles_) : 0) +
        " ms max " + format("%.2f", idleMax_) + " ms, clock " + format("%.0f", world_.clock()) + " s, ledger " +
        std::to_string(world_.ledgerRows()) + " rows, deposits " + std::to_string(world_.depositsOre()) + " ore, radio " +
        std::to_string(world_.radioLog().size()) + " lines");
    idleStatsFrom_ = now;
    idleSum_ = 0;
    idleMax_ = 0;
    idles_ = 0;
}

bool Server::save() {
    if (!options_.store) return false;
    std::string why;
    WorldSave s = world_.save();
    s.savedAt = wallNow();
    const auto written = options_.store->write(s, &why);
    if (!written) {
        log("save failed: " + why);
        return false;
    }
    lastSave_ = *written;
    writeAway();
    return true;
}

bool Server::writeAway() {
    if (options_.awayFile.empty()) return false;
    const net::AwaySummaries all = world_.awaySummaries();
    const std::string text = net::encodeAwayFile(all);
    if (awayWritten_ && *awayWritten_ == text) return true;
    if (!net::writeAwayFile(options_.awayFile, all)) {
        log("away file failed: " + options_.awayFile);
        return false;
    }
    awayWritten_ = text;
    return true;
}

void Server::sendReliable(net::ConnectionId c, const std::vector<uint8_t>& bytes) {
    const net::Channel ch = bytes.size() <= net::kMaxReliableBytes ? net::Channel::reliable : net::Channel::bulk;
    endpoint_.send(c, ch, bytes.data(), bytes.size());
}

void Server::sendWorld(net::ConnectionId c, PilotId pilot) {
    if (const auto m = world_.worldMessage(pilot)) sendReliable(c, net::encode(*m));
}

void Server::sendMoves() {
    for (const Moved& m : world_.takeMoves()) {
        for (const auto& [c, client] : clients_) {
            if (client.pilot != m.pilot) continue;
            if (m.change) {
                sendReliable(c, net::encode(*m.change));
                log("pilot " + std::to_string(m.pilot) + " to " + m.change->regionId);
            }
            sendWorld(c, m.pilot);
        }
    }
}

void Server::log(const std::string& line) const {
    if (options_.log) options_.log(line);
}

std::vector<std::string> Server::devControl(const std::string& text) {
    std::vector<std::string> said;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        char what[16] = {};
        double v = 0;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        // W5: `as N LINE`, the line for the N-th pilot on line (2: the
        // second human), not the first.
        if (int n = 0, used = 0; line.rfind("as ", 0) == 0 && std::sscanf(line.c_str(), "as %d %n", &n, &used) == 1 && used > 0) {
            world_.devAs = world_.devPilotId(n);
            if (!world_.devAs) {
                said.push_back("dev: control '" + line + "' not done (no pilot " + std::to_string(n) + " on line)");
                continue;
            }
            // (Logged once, here.)
            const auto log = options_.log;
            options_.log = nullptr;
            const std::vector<std::string> one = devControl(line.substr(size_t(used)));
            options_.log = log;
            world_.devAs = std::nullopt;
            for (const std::string& x : one) said.push_back("as " + std::to_string(n) + ": " + x);
            continue;
        }
        // The Brood's scenes (WorldMap::devBrood): `brood linger|attack|dive|lodeback|raid|zone|clear`.
        if (line.rfind("brood", 0) == 0) {
            said.push_back(world_.devBrood(line));
            continue;
        }
        // The defences' scenes (WorldMap::devDefence): `defence wallet|blueprints|citadel|gatling|aa`.
        if (line.rfind("defence", 0) == 0) {
            said.push_back(world_.devDefence(line));
            continue;
        }
        // The Dropship's scenes (WorldMap::devFerry): `ferry pad [here|REGION]|seen|go REGION|mh N`.
        if (line.rfind("ferry", 0) == 0) {
            said.push_back(world_.devFerry(line));
            continue;
        }
        // KSTR's next goal (WorldMap::devGoal): `goal`, `goal pass KEY`, `goal level N`.
        if (line.rfind("goal", 0) == 0) {
            said.push_back(world_.devGoal(line));
            continue;
        }
        // The canyon game (WorldMap::devCanyon): `canyon send N|fail|beat|hold|reset|break S|turrets G [A [FIRST]]|place KIND X Y…`.
        if (line.rfind("canyon", 0) == 0) {
            said.push_back(world_.devCanyon(line));
            continue;
        }
        // The hunt (WorldMap::devHunt): `hunt call|gathered|fail|open|list`.
        if (line.rfind("hunt", 0) == 0) {
            said.push_back(world_.devHunt(line));
            continue;
        }
        // E1's scenes (WorldMap::devStarfall): `starfall victims`.
        if (line.rfind("starfall", 0) == 0) {
            said.push_back(world_.devStarfall(line));
            continue;
        }
        // The realm week (WorldMap::devWeek): `week end|left S|list`.
        if (line.rfind("week", 0) == 0) {
            said.push_back(world_.devWeek(line));
            continue;
        }
        // The Citadel board (WorldMap::devMissions): `missions here`.
        if (line.rfind("missions", 0) == 0) {
            said.push_back(world_.devMissions(line));
            continue;
        }
        // Frontier pilots (WorldMap::devFrontier): `frontier here HABIT [SIDE]|build KIND|radio KIND|level N|list`.
        if (line.rfind("frontier", 0) == 0) {
            said.push_back(world_.devFrontier(line));
            continue;
        }
        // The Matriarch's scenes (WorldMap::devMatriarch): `matriarch here|near|roar|calm|slam|hurt|down|back|siege`.
        if (line.rfind("matriarch", 0) == 0) {
            said.push_back(world_.devMatriarch(line));
            continue;
        }
        // Daylight sight (S3, Region::setDaylight): `daylight off` is the
        // sight before it, to measure against; `daylight on` again.
        if (line.rfind("daylight", 0) == 0) {
            const bool on = line.find("off") == std::string::npos;
            for (size_t r = 0; r < world_.regionCount(); r++) world_.region(r).setDaylight(on);
            said.push_back(on ? "dev: daylight: on" : "dev: daylight: off (the sight before S3)");
            continue;
        }
        const double was = world_.clock();
        if (!world_.isWorld() || std::sscanf(line.c_str(), "%15s %lf", what, &v) != 2) {
            said.push_back("dev: control '" + line + "' not done (no world, or not `clock|hour|ahead|catchup|idle N`)");
            continue;
        }
        const std::string w = what;
        if (w == "clock" && v >= 0) world_.setClock(v);
        else if (w == "hour" && v >= 0 && v <= 24) world_.setClock(WorldClock::timeOf(v, was, world_.params().dayLength));
        else if (w == "ahead" && v >= 0) world_.setClock(was + v);
        else if (w == "catchup" && v >= 0) {
            // X2's shots: the world caught up on `v` s, as a restart after
            // that long down (the mercenaries' work, the projects' ends…).
            const double moved = world_.catchUp(v);
            writeAway();
            char out[160];
            std::snprintf(out, sizeof out, "dev: caught up %.0f s: clock %.1f s -> %.1f s", moved, was, world_.clock());
            said.push_back(out);
            continue;
        }
        else if (w == "idle" && v >= 0) {
            // S4's shots: the world idles `v` s as with no pilot on line
            // (the regions held; the frontier pilots' work and the
            // mercenaries' booked), a second at a time.
            for (double left = v; left > 1e-9; left -= 1.0) world_.idle(std::min(1.0, left));
            char out[160];
            std::snprintf(out, sizeof out, "dev: idled %.0f s: clock %.1f s -> %.1f s", v, was, world_.clock());
            said.push_back(out);
            continue;
        }
        else {
            said.push_back("dev: control '" + line + "' not done (not `clock|hour|ahead|catchup|idle N`)");
            continue;
        }
        char out[160];
        std::snprintf(out, sizeof out, "dev: clock %.1f s -> %.1f s (hour %.2f)", was, world_.clock(),
                      world_.sky() ? world_.sky()->hour() : 0.0);
        said.push_back(out);
    }
    for (const std::string& s : said) log(s);
    return said;
}

void Server::logStarfalls() {
    if (!world_.isWorld()) return;
    std::map<uint32_t, bool> now;
    for (const WorldMap::Fall& f : world_.starfalls()) {
        now[f.id] = f.patch.has_value();
        const auto was = starfalls_.find(f.id);
        if (was != starfalls_.end() && was->second == now[f.id]) continue;
        const std::string region = world_.loaded()->regions[f.region].file.id;
        const Vec2 at = world_.loaded()->regions[f.region].built.regionAt(f.at);
        char line[200];
        if (!f.patch)
            std::snprintf(line, sizeof line, "starfall %u: told, %s at (%.1f, %.1f), impact in %.0f s from bearing %.0f", f.id,
                          region.c_str(), at.x, at.y, f.impact - world_.clock(), f.from);
        else
            std::snprintf(line, sizeof line, "starfall %u: landed in %s at (%.1f, %.1f), deposit %lld with %lld ore", f.id, region.c_str(),
                          at.x, at.y, (long long)*f.patch, (long long)f.ore);
        log(line);
    }
    starfalls_ = std::move(now);
}

void Server::logRespawns() {
    if (!world_.isWorld()) return;
    std::map<PilotId, bool> now;
    for (const auto& [c, client] : clients_) {
        if (!client.seated) continue;
        const auto r = world_.respawning(client.pilot);
        now[client.pilot] = r.has_value();
        const auto was = respawns_.find(client.pilot);
        const bool down = was != respawns_.end() && was->second;
        if (r && !down) {
            log("pilot " + std::to_string(client.pilot) + " fell in " + world_.loaded()->regions[*world_.where(client.pilot)].file.id +
                ", back in " + format("%.0f", r->at - world_.clock()) + " s in " + r->region +
                (r->home ? " at its " + std::string(rawValue(*r->home)) : std::string(" at the start pad")));
        } else if (!r && down) {
            const auto at = world_.where(client.pilot);
            log("pilot " + std::to_string(client.pilot) + " respawns in " + (at ? world_.loaded()->regions[*at].file.id : std::string("?")));
        }
    }
    respawns_ = std::move(now);
}

void Server::tick(double now, bool stepRegion) {
    const auto t0 = std::chrono::steady_clock::now();
    if (idling_) {
        idling_ = false;
        log("awake: a pilot on line, 60 Hz");
    }
    endpoint_.update(now);
    for (net::Event e; endpoint_.poll(e);) handle(e, now);
    if (stepRegion) {
        world_.step();
        stepSum_ += world_.lastStepMs();
        stepMax_ = std::max(stepMax_, world_.lastStepMs());
        logStarfalls();
        logRespawns();
    }
    sendMoves();
    ticked_ += 1;
    if (options_.snapshotEvery > 0 && ticked_ % options_.snapshotEvery == 0) sendSnapshots(now);
    endpoint_.flush(now);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    tickSum_ += ms;
    tickMax_ = std::max(tickMax_, ms);
    ticks_ += 1;
    if (statsFrom_ < 0) statsFrom_ = now;
    if (options_.statsEvery > 0 && now - statsFrom_ >= options_.statsEvery) statsLine(now);
    // What changed, every few seconds: a crash loses no more.
    if (savedAt_ < 0) savedAt_ = now;
    if (options_.store && now - savedAt_ >= options_.saveEvery) {
        savedAt_ = now;
        save();
    }
}

void Server::stop(double now, const std::string& reason) {
    std::vector<net::ConnectionId> ids;
    for (const auto& [c, client] : clients_) ids.push_back(c);
    for (net::ConnectionId c : ids) {
        endpoint_.disconnect(c, now, reason);
        world_.leave(clients_[c].pilot);
        log("disconnect pilot " + std::to_string(clients_[c].pilot) + ": " + reason);
        clients_.erase(c);
    }
    if (options_.store && save())
        log("saved: " + std::to_string(lastSave_.changed) + " rows changed, " + std::to_string(lastSave_.removed) + " removed");
}

void Server::handle(const net::Event& e, double now) {
    switch (e.type) {
    case net::Event::Type::connected: {
        // Seated once its Join comes (`join`).
        Client& client = clients_[e.connection];
        client.pilot = e.connection;
        log("connect pilot " + std::to_string(client.pilot) + " from " + e.remote.toString());
        return;
    }
    case net::Event::Type::disconnected: {
        const auto it = clients_.find(e.connection);
        if (it == clients_.end()) return;
        world_.leave(it->second.pilot);
        log("disconnect pilot " + std::to_string(it->second.pilot) + ": " + e.reason);
        clients_.erase(it);
        return;
    }
    case net::Event::Type::refused:
        log("refuse " + e.remote.toString() + ": " + e.reason);
        return;
    case net::Event::Type::message: {
        const auto it = clients_.find(e.connection);
        if (it != clients_.end()) message(e.connection, it->second, e.data, now);
        return;
    }
    }
}

void Server::message(net::ConnectionId c, Client& client, const std::vector<uint8_t>& data, double now) {
    const auto type = net::messageType(data);
    if (!type) return;
    // Nothing but the Join until the pilot is seated.
    if (*type == net::MessageType::join) return join(c, client, data, now);
    if (!client.seated) return;
    switch (*type) {
    case net::MessageType::input: {
        net::InputMessage m;
        if (!net::decode(data, m)) return;
        // Oldest first: the region keeps them in tick order and drops repeats.
        for (auto it = m.inputs.rbegin(); it != m.inputs.rend(); ++it) world_.input(client.pilot, net::quantised(*it));
        return;
    }
    case net::MessageType::ack: {
        net::AckMessage m;
        if (net::decode(data, m)) client.writer.ack(m, now);
        return;
    }
    case net::MessageType::request: {
        net::RequestMessage m;
        if (!net::decode(data, m)) return;
        net::ReplyMessage reply;
        reply.request = m.id;
        Region* region = world_.regionOf(client.pilot);
        if (!region) return;
        if (const auto* take = std::get_if<net::RequestMessage::Take>(&m.what)) {
            // W5: another human driving here takes nothing from this one
            // (Region::take refuses its unit).
            if (world_.flying(client.pilot)) reply.reason = "in flight";
            else if (!region->take(client.pilot, take->unit)) reply.reason = "that unit cannot be driven";
            else reply.done = true;
            if (!reply.done)
                log("refuse take " + std::to_string(take->unit) + " by pilot " + std::to_string(client.pilot) + ": " + reply.reason);
        } else if (const auto* fly = std::get_if<net::RequestMessage::Fly>(&m.what)) {
            if (const auto why = world_.fly(client.pilot, fly->region, fly->pad)) {
                reply.reason = *why;
                log("refuse flight to " + fly->region + " " + fly->pad + " by pilot " + std::to_string(client.pilot) + ": " + *why);
            } else {
                reply.done = true;
                log("pilot " + std::to_string(client.pilot) + " flies to " + fly->region + " " + fly->pad);
            }
            sendMoves();
        } else if (const auto* build = std::get_if<net::RequestMessage::Build>(&m.what)) {
            const std::string what = std::string(rawValue(build->kind)) + " at " + std::to_string(build->at.x) + ","
                                     + std::to_string(build->at.y) + " by pilot " + std::to_string(client.pilot);
            if (const auto why = world_.build(client.pilot, build->kind, build->at)) {
                reply.reason = *why;
                log("refuse build " + what + ": " + *why);
            } else {
                reply.done = true;
                log("build " + what);
            }
        } else if (const auto* pick = std::get_if<net::RequestMessage::Pick>(&m.what)) {
            const std::string what = std::string(rawValue(pick->perk)) + " for " + std::string(rawValue(pick->kind)) + " by pilot "
                                     + std::to_string(client.pilot);
            if (const auto why = world_.pick(client.pilot, pick->kind, pick->perk)) {
                reply.reason = *why;
                log("refuse pick " + what + ": " + *why);
            } else {
                reply.done = true;
                log("pick " + what);
            }
        } else if (const auto why = skillTree(m, client.pilot)) {
            reply.reason = why->value_or("");
            reply.done = !why->has_value();
        } else {
            if (region->driving(client.pilot)) {
                region->handBack(client.pilot);
                reply.done = true;
            } else {
                reply.reason = "nothing driven";
            }
        }
        const std::vector<uint8_t> bytes = net::encode(reply);
        endpoint_.send(c, net::Channel::reliable, bytes.data(), bytes.size());
        return;
    }
    default:
        return;  // the server's own kinds
    }
}

std::optional<std::optional<std::string>> Server::skillTree(const net::RequestMessage& m, PilotId pilot) {
    std::string what;
    std::optional<std::string> why;
    if (const auto* s = std::get_if<net::RequestMessage::Spend>(&m.what)) {
        what = "spend " + std::string(rawValue(s->skill));
        why = world_.spend(pilot, s->skill);
    } else if (const auto* u = std::get_if<net::RequestMessage::Undo>(&m.what)) {
        what = "undo " + std::string(rawValue(u->skill));
        why = world_.undo(pilot, u->skill);
    } else if (std::holds_alternative<net::RequestMessage::Respec>(m.what)) {
        what = "respec";
        why = world_.respec(pilot);
    } else if (const auto* sl = std::get_if<net::RequestMessage::Slot>(&m.what)) {
        what = "slot " + std::to_string(sl->index) + " " + (sl->skill ? std::string(rawValue(*sl->skill)) : "empty");
        why = world_.slot(pilot, sl->index, sl->skill);
    } else if (const auto* ab = std::get_if<net::RequestMessage::Abandon>(&m.what)) {
        what = "abandon contract " + std::to_string(ab->contract);
        why = world_.abandon(pilot, ab->contract);
    } else if (const auto* mk = std::get_if<net::RequestMessage::Make>(&m.what)) {
        what = "make " + std::string(rawValue(mk->module)) + " mk " + std::to_string(mk->mark);
        why = world_.make(pilot, mk->module, mk->mark);
    } else if (const auto* rs = std::get_if<net::RequestMessage::Research>(&m.what)) {
        what = "research " + std::string(rawValue(rs->module)) + " mk " + std::to_string(rs->mark);
        why = world_.research(pilot, rs->module, rs->mark);
    } else if (const auto* ft = std::get_if<net::RequestMessage::Fit>(&m.what)) {
        what = "fit " + std::string(rawValue(ft->module)) + " mk " + std::to_string(ft->mark);
        why = world_.fit(pilot, ft->module, ft->mark);
    } else if (const auto* dn = std::get_if<net::RequestMessage::Donate>(&m.what)) {
        what = "donate " + std::to_string(dn->ore) + " ore " + std::to_string(dn->hydrogen) + " MH";
        why = world_.donate(pilot, dn->ore, dn->hydrogen);
    } else if (const auto* by = std::get_if<net::RequestMessage::Buy>(&m.what)) {
        what = "buy " + (by->blueprint ? std::string(rawValue(*by->blueprint)) + " blueprint" : std::string(rawValue(by->item)));
        why = world_.buy(pilot, by->blueprint, by->item);
    } else if (const auto* us = std::get_if<net::RequestMessage::Use>(&m.what)) {
        what = "use " + std::string(rawValue(us->item));
        why = world_.use(pilot, us->item);
    } else if (const auto* mi = std::get_if<net::RequestMessage::MissionAsk>(&m.what)) {
        what = std::string(mi->take ? "take" : "abandon") + " mission " + std::to_string(mi->mission);
        why = mi->take ? world_.takeMission(pilot, mi->mission) : world_.abandonMission(pilot, mi->mission);
    } else if (const auto* uc = std::get_if<net::RequestMessage::UpgradeCitadel>(&m.what)) {
        what = "upgrade the Citadel's " + std::string(rawValue(uc->stat));
        why = world_.upgradeCitadel(pilot, uc->stat);
    } else if (const auto* sp = std::get_if<net::RequestMessage::StartProject>(&m.what)) {
        // R2: a realm project, paid from the realm wallet.
        if (sp->kind >= RealmProject::count) {
            what = "start a realm project";
            why = std::string("No such project");
        } else {
            const auto kind = RealmProject::Kind(sp->kind);
            what = "start the realm's " + std::string(RealmProject::title(kind));
            why = world_.startProject(pilot, kind);
        }
    } else if (std::holds_alternative<net::RequestMessage::CallHunt>(m.what)) {
        // N4: THE HUNT, free, once a world day realm-wide.
        what = "call the hunt";
        why = world_.callHunt(pilot);
    } else if (const auto* ut = std::get_if<net::RequestMessage::UpgradeTurret>(&m.what)) {
        what = "upgrade turret " + std::to_string(ut->turret) + "'s " + std::string(rawValue(ut->stat));
        why = world_.upgradeTurret(pilot, ut->turret, ut->stat);
    } else if (const auto* mc = std::get_if<net::RequestMessage::Merc>(&m.what)) {
        using Act = net::RequestMessage::Merc::Act;
        if (mc->act == Act::hire) {
            what = mc->guard ? "hire a guard mercenary" : "hire a mercenary";
            why = world_.hireMerc(pilot, mc->guard);
        } else if (mc->act == Act::role) {
            what = "make mercenary " + std::to_string(mc->merc) + (mc->guard ? " a guard" : " a miner");
            why = world_.mercRole(pilot, mc->merc, mc->guard);
        } else if (mc->act == Act::dismiss) {
            what = "dismiss mercenary " + std::to_string(mc->merc);
            why = world_.dismissMerc(pilot, mc->merc);
        } else {
            what = "send mercenary " + std::to_string(mc->merc) + " to " + mc->region + " field " + std::to_string(mc->field);
            why = world_.assignMerc(pilot, mc->merc, mc->region, mc->field);
        }
    } else {
        return std::nullopt;
    }
    log((why ? "refuse " + what : what) + " by pilot " + std::to_string(pilot) + (why ? ": " + *why : ""));
    return why;
}

void Server::sendProgress(net::ConnectionId c, Client& client) {
    auto m = world_.progressMessage(client.pilot);
    if (!m) return;
    m->news = world_.takeNews(client.pilot);
    // X2: what the world did while it was away, once.
    m->away = world_.takeAway(client.pilot);
    // XP alone rides in the snapshots.
    net::ProgressMessage same = *m;
    same.xp = client.told ? client.told->xp : same.xp;
    same.news.clear();
    same.away = std::nullopt;
    if (client.told && *client.told == same && m->news.empty() && !m->away) return;
    for (const auto& n : m->news)
        if (n.kind == net::ProgressMessage::NewsKind::level)
            log("pilot " + std::to_string(client.pilot) + " reaches level " + std::to_string(n.value));
    if (m->away)
        log("pilot " + std::to_string(client.pilot) + " back after " + net::awayRan(m->away->ran) + ": " +
            std::to_string(m->away->rows.size()) + " away rows told");
    sendReliable(c, net::encode(*m));
    m->news.clear();
    m->away = std::nullopt;
    client.told = *m;
}

void Server::sendStandings(net::ConnectionId c, Client& client) {
    // At most once a second, and only when it changed (S4).
    if (client.standingsTold && ticked_ % 60 != 0) return;
    auto m = world_.standingsMessage(client.pilot);
    if (!m || (client.standingsTold && *client.standingsTold == *m)) return;
    sendReliable(c, net::encode(*m));
    client.standingsTold = std::move(m);
}

void Server::sendHunt(net::ConnectionId c, Client& client) {
    // At most once a second, and only when it changed (N4).
    if (client.huntTold && ticked_ % 60 != 0) return;
    auto m = world_.huntMessage(client.pilot);
    if (!m || (client.huntTold && *client.huntTold == *m)) return;
    sendReliable(c, net::encode(*m));
    client.huntTold = std::move(m);
}

void Server::join(net::ConnectionId c, Client& client, const std::vector<uint8_t>& data, double now) {
    net::JoinMessage m;
    if (client.seated || !net::decode(data, m)) return;
    const std::string key = net::pilotKeyHex(m.pilot);
    const std::string who = "pilot " + std::to_string(client.pilot) + " id " + net::pilotKeyShort(m.pilot);
    const WorldMap::Seated seated = world_.seat(client.pilot, key);
    if (seated == WorldMap::Seated::refused) {
        log("refuse join " + who + ": already in the world");
        endpoint_.disconnect(c, now, "this pilot is in the world already");
        clients_.erase(c);
        return;
    }
    client.seated = true;
    const Region& r = *world_.regionOf(client.pilot);
    net::Welcome w;
    w.pilot = client.pilot;
    w.region = r.description();
    w.serverTick = r.tick();
    w.time = r.simulation().state.time;
    sendReliable(c, net::encode(w));
    sendWorld(c, client.pilot);
    std::string where;
    if (const auto& loaded = world_.loaded()) where = loaded->regions[*world_.where(client.pilot)].file.id;
    if (seated == WorldMap::Seated::restored) {
        log("join " + who + ": restored in " + where);
    } else {
        const auto wallet = world_.wallet(client.pilot);
        log("join " + who + ": new" + (where.empty() ? "" : " at the start pad in " + where) +
            (wallet ? ", " + std::to_string(wallet->ore) + " ore" : ""));
    }
}

void Server::sendSnapshots(double now) {
    // One round: nothing between the pilots' snapshots changes a unit or a
    // building, so each entity's bits are made once (`WorldMap::beginSnapshots`).
    world_.beginSnapshots();
    for (auto& [c, client] : clients_) {
        if (!client.seated) continue;
        sendProgress(c, client);
        sendStandings(c, client);
        sendHunt(c, client);
        net::Snapshot s = world_.snapshot(client.pilot, options_.interest);
        if (!s.events.empty())
            for (const auto& bytes : net::encodeEvents(s.serverTick, s.events))
                endpoint_.send(c, net::Channel::reliable, bytes.data(), bytes.size());
        const net::SnapshotWriter::Output out = client.writer.write(s, now, endpoint_.budgetLeft(c, now));
        for (const auto& bytes : out.messages) endpoint_.send(c, net::Channel::unreliable, bytes.data(), bytes.size());
        if (!out.deferred.empty()) world_.deferred(client.pilot, out.deferred);
        client.snapshots += out.messages.empty() ? 0 : 1;
        client.snapshotMessages += int64_t(out.messages.size());
        client.deferred += int64_t(out.deferred.size());
    }
    world_.endSnapshots();
}

std::vector<Server::ClientStats> Server::clientStats() const {
    std::vector<ClientStats> out;
    for (const auto& [c, client] : clients_) {
        ClientStats s;
        s.connection = c;
        s.pilot = client.pilot;
        if (const net::ConnectionStats* cs = endpoint_.stats(c)) s.bytesSent = cs->bytesSent;
        s.snapshots = client.snapshots;
        s.snapshotMessages = client.snapshotMessages;
        s.deferred = client.deferred;
        s.fullResets = client.writer.fullResets();
        out.push_back(s);
    }
    return out;
}

void Server::statsLine(double now) {
    const double seconds = now - statsFrom_;
    std::string line = "stats: clients " + std::to_string(clients_.size()) + ", tick mean " +
                       format("%.2f", ticks_ ? tickSum_ / double(ticks_) : 0) + " ms max " + format("%.2f", tickMax_) + " ms, " +
                       std::to_string(world_.regionCount()) + " regions step mean " +
                       format("%.2f", ticks_ ? stepSum_ / double(ticks_) : 0) + " ms max " + format("%.2f", stepMax_) + " ms";
    for (auto& [c, client] : clients_) {
        const net::ConnectionStats* cs = endpoint_.stats(c);
        const uint64_t sent = cs ? cs->bytesSent : 0;
        const double rate = seconds > 0 ? double(sent - client.bytesAtStats) / seconds : 0;
        client.bytesAtStats = sent;
        line += ", pilot " + std::to_string(client.pilot) + " out " + format("%.0f", rate) + " B/s";
    }
    log(line);
    statsFrom_ = now;
    tickSum_ = 0;
    tickMax_ = 0;
    ticks_ = 0;
    stepSum_ = 0;
    stepMax_ = 0;
}

bool rotateLog(const std::string& path, long long most) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0 || st.st_size <= most) return false;
    const std::string old = path + ".1";
    std::remove(old.c_str());
    return std::rename(path.c_str(), old.c_str()) == 0;
}

} // namespace ac::server
