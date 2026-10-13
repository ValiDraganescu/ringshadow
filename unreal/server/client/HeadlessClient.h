// A client with no engine, for the tests and the bench: it connects, keeps
// a mirror of the region (net::Mirror: an unstepped ac::Simulation built from
// the Welcome's region, its state overwritten by the snapshots), and each
// tick sends its input (with the last few again) and its ack, as the game
// will at 60 Hz.
#pragma once

#include "NetEndpoint.h"
#include "NetMirror.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ac::server {

class HeadlessClient {
public:
    HeadlessClient(net::PacketIo& io, const net::ClientConfig& config);

    void connect(double now);
    /// One client tick: reads what came, sends this tick's input (`steer`,
    /// stamped with the next tick) and the ack.
    void tick(double now);

    /// What the pilot does now.
    net::PilotInput steer;

    /// Sends a request; its reply lands in `replies` under the number returned.
    uint32_t take(int64_t unit);
    uint32_t handBack();

    const net::ClientEndpoint& endpoint() const { return endpoint_; }
    bool connected() const { return endpoint_.state() == net::ClientEndpoint::State::connected; }
    /// Why the connection ended or was refused (empty while it holds).
    const std::string& disconnectReason() const { return disconnectReason_; }
    const std::optional<net::Welcome>& welcome() const { return welcome_; }
    /// Nil until the Welcome.
    const net::Mirror* mirror() const { return mirror_.get(); }
    int64_t clientTick() const { return tick_; }

    std::map<uint32_t, net::ReplyMessage> replies;
    std::vector<GameEvent> events;
    /// Snapshot messages read, and those that could not be.
    int64_t partsRead = 0;
    int64_t partsFailed = 0;

private:
    uint32_t request(const net::RequestMessage::Take* take);
    void message(const std::vector<uint8_t>& data);

    net::ClientEndpoint endpoint_;
    net::SnapshotReader reader_;
    std::unique_ptr<net::Mirror> mirror_;
    std::optional<net::Welcome> welcome_;
    std::deque<net::PilotInput> sent_;
    std::string disconnectReason_;
    int64_t tick_ = 0;
    uint32_t nextRequest_ = 1;
};

} // namespace ac::server
