// autocraft-server: the MMORPG mode's server (docs/networking.md, "The local
// server"). A world of regions from a world file (docs/worlds.md), or one
// region, today's generated map, behind the protocol on UDP.
//
//   autocraft-server --world <file> | --map <style> --size <size> [--starts n]
//                    [--bind 127.0.0.1] [--port 0] [--parent-pid pid] [--log path]
//                    [--save file]  (a world's save: loaded at start, written every 5 s and on stop)
//                    [--dev-xp xp]  (dev, tests: a new pilot's Prospector earns that much XP at once)
//                    [--dev-blueprints 1]  (dev, tests, shots: a new pilot has every blueprint)
//                    [--dev-//                                           `ferry pad [here|REGION]|seen|go REGION|mh N`, or the Citadel board's:
//                                           `missions here`; Server::devControl) a,b]  (dev, shots: a new pilot has been to these regions)
//                    [--dev-rig 1]  (dev, shots: a new pilot owns a finished Citadel, Foundry and Lab, its rig stocked)
//                    [--dev-hydrogen mh]  (dev, tests, shots: a new pilot's wallet gets that much MH at once)
//                    [--first-step n]  (dev, shots: a new pilot starts the first ten minutes at step n, 1-6; 0 none;
//                                       the default 1, 0 with --dev-rig)
//                    [--hour h]  (dev, shots: hold the world's hour, 0-24)
//                    [--weather clear|cloudy|rain|storm|0-3]  (dev, shots: hold the world's weather)
//                    [--ringshadow s]  (dev, shots: move the world's clock so a ringshadow sweep starts in s seconds)
//                    [--starfall s]  (dev, tests, shots: a starfall lands near the first pilot s seconds after it comes)
//                    [--clock-hour h]  (dev, shots: move the world's clock on to hour h, 0-24; it runs on from there)
//                    [--dev-control file]  (dev, shots: read 4 times a second, then removed; each line moves the
//                                           world's clock at once: `clock T`, `hour H`, `ahead S`, or stages a Brood
//                                           scene: `brood linger|attack|dive|lodeback|raid|clear`, or the Dropship's:
//                                           `ferry pad [here|REGION]|seen|go REGION|mh N`, or the Citadel
//                                           board's: `missions here`; Server::devControl)
//                    [--brood 0|1]  (the Brood's director spawns bugs; on unless 0)
//                    [--frontier n]  (frontier pilots the server runs, 0 to 6; 5 unless set)
//                    [--dev-down s]  (dev, shots: the save counts as written s seconds earlier: the start catches
//                                     the world up on that much more, H2's mercenaries' work with it)
//                    [--detached | --attached]  (S2; attached, the default: it ends with --parent-pid's process;
//                                                detached, with --save: it outlives the game, writes the run file
//                                                beside the save (first.sqlite: first.run, net::RunFile) and removes
//                                                it when it stops)
//   autocraft-server --stop <save | folder>  (S2: stops the server serving that save, or every one in the folder:
//                                              SIGTERM, it saves; exit 0 stopped, 1 none running, 3 still running)
//
// Reads the one-time token as the first line of its standard input, binds,
// prints exactly `PORT <n>` on its standard output, and ticks at 60 Hz until
// SIGTERM or SIGINT, or until the parent process is gone. A save that will
// not open prints `FAILED save <why>` there instead and exits 1. Logs to
// ~/Library/Logs/Autocraft/server.log unless told otherwise. With no client
// on line a world idles (S2): the clock and its timers move on at 2 Hz, the
// regions wait, and a datagram wakes it at once (Server::idle).

#include "NetPacket.h"
#include "NetRunFile.h"
#include "Ringshadow.h"
#include "Server.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <optional>
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

/// `--weather`: a state's name, or a number 0 (clear) … 3 (storm).
std::optional<double> weatherNamed(const std::string& v) {
    const char* names[] = {"clear", "cloudy", "rain", "storm"};
    for (int i = 0; i < 4; i++)
        if (v == names[i]) return double(i);
    char* end = nullptr;
    const double x = std::strtod(v.c_str(), &end);
    if (end == v.c_str() || *end || x < 0 || x > 3) return std::nullopt;
    return x;
}

int usage(const char* why) {
    std::fprintf(stderr,
                 "autocraft-server: %s\nusage: autocraft-server --world <file> | --map <highlands|openField|ridge|badlands> "
                 "--size <small|medium|large|huge> [--starts 2|4|8]; [--bind 127.0.0.1] [--port 0] "
                 "[--parent-pid pid] [--log path] [--save file] [--dev-xp xp] [--dev-blueprints 1] [--dev-seen a,b] [--dev-rig 1] [--brood 0|1] [--frontier n] [--dev-hydrogen mh] [--first-step n] [--hour h] [--weather clear|cloudy|rain|storm] [--ringshadow s] [--starfall s] [--clock-hour h] [--dev-control file] [--dev-down s] [--detached | --attached]\n"
                 "       autocraft-server --stop <save | folder>\n",
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

/// `--stop <save | folder>`: the server serving that save (its run file), or
/// every one with a run file in the folder, asked to save and stop.
int stopCommand(const std::string& target) {
    std::vector<std::string> runs;
    struct stat st {};
    if (stat(target.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        if (DIR* d = opendir(target.c_str())) {
            while (const dirent* e = readdir(d)) {
                const std::string name = e->d_name;
                if (name.size() > 4 && name.compare(name.size() - 4, 4, ".run") == 0) runs.push_back(target + "/" + name);
            }
            closedir(d);
        }
    } else {
        runs.push_back(net::runFilePath(target));
    }
    int stopped = 0, stuck = 0;
    for (const std::string& path : runs) {
        const net::RunCheck check = net::checkRunFile(path);
        if (check.state == net::RunState::stale) std::printf("stale run file %s removed (its server is gone)\n", path.c_str());
        if (check.state != net::RunState::live) continue;
        const net::RunFile& f = *check.file;
        if (net::stopServer(f, 15)) {
            std::printf("stopped the world server pid %d on port %u (save %s)\n", f.pid, unsigned(f.port), f.save.c_str());
            stopped += 1;
        } else {
            std::printf("the world server pid %d (save %s) did not stop in 15 s\n", f.pid, f.save.c_str());
            stuck += 1;
        }
    }
    if (stuck) return 3;
    if (!stopped) {
        std::printf("no world server running for %s\n", target.c_str());
        return 1;
    }
    return 0;
}

std::string absolutePath(const std::string& path) {
    if (!path.empty() && path[0] == '/') return path;
    char here[4096];
    if (!getcwd(here, sizeof here)) return path;
    return std::string(here) + "/" + path;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--stop") return stopCommand(argv[2]);
    std::optional<MapStyle> style;
    std::optional<MapSize> size;
    int64_t starts = 2;
    std::string worldPath;
    std::string savePath;
    std::string devControlPath;
    net::Address bind = net::Address::loopback(0);
    long port = 0;
    pid_t parent = 0;
    double devXp = 0;
    bool devBlueprints = false;
    std::vector<std::string> devSeen;
    bool devRig = false;
    bool brood = true;
    int64_t frontier = 5;
    long long devHydrogen = 0;
    std::optional<int> firstStep;
    std::optional<double> heldHour, weatherHold, ringshadowIn, starfallIn, clockHour;
    bool detached = false;
    double devDown = 0;
    const char* home = std::getenv("HOME");
    std::string logPath = std::string(home ? home : ".") + "/Library/Logs/Autocraft/server.log";
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--detached" || a == "--attached") {
            detached = a == "--detached";
            continue;
        }
        if (a == "--stop") return usage("--stop <save>: on its own");
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
        else if (a == "--dev-control") devControlPath = v;
        else if (a == "--dev-down") devDown = std::strtod(v.c_str(), nullptr);
        else if (a == "--dev-xp") devXp = std::atof(v.c_str());
        else if (a == "--dev-blueprints") devBlueprints = v != "0";
        else if (a == "--dev-rig") devRig = v != "0";
        else if (a == "--dev-seen") {
            for (size_t from = 0; from <= v.size();) {
                const size_t comma = std::min(v.find(',', from), v.size());
                if (comma > from) devSeen.push_back(v.substr(from, comma - from));
                from = comma + 1;
            }
        }
        else if (a == "--brood") brood = v != "0";
        else if (a == "--frontier") {
            frontier = std::atoll(v.c_str());
            if (frontier < 0 || frontier > 6) return usage("--frontier: 0…6");
        }
        else if (a == "--dev-hydrogen") devHydrogen = std::atoll(v.c_str());
        else if (a == "--first-step") {
            firstStep = std::atoi(v.c_str());
            if (*firstStep < 0 || *firstStep > WorldContracts::firstSteps) return usage("--first-step: 0…6");
        }
        else if (a == "--hour") {
            heldHour = std::atof(v.c_str());
            if (*heldHour < 0 || *heldHour > 24) return usage("--hour: 0…24");
        } else if (a == "--ringshadow") {
            ringshadowIn = std::atof(v.c_str());
            if (*ringshadowIn < 0) return usage("--ringshadow: seconds, 0 or more");
        } else if (a == "--starfall") {
            starfallIn = std::atof(v.c_str());
            if (*starfallIn < 0) return usage("--starfall: seconds, 0 or more");
        } else if (a == "--clock-hour") {
            clockHour = std::atof(v.c_str());
            if (*clockHour < 0 || *clockHour > 24) return usage("--clock-hour: 0…24");
        } else if (a == "--weather") {
            weatherHold = weatherNamed(v);
            if (!weatherHold) return usage("--weather: clear, cloudy, rain, storm or 0…3");
        }
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
    if (detached && savePath.empty()) return usage("--detached: only with --save (its run file goes beside it)");
    if (detached && parent > 0) return usage("--detached: not with --parent-pid");
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
    options.worldParams.devBlueprints = devBlueprints;
    options.worldParams.devSeen = devSeen;
    options.worldParams.devRig = devRig;
    options.devDown = devDown;
    // A dev rig's pilot is past the first ten minutes unless told a step.
    options.worldParams.firstStep = firstStep.value_or(devRig ? 0 : 1);
    if (devHydrogen > 0) options.worldParams.startHydrogen = devHydrogen;
    options.worldParams.heldHour = heldHour;
    options.worldParams.weatherHold = weatherHold;
    options.worldParams.devStarfall = starfallIn;
    options.worldParams.brood = brood;
    options.worldParams.frontier = frontier;
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
    // Dev and shots (`--ringshadow S`): the world's clock moved on so a
    // ringshadow sweep starts S seconds from now (Ringshadow.h).
    if (ringshadowIn && server.world().isWorld()) {
        const double now = server.world().clock();
        double start = Ringshadow::next(now + *ringshadowIn).start;
        if (start < now + *ringshadowIn) start = Ringshadow::next(start + Ringshadow::duration).start;
        server.world().setClock(start - *ringshadowIn);
    }
    // Dev and shots (`--clock-hour H`): the world's clock on to hour H.
    if (clockHour && server.world().isWorld())
        server.world().setClock(WorldClock::timeOf(*clockHour, server.world().clock(), server.world().params().dayLength));
    if (server.world().isWorld()) {
        const auto sweep = Ringshadow::next(server.world().clock());
        char line[160];
        std::snprintf(line, sizeof line, "ringshadow: next sweep at clock %.1f s (in %.0f s), %.0f s long", sweep.start,
                      sweep.start - server.world().clock(), Ringshadow::duration);
        logLine(line);
    }
    const std::string what = server.world().isWorld()
                                 ? "world " + server.world().loaded()->file.id + " (" + std::to_string(server.world().regionCount()) + " regions)"
                                 : server.region().description().mapName;
    logLine("start: " + what + " on " + socket.localAddress().toString() +
            (parent > 0 ? ", parent " + std::to_string(parent) : ""));
    if (const auto sky = server.world().sky()) {
        char line[200];
        std::snprintf(line, sizeof line, "sky: clock %.1f s, hour %.2f%s, day %.0f s, weather seed %u%s", sky->time, sky->hour(),
                      sky->heldHour ? " (held)" : "", sky->dayLength, unsigned(sky->weatherSeed),
                      sky->weatherHold ? (" held at " + std::to_string(*sky->weatherHold)).c_str() : "");
        logLine(line);
    }
    struct sigaction sa {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN);

    // S2: detached, the run file beside the save says who serves it (the
    // game reattaches by it), and it outlives what started it.
    std::string runPath;
    if (detached) {
        std::signal(SIGHUP, SIG_IGN);
        runPath = net::runFilePath(absolutePath(savePath));
        net::RunFile run;
        run.pid = int(getpid());
        run.port = socket.localAddress().port;
        run.protocol = net::kProtocolVersion;
        run.build = net::programBuild(net::ownProgram());
        run.token = token;
        run.since = int64_t(std::time(nullptr));
        run.save = absolutePath(savePath);
        std::string runWhy;
        if (!net::writeRunFile(runPath, run, &runWhy)) {
            logLine("start failed: run file " + runWhy);
            std::printf("FAILED run file %s\n", runWhy.c_str());
            std::fflush(stdout);
            return 1;
        }
        logLine("detached: run file " + runPath + ", build " + run.build);
    }
    std::printf("PORT %u\n", unsigned(socket.localAddress().port));
    std::fflush(stdout);

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
            // Dev and shots: the control file's clock moves, once each.
            if (!devControlPath.empty())
                if (FILE* f = std::fopen(devControlPath.c_str(), "rb")) {
                    std::string text;
                    char buf[512];
                    for (size_t got; (got = std::fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, got);
                    std::fclose(f);
                    std::remove(devControlPath.c_str());
                    server.devControl(text);
                }
        }
        // S2: no one on line, a world idles: its clock and timers at 2 Hz,
        // woken at once by a datagram (a pilot connecting).
        if (server.world().isWorld() && options.idleEvery > 0 && server.clients() == 0) {
            server.idle(now);
            socket.wait(std::min(options.idleEvery, std::max(0.0, nextParentCheck - now)));
            start = clockNow();
            n = 0;
            continue;
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
    if (!runPath.empty()) net::removeRunFile(runPath, int(getpid()));
    logLine("stop: " + why);
    if (logFile) std::fclose(logFile);
    return 0;
}
