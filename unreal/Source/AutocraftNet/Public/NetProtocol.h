// The protocol's messages (docs/networking.md, "The messages"): what the
// server and the game say to each other over the transport, as the core's own
// types, and their bit-packed encoding. The transport (NetEndpoint.h) knows
// nothing of these; only the protocol files use the core.
//
// Every message starts with its type (u8). Client to server: Input
// (unreliable), Ack (unreliable), Request (reliable), Join (reliable, the
// first after connected: no Welcome before it). Server to client:
// Welcome (reliable, the first message after connected), Snapshot
// (unreliable, NetSnapshot.h), Event (reliable), Reply (reliable),
// RegionChange and World (reliable). A Welcome or a RegionChange that
// carries a region file too large for one reliable message goes on the bulk
// channel.
#pragma once

#include "AutocraftNetApi.h"
#include "NetBitStream.h"

#include "Pilot.h"
#include "RegionFile.h"
#include "Types.h"
#include "WindowMaps.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ac::net {

enum class MessageType : uint8_t {
    welcome = 1,
    input = 2,
    ack = 3,
    request = 4,
    reply = 5,
    event = 6,
    snapshot = 7,
    regionChange = 8,
    world = 9,
    join = 10,
};

/// The type of a message, or nil when it is none of ours.
AUTOCRAFTNET_API std::optional<MessageType> messageType(const std::vector<uint8_t>& message);

// MARK: - What both ends share

/// What a client needs to build the same ground: the map is generated from
/// its choice (`WindowMaps::build`, or `buildSquare` past 2 starts), or
/// built from a region file (docs/regions.md) when `regionFile` is set; name,
/// version and seed let it check it built the same one.
struct RegionDescription {
    MapChoice map;
    /// A world's region: its region file, whole (`region::parse`,
    /// `region::build`). Empty: the generated map of `map`.
    std::string regionFile;
    /// A world's region is wild ground: no starting bases, no AI; the state
    /// starts with the map's ore and wells only.
    bool wild = false;
    std::string mapName;
    int64_t mapVersion = 0;
    uint64_t seed = 0;
    bool fog = true;
    /// Seconds of game time a server tick steps.
    double step = 1.0 / 60;
    bool operator==(const RegionDescription&) const = default;
};

/// One tick of a client's controls: the `ac::Pilot` fields it sets each
/// frame (the unit is the region's business), stamped with the client's tick.
struct PilotInput {
    int64_t tick = 0;
    Vec2 walk{0, 0};
    double heading = 0;
    /// Key presses since the client's last tick: applied on this input's
    /// server tick only.
    bool act = false;
    bool ability = false;
    /// Keys held, and the crosshair: kept until the next input.
    bool hold = false;
    bool mend = false;
    std::optional<Pilot::Crosshair> crosshair;
    bool operator==(const PilotInput&) const = default;
};

/// How much an entity matters to the pilot, most first.
enum class Rank : uint8_t {
    /// The unit it drives.
    driven,
    /// What shoots at the driven unit, and what it shoots at or aims at.
    engaged,
    /// The rest, by distance.
    nearby,
};

/// A unit or a building as the pilot's side may know it: the core's own
/// types, so a client writes it back into its `GameState`. An enemy unit's
/// plans (where it walks, its errand, its build order) are left out.
struct EntityState {
    std::variant<Unit, Structure> value;
    /// An enemy building as the side last saw it (`Intel`), not in sight now.
    bool remembered = false;
    /// The server's order of interest; not sent (a decoded entity is
    /// `nearby` at 0).
    Rank rank = Rank::nearby;
    /// Cells from the pilot's focus; not sent.
    double distance = 0;

    int64_t id() const { return std::visit([](const auto& v) { return v.id; }, value); }
    const Unit* unit() const { return std::get_if<Unit>(&value); }
    const Structure* structure() const { return std::get_if<Structure>(&value); }
};

/// An ore deposit or an MH well in sight, by its index in the state (a
/// Prospector's `patch` is that index).
struct PatchView {
    int64_t index = 0;
    OreDeposit patch;
    bool operator==(const PatchView&) const = default;
};
struct WellView {
    int64_t index = 0;
    Well well;
    bool operator==(const WellView&) const = default;
};

/// The side's fog: what it sees now (`Sight::seen`, `Sight::near`, on the
/// client's own `Vision` grid of the same map) and what it has explored and
/// when it last saw each base site (`Intel`).
struct FogView {
    std::vector<uint8_t> seen;
    std::vector<uint8_t> near;
    std::vector<uint64_t> explored;
    std::vector<double> sites;
    bool operator==(const FogView&) const = default;
};

/// What a pilot is told after a tick: plain data. The server program sends
/// it as Snapshot messages (NetSnapshot.h) and `events` as Event messages.
struct Snapshot {
    int64_t serverTick = 0;
    /// The last of this pilot's inputs applied (-1: none yet).
    int64_t lastInputTick = -1;
    /// Bumped when the region starts the next game after a victory: the
    /// client starts its mirror over.
    int64_t game = 0;
    double time = 0;
    /// The unit this pilot drives.
    std::optional<int64_t> driven;
    std::optional<int64_t> winner;
    std::optional<double> endedAt;
    std::vector<int64_t> score;
    /// Most important first: the driven unit, what it fights, then by
    /// distance. Only what is due and fits the budget.
    std::vector<EntityState> entities;
    /// Ids told before that are gone from the side's knowledge (dead, or
    /// out of sight).
    std::vector<int64_t> removed;
    /// Every player, by index: the side's own and its allies' whole; the
    /// others' upgrades and team only (what `maxHP` and `weapon` read).
    std::vector<Player> players;
    std::vector<PatchView> patches;
    std::vector<WellView> wells;
    /// Nil: a region without fog.
    std::optional<FogView> fog;
    /// What happened since the pilot's last snapshot that its side may know.
    std::vector<GameEvent> events;
};

// MARK: - Quantising

/// Fixed precision for the protocol's values (docs/networking.md, "Snapshots
/// as deltas"); `quantise(x)` is what arrives at the other end.
namespace q {
/// Positions: centimetres, signed, 22 bits a coordinate (±20 km).
inline constexpr int kPositionBits = 22;
AUTOCRAFTNET_API int64_t centimetres(double v);
/// Angles: 16 bits a full turn; out comes 0…2π.
AUTOCRAFTNET_API uint16_t angle(double radians);
AUTOCRAFTNET_API double angle(uint16_t q);
/// Health, shields, energy: 12 bits over 0…`range`, or the exact value
/// past it (one more bit).
inline constexpr int kHealthBits = 12;
/// A walk axis, −1…1: in 255ths, so 0 and ±1 are exact.
inline constexpr int64_t kWalkSteps = 255;
} // namespace q

/// What the server applies: the walk and the heading as they go on the wire.
AUTOCRAFTNET_API PilotInput quantised(const PilotInput& in);

// MARK: - Messages

/// Server → client, reliable, first after connected.
struct Welcome {
    uint64_t pilot = 0;
    RegionDescription region;
    int64_t serverTick = 0;
    double time = 0;
    bool operator==(const Welcome&) const = default;
};

/// Client → server, unreliable, every client tick: the current input and the
/// last few again, newest first, so a lost packet costs nothing.
struct InputMessage {
    static constexpr size_t kMaxInputs = 4;
    std::vector<PilotInput> inputs;
    bool operator==(const InputMessage&) const = default;
};

/// Client → server, unreliable, every client tick: the snapshot parts
/// received, newest snapshot first. The first is the newest snapshot
/// received (the baseline for the next deltas); the few before it come again
/// for loss.
struct AckMessage {
    static constexpr size_t kMaxSnapshots = 4;
    struct Received {
        int64_t snapshot = 0;
        /// Bit i: part i arrived and was read.
        uint32_t parts = 0;
        bool operator==(const Received&) const = default;
    };
    std::vector<Received> received;
    bool operator==(const AckMessage&) const = default;
};

/// Client → server, reliable: everything that is not steering. Take a
/// unit, hand it back, fly from a pad to a pad (a world's region, by its id,
/// and the pad's name in it), build with the driven Prospector, pick a
/// level-up card.
///
/// The pilot's levels themselves (per kind: level, XP, picks) come in the
/// snapshot's player data: its side's `Player::pilot` is the pilot's own
/// record (the server's `Region::levels`), so the two cards on offer are
/// `PilotRecord::pending`, the core's catalogue, on both ends.
struct RequestMessage {
    struct Take {
        int64_t unit = 0;
        bool operator==(const Take&) const = default;
    };
    struct HandBack {
        bool operator==(const HandBack&) const = default;
    };
    struct Fly {
        std::string region;
        std::string pad;
        bool operator==(const Fly&) const = default;
    };
    /// The driven Prospector puts up a `kind` at `at` (the region's ground
    /// position, snapped as `Simulation::snap` does), paid from the wallet.
    struct Build {
        StructureKind kind = StructureKind::citadel;
        Vec2 at{0, 0};
        bool operator==(const Build&) const = default;
    };
    /// Keep `perk` for `kind`: one of the two cards on offer at the kind's
    /// lowest pending level (the server checks, `PilotRecord::refusal`).
    struct Pick {
        UnitKind kind = UnitKind::prospector;
        Perk perk{};
        bool operator==(const Pick&) const = default;
    };
    /// The client's number for it, echoed in the reply.
    uint32_t id = 0;
    std::variant<Take, HandBack, Fly, Build, Pick> what;
    bool operator==(const RequestMessage&) const = default;
};

/// A pilot's own id: 128 random bits the game keeps (docs/mmorpg-m3.md,
/// "Pilot identity"). The server saves the pilot under it.
using PilotKey = std::array<uint8_t, 16>;
/// 32 lowercase hex digits; `pilotKeyShort`: the first 8, for the log.
AUTOCRAFTNET_API std::string pilotKeyHex(const PilotKey& key);
AUTOCRAFTNET_API std::string pilotKeyShort(const PilotKey& key);
/// From 32 hex digits (either case); nil when it is not that.
AUTOCRAFTNET_API std::optional<PilotKey> pilotKeyFromHex(std::string_view hex);
/// A new random key (net::secureRandom); all zeros when there is no source.
AUTOCRAFTNET_API PilotKey newPilotKey();

/// Client → server, reliable, the first message after connected: who the
/// pilot is. A known id is seated where it left; an unknown one is a new
/// pilot at the start pad. The server sends the Welcome after it.
struct JoinMessage {
    PilotKey pilot{};
    bool operator==(const JoinMessage&) const = default;
};

/// Server → client, reliable: the answer to a request.
struct ReplyMessage {
    uint32_t request = 0;
    bool done = false;
    /// Why not, when refused.
    std::string reason;
    bool operator==(const ReplyMessage&) const = default;
};

/// Server → client, reliable: the pilot's events since the last, in order
/// (as many messages as they need).
struct EventMessage {
    int64_t serverTick = 0;
    std::vector<GameEvent> events;
    bool operator==(const EventMessage&) const = default;
};

/// A snapshot's `game` in a world: the pilot's region changes (its visit)
/// in the high 32 bits, the region's own game in the low. A client's mirror
/// takes only the snapshots of its visit.
inline int64_t visitGame(int64_t visit, int64_t game) { return (visit << 32) | (game & 0xffffffff); }
inline int64_t visitOf(int64_t game) { return game >> 32; }

/// Server → client, reliable: the pilot is in another region now (it drove
/// across an edge, or landed at a pad). The client builds a new mirror from
/// `region`; the snapshots of visit `visit` are this region's.
struct RegionChange {
    int64_t visit = 0;
    RegionDescription region;
    /// The world's region id.
    std::string regionId;
    /// The unit the pilot drives there.
    std::optional<int64_t> driven;
    int64_t serverTick = 0;
    double time = 0;
    bool operator==(const RegionChange&) const = default;
};

/// Server → client, reliable: the world map (after the Welcome, and again
/// whenever the pilot's place in it changes).
struct WorldMessage {
    struct Pad {
        std::string name;
        /// Region cells from the north-west corner (docs/regions.md).
        Vec2 at;
        bool operator==(const Pad&) const = default;
    };
    struct Region {
        std::string id;
        std::string name;
        MapStyle style = MapStyle::highlands;
        MapSize size = MapSize::medium;
        int64_t column = 0;
        int64_t row = 0;
        std::vector<Pad> pads;
        bool operator==(const Region&) const = default;
    };
    int64_t columns = 0;
    int64_t rows = 0;
    std::vector<Region> regions;
    /// Index in `regions` of where the pilot is (its region; in flight,
    /// where it lifted off).
    int64_t here = 0;
    /// In flight to `regions[flyingTo]` (nil: on the ground).
    std::optional<int64_t> flyingTo;
    bool operator==(const WorldMessage&) const = default;
};

/// A World message region's pads on the ground of its map (`bounds`: the
/// region's map; pads are region cells from its north-west corner), for
/// `region::nearestPad` with a unit's position: the server's own rule.
AUTOCRAFTNET_API std::vector<region::Place> padsOnGround(const WorldMessage::Region& r, const GroundRect& bounds);

AUTOCRAFTNET_API std::vector<uint8_t> encode(const Welcome& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const RegionChange& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const WorldMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const InputMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const AckMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const RequestMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const ReplyMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const JoinMessage& m);
/// As many Event messages as the events need, each within a reliable
/// message's size.
AUTOCRAFTNET_API std::vector<std::vector<uint8_t>> encodeEvents(int64_t serverTick, const std::vector<GameEvent>& events);

/// False when the bytes are not that message, or are cut short.
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, Welcome& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, RegionChange& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, WorldMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, InputMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, AckMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, RequestMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, ReplyMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, JoinMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, EventMessage& out);

// MARK: - Bits the messages share

/// Unsigned LEB-style varint (7 bits a group) and its zigzag signed form.
AUTOCRAFTNET_API void writeVarint(BitWriter& w, uint64_t v);
AUTOCRAFTNET_API uint64_t readVarint(BitReader& r);
AUTOCRAFTNET_API void writeSigned(BitWriter& w, int64_t v);
AUTOCRAFTNET_API int64_t readSigned(BitReader& r);
/// `bits` bits of `src` from bit `from`, appended to `w`.
AUTOCRAFTNET_API void appendBits(BitWriter& w, const uint8_t* src, size_t from, size_t bits);
AUTOCRAFTNET_API void writeEvent(BitWriter& w, const GameEvent& e);
AUTOCRAFTNET_API bool readEvent(BitReader& r, GameEvent& e);

} // namespace ac::net
