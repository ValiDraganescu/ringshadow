// Members of `class Simulation` for the Mule (C4, docs/haul-and-ferry.md
// section 2, `Mule.h`): its rider, its fill at the hopper, its booking in a
// Citadel's hall, its roll-out. Included inside the class body by
// Simulation.h. Bodies in Private/Simulation+Mule.cpp. (No include guard: it
// is a fragment of a class body.)

public:
    /// The Prospector riding Mule `mule` (its `cargo`), if any.
    std::optional<int64_t> muleRider(int64_t mule) const;
    /// The Mule unit `u` rides (aboard, its `structure` a Mule), if any.
    std::optional<int64_t> ridden(const Unit& u) const;
    /// Why Prospector `u` can't climb onto Mule `m` now, or nil: `u` a
    /// Prospector of `m`'s side on foot, `m` a Mule with no one aboard, `u`
    /// within `Mule::reach` of its perch. (Whose Mule it is, the server
    /// checks.)
    std::optional<std::string> muleBoardRefusal(const Unit& u, const Unit& m) const;
    /// Prospector `prospector` climbs onto Mule `mule`: aboard it, on its
    /// perch, riding along. The refusal, or nil. The server then gives its
    /// pilot the Mule to drive.
    std::optional<std::string> muleBoard(int64_t prospector, int64_t mule);
    /// The rider of Mule `mule` climbs off beside its perch (its left, else
    /// its right, else the nearest open ground): its id, nil when no one
    /// rides it.
    std::optional<int64_t> muleLeave(int64_t mule);
    /// Why Prospector `u` can't put its load into Mule `m` now, or nil: `u`
    /// on foot and carrying, within `Mule::reach` of the hopper, the Mule
    /// below `Mule::capacity` (`Mule::full`).
    std::optional<std::string> muleFillRefusal(const Unit& u, const Unit& m) const;
    /// Prospector `prospector` puts its whole load into Mule `mule`'s
    /// hopper (ore or MH): no XP, no wallet. The refusal, or nil.
    std::optional<std::string> muleFill(int64_t prospector, int64_t mule);
    /// A Mule of side `owner` rolls out of Foundry `foundry`'s door
    /// (`Mule::door`, the nearest free ground there): a new unit, its
    /// `Trained` with the next step's events. Its id; nil: no such building.
    std::optional<int64_t> rollOut(int64_t foundry, int64_t owner);
    /// The same at door point `door` (`Mule::door`; the Foundry gone since
    /// it was bought): its id.
    int64_t rollOutAt(Vec2 door, int64_t owner);
    /// Each step after the units: every rider kept on its Mule; a Mule a
    /// human drives with a load, its centre in the hall
    /// (`Footprint::inside`) of a complete Citadel of its side, books the
    /// load to its driver's bank: Stardust heart and Silver tide, the
    /// realm's cut while the driver has its progress (a world), `Booked`.
    void stepMules(std::vector<GameEvent>& events);
    /// A Mule going down sets its rider beside it, on its feet; its load
    /// goes with it.
    void muleDown(const Unit& m);

    /// The driven Prospector `u` mends a Mule as it mends a building: a hurt
    /// Mule of its side within its work reach, ahead (nil: none).
    std::optional<int64_t> hurtMule(const Unit& u) const;
    /// One step of mending Mule `mule` (the key held, standing): a whole bar
    /// takes its roll-out time (`Rules::trainTime`) at the Prospector's
    /// repair rate, and costs `Rules::repairShare` of its price, paid from
    /// the repairer's bank in whole units as the debt crosses them
    /// (`Repaired`). False when it stops (healed, gone, out of reach, the
    /// bank short).
    bool prospectorMendMule(Unit& u, int64_t mule, const Pilot& p, double dt, std::vector<GameEvent>& events);
    /// What each Mule's repairs owe in parts of a unit (not saved).
    std::map<int64_t, Structure::Owed> muleOwed;
