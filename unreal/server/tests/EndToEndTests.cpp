// The real server program (build/autocraft-server, started as a process the
// way the game will) and the headless client on 127.0.0.1, in real time. The
// lag simulator sits at the client's end only. Isolated moments of a few
// seconds; every server is stopped at the end. Run from unreal/server (as
// `make test` does).

#include "test.h"

#include "HeadlessClient.h"
#include "NetLagSimulator.h"
#include "NetLocalServer.h"
#include "NetRunFile.h"
#include "World.h"
#include "WorldStore.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

extern char** environ;

namespace {

using namespace ac;
using namespace ac::server;

const char* kProgram = "./build/autocraft-server";
const std::string kToken = "e2e-token-5f1c";

double clockNow() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

std::string logPath(const char* name) { return std::string("build/test-logs/") + name + ".log"; }

std::string readFile(const std::string& path) {
    std::string out;
    if (FILE* f = std::fopen(path.c_str(), "r")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

/// A child process with its standard input and output on pipes. Stopped
/// when it goes out of scope: a test that fails an ASSERT leaves no server
/// running (it would hold `make test`'s pipe open).
struct Process {
    pid_t pid = -1;
    int in = -1;
    int out = -1;

    Process() = default;
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&& o) noexcept : pid(std::exchange(o.pid, -1)), in(std::exchange(o.in, -1)), out(std::exchange(o.out, -1)) {}
    Process& operator=(Process&& o) noexcept {
        if (this != &o) {
            stop();
            pid = std::exchange(o.pid, -1);
            in = std::exchange(o.in, -1);
            out = std::exchange(o.out, -1);
        }
        return *this;
    }
    ~Process() { stop(); }

    static std::optional<Process> start(const std::vector<std::string>& args) {
        int toChild[2], fromChild[2];
        if (pipe(toChild) != 0 || pipe(fromChild) != 0) return std::nullopt;
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, toChild[0], 0);
        posix_spawn_file_actions_adddup2(&actions, fromChild[1], 1);
        posix_spawn_file_actions_addclose(&actions, toChild[1]);
        posix_spawn_file_actions_addclose(&actions, fromChild[0]);
        std::vector<char*> argv;
        for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        Process p;
        const int rc = posix_spawn(&p.pid, argv[0], &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        close(toChild[0]);
        close(fromChild[1]);
        if (rc != 0) {
            close(toChild[1]);
            close(fromChild[0]);
            return std::nullopt;
        }
        p.in = toChild[1];
        p.out = fromChild[0];
        return p;
    }

    /// The next line of its output within `seconds`.
    std::optional<std::string> line(double seconds) const {
        std::string got;
        const double end = clockNow() + seconds;
        while (clockNow() < end) {
            pollfd fd{out, POLLIN, 0};
            if (poll(&fd, 1, 50) <= 0) continue;
            char c;
            const ssize_t n = read(out, &c, 1);
            if (n <= 0) return std::nullopt;
            if (c == '\n') return got;
            got.push_back(c);
        }
        return std::nullopt;
    }

    void write(const std::string& s) const { (void)!::write(in, s.data(), s.size()); }

    /// Its exit status if it ends within `seconds`.
    std::optional<int> wait(double seconds) {
        const double end = clockNow() + seconds;
        while (clockNow() < end) {
            int status = 0;
            if (waitpid(pid, &status, WNOHANG) == pid) {
                pid = -1;
                return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return std::nullopt;
    }

    void stop() {
        if (pid > 0) {
            kill(pid, SIGTERM);
            if (!wait(3)) {
                kill(pid, SIGKILL);
                wait(1);
            }
        }
        if (in >= 0) close(in);
        if (out >= 0) close(out);
        in = out = -1;
    }
};

/// A server on Open Field (small), its port read, its token sent.
struct Started {
    Process process;
    uint16_t port = 0;
};

std::optional<Started> startServer(const char* name, std::vector<std::string> extra = {}) {
    std::vector<std::string> args = {kProgram, "--map", "openField", "--size", "small", "--log", logPath(name)};
    args.insert(args.end(), extra.begin(), extra.end());
    std::remove(logPath(name).c_str());
    auto p = Process::start(args);
    if (!p) return std::nullopt;
    p->write(kToken + "\n");
    const auto first = p->line(10);
    unsigned port = 0;
    if (!first || std::sscanf(first->c_str(), "PORT %u", &port) != 1 || port == 0) {
        p->stop();
        return std::nullopt;
    }
    return Started{std::move(*p), uint16_t(port)};
}

/// A client on its own UDP socket, behind the lag simulator.
struct Client {
    Client(uint16_t port, const std::string& token, const net::LagSettings& lag) : lagged(socket, 31) {
        socket.open(net::Address::loopback(0));
        lagged.setDefault(lag);
        net::ClientConfig c;
        c.server = net::Address::loopback(port);
        c.token.assign(token.begin(), token.end());
        client.emplace(lagged, c);
    }
    /// Ticks at 60 Hz in real time until `done` or for `seconds`.
    template <class F> bool until(F&& done, double seconds) {
        const double end = clockNow() + seconds;
        double next = clockNow();
        while (clockNow() < end) {
            const double now = clockNow();
            if (now < next) {
                std::this_thread::sleep_for(std::chrono::duration<double>(next - now));
                continue;
            }
            next += 1.0 / 60;
            client->tick(now);
            if (done()) return true;
        }
        return false;
    }
    net::UdpSocket socket;
    net::LagSimulator lagged;
    std::optional<HeadlessClient> client;
};

net::LagSettings rough() {
    net::LagSettings s;
    s.delay = 0.15;
    s.jitter = 0.02;
    s.loss = 0.05;
    return s;
}

} // namespace

TEST(e2e_drive_two_seconds_under_lag) {
    auto server = startServer("drive");
    ASSERT_TRUE(server.has_value());
    Client c(server->port, kToken, rough());
    c.client->connect(clockNow());
    c.client->join(net::newPilotKey());
    HeadlessClient& hc = *c.client;
    ASSERT_TRUE(c.until([&] { return hc.mirror() && !hc.mirror()->simulation().state.units.empty(); }, 10));
    EXPECT_TRUE(hc.mirror()->matches());
    // Our side's Prospector.
    int64_t unit = -1;
    for (const Unit& u : hc.mirror()->simulation().state.units)
        if (u.owner == 0 && u.kind == Unit::Kind::prospector) unit = u.id;
    ASSERT_TRUE(unit >= 0);
    const uint32_t req = hc.take(unit);
    ASSERT_TRUE(c.until([&] { return hc.replies.count(req) > 0 && hc.mirror()->driven() == unit; }, 5));
    EXPECT_TRUE(hc.replies[req].done);
    const Vec2 from = hc.mirror()->unit(unit)->position;
    // Where the ground is open longest, as the mirror's own map says.
    {
        const Simulation& sim = hc.mirror()->simulation();
        const Unit& u = *hc.mirror()->unit(unit);
        double best = -1;
        for (int k = 0; k < 16; k++) {
            const double h = k * 6.283185307179586 / 16;
            double clear = 0;
            while (clear < 12 && sim.pilotCanStand(u, from + Vec2(std::cos(h), std::sin(h)) * (clear + 0.25))) clear += 0.25;
            if (clear > best) {
                best = clear;
                hc.steer.heading = h;
            }
        }
    }
    // Forward 2 s, then stand.
    hc.steer.walk = Vec2(1, 0);
    c.until([] { return false; }, 2);
    hc.steer.walk = Vec2(0, 0);
    const int64_t stopped = hc.clientTick();
    // Until the server has used every input and the snapshots after it are in.
    ASSERT_TRUE(c.until([&] { return hc.mirror()->lastInputTick() >= stopped + 30; }, 5));
    c.until([] { return false; }, 0.7);
    const Unit mirrored = *hc.mirror()->unit(unit);
    const double walked = distance(mirrored.position, from);
    std::printf("  drove %.2f cells in 2 s under 150 ms each way, 5%% loss\n", walked);
    EXPECT_TRUE(walked > 3);

    // Where the server says it is: a second client, no lag, new to the
    // region, so every item comes whole.
    Client o(server->port, kToken, {});
    o.client->connect(clockNow());
    o.client->join(net::newPilotKey());
    ASSERT_TRUE(o.until([&] { return o.client->mirror() && o.client->mirror()->unit(unit) != nullptr; }, 5));
    // A few ticks for c too, so both read the same standing unit.
    const Unit truth = *o.client->mirror()->unit(unit);
    EXPECT_EQ(truth.position.x, mirrored.position.x);
    EXPECT_EQ(truth.position.y, mirrored.position.y);
    EXPECT_EQ(truth.heading, mirrored.heading);
    EXPECT_EQ(truth.hp, mirrored.hp);
    // A pilot that is not first cannot take the driven unit.
    const uint32_t second = o.client->take(unit);
    ASSERT_TRUE(o.until([&] { return o.client->replies.count(second) > 0; }, 3));
    EXPECT_TRUE(!o.client->replies[second].done);
    server->process.stop();
    const std::string log = readFile(logPath("drive"));
    EXPECT_TRUE(log.find("connect pilot") != std::string::npos);
    EXPECT_TRUE(log.find("refuse take") != std::string::npos);
    EXPECT_TRUE(log.find("stop: SIGTERM") != std::string::npos);
}

TEST(e2e_bad_token_refused) {
    auto server = startServer("token");
    ASSERT_TRUE(server.has_value());
    Client c(server->port, "not-the-token", {});
    c.client->connect(clockNow());
    c.client->join(net::newPilotKey());
    ASSERT_TRUE(c.until([&] { return !c.client->disconnectReason().empty(); }, 5));
    EXPECT_TRUE(c.client->disconnectReason() == "bad token");
    EXPECT_TRUE(!c.client->welcome().has_value());
    server->process.stop();
    EXPECT_TRUE(readFile(logPath("token")).find("bad token") != std::string::npos);
}

TEST(e2e_server_exits_when_its_parent_is_gone) {
    auto parent = Process::start({"/bin/sleep", "30"});
    ASSERT_TRUE(parent.has_value());
    auto server = startServer("parent", {"--parent-pid", std::to_string(parent->pid)});
    ASSERT_TRUE(server.has_value());
    // Alive while the parent is.
    EXPECT_TRUE(!server->process.wait(0.8).has_value());
    kill(parent->pid, SIGKILL);
    parent->wait(2);
    parent->stop();
    const auto code = server->process.wait(3);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(*code, 0);
    server->process.stop();
    EXPECT_TRUE(readFile(logPath("parent")).find("is gone") != std::string::npos);
}

TEST(e2e_server_exits_cleanly_on_sigint) {
    auto server = startServer("sigint");
    ASSERT_TRUE(server.has_value());
    kill(server->process.pid, SIGINT);
    const auto code = server->process.wait(3);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(*code, 0);
    server->process.stop();
    EXPECT_TRUE(readFile(logPath("sigint")).find("stop: SIGINT") != std::string::npos);
}

TEST(e2e_local_server_as_the_game_starts_it) {
    // net::LocalServer, as the game's MMORPG mode uses it: the port read,
    // a client in on the token, a clean stop on SIGTERM.
    std::remove(logPath("local").c_str());
    net::LocalServer server;
    ASSERT_TRUE(server.start(kProgram, {"--map", "openField", "--size", "small", "--log", logPath("local")}, kToken));
    const double end = clockNow() + 10;
    while (server.poll() == net::LocalServer::State::starting && clockNow() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(server.state() == net::LocalServer::State::running);
    Client c(server.port(), kToken, {});
    c.client->connect(clockNow());
    c.client->join(net::newPilotKey());
    ASSERT_TRUE(c.until([&] { return c.client->mirror() && !c.client->mirror()->simulation().state.units.empty(); }, 5));
    EXPECT_TRUE(server.poll() == net::LocalServer::State::running);
    EXPECT_TRUE(server.stop(3));
    EXPECT_EQ(server.exitCode(), 0);
    EXPECT_TRUE(readFile(logPath("local")).find("stop: SIGTERM") != std::string::npos);
    // No program there: it says so.
    net::LocalServer missing;
    EXPECT_TRUE(!missing.start("./build/no-such-server", {}, kToken));
    EXPECT_TRUE(missing.error().find("no server program") != std::string::npos);
}

TEST(e2e_world_spawn_and_fly_to_a_neighbour) {
    // The real program on the first world: the pilot starts at the pad in a
    // Prospector with 600 ore; a Landing Pad under it and one in Cold Fork
    // (the dev control's `ferry pad`), the Dropship there (F2), and the
    // client follows it: the region change at the approach, nothing driven,
    // then its Prospector on the pad.
    const std::string control = "build/e2e-world.control";
    std::remove(control.c_str());
    std::vector<std::string> args = {kProgram, "--world", "worlds/first.json", "--dev-hydrogen", "100", "--log", logPath("world"),
                                     "--dev-control", control};
    std::remove(logPath("world").c_str());
    auto p = Process::start(args);
    ASSERT_TRUE(p.has_value());
    p->write(kToken + "\n");
    unsigned port = 0;
    const auto first = p->line(20);
    ASSERT_TRUE(first && std::sscanf(first->c_str(), "PORT %u", &port) == 1 && port > 0);
    Client c(uint16_t(port), kToken, rough());
    HeadlessClient& hc = *c.client;
    hc.connect(clockNow());
    hc.join(net::newPilotKey());
    ASSERT_TRUE(c.until([&] { return hc.mirror() && hc.mirror()->driven() && hc.world(); }, 10));
    EXPECT_TRUE(hc.mirror()->matches());
    EXPECT_TRUE(hc.mirror()->region().wild);
    EXPECT_TRUE(!hc.mirror()->region().regionFile.empty());
    EXPECT_EQ(hc.world()->regions.size(), size_t(9));
    EXPECT_TRUE(hc.world()->regions[size_t(hc.world()->here)].id == "start");
    // The pilots' side and the Brood's (Prospect Landing has a brood level).
    ASSERT_TRUE(c.until([&] { return hc.mirror()->simulation().state.players.size() == 2; }, 3));
    EXPECT_EQ(hc.mirror()->simulation().state.players[0].ore, int64_t(600));
    EXPECT_EQ(hc.mirror()->simulation().state.units.size(), size_t(1));
    EXPECT_TRUE(hc.mirror()->unit(*hc.mirror()->driven())->kind == UnitKind::prospector);
    // A region file's pad is no Landing Pad.
    const uint32_t no = hc.fly("cold-fork", "landing");
    ASSERT_TRUE(c.until([&] { return hc.replies.count(no) > 0; }, 3));
    EXPECT_TRUE(!hc.replies[no].done);
    if (FILE* f = std::fopen(control.c_str(), "wb")) {
        std::fputs("ferry pad here\nferry pad cold-fork\n", f);
        std::fclose(f);
    }
    auto pads = [&](const std::string& id) {
        size_t n = 0;
        for (const auto& g : hc.world()->regions)
            if (g.id == id)
                for (const auto& q : g.pads) n += q.building ? 1 : 0;
        return n;
    };
    ASSERT_TRUE(c.until([&] { return pads("start") == 1 && pads("cold-fork") == 1; }, 5));
    const uint32_t req = hc.fly("cold-fork", "Landing Pad 1");
    ASSERT_TRUE(c.until([&] { return hc.replies.count(req) > 0; }, 3));
    EXPECT_TRUE(hc.replies[req].done);
    ASSERT_TRUE(c.until([&] { return hc.world()->ferry.has_value(); }, 3));
    ASSERT_TRUE(c.until([&] { return hc.visit() == 1; }, 20));
    EXPECT_TRUE(hc.world()->ferry.has_value() && !hc.mirror()->driven());
    ASSERT_TRUE(c.until([&] { return hc.mirror()->driven() && hc.mirror()->unit(*hc.mirror()->driven()); }, 10));
    EXPECT_TRUE(!hc.world()->ferry.has_value());
    EXPECT_TRUE(hc.regionChange()->regionId == "cold-fork");
    EXPECT_TRUE(hc.mirror()->matches());
    EXPECT_EQ(hc.mirror()->simulation().state.players[0].ore, int64_t(600));
    p->stop();
    const std::string log = readFile(logPath("world"));
    EXPECT_TRUE(log.find("start: world first (9 regions)") != std::string::npos);
    EXPECT_TRUE(log.find("flies to cold-fork Landing Pad 1") != std::string::npos);
    EXPECT_TRUE(log.find("to cold-fork") != std::string::npos);
}

TEST(e2e_world_refuses_a_bad_world_file) {
    auto p = Process::start({kProgram, "--world", "worlds/no-such-world.json", "--log", logPath("badworld")});
    ASSERT_TRUE(p.has_value());
    p->write(kToken + "\n");
    const auto code = p->wait(5);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(*code, 2);
    p->stop();
}

// MARK: - The save (--save)

namespace {

/// The real program on the first world with a save file; its port, or nil.
std::optional<Started> startSaving(const char* name, const std::string& save) {
    auto p = Process::start({kProgram, "--world", "worlds/first.json", "--save", save, "--dev-blueprints", "1", "--log", logPath(name)});
    if (!p) return std::nullopt;
    p->write(kToken + "\n");
    unsigned port = 0;
    const auto first = p->line(20);
    if (!first || std::sscanf(first->c_str(), "PORT %u", &port) != 1 || port == 0) {
        p->stop();
        return std::nullopt;
    }
    return Started{std::move(*p), uint16_t(port)};
}

std::string freshSave(const std::string& name) {
    const std::string path = "build/test-logs/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

const Structure* citadelIn(const net::Mirror& m) {
    for (const Structure& s : m.simulation().state.structures)
        if (s.kind == StructureKind::citadel) return &s;
    return nullptr;
}

} // namespace

TEST(e2e_save_kill_9_mid_play_loses_at_most_5_s) {
    const std::string save = freshSave("kill9");
    std::remove(logPath("kill9").c_str());
    auto server = startSaving("kill9", save);
    ASSERT_TRUE(server.has_value());
    const net::PilotKey key = net::newPilotKey();
    Client c(server->port, kToken, {});
    HeadlessClient& hc = *c.client;
    hc.connect(clockNow());
    hc.join(key);
    ASSERT_TRUE(c.until([&] { return hc.mirror() && hc.mirror()->driven() && hc.mirror()->unit(*hc.mirror()->driven()); }, 10));
    // Off the pad east onto open ground, then a Citadel ahead, whichever way fits.
    hc.steer.heading = 0;
    hc.steer.walk = Vec2(1, 0);
    c.until([] { return false; }, 1.5);
    hc.steer.walk = Vec2(0, 0);
    c.until([] { return false; }, 0.5);
    bool built = false;
    for (int k = 0; k < 8 && !built; k++) {
        hc.steer.heading = k * 6.283185307179586 / 8;
        c.until([] { return false; }, 0.4);
        const Unit& u = *hc.mirror()->unit(*hc.mirror()->driven());
        const Vec2 ahead(std::cos(u.heading), std::sin(u.heading));
        const auto at = hc.mirror()->simulation().snap(StructureKind::citadel, u.position + ahead * Region::buildReach(StructureKind::citadel));
        if (!at) continue;
        const uint32_t req = hc.build(StructureKind::citadel, *at);
        ASSERT_TRUE(c.until([&] { return hc.replies.count(req) > 0; }, 3));
        built = hc.replies[req].done;
    }
    ASSERT_TRUE(built);
    ASSERT_TRUE(c.until([&] { return citadelIn(*hc.mirror()) != nullptr; }, 3));
    // It welds, standing; past one save at least. What the mirror saw, when.
    std::vector<std::pair<double, double>> left;
    c.until([&] {
        if (const Structure* s = citadelIn(*hc.mirror()); s && s->buildLeft) left.push_back({clockNow(), *s->buildLeft});
        return false;
    }, 7);
    ASSERT_TRUE(!left.empty());
    const Unit stood = *hc.mirror()->unit(*hc.mirror()->driven());
    const double killedAt = clockNow();
    kill(server->process.pid, SIGKILL);
    const auto code = server->process.wait(3);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(*code, 128 + SIGKILL);
    server->process.stop();

    // Started again on the same file: the pilot comes back where it stood.
    auto again = startSaving("kill9-again", save);
    ASSERT_TRUE(again.has_value());
    Client d(again->port, kToken, {});
    HeadlessClient& hd = *d.client;
    hd.connect(clockNow());
    hd.join(key);
    ASSERT_TRUE(d.until([&] {
        return hd.mirror() && hd.mirror()->driven() && hd.mirror()->unit(*hd.mirror()->driven()) && citadelIn(*hd.mirror()) &&
               hd.mirror()->simulation().state.players.size() == 2;
    }, 10));
    const Unit back = *hd.mirror()->unit(*hd.mirror()->driven());
    EXPECT_TRUE(hd.world()->regions[size_t(hd.world()->here)].id == "start");
    EXPECT_TRUE(distance(back.position, stood.position) < 0.02);
    EXPECT_EQ(hd.mirror()->simulation().state.players[0].ore, int64_t(200));
    // The Citadel as it was at most 5 s (and a save's tick) before the kill.
    const Structure* s = citadelIn(*hd.mirror());
    ASSERT_TRUE(s->buildLeft.has_value());
    double oldest = left.back().second;
    for (const auto& [t, v] : left)
        if (t >= killedAt - 5.3) oldest = std::max(oldest, v);
    std::printf("  build left at the kill %.2f s, 5 s before %.2f s, restored %.2f s\n", left.back().second, oldest, *s->buildLeft);
    EXPECT_TRUE(*s->buildLeft >= left.back().second - 0.2);
    EXPECT_TRUE(*s->buildLeft <= oldest + 0.01);
    again->process.stop();
    EXPECT_TRUE(readFile(logPath("kill9")).find(": new at the start pad in start") != std::string::npos);
    const std::string log = readFile(logPath("kill9-again"));
    EXPECT_TRUE(log.find("id " + net::pilotKeyShort(key) + ": restored in start") != std::string::npos);
    EXPECT_TRUE(log.find("saved: ") != std::string::npos);
}

TEST(e2e_save_corrupt_or_held_file_stops_the_server_untouched) {
    // Not a database.
    const std::string bad = freshSave("corrupt");
    const std::string junk = "this is not a database, and it stays as it is\n";
    if (FILE* f = std::fopen(bad.c_str(), "wb")) {
        std::fputs(junk.c_str(), f);
        std::fclose(f);
    }
    auto p = Process::start({kProgram, "--world", "worlds/first.json", "--save", bad, "--log", logPath("corrupt")});
    ASSERT_TRUE(p.has_value());
    p->write(kToken + "\n");
    auto code = p->wait(10);
    ASSERT_TRUE(code.has_value());
    EXPECT_TRUE(*code != 0);
    p->stop();
    EXPECT_TRUE(readFile(bad) == junk);
    EXPECT_TRUE(readFile(logPath("corrupt")).find("start failed: save") != std::string::npos);
    // Held by a running server: the second stops, the first carries on.
    const std::string held = freshSave("held");
    auto first = startSaving("held", held);
    ASSERT_TRUE(first.has_value());
    auto second = Process::start({kProgram, "--world", "worlds/first.json", "--save", held, "--log", logPath("held-second")});
    ASSERT_TRUE(second.has_value());
    second->write(kToken + "\n");
    code = second->wait(10);
    ASSERT_TRUE(code.has_value());
    EXPECT_TRUE(*code != 0);
    second->stop();
    EXPECT_TRUE(readFile(logPath("held-second")).find("locked") != std::string::npos);
    EXPECT_TRUE(!first->process.wait(0.5).has_value());
    kill(first->process.pid, SIGTERM);
    code = first->process.wait(5);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(*code, 0);
    first->process.stop();
    std::string why;
    auto store = WorldStore::open(held, "first", &why);
    EXPECT_TRUE(store != nullptr && store->load(&why).has_value());
}

TEST(e2e_local_server_says_why_a_save_did_not_open) {
    // As the game starts it (net::LocalServer): the server's reason, read
    // from its FAILED line, for the game's banner.
    const std::string bad = freshSave("local-corrupt");
    if (FILE* f = std::fopen(bad.c_str(), "wb")) {
        std::fputs("not a database\n", f);
        std::fclose(f);
    }
    net::LocalServer server;
    ASSERT_TRUE(server.start(kProgram, {"--world", "worlds/first.json", "--save", bad, "--log", logPath("local-corrupt")}, kToken));
    const double end = clockNow() + 10;
    while (server.poll() == net::LocalServer::State::starting && clockNow() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(server.state() == net::LocalServer::State::exited);
    EXPECT_EQ(server.exitCode(), 1);
    std::printf("  the reason: %s\n", server.error().c_str());
    EXPECT_TRUE(server.error().rfind("save ", 0) == 0);
    EXPECT_TRUE(server.error().find("exited with code") == std::string::npos);
}

// MARK: - The world runs on (S2)

namespace {

/// A detached server a test started: stopped at the end whatever happens
/// (a test never leaves a server behind).
struct DetachedGuard {
    int pid = -1;
    DetachedGuard() = default;
    DetachedGuard(const DetachedGuard&) = delete;
    DetachedGuard& operator=(const DetachedGuard&) = delete;
    ~DetachedGuard() {
        if (pid <= 0) return;
        kill(pid, SIGTERM);
        for (int i = 0; i < 300 && waitpid(pid, nullptr, WNOHANG) == 0; i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, WNOHANG);
    }
};

/// A server as the game starts one that outlives it: detached, on `save`,
/// let go once it gave its port; what its run file says.
std::optional<net::RunFile> startDetached(const std::string& save, const char* log, DetachedGuard& guard, std::string* why = nullptr) {
    std::remove(logPath(log).c_str());
    net::LocalServer server;
    if (!server.start(kProgram, {"--world", "worlds/first.json", "--save", save, "--log", logPath(log), "--detached"}, kToken, true))
        return std::nullopt;
    guard.pid = server.pid();
    const double end = clockNow() + 20;
    while (server.poll() == net::LocalServer::State::starting && clockNow() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (server.state() != net::LocalServer::State::running) {
        if (why) *why = server.error();
        guard.pid = -1;
        return std::nullopt;
    }
    server.release();
    return net::readRunFile(net::runFilePath(save));
}

/// `autocraft-server --stop <target>`: its exit code and what it printed.
std::pair<int, std::string> stopCommand(const std::string& target) {
    auto p = Process::start({kProgram, "--stop", target});
    if (!p) return {-1, ""};
    const auto line = p->line(20);
    const auto code = p->wait(20);
    p->stop();
    return {code.value_or(-1), line.value_or("")};
}

/// The world server's own clock as a client sees it (its sky).
std::optional<double> skyClock(uint16_t port, const std::string& token) {
    Client c(port, token, {});
    c.client->connect(clockNow());
    c.client->join(net::newPilotKey());
    if (!c.until([&] { return c.client->mirror() && c.client->mirror()->sky(); }, 10)) return std::nullopt;
    return c.client->mirror()->sky()->time;
}

} // namespace

// Detached: the run file says who serves the save; a second start finds it
// live and of its own build and reattaches with its token; it idles with no
// one on line and its clock runs on; `--stop <save>` saves and stops it and
// its run file goes; a second `--stop` finds none.
TEST(e2e_detached_server_reattached_and_stopped) {
    const std::string save = freshSave("runon-detached");
    DetachedGuard guard;
    const auto run = startDetached(save, "runon-detached", guard);
    ASSERT_TRUE(run.has_value());
    EXPECT_EQ(run->pid, guard.pid);
    EXPECT_EQ(run->token, kToken);
    EXPECT_EQ(run->protocol, net::kProtocolVersion);
    EXPECT_EQ(run->build, net::programBuild(kProgram));
    EXPECT_TRUE(net::serverRunning(run->pid));
    const net::Reattach again = net::reattachOrReplace(save, kProgram);
    ASSERT_TRUE(again.what == net::Reattach::Do::reattach);
    EXPECT_EQ(again.server->pid, run->pid);
    const auto first = skyClock(again.server->port, again.server->token);
    ASSERT_TRUE(first.has_value());
    // No one on line: idle, its clock on by the time that passed.
    const double from = clockNow();
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    const auto second = skyClock(again.server->port, again.server->token);
    ASSERT_TRUE(second.has_value());
    const double passed = clockNow() - from;
    std::printf("  world clock %.2f s, then %.2f s, %.2f s later\n", *first, *second, passed);
    EXPECT_TRUE(*second - *first > passed - 0.8 && *second - *first < passed + 0.8);
    const auto [code, said] = stopCommand(save);
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(said.find("stopped the world server pid " + std::to_string(run->pid)) == 0);
    waitpid(run->pid, nullptr, 0);
    guard.pid = -1;
    EXPECT_TRUE(!net::serverRunning(run->pid));
    EXPECT_TRUE(!net::readRunFile(net::runFilePath(save)).has_value());
    const std::string log = readFile(logPath("runon-detached"));
    EXPECT_TRUE(log.find("idle: no pilot on line") != std::string::npos);
    EXPECT_TRUE(log.find("awake: a pilot on line") != std::string::npos);
    EXPECT_TRUE(log.find("stop: SIGTERM") != std::string::npos);
    const auto [none, noneSaid] = stopCommand(save);
    EXPECT_EQ(none, 1);
    EXPECT_TRUE(noneSaid.find("no world server running") == 0);
}

// An older server on the save (another protocol or build): asked to save
// and stop, then a new one starts on the same file; never two on one save.
TEST(e2e_older_server_replaced_never_two_on_a_save) {
    const std::string save = freshSave("runon-older");
    DetachedGuard oldGuard;
    auto old = startDetached(save, "runon-older", oldGuard);
    ASSERT_TRUE(old.has_value());
    // As a server of the protocol before would have written it.
    net::RunFile older = *old;
    older.protocol = uint16_t(net::kProtocolVersion - 1);
    ASSERT_TRUE(net::writeRunFile(net::runFilePath(save), older));
    std::thread reap([pid = old->pid] { waitpid(pid, nullptr, 0); });
    const net::Reattach r = net::reattachOrReplace(save, kProgram);
    reap.join();
    oldGuard.pid = -1;
    EXPECT_TRUE(r.what == net::Reattach::Do::start);
    ASSERT_TRUE(r.replaced.has_value());
    EXPECT_EQ(r.replaced->pid, old->pid);
    EXPECT_TRUE(!net::serverRunning(old->pid));
    EXPECT_TRUE(readFile(logPath("runon-older")).find("stop: SIGTERM") != std::string::npos);
    DetachedGuard newGuard;
    const auto now = startDetached(save, "runon-newer", newGuard);
    ASSERT_TRUE(now.has_value());
    EXPECT_TRUE(now->pid != old->pid);
    // A second on the same save stops at once: the save is held.
    DetachedGuard twoGuard;
    std::string why;
    EXPECT_TRUE(!startDetached(save, "runon-second", twoGuard, &why).has_value());
    EXPECT_TRUE(why.find("locked") != std::string::npos);
    EXPECT_EQ(net::readRunFile(net::runFilePath(save))->pid, now->pid);
    EXPECT_TRUE(net::stopServer(*now, 10));
}
