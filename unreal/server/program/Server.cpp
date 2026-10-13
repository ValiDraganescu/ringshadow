#include "Server.h"

#include "NetProtocol.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

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
    : options_(options), world_(makeWorld(options)), endpoint_(io, endpointConfig(options)) {}

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

void Server::tick(double now, bool stepRegion) {
    const auto t0 = std::chrono::steady_clock::now();
    endpoint_.update(now);
    for (net::Event e; endpoint_.poll(e);) handle(e, now);
    if (stepRegion) {
        world_.step();
        stepSum_ += world_.lastStepMs();
        stepMax_ = std::max(stepMax_, world_.lastStepMs());
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
}

void Server::handle(const net::Event& e, double now) {
    switch (e.type) {
    case net::Event::Type::connected: {
        Client& client = clients_[e.connection];
        client.pilot = e.connection;
        world_.join(client.pilot);
        const Region& r = *world_.regionOf(client.pilot);
        net::Welcome w;
        w.pilot = client.pilot;
        w.region = r.description();
        w.serverTick = r.tick();
        w.time = r.simulation().state.time;
        sendReliable(e.connection, net::encode(w));
        sendWorld(e.connection, client.pilot);
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
            bool other = false;
            for (const auto& [id, o] : clients_)
                if (id != c && region->driving(o.pilot)) other = true;
            if (world_.flying(client.pilot)) reply.reason = "in flight";
            else if (other) reply.reason = "another pilot drives";
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

void Server::sendSnapshots(double now) {
    for (auto& [c, client] : clients_) {
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

} // namespace ac::server
