// A Citadel with its hall and a driven Prospector, and bugs held still
// (C1, C2): the scenes of CitadelTests.cpp and GunsTests.cpp.
#pragma once

#include "Brood.h"
#include "CitadelHall.h"
#include "Defence.h"
#include "SimHelpers.h"

namespace simtest {

using namespace ac;

/// A complete Citadel of blue's at `at` (the hall on), with no map under it,
/// blue's Prospector at `pilotAt` driven by hand; player 1 is the Brood.
struct Hall {
    Simulation sim;
    int64_t citadel;
    int64_t pilot;
};

constexpr Vec2 kAt{50, 50};

inline Hall hallScene(Vec2 pilotAt, std::optional<MapDefinition> map = std::nullopt, Vec2 at = kAt) {
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t c = addStructure(s, Structure::Kind::citadel, at, 0);
    const int64_t p = addUnit(s, Unit::Kind::prospector, pilotAt, 0);
    Simulation sim(s, map);
    sim.broodSide = 1;
    EXPECT_TRUE(sim.take(p));
    sim.step(1.0 / 30);
    return Hall{sim, c, p};
}

inline Unit& unitOf(Simulation& sim, int64_t id) { return sim.state.units[size_t(*sim.unitIndex(id))]; }

inline Structure& structureOf(Simulation& sim, int64_t id) {
    for (Structure& s : sim.state.structures) if (s.id == id) return s;
    return sim.state.structures.front();
}

/// A bug of the Brood's at `p` that stays where it is.
inline int64_t bugAt(Simulation& sim, Unit::Kind kind, Vec2 p) {
    const int64_t id = sim.state.nextID++;
    Unit u(id, kind, 1, p, 0, Unit::Task::idle);
    u.bug = Bug{};
    u.bug->site = p;
    u.bug->coming = false;
    u.bug->wanderAt = 1e9;
    sim.state.units.push_back(u);
    sim.reindex();
    return id;
}

/// Steps of 1/30 s with the pilot's keys as they are.
inline std::vector<GameEvent> steps(Simulation& sim, double seconds) { return run(sim, seconds); }

/// Arms the Citadel and puts the pilot on gun `gun` (it walks to the
/// station first: it stands there).
inline void manGun(Hall& h, int gun) {
    structureOf(h.sim, h.citadel).marks = TurretMarks{};
    h.sim.step(1.0 / 30);
    unitOf(h.sim, h.pilot).position = Defence::gunStation(kAt, gun);
    EXPECT_TRUE(!h.sim.man(h.pilot, h.citadel, gun));
}

} // namespace simtest
