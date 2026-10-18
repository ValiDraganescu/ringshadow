# The Citadel's interior (C1)

Status, 2026-10-18: designed; in work by four workers (core, net+server,
game, model). This page is the contract between them: the names, the
numbers and who owns what. Where the code and this page disagree while the
work runs, the code of the owning worker wins and this page is corrected.

The user's ask, 2026-10-18: buildings should be huge next to the units. The
Prospector gets inside the first one, the Citadel. A door (a ramp) opens on
a key and closes from the inside. Units inside are safe from the Brood. The
Citadel's turrets no longer shoot by themselves: players run inside and
operate each one by hand, first person, the HUD a live feed from the gun.
A turret never turns through the Citadel's walls; to shoot the other way a
player disengages and takes another turret. The first- and third-person
cameras never pass through walls.

## 1. Scale

The units stay as they are (one cell is 1 m, `AcSpace.h`; a Prospector is
1.47 cells tall and 0.75 across). The Citadel grows:

| | Today | C1 |
|---|---|---|
| Footprint, `Rules::radius(citadel)` | 2.5 (5×5 cells) | **6.0 (12×12 cells)** |
| Walls | solid block | 1.0 cell thick, the hall inside 10×10 |
| Hall height (floor to ceiling) | – | 6.0 cells |
| Roof slab | – | 6.0 to 7.0; the roof deck at 7.0, the four corner towers rise past it to the turret mounts |
| Body height, `BuildingHeight(citadel)` (`AcPose.cpp`) = `Rules::height` | 4.6 | 12.3 (deck 7.0, the corner towers' mounts at 9.0, crowns 8.3-8.7, the central tower's beacon top 12.3) |
| Doorway | – | the +y face, x in [-1.5, 1.5], 3.0 wide, 3.0 tall |
| Corner guns, `Defence::cornerInset()` | 2.0 | 5.5 (the mounts at (±5.5, ±5.5), radius 0.5, on the corner towers at z 9.0, `Defence::mountHeight()`) |

The hall is the MMO's: `Rules::setCitadelHall(bool)` / `citadelHall()`
switch it for the whole process (an atomic, read on every region's
thread). The MMO's start and the server turn it on, the strategy game's
start turns it off (its maps, AI layouts and Swift goldens are built round
the solid 5x5 Citadel); every game start sets it, both ways. `Rules::radius`,
`Rules::footprint`, `Rules::height` (the body height, 12.3 / 4.6, which
`BuildingHeight` reads), `Defence::cornerInset()` (5.5 / 2.0) and
`Defence::stationInset()`, `Defence::mountHeight()` (9.0 / 4.6) and
`Rules::depositRange()` (the edge + 0.5: 6.5 / 3.0) all read it, so nothing
half-switches. It is on by default (any MMO path that forgets it still gets
the hall); the core tests on the strategy game's maps (window maps, the home
map, the Swift goldens) turn it off per test (`simtest::CitadelHall`,
`core-tests/tests/CitadelHall.h`), and `simbench` turns it off at its start.

Everything that reads `Rules::radius` follows (deposit reach, unload reach,
spawn at the door, placement, the map's footprint). A 12×12 Citadel needs
flatter ground to place: `placementRefusal`'s height-spread tolerance is 0.2
up to the 5x5's footprint (its samples 4.5 cells apart) and grows with the
samples' span beyond (0.2 × (2r − 0.5) / 4.5, 0.51 for the 12x12), so the
same slope that takes a 5x5 takes the 12x12 (the hall floor is the
ground's height at the centre; the model's floor slab hides the slope).
On the start region, of 289 spots that take the 5x5 every 4 cells, 144 take
the 12x12: the rest have a rock, a cliff or a deposit under the bigger
square (3 a ridge's bend). The "Too close to resources" band keeps 3 cells
from the Citadel's edge to the ore and 3.5 to a well (`r + 3`, `r + 3.5`:
5.5 and 6 for the 5x5 as before).

## 2. The footprint has a shape (core)

`Rules::footprint(StructureKind) -> const Footprint&` (new, `Rules.h`): a
kind's solid parts in its own frame, cells, relative to `Structure::position`:

```cpp
struct Slab { Vec2 min, max; double z0 = 0, z1 = 0; };   // an axis-aligned box
struct Footprint {
    double radius;                 // the bounding half side (= Rules::radius)
    std::vector<Slab> walls;       // solid, always
    std::optional<Slab> door;      // solid while the door is not open
    std::optional<Slab> interior;  // the hall: inside it a unit is sheltered
    std::optional<Slab> roof;      // over the interior (the camera's ceiling)
    bool blocks(Vec2 local, bool doorOpen, double grow) const;  // walls on the ground (+ door if closed), each grown by `grow`
    bool inside(Vec2 local) const;                               // in `interior`
};
```

Every kind but the Citadel (and the Citadel without the hall): one wall slab
±radius, z 0 to its body height, no door, no interior. The Citadel with the
hall (`blocks` leaves out a wall that starts above the ground, the lintel, so
the open doorway is walkable):

| Slab | x | y | z |
|---|---|---|---|
| wall +y, left of the door | [-6, -1.5] | [5, 6] | [0, 7] |
| wall +y, right of the door | [1.5, 6] | [5, 6] | [0, 7] |
| lintel over the door | [-1.5, 1.5] | [5, 6] | [3, 7] |
| wall -y | [-6, 6] | [-6, -5] | [0, 7] |
| wall -x | [-6, -5] | [-5, 5] | [0, 7] |
| wall +x | [5, 6] | [-5, 5] | [0, 7] |
| door | [-1.5, 1.5] | [5, 6] | [0, 3] |
| interior | [-5, 5] | [-5, 5] | [0, 6] |
| roof | [-5, 5] | [-5, 5] | [6, 7] |

Who uses it:

- `Simulation::pilotCanStand` (`Pilot.cpp`): a **human-driven** unit is
  refused where `footprint.blocks(p - s.position, doorOpen(s), radius·0.8)`
  for a complete Citadel with the hall, the whole square for anything else
  (a Citadel's scaffold too). So a pilot walks through the open door and
  around the hall; the slide along x then y stays. A map doodad inside a
  complete Citadel's bounding square + 1 (`Simulation::swallowedByCitadel(Vec2)`,
  a saved world's Citadel grown over its rocks) blocks no pilot: its rock
  circles and the cells the grid left out for it (`swallowedDoodadCell`) are
  walkable; the game hides those doodads by the same predicate.
- `NavGrid::setDynamic`, `NavGrid::solid`, `Simulation::refreshNav`: **unchanged
  in meaning**, the whole bounding square blocks: AI units, mercenaries and
  the Brood never enter. `refreshNav` does not push out a unit standing in
  a complete Citadel's hall or its doorway, or at a gun
  (`Simulation::standsInHall`; one on a wall is pushed out as ever), and
  `trapped` never kills one there.
- `NavGrid::insideStructure(p, st, grow)` keeps meaning "within the bounding
  square grown by `grow`", so "at the Citadel" checks (unload, the H screen,
  `AtRealmCitadel`) hold inside the hall too.
- The game's cameras (section 6) sweep against the same slabs. One truth.

## 3. The door (core, server, net, game)

```cpp
struct Door {
    double open = 0;        // 0 closed … 1 open
    bool opening = false;   // the way it moves
    bool operator==(const Door&) const = default;
};
std::optional<Door> Structure::door;   // set on every complete Citadel (the server, armCitadels; the core, on completion)
```

- Speed: `Rules::doorSeconds = 2.5` closed to open. The core steps it every
  tick toward `opening ? 1 : 0` (`Simulation::stepCitadels`, before the
  buildings' step), and gives every complete Citadel with the hall its
  `Door` there (shut).
- Passable to pilots when `open >= 0.95` (`Simulation::doorOpen(s)`).
- A pilot opens it from either side and closes it **only from inside**:
  `RequestMessage::Door { bool open; }` (protocol 36, index 22). The server
  (`WorldMap::door(pilot, open)`) refuses with a reason unless: the pilot's
  unit is complete; a complete realm Citadel is in its region; to open, the
  unit stands within `Rules::doorReach = 2.0` of the door slab (either side);
  to close, the unit is `footprint.inside()`; to close, no unit stands in the
  door slab grown by its radius ("Someone is in the doorway"). The core's
  half: `Simulation::doorRefusal(s, open, unit)` ("No door to work here",
  "Too far from the door" past 2.0 cells from the slab seen from above,
  "Close it from inside", "Someone is in the doorway") and
  `Simulation::door(unit, structure, open)`, which applies it.
- The game sends it on the act key (E / click / Space) with the door panel
  under the crosshair or within `doorReach` and nothing else to act on
  (section 7).
- Save: the door state is not saved; a loaded Citadel starts closed.

## 4. Shelter (core, server)

`bool Simulation::sheltered(const Unit& u) const`: `u.task == atGun`, or
`u`, on the ground, stands `footprint.inside()` a complete Citadel not
hostile to it (position test; the doorway slab is outside, so a unit in the
doorway is not sheltered). Never with the hall off.

Applied in: `Simulation::target()` (so bugs, turrets and soldiers never pick
it), `hit()` (nothing happens: no damage, no `hitAt`), `slamStrikes` (the Matriarch), and
on the server the prey lists in `WorldBrood.cpp` and `WorldNest.cpp`. The
Brood can still attack the Citadel itself and anything outside. The Brood
never paths inside (section 2).

An unattended Citadel (the manager's ruling, 2026-10-18): the guns need a
gunner, so a home must not be lost while nobody is there to man them.

- The director's raids and the Matriarch take a Citadel as a raid's
  building only while a human pilot is on the ground in its region
  (`WorldMap::humanIn`), manned or not; with nobody there it is not a
  candidate, as before D1 (docs/creatures.md, "Raids on buildings").
- A floor: bugs never take a Citadel below `Rules::citadelBugFloor` (10%)
  of its hp (the core's `Simulation::hit`, a bug the shooter), so the worst
  case is a wreck to repair; the destroyed path stays only for what is not
  a bug. A pilot sheltered in the hall stays sheltered.
- Tests: `raid_skipsTheCitadelWithNobodyInTheRegion`,
  `citadel_neverFallsBelowTheFloorToBugs` (server, HuntTests.cpp, the
  Matriarch at Tern Ridge's Citadel).

The floor (the manager's ruling, 2026-10-18): the Brood never takes a
Citadel below `Rules::citadelBugFloor` (10%) of its hit points; one already
lower takes nothing more from it (`Simulation::hit`, the shooter a Brood
unit; every bug's damage to a building comes through there: bites, spits,
the Matriarch's slam and spit). The worst a raid leaves is a wreck to
repair. Anything that is not a bug still destroys it. A biter measures
its reach to a hall Citadel from the square's nearest face (not a circle of
its half side, which left one along a 12x12 wall up to 0.9 cells short) and
goes to the middle of that face, clear of the corners the grid holds it off
(`Simulation::bugAttack`); every other building as before. The server's half
(raids pick a Citadel only while a human pilot is in its region) is in
`WorldBrood.cpp` / `WorldNest.cpp`.

Respawn at a Citadel (server) puts the Prospector in the hall at
`position + (0, -3)` when the Citadel is complete, else at the door as today.

## 5. Manual turrets (core, server, net, game)

The guns' names follow the HUD's compass, where +y is the compass's south:
gun 0 (+x+y) SE, 1 (−x+y) SW, 2 (−x−y) NW, 3 (+x−y) NE; the door (+y face)
is on the Citadel's south side.

The four corner guns exist as today (`Structure::guns`, armed by the server
with the realm's marks) but **never fire by themselves**, hall or not.
`Simulation::stepCornerGun` for a corner gun with no gunner only turns the
aim back to rest (`Defence::cornerAngle(i)`, pitch 0) at `turretTurnRate`.

```cpp
struct CornerGun {
    std::optional<int64_t> target;   // what the last shot hit (for the pose); no longer chosen by the gun
    double cooldown = 0;
    double aim = 0;                  // yaw, world radians, within the arc
    double pitch = 0;                // radians, within the pitch limits
    std::optional<int64_t> gunner;   // the unit operating it
};
```

Constants (`Defence.h`):

| | Value | Meaning |
|---|---|---|
| `gunArcHalf` | 110° (1.92 rad) | the yaw arc about `cornerAngle(i)` a gun may point in; the hull lies in the other 140°, so the barrels never cross it |
| `gunPitchMin`, `gunPitchMax` | -20°, +50° | |
| `gunStation(citadel, i)` | `position + (±4.0, ±4.0)` in corner i's quadrant | where the gunner stands, in the hall |
| `stationReach` | 1.5 | a pilot within it of the station mans the gun |
| `mountHeight()` | 9.0 (4.6 without the hall) | the turret base's z on the corner towers (the deck stays `deckHeight` 7.0) |
| `gunEye(citadel, i, aim, pitch = 0)` | mount + R_yaw(aim) · [T + R_pitch(pitch) · E], T = (0.144 along the aim, 0.396 up) the trunnion, E = (−0.40 along the barrels, +0.55 up); x, y on the ground, z from the Citadel's ground (level: 0.256 back, 9.946) | the gun's camera (game), riding the cradle: the barrels stay in frame from −20° to +50°; it looks along the aim and pitch |
| `clampAim(i, aim)` | | the aim held in the arc at its hard stops (no wrap) |

Manning: `RequestMessage::Man { int8_t gun; }` (protocol 36, index 23; `gun`
-1 disengages). The server (`WorldMap::man(pilot, gun)`) refuses unless the
pilot's unit is complete, `sheltered`, within `stationReach` of
`gunStation(s, gun)`, and the gun has no gunner. Then `u.task = atGun`,
`u.structure = s.id`, `u.position = gunStation`, `guns[gun].gunner = u.id`.
Disengage (the request with -1, the Esc/F key in the game, the unit dying,
leaving the region or being handed back) clears both. The core's half:
`Simulation::man(unit, structure, gun)` (refusals "Only a finished Citadel
has guns", "The Citadel's guns are not armed", "Already at a gun", "Get
inside the Citadel first", "Too far from the gun's station", "Someone is at
that gun"), `unman(unit)` and `mannedGun(u)`. `stepCitadels` frees a gun
whose gunner is dead, gone or no longer `atGun` at it every step; a unit
`atGun` that no one drives steps away (idle).

`Unit::Task::atGun` is the 19th task (`EnumInfo<Unit::Task>`, still 5 bits on
the wire). `pilotBusy` holds a unit of any kind at its station like
`inDerrick` (walk ignored); `atGun` is never a target of the act key's
`pilotTarget` (the key fires the gun), never boarded by a Dropship nor a
driven Dropship's patient. The renderer shows the unit standing at the console (not
hidden); the game's first-person view hides it as it hides the driven unit.

Aim and fire, each tick, in `stepCornerGun` when the gun has a gunner (its
input is the gunner's own `Pilot`, `pilotFor(gunner)`): the gun's
`aim` is the gunner's `Pilot::heading` clamped into the arc and its `pitch`
the gunner's `Pilot::pitch` clamped (new field, also on `PilotInput`,
quantised to 8 bits over ±90°; a non-gunner's pitch is ignored). Fire while
`act || hold` and `cooldown <= 0`: the target is the gunner's
`Pilot::Crosshair::On{target}` when it is a foe the Citadel's side sees
(`foe`), within the weapon's range of the mount's edge and within 0.25 rad
of the aim; a hit as today (`hit`, `bugHits`, `GameEvent::Shot{s.id, …}`,
`CornerGun::target` set to it), else a miss tracer along the aim:
`GameEvent::Missed{s.id, mount + aim × (range + 0.5), 0, pitch}` (the game
finds the gun by its aim, as for `Shot`). The weapon is
`Defence::weapon(s)` with the realm's marks as today.

## 6. Cameras never pass through walls (game)

`FAcPilotChase::Follow` (third person) replaces its footprint-circle loop
(`AcPilotChase.cpp` "docs/mmorpg-shots.md #12") with a sweep: the boom from
the pivot to the camera, a sphere of 0.25 cells, against the slabs
(`Rules::footprint`, world space, the door slab while `open < 0.95`) of
every structure within 20 cells, plus the rays and the ground as today. The
camera stops at the first hit less the sphere. Inside the hall the roof slab
is its ceiling. The "lift to look over a low wall ahead" rule stays for
slabs lower than 3 cells (scaffolds, small buildings).

As built (`AcPilotWalls.h`): the slabs are each building's
`Rules::footprint` boxes on the ground at its centre, sunk as the renderer
sinks a building under construction; a Landing Pad (walkable) is no wall.
The sphere is swept as a point against each box grown by 0.25 (square
corners); a box the sphere already touches at the start is swept with the
point alone, one the start is inside is left out. The pivot (over the
right shoulder) is first swept from the eye, so a pilot hugging a wall
never has its boom start inside it. A camera stopped by a low slab rises
by `0.8 × (1 − t)` as before; a lift never goes up through a slab over it.

`FAcPilotCamera::Follow` (first person) clamps the eye the same way against
the slabs when the eye's offset from the unit's centre would cross one (the
lean, a wide unit); the Prospector's eye is over its centre so it rarely
moves. As built: the sweep runs from over the unit's centre (at the eye's
height) to the eye with a sphere of 0.12 cells (the near plane is 0.04),
after `Follow`, in the pawn (`AcPilotWalls::ClampEye`).

At a gun the view is **always first person** from `gunEye` (the V key does
nothing; third person resumes on disengage), the look yaw clamped to the
arc with a hard stop (no wrap) and the pitch to its limits, both on the
client and again by the server. The cockpit is hidden; the unit is hidden.

## 7. Input and the HUD (game)

- The act key (E, click, Space) in order: the existing targets (drill,
  strike, repair, enter the Derrick, place a building), else a **gun
  station** within `stationReach` (prompt "E · TAKE GUN 1 SE"), else the
  **door panel** within `doorReach` (prompt "E · OPEN" outside and inside,
  "E · CLOSE" inside when open). Prompts sit where the Derrick's prompt sits.
  Panels: a part on each side of the doorway (`doorPanels_0` outside,
  `doorPanels_1` inside) that lights when in reach.
- At a gun: mouse aims within the arc; click or E or Space fires (held
  fires at the weapon's rate); Esc or F disengages (not leave the
  Prospector); V ignored.
- The feed: `SAcPilotOverlay` paints over the normal reticle a monitor frame
  (dark corner brackets, a thin scanline tint, a "LIVE" pip), "TURRET 1 · SE
  · CITADEL" top left, an **arc gauge** bottom centre (the 220° arc as a
  band, the hard stops marked, the aim as a needle), the heat/cooldown pips,
  the gun's marks, and "ESC · DISENGAGE". The unit's own HP stays. The
  reticle's hit target health as today.
- Pose (`PoseCitadel`): the `ramp` part turns about its hinge (the door
  slab's outer bottom edge) by `door.open · 90°` outward; `gunStations_k`
  screens lit (blue, 1.9) while manned; `hallLamps_k` yellow working lamps
  steady when complete; the turrets shown whenever the Citadel is complete
  (not only when `marks` is set), aimed by `aim` and `pitch`, barrels spinning
  while the gun fired within 0.3 s.

As built (the game):

- `AcCitadelAct` (`AcCitadelAct.h`) finds the act: in the MMORPG mode, with
  no `pilotTarget`, a free gun's station within `stationReach` while
  `sheltered`, else the door within `doorReach` of its slab (outside only
  while it is shut or shutting: it opens; inside it closes while opening and
  opens while shutting). When the core would refuse it (`doorRefusal`:
  "Someone is in the doorway") the prompt is the reason in the blocked tone
  and the key sends nothing. The pawn sends `FAcMmoClient::Door`/`Man`; a
  refusal from the server comes back as a note (`HallRefusal`).
- At a gun the prompt reads "Click  fire   ·   Esc  disengage"; the
  console's status "At a corner gun". The look is held by
  `AcCitadelAct::LookYaw` (measured from the arc's middle, clamped, never
  wrapped) and `LookPitch`; `Pilot::pitch` is the look's pitch, sent on
  every input. The view turns about the gun's mount between frames.
- The feed's heat: the wire carries no cooldown, so the overlay counts the
  Citadel's `Shot` and `Missed` events put down to the gun aimed nearest
  them (as the pose does): six pips over the last 1.5 s against the
  weapon's rate flat out, and a cooldown bar under them. A short pitch
  scale stands right of the arc gauge. Slate paint only.
- At a gun the dashboard is down (`UAcConsoleSubsystem::SetDashboardHidden`:
  the minimap, the unit's panel; the skill slots and item stacks are not
  drawn); the top bars, the compass, the wallet and the step card stay. In
  its place a slim strip at the bottom edge: the unit's hit points and
  "Click · FIRE   Esc · DISENGAGE". The arc gauge and the heat sit lower
  left, the gun's name, marks and LIVE at the top; the lower middle stays
  clear down to the strip, where the gun's barrels and muzzle ring show
  (at the game's 62-degree view a little lower in the frame than the
  model's renders).
- Pose numbers: `gunScreens_k` 1.9 lit / 0.04 dark (exported 0.04),
  `hallLamps_k` (`hallGlow`) 1.0 / 0.05 under construction, `doorPanels_k`
  (`panelGlow`) 1.0 on the pilot's side while in reach (the pawn sets
  `AcCitadelAct::SetPanel` each frame) / 0.05. The pitch turns
  `turretCradles_k` (the model's part between the turret and its barrels);
  the barrels keep their spin. Without the hall (the strategy game) the
  turrets show only on an armed Citadel, as before.
- The map's doodads and the sites' crates under a complete Citadel's grown
  square (`Simulation::swallowedByCitadel`) are not drawn
  (`AAcRegionGround::Swallow`, `AAcSites`).
- The pictures: `-AcFlowTest=mmocitadel -AcMmoShots=DIR -AcMmoShotsC1`
  (the step itself takes them: `c1-door-shut`, `c1-door-open`, `c1-hall`,
  `c1-feed`).

## 8. The model (model worker)

As built (2026-10-18): a Geometry Script model,
`unreal/Tools/Editor/models/citadel.py`, replacing the SceneKit export (the
`citadel_blue` entry, `citadel_red` written as its copy), then
`citadel_turrets.py` (run it after `citadel.py`, every time: it splices the
turrets into both entries). Frame: UE cm, front +Y, Z up, 100 cm a cell;
the ground at z 0 is the ground's height at the centre. Renders:
`art/models/citadel/c1/` (`before-3q`, `look-*`: outside, the door, the
door from the ground looking up, the hall, the hall to the door, the
scaffold; `cradle-eye-gun0*`: the gun's eye at pitch 0, -20, +50).

- **Hull, exactly the slabs of section 2**: the walls' boxes are the wall
  slabs (their outer face at 5.85, the skin standing on it), sunk 1.0 into
  the ground to hide a slope; the roof slab; the hall floor (top at 0.05)
  and the doorway's threshold. Everything inside the hall, doorway or roof
  slabs is decoration above head height or flat on the floor. No solid
  part of the model lies outside ±6.0; the skin (panels, ribs, bands,
  slits, markers, coping) stands up to 0.10 out (the corner edges and the
  ribs' feet; 2026-10-18, no two solids share a face: each layer stands
  2 cm off the one under it, `coplanar_check` in citadel.py), the side pipe 0.12, the
  floodlights over the door 0.18 (at z 4.0, over any unit's head). Outside the
  footprint only flat ground pieces: the open ramp and the apron plate
  before it (x ±2.6, to y 10.2, 0.02 high, no ray piece).
- **Corner towers**: the wall corners, clad 2 cells wide on both outer
  faces, carried up past the deck (over the roof they widen inward to
  [4.0, 6.0]²) to a dark crown at 8.3 to 8.7 with a hazard band, and a
  pedestal to the turret mount at 9.0 on (±5.5, ±5.5). Three `floodGlow`
  slits per outer face in the shaft (4.3) and over the deck (7.5), a
  `markerGlow` at each crown's corner, stepped anchor feet at the base.
- **Walls**: buttress ribs every 2 cells (±2.9 on the front, either side of
  the door), light panels in the bays, a service band at 3.0 with a pipe
  on the sides and back, `ventGlow` vents in the lower bays, the blue team
  band at 5.9, the coping round the deck's edge.
- **Front**: the doorway 3 × 3 with a hazard frame and lintel; the
  `doorSegments_0..5` strip over it (z 3.4) and two floodlights (`floodGlow`,
  z 4.0; the night pool of `AcLamps` follows them); `doorPanels_0` outside
  and `_1` inside, right of the doorway at x 2.25, z 1.0 to 1.4
  (`panelGlow`, Emission 1.0 lit); `light_2` at (0, 7.0, 2.5); the apron.
- **The ramp**: the door standing in the doorway (y 5.7 to 6.0, z 0 to
  2.96), pivot at the doorway's outer bottom edge (0, 600, 0) UE cm, at
  rest closed. Open = 90° about its X axis, top outward (UE roll +90, SK
  euler x +π/2; `Autocraft.Pose.CitadelRamp`). Treads on its outer face.
- **Deck**: plating with seams, a railed walkway along the edges between
  the towers (z 7.0 to 7.2), two tanks, a crane, the steam stack
  (`emitter_1` at its mouth) and the `dish` on a braced mast (pivot at 9.8).
- **Central tower** (the old keep's): an octagon from the deck to 9.0 with
  the four `progressLamps` windows on its faces toward the camera (z 7.8 to
  8.2), a chrome ring, a narrower octagon to 10.6, the window drum with the
  five `queueLamps` (z 10.7 to 10.9), a dome, the `beacon` at 12.2 (the
  top, 12.31).
- **Inside**: `hullMid` walls and ceiling, `hullDark` floor; baseboards,
  ceiling beams, cable trays; a railed catwalk along the back and both sides
  at 4.0 (0.9 deep: brackets, a ladder on the back wall); a hoist on the
  middle beam whose hook stops 2.5 over the floor; `hallLamps_0..1` on the
  front wall (z 4.3), `hallLamps_2..7` under the catwalk's edge looking
  down (z 3.6) (`hallGlow`, Emission 1.0 steady); the team disc on the
  floor. Lumen lights the hall from the lamps and the open door.
- **Gun stations** `gunStations_0..3` (corner order 0 (+x,+y), 1 (−x,+y),
  2 (−x,−y), 3 (+x,−y)), after the user's reference
  (`art/models/citadel/ref/console-reference.jpg`, 2026-10-18): a squat
  armoured console 1.0 wide, 0.8 deep, 0.9 tall on a hazard plinth with
  lit yellow strips, a control deck with a joystick (the gunner's left)
  and coloured buttons, a monitor leaning back on a stand to 1.7, cables,
  a conduit up to the gun. It stands on the corner's diagonal facing the
  gunner's point (±4.0, ±4.0), its middle 0.97 from it: every solid of it
  is at least 0.51 from the point, and its back corners sink 0.35 into
  the walls (built into the corner). The monitor's screen `gunScreens_k`
  shows the gun's view, a baked horizon
  (`unreal/Resources/Textures/citadel_gun_screen.png`, made by
  `models/citadel_screen.py`) in the `gunScreen` material, lit by the pose
  as before (1.9 lit, 0.04 dark). A hazard ring of radius 0.48 marks the
  gunner's spot. No seats.
- **Turrets** (`citadel_turrets.py`, `CORNER = 550`, `TOP = 900`): the
  parts as before plus `turretCradles_k` between `turrets_k` (yaw: SK euler
  (0, −aim, 0)) and `turretBarrels_k` (spin about X): the pitch, SK euler
  (0, 0, +pitch), pivot on the barrel axis at the trunnion, 0.144 along the
  aim and 0.396 over the mount; the muzzles, flashes and the muzzle ring
  ride on it. The housing's back and cheeks are low so the gun's eye sees
  the barrels. The script asserts that the barrels and the ring keep 3 cm
  off every solid of `citadel_blue` (its ray pieces) at every yaw of the
  220° arc at pitch −20° (worst 11.9 cm; 15.1 cm with C2's minigun look, docs/guns-and-practice.md section 2); at −45° it fails.
- **The gun's eye** for these numbers: on the cradle, mount + R_yaw(aim) ·
  [T + R_pitch(pitch) · E], T = (0.144 along the aim, 0.396 up), E = (−0.40
  along the barrels, +0.55 up): the barrels and the glowing ring sit in the
  lower middle of the frame at every pitch (`cradle-eye-gun0*.png`).
- `scaffold_citadel`: poles, rails and braces round the walls at 6.25,
  9.2 high, the doorway's bays left open.
- About 12k triangles (the SceneKit model had 56k).
  `Content/Shatter/citadel_blue.bin` is baked again by any editor game run
  (a headless `-AcFlowTest=newgame` does it): commit it with the model.

## 9. Wire and save (net, server)

- Protocol **36**: `RequestMessage::Door{open}` (22, 1 bit) and
  `Man{gun}` (23, 3 bits: the gun + 1, so -1 leaves the gun; a wild gun is
  clamped to -1..3); the index stays 5 bits (24 of 32 used).
  `structureFields` appends `door` (`Opt<Door>`: `open` in 255ths, 8 bits,
  `opening` 1 bit); `GunsCodec` adds each gun's `pitch` (8 bits) and
  `gunner` (`Opt<id>`): the client gets a gun's aim, pitch and gunner, never
  its target or reload. `PilotInput::pitch` is 8 bits over ±90°
  (`q::pitch`: 127 steps each way, 0 and ±90° exact); the server copies it
  onto the driver's `Pilot::pitch` (`Region::step`). `Unit::Task` gains
  `atGun` (19 tasks, still 5 bits, `codec::kTaskBits`).
- The server (`WorldCitadel.cpp`): `WorldMap::door` and `WorldMap::man` act
  on the realm's complete Citadel nearest the pilot's Prospector through
  the core's `Simulation::door`, `man` and `unman`; they are links of
  `Server::skillTree`'s chain, logged `refuse … by pilot N: why`. A unit
  leaves its gun when its pilot hands it back, goes off line or it leaves
  the region (`Region`); the core frees a gun whose gunner died.
  `Rules::setCitadelHall(true)` is the first line of the server's `main`
  (and of the server tests, `tests/HallOn.cpp`).
- Save schema **27** (a marker step, no new rows): on load every Citadel's
  footprint grown to its bounding square + 1 is adopted
  (`WorldMap::adoptCitadels`): its ore deposits are emptied for good (kept
  in the list, as the deposits go by index); the region file's blueprint
  sites in it are swallowed (`siteSwallowed`: never seen, opened, handed
  out by a contract, a mission, a goal or a survey chip); the map's doodads
  in it stay in the region file, which both ends build from, so the core
  lets a driven unit through them and the game hides them and the
  swallowed sites' crates (the shared predicate, the core's
  `swallowedByCitadel`); a unit in its walls is pushed out by `refreshNav`;
  another structure overlapping it is left. Each is a line of
  `grownCitadels()`, logged `save: schema 27: …` apart from the restore's
  problems. The user's saved world comes back with its Citadel grown in
  place.
- Dev control (`Server::devControl`, as the seated pilot, `as N` for
  another): `citadel here` (a complete, armed Citadel of the realm on clear
  ground ahead, its door shut, the Prospector 4 cells out from its door
  facing it; it answers with the Citadel's id), `citadel door open|close`
  and `citadel man N|none`, for the flow tests.

## 10. Tests

| Layer | Tests |
|---|---|
| core | `CitadelTests.cpp`: `footprint_citadelHallIsHollow` (the slabs, `blocks`, `inside`, both switch positions); `pilot_walksInThroughTheOpenDoorOnly`; `pilot_isPushedNowhereInsideTheHall`; `pilot_walksOverADoodadSwallowedByTheCitadel`; `pilot_unloadsInTheHallAndAtItsWalls`; `brood_neverEntersAndNeverTargetsASheltered` (A* blocked, `target()` nil, `hit()` 0, the slam); `door_closesOnlyWhenTheDoorwayIsClear`; `gun_aimClampsAtTheArcsHardStops`; `gun_restsWithoutAGunnerAndNeverFires`; `gun_aimClampedToTheArcAndPitch`; `gun_firesAtTheCrosshairFoeInRange`; `gun_eyeRidesTheCradle`; `citadel_bugsNeverTakeItBelowTheFloor`; `brood_strikesTheCitadelFromEveryFace`; `placement_citadelPlacesOnTodaysGround` (the start region). `DefenceTests.cpp`: `Defence_citadelTurretsNeverFireAlone` (was `…HitGroundAndAir`), `Defence_citadelRangeMarks` (a gunner's reach) |
| net | round trips of `Door`, `Man`, `PilotInput::pitch`, `Structure::door`, `CornerGun::pitch/gunner`; the width tests |
| server | `door_openFromOutsideCloseOnlyInside`; `man_takesAFreeGunWithinReach`; `man_freedWhenTheGunnerDies`; `brood_raidIgnoresThePilotInTheHall` (a group with prey set on a sheltered pilot drops it; the Citadel still takes hits); `citadel_noAutoFireWithoutAGunner`; `load_schema27ClearsTheGrownFootprint`; `respawn_insideACompleteCitadel`; `raid_skipsTheCitadelWithNobodyInTheRegion`; `citadel_neverFallsBelowTheFloorToBugs` |
| game | `Autocraft.Pilot.ChaseStopsAtWalls` (the sweep against the slabs); `Autocraft.Pilot.GunLookClamps`; `Autocraft.Hud.TurretFeedPaints`; `Autocraft.Pose.CitadelRamp` |
| flow | `-AcFlowTest=mmocitadel`: dev rig, walk to the door, open, enter, close, man gun 0, fire, disengage, leave; headless, then the shots (`-AcMmoShotsC1`: at the door, in the hall, the feed) |

## 11. Who owns what

| Worker | Owns | Lands first |
|---|---|---|
| core | `unreal/Source/AutocraftCore`, `unreal/core-tests` | the headers of sections 2 to 5 (`Footprint`, `Door`, `CornerGun`, `atGun`, `Pilot::pitch`, the `Defence` constants) as a first merge within the hour, so the others compile against them |
| net+server | `unreal/Source/AutocraftNet`, `unreal/net-tests`, `unreal/server` | protocol 36 |
| game | `unreal/Source/Autocraft` (camera, input, HUD, pose, flow test), `unreal/Tools/Editor/make_pilot_input.py` | the camera sweep |
| model | `unreal/Tools/Editor/models/citadel.py`, `citadel_turrets.py`, the Citadel's and scaffold's uassets and catalog entries, `art/models/citadel/` | the hull |

The manager merges (`git merge --ff-only`), runs the suites and
`unreal/Tools/build.sh` on the merged tree, and tells the others to rebase.
