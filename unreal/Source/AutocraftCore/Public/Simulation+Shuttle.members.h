// Members of `class Simulation` for the walk-in Dropship (C4,
// docs/haul-and-ferry.md section 3; `Shuttle`, Shuttle.h). Included inside
// the class body by Simulation.h. Bodies in Private/Simulation+Shuttle.cpp.
// (No include guard: it is a fragment of a class body.)

public:
    /// A Dropship comes down onto ground point `at` (a Landing Pad's
    /// centre) facing `heading`: `descend`, then on the pad with its ramp
    /// lowering. Its id.
    int64_t callShuttle(Vec2 at, double heading);
    /// The Dropship with id `id` (nil: none here).
    Shuttle* shuttle(int64_t id);
    const Shuttle* shuttle(int64_t id) const;
    /// The Dropship unit `u` is in: riding one in flight, or standing in a
    /// grounded one's bay or on its ramp (nil: none).
    const Shuttle* aboard(const Unit& u) const;
    /// Why unit `u` can't work the ramp of `s` (`open`: down; else shut)
    /// now, or nil: within `Act::serverReach` of the ramp button's box, inside;
    /// on the ground; to shut it, nobody on the ramp.
    std::optional<std::string> rampRefusal(const Shuttle& s, const Unit& u, bool open) const;
    /// Unit `unit` works the ramp of Dropship `shuttle`: the refusal, or nil
    /// once the ramp is on its way.
    std::optional<std::string> ramp(int64_t unit, int64_t shuttle, bool open);
    /// Why unit `u` can't press the take-off button of `s` now, or nil:
    /// within `Act::serverReach` of its box, inside; on the ground; the ramp
    /// shut ("Close the ramp first").
    std::optional<std::string> takeOffRefusal(const Shuttle& s, const Unit& u) const;
    /// Dropship `shuttle` lifts off for the flight `plan` from its pad
    /// (`from`, its ground point here) to `to` (the target pad's ground
    /// point in this region's frame; `hop`: it is in this region). Everyone
    /// in the bay rides along. The refusal (`takeOffRefusal` for `unit`),
    /// or nil.
    std::optional<std::string> takeOff(int64_t unit, int64_t shuttle, const Ferry::Plan& plan, Vec3 to, bool hop);
    /// The flight's frame moves to the target pad: `at` facing `heading`,
    /// its ground `ground`, with its riders where they stand in the bay.
    void moveShuttle(Shuttle& s, Vec2 at, double heading, double ground);
    /// Units of `s`'s hull (and its ramp's strip) out to its nearest side
    /// that stays on the region's ground, all but its riders (the touch
    /// down).
    void clearShuttlePad(const Shuttle& s);

    /// Each step: the stages' clocks, the ramps, the riders, a hop's frame
    /// moved at its approach, the touch downs, the empty ships leaving and
    /// gone.
    void stepShuttles(double dt);

    /// For `pilotCanStand`: what the Dropships say to unit `u` at `p` with
    /// radius `r`: `blocked` by a solid of one (a rider in flight: anywhere
    /// out of its bay), `deck` in a bay or on a ramp that is down (the ground
    /// under it does not matter), or `none` of their business.
    enum class ShuttleWalk : uint8_t { none, blocked, deck };
    ShuttleWalk shuttleWalk(const Unit& u, Vec2 p, double r) const;
    /// The solid of a Dropship blocking `u` at `to` seen from `from` (its
    /// nearest point to `from`, region frame); nil: none.
    std::optional<Vec2> shuttleContact(const Unit& u, Vec2 from, Vec2 to, double r) const;
