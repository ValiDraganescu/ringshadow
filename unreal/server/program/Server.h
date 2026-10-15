// The server: a world of regions (WorldMap.h), or one generated region (M1),
// behind the protocol (docs/networking.md). A connection is a pilot: it joins
// on connect and leaves on disconnect. Each tick reads what came in, steps
// every region once (1/60 s), tells pilots that changed region, and, every third
// tick (20 Hz), sends each client its events and its snapshot within what is
// left of its byte budget. Polled with the caller's clock, on any PacketIo:
// the program (main.cpp) runs it on a UDP socket, the tests on their own
// network.
#pragma once

#include "NetEndpoint.h"
#include "NetSnapshot.h"
#include "World.h"
#include "WorldMap.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ac::server {

struct ServerOptions {
    /// The one region (M1, `--map`); in a world, the settings every region
    /// starts from.
    RegionConfig region;
    /// A world (`--world`): its regions, wild ground.
    std::optional<LoadedWorld> world;
    WorldParams worldParams;
    /// What a client must answer with (the local server's one-time secret).
    std::vector<uint8_t> token;
    /// The key of the handshake's cookies: random (net::secureRandom).
    std::array<uint8_t, 16> secret{};
    net::ConnectionSettings connection;
    InterestParams interest;
    /// Ticks between snapshots: 3 is 20 Hz.
    int64_t snapshotEvery = 3;
    /// Seconds between stats lines in the log.
    double statsEvery = 10;
    /// One line of the log (no newline).
    std::function<void(const std::string&)> log;
    /// The world's save (`--save`, WorldStore.h): what it held, put in
    /// place before the first tick, and the store written every
    /// `saveEvery` seconds (what changed) and on stop. Null: nothing saved.
    std::optional<WorldSave> saved;
    WorldStore* store = nullptr;
    double saveEvery = 5;
    /// X2: the away summary file beside the save (`net::awayFilePath`),
    /// written on each idle tick and save when it changed. Empty: none.
    std::string awayFile;
    /// The Mac's clock, Unix seconds (null: the system's): each save is
    /// stamped with it (`WorldSave::savedAt`), and a save loaded catches
    /// the world up on the time since (`WorldMap::catchUp`, S2).
    std::function<double()> wallClock;
    /// S2: seconds between `idle` calls with no client on line (the
    /// program's loop; 0: it steps at 60 Hz all the same).
    double idleEvery = 0.5;
    /// Dev and shots (`--dev-down s`): the save counts as written this many
    /// seconds earlier, so the start catches up that much more (H2's
    /// "while you were away" on a run a minute after the last).
    double devDown = 0;
};

class Server {
public:
    Server(net::PacketIo& io, const ServerOptions& options);

    /// One server tick at `now` (seconds, never going back). `stepRegion`
    /// false holds the region still (the tests): the network goes on.
    void tick(double now, bool stepRegion = true);
    /// No client on line (S2): what came in is read, the world's clock and
    /// timers move on by the time since the last call (`WorldMap::idle`; the
    /// regions held still), and it saves as `tick` does. The first call
    /// after ticks counts from `now`.
    void idle(double now);
    /// The seconds the start caught the world up on (`WorldMap::catchUp`).
    double caughtUp() const { return caughtUp_; }
    /// Tells every client the server stops, and sends it; then saves.
    void stop(double now, const std::string& reason);
    /// Writes what changed to the store now (false: no store, or it
    /// failed, which the log says).
    bool save();
    /// X2: writes the away summary file when its text changed (false: no
    /// file, or it failed).
    bool writeAway();
    /// Rows the last save wrote (changed, removed).
    WorldStore::Written lastSave() const { return lastSave_; }

    /// The start region (M1: the one region).
    Region& region() { return world_.start(); }
    const Region& region() const { return world_.start(); }
    WorldMap& world() { return world_; }
    const WorldMap& world() const { return world_; }
    size_t clients() const { return clients_.size(); }

    /// One client's counters.
    struct ClientStats {
        net::ConnectionId connection = 0;
        PilotId pilot = 0;
        uint64_t bytesSent = 0;
        int64_t snapshots = 0;
        /// Snapshot messages sent, all parts counted.
        int64_t snapshotMessages = 0;
        int64_t deferred = 0;
        int64_t fullResets = 0;
    };
    std::vector<ClientStats> clientStats() const;

    /// Dev and shots (`--dev-control FILE`, main.cpp): the lines of the
    /// control file, each one a move of the world's clock, done at once:
    /// `clock T` to T seconds, `hour H` on to the next hour H, `ahead S`
    /// S seconds on; `brood …` a Brood scene near the first pilot
    /// (WorldMap::devBrood). A line a log line each (what it did, or why not).
    std::vector<std::string> devControl(const std::string& text);

private:
    struct Client {
        PilotId pilot = 0;
        /// Its Join came: the pilot is in the world.
        bool seated = false;
        net::SnapshotWriter writer;
        int64_t snapshots = 0;
        int64_t snapshotMessages = 0;
        int64_t deferred = 0;
        uint64_t bytesAtStats = 0;
        /// The progression it was last told (a world).
        std::optional<net::ProgressMessage> told;
    };

    void handle(const net::Event& e, double now);
    void join(net::ConnectionId c, Client& client, const std::vector<uint8_t>& data, double now);
    /// Reliable, or on the bulk channel when too large for one message.
    void sendReliable(net::ConnectionId c, const std::vector<uint8_t>& bytes);
    void sendWorld(net::ConnectionId c, PilotId pilot);
    void sendMoves();
    void message(net::ConnectionId c, Client& client, const std::vector<uint8_t>& data, double now);
    void sendSnapshots(double now);
    /// A skill tree request (nil: not one): done (nil inside), or why not.
    std::optional<std::optional<std::string>> skillTree(const net::RequestMessage& m, PilotId pilot);
    /// The pilot's progression when it changed (more than its XP), with its news.
    void sendProgress(net::ConnectionId c, Client& client);
    void statsLine(double now);
    void log(const std::string& line) const;
    /// A log line for each starfall told and landed.
    void logStarfalls();
    /// `pilot X fell in …` and `pilot X respawns in …`, as they happen.
    void logRespawns();

    ServerOptions options_;
    WorldMap world_;
    net::ServerEndpoint endpoint_;
    std::map<net::ConnectionId, Client> clients_;
    int64_t ticked_ = 0;
    // The tick's time, for the stats line.
    double statsFrom_ = -1;
    double tickSum_ = 0;
    double tickMax_ = 0;
    int64_t ticks_ = 0;
    // The regions' step time (all of them), for the stats line.
    double stepSum_ = 0;
    double stepMax_ = 0;
    double savedAt_ = -1;
    /// X2: the away file's text last written.
    std::optional<std::string> awayWritten_;
    WorldStore::Written lastSave_;
    double wallNow() const;
    double caughtUp_ = 0;
    /// Idle since `idleFrom_` (the world's clock is that far on).
    bool idling_ = false;
    double idleFrom_ = 0;
    /// The starfalls last logged, and whether each had landed.
    std::map<uint32_t, bool> starfalls_;
    std::map<PilotId, bool> respawns_;
};

} // namespace ac::server
