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
    /// Who the pilot is: a reliable Join, sent as soon as the connection is
    /// up (and again after each `connect`). The server seats the pilot and
    /// sends the Welcome only after it.
    void join(const PilotKey& pilot);
    const std::optional<PilotKey>& pilotKey() const { return pilotKey_; }
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

    /// What the pilot does now. The presses (`act`, `ability`, `slot`) go out once.
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
    /// Pick a level-up card: `perk` for `kind` (the reply says why not).
    uint32_t pick(UnitKind kind, Perk perk);
    /// A world's skill tree (the reply says why not): a point into `skill`,
    /// the last point in it back, every point back (at the Citadel), an
    /// active into slot `index` (nil: empty it).
    uint32_t spend(Skill skill);
    uint32_t undo(Skill skill);
    uint32_t respec();
    uint32_t slot(int64_t index, std::optional<Skill> skill);
    /// Drop an open KSTR contract (docs/contracts.md).
    uint32_t abandon(uint32_t contract);
    /// The rig (P7): the Foundry makes `module` at `mark`, the Lab researches
    /// `mark`, `mark` is fitted at home (0: taken off).
    uint32_t make(Module module, int64_t mark);
    uint32_t research(Module module, int64_t mark);
    uint32_t fit(Module module, int64_t mark);
    /// C1: give ore and MH from the wallet to the realm wallet, at the
    /// realm's Citadel.
    uint32_t donate(int64_t ore, int64_t hydrogen);
    /// C2, the Citadel shop: buy one blueprint copy, or one consumable (at
    /// the realm's Citadel); use one consumable in the field.
    uint32_t buyBlueprint(StructureKind kind);
    uint32_t buyItem(Item item);
    uint32_t use(Item item);
    /// K2: sign up for a Citadel mission on the board, or drop one from the
    /// Journal (`RequestMessage::MissionAsk`).
    uint32_t takeMission(uint32_t mission);
    uint32_t abandonMission(uint32_t mission);

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
    /// A world's pilot's progression, the newest Progress message (nil: not
    /// a world, or not told yet). Its XP is the Progress message's; the
    /// mirror's `xp` is newer.
    const std::optional<ProgressMessage>& progress() const { return progress_; }
    /// The progress news received, in order (the caller takes them).
    std::vector<ProgressMessage::News> news;
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
    void sendJoin();

    ClientEndpoint endpoint_;
    SnapshotReader reader_;
    std::unique_ptr<Mirror> mirror_;
    std::optional<Welcome> welcome_;
    std::optional<RegionChange> regionChange_;
    std::optional<WorldMessage> world_;
    std::optional<ProgressMessage> progress_;
    int64_t visit_ = 0;
    std::deque<SnapshotPart> waiting_;
    std::deque<PilotInput> sent_;
    std::string disconnectReason_;
    int64_t tick_ = 0;
    uint32_t nextRequest_ = 1;
    std::optional<PilotKey> pilotKey_;
    bool joinSent_ = false;
};

} // namespace ac::net
