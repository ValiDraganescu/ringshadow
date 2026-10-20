# Building sizes (C5)

The user's ask, 2026-10-19: "we need to upgrade all the buildings to
increase the size by the same factor we increased the citadel size (for
example the factory is just too small)". In C1 the Citadel went from 5x5 to
12x12 cells ([citadel-interior.md](citadel-interior.md) section 1), a factor
of 2.4. C5 grows every other building of the MMO by the same factor.

## The rule

- Footprint side x 2.4 (`Rules::buildingScale`), rounded to the nearest
  whole cell: 3x3 to 7x7, 2x2 to 5x5. The snap centres odd sides on cell
  centres and even ones on cell corners (`Simulation::snap`), so no kind
  needs a parity of its own.
- Body height x 2.4 (`Rules::height`, what the game's model and pose stand
  to, and what rounds hit).
- Two exceptions, the manager's rulings: the **Landing Pad** is as big as
  the walk-in Dropship landed on it. The ship lands aligned with the pad
  (its ramp to the pad's front, +y; its nose to the back) with its hull
  centred (c5-ship's landing), so the pad is the smallest odd square
  holding the hull's length and width with a 0.25 margin
  (`ShuttleBay::padHalfSide()`): 17x17 for the C5 ship (hull 16.35 long,
  nacelles 4.56 to each side). The ramp's foot lies on the ground about 2
  cells past the front edge. Its plate stays 0.18 high: the ship stands on
  the ground. The **Target Dummy** keeps its 1x1 and 1.8: a practice
  target the size of a unit, not a building.
- The strategy game keeps today's sizes. The switch is the Citadel's hall,
  `Rules::citadelHall()` (one switch for the process, set at every game
  start: on for the MMO and the server, off for the strategy game).
  `Rules::radius`, `Rules::height`, `Rules::footprint`,
  `Rules::addonOffset()`, `Rules::buildReach` and the guns' model numbers
  (`Defence::gatlingScale()`, `sentinelScale()`) all read it, so nothing
  half-switches.

## The table

| Kind | Side now (strategy) | MMO side, `Rules::radius` | Height now | MMO height |
|---|---|---|---|---|
| Citadel | 5x5 | 12x12, 6.0 (C1) | 4.6 | 12.3 (C1) |
| Hab Dome | 2x2 | **5x5, 2.5** | 1.2 | **2.88** |
| Garrison | 3x3 | **7x7, 3.5** | 2.5 | **6.0** |
| Bastion | 3x3 | **7x7, 3.5** | 1.4 | **3.36** |
| Derrick | 3x3 | **7x7, 3.5** | 2.5 | **6.0** |
| Foundry | 3x3 | **7x7, 3.5** | 2.4 | **5.76** |
| Spacedock | 3x3 | **7x7, 3.5** | 3.2 | **7.68** |
| Lab | 2x2 | **5x5, 2.5** | 1.9 | **4.56** |
| Sentinel | 2x2 | 2x2 for now (`Rules::sentinelScale` 1; 5x5 by the rule, see "The guns") | 1.7 | 1.7 (x the scale) |
| Gatling Gun | 2x2 | 2x2 for now (`Rules::gatlingScale` 1; 5x5 by the rule) | 1.3 | 1.3 (x the scale) |
| Landing Pad | 3x3 | **the Dropship's square**: 17x17, 8.5 with the C5 ship (9x9 with the C4 one) | 0.18 | 0.18 |
| Target Dummy | 1x1 | 1x1, 0.5 | 1.8 | 1.8 |

What follows the sizes:

- **The Lab's add-on spot** (`Rules::addonOffset()`): flush on the host's +x
  side and its +y edge, (2.5, 0.5) for the strategy game, (6, 1) for the
  MMO. (The MMO's Lab stands on its own; the playground's add-on uses it.)
- **The guns** (`Defence.h`): each by its own scale, `Rules::gatlingScale`
  and `Rules::sentinelScale` (see "The guns" below): its side x the scale to
  the nearest cell, its height and the numbers read off its model x the
  scale (`Defence::gatlingScale()`, `sentinelScale()`: the gatling gun's
  barrels pivot 1.28 up, 0.3 along the aim, the muzzle 1.28 past the pivot;
  the Sentinel's pods pitch about 1.17 up). Their reaches stay as numbers,
  measured from the edge as before. The gatling gun leads a walker by the
  round's flight from its muzzle, not from its centre (at 2.4 the muzzle is
  3.8 cells out).
- **The build reach** (`Rules::buildReach`, the game's ghost and the
  server's check): the edge and 2.2 cells ahead of the Prospector, a Derrick
  the edge and 1.5 (3.0 from the 3x3's centre as before, 5.0 for the 7x7).
- Everything that read `Rules::radius` follows by itself: placement and its
  flat-ground tolerance (C1's: 0.2 up to a 5x5's samples, then in
  proportion to the span), the spacing between buildings, the ore and well
  clearances, the nav grid's blocking, the door a trained unit or the Mule
  comes out of (the edge + 0.2 + its own half), repair and demolish reach
  (from the edge), the Brood's sight of buildings, the pads' plates
  (`region::touchesPadPlate`), the frontier pilots' spacing.
- **The frontier pilots** search out to 16 cells past a building's own side
  for its spot (16 flat before: a 7x7 needs more room).

## The Landing Pads and the regions' pads

- A pilot calls the Dropship on a realm Landing Pad (or a Spacedock) within
  its edge and 2 cells (`WorldRoles::buildingPadReach`), or Pad link's
  cells past its edge, whichever is farther; the game's
  `FAcMmoClient::NearestPad` says the same.
- A region file's pads grow to the same size: `region::padReach` (the
  plate, the "at a pad" reach) and `region::padRadius` (the flat, clear
  ground the validator asks for) are both `ShuttleBay::padHalfSide()`.
  The validator also asks for the ramp's apron in front of the pad's +y
  edge (2.5 deep, 1.6 to each side of the middle) flat and clear, and keeps
  spawns off the plate. The build clears base rocks off the plate and the
  apron.
- **Pad link** reaches from a pad's edge now, not its centre (the
  manager's ruling: the plate grew past rank 1's 8): its ranks 8, 12, 16
  cells past a Landing Pad's edge or a region pad's plate
  (`WorldMap::padLink`, `padReach` = the plate + it; the client's
  `PadReachNow`). Without it: on the plate, or within 2 of a building
  pad's edge.

What the regions needed (validated at 8.5):

| Region | Edit |
|---|---|
| Brine Pans | the pad "landing" (72, 48) to (71, 50): a cliff 8 cells north of it; the prize ring 4 to 6 out; the Gull tunnel's path ends 7.5 from the well |
| Cold Fork | the prize ring 4 to 6 out, its north mound turned from -72° to -54° (the basin's north wall); the North tunnel's path ends 7.5 from the well |
| Dust Terraces | the prize ring 4 to 6 out; the Dust tunnel's path ends 7.5 from the well |
| Prospect Landing (start) | the Landing crate (70, 42) to (88, 50): off the plate and within sight (12) of where a new pilot now starts, just off the plate toward the ore (c5-ship); the spawn "start" (80, 53) to (82, 59), off the plate |
| the other six | nothing: their pads' ground is flat and clear 8.5 round |

The prize rings: the 7x7 Derrick on the well (half side 3.5) stood on all
five mounds 4 cells out; at 6 out, each mound's 1-cell half length leaves
1.5 cells to walk round the Derrick. A ring round a well is one vein by
rule (`region::ringReach` 6.5, docs/regions.md "Scarcity").

## The guns

Waiting on the user (2026-10-19). The canyon zones (The Fork, the Drowned
Cut, the Roost) were cut for 2x2 guns: arms 5 to 6 wide, narrow mesa
rims. At 5x5 a gun at a rim's lip reaches over the cliff face onto the
arm's floor on the walking grid and pinches the arm (a wave's walkers jam
under it, where its barrels cannot dip); a rim near an arm takes one such
gun, an arm's floor one across. The choice: re-cut the three canyons for
5x5 guns (A), or keep the guns smaller (B). Either is one number each:
`Rules::gatlingScale`, `Rules::sentinelScale` (2.4 for 5x5, 1.5 for 3x3, 1
for 2x2). Until then both are 1.

## Drawn (the game)

- **Every model stands on its footprint.** The renderer stretches each
  building's model by `AcPose::BuildingScale(kind)`: `Rules::radius` over
  the radius the model was made for, across, and `Rules::height` over the
  height it was made for, up. The stretch sits on the placement, so the
  scaffold, the build ghost (`AAcHologram::Show`), the shadows, the rays,
  the shatter pieces and the night lamps all follow it. A size change in
  the core needs no new model. In the strategy game the stretch is one,
  except for models made at the MMO's size, which are drawn smaller there.
- **What reads a building's size reads the core's.** The selection ring,
  the build plate, the map's and the minimap's footprints, the life bar's
  width and height, the hurt building's smoke and fire, the weld sparks, the
  shatter, a Ranger's aim at a wall, the HUD's tag sight and the pilot's
  camera walls (`AcPilotWalls`, the slabs of `Rules::footprint`) all take
  `Rules::radius` and `Rules::height`. None keeps its own table.
- **Night lamps** reach over a building as at the size its model was made
  (`AcLamps.cpp`: the pool and beacon reach x the stretch).
- **Re-made at the MMO's size** (Geometry Script models, made where a plain
  stretch would blow up what stands at a Prospector's scale):
  - the **Landing Pad** (`landing_pad.py`), made at 11x11 and stretched to
    the Dropship's square. Its corner posts with their lamps and beacon, and
    each side's lip and hazard band, are parts of their own. The pose keeps
    them at a Prospector's size at any pad size
    (`AcBuildingPose::KeepSize`), and the edges stretch along their side
    only.
  - the **Foundry** (`foundry.py`, with `scaffold_foundry`), made at 7x7.
    It is the same building with its door 3 wide and 2.5 tall (a Mule
    passes), 12 cm hazard bands, floodlights, vents, windows and a 1 m roof
    railing. A Mule at its door opens it, as a trained vehicle does.
  - The other kinds (Hab Dome, Garrison, Bastion, Derrick, Spacedock, Lab,
    Sentinel, Gatling Gun) are the SceneKit exports stretched whole, so
    their stripes and lamps grow with them (the manager's ruling: the
    stretched set reads well; re-make one as a script if it looks wrong).
- **Ground clutter** (pebbles, tufts) is hidden under every building's
  footprint and comes back once the building is gone
  (`AAcTerrainScenery::ClearUnderBuildings`). Nothing shows through a
  Landing Pad's deck.
- **Pictures:** `-AcFlowTest=mmosize -AcMmoShots=DIR` with the server's dev
  line `street KIND|all|door` (WorldStreet.cpp). It shoots a street of every
  kind down from a Citadel's door, then each kind beside the Prospector from
  a camera framed for its MMO size, and the Foundry's door open on the
  Mule. Before and after: `art/models/buildings/c5-size/`.

## Saved worlds

A realm Citadel on load empties a mound under it for good (C1's schema-27
step for Citadels grown in place). Since C5 that step asks placement's own
test, the mound's middle or corners under the Citadel's square: before, it
took the square and one cell more, so a Citadel placed by the rules beside a
vein (9 cells from a mound's middle, the "Too close to resources" circle)
emptied a mound at its diagonal on the next load.


No migration (the manager's ruling): a saved building keeps its position
and takes the new size on load, as C1's Citadel did. Where it now overlaps
rock or another building it stays: placement is checked only when a
building goes up. The user starts a new world for C5.

## Placement on the regions

How many spots every 4 cells across a region take each size, MMO size
against the strategy game's (`BuildingSizesTests.cpp`, the First Reach's
nine regions; the ordinary rules: open, level ground, off the pads' plates,
clear of ore, wells and rock). Measured with the C5 Dropship's 17x17 pad
and plates (8.5); the 7x7 column is the Garrison, Bastion, Foundry and
Spacedock, the 5x5 the Hab Dome and Lab.

| Region | Citadel 12x12 | 7x7 | 5x5 | Landing Pad 17x17 |
|---|---|---|---|---|
| Prospect Landing (start) | 90 / 337 | 279 / 433 | 343 / 496 | 22 / 433 |
| Ashen Shelf | 95 / 286 | 237 / 379 | 296 / 437 | 17 / 379 |
| Brine Pans | 114 / 372 | 308 / 454 | 374 / 503 | 24 / 454 |
| Cold Fork | 163 / 377 | 317 / 446 | 380 / 478 | 45 / 446 |
| Dust Terraces | 73 / 260 | 188 / 332 | 262 / 397 | 25 / 332 |
| Glass Flats | 233 / 452 | 421 / 532 | 467 / 556 | 108 / 532 |
| Rust Hollow | 103 / 338 | 287 / 432 | 345 / 466 | 21 / 432 |
| Slag Pockets | 170 / 417 | 381 / 510 | 428 / 561 | 64 / 510 |
| Tern Ridge | 92 / 329 | 256 / 429 | 334 / 480 | 27 / 429 |

Every region still takes every kind. The 7x7s keep about two thirds of
their spots and the 5x5s about three quarters; the Landing Pad is the tight
one: 17 to 108 spots a region (a pilot builds two a region). Cinder Steps,
a generated example in no world, takes no 17x17 pad at all.

## Tests

- Core: every core test runs as before; the strategy AI's tests on the home
  map (Commander, Directives, the stall and the rally tests) run with the
  hall off (`simtest::CitadelHall`), as the strategy game does. The MMO's
  tests that placed buildings by hand at fixed spots now measure from the
  edge (`GunsTests`, `DefenceTests`, `AimTests`, `PilotSlideTests`,
  `ProgressionEffectsTests`).
- Server `BuildingSizesTests.cpp`: the table (both switches), the pad round
  the Dropship, the Lab's offset, the counts above, no Derrick on ore. The
  rigs that welded buildings at fixed offsets ask the core's rule for the
  nearest spot that takes the bigger square (`rig::standFor`,
  `rig::placeableNear`, CitadelSpot.h); the canyon rigs keep guns back from
  a rim's lip (`rig::offTheLip`, GunAim.h: on the walking grid a footprint
  over the lip pinches the arm below, whatever its size) and lay a wall's
  gap in the floor's middle. Roles and World tests: the Dropship called on
  the pad's edge, refused past its edge and 2, Pad link from the edge.
- The Foundry's Mule rolls out clear of the 7x7 Foundry (core
  `mule_rollsOutOfTheFoundryDoor`).
