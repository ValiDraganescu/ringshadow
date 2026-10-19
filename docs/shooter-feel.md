# Shooter feel

Driving a unit (`Pilot`) should feel like a shooter. Today it feels
sluggish, and the guns reach too short: a Ranger's round flies about five
cells and stops in mid-air. This document compares the drive with the
numbers of the classic tactical shooters, says why it feels slow, and
proposes a range system with a random spread cone for driven guns, and
optionally for the AI's guns too.

Status, 2026-10-12: the range model is built for every gun (see "Built"),
with the Ranger's two guns tuned; the other units' guns have a neutral
default until each is reworked. The feel items (camera smoothing, field of
view, recoil kick, sprint) are not built yet.

## Scale: cells against body heights

One cell is one metre (`AcSpace.h`), and a Ranger stands about one cell
tall: the world is about half human scale. A shooter's player is about
1.83 m tall, so its distances divided by 1.83 give cells, and its speeds
divided by 1.83 give body heights a second. Angles (spread, field of view)
carry over unchanged.

| | The classic tactical shooter | Ringshadow today |
|---|---|---|
| Run speed, body heights a second | 3.0 (rifle out, 5.5 m/s) | 3.15 (Ranger, 3.15 cells/s) |
| Close fights | under 15 m, 8 body heights | |
| A typical rifle duel | 20–45 m, 11–25 body heights | Ranger range 5 cells |
| A long sniper line | 40–55 m, 22–30 body heights | Longbow anchored 13 cells |
| How far a round flies | 208 m (rifle), 104 m (pistol) | the weapon's range: 5 cells for a Ranger, then it stops |
| Sight | the whole lane | Ranger vision 9 cells; enemies past what the side sees are hidden |
| Rifle fire rate | 600 a minute (0.1 s) | Ranger 98 a minute (0.61 s) |
| Time to kill one soldier | 0.1–0.4 s (1–4 rounds) | 4.3 s (8 rounds of 6 against 45 hp) |
| Horizontal field of view | 90–106° | 88° at 16:10 (62° vertical, `AcPilotCamera.h`) |

**The walk is not the problem.** In body heights a Ranger already runs at
a shooter's pace. The slowness comes from everything else:

1. **The round stops at five cells.** With nothing on the sight,
   `pilotMiss` (`Pilot.cpp`) sends the tracer to `position + facing ×
   range`, flat on the ground and ignoring the pitch, so every miss ends in
   mid-air five metres out (about nine metres to the eye, at this scale).
   An enemy on the sight past range takes no hit at all; the readout says
   "Out of range".
2. **Fights take seconds, not fractions of one.** 4.3 s to drop a Ranger
   is ten times a shooter's time to kill, and 98 rounds a minute is a
   sixth of a rifle's cadence. Each click does little and the gun answers
   slowly.
3. **Short sight.** Vision is 9–11 cells, so even a longer gun would have
   nothing to aim at past ten cells unless a friend sees it.
4. **The camera steps at 60 Hz.** The sim runs fixed 1/60 s steps
   (`AcSimSubsystem::StepSeconds`) and the pilot camera follows the
   unit's position with no interpolation (`Alpha` is computed but unused).
   On a 120 Hz display, the mouse turns the view every frame but the
   view moves forward every other frame: a slight judder that reads as
   heaviness. To measure before changing.

## How the classic shooter does range

Two separate knobs, both on a hitscan round (no travel time):

**Damage falloff.** `damage = base × RM ^ (distance / 12.7 m)`, applied
smoothly. A rifle's RM is 0.97–0.98 (98% left at 12.7 m), a pistol's
0.79–0.94, a submachine gun's 0.75–0.87, a shotgun's 0.45–0.70. The round
itself flies 100–200 m before it is deleted. Falloff makes the short guns
weak at range without ever stopping the round.

**A random spread cone.** Each shot leaves along the sight turned by a
random angle inside a cone. The cone's radius is the sum of:

- the gun's stance inaccuracy (rifle: about 6.4 mrad standing, 4.8
  crouched; a sniper unscoped 81 mrad, scoped 2);
- a movement term: none below 34% of run speed, all of it (rifle: about
  175 mrad, 10°) above 95%, linear in between. Moving rifles cannot hit at
  range; that is the rule that makes a shooter feel like one;
- a firing penalty: each shot adds about 7–8 mrad, which decays 90% in
  0.3–0.4 s (`penalty × 0.1^(dt / recovery)`). Tapping stays accurate,
  spraying does not;
- a jump term, large, best near the top of the jump.

The angle round the sight is uniform, and the radius is uniform from 0 to
the cone's radius (not uniform over the disc), so the shots bunch toward
the middle: half of them land inside half the radius. A small constant
weapon spread is sampled the same way and added. The miss at the target
is `distance × angle`, so the same aim that always hits at ten metres
misses at forty.

Other shooters do the same with steps instead of a curve (full damage to
20 m, then a lower band), or with real projectiles that drop and slow
(travel time instead of a cone). For a game whose units are small and
seen from close, the hitscan cone with falloff is the clearest.

## What a cone means at Ringshadow's scale

A Ranger is 0.75 cells wide. The share of shots that hit it, for a cone
of radius θ at distance d (radius uniform, so the share is about
`0.375 / (d·θ)` once the cone is wider than the target):

| Cone | 5 cells | 10 | 20 | 30 | 50 |
|---|---|---|---|---|---|
| Standing, 7 mrad | 100% | 100% | 100% | 100% | 100% |
| Walking, 30 mrad | 100% | 100% | 62% | 42% | 25% |
| Running, 175 mrad | 42% | 21% | 11% | 7% | 4% |

That is the feel wanted: standing still, a Ranger hits anything it can see;
running, it hits what is close.

## How the hybrids reconcile the two ranges

The RTS-and-shooter hybrids that held together (late-nineties games where
the commander sits in one of the army's tanks; team games where one player
commands from above and everyone else is a soldier) all keep **one stat
block per weapon**. A unit a human drives shoots exactly as far and as hard
as the same unit under the AI; the human is already better at aiming. A
range or damage bonus for the driven unit makes the overhead view lie: the
range circle on the map is no longer what the gun reaches in the cockpit.
Where they need a long gun and a short gun, they are different weapons, and
the long one is a slow shell or a long reload, so the range is felt as
travel time.

So the range system below is for **every** gun, driven or not. What the
driver gets that the AI does not is the aim, not a stat.

## Proposal

### 1. One weapon model, driven or not

- **`range` becomes effective range.** Inside it, full damage, as now.
  Past it, falloff: `damage × falloff ^ ((d − range) / range)`, with a
  `falloff` per weapon class (rifles 0.7, pistols 0.5, rockets and shells
  0.85; the flamer, the cutter and the sting 0: they really stop). A Ranger
  does 70% at 10 cells, 49% at 15, 34% at 20.
- **A reach** past which the round is gone: about 4× `range` for guns
  (Ranger 20 cells), the crosshair cast's 60 for shells and rails.
- **A spread cone** per weapon with the four terms of "How the classic
  shooter does range" (stance, movement, firing penalty, jump), sampled the
  same way (uniform angle, uniform radius, a small constant spread added),
  with the simulation's seeded random numbers, so replays and tests stay
  deterministic.
- **The AI rolls the cone instead of aiming it.** Each AI shot hits with
  the share the cone gives at that distance for the shooter's motion and
  firing penalty, and does the falloff damage. A miss still flies: a tracer
  past the target and a puff where it lands. Today every AI shot in range
  hits; standing Rangers at 5 cells keep hitting almost always, so close
  fights stay as they are.
- **Engage range.** The AI opens fire out to about 1.6× `range` (a Ranger
  at 8 cells), where its shots land less often and do less. Long fights
  become possible and costly; the player sees it on the map as a second,
  fainter ring.
- **Vision grows with it**, or nothing is seen at the new reach: about
  +4 cells for every kind (Ranger 9 → 13).

This is a balance change across the roster: every range, vision, the
Bastion and high-ground bonuses, the anchored Longbow's 13, the Sentinel's
7, and the commander's fight estimates. It needs isolated matchup scenarios
(as `NewUnitsBalanceTests`) before it ships.

### 2. The driven gun

- **The round flies on.** It leaves along the view, pitch included, turned
  by the cone, and goes until it hits a unit, a building, a rock or the
  ground, out to the weapon's reach. A miss leaves a puff where it lands,
  never a tracer that ends in the air. An enemy past `range` takes the
  falloff damage instead of nothing; the readout shows the share
  ("FALLOFF 49%") instead of "Out of range".
- **The crosshair shows the cone.** Its brackets open with movement and
  firing and close as the player stops, so standing still to shoot is
  something the player learns from the sight.
- **Cadence.** More, smaller rounds at the same damage a second, for the
  unit itself (the AI's too, so the stat block stays one): a Ranger fires
  2 every 0.2 s instead of 6 every 0.61 s, about 10 a second either way.
  Five times the rounds and the tracers, the same balance. The Comet's
  pistols likewise; the Juggernaut's grenades and the shells stay slow, as
  heavy guns should.
- **A weak spot (decision).** The crosshair cast already tests every model
  part (`AcRaysWorld.h`: a shot between a Ranger's legs misses), but its
  hit (`FAcRayHit`) carries only the unit, so it would also report the
  part. A head or a vehicle's rear could take double damage. That
  rewards aim without giving the driven unit a stat; the AI would roll a
  small share of weak-spot hits to stay even.

### 3. Feel

The run itself is fine: a Ranger runs 3.15 body heights a second, the
tactical shooter's rifle pace (an arena shooter's 5–7 is what reads as
"fast", too fast for this game's fights), and its speed is instant, with no
ramp. The changes are the camera and the feedback:

| | Today | Proposed | Why |
|---|---|---|---|
| Camera position | steps at 60 Hz | interpolated between sim steps | judder on a 120 Hz screen reads as heaviness |
| Horizontal field of view | 88° at 16:10 | 100° default, a setting from 80 to 110 | wider edges stream faster: the same speed reads faster |
| Shot to picture | next sim step | next frame: flash, kick and tracer at once | above about 100 ms between click and picture feels sluggish; 50 ms feels instant |
| Recoil kick | none on foot | a spring of about 1–2° of pitch, back in 0.3 s | the gun answers the hand |
| Muzzle flash | on the effect pool | one or two frames, with a hole on the aim line | must never hide the target |
| Hit confirm | marker, "-6", sound | the same, on every round, plus a short trauma shake when hit | trauma² drives rotation only, decays in under half a second |
| Hit-stop | none | none, or 20 ms on a kill | longer holds read as lag while tracking |
| Sprint (decision) | none | Shift: 1.4× speed, no firing, the cone at its widest, a 5° field-of-view widen | speed when crossing ground, a choice against shooting |

Numbers from the research: input to picture at or under 50 ms feels
instant, around 100 ms is the edge, 150–200 ms is sluggish. Shooters'
rifles fire about 600 rounds a minute, and readable fights kill in 200–300
ms; that is too fast for an RTS, where a Ranger lasting 4 s is what lets a
player react on the map, so the cadence goes up while the damage a second
stays.

## Decisions

Taken 2026-10-12:

1. **One model for every gun**, driven or not.
2. **Each weapon its own cone and falloff.** The Ranger's coil rifle is
   precise far out (a scope upgrade comes later); its chain gun sprays and
   is poor at range. The other units' guns get theirs one by one.
3. **Engage reach 1.6× range, vision +4**, as proposed.
4. **Cadence**: attacks split into rounds at the same damage a second.
5. **No range ring for a soldier on foot** (Prospector, Ranger, Comet,
   Juggernaut), and **no limit on how far a round flies**: the aim is what
   makes long shots miss.

Open: weak spots, sprint, and the feel items.

## Built

The numbers live in `UnitStats` (`Rules.h`): `coneStand`, `coneMove`,
`coneFire`, `coneRecovery` (radians and seconds), `falloff`, `rounds`.

| Weapon | Stand | Move | Per round | Recovery | Falloff | Rounds |
|---|---|---|---|---|---|---|
| Ranger coil rifle | 3 mrad | +120 | +4 | 0.35 s | 0.9 | 3 (2 every 0.2 s) |
| Ranger chain gun (upgrade) | 40 mrad | +120 | +6 | 0.5 s | 0.5 | 1 (its bursts) |
| Every other gun (default) | 10 mrad | +30 | 0 | 0.3 s | 0.7 | 1 |
| Flame, cutter, sting | never miss | | | | stops at range | 1 |

What a Ranger hits, standing still: the rifle every round out past 20
cells; the chain gun every round close by and about half 20 cells off. On
the run the rifle hits a Ranger 10 cells off about a third of the time.

- **The roll** (`Simulation::aimRound`): a uniform angle round the sight and
  a uniform radius out to the cone's edge; a hit if the miss at the
  target's distance is inside its radius. Seeded by the unit and the game
  time (`Simulation::roll`), so a scene plays out the same every run.
- **The cone** (`Simulation::cone`): standing below a third of full speed,
  all of the moving extra from 95%; each round adds `coneFire` to
  `Unit::spread`, which fades by 90% every `coneRecovery` s (`upkeep`). The
  driven unit's speed is `Simulation::pilotPace`; an AI soldier closing in
  fires as moving, one standing as still.
- **Falloff** (`Simulation::falloffShare`): past range, `falloff` for every
  further range flown. A weapon that stops at its range does all its
  damage where it strikes.
- **Rounds**: armour comes off the whole attack, then the damage is shared;
  the rounds keep their beat across 1/30 s steps (`reload`), so the damage a
  second is the old one. A retreating soldier fires one round per attack's
  time, so it stops no more often than before.
- **Engage reach** (`Rules::engageReach`): an AI soldier fires on the move
  from 1.6× its range while closing in; an anchored tank fires at what is
  in engage reach. Vision is 4 cells longer for every unit (`Rules::vision`,
  `Rules::sight` 13), and workers run as far behind their Citadel
  (`Commander::fleeDistance`).
- **The driven gun**: the crosshair ray is cast out to 120 cells
  (`AcPilotAim::ShotReach`, past the day's fog veil); an enemy under it is a
  target at any distance, for the falloff share. A missed round is cast
  from the eye along the view turned by the miss's angles
  (`GameEvent::Missed::yaw/pitch`, `UAcEffects::DrivenMiss`) to whatever it
  meets; the tracer is a streak of at most 8 cells. A soldier's reticle
  brackets open with its cone.
- **Goldens moved** (2026-10-12, reviewed): `vision.json` (every sight set
  a strict superset of the old, the fixed-radius sights unchanged) and three
  scenes of `simulation.json` (three rounds an attack, misses; the same
  deaths in the skirmish and the playground).

Known: `balance_aScorpionLosesToWhatSpotsItFirst` fails. With a Sentinel
spotting the buried Scorpion, four Rangers now see it from 12 cells, walk in
together to their range of 5, which is the sting's reach, and one sting with
its splash kills all four.

## Third person (2026-10-18)

The user, driving in third person: the view needed a wheel to look over a
building site; the unit sat left of the centre and did not go where the
cursor pointed; it kept getting stuck on rocks and scenery.

**Zoom.** The mouse wheel zooms the chase camera (`IA_PilotZoom`,
`FAcPilotChase::FZoom`): the boom times a factor in [0.45, 2.6], 12% a
notch, at most three notches an event (a trackpad's flick), eased to 95% in
0.15 s. Past 1 the camera tilts down, smoothly, up to 25° more than the mouse
pitch at 2.6: the far end is a view over the site. The factor is kept in
`UAcSettings::ChaseZoom` when a ride ends. First person, at a gun, with the
pointer out or the world map open, the wheel does nothing. While a building
is held out the boom eases out to at least 1.8 (12.5° of tilt), because the
centred unit hid the ghost right ahead of it at 0.45; the player's zoom comes
back once the building is placed or dropped.

**The boom across regions (2026-10-19).** The user, riding the Mule after a
flight: V to third person put the camera far overhead, the Mule a dot, and
the wheel could not zoom in. The boom is `(2.6 + 3.3·reach)` cells, the reach
measured once per model, the first time it is chased. The measure read the
parts' local transforms through `AcRaySpace::Matrix`, which takes the
seamless world frame's origin off (right for a world transform, wrong for a
part's local), so a model first chased after a region change reached as far
as the frame had moved: the Mule 346 cells after the flight (+144, +96), a
Prospector 382 after walking across regions. Locals now go through
`AcRaySpace::LocalMatrix` (the chase's reach, the ray shapes' rest reach), and
the reach is the same in every region.

**Centred, and the walk follows the camera.** The boom's pivot sat
0.4 + 0.45·reach cells right of the unit (0.85 for a Prospector). The unit
faces what the sight falls on, so at close range its forward was the line
from the unit to that point, a visible angle off the camera's forward, and W
walked along it. Now the offset is `ac.Chase.Shoulder`, cells at reach 1
(0.12 by default; 0 centred; 0.85 the old boom), grown with the reach as
before. On foot in third person W is the camera's forward across the ground
and A/D its right (`AcPilotAim::ChaseWalk`), turned into the unit's frame
and pre-scaled for backing (×0.6) and sidestepping (×0.8) (`Pilot::walkAlong`),
so the step runs exactly along the camera's line whichever way the unit
faces. Walking with the act key up and no target under the sight, the unit
faces the camera's yaw (`AcPilotAim::ChaseHeading`); with act held or a target
under the sight, the crosshair's point, as before. The turn between the two
eases over about 0.06 s; the mouse's turns stay immediate. The eye, the shots
and the sight are as before; first person and vehicles are unchanged. All of
it is the client's: the server takes the walk and the heading as it always did.

**Sliding along what blocks.** `Simulation::pilotWalk` tried the step, then
its x, then its y: world-axis slides, poor against a round rock or a slanted
edge. Now, blocked, `pilotSlide` tries in order: the step projected on the
blocker's tangent at the contact (`pilotContact`: a rock's circle, a
building's wall or door slab, the hall's furniture, an ore deposit's turned
box, each by its point nearest the unit; for a cliff, a well, a region's
wall or edge, the way the open ground round the unit lies); the old axis
slides; the step turned 25°, then 50°, the side away from the blocker first.
Never a step with a negative dot against the one wanted, and never onto
ground the unit can't stand on: in a concave corner it stands still.

**The rocks' teeth.** The map's rocks blocked a driven unit by the nav
grid's half-cell cells (a cell out when its centre is within the rock's
circle + 0.35): a staircase round each rock. Measured on the home map
(10 open rocks × 360 directions), a walker stopped from 0.005 to 0.680 cells
past the footprint circle, 0.312 on average. The footprints aren't padded
past the models (a rock's 0.55 against its model's half size of 0.56 to
0.71; a boulder's 1.15 plus a 0.45 lobe against 1.5 by 1.2): the teeth were
the trouble, and they caught the slide. Now `NavGrid::rocky` marks the cells
left out only for the map's doodads, and there the rock's circle grown by
0.35 decides (`NavGrid::rockAt`): exactly 0.35 all round. A cliff jump lands
only where the pilot can stand (`Simulation::landing`, `driven`). The AI's
grid is unchanged; a unit handed back where it has no ground (beside a rock,
or nearer a wall than its cells allow) steps onto the nearest free ground
(`Simulation::take`), so the AI walks on and `trapped` never takes it for
boxed in. The Prospector up against a rock, its centre 0.359 off
the circle: art/perf/tps-feel/tps-rock.jpg.

The hall's gun consoles block the walk and the cameras too
(`Footprint::furniture`, docs/citadel-interior.md section 2).

**Look around.** The user: holding the right button should turn the camera
the way it does in the top-down view, "just like a look around", without
moving the crosshair. In third person, with the mouse taken and not at a
gun, the right button held swings the chase camera round the unit by the
mouse (`FAcPilotChase::FOrbit`): across at the look's gain, up and down at
half of it, held to ±20° and to the unit's pitch limits. Nothing else moves:
the pawn still follows the aim's view (the camera's yaw and pitch from before
the drag, `PilotYaw` and `Cam.Pitch`), casts the crosshair's ray from it,
faces the unit by it and turns W into its frame. The swung view is a second
`FAcPilotChase::Follow` at the swung yaw and pitch, and it is the one shown.
The crosshair is drawn where the aim's point falls in it (`ScreenOffset`, as
the gun marker), and hidden when that point is off the view; the prompt and
the work ring stay in the middle. Let go, the swing eases back to none over
0.25 s (a smoothstep, the short way round). Before this the right button did
nothing while driving: the pilot's context binds no right button.

Tests: `Autocraft.Pilot.ChaseZoomClamps`, `Autocraft.Pilot.WalkFollowsTheCamera`,
`Autocraft.Pilot.LookAroundKeepsTheAim`, `Autocraft.Pilot.ChaseBoomAfterARegionChange`
(and `mmoferry` out on Cold Fork's pad),
`Autocraft.Pilot.ChaseStopsAtWalls` (the console); core `pilot_slidesAlongARock`,
`pilot_roundsACorner`, `pilot_standsAtEveryStationButNotInTheConsole`,
`pilot_handedBackBesideARockStepsOntoTheGrid`.
Pictures: `-AcFlowTest=mmocitadel -AcMmoShots=DIR -AcMmoShotsTps`.
