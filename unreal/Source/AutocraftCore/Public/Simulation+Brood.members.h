// Members of `class Simulation` for the Brood (docs/creatures.md), included
// inside the class body by Simulation.h. Bodies in Private/Simulation+Brood.cpp.
// (No include guard: it is a fragment of a class body.)

public:
    /// The Brood's side (a player of its own, hostile to all), when the
    /// server keeps one: no victory is ever declared with it.
    std::optional<int64_t> broodSide;
    /// A hit on a bug (who, on what, how much hp it took, whether it killed
    /// it): the server pays the driven unit's XP from these and turns the
    /// group on the hitter. The server's director clears them once read.
    struct BugHit { int64_t shooter; int64_t bug; UnitKind kind; double hp; bool kill; };
    std::vector<BugHit> bugHits;

    /// One step of a bug by its orders (`Bug`): a scare, a chase, a raid, a
    /// Lodeback's meal or flight, the way in, the way out, the wander.
    void stepBug(Unit& u, double dt, std::vector<GameEvent>& events);

    /// The Matriarch's acid puddles (N3): each does `Matriarch::puddleDps`
    /// a second to every ground foe of `owner` in it until `until`.
    struct Puddle { Vec2 at; double until; int64_t owner; };
    std::vector<Puddle> puddles;
    /// The foes her slam would strike now, in front of her (ids of units and
    /// buildings).
    std::vector<int64_t> slamStrikes(const Unit& u) const;

private:
    /// Closes on `id` and bites (or spits, or dives and spits) when in
    /// reach. False: no such target, or it can't hit it.
    bool bugAttack(Unit& u, int64_t id, double dt);
    /// A Gloamwing's height toward where its state wants it.
    void bugClimb(Unit& u, double dt) const;
    /// A random spot in the bug's site, by its own dice.
    Vec2 bugWanderSpot(const Unit& u) const;
    /// The Matriarch's step (N3): her slam's crash, her roar's pose, her
    /// spit, her slam, closing on her foe within her tether, home, mending.
    void stepMatriarch(Unit& u, double dt);
    /// The puddles' burn, once a step.
    void stepPuddles(double dt);
