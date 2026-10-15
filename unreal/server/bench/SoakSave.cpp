// The idle soak's save (Q3, docs/networking.md "Soak"): The First with a
// pilot's dev rig (a Citadel, a Foundry, a Lab), three mercenaries (two
// miners and a guard), the realm's Beacon under way, The Fork held, and the
// pilot off line with its away ledger; the frontier pilots as the world
// runs them. Written to the path given, then served by build/autocraft-server
// with no client on line (unreal/Tools/soak.py).
//
//   build/soak-save <save.sqlite> [days]
//
// With `days`: the world then idles that many world days as the server's
// idle does (0.5 s calls), and each day prints the idle call's mean, the
// starfalls standing, the deposits and the ledgers' rows (Q3's projection
// past the 2 h soak). The save is written before and again after.

#include "RealmProjects.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string>

using namespace ac;
using namespace ac::server;

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::fprintf(stderr, "usage: soak-save <save.sqlite> [days]\n");
        return 2;
    }
    const std::string path = argv[1];
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
    const std::string key = "50a450a450a450a450a450a450a450a4";
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
    // On line until the frontier pilots have landed (a few world minutes).
    auto landed = [&] {
        size_t n = 0;
        for (const auto& f : w.frontier()) n += f.arrived ? 1 : 0;
        return n;
    };
    for (int i = 0; i < 20 * 60 * 60 && landed() < w.frontier().size(); i++) w.step();
    w.leave(a);
    const auto fork = w.regionNamed("cold-fork");
    if (!fork || !w.canyonSend(*fork, "the-fork", 10) || !w.canyonBeat(*fork, "the-fork")) {
        std::fprintf(stderr, "canyon: The Fork not held\n");
        return 1;
    }
    for (int i = 0; i < 120; i++) w.step();
    std::string why;
    auto store = WorldStore::open(path, l.world->file.id, &why);
    if (!store) {
        std::fprintf(stderr, "save: %s\n", why.c_str());
        return 1;
    }
    WorldSave s = w.save();
    s.savedAt = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (!store->write(s, &why)) {
        std::fprintf(stderr, "save: %s\n", why.c_str());
        return 1;
    }
    std::printf("soak save %s: mercs %zu, frontier %zu landed, ledger %zu rows, deposits %lld ore, clock %.0f s\n", path.c_str(),
                w.mercsOf(key).size(), landed(), w.ledgerRows(), (long long)w.depositsOre(), w.clock());
    const int days = argc == 3 ? std::atoi(argv[2]) : 0;
    for (int d = 1; d <= days; d++) {
        const auto t0 = std::chrono::steady_clock::now();
        const int calls = int(w.params().dayLength / 0.5);
        for (int i = 0; i < calls; i++) w.idle(0.5);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        size_t patches = 0;
        for (size_t r = 0; r < w.regionCount(); r++) patches += w.region(r).simulation().state.patches.size();
        std::printf("world day %d: idle call mean %.3f ms, starfalls %zu, deposits %zu (%lld ore), ledger %zu rows\n", d,
                    ms / calls, w.starfalls().size(), patches, (long long)w.depositsOre(), w.ledgerRows());
        std::fflush(stdout);
    }
    if (days > 0) {
        WorldSave after = w.save();
        after.savedAt = s.savedAt;
        if (!store->write(after, &why)) {
            std::fprintf(stderr, "save: %s\n", why.c_str());
            return 1;
        }
    }
    return 0;
}
