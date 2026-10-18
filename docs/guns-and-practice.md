# Guns and practice (C2)

Status, 2026-10-18: designed; in work by four workers (core, net+server,
game, model). This page is the contract between them, as
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
```

Constants (`Rules.h`), tuned by feel later:

| | Value | |
|---|---|---|
| `roundSpeed` | 90 cells/s | muzzle speed (at 30 cells a round takes 0.33 s and drops 0.55 cells) |
| `roundGravity` | 9.8 cells/s² | |
| `roundLife` | 3.0 s | then it is gone |
| `roundRadius` | 0.08 cells | the hit sweep's thickness |

Stepping (`Simulation::stepRounds`, before the units each tick): for every
round, `velocity.z -= g·dt`, the segment from the old to the new position
is swept against: units (a vertical capsule of the kind's radius and its
model height, 1.5 for ground units unless the kind table says otherwise;
flyers at their altitude), structures (the footprint's slabs for a hall
Citadel, else the bounding square by the body height; the shooter's own
structure excluded for the first 0.2 s), the terrain (the height field:
the segment's end under the ground is an impact), the region's bounds
(gone). The first thing hit takes `hit(victim, damage, false, shooter)`
exactly as a gun hit does today (bug aggro through `bugHits`, kill
credit), or nothing for the ground. Then `GameEvent::Impact`.

Firing: `GameEvent::Fired{shooter, round id, muzzle (Vec3), velocity
(Vec3)}` replaces `Shot`/`Missed` for these guns. The velocity is the aim
(yaw, pitch) turned by the cone roll (`Simulation::roll`, seeded, so a
scene replays the same) times `roundSpeed`. Nothing per tick goes on the
wire for a round: the client integrates the same equations from `Fired`
and ends the streak at `Impact{round id, at, what: ground | unit |
structure, victim}` or at `roundLife`. Protocol 37.

Rate, spin-up and heat on a corner gun and a gatling:

| | Citadel corner gun | Gatling gun |
|---|---|---|
| rounds a second (`cooldown`) | 30 (1/30) | 25 |
| damage a round | 1.0 | 0.8 |
| spin-up `spinUp` | 0.4 s from trigger to full rate (the rate ramps linearly; the barrels spin up) | 0.4 s |
| spread `coneStand` | 8 mrad, `coneFire` +0.3 mrad a round, recovery 0.4 s | same |
| heat (`CornerGun::heat`, `Structure::heat` for the gatling) | +0.012 a round, cools 0.35/s; at 1.0 the gun stops ("OVERHEAT") until it cools to 0.4 | same |
| range | none: the round flies until it hits or dies | the AI aims a ballistic solve at targets within 20 cells |

The realm's marks keep their meaning: rate marks scale rounds a second,
damage marks the damage, range marks the gatling's AI reach (+1 cell each)
and the corner gun's nothing (the user sees the drop instead).

The manned corner gun (C1) loses its range gate: it fires along its aim
whenever the trigger is down and the gun is not overheated; the crosshair's
target is only the HUD's business.

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
still 4). Rules: cost 10 ore, 0 MH; `buildTime` 2 s; radius 0.5 (1×1);
hp 150; `height` 1.8; no blueprint (`Rules::freeBuild(kind)` true, the only
one; `buildRefusal` skips the blueprint and floor for it); limit 12 a
region; not a turret; walkable no; the Brood ignores it (`priority` 0,
never a raid target); no vision.

**Targetable by its own side:** `Simulation::targetable(const Unit* or
Structure*, side)` = hostile, or a complete dummy that is up. Used in
`crosshairSight`, `pilotFoe`'s and `pilotSight`'s structure loops, and the
corner gun. Auto turrets keep their hostile-only scan.

**Down and up:** a dummy at 0 hp is not destroyed: `Structure::downUntil =
time + 3 s`, `GameEvent::DummyDown{id, hits}`; while down it is not
targetable and rounds pass through it (the plate lies back); at `downUntil`
it stands with full hp. `Structure::hits` counts impacts since it last
stood (sent in the snapshot; the life bar shows "HITS 37").

The build wheel offers it with no tag; placement as any building.

## 4. Blueprints learned once (core, server, game)

- `PilotProgress::learned` (a set of `StructureKind`) replaces the per-build
  copy. `blueprints` (the copies held) stays as the inventory of unused
  blueprints; `bought` and the "dearer each copy" price go.
- **Learning:** `RequestMessage::Learn{StructureKind}` (protocol 37, index
  24): the pilot holds a copy, is not learning what it knows ("You know the
  Derrick"), the copy is consumed, the kind is learned, a toast "DERRICK
  LEARNED". The build wheel shows an unlearned kind with copies as LEARN
  (selecting it learns, then the wheel stays open on it to build); keys 1–4
  stay items.
- **Building:** `buildRefusal`: free kinds always; else learned, else "No
  blueprint: buy one at a Citadel (H)" if no copy, "Learn it first (B)" if a
  copy is held; then the level and the limits as today. No copy is consumed
  by a build.
- **The shop:** sells each blueprint once per pilot at its first-copy price
  (`Shop::blueprintPrice(kind, 0)`), refuses "You know the X". **Sites,
  chips, missions, the Matriarch:** a blueprint of a learned kind gives the
  blueprint's ore price instead (the message says so).
- Schema 28: on load, a pilot learns every kind it ever bought, owns a
  building of, or holds a copy of; the copies of learned kinds are removed.
  Frontier pilots: unchanged (they never build).
- docs/prospector-progression.md "The Citadel shop" rewritten to this.

## 5. The radial build wheel (game)

B opens a wheel at the screen's centre: one slice per kind of
`blueprintKinds()` plus the dummy, in floor order starting at the top and
going clockwise (Citadel, Derrick, Target Dummy, Landing Pad, Gatling,
…). The pointer stays hidden and captured: the mouse's motion since B
moves a virtual stick; past a dead zone of 40 px the slice under its
direction is the pick, drawn lit with the kind's name, cost, and its state
under it (LEARN, LV 8, NO BLUEPRINT, REALM LIMIT, ×N copies). Click (or
releasing B when it was held over 0.25 s) confirms: a buildable kind goes to
placement as today; LEARN sends `Learn` and keeps the wheel open; a locked
slice does nothing but show why. Esc or B closes. The number keys still
pick directly. The wheel is Slate, in the pilot overlay's layer; no new
input assets beyond what the pointer capture needs.

## 6. The gun feel (game)

- **Tracers:** every round is a red-hot streak (the amalgam pool, recoloured
  hot orange-red with a yellow-white core and a halo), 1.5 cells long,
  flown by the client along the same ballistic path from `Fired` (one
  integrator shared with the core's constants) until its `Impact` or
  `roundLife`. Muzzle flash every round (visible every second round), a
  puff of smoke every tenth, spent casings every fifth from the chute
  (small, short-lived, pooled).
- **Impacts:** ground: a dust plume with sparks and a scorch burst, scaled
  by the round's speed at impact; metal and rock: sparks and a flash; a
  bug: a chitin burst (the Brood's hurt colour) and a hit flash; a dummy:
  a ring lights and the "HITS" count on its bar ticks.
- **Sound:** a minigun loop with spin-up whine and spin-down, from the
  `orc-elevenlabs` skill's sound effects (or the ElevenLabs CLI), looped
  seamlessly; impact sounds for ground, metal and chitin, pooled, with
  distance falloff; the existing `turretburst` and `gatlingburst` cues retired
  for these guns.
- **The feed (C1):** the arc gauge stays; the heat bar becomes real heat
  with an OVERHEAT state (red, the trigger ignored); a **ballistic impact
  marker**: the client integrates the aim's trajectory to its first hit
  (terrain, building) and draws a small ring there with the distance
  ("34 c"); the marks line reads "30/s · 1.0 · V0 90"; a short camera shake
  (0.6 px at 60 Hz) while firing. The driven Prospector's HUD shows its
  cutter's reach in cells where the weapon line is.
- **Camera:** the gun view keeps the barrels in frame (C1).

## 7. Tests

| Layer | |
|---|---|
| core | `round_fliesOnABallisticArcAndDies`; `round_hitsTheFirstThingOnItsPath` (a unit, a building, the ground); `round_damageLandsAtImpactNotAtFire`; `gun_spinsUpAndOverheats`; `gun_firesAlongItsAimWithNoRangeGate`; `gatling_aiLeadsAndFiresRounds`; `dummy_isTargetableByItsOwnSideAndStandsAgain`; `dummy_buildsForTenOreWithNoBlueprint`; `blueprint_learnedOnceThenBuildsForOre`; `round_replayIsDeterministic` |
| net | `Fired`, `Impact`, `DummyDown`, `Learn`, `Structure::hits/downUntil/heat`, `CornerGun::heat` round trips; widths |
| server | `learn_consumesACopyOnce`; `shop_sellsABlueprintOncePerPilot`; `site_givesOreForAKnownBlueprint`; `load_schema28LearnsWhatWasBoughtOrBuilt`; `build_dummyNeedsNoBlueprint`; `rounds_reachEveryClientThatMayKnow`; `citadel_mannedGunHitsAtThirtyCells` (the user's case) |
| game | `Autocraft.Effects.RoundFliesTheSamePathAsTheCore`; `Autocraft.Hud.BuildWheelPicksBySector`; `Autocraft.Hud.FeedImpactMarker`; `Autocraft.Pose.MinigunSpinsUp` |
| flow | `-AcFlowTest=mmopractice`: dev rig, open the wheel, build a dummy 20 cells from the Citadel, enter, man the gun facing it, fire until it falls, it stands again, hits counted; pictures (`-AcMmoShotsC2`: the wheel, the tracers in flight, an impact, the dummy down) |

## 8. Who owns what

| Worker | Owns | Lands first |
|---|---|---|
| core (c2-core) | `AutocraftCore`, `core-tests` | the headers: `Round`, `GameState::rounds`, `Fired`/`Impact`/`DummyDown`, `StructureKind::dummy` and its rules, `Structure::hits/downUntil/heat`, `CornerGun::heat`, `PilotProgress::learned`, `targetable`, `freeBuild` |
| net+server (c2-netserver) | `AutocraftNet`, `net-tests`, `server` | protocol 37, schema 28 |
| game (c2-game) | `Source/Autocraft` | the wheel; then the rounds' effects |
| model (the C1 z-fight worker, after its fix) | `citadel_turrets.py`, `gatling.py`, `dummy.py`, their uassets and catalog entries | the minigun |

The driving-feel worker (tps-feel, wt-1) works in `Pilot.cpp` (the walk),
`AcPilotChase`, `AcPilotWalls` and `Rules::footprint`: not these files.
