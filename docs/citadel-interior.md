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
| Roof slab | – | 6.0 to 7.0; the roof deck at 7.0 carries the four turret pylons |
| Body height, `BuildingHeight(citadel)` (`AcPose.cpp`) | 4.6 | 9.0 (deck 7.0, the pylons and the old tower, dish and beacon above) |
| Doorway | – | the +y face, x in [-1.5, 1.5], 3.0 wide, 3.0 tall |
| Corner guns, `Defence::cornerInset` | 2.0 | 5.5 (the mounts at (±5.5, ±5.5), radius 0.5, on the deck at z 7.0) |

The hall is the MMO's: `Rules::setCitadelHall(bool)` / `citadelHall()`
switch it for the whole process (an atomic, read on every region's
thread). The MMO's start and the server turn it on, the strategy game's
start turns it off (its maps, AI layouts and Swift goldens are built round
the solid 5x5 Citadel); every game start sets it, both ways. `Rules::radius`,
`Rules::footprint`, `Rules::height` (the body height, 9.0 / 4.6, which
`BuildingHeight` reads), `Defence::cornerInset()` (5.5 / 2.0) and
`Defence::stationInset()` all read it, so nothing half-switches. The core
tests set it per test (`simtest::CitadelHall`).

Everything that reads `Rules::radius` follows (deposit reach, unload reach,
spawn at the door, placement, the map's footprint). A 12×12 Citadel needs
flatter ground to place: the core keeps `placementRefusal`'s height-spread
tolerance proportional to the footprint's side so a Citadel places where
today's does on the same ground (the hall floor is the ground's height at
the centre; the model's floor slab hides the slope).

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
  for any non-walkable structure (today: the whole square). So a pilot walks
  through the open door and around the hall; the slide along x then y stays.
- `NavGrid::setDynamic`, `NavGrid::solid`, `Simulation::refreshNav`: **unchanged
  in meaning**, the whole bounding square blocks: AI units, mercenaries and
  the Brood never enter. `refreshNav` must not push a unit standing in the
  interior out: a unit that `footprint.inside()` a complete Citadel (or has
  task `atGun`) is exempt, as `inBastion` is.
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
  tick toward `opening ? 1 : 0`.
- Passable to pilots when `open >= 0.95` (`Simulation::doorOpen(s)`).
- A pilot opens it from either side and closes it **only from inside**:
  `RequestMessage::Door { bool open; }` (protocol 36, index 22). The server
  (`WorldMap::door(pilot, open)`) refuses with a reason unless: the pilot's
  unit is complete; a complete realm Citadel is in its region; to open, the
  unit stands within `Rules::doorReach = 2.0` of the door slab (either side);
  to close, the unit is `footprint.inside()`; to close, no unit stands in the
  door slab grown by its radius ("Someone is in the doorway").
- The game sends it on the act key (E / click / Space) with the door panel
  under the crosshair or within `doorReach` and nothing else to act on
  (section 7).
- Save: the door state is not saved; a loaded Citadel starts closed.

## 4. Shelter (core, server)

`bool Simulation::sheltered(const Unit& u) const`: `u.task == atGun`, or
`u` stands `footprint.inside()` a complete Citadel of its side (position
test; the doorway slab is outside, so a unit in the doorway is not sheltered).

Applied in: `Simulation::target()` (so bugs, turrets and soldiers never pick
it), `hit()` (damage 0, like `shielded`), `slamStrikes` (the Matriarch), and
on the server the prey lists in `WorldBrood.cpp` and `WorldNest.cpp`. The
Brood can still attack the Citadel itself (its HP as today) and anything
outside. The Brood never paths inside (section 2).

Respawn at a Citadel (server) puts the Prospector in the hall at
`position + (0, -3)` when the Citadel is complete, else at the door as today.

## 5. Manual turrets (core, server, net, game)

The four corner guns exist as today (`Structure::guns`, armed by the server
with the realm's marks) but **never fire by themselves**. `stepGun` for a
corner gun with no gunner only turns the aim back to rest
(`Defence::cornerAngle(i)`, pitch 0) at `turretTurnRate`.

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
| `gunEye(citadel, i)` | the mount + (0, 0, 0.9) cells, 0.6 behind the barrels along the aim | the gun's camera (game) |

Manning: `RequestMessage::Man { int8_t gun; }` (protocol 36, index 23; `gun`
-1 disengages). The server (`WorldMap::man(pilot, gun)`) refuses unless the
pilot's unit is complete, `sheltered`, within `stationReach` of
`gunStation(s, gun)`, and the gun has no gunner. Then `u.task = atGun`,
`u.structure = s.id`, `u.position = gunStation`, `guns[gun].gunner = u.id`.
Disengage (the request with -1, the Esc/F key in the game, the unit dying,
leaving the region or being handed back) clears both. The server frees a gun
whose gunner is gone each world step.

`Unit::Task::atGun` is the 19th task (`EnumInfo<Unit::Task>`, still 5 bits on
the wire). `prospectorBusy` holds the unit at its station like `inDerrick`
(walk ignored). The renderer shows the unit standing at the console (not
hidden); the game's first-person view hides it as it hides the driven unit.

Aim and fire, each tick, in `stepGun` when the gun has a gunner: the gun's
`aim` is the gunner's `Pilot::heading` clamped into the arc and its `pitch`
the gunner's `Pilot::pitch` clamped (new field, also on `PilotInput`,
quantised to 8 bits over ±90°; a non-gunner's pitch is ignored). Fire while
`act || hold` and `cooldown <= 0`: the target is the gunner's
`Pilot::Crosshair::On{target}` when it is a foe within the weapon's range of
the mount's edge and within 0.25 rad of the aim; a hit as today
(`hit`, `bugHits`, `GameEvent::Shot{s.id, …}`), else a miss tracer along the
aim (`GameEvent::Missed` as `pilotMiss` does, from the mount). The weapon is
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
  station** within `stationReach` (prompt "E · TAKE GUN 1 NE"), else the
  **door panel** within `doorReach` (prompt "E · OPEN" outside and inside,
  "E · CLOSE" inside when open). Prompts sit where the Derrick's prompt sits.
  Panels: a part on each side of the doorway (`doorPanels_0` outside,
  `doorPanels_1` inside) that lights when in reach.
- At a gun: mouse aims within the arc; click or E or Space fires (held
  fires at the weapon's rate); Esc or F disengages (not leave the
  Prospector); V ignored.
- The feed: `SAcPilotOverlay` paints over the normal reticle a monitor frame
  (dark corner brackets, a thin scanline tint, a "LIVE" pip), "TURRET 1 · NE
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

A Geometry Script model, `unreal/Tools/Editor/models/citadel.py`, replacing
the SceneKit export (the `citadel_blue`/`citadel_red` catalog entries), per
the `ringshadow-model-pipeline` and `ringshadow-building-visuals` skills.
Frame: UE cm, front +Y, Z up, 100 cm a cell.

- Hull per section 1: four walls 1 cell thick, 7 tall; the roof slab; a
  floor slab inside (hides the slope); the doorway 3 wide × 3 tall on +Y
  with a lintel; the `ramp` part, a 3 × 3 × 0.3 slab hinged at the doorway's
  outer bottom edge (pivot there; the pose rotates it); `doorPanels_0` and
  `_1`; four `gunStations_0..3` consoles at (±4.0, ±4.0) facing their corner,
  each with a `gunScreens_k` emissive; `hallLamps_0..7` yellow lamps along
  the walls; the four corner pylons on the deck at (±5.5, ±5.5) carrying the
  turrets; the tower, dish and beacon as today above the deck centre.
- Keep every part name `PoseCitadel` drives: `progressLamps_0..3`,
  `queueLamps_0..4`, `doorSegments_0..5` (the hangar strip, now along the
  lintel), `dish`, `beacon`, `light_2`, and the materials' roles
  (`hullDark/Mid/Light`, `chrome`, `team`, the glow parts).
- `citadel_turrets.py`: `CORNER = 550`, `TOP = 700 + pylon height`; the
  parts as today; the barrels must clear the hull everywhere in the arc
  (section 5) at pitch -20°: check it in the script (a geometric assert).
- `scaffold_citadel` regenerated to the new size. The catalog bounds follow.
- Inside: the walls' inner faces and the floor and ceiling must be real
  faces (solid boxes, not open shells); a Lumen interior lit by the lamps
  and the open door. Materials inside: `hullMid` walls, `hullDark` floor.
- Renders: the model row (`AcModelRow`), an MMO shot at the door, inside the
  hall, from a gun; before/after under `art/models/citadel/`.

## 9. Wire and save (net, server)

- Protocol **36**: `RequestMessage::Door` (22), `Man` (23);
  `structureFields` appends `door` (`Opt<Door>`: `open` 8 bits, `opening`
  1 bit); `GunsCodec` adds `pitch` (8 bits) and `gunner` (`Opt<id>`);
  `PilotInput` and `Pilot` gain `pitch`; `Unit::Task` gains `atGun`.
- Save schema **27**: on load every Citadel's new footprint (bounding
  square + 1) is cleared of deposits, mounds, doodads and sites; a unit in
  it is pushed out by `refreshNav`; another structure overlapping it is
  logged and left. The user's saved world comes back with its Citadel grown
  in place.
- Dev control (`Server::devControl`): `citadel door open|close`, `citadel
  man N|none` (as the seated pilot), for the flow tests.

## 10. Tests

| Layer | Tests |
|---|---|
| core | `footprint_citadelHallIsHollow` (the slabs, `blocks`, `inside`); `pilot_walksInThroughTheOpenDoorOnly`; `pilot_isPushedNowhereInsideTheHall`; `brood_neverEntersAndNeverTargetsASheltered` (A* blocked, `target()` nil, `hit()` 0); `door_closesOnlyWhenTheDoorwayIsClear`; `gun_restsWithoutAGunnerAndNeverFires`; `gun_aimClampedToTheArcAndPitch`; `gun_firesAtTheCrosshairFoeInRange`; `placement_citadelPlacesOnTodaysGround` |
| net | round trips of `Door`, `Man`, `PilotInput::pitch`, `Structure::door`, `CornerGun::pitch/gunner`; the width tests |
| server | `door_openFromOutsideCloseOnlyInside`; `man_takesAFreeGunWithinReach`; `man_freedWhenTheGunnerDies`; `brood_raidIgnoresThePilotInTheHall` (a group with prey set on a sheltered pilot drops it; the Citadel still takes hits); `citadel_noAutoFireWithoutAGunner`; `load_schema27ClearsTheGrownFootprint`; `respawn_insideACompleteCitadel` |
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
