// autocraft-server: the MMORPG mode's server (docs/networking.md, "The local
// server"). A world of regions from a world file (docs/worlds.md), or one
// region, today's generated map, behind the protocol on UDP.
//
//   autocraft-server --world <file> | --map <style> --size <size> [--starts n]
//                    [--bind 127.0.0.1] [--port 0] [--parent-pid pid] [--log path]
//                    [--save file]  (a world's save: loaded at start, written every 5 s and on stop)
//                    [--dev-xp xp]  (dev, tests: a new pilot's Prospector earns that much XP at once)
//
// Reads the one-time token as the first line of its standard input, binds,
// prints exactly `PORT <n>` on its standard output, and ticks at 60 Hz until
// SIGTERM or SIGINT, or until the parent process is gone. A save that will
// not open prints `FAILED save <why>` there instead and exits 1. Logs to
// ~/Library/Logs/Autocraft/server.log unless told otherwise.

#include "Server.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

using namespace ac;
using namespace ac::server;

namespace {

volatile std::sig_atomic_t stopSignal = 0;

void onSignal(int signal) { stopSignal = signal; }

double clockNow() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

FILE* logFile = nullptr;

void logLine(const std::string& line) {
    if (!logFile) return;
    timeval tv{};
    gettimeofday(&tv, nullptr);
    std::tm local{};
    const std::time_t secs = tv.tv_sec;
    localtime_r(&secs, &local);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &local);
    std::fprintf(logFile, "%s.%03d %s\n", stamp, int(tv.tv_usec / 1000), line.c_str());
    std::fflush(logFile);
}

/// `mkdir -p` of the folder a file goes in.
void makeFolders(const std::string& file) {
    for (size_t i = 1; i < file.size(); i++) {
        if (file[i] != '/') continue;
        mkdir(file.substr(0, i).c_str(), 0755);
    }
}

bool parseIpv4(const std::string& s, net::Address& out) {
    unsigned a, b, c, d;
    char tail;
    if (std::sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
        return false;
    out = net::Address::ipv4(uint8_t(a), uint8_t(b), uint8_t(c), uint8_t(d), out.port);
    return true;
}

int usage(const char* why) {
    std::fprintf(stderr,
                 "autocraft-server: %s\nusage: autocraft-server --world <file> | --map <highlands|openField|ridge|badlands> "
                 "--size <small|medium|large|huge> [--starts 2|4|8]; [--bind 127.0.0.1] [--port 0] "
                 "[--parent-pid pid] [--log path] [--save file] [--dev-xp xp]\n",
                 why);
    return 2;
}

/// The parent is gone: no such process any more, or (when it was our own
/// parent) we were handed to another.
bool parentGone(pid_t parent, pid_t startParent) {
    if (parent <= 0) return false;
    if (parent == startParent && getppid() != parent) return true;
    return kill(parent, 0) != 0 && errno == ESRCH;
}

} // namespace

int main(int argc, char** argv) {
    std::optional<MapStyle> style;
    std::optional<MapSize> size;
    int64_t starts = 2;
    std::string worldPath;
    std::string savePath;
    net::Address bind = net::Address::loopback(0);
    long port = 0;
    pid_t parent = 0;
    double devXp = 0;
    const char* home = std::getenv("HOME");
    std::string logPath = std::string(home ? home : ".") + "/Library/Logs/Autocraft/server.log";
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (i + 1 >= argc) return usage(("no value for " + a).c_str());
        const std::string v = argv[++i];
        if (a == "--world") worldPath = v;
        else if (a == "--map") style = parse<MapStyle>(v);
        else if (a == "--size") size = parse<MapSize>(v);
        else if (a == "--starts") starts = std::atol(v.c_str());
        else if (a == "--bind") { if (!parseIpv4(v, bind)) return usage("bad --bind"); }
        else if (a == "--port") port = std::atol(v.c_str());
        else if (a == "--parent-pid") parent = pid_t(std::atol(v.c_str()));
        else if (a == "--log") logPath = v;
        else if (a == "--save") savePath = v;
        else if (a == "--dev-xp") devXp = std::atof(v.c_str());
        else return usage(("unknown " + a).c_str());
    }
    std::optional<LoadedWorld> world;
    if (!worldPath.empty()) {
        WorldLoad load = loadWorld(worldPath);
        if (!load.world) {
            for (const std::string& p : load.problems) std::fprintf(stderr, "autocraft-server: %s\n", p.c_str());
            return usage(("--world " + worldPath + ": not a valid world").c_str());
        }
        world = std::move(load.world);
    } else {
        if (!style) return usage("--map: a map style (or --world)");
        if (!size) return usage("--size: a map size");
    }
    if (!savePath.empty() && !world) return usage("--save: only with --world");
    if (starts != 2 && starts != 4 && starts != 8) return usage("--starts: 2, 4 or 8");
    if (port < 0 || port > 65535) return usage("--port: 0…65535");
    bind.port = uint16_t(port);

    makeFolders(logPath);
    logFile = std::fopen(logPath.c_str(), "a");

    // The save: opened (and held) before anything else, loaded before the
    // first tick. One that will not open (not a database, another server
    // has it, a newer schema, another world's) stops the server untouched.
    std::unique_ptr<WorldStore> store;
    std::optional<WorldSave> saved;
    if (!savePath.empty()) {
        makeFolders(savePath);
        std::string why;
        store = WorldStore::open(savePath, world->file.id, &why);
        if (store) saved = store->load(&why);
        if (!store || !saved) {
            logLine("start failed: save " + why);
            std::fprintf(stderr, "autocraft-server: save %s\n", why.c_str());
            // The game reads it (net::LocalServer::error) for its banner.
            std::printf("FAILED save %s\n", why.c_str());
            std::fflush(stdout);
            return 1;
        }
    }

    // The one-time token: the first line of the standard input.
    std::string token;
    for (int c; (c = std::fgetc(stdin)) != EOF && c != '\n';) token.push_back(char(c));
    if (!token.empty() && token.back() == '\r') token.pop_back();

    ServerOptions options;
    if (world) options.world = std::move(world);
    else options.region.map = MapChoice{*style, *size, starts};
    options.worldParams.devXp = devXp;
    options.token.assign(token.begin(), token.end());
    if (!net::secureRandom(options.secret.data(), options.secret.size())) {
        logLine("start failed: no secure random source");
        return 1;
    }
    options.log = logLine;
    options.store = store.get();
    options.saved = std::move(saved);

    net::UdpSocket socket;
    if (!socket.open(bind)) {
        logLine("start failed: bind " + bind.toString() + ": " + socket.error());
        std::fprintf(stderr, "autocraft-server: bind %s: %s\n", bind.toString().c_str(), socket.error().c_str());
        return 1;
    }
    Server server(socket, options);
    const std::string what = server.world().isWorld()
                                 ? "world " + server.world().loaded()->file.id + " (" + std::to_string(server.world().regionCount()) + " regions)"
                                 : server.region().description().mapName;
    logLine("start: " + what + " on " + socket.localAddress().toString() +
            (parent > 0 ? ", parent " + std::to_string(parent) : ""));
    std::printf("PORT %u\n", unsigned(socket.localAddress().port));
    std::fflush(stdout);

    struct sigaction sa {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN);

    // 60 Hz on the steady clock: a tick when its time comes, sleeping
    // between; far behind (a stall), it starts counting again from now.
    const double step = server.region().description().step;
    double start = clockNow();
    int64_t n = 0;
    double nextParentCheck = start;
    const pid_t startParent = getppid();
    std::string why;
    while (!stopSignal) {
        const double now = clockNow();
        if (now >= nextParentCheck) {
            nextParentCheck = now + 0.5;
            if (parentGone(parent, startParent)) {
                why = "parent " + std::to_string(parent) + " is gone";
                break;
            }
        }
        const double due = start + double(n) * step;
        if (now < due) {
            const double wait = std::min(due - now, 0.25);
            timespec ts{};
            ts.tv_sec = time_t(wait);
            ts.tv_nsec = long((wait - double(ts.tv_sec)) * 1e9);
            nanosleep(&ts, nullptr);
            continue;
        }
        server.tick(now);
        n += 1;
        if (now - due > 0.25) {
            logLine("behind by " + std::to_string(int((now - due) * 1000)) + " ms: ticks skipped");
            start = now;
            n = 1;
        }
    }
    if (why.empty()) why = stopSignal == SIGINT ? "SIGINT" : "SIGTERM";
    server.stop(clockNow(), "the server stopped");
    logLine("stop: " + why);
    if (logFile) std::fclose(logFile);
    return 0;
}
