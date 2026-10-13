// Snapshots on the wire (docs/networking.md, "Snapshots as deltas"): a
// `Snapshot` (NetProtocol.h) written as one or more unreliable messages, each
// readable on its own, as deltas against what the client has acked.
//
// A snapshot is made of items, each with its own baseline: every unit and
// building (by id), every player (split in its core, one record per kind
// driven and one per level stamp), every patch and well in view, the fog in
// chunks of cells, and each base site's last-seen time. The server keeps,
// per client, each item's last state the client acked (it learns from the
// Ack which parts of which snapshots arrived and what was in them) and
// writes an item as the fields that changed against it, or whole when it
// has none. An item the client holds acked and unchanged is not written at
// all. Removed ids go again until a part that carried them is acked. If
// nothing is acked for a second, every item goes whole again.
//
// A message: type, snapshot id, part, the snapshot's header (ticks, time,
// driven, winner, score, player count, fog size), then items, each led by a
// 1 bit and its kind and key, then a 0 bit.
#pragma once

#include "AutocraftNetApi.h"
#include "NetProtocol.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ac::net {

/// At most this many messages a snapshot (bit i of an ack's mask: part i).
inline constexpr size_t kMaxSnapshotParts = 16;
/// Cells of fog an item carries.
inline constexpr size_t kFogChunkCells = 2048;

/// Each value as the other end gets it (written whole and read back).
AUTOCRAFTNET_API Unit quantised(const Unit& u);
AUTOCRAFTNET_API Structure quantised(const Structure& b);
AUTOCRAFTNET_API Player quantised(const Player& p);
AUTOCRAFTNET_API OreDeposit quantised(const OreDeposit& p);
AUTOCRAFTNET_API Well quantised(const Well& w);
AUTOCRAFTNET_API FogView quantised(const FogView& f);
AUTOCRAFTNET_API double quantisedTime(double t);

class AUTOCRAFTNET_API SnapshotWriter {
public:
    struct Output {
        /// 0: nothing written (no budget for even a header).
        int64_t id = 0;
        std::vector<std::vector<uint8_t>> messages;
        /// Entities of the snapshot left out for the budget, for the
        /// region's interest to raise (`Region::deferred`).
        std::vector<int64_t> deferred;
        /// Items written whole and as deltas, and those skipped as unchanged.
        size_t whole = 0;
        size_t deltas = 0;
        size_t unchanged = 0;
    };

    SnapshotWriter();
    ~SnapshotWriter();
    SnapshotWriter(SnapshotWriter&&) noexcept;
    SnapshotWriter& operator=(SnapshotWriter&&) noexcept;

    /// Writes `s` (its events aside) within `budget` bytes, counting each
    /// message's packet overhead. `now`: seconds, the caller's clock.
    Output write(const Snapshot& s, double now, int64_t budget);
    /// What the client says it received.
    void ack(const AckMessage& m, double now);

    /// Times every item went whole again for a second without acks.
    int64_t fullResets() const;

    /// Bytes a message costs the budget past its own: the transport's
    /// chunk head and its share of a packet header.
    static constexpr int64_t kOverhead = 22;

private:
    struct State;
    std::unique_ptr<State> s_;
};

/// One message read.
struct SnapshotPart {
    int64_t id = 0;
    int part = 0;
    /// The header always. `entities`, `removed`, `patches` and `wells`: what
    /// this message carried, whole. `players`: all of them, as the client
    /// knows them now, when this message changed any (else empty). `fog`:
    /// the whole fog as known now, when this message changed any of it
    /// (else nil).
    Snapshot snapshot;
};

class AUTOCRAFTNET_API SnapshotReader {
public:
    SnapshotReader();
    ~SnapshotReader();
    SnapshotReader(SnapshotReader&&) noexcept;
    SnapshotReader& operator=(SnapshotReader&&) noexcept;

    /// Nil when the message is not a snapshot, is broken, or names a
    /// baseline this reader no longer has (it is then not acked, and the
    /// server writes those items again).
    std::optional<SnapshotPart> read(const std::vector<uint8_t>& message);
    /// The parts read of the newest snapshots, for the next Ack.
    AckMessage ack() const;
    /// Forgets everything (a new game).
    void reset();

private:
    struct State;
    std::unique_ptr<State> s_;
};

} // namespace ac::net
