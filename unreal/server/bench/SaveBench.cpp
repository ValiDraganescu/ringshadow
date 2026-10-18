// `savebench [SAVES] [DIR]`: the server's save, timed (docs/perf.md
// "Server"). The soak's world (bench/SoakSave.cpp: The First with a pilot's
// dev rig, three mercenaries, the Beacon under way, the frontier pilots
// landed) with the pilot on line, its Prospector driven round in a slow
// circle, saved as the server saves it: every 5 s of game time, what
// changed (`WorldMap::save`, then `WorldStore::write`), SAVES times
// (default 40) into a new file in DIR (default build/savebench-files). Prints the
// first write (every row), the spread of the later ones (the world's save,
// the store's write, the rows they changed) and a load of the file.
// Built and run by `make bench` (-O3).

#include "RealmProjects.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

double now() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

double at(std::vector<double> v, double f) {
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(size_t(double(v.size()) * f), v.size() - 1)];
}

double mean(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return v.empty() ? 0 : s / double(v.size());
}

} // namespace

int main(int argc, char** argv) {
    const int saves = argc > 1 ? std::atoi(argv[1]) : 40;
    const std::string dir = argc > 2 ? argv[2] : "build/savebench-files";
    WorldLoad l = loadWorld("worlds/first.json");
    if (!l.world) {
        for (const std::string& p : l.problems) std::fprintf(stderr, "  %s\n", p.c_str());
        return 1;
    }
    WorldParams params;
    params.devRig = true;
    params.firstStep = 0;
    WorldMap w(*l.world, params);
    constexpr PilotId a = 7;
    const std::string key = "5a5e5a5e5a5e5a5e5a5e5a5e5a5e5a5e";
    w.seat(a, key);
    w.step();
    ProspectorProgress g = *w.progress(a);
    g.xp = Progression::xp(30);
    w.setProgress(a, g);
    w.step();
    for (const bool guard : {false, false, true})
        if (const auto why = w.hireMerc(a, guard)) {
            std::fprintf(stderr, "hire: %s\n", why->c_str());
            return 1;
        }
    w.setRealmWallet({50000, 5000});
    if (const auto why = w.startProject(a, RealmProject::Kind::beacon)) {
        std::fprintf(stderr, "project: %s\n", why->c_str());
        return 1;
    }
    auto landed = [&] {
        size_t n = 0;
        for (const auto& f : w.frontier()) n += f.arrived ? 1 : 0;
        return n;
    };
    int64_t tick = 0;
    auto drive = [&](int ticks) {
        for (int i = 0; i < ticks; i++) {
            PilotInput in;
            in.tick = ++tick;
            in.visit = w.visit(a);
            in.walk = Vec2(1, 0);
            in.heading = double(tick) / 60 * 0.4;
            w.input(a, in);
            w.step();
        }
    };
    for (int i = 0; i < 20 * 60 * 60 && landed() < w.frontier().size(); i += 60) drive(60);

    mkdir(dir.c_str(), 0755);
    const std::string path = dir + "/save.sqlite";
    for (const char* end : {"", "-wal", "-shm"}) unlink((path + end).c_str());
    std::string why;
    auto store = WorldStore::open(path, l.world->file.id, &why);
    if (!store) {
        std::fprintf(stderr, "save: %s\n", why.c_str());
        return 1;
    }
    double savedAt = 1.0e9;
    auto saveOnce = [&](double& saveMs, double& writeMs, WorldStore::Written& n) {
        const double t0 = now();
        WorldSave s = w.save();
        s.savedAt = (savedAt += 5);
        const double t1 = now();
        const auto written = store->write(s, &why);
        const double t2 = now();
        if (!written) {
            std::fprintf(stderr, "save: %s\n", why.c_str());
            std::exit(1);
        }
        saveMs = t1 - t0;
        writeMs = t2 - t1;
        n = *written;
    };
    double save0 = 0, write0 = 0;
    WorldStore::Written n0;
    saveOnce(save0, write0, n0);
    std::vector<double> saveMs, writeMs, total, changed, walKb;
    auto walSize = [&] {
        struct stat st {};
        return stat((path + "-wal").c_str(), &st) == 0 ? double(st.st_size) / 1024 : 0.0;
    };
    for (int k = 0; k < saves; k++) {
        drive(5 * 60);
        double s = 0, wr = 0;
        WorldStore::Written n;
        walKb.push_back(walSize());
        saveOnce(s, wr, n);
        saveMs.push_back(s);
        writeMs.push_back(wr);
        total.push_back(s + wr);
        changed.push_back(double(n.changed + n.removed));
    }
    std::vector<size_t> slow(writeMs.size());
    for (size_t i = 0; i < slow.size(); i++) slow[i] = i;
    std::sort(slow.begin(), slow.end(), [&](size_t a, size_t b) { return writeMs[a] > writeMs[b]; });
    std::string slowest;
    for (size_t i = 0; i < std::min<size_t>(3, slow.size()); i++) {
        char line[96];
        std::snprintf(line, sizeof line, "%s save %zu %.3f ms (WAL %.0f KB before, %.0f KB after)", i ? "," : "", slow[i] + 1,
                      writeMs[slow[i]], walKb[slow[i]], slow[i] + 1 < walKb.size() ? walKb[slow[i] + 1] : walSize());
        slowest += line;
    }
    store.reset();
    const double l0 = now();
    auto again = WorldStore::open(path, l.world->file.id, &why);
    const auto loaded = again ? again->load(&why) : std::nullopt;
    const double loadMs = now() - l0;
    struct stat st {};
    stat(path.c_str(), &st);
    std::printf("soak world, pilot on line, mercs %zu, frontier %zu landed, %zu regions\n", w.mercsOf(key).size(), landed(),
                w.regionCount());
    std::printf("  first write: world's save %.3f ms, store's write %.3f ms, %lld rows\n", save0, write0,
                (long long)(n0.changed + n0.removed));
    std::printf("  %d saves, 5 s apart: world's save mean %.3f ms (max %.3f), store's write mean %.3f ms (p50 %.3f, max %.3f)\n",
                saves, mean(saveMs), at(saveMs, 1), mean(writeMs), at(writeMs, 0.5), at(writeMs, 1));
    std::printf("  whole save mean %.3f ms, max %.3f ms; rows changed a save mean %.1f (max %.0f)\n", mean(total), at(total, 1),
                mean(changed), at(changed, 1));
    std::printf("  slowest writes:%s\n", slowest.c_str());
    std::printf("  load: %.3f ms (%s), file %lld KB\n", loadMs, loaded ? "ok" : why.c_str(), (long long)(st.st_size / 1024));
    return 0;
}
