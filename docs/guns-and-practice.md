# Guns and practice (C2)

Status, 2026-10-18: designed; in work by four workers (core, net+server,
game, model); the core of sections 1, 3 and 4 built
(`core-tests/tests/GunsTests.cpp`). This page is the contract between them, as
[citadel-interior.md](citadel-interior.md) was for C1: names, numbers,
owners. Where the code had to differ, the owning worker corrects this page.

The user's asks, 2026-10-18 (with a reference picture,
`art/models/guns/ref/minigun-reference.png`, and a screenshot from a corner
gun that could not shoot a bug, `user-turret-view-2026-10-18.png`):

1. The Citadel's chain guns should look like the reference: a real
   six-barrel minigun.
2. A target dummy the Prospector builds for 10 ore, no blueprint, to
   practise on, since there is no empirical way to test a gun today.
3. Blueprints are wrong today (a copy is used up per building, dearer each
   copy). A blueprint is bought or found and used **once** to learn the
   building; after that the building is built for its ore.
4. The range of the weapons is unknown to the player; most weapons should
   reach anywhere in the line of sight within a region.
5. The chain guns' rate of fire is low and the animation poor. Wanted: the
   helicopter-door-minigun scene: red-hot tracers, impact explosions, rounds
   that drop a little with gravity, damage done at impact and not when the
   gun fires. The user tunes the numbers by feel after testing.
6. The B build menu becomes a radial wheel driven by the mouse with no
   cursor shown.

## 1. Rounds: real projectiles (core, net, server, game)

A new simulated entity, the **round**, for the minigun class of weapon: the
Citadel's corner guns and the gatling gun turret in this chapter. Unit
weapons keep today's hitscan-with-cone model (docs/shooter-feel.md); the
Prospector's cutter is melee and unchanged.

```cpp
struct Round {
    int64_t id = 0;
    int64_t shooter = 0;      // the structure (or unit) that fired it
    int64_t owner = 0;        // its side
    Vec3 position;            // cells; z above the ground datum
    Vec3 velocity;            // cells/s
    double damage = 0;
    double born = 0;          // state.time
    bool operator==(const Round&) const = default;
};
std::vector<Round> GameState::rounds;
int64_t GameState::nextRound;   // round ids, apart from nextID
```

Not saved, nothing per tick on the wire.

Constants (`Rules.h`), tuned by feel later:

| | Value | |
|---|---|---|
| `roundSpeed` | 90 cells/s | muzzle speed (at 30 cells a round takes 0.33 s and drops 0.55 cells) |
| `roundGravity` | 9.8 cells/s² | |
| `roundLife` | 3.0 s | then it is gone |
| `roundRadius` | 0.08 cells | the hit sweep's thickness |
| `roundSelfGrace` | 0.3 s | the shooter's own building, and the ground under its footprint, are passed through this long |

`Rules::roundAt(muzzle, velocity, t)` is the parabola, the one integrator:
the core steps every round by the same closed form, so the client's streak
flies the core's path at any frame rate. `Rules::roundVelocity(yaw, pitch)`
and `Rules::ballisticPitch(ahead, rise)` (the low arc at `roundSpeed`, nil
out of reach, about 826 cells level) go with it.

Stepping (`Simulation::stepRounds`, after the buildings fire and before
the units act; a round fired inside the step flies from its own moment):
every round flies to the step's end on `roundAt`, and the segment it flew
is swept against: units (an upright cylinder of the kind's radius and
`UnitStats::height`, 1.5 unless the row says otherwise: the Matriarch 5;
a flyer's centred on its height over the ground (`Unit::bodyHeight`),
`UnitStats::hover`: Dropship 2.4, Kestrel 2.1, Peregrine 1.9, a
Gloamwing's `Bug::height`, in its dive too, never below the ground;
units aboard, in a Bastion or Derrick, at a gun or sheltered in the hall
are never struck), structures (a complete hall Citadel's slabs: walls,
the door while shut, the roof; any other its bounding square by its body
height, a scaffold by what has risen; a Target Dummy lying down is passed
over; the shooter's own building and its ground passed for
`roundSelfGrace`), the
terrain (sampled every 0.5 cells along the path, the crossing bisected;
skipped while the path is above `Simulation::groundTop`, the highest
ground of its 4-cell tiles plus 1.5, worked out the first time a round
flies over a tile), the region's bounds (gone, no event). Candidates come
from 4-cell buckets of the units and buildings, built once a step while
rounds fly; the cylinder or box test only for those.

The first thing hit stops the round, whoever's it is. It is hurt only when
it is `targetable` by the shooter's side (an enemy, or a Target Dummy
standing): `hit(victim, max(0.5, damage − armour))` at the impact, a bug's
hit told to the director (`bugHits`, the building as the shooter) as the
corner guns did; no pilot kill credit (the shooter is a building, as in
C1). A friend's unit or building stops it unhurt: no friendly fire. Then
`GameEvent::Impact`, for the ground too.

Cost (`rounds_benchSixGuns`, core-tests with `AUTOCRAFT_ROUNDS_BENCH=1`;
540 rounds in flight over 200 Shardlings on the home map, the Mac at load
20): 72 µs a step (0.13 µs a round), 32 µs of it the bodies' sweep; the
first step over unknown ground 0.55 ms while its tiles are worked out.
Without the tiles the terrain's samples (64 ns each) made it 230 µs.

Firing (`Simulation::fireRound`): `GameEvent::Fired{shooter, round id,
muzzle (Vec3), velocity (Vec3)}` replaces `Shot`/`Missed` for these guns.
The muzzle is the barrels' end: `Defence::gunMuzzle(citadel, i, aim,
pitch)`, `muzzleAlong` 1.3 from the trunnion along the barrels, and
`Defence::gatlingMuzzle(at, aim, pitch)` from the model's catalog (the
barrels' cradle pivots 1.28 up and 0.3 along the aim, the muzzle 1.28
past the pivot: 1.58 out level); z on the ground's height. The gatling's
solve is `Structure::pitch` (back to level with nothing to shoot), for the
pose's cradle and the wire. The velocity is the aim (yaw,
pitch) turned by the cone roll (uniform over the cone's disc,
`Simulation::roll` seeded by the shooter, the step's time and the round, so
a scene replays the same) times `roundSpeed`. Nothing per tick goes on the
wire for a round: the client integrates the same equations from `Fired`
and ends the streak at `Impact{round id, at, what: ground | unit |
structure, victim (0 for the ground)}` or at `roundLife`. Protocol 37.

Rate, spin-up and heat on a corner gun and a gatling:

| | Citadel corner gun | Gatling gun |
|---|---|---|
| rounds a second (`UnitStats::citadelGuns` / `gatling` `cooldown`) | 30 (1/30) | 25 (1/25) |
| damage a round | 1.0 | 0.8 |
| spin-up `Rules::gunSpinUp` (`spin` 0…1 on the gun) | 0.4 s from trigger to full rate (the rate ramps linearly with the spin; it runs down as fast with the trigger up) | 0.4 s |
| spread `coneStand` | 8 mrad, `coneFire` +0.3 mrad a round (`bloom`), fading by 90% every 0.4 s (`coneRecovery`) | same |
| heat (`CornerGun::heat`, `Structure::heat` for the gatling) | `Rules::gunHeatPerRound` +0.014 a round, `gunCooling` 0.20/s at all times; at 1.0 the gun stops (`overheated`, "OVERHEAT") until it cools to `gunHeatResume` 0.4. It overheats after about 4.5 s of fire and cools from 1 to 0.4 in 3 s | the same numbers: about 6.5 s of fire |
| range | none: the round flies until it hits or dies | the AI picks targets within 20 cells (`UnitStats::gatling.range`; the air within 4.5), leads them (its speed along its heading × the flight) and fires on the ballistic solve (from the barrels' pivot, −60° to +70°, `Defence::gatlingPitchMin/Max`) once it points within 0.05 rad (`Simulation::gatlingAimGate`). It keeps or picks only a target whose arc clears the ground to the target's edge (`Simulation::gatlingPitch`, sampled every 0.5 cells): a gun on a ledge holds fire at a bug its lip hides |
| on the wire (protocol 37) | the heat and the barrels' `spin` (0…1 in 255ths, 8 bits each) and `overheated` (1 bit: the latch, since heat between 0.4 and 1.0 alone can't say OVERHEAT) in each gun of `GunsCodec`; the spread (`bloom`) and the `cooldown` stay on the server | the same three in the structure's fields |

`Simulation::minigun` steps a gun's barrels and heat; its `cooldown` is the
share of a round still due before the next leaves (0: the next pull fires
at once), not seconds (the Sentinel keeps seconds).

The realm's marks keep their meaning: rate marks scale rounds a second,
damage marks the damage, range marks the gatling's AI reach (+1 cell each)
and the corner gun's nothing (the user sees the drop instead).

The manned corner gun (C1) loses its range gate: it fires along its aim
whenever the trigger is down and the gun is not overheated; the crosshair's
target is only the HUD's business (and `CornerGun::target`, the pose's,
when it is `targetable`). C1's −20° stop left the ground nearer than about
26 cells under every gun, a raid at the walls out of reach. So the mounts
move outboard onto cantilevered brackets over the corners
(`Defence::cornerInset()` 6.2: the trunnion 0.2 outside the walls' outer
faces on the diagonal, the barrel clearing the tower's top edge pointed
down), the barrels dip to `gunPitchMin` −70° (`gunPitchMax` +50° as
before) and each gun turns `gunArcHalf` 135° either side of its corner
(C1's 110° stopped it 25° short of pointing along its walls, a strip along
each face's middle blind): the rounds meet flat ground about 3 cells out
from the wall's foot, along both its walls (test
`gun_reachesTheFootOfItsWall`).

### Range and line of sight: the body at its height

Every weapon aims at its target's body where it is now (C3, the user's
case of 2026-10-19: the Sentinel's missiles flew under the Brood's
flyers). The core has one rule, `Unit::bodyHeight`, cells over the
ground: a flyer at its hover, a Gloamwing at its `Bug::height` whether
it hovers or dives (its dive counts as ground for who may shoot it, not
for where its body is), anything else at half its height.
`Simulation::aimHeight` reads it for the gatling's ballistic solve and
the Sentinel's pods (`Defence::sentinelPitch`, sent on the structure's
`pitch`), and the rounds' hit cylinders are centred on it, so the aim
follows a diving Gloamwing down shot by shot. The game's shots, streaks
and missiles end on the drawn body (`UAcEffects::Body`: the kind's
`BodyPart`, the strategy flyers' `body`, the Gloamwing's `thorax`, as it
bobs and dives; not drawn, the core's height, never the ground), and the
units' turrets pose onto `AcPose::BodyHeight`. Tests: core
`aim_*` (AimTests.cpp), game `Autocraft.Aim.Body`,
`Autocraft.Aim.SentinelPods`; pictures `-AcMmoShotsC3Aim`
(art/models/sentinel/c3-aim).

## 2. The minigun look (model)

`citadel_turrets.py` and `gatling.py` rebuilt after the reference picture:
six long barrels in a cluster with a front clamp and a mid clamp, a stubby
rotor housing behind them, a flash hider ring at the muzzle, an ammunition
chute from a box on the mount's flank, the whole cluster on the cradle
(pitch) over the yaw ring. Part names and drivers unchanged (`turrets_k`,
`turretCradles_k`, `turretBarrels_k` spinning, `turretTips_k`,
`turretMuzzles_k`, `turretFlashes_k`; the gatling's `head`, `barrels`,
`tips`, `muzzles_0`, `flashes`, `hopper`). The arc assert of C1 holds.
Spin: the pose spins the barrels to 30 rad/s on firing, with the spin-up and
spin-down of section 1; a round leaves every `cooldown`, the muzzle flash
every round (every second round at 60 fps is enough to read).

Also a **target dummy** model, `dummy.py`: a hazard-striped post on a
square base, carrying a plate with a Shardling silhouette and ring marks;
parts `plate` (hinges back when the dummy is down), `rings_0..2` (lit by
hits), `beacon`.

## 3. The target dummy (core, server, game, model)

`StructureKind::dummy` ("Target Dummy"), the 12th kind (`kStructureKindBits`
still 4). Rules: cost 10 ore, 0 MH (`Progression::worldOre` the same);
`buildTime` 2 s; radius 0.5 (1×1); hp 150, no armour (every round counts
in full: 150 corner-gun rounds, 5 s at full rate); `height` 1.8; no
blueprint (`Rules::freeBuild(kind)` true, the only one; `buildRefusal`
skips the blueprint and floor for it); limit 12 a region
(`Progression::perRegion`); not a turret; walkable no; the Brood ignores
it (a bug's `acquire` and its breach skip it); no vision
(`Rules::vision` 0). In `PilotBuild::worldKinds`.

**Targetable by its own side:** `Simulation::targetable(const Unit* or
Structure*, side)` = hostile, or a complete dummy that is up. Used in
`crosshairSight`, `pilotFoe`'s and `pilotSight`'s structure loops, and the
corner gun. Auto turrets keep their hostile-only scan.

**Down and up:** a finished dummy at 0 hp is not destroyed: `Structure::downUntil =
time + Rules::dummyDownTime` (3 s), `GameEvent::DummyDown{id, hits}`; while
down it is not targetable, takes nothing and rounds pass over it (the plate
lies back); at `downUntil` it stands with full hp and `hits` 0
(`Simulation::stepDummies`). `Structure::hits` counts the hits since it
last stood, a round's or any other weapon's (sent in the snapshot; the life
bar shows "HITS 37"). An unfinished dummy is destroyed like any building. On the wire
`downUntil` goes as the absolute game time in milliseconds (`Opt<ms>`, as
the unit's `hitAt` goes; nil while up): the snapshot's fields have no clock,
and an absolute time changes twice a fall where seconds left would change
every tick.

The build wheel offers it with no tag; placement as any building.

## 4. Blueprints learned once (core, server, game)

- `PilotProgress::learned` (a set of `StructureKind`, saved as `learned` in
  the progress JSON once one is learned) replaces the per-build copy, with
  `knows(kind)` (learned, or free), `learnRefusal(kind)` ("You know the
  Derrick", "No Derrick blueprint to learn from") and `learn(kind)` (one
  copy used, the kind learned). `blueprints` (the copies held) stays as the
  inventory of unused blueprints; `bought` is gone (an older blob's
  "bought" is read into `learned`), and the shop prices each kind at its
  first copy (`Shop::blueprintPrice(kind, 0)`).
- **Learning:** `RequestMessage::Learn{StructureKind}` (protocol 37, index
  24): the pilot holds a copy, is not learning what it knows ("You know the
  Derrick"), the copy is consumed, the kind is learned, a toast "DERRICK
  LEARNED". The build wheel shows an unlearned kind with copies as LEARN
  (selecting it learns, then the wheel stays open on it to build); keys 1–4
  stay items.
- **Building:** `buildRefusal`: free kinds skip the blueprint and the
  level; else a kind with no blueprint in the world as today, then
  learned, else "No blueprint: buy one at a Citadel (H)" if no copy,
  "Learn it first (B)" if a copy is held; then the level and the limits as
  today (the dummy's too). No copy is consumed by a build.
- **The shop:** sells each blueprint once per pilot at its first-copy price
  (`Shop::blueprintPrice(kind, 0)`), refuses "You know the X". **Sites,
  chips, missions, the Matriarch:** a blueprint of a learned kind gives the
  blueprint's ore price instead (the message says so).
- Schema 28: on load, a pilot learns every kind it ever bought, owns a
  building of, or holds a copy of; the copies of learned kinds are removed
  (a `learned` column on the pilots, 0 for a row from before;
  `WorldMap::adoptLearned`, a log line per pilot).
- Frontier pilots do buy and build (the doc said otherwise): they follow
  the same model. A frontier pilot buys a blueprint once at the first
  copy's price and learns it at once, learns a copy it found before it
  builds, builds for ore, and a known kind found pays its ore into its own
  wallet; schema 28 learns their bought, built and held kinds too (their
  JSON's `"learned": true` key).
- As built (server, net): `WorldMap::learn` (WorldShop.cpp) in the request
  chain; `WorldMap::giveBlueprint` for every find (sites, chips, missions,
  the Matriarch, canyon waves): a copy, or the ore price of a known kind,
  told as the blueprint news with `by` "known" and `value` the ore; the
  dev lines that gave copies (`--dev-blueprints`, `defence blueprints`, the
  rig's) learn instead; `ProgressMessage::learned`; `bought` is no longer
  written or read by the server.
- docs/prospector-progression.md "The Citadel shop" rewritten to this.

## 5. The radial build wheel (game)

B opens a wheel round the middle of the view's glass (`SAcPilotOverlay::PaintWheel`,
sized to fit it): one slice per kind of `AcPilotText::BuildKinds` (the order
of `blueprintKinds()`: Citadel, Landing Pad, Derrick, Gatling Gun, Hab Dome,
Sentinel, Foundry, Lab, Spacedock, Bastion), the first at the top and the
rest clockwise, then the Target Dummy last. The first nine carry their number
key; the dummy has none. The pointer stays hidden and captured: the mouse's
motion since B moves a virtual stick (`FAcBuildWheel`, AcBuildWheel.h; the
pawn offers its motion to `OnLookTaken` first), held inside a reach of 120
points so a turn of the hand swings it round at once; past a dead zone of 40
points the slice under its direction is the pick, drawn lit and pushed out,
with the kind's name, cost, and its state in the hub (LEARN, LV 8, NO
BLUEPRINT, REALM LIMIT, NEEDS 100 ORE, ×N COPIES). The reticle gives way to
the wheel while it is open. Click (the act key offers "confirm" to `OnKey`
first), or releasing B when it was held over 0.25 s, confirms: a buildable
kind goes to placement as today; LEARN sends `Learn` and keeps the wheel
open; a locked slice does nothing but note why. Esc or B closes. The number
keys still pick directly. The wheel is Slate, in the pilot overlay's layer;
no new input assets. Dev: `ac.PilotWheel DX DY` moves the stick, `ac.PilotKey
confirm` clicks.

## 6. The gun feel (game)

- **Tracers** (an effects extension, `AcEffectsRounds.cpp`): every `Fired`
  is a red-hot tracer: an orange-red emissive body 1.5 cells long and 0.045
  thick, a yellow-white core 0.02 thick along its front 60%, an additive
  orange-red halo 0.12 thick round the body and a dimmer tail 1.5 cells on
  behind it, all emissive well over daylight; flown on the core's own parabola
  (`FAcRoundFlight`, AcRounds.h, calls `Rules::roundAt`: one integrator,
  no constant copied) until its `Impact` or `roundLife`. It leaves the
  model's muzzle (the turret as posed), the offset to the core's muzzle
  fading out over 0.15 s. A muzzle flash every round, a puff of smoke every
  tenth, a spent brass casing every fifth (thrown sideways, tumbling, 0.7
  s; pools of 256 streaks, 64 casings). A muzzle within 3 cells of the
  camera (the gun the pilot mans) makes no flash, smoke or casing of its
  own: the turret's `turretFlashes_k` show there.
- **Impacts** by `what`, scaled by the round's speed there: ground: an
  orange flash swelling at its root, hot sparks thrown off the mirror of the
  round's line that streak along their motion, arc and fall, a scorch, and a
  dust plume for one strike in five (a stream's sunlit puffs pile up into a
  white cloud that hides the impacts: each flash, spark and plume is faint
  on its own for the same reason); a building or rock: a flash, a glow and
  sparks; a bug: a hit flash and twelve flecks of green ichor; a Target
  Dummy: a flash and sparks, its rings lighting in turn (its pose) and
  "HITS n" over its bar. The Prospector's helmet lamp is off at a gun (it
  pooled on the aim).
- **Poses:** the corner guns' and the gatling's barrels spin with the
  core's `spin` (30 rad/s at full rate), the tips (and the gatling's hopper)
  glow with its `heat`, the muzzle flash shows every other frame while it
  fires (spun up or winding up, not overheated). The Target Dummy's `plate`
  falls back on its hinge in 0.14 s at `downUntil` and swings up in about
  0.55 s; each strike lights the next of `rings_0..2`, fading over 0.5 s;
  the `beacon` blinks, red and quick while down. The Citadel's
  `consoleLamps_k` are lit once it is complete.
- **Sound** (AudioDirector rules): `rotaryfire`, a seamless loop per gun
  (key: the shooter and the corner gun nearest the muzzle), in 0.03 s with
  its first round, out 0.08 s after its last (no round for 0.12 s), the
  nearest 6 heard; `rotaryspinup` with a burst's first round,
  `rotaryspindown` when it stops; `roundground`, `roundmetal` (a building, or
  a unit that is not a bug), `roundchitin` (a bug) at each impact, 10 a
  second at most. Made with ElevenLabs sound effects
  (Tools/AudioGen/guns.manifest.json, picks levelled by guns_pick.py into
  unreal/Resources/Sounds/guns/). The `turretburst` and `gatlingburst` cues
  and the amalgam streaks are retired for these guns (they send no `Shot`).
- **The feed (C1):** the heat pips are the gun's own heat
  (`CornerGun::heat`); at OVERHEAT every pip blinks red with the word under
  them, the trigger ignored. The cooldown bar runs from its rounds' times.
  The **ballistic impact marker**: `AcPilotText::BallisticImpact` flies the
  aim's round (the gun's muzzle, `Rules::roundVelocity(yaw, pitch)`) at 240
  Hz to its first hit (the ground, a standing building's box; its own
  Citadel passed for `roundSelfGrace`), bisects the crossing, and the feed
  draws an orange ring there with the distance ("34 c"). The marks line
  reads "30/s · 1.0 · V0 90" (rounds a second, damage a round, muzzle
  speed). A shake while firing: the view turned by about 0.6 px of a
  1080-line view, alternating at 60 Hz, while the trigger is held with the
  barrels spun up. The driven Prospector's Strike line gives the cutter's
  damage and reach in cells (its weapon's, "5 damage, reach N cells").
- **Camera:** the gun view keeps the barrels in frame (C1).

## 7. Tests

| Layer | |
|---|---|
| core | `round_fliesOnABallisticArcAndDies`; `round_hitsTheFirstThingOnItsPath` (a unit, a building, the ground); `round_damageLandsAtImpactNotAtFire`; `gun_spinsUpAndOverheats`; `gun_firesAlongItsAimWithNoRangeGate`; `gatling_aiLeadsAndFiresRounds`; `dummy_isTargetableByItsOwnSideAndStandsAgain`; `dummy_buildsForTenOreWithNoBlueprint`; `blueprint_learnedOnceThenBuildsForOre`; `round_replayIsDeterministic` |
| net | `Fired`, `Impact`, `DummyDown`, `Learn`, `Structure::hits/downUntil/heat`, `CornerGun::heat` round trips; widths (as built: `net-tests/tests/GunsWireTests.cpp`, with the learned kinds, overheat and spin) |
| server | `learn_consumesACopyOnce`; `shop_sellsABlueprintOncePerPilot`; `site_givesOreForAKnownBlueprint`; `load_schema28LearnsWhatWasBoughtOrBuilt`; `build_dummyNeedsNoBlueprint`; `rounds_reachEveryClientThatMayKnow`; `citadel_mannedGunHitsAtThirtyCells` (the user's case) (as built: `LearnTests.cpp`, `ShopTests.cpp`, `CitadelTests.cpp`; also `frontier_buildsAfterSchema28`, `dev_dummyHereAheadOfTheGun`, and `defence_citadel_turrets_beat_a_raid` with a led ballistic aim) |
| game | `Autocraft.Effects.RoundFliesTheSamePathAsTheCore`; `Autocraft.Hud.BuildWheelPicksBySector`; `Autocraft.Hud.FeedImpactMarker`; `Autocraft.Pose.MinigunSpinsUp` |
| flow | `-AcFlowTest=mmopractice` (AcMmoPracticeFlowTest.cpp): dev rig; the wheel leaned onto the Target Dummy, a click holds it out, the act key puts it down off the pad; `citadel here`, the door opened on the act key, into the hall to gun 1's station, the gun taken; the gun turned where the round's path 15 cells out is clear, `dummy here 15`; a burst into the ground short of it; the look onto its plate by `Rules::ballisticPitch`, fire until it falls (hits counted, DOWN over its bar), it stands again with its hits cleared; pictures (`-AcMmoShotsC2`: c2-wheel, c2-ground, c2-tracers, c2-impact, c2-dummy-down, all from gun 1's feed) |

## 8. Who owns what

| Worker | Owns | Lands first |
|---|---|---|
| core (c2-core) | `AutocraftCore`, `core-tests` | the headers: `Round`, `GameState::rounds`, `Fired`/`Impact`/`DummyDown`, `StructureKind::dummy` and its rules, `Structure::hits/downUntil/heat`, `CornerGun::heat`, `PilotProgress::learned`, `targetable`, `freeBuild` |
| net+server (c2-netserver) | `AutocraftNet`, `net-tests`, `server` | protocol 37, schema 28 |
| game (c2-game) | `Source/Autocraft` | the wheel; then the rounds' effects |
| model (the C1 z-fight worker, after its fix) | `citadel_turrets.py`, `gatling.py`, `dummy.py`, their uassets and catalog entries | the minigun |

The driving-feel worker (tps-feel, wt-1) works in `Pilot.cpp` (the walk),
`AcPilotChase`, `AcPilotWalls` and `Rules::footprint`: not these files.

## 9. Limits per region and demolish (C3)

The user, 2026-10-19: "there should be no limit on the number of buildings
I build, or at least, the limit should be per region not per realm", and
"there is no way of demolishing buildings, so once the limit is reached I am
toast".

**Limits per region.** Every building limit counts the realm's buildings of
that kind in the pilot's region, finished or not, never across the realm
(`Progression::perRegion` is true for every kind; the function stays so a
kind could be counted across the realm again). The numbers are the old ones,
now a region's: Derrick 4, Hab Dome 3, Foundry 1, Lab 1, Spacedock 1,
Bastion 2, Sentinel 4, Gatling Gun 6, Landing Pad 2, Citadel 1, Target Dummy
12; Overbuild still adds to the Hab Dome, the Derrick and the Sentinel. The
server counts the region's in every place it counts a limit (the build, the
frontier's builds, the contracts' build list, the goals, the World message,
the schema-10 blueprint migration: `WorldMap::limitCount`). The refusal
says so: "Gatling Gun limit reached: 6 in this region".

**Demolish.** A pilot takes down any building of the realm in its region,
finished or a scaffold:

- **Driving, first or third person:** the building under the crosshair,
  within the repair reach (`Simulation::demolishReach`: `workReach` from the
  building's edge, 1.1 cells and Field welder's reach; the server allows
  1.5 cells more for the trip), and **E held for 1.5 s**
  (`Rules::demolishHold`). The cockpit shows "HOLD E · DEMOLISH GATLING GUN ·
  +62 ORE +12 MH" (no MH part when none comes back) over the act's prompt,
  and an orange ring filling round the reticle while E is down. Letting go
  sooner, or looking off the building, does nothing.
- **The key split:** only E holds to demolish. The click and Space keep
  every act they have (fire, weld a scaffold, mend, enter a Derrick), so
  firing at a Target Dummy or welding a scaffold never takes it down. Since
  C5 ([acting.md](acting.md)) E holds to demolish only when no act's
  box (a door panel, a console, a call post) is under the crosshair, and the
  point hit is within `ac::Act::reach` of the body.
- **Top-down, nothing driven:** a selected building's card has one button,
  DEMOLISH, held down 1.5 s with the mouse (a bar fills along its foot); it
  reaches any realm building in the pilot's region.
- **The refund:** half the build price, `Rules::salvageShare` 0.5 of the
  world's price, whole units rounded down (`Simulation::salvage`), into the
  demolishing pilot's own wallet (the region bank that pays for buildings),
  not the realm wallet. The price is paid in full when a building is
  placed, so a scaffold gives back the same half as a finished building: a
  Gatling Gun 62 ore and 12 MH, a Target Dummy 5 ore, a Citadel 200 ore.
  The welding XP stays: the half price lost on every build-and-tear-down is
  the anti-farm cost.
- **Refused:** the realm's last finished Citadel ("The realm's last Citadel
  stays: a home must remain"); a Citadel with a unit in its hall or doorway
  ("Someone is inside") or at its guns ("Someone is at its guns"); out of
  reach ("Too far away"). A frontier pilot repairing or welding it simply
  stops. Frontier pilots never demolish.
- **What happens:** the building leaves at once (`Simulation::demolish`:
  what a fallen building leaves behind is set right, `Simulation::leave`),
  with `Destroyed` for its shatter to everyone who may know and
  `Demolished{structure, ore, hydrogen}` to the demolisher alone (its toast,
  "Target Dummy demolished · +5 ore"). Its builder's credit and its kills
  go, the next save leaves it out, and the region's count falls, so the
  next build is allowed.
- **The wire:** the request `Demolish{structure}`, index 25 of the 5-bit
  index; the event `Demolished`, case 22; protocol 40 (save schema
  unchanged).

| Layer | Tests |
|---|---|
| core | `limits_areARegions`; `demolish_salvageIsHalfThePriceWhole`, `demolish_leavesAtOnceAndTellsTheNextStep`, `demolish_aScaffoldStopsItsWelder`, `demolish_aCitadelWithSomeoneInItStays` |
| net | `demolish_protocol_is_40`, `demolish_request_round_trips`, `demolished_event_round_trips`; every event in the all-events round trip |
| server | `roles_limits_are_a_regions` (six gatlings in one region, a seventh refused there and built in the next); `DemolishTests.cpp`: a finished building's half back and the event to the demolisher only, a scaffold's same half, a seventh gatling after one demolished, the reach and the top-down view, the last Citadel and one with someone inside |
| game | `Autocraft.Pilot.DemolishHold` (the hold, letting go, looking off, the target, the words); `-AcFlowTest=mmopractice`: twelve dummies, the thirteenth refused, E held on the pilot's own (let go early first), gone, 5 ore back, the toast, a thirteenth built; pictures with `-AcMmoShotsC3` in art/models/guns/c3-build/ |
