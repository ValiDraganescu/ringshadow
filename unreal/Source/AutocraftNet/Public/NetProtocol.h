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
// RegionChange and World (reliable), Progress (reliable, a world's pilot's
// skill tree), Standings (reliable, the realm week's, Standings.h), Hunt
// (reliable, the realm's hunt, Hunt.h). A Welcome or a RegionChange that
// carries a region file too large for one reliable message goes on the bulk
// channel.
#pragma once

#include "AutocraftNetApi.h"
#include "AwayReport.h"
#include "Hunt.h"
#include "Standings.h"
#include "NetBitStream.h"

#include "Flight.h"
#include "Pilot.h"
#include "Progression.h"
#include "RegionFile.h"
#include "Types.h"
#include "Weather.h"
#include "WindowMaps.h"
#include "WorldClock.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
    progress = 11,
    standings = 12,
    hunt = 13,
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
    /// The client's region changes when it made this input (`Client::visit`).
    /// In a world the server drops an input of an earlier visit: one made
    /// before the client heard of the change (the walk held into the edge)
    /// does not move the unit on in the new region.
    int64_t visit = 0;
    Vec2 walk{0, 0};
    double heading = 0;
    /// The look's pitch, radians (C1: a gunner's aim, `Pilot::pitch`; the
    /// server ignores a non-gunner's). 8 bits over ±90° (`q::pitch`).
    double pitch = 0;
    /// Key presses since the client's last tick: applied on this input's
    /// server tick only.
    bool act = false;
    bool ability = false;
    /// Keys held, and the crosshair: kept until the next input.
    bool hold = false;
    bool mend = false;
    std::optional<Pilot::Crosshair> crosshair;
    /// An active's slot key went down (`Pilot::slot`: 1–3 for Q, C and G, 0
    /// none; docs/prospector-progression.md, "Actives"): a press, like `act`.
    int64_t slot = 0;
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
/// An item's bits as the snapshot writer puts them on the wire
/// (NetSnapshot.cpp; `canonBits`).
struct ItemBits;

struct EntityState {
    std::variant<Unit, Structure> value;
    /// An enemy building as the side last saw it (`Intel`), not in sight now.
    bool remembered = false;
    /// The server's order of interest; not sent (a decoded entity is
    /// `nearby` at 0).
    Rank rank = Rank::nearby;
    /// Cells from the pilot's focus; not sent.
    double distance = 0;
    /// `canonBits` of `value`, made once for every pilot that is sent the
    /// same value in a snapshot round (`Region::snapshot`); not sent. Nil:
    /// the writer makes them itself. Set it only from this very value.
    std::shared_ptr<const ItemBits> canon = nullptr;

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

/// P1, the frontier pilots the server runs (docs/frontier.md): a habit
/// each, and what one is doing now (the map's tooltip).
enum class FrontierHabit : uint8_t { miner, builder, defender, wanderer };
enum class FrontierDoing : uint8_t { coming, mining, hauling, pumping, buying, building, patrolling, fighting, repairing, walking, flying,
                                     opening, resting, down };
/// "Miner", "Builder", "Defender", "Wanderer"; "mining", "hauling ore", …
AUTOCRAFTNET_API const char* title(FrontierHabit h);
AUTOCRAFTNET_API const char* title(FrontierDoing d);

/// The world's sky (docs/mmorpg-m6.md, C1): its clock and its weather, the
/// same in every region of a world. The client's sky draws it: the hour from
/// `WorldClock::hourAt`, the weather from `Weather(weatherSeed)` sampled at
/// `time` and held at `weatherHold`, as the server decides it.
struct Sky {
    /// World seconds since the world began (saved with the world).
    double time = 0;
    double dayLength = WorldClock::dayLength;
    /// The server's held hour (`--hour`), 0-24.
    std::optional<double> heldHour;
    uint32_t weatherSeed = 0;
    /// The server's held weather (`--weather`): 0 clear .. 3 storm.
    std::optional<double> weatherHold;
    /// The weather's spell now, as the server sampled it.
    WeatherState weather = WeatherState::clear;
    /// A starfall (docs/weather.md "Starfall", E2; Starfall.h), the same for
    /// every pilot: told from `Starfall::warning` before its impact until
    /// its crater has faded and its deposit is empty.
    struct Starfall {
        uint32_t id = 0;
        /// Index in the World message's `regions`, and the impact point on
        /// that region's ground (its map's coordinates).
        int64_t region = 0;
        Vec2 at;
        /// World time it lands (`time`), and the bearing it comes from
        /// (compass degrees, `Starfall::head`).
        double impact = 0;
        double from = 0;
        /// Its deposit, once landed: an index in that region's
        /// `GameState::patches`.
        std::optional<int64_t> patch;
        bool operator==(const Starfall&) const = default;
    };
    std::vector<Starfall> starfalls;
    /// A raid of the Brood on a realm building (docs/creatures.md, "Raids on
    /// buildings"), told from its warning (`Brood` director: 20 s before it
    /// arrives) until it arrives: the client's toast and the swarm's sound
    /// for the pilots in its region and next to it.
    struct Raid {
        uint32_t id = 0;
        /// Index in the World message's `regions`; the building (its id and
        /// kind) and where it stands, and where the swarm was when told
        /// (both that region's ground).
        int64_t region = 0;
        int64_t building = 0;
        StructureKind kind = StructureKind::habDome;
        Vec2 at;
        Vec2 from;
        /// World time it reaches the building (`time`).
        double arrives = 0;
        bool operator==(const Raid&) const = default;
    };
    std::vector<Raid> raids;
    /// The Matriarch's roar (N3): heard by every pilot in earshot, in sight
    /// or not; told for a few seconds after `at`.
    struct Roar {
        uint32_t id = 0;
        int64_t region = 0;
        Vec2 position;
        double at = 0;
        bool operator==(const Roar&) const = default;
    };
    std::vector<Roar> roars;
    /// A canyon zone's game (Z2, docs/canyon.md), the same for every pilot:
    /// its state, its wave, the bugs of it left, its clocks and the prize
    /// taken against the fail mark. The wave panel, the warnings, the map
    /// and the Journal read it.
    struct Zone {
        /// WAITING: wave 1 comes again and again until it is beaten;
        /// RUNNING: the waves, one after another; FAILED: a wave broke
        /// through, the Matriarch's siege; HELD: the canyon held, no waves.
        enum class State : uint8_t { waiting, running, failed, held };
        /// Index in the World message's `regions`, and in that region
        /// file's `zones`.
        int64_t region = 0;
        int64_t zone = 0;
        State state = State::waiting;
        /// The wave out, or the next one (1…`waves`); 0: none yet.
        int64_t wave = 0;
        int64_t waves = 0;
        /// Bugs of the wave out still alive (0: none out).
        int64_t left = 0;
        /// World time (`time`) the next wave comes (WAITING, RUNNING
        /// between waves), or FAILED's and HELD's end. Nil: not counting
        /// (no pilot near, or a wave out).
        std::optional<double> next;
        /// World time the wave out runs out of time: past it, a bug on the
        /// prize fails it. Nil: no wave out.
        std::optional<double> ends;
        /// Ore the wave out has taken from the prize, and the mark that
        /// fails it.
        int64_t taken = 0;
        int64_t mark = 0;
        /// The entrances the wave out came from (bit k: the zone's
        /// `entrances[k]`).
        uint32_t tunnels = 0;
        /// No pilot near: the waves wait.
        bool paused = false;
        /// The wave out is a mini-boss wave.
        bool boss = false;
        bool operator==(const Zone&) const = default;
    };
    std::vector<Zone> zones;
    /// P1: the frontier pilots come (docs/frontier.md): where each is (a
    /// region of the World message, region cells from its north-west
    /// corner; nil region: on its way), its Prospector there (0: none, its
    /// work booked), its level and what it is doing. Their names and
    /// paints are in the Progress message's roster (`id`).
    struct Frontier {
        uint32_t id = 0;
        std::optional<int64_t> region;
        Vec2 at;
        int64_t unit = 0;
        int64_t level = 1;
        FrontierDoing doing = FrontierDoing::coming;
        bool operator==(const Frontier&) const = default;
    };
    std::vector<Frontier> frontier;
    /// W5: the human pilots on line (docs/mmorpg-play.md "Two pilots"):
    /// each one's callsign ("Pilot 2B3C"), its region (of the World message)
    /// and the Prospector it drives there (0: none, aboard the Dropship).
    /// A client tags the others' as it tags the frontier's.
    struct Human {
        std::string callsign;
        int64_t region = 0;
        int64_t unit = 0;
        /// Aboard the Dropship (F2): the flight as the others see it come
        /// down, take it aboard, lift and go, and come in at the target.
        /// `region` is the one it left; the pads in region cells there and
        /// at `to`; seconds since the call when this was sent.
        struct Ferry {
            Vec2 boarded;
            int64_t to = 0;
            Vec2 at;
            ac::Ferry::Plan plan;
            double elapsed = 0;
            bool operator==(const Ferry&) const = default;
        };
        std::optional<Ferry> ferry;
        bool operator==(const Human&) const = default;
    };
    std::vector<Human> pilots;
    double hour() const { return WorldClock::hourAt(time, dayLength, heldHour); }
    bool operator==(const Sky&) const = default;
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
    /// Nil: a region without fog. Shared, never changed once made: a region
    /// gives its side's pilots one between two looks.
    std::shared_ptr<const FogView> fog;
    /// What happened since the pilot's last snapshot that its side may know.
    std::vector<GameEvent> events;
    /// The world's clock and weather (nil: a region of no world, M1: the
    /// client's own clock).
    std::optional<Sky> sky;
    /// A world's pilot's XP in all (`ProspectorProgress::xp`; nil: not a
    /// world). The rest of its progress comes in Progress messages.
    std::optional<double> xp;
    /// C4: the region's walk-in Dropships, whole, in every message's header
    /// (protocol 42): the client's mirror walks in their bays and draws them
    /// at `shuttlePose` (Shuttle.h).
    std::vector<Shuttle> shuttles;
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
/// A look's pitch (C1): 8 bits over −90°…+90°, 0 exact (`kPitchSteps`
/// steps each way, so the code 0..254).
inline constexpr int kPitchBits = 8;
inline constexpr int64_t kPitchSteps = 127;
AUTOCRAFTNET_API uint8_t pitch(double radians);
AUTOCRAFTNET_API double pitch(uint8_t q);
/// Health, shields, energy: 12 bits over 0…`range`, or the exact value
/// past it (one more bit).
inline constexpr int kHealthBits = 12;
/// A walk axis, −1…1: in 255ths, so 0 and ±1 are exact.
inline constexpr int64_t kWalkSteps = 255;
/// A round's points (C2, `Fired::muzzle`, `Impact::at`): 1/64 cell, x and
/// y 22 bits each (±32768 cells), z 16 bits (±512 cells), two's complement.
inline constexpr int64_t kPointSteps = 64;
inline constexpr int kPointBits = 22;
inline constexpr int kPointHeightBits = 16;
/// A round's velocity (`Fired::velocity`): 1/8 cell/s, 12 bits an axis
/// (±256 cells/s; the muzzle speed is `Rules::roundSpeed`, 90).
inline constexpr int64_t kVelocitySteps = 8;
inline constexpr int kVelocityBits = 12;
/// A gun's heat and its barrels' spin (C2): 0…1 in 255ths, 8 bits.
inline constexpr int kShareBits = 8;
} // namespace q

/// What the server applies: the walk, the heading and the pitch as they go
/// on the wire.
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
/// level-up card (not in a world), and a world's skill tree.
///
/// Off a world, the pilot's levels themselves (per kind: level, XP, picks)
/// come in the snapshot's player data: its side's `Player::pilot` is the
/// pilot's own record (the server's `Region::levels`), so the two cards on
/// offer are `PilotRecord::pending`, the core's catalogue, on both ends. In
/// a world the cards are off: the skill tree's state comes in Progress
/// messages, the XP in the snapshots.
struct RequestMessage {
    struct Take {
        int64_t unit = 0;
        bool operator==(const Take&) const = default;
    };
    struct HandBack {
        bool operator==(const HandBack&) const = default;
    };
    /// C4: from the take-off button of the Dropship the driven unit is in
    /// (its ramp shut): fly to Landing Pad `pad` of `region`, paying the
    /// fare (`WorldMap::fly`).
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
    /// A world's skill tree (docs/prospector-progression.md): a point into
    /// `skill`, the last point in it back (spent within the undo window),
    /// every point back (at the pilot's Citadel, for MH unless free), an
    /// active into slot `index` (0 Q, 1 C, 2 G; nil `skill`: empty it).
    struct Spend {
        Skill skill{};
        bool operator==(const Spend&) const = default;
    };
    struct Undo {
        Skill skill{};
        bool operator==(const Undo&) const = default;
    };
    struct Respec {
        bool operator==(const Respec&) const = default;
    };
    struct Slot {
        int64_t index = 0;
        std::optional<Skill> skill;
        bool operator==(const Slot&) const = default;
    };
    /// Drop an open KSTR contract (docs/contracts.md): it costs nothing, and
    /// its slot gets a new contract at the next world dawn.
    struct Abandon {
        uint32_t contract = 0;
        bool operator==(const Abandon&) const = default;
    };
    /// The rig (P7, docs/prospector-progression.md "Foundry and Lab"): the
    /// pilot's Foundry starts making `module` at `mark` (paid from the
    /// wallet), its Lab starts researching `mark` (2 or 3), or `mark` is
    /// fitted from the stores at its Citadel or a Hab Dome (0: take it off).
    struct Make {
        Module module{};
        int64_t mark = 1;
        bool operator==(const Make&) const = default;
    };
    struct Research {
        Module module{};
        int64_t mark = 2;
        bool operator==(const Research&) const = default;
    };
    struct Fit {
        Module module{};
        int64_t mark = 1;
        bool operator==(const Fit&) const = default;
    };
    /// C1: give `ore` and `hydrogen` from the wallet to the realm wallet, at
    /// the realm's Citadel (where it unloads).
    struct Donate {
        int64_t ore = 0;
        int64_t hydrogen = 0;
        bool operator==(const Donate&) const = default;
    };
    /// C2: buy one blueprint copy of `blueprint` (set), or one `item` (no
    /// blueprint), from the wallet, at the realm's Citadel (where it
    /// unloads). The price is the core's (`Shop`).
    struct Buy {
        std::optional<StructureKind> blueprint;
        Item item = Item::repairKit;
        bool operator==(const Buy&) const = default;
    };
    /// C2: use one `item` from the pilot's stacks, in the field.
    struct Use {
        Item item = Item::repairKit;
        bool operator==(const Use&) const = default;
    };
    /// K2: sign up for mission `mission` on the Citadel's board (`take`),
    /// at the realm's Citadel (where it unloads), or drop one from the
    /// Journal, anywhere.
    struct MissionAsk {
        uint32_t mission = 0;
        bool take = true;
        bool operator==(const MissionAsk&) const = default;
    };
    /// D1: the realm's Citadel takes its next mark of `stat` (its corner
    /// turrets' fire rate, damage or range), paid from the realm wallet, at
    /// the realm's Citadel (where it unloads). The price is the core's
    /// (`Defence::citadelPrice`).
    struct UpgradeCitadel {
        DefenceStat stat = DefenceStat::rate;
        bool operator==(const UpgradeCitadel&) const = default;
    };
    /// H2: the pilot's mercenaries (docs/prospector-progression.md
    /// "Mercenaries"): hire one at the realm's Citadel (where it unloads),
    /// dismiss mercenary `merc`, or send it to deposit field `field` of
    /// region `region` (a world region id; `ProgressMessage::MercField`).
    /// N4: `guard` hires a guard (a miner that answers the hunt), and
    /// `role` makes mercenary `merc` a guard or a miner again.
    struct Merc {
        enum class Act : uint8_t { hire, dismiss, assign, role };
        Act act = Act::hire;
        uint32_t merc = 0;
        std::string region;
        int64_t field = 0;
        bool guard = false;
        bool operator==(const Merc&) const = default;
    };
    /// D3: placed turret `turret` (an AA tower or a gatling gun of the
    /// realm, in the pilot's region) takes its next mark of `stat`, paid
    /// from the pilot's own wallet, the pilot within `Defence::turretReach`
    /// of it. The price is the core's (`Defence::turretPrice`).
    struct UpgradeTurret {
        int64_t turret = 0;
        DefenceStat stat = DefenceStat::rate;
        bool operator==(const UpgradeTurret&) const = default;
    };
    /// R2: start the realm project of `kind` (`RealmProject::Kind`), paid
    /// from the realm wallet, at the realm's Citadel (where it unloads).
    struct StartProject {
        uint8_t kind = 0;
        bool operator==(const StartProject&) const = default;
    };
    /// N4: call THE HUNT on the Matriarch, free, at the realm's Citadel
    /// (where it unloads); once a world day realm-wide (WorldHunt.cpp).
    struct CallHunt {
        bool operator==(const CallHunt&) const = default;
    };
    /// C1: open or close the door of the realm's Citadel in the pilot's
    /// region, from either side within `Act::serverReach` of its leaf or panels (C4: it closes
    /// from outside too; docs/citadel-interior.md, section 3).
    struct Door {
        bool open = true;
        bool operator==(const Door&) const = default;
    };
    /// C1: man corner gun `gun` (0 to 3) of that Citadel from its station in
    /// the hall, or -1: leave the gun (docs/citadel-interior.md, section 5).
    struct Man {
        int8_t gun = -1;
        bool operator==(const Man&) const = default;
    };
    /// C2: learn the building `kind` from a blueprint copy the pilot holds,
    /// anywhere; the copy is used up (docs/guns-and-practice.md, section 4).
    struct Learn {
        StructureKind kind = StructureKind::citadel;
        bool operator==(const Learn&) const = default;
    };
    /// C3: demolish building `structure` of the realm in the pilot's region,
    /// within the repair reach of the driven unit, or from the top-down
    /// view; half its price back to the pilot's wallet
    /// (docs/guns-and-practice.md, "Limits per region and demolish").
    struct Demolish {
        int64_t structure = 0;
        bool operator==(const Demolish&) const = default;
    };
    /// C4: buy the Mule at finished realm Foundry `foundry` of the pilot's
    /// region, paid from the pilot's wallet (`Mule::ore`, `Mule::hydrogen`);
    /// it rolls out of the door `Mule::rollOut` s later. One a pilot
    /// (docs/haul-and-ferry.md section 2).
    struct BuyMule {
        int64_t foundry = 0;
        bool operator==(const BuyMule&) const = default;
    };
    /// C4: the pilot's Prospector, on foot with a load, puts it all into
    /// Mule `mule`'s hopper (within `Act::serverReach` of it).
    struct MuleFill {
        int64_t mule = 0;
        bool operator==(const MuleFill&) const = default;
    };
    /// C4: call the walk-in Dropship down onto the realm Landing Pad the
    /// driven unit stands on (docs/haul-and-ferry.md section 3). No
    /// destination and no fare yet.
    struct CallShuttle {
        bool operator==(const CallShuttle&) const = default;
    };
    /// C4: lower (`open`) or shut the ramp of the Dropship the driven unit is
    /// in, at its ramp button (`Simulation::rampRefusal`).
    struct Ramp {
        bool open = true;
        bool operator==(const Ramp&) const = default;
    };
    /// The client's number for it, echoed in the reply.
    uint32_t id = 0;
    /// On the wire its index takes 5 bits (protocol 14; before, 4): Buy is
    /// 14, Use 15, MissionAsk 16 (protocol 16), UpgradeCitadel 17 (protocol
    /// 20; kept for it since 18), Merc 18 (protocol 19), UpgradeTurret 19
    /// (protocol 21), StartProject 20 (protocol 29), CallHunt 21 (protocol 35),
    /// Door 22 and Man 23 (protocol 36), Learn 24 (protocol 37), Demolish 25
    /// (protocol 40), BuyMule 26 and MuleFill 27 (protocol 41; the Dropship's
    /// CallShuttle 28 and Ramp 29, protocol 42); 5 bits hold 32. C4: Take on a Mule
    /// climbs aboard it (a world), Take on the pilot's own Prospector or
    /// HandBack while driving it climbs off.
    std::variant<Take, HandBack, Fly, Build, Pick, Spend, Undo, Respec, Slot, Abandon, Make, Research, Fit, Donate, Buy, Use,
                 MissionAsk, UpgradeCitadel, Merc, UpgradeTurret, StartProject, CallHunt, Door, Man, Learn, Demolish, BuyMule,
                 MuleFill, CallShuttle, Ramp>
        what;
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

/// A KSTR contract (docs/contracts.md): a short job the radio offers a
/// pilot, paid in XP and MH when `have` reaches `need`.
struct Contract {
    /// deliverOre, deliverHydrogen: unload that much (ore, MH) at the
    /// pilot's own buildings. drill: ore loads drilled in region `target`.
    /// reach: stand at `target` ("<region id>/<site name>" a blueprint site,
    /// "<region id>/pad/<pad name>" a pad). build: finish a building of
    /// kind `target` (its code name, "derrick"). sweep: drill under a
    /// ringshadow sweep (drawn once the server has the sweep, E1). skill:
    /// spend a skill point (the first ten minutes' last step).
    enum class Kind : uint8_t { deliverOre, deliverHydrogen, drill, reach, build, sweep, skill };
    /// open; done (paid, waits for dawn); abandoned (waits for dawn).
    enum class State : uint8_t { open, done, abandoned };
    /// The pilot's own number for it (never reused).
    uint32_t id = 0;
    Kind kind = Kind::deliverOre;
    State state = State::open;
    /// The first ten minutes' step it is (docs/contracts.md, "First ten
    /// minutes"), 1…6; 0: a daily contract.
    uint8_t step = 0;
    /// "STARDUST RUN"-style title and the radio's one-line pitch, ready to
    /// show.
    std::string title;
    std::string pitch;
    std::string target;
    /// The region it points at (empty: anywhere), for the world map.
    std::string region;
    int64_t need = 1;
    int64_t have = 0;
    /// What it pays when done.
    double xp = 0;
    int64_t hydrogen = 0;
    bool operator==(const Contract&) const = default;
};

/// L1: KSTR's next goal after the first ten minutes (docs/contracts.md,
/// "What next"): one at a time, chosen by the pilot's level and what it has
/// done, paid a little when done; it never blocks anything.
struct Goal {
    /// Its code name ("gatling"); "level" while the next one waits for a
    /// level (`level`).
    std::string key;
    /// Ready to show: the title, the radio's line, why it matters (the
    /// Journal), the HUD's short line ("B  BUILD A GATLING GUN BY YOUR ORE").
    std::string title;
    std::string pitch;
    std::string why;
    std::string hint;
    /// Its marker (`marked`): region `region` (a world region id), ground
    /// point `at` there.
    bool marked = false;
    std::string region;
    Vec2 at{0, 0};
    /// The level it opens at (a wait: the level waited for).
    int64_t level = 1;
    /// What it pays when done (a wait: nothing).
    double xp = 0;
    int64_t hydrogen = 0;
    bool operator==(const Goal&) const = default;
};

/// K2: a Citadel mission (docs/missions.md): a job on the realm's Citadel's
/// board, in three tiers, that the pilot signs up for and follows in its
/// Journal, paid in XP, ore and MH (hard and perilous: a blueprint copy or
/// supplies) when `have` reaches `need`.
struct CitadelMission {
    /// haul: unload `need` of `target` ("ore", "hydrogen") at the realm's
    /// buildings. survey: stand within `radius` of `at` in region `target`
    /// for `need` seconds (the scan). salvage: open blueprint site `target`
    /// ("<region id>/<site name>"). hold: stand within `radius` of `at` in
    /// region `target` for `need` seconds under the ringshadow band. hunt:
    /// kill `need` of the Brood in region `target` (N2).
    enum class Kind : uint8_t { haul, survey, salvage, hold, hunt };
    enum class Tier : uint8_t { easy, hard, perilous };
    /// offered (on the board), taken (in the Journal), done (paid; it leaves
    /// the Journal when the board is next drawn).
    enum class State : uint8_t { offered, taken, done };
    /// The pilot's own number for it (never reused).
    uint32_t id = 0;
    Kind kind = Kind::haul;
    Tier tier = Tier::easy;
    State state = State::offered;
    /// Its template ("survey-hard-2"): the narration the game plays
    /// (`Sounds/missions/<key>.wav`).
    std::string key;
    /// Ready to show: the title, the officer's briefing, what to do.
    std::string title;
    std::string briefing;
    std::string instructions;
    std::string target;
    /// Its place on the map: region `region` (a world region id; empty:
    /// none), ground point `at`, `radius` cells (0: a point).
    std::string region;
    Vec2 at{0, 0};
    double radius = 0;
    int64_t need = 1;
    int64_t have = 0;
    /// What it pays: XP, ore and MH into the wallet, and `count` copies of
    /// `blueprint`'s blueprint or `count` of `item`.
    double xp = 0;
    int64_t ore = 0;
    int64_t hydrogen = 0;
    std::optional<StructureKind> blueprint;
    std::optional<Item> item;
    int64_t count = 0;
    bool operator==(const CitadelMission&) const = default;
};

/// Server → client, reliable: a world's pilot's progression
/// (docs/prospector-progression.md), whole, on every change but its XP
/// alone (the snapshots carry that): its level, points, ranks, slots, when
/// its actives are ready, its respec price, blueprints and sites, and what
/// just happened (`news`).
struct ProgressMessage {
    /// What happened: a level reached (`value`), a region discovered or a pad
    /// landed on for the first time (`id`), a feat done (`id`), a blueprint
    /// or a survey chip found (P4), the skills reset by an old save's
    /// conversion (`value`: the points to spend), a site opened. `xp`: what
    /// it paid.
    ///
    /// P4b: `blueprint`: `id` the building's code name ("habDome"), `value`
    /// the level it opens at for the pilot. `chip`: `id` the site it marked
    /// ("cold-fork/Fork wreck"), "blueprint" when the chip was a blueprint
    /// itself (its own `blueprint` news follows), or empty (nothing left to
    /// mark); `value` the chips found in all. `site`: `id` the site opened;
    /// `value` 0 (C2: every site gives a blueprint copy; before, 1 told of
    /// ore and MH given for a blueprint known already).
    ///
    /// K1: `contract`: a contract done, `id` its number, `xp` what it paid,
    /// `value` the MH it paid, `by` its title (U1).
    ///
    /// P7: `made`: the Foundry finished a module, `id` its code name
    /// ("drillHead"), `value` its mark; it waits in the stores. `researched`:
    /// the Lab opened a mark, `id` the module, `value` the mark.
    ///
    /// R1: `built`: a building of the realm finished, told to every pilot in
    /// the world: `id` its kind ("habDome"), `by` its builder's callsign
    /// (the pilot's own `callsign`: it built it), `value` 1 when it stands
    /// in the pilot's region.
    ///
    /// K2: `mission`: a Citadel mission done, `id` its number, `xp` what it
    /// paid, `value` its ore, `by` its title (U1).
    ///
    /// H2: `merc`: `id` "away" (while the pilot was off line its mercenaries
    /// delivered `value` ore to its wallet, after the tax), "left" (`by`
    /// left at dawn: the wallet could not pay its wage, `value` MH), "down"
    /// (`by` fell to the bugs; it walks back and resumes, `value` world
    /// seconds later).
    ///
    /// D3: `upgraded`: a placed turret took a mark, told to every pilot in
    /// its region: `id` its kind and the stat ("gatling/air"), `by` who paid
    /// (the pilot's own `callsign`: it did), `value` the mark.
    /// N3: `matriarch`: "down" to every pilot when she falls (`value`: the
    /// pilots who fought her, `by`: the one who hurt her most); her share to
    /// each of them, "share/<MH>/<blueprint>" (`value`: the ore); "back"
    /// when she returns to her nest.
    ///
    /// L1: `goal`: KSTR's next goal done, `id` its key, `by` its title,
    /// `xp` what it paid, `value` the MH.
    ///
    /// Z2: `canyon`, `by` the zone's name ("The Fork"): "wave/<n>/<MH>"
    /// (and "/<blueprint>" for the last wave) to each pilot who fought a
    /// wave beaten, `xp` and `value` (the ore) its share; "held" to every
    /// pilot when the last wave is beaten (`value`: the pilots who fought
    /// it); "failed" to every pilot when a wave breaks through (`value`:
    /// the wave), "spared" when the Matriarch can't come (she is down).
    ///
    /// P1: `frontier`, to every human on line: "radio" a KSTR radio line
    /// (`by`: the line, `value`: the frontier pilot's id), "deed" a notable
    /// deed (`by`: "Vesper built a Hab Dome in Rust Hollow"), "come" one
    /// landed (`by`: its line).
    ///
    /// R2: `project`, to every human on line: "start/<slug>" a realm project
    /// started (`by`: who), "end/<slug>" one ran out (`RealmProject::slug`).
    ///
    /// S4: `week`, to every human on line when the realm week ends: "top"
    /// (`by`: its top hauler's callsign, `value`: the ore), "none" (nobody
    /// hauled).
    ///
    /// N4: `hunt`, to every human on line: "called/<region name>" (`by`: the
    /// caller), "gathered/<region name>" (`by`: the caller, `value`: the
    /// party at the ring), "charge/<region name>" (`value`: the party going
    /// in), "won/<region name>" (`by`: her hardest hitter, `value`: the
    /// fighters), "failed/<region name>" (`value`: 1 the party fell, 0 the
    /// time ran out). Hunt.h words them.
    enum class NewsKind : uint8_t { level, region, pad, feat, blueprint, chip, reset, site, contract, made, researched, built, mission,
                                    merc, upgraded, matriarch, goal, canyon, frontier, project, week, hunt };
    struct News {
        NewsKind kind = NewsKind::level;
        std::string id;
        double xp = 0;
        int64_t value = 0;
        std::string by;
        bool operator==(const News&) const = default;
    };
    /// A point that can still be taken back free, until world time `until`
    /// (`Sky::time`).
    struct Undoable {
        Skill skill{};
        double until = 0;
        bool operator==(const Undoable&) const = default;
    };
    double xp = 0;
    int64_t level = 1;
    /// Points waiting to be spent.
    int64_t points = 0;
    std::map<Skill, int64_t> ranks;
    std::array<std::optional<Skill>, Progression::slotCount> slots{};
    /// When each used active is ready again: the region's game time (the
    /// mirror's `state.time`).
    std::map<Skill, double> ready;
    std::vector<Undoable> undo;
    /// MH the next respec costs, and whether it is free.
    int64_t respecPrice = 0;
    bool respecFree = true;
    /// Blueprint copies held, by kind: each is learned once (`Learn`).
    std::map<StructureKind, int64_t> blueprints;
    /// The buildings the pilot has learned (C2, protocol 37): it builds
    /// them for their ore.
    std::set<StructureKind> learned;
    /// Blueprint sites the pilot knows of, and has opened (ids; P4):
    /// "<region id>/<site name>", the region file's `sites`.
    std::set<std::string> sitesKnown;
    std::set<std::string> sitesFound;
    /// A site being opened now: the act key held at a wreck, a cache or a
    /// vault since world time `since` (`Sky::time`), opening at `since +
    /// needs`. `refusal`: why holding does nothing there ("The Lab vault
    /// opens at level 25"); `since` is then 0.
    struct Opening {
        std::string site;
        double since = 0;
        double needs = 0;
        std::string refusal;
        bool operator==(const Opening&) const = default;
    };
    std::optional<Opening> opening;
    /// The pilot's KSTR contracts today (three slots: open, done or
    /// abandoned), and the world time of the next dawn, when the done and
    /// abandoned ones are replaced (docs/contracts.md).
    std::vector<Contract> contracts;
    double contractsDawn = 0;
    /// L1: KSTR's next goal (none: on the first ten minutes, or every goal
    /// done).
    std::optional<Goal> goal;
    /// The pilot's Prospector fell (P6): it comes back at world time `at`
    /// (`Sky::time`) in region `region` (a world region id), beside its
    /// `home` (a Citadel or a Hab Dome; nil: the world's start pad),
    /// carrying `cargo` (MH if `hydrogen`: Second wind). Nil: on its feet.
    struct Respawn {
        double at = 0;
        std::string region;
        std::optional<StructureKind> home;
        int64_t cargo = 0;
        bool hydrogen = false;
        bool operator==(const Respawn&) const = default;
    };
    std::optional<Respawn> respawn;
    /// The pilot's rig (P7): its fitted modules, its stores, the marks
    /// researched, the Foundry's job and the Lab's (`done` in world time,
    /// `Sky::time`).
    Rig rig;
    /// The realm has a finished Foundry, and a finished Lab: a job can start.
    bool foundry = false;
    bool lab = false;
    /// R1: the realm's buildings across the world by kind, finished or not
    /// (the limits are the realm's: the build menu counts these); who built
    /// each building in the pilot's region (id → callsign, none when not
    /// known); and the pilot's own callsign.
    std::map<StructureKind, int64_t> realm;
    std::map<int64_t, std::string> builders;
    std::string callsign;
    /// D3: the bugs each placed turret and Citadel in the pilot's region
    /// has killed (id → kills; none: 0), for the turret card.
    std::map<int64_t, int64_t> turretKills;
    /// C1: ore and MH.
    struct Purse {
        int64_t ore = 0;
        int64_t hydrogen = 0;
        bool operator==(const Purse&) const = default;
    };
    /// The realm wallet (every pilot's tax and donations; D1 spends it on
    /// the Citadel), the tax in percent, and what this pilot has given it
    /// in all: in tax, and donated.
    Purse realmWallet;
    int64_t taxPercent = 0;
    Purse taxed;
    Purse donated;
    /// C2, the Citadel shop: consumables held, when each used one is ready
    /// again (world time, `Sky::time`), and a shield charge running until
    /// (world time; 0: none). (The copies bought per kind went with the
    /// learned blueprints: each is sold once, `learned`.)
    std::map<Item, int64_t> items;
    std::map<Item, double> itemReady;
    double shieldUntil = 0;
    /// D1: the marks the realm's Citadel turrets have (the DEFENCE tab; the
    /// next mark's price is `Defence::citadelPrice`).
    TurretMarks citadelMarks;
    /// A survey flare burning: in region `region` (a world region id),
    /// round region ground point `at`, `radius` cells, until world time
    /// `until`. The map shows the deposits within it.
    struct Flare {
        std::string region;
        Vec2 at{0, 0};
        double radius = 0;
        double until = 0;
        bool operator==(const Flare&) const = default;
    };
    std::optional<Flare> flare;
    /// K2: the Citadel's board today (offers; drawn again at the next dawn,
    /// `contractsDawn`), and the pilot's Journal (taken and done missions).
    std::vector<CitadelMission> board;
    std::vector<CitadelMission> journal;
    /// H2, mercenary prospectors: a deposit field one can be sent to (in a
    /// region with a realm depot to haul to): its region (id and name), its
    /// index there, its centre (region cells), its deposits and the ore
    /// left in them (rounded down to 100).
    struct MercField {
        std::string region;
        std::string regionName;
        int64_t field = 0;
        Vec2 at{0, 0};
        int64_t deposits = 0;
        int64_t ore = 0;
        bool operator==(const MercField&) const = default;
    };
    /// A mercenary of the pilot's: what it does now (working its field,
    /// down until world time `downUntil`, its field mined out, no realm
    /// depot in its region), its field, the ore it has delivered in all
    /// (after the tax), and its unit in its region (0: none now).
    struct Merc {
        enum class State : uint8_t { working, down, empty, noDepot };
        uint32_t id = 0;
        std::string name;
        State state = State::working;
        std::string region;
        int64_t field = 0;
        double downUntil = 0;
        int64_t delivered = 0;
        int64_t unit = 0;
        /// N4: a guard (answers the hunt), and answering one now (on its
        /// way to the ring, at it, in the fight, or back to its field).
        bool guard = false;
        bool hunting = false;
        bool operator==(const Merc&) const = default;
    };
    std::vector<Merc> mercs;
    std::vector<MercField> mercFields;
    /// How many it may hold at its level, the fee for the next one (ore and
    /// MH), the wage each takes at dawn (MH), the ore one delivers an hour
    /// (the pilot's hour of mining over a day), and the level the next
    /// place opens at (0: none more).
    int64_t mercLimit = 0;
    Purse mercFee;
    int64_t mercWage = 0;
    double mercRate = 0;
    int64_t mercNextLevel = 0;
    /// Every mercenary's unit in the pilot's region, any pilot's: id → its
    /// name (the tag reads "MERC · <name>").
    std::map<int64_t, std::string> mercTags;
    /// M2: the world's regions the pilot has been to (ids; the map shows
    /// the rest dark).
    std::set<std::string> regionsSeen;
    /// P1: the frontier pilots come (the roster: their callsigns, habits,
    /// paints; `Sky::frontier` has where they are).
    struct FrontierPilot {
        uint32_t id = 0;
        std::string callsign;
        FrontierHabit habit = FrontierHabit::miner;
        int32_t paint = 2;
        bool operator==(const FrontierPilot&) const = default;
    };
    std::vector<FrontierPilot> frontier;
    /// R2: the realm's projects, one entry a kind (`RealmProject::Kind`, in
    /// order): how many the realm has started (the next one's price is
    /// `RealmProject::price`), and the one running: until world time `ends`
    /// (`Sky::time`; 0: none running), started by `by` (a callsign).
    struct Project {
        int64_t runs = 0;
        double ends = 0;
        std::string by;
        bool operator==(const Project&) const = default;
    };
    std::vector<Project> projects;
    /// X2: what happened while the pilot was off line (`AwayReport.h`), on
    /// its first Progress after it joins; nil after (and for a new pilot).
    std::optional<AwayReport> away;
    /// C4: the pilot's Mule (docs/haul-and-ferry.md section 2): the region
    /// it is in (a world region id), its unit there (0 while it rolls out),
    /// the Foundry it rolls out of, and when it is out (world time, `Sky::
    /// time`; 0 once out). Nil: none (never bought, or destroyed).
    struct Mule {
        std::string region;
        int64_t unit = 0;
        int64_t foundry = 0;
        double readyAt = 0;
        bool operator==(const Mule&) const = default;
    };
    std::optional<Mule> mule;
    std::vector<News> news;
    bool operator==(const ProgressMessage&) const = default;
};

/// Server → client, reliable: the pilot's events since the last, in order
/// (as many messages as they need).
struct EventMessage {
    int64_t serverTick = 0;
    std::vector<GameEvent> events;
    bool operator==(const EventMessage&) const = default;
};

/// What the game's build menu offers in a world until it lists the pilot's
/// blueprints (P5). The server gates by blueprint, floor and limit
/// (`ProspectorProgress::buildRefusal`) and no longer reads this list.
inline constexpr std::array<StructureKind, 2> kWorldBuildKinds{StructureKind::citadel, StructureKind::derrick};
inline constexpr const char* kWorldBuildRefusal = "Only a Citadel or a Derrick can be built here";
inline constexpr bool worldBuilds(StructureKind kind) {
    for (const StructureKind k : kWorldBuildKinds)
        if (k == kind) return true;
    return false;
}

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
        /// A pad the Dropship lands on (F2): a Landing Pad or a Spacedock
        /// the realm built ("Landing Pad 2", "Spacedock"). Nil: the region
        /// file's pad, where new pilots start and arrive, not flown to.
        std::optional<StructureKind> building{};
        bool operator==(const Pad&) const = default;
    };
    struct Region {
        std::string id;
        std::string name;
        MapStyle style = MapStyle::highlands;
        MapSize size = MapSize::medium;
        /// The ground's palette (docs/regions.md, "ground"): the client
        /// paints its region and blends toward the neighbours' at the edges.
        region::Ground ground = region::Ground::plain;
        int64_t column = 0;
        int64_t row = 0;
        std::vector<Pad> pads;
        /// The Matriarch's nest (N3), region cells from the north-west
        /// corner (nil: none).
        std::optional<Vec2> nest{};
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
    /// F2: the Dropship's flight, while the pilot is aboard (`ac::Ferry`).
    struct Ferry {
        /// Index in `regions` of the region it left, and the pad it boarded
        /// at (region cells there); the target pad by name and place.
        int64_t from = 0;
        Vec2 boarded;
        std::string pad;
        Vec2 at;
        ac::Ferry::Plan plan;
        /// Seconds since the call when this message was sent.
        double elapsed = 0;
        bool operator==(const Ferry&) const = default;
    };
    std::optional<Ferry> ferry;
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
AUTOCRAFTNET_API std::vector<uint8_t> encode(const ProgressMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const StandingsMessage& m);
AUTOCRAFTNET_API std::vector<uint8_t> encode(const HuntMessage& m);
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
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, ProgressMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, StandingsMessage& out);
AUTOCRAFTNET_API bool decode(const std::vector<uint8_t>& bytes, HuntMessage& out);
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
