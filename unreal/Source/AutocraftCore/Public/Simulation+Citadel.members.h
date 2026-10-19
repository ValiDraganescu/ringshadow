// Members of `class Simulation` for the Citadel's interior (C1,
// docs/citadel-interior.md): the door, the shelter, the manned corner guns.
// Included inside the class body by Simulation.h. Bodies in
// Private/Simulation+Citadel.cpp. (No include guard: it is a fragment of a
// class body.)

public:
    /// `u` is safe from everything: at a corner gun (`atGun`), or standing
    /// in the hall (`Footprint::inside`) of a complete Citadel of its side.
    /// The doorway is outside. Nothing targets a sheltered unit, and a hit
    /// on it does nothing.
    bool sheltered(const Unit& u) const;
    /// A Citadel's door is open enough to walk through (`Door::open` 0.95).
    static bool doorOpen(const Structure& s);
    /// `at` (a map doodad's position, a crate's) lies within a complete
    /// Citadel's bounding square grown by 1, with the hall on: a saved
    /// world's Citadel grown over it swallowed it. Such a doodad blocks no
    /// driven unit, and the game hides it.
    bool swallowedByCitadel(Vec2 at) const;
    /// The grid left the cell at `p` out for a map doodad (not a cliff).
    bool swallowedDoodadCell(Vec2 p) const;
    /// `u` stands in a complete Citadel's hall or its doorway, or at a gun:
    /// `refreshNav` leaves it where it is and it is never trapped there,
    /// though the grid blocks the whole square for everyone else.
    bool standsInHall(const Unit& u) const;
    /// Why unit `u` can't open (`open`) or close the door of `s` now, or nil:
    /// `s` a complete Citadel; to open, `u` within `Rules::doorReach` of the
    /// door slab (either side); to close, `u` in the hall and no unit in the
    /// doorway (the door slab grown by each unit's radius). The server checks
    /// the rest (the unit complete, the Citadel the realm's).
    std::optional<std::string> doorRefusal(const Structure& s, bool open, const Unit& u) const;
    /// Unit `unit` opens or closes the door of building `structure`; the
    /// refusal (`doorRefusal`, or no such unit or building), or nil once
    /// the door is on its way.
    std::optional<std::string> door(int64_t unit, int64_t structure, bool open);
    /// Unit `unit` takes corner gun `gun` (0 to 3) of the Citadel
    /// `structure`: it must be sheltered, within `Defence::stationReach` of
    /// the gun's station, and the gun free. It then stands at the station
    /// with task `atGun`. The refusal, or nil.
    std::optional<std::string> man(int64_t unit, int64_t structure, int gun);
    /// Unit `unit` leaves the gun it mans (nothing when it mans none).
    void unman(int64_t unit);
    /// The gun unit `u` mans (its Citadel's index into `state.structures`
    /// and the gun), if any.
    std::optional<std::pair<size_t, int>> mannedGun(const Unit& u) const;

    /// Each step: every door moves toward its way (`Rules::doorSeconds`
    /// end to end); a gun whose gunner is dead, gone or no longer at it is
    /// freed; a Citadel just finished gets its door.
    void stepCitadels(double dt);
    /// One corner gun `i` of `s` with weapon `w` (C1, C2): no gunner, it
    /// turns back to rest and never fires; with one, it aims where the
    /// gunner looks (within its arc and pitch) and, on the act or hold key,
    /// spins up and fires rounds along its aim (`minigun`, `fireRound`),
    /// with no range gate.
    void stepCornerGun(Structure& s, int i, const UnitStats& w, double dt, std::vector<GameEvent>& events);

    // Rounds and practice (C2, docs/guns-and-practice.md sections 1 and 3).
    /// Each step, before the units: every round in `state.rounds` flies on
    /// along its parabola (`Rules::roundAt`); the segment it flew is swept
    /// against the units, the buildings and the ground, and the first thing
    /// on it takes the round's damage (`hit`, as a gun hit) with an
    /// `Impact`. A round past `Rules::roundLife` or out of the region is gone.
    void stepRounds(double dt, std::vector<GameEvent>& events);
    /// `side` may aim at it: hostile to `side`, or a complete Target Dummy
    /// that is up (its own side's too). The crosshair, the driven unit's
    /// foe and sight, and the corner guns use it; the auto turrets keep
    /// their hostile-only scan.
    bool targetable(const Unit& u, int64_t side) const;
    bool targetable(const Structure& s, int64_t side) const;
    /// The ground's height at `p` (0 without a terrain), the datum of a
    /// round's `z`.
    double groundZ(Vec2 p) const;
    /// No ground between `lo` and `hi` (seen from above) stands higher than
    /// this: the highest of the `groundTile`-cell tiles there, each its
    /// samples a cell apart plus `groundSlack`, worked out the first time a
    /// round flies over it. A round above it skips the ground's samples.
    double groundTop(Vec2 lo, Vec2 hi) const;
    static constexpr double groundTile = 4.0, groundSlack = 1.5;
    mutable std::vector<double> groundTops;
    /// One step of a minigun's barrels and heat with weapon `w`: it cools
    /// (`Rules::gunCooling`), its cone's bloom fades, and with `trigger`
    /// down and not overheated its barrels spin up (`Rules::gunSpinUp`) and
    /// it fires at `w.cooldown` scaled by the spin. `late` gets each round
    /// fired this step, as seconds before the step's end; each heats it
    /// (`Rules::gunHeatPerRound`), and at 1 it is overheated until it has
    /// cooled to `Rules::gunHeatResume`. `cooldown`: the share of a round
    /// still due before the next leaves (1 just after one left; 0 when the
    /// trigger is up: the next pull fires at once).
    void minigun(double& cooldown, double& heat, bool& overheated, double& spin, double& bloom, const UnitStats& w,
                 bool trigger, double dt, std::vector<double>& late) const;
    /// A round leaves `s`'s muzzle at `muzzle` along `yaw` and `pitch`,
    /// turned by the cone roll inside `cone` (radians), fired `late`
    /// seconds before the step's end: into `state.rounds`, and `Fired`.
    void fireRound(const Structure& s, Vec3 muzzle, double yaw, double pitch, double cone, double damage, double late,
                   std::vector<GameEvent>& events);
    /// Round `r` strikes unit or building `id`: its damage less the armour
    /// (`hit`), a bug's hit told to the director (`bugHits`).
    void strike(const Round& r, int64_t id);
    /// A gatling gun (C2): it keeps or picks a target within its reach
    /// that its arc can reach over the ground (`gunTarget`, `gatlingPitch`),
    /// turns after where the target will be when a round
    /// gets there, and fires rounds on the ballistic pitch (`Rules::
    /// ballisticPitch`) once it points within `gatlingAimGate`.
    void stepGatling(Structure& s, const UnitStats& w, double dt, std::vector<GameEvent>& events);
    /// The pitch a gatling gun `s` fires at to land on `at`, `z` up (the
    /// low arc), if its arc clears the ground all the way to the target's
    /// edge `radius` short of it; nil when out of reach or the ground is in
    /// the way (a gun on a ledge, a bug below its lip).
    std::optional<double> gatlingPitch(const Structure& s, Vec2 at, double z, double radius) const;
    /// The height every weapon aims at on unit or building `id`, now: its
    /// body's middle (`Unit::bodyHeight`: a flyer at its height, a diving
    /// Gloamwing lower), a building's half height.
    double aimHeight(int64_t id) const;
    static constexpr double gatlingAimGate = 0.05;
    /// A Target Dummy whose time down is over stands again with full hp and
    /// no hits counted.
    void stepDummies();
