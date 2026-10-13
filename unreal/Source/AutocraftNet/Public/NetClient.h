// The client's end of the protocol (docs/networking.md, "The client"): it
// connects, keeps a mirror of the region (net::Mirror: an unstepped
// ac::Simulation built from the Welcome's region, its state overwritten by
// the snapshots), and each client tick sends its input (with the last few
// again) and its ack. In a world, a RegionChange replaces the mirror with
// one of the new region; snapshots of a visit not reached yet wait for it. The game's MMORPG mode (AcMmo.h) and the server's
// headless tests and bench share it.
#pragma once

#include "AutocraftNetApi.h"
#include "NetEndpoint.h"
#include "NetMirror.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ac::net {

class AUTOCRAFTNET_API Client {
public:
    Client(PacketIo& io, const ClientConfig& config);

    void connect(double now);
    /// One client tick: `receive`, `sendInput`, `flush`.
    void tick(double now);

    /// Reads what came: the Welcome, snapshot parts (into the mirror),
    /// events, replies, a disconnect.
    void receive(double now);
    /// Sends this tick's input (`steer`, stamped with the next client tick,
    /// with the last few again) and the ack. Nothing before the Welcome.
    /// The input sent, with its tick (for the prediction, `Predictor`).
    std::optional<PilotInput> sendInput();
    /// Puts what was sent on the wire.
    void flush(double now);

    /// What the pilot does now. The presses (`act`, `ability`) go out once.
    PilotInput steer;

    /// Sends a request; its reply lands in `replies` under the number returned.
    uint32_t take(int64_t unit);
    uint32_t handBack();
    /// Fly from the pad the driven Prospector stands at to a world's region's
    /// pad (by the region's id and the pad's name).
    uint32_t fly(const std::string& region, const std::string& pad);
    /// Build a `kind` at `at` with the driven Prospector, paid from the
    /// wallet (the reply says why not).
    uint32_t build(StructureKind kind, Vec2 at);

    const ClientEndpoint& endpoint() const { return endpoint_; }
    bool connected() const { return endpoint_.state() == ClientEndpoint::State::connected; }
    /// Why the connection ended or was refused (empty while it holds).
    const std::string& disconnectReason() const { return disconnectReason_; }
    const std::optional<Welcome>& welcome() const { return welcome_; }
    /// Nil until the Welcome.
    const Mirror* mirror() const { return mirror_.get(); }
    Mirror* mirror() { return mirror_.get(); }
    int64_t clientTick() const { return tick_; }

    /// Called after each snapshot part is applied to the mirror.
    std::function<void(const SnapshotPart&)> onSnapshot;
    /// Called after a RegionChange built the new mirror (the old one is gone).
    std::function<void(const RegionChange&)> onRegionChange;

    /// The pilot's region changes so far (0: still the Welcome's region).
    int64_t visit() const { return visit_; }
    /// The last RegionChange (nil: none yet).
    const std::optional<RegionChange>& regionChange() const { return regionChange_; }
    /// The world map, the newest (nil: not a world, or not told yet).
    const std::optional<WorldMessage>& world() const { return world_; }
    /// Snapshot parts waiting for their region's mirror.
    static constexpr size_t kMaxWaiting = 64;

    std::map<uint32_t, ReplyMessage> replies;
    /// The events received, in order (the caller takes them).
    std::vector<GameEvent> events;
    /// Snapshot messages read, and those that could not be.
    int64_t partsRead = 0;
    int64_t partsFailed = 0;

private:
    uint32_t request(const decltype(RequestMessage::what)& what);
    void message(const std::vector<uint8_t>& data);
    void applyPart(const SnapshotPart& part);

    ClientEndpoint endpoint_;
    SnapshotReader reader_;
    std::unique_ptr<Mirror> mirror_;
    std::optional<Welcome> welcome_;
    std::optional<RegionChange> regionChange_;
    std::optional<WorldMessage> world_;
    int64_t visit_ = 0;
    std::deque<SnapshotPart> waiting_;
    std::deque<PilotInput> sent_;
    std::string disconnectReason_;
    int64_t tick_ = 0;
    uint32_t nextRequest_ = 1;
};

} // namespace ac::net
