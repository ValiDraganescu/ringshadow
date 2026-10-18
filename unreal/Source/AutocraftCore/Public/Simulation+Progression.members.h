// Members of `class Simulation` for the Prospector's skill tree in the
// MMORPG mode (docs/prospector-progression.md), included inside the class
// body by Simulation.h; the bodies are in Simulation+Progression.cpp. C++
// only, no Swift original.
//
// The region's sim holds the progress of the pilot who drives (`progress`).
// Its skills reach only the driven unit, and only a Prospector, and only
// while it is set. With none (the strategy game, every test before them),
// nothing here changes a number.
// (No include guard: it is a fragment of a class body.)

public:
    // MARK: - The pilot's progress

    /// The driving pilot's progression, set by the server's Region with the
    /// pilot it seats in the driven unit (as it sets the side's
    /// `PilotRecord`). Set it with `setProgress`, which also works out
    /// `skillEffects`.
    std::optional<ProspectorProgress> progress;
    /// What every learned rank of `progress` does (`Progression::effects`),
    /// one rank row after another: `.percent` and `.plus` add up in a Boost.
    std::vector<SkillEffect> skillEffects;
    /// The pilot's own buildings (ids): Reinforced, Hardened works, Emergency
    /// weld, Derrick tender and Wellspring reach these. The server sets the
    /// ones it knows; a building the driven Prospector places while
    /// `progress` is set joins by itself.
    std::set<int64_t> pilotBuildings;

    /// One pilot unit's skill clocks: when each active is ready again and
    /// when a running one ends (game time), what the every-Nth nodes have
    /// counted, and a Quickfill waiting for the next pump.
    struct SkillClock {
        std::map<Skill, double> ready;
        std::map<Skill, double> until;
        std::map<Work, int64_t> counts;
        bool quickfill = false;
        /// A shield charge (C2, `Item::shieldCharge`) runs until this game
        /// time: the unit takes no damage.
        std::optional<double> shield;
        bool operator==(const SkillClock&) const = default;
    };
    /// Kept per pilot unit (its id).
    std::map<int64_t, SkillClock> skillClocks;
    /// Pilot unit `unit` has a shield charge running now.
    bool shielded(int64_t unit) const;
    /// Derrick tender: MH waiting in each of the pilot's Derricks (by id).
    std::map<int64_t, double> derrickTanks;
    /// Wellspring: the part of a whole MH a well (by index) has grown back.
    std::map<int64_t, double> wellGrowth;
    /// Quick regrowth: a deposit (by index) the pilot emptied grows back
    /// after this share of `Rules::regrowDelay`.
    std::map<int64_t, double> regrowShare;
    /// Field pump: the well (by index) the driven Prospector pumps with no
    /// Derrick, by unit id.
    std::map<int64_t, int64_t> fieldPumps;

    /// Second wind: the driven Prospector died with it learned. The server
    /// brings the pilot back `sooner` of the wait sooner, carrying `cargo`
    /// (MH if `hydrogen`), and clears it.
    struct Fallen {
        int64_t unit = 0;
        int64_t cargo = 0;
        bool hydrogen = false;
        double sooner = 0;
        bool operator==(const Fallen&) const = default;
    };
    std::optional<Fallen> fallen;

    /// Set (or clear) the driving pilot's progress and work out its effects.
    void setProgress(const std::optional<ProspectorProgress>& p);

    // MARK: - More humans driving (W5)

    /// C++ only, a world (W5): another human driving in this simulation. The
    /// active driver's context lives in `pilot`, `progress`, `skillEffects`,
    /// `pilotBuildings` and the side's `PilotRecord` (`state.players[
    /// Pilot::player].pilot`); each other driver keeps its own here, and the
    /// step swaps it in for its own unit's turn, its leveling and its
    /// skills, and `boost` reads a unit with its driver's. The single-player
    /// game never has one.
    struct Driver {
        Pilot pilot;
        std::optional<ProspectorProgress> progress;
        std::vector<SkillEffect> skillEffects;
        std::set<int64_t> buildings;
        std::optional<PilotRecord> levels;
        /// Its bank: the side's ore and MH while it is swapped in.
        int64_t ore = 0;
        int64_t hydrogen = 0;
    };
    std::vector<Driver> drivers;
    /// The other driver driving `unit` (an index in `drivers`), if any.
    std::optional<size_t> driverIndex(int64_t unit) const;
    /// Swaps other driver `k`'s context with the active one (twice: back).
    void swapDriver(size_t k);
    /// `unit` taken by one more human: the active driver when none drives,
    /// else another. False as `take` refuses (not the pilots', not drivable,
    /// driven already).
    bool drive(int64_t unit, const std::optional<PilotRecord>& levels = std::nullopt);
    /// The human driving `unit` hands it back to the AI (the next other
    /// driver becomes the active one when it was the active one).
    void handBack(int64_t unit);
    /// The input of the human driving `unit` (nil: none).
    Pilot* pilotFor(int64_t unit);
    /// The progress and the levels of the human driving `unit`, to set and
    /// read (the active's are `progress` and the side's record).
    std::optional<ProspectorProgress>* progressFor(int64_t unit);
    std::optional<PilotRecord>* levelsFor(int64_t unit);
    std::set<int64_t>* buildingsFor(int64_t unit);
    /// `setProgress` for the human driving `unit`.
    void setProgressFor(int64_t unit, const std::optional<ProspectorProgress>& p);
    /// Every unit a human drives (the active one first).
    std::vector<int64_t> drivenUnits() const;
    /// Runs `f` with the context of the human driving `unit` as the active
    /// one (swapped in and back; as it is when it is the active one or none
    /// drives it): the server's asks for a seat (build, items, skills).
    /// The side's ore and MH as the human driving `unit` holds them: what
    /// `asDriver` reads from the side, read in place (an other driver's bank
    /// is its `Driver::ore` and `hydrogen` while another is swapped in).
    std::pair<int64_t, int64_t> bankFor(int64_t unit) const;
    /// Sets them, as `asDriver` setting the side's would.
    void setBankFor(int64_t unit, int64_t ore, int64_t hydrogen);
    template <class F> auto asDriver(int64_t unit, F&& f) -> decltype(f()) {
        const std::optional<size_t> k = (pilot && pilot->unit == unit) ? std::nullopt : driverIndex(unit);
        struct Back {
            Simulation& sim;
            std::optional<size_t> k;
            ~Back() { if (k) sim.swapDriver(*k); }
        } back{*this, k};
        if (k) swapDriver(*k);
        return f();
    }

    /// `progress` is set and `u` is the driven unit of its kind: its skills are on.
    bool skilled(const Unit& u) const;

    /// The first of the skills' effects of case `T`, for the driven
    /// Prospector `u`; nil when it is not skilled or has none.
    template <class T> std::optional<T> skill(const Unit& u) const {
        if (!skilled(u)) return std::nullopt;
        for (const SkillEffect& e : skillEffects) {
            if (const T* t = e.as<T>()) return *t;
        }
        return std::nullopt;
    }

    /// The skills' sum on a number the strategy game's `Stat` lacks, while
    /// `progress` is set (`Boost::none` without it).
    Boost skillBoost(SkillStat stat) const;

    /// The skills' part of `boost(stat, u)` for the driven Prospector `u`:
    /// the `Stat` blocks on `Scope::Driven`, the running actives' channels,
    /// and walking while carrying MH or nothing.
    void addSkills(const Stat& stat, const Unit& u, Boost& b) const;

    // MARK: - Every Nth

    /// The driven Prospector did `work` once more: true on every Nth time
    /// (its node's N), when it counts double. False, and nothing counted,
    /// without the node.
    bool everyNth(Work work, const Unit& u);

    // MARK: - The pilot's buildings

    /// A building the pilot put up (`pilotBuildings`), while `progress` is set.
    bool ownBuilding(const Structure& s) const;
    /// A building's full hp: `Rules::hp`, and Reinforced on the pilot's own.
    double fullHP(const Structure& s) const;

    // MARK: - Work

    /// How far from a building's edge the driven unit welds and repairs
    /// (`pilotHydrogenReach`, and Long arm).
    double workReach(const Unit& u) const;
    /// A load taken from a deposit (or, `hydrogen`, a well) costs it this
    /// much: Clean cut and Cold seal take less than the load.
    int64_t drawn(const Unit& u, int64_t load, bool hydrogen) const;
    /// What a load dropped off counts in the wallet and in XP (Stardust
    /// heart, Silver tide).
    int64_t unloaded(const Unit& u, int64_t load, bool hydrogen) const;

    /// The driven Prospector's drilling ends: its load from deposit `at`
    /// (Rich vein's double, Motherlode's triple, Clean cut), and the
    /// deposit, if it ran dry, grows back after Quick regrowth's share.
    void takeOre(Unit& u, int64_t at, std::vector<GameEvent>& events);
    /// A pump ends: its load from well `g` (Twin canister, Cold seal).
    void pumpLoad(Unit& u, Well& g);
    /// Quickfill is waiting for this pump: true once, and it is used.
    bool takeQuickfill(const Unit& u);

    /// Field pump: a well with MH and no Derrick on it, close and ahead.
    std::optional<int64_t> fieldWell(const Unit& u) const;
    /// Pumping at a well with no Derrick while the key is held; true while it does.
    bool fieldPumpBusy(Unit& u, const Pilot& p, double dt);
    void fieldPumpStart(Unit& u, int64_t well);
    void fieldPumpStop(Unit& u);

    /// Derrick tender: going into one of the pilot's Derricks collects its
    /// tank into the wallet (no XP).
    void collectTank(const Unit& u, int64_t derrick);

    // MARK: - Actives

    /// The active in slot `slot` (0 Q, 1 C, 2 G) of the driven Prospector,
    /// if one is there.
    std::optional<Skill> slotted(const Unit& u, int64_t slot) const;
    /// Why the active `s` can't fire for `u` now ("Ready in 12 s", "Needs
    /// 10 MH", nothing in reach), or nil.
    std::optional<std::string> activeRefusal(const Unit& u, Skill s) const;
    /// When the active `s` of pilot unit `unit` is ready again, game time
    /// (nil: it has not been used).
    std::optional<double> activeReady(int64_t unit, Skill s) const;
    /// The active `s` of pilot unit `unit` is running now (a burst).
    bool activeRunning(int64_t unit, Skill s) const;
    /// Seismic charge: the nearest deposit with ore within `radius` cells.
    std::optional<int64_t> seismicDeposit(const Unit& u, double radius) const;
    /// Emergency weld: the nearest hurt finished building of the pilot's
    /// within `radius` cells of its edge.
    std::optional<int64_t> weldTarget(const Unit& u, double radius) const;
    /// Pulse mine from a slot: `throwGrenade` with the node's grenade (its
    /// cooldown is the slot's, not the ability key's).
    void throwGrenade(Unit& u, const Grenade& g, std::vector<GameEvent>& events);
    /// The slot key `slot` went down: its active fires if it may. True when
    /// it did.
    bool useSlot(Unit& u, int64_t slot, std::vector<GameEvent>& events);

    // MARK: - Upkeep

    /// Once a step while `progress` is set: the pilot's Derricks fill their
    /// tanks, and wells under them grow back.
    void stepSkills(double dt);
