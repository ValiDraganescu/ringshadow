# Region files

A region of the MMORPG world ([mmorpg.md](mmorpg.md), "The world") is a JSON
file in `unreal/server/regions/<id>.json`. A **base** (the game's map
generator: a style, a size, a seed) and **crafted layers** on top. A
generated region is the base alone. AI agents craft the rest with
`autocraft-region` (skill: `.claude/skills/ringshadow-region-crafting/`).
A world puts region files side by side on a grid: [worlds.md](worlds.md).

Code: `AutocraftCore` `RegionFile.h` (parse, write, build, validate; the
server and the game share it), `unreal/server/region/` (the tool and its
preview), tests in `unreal/server/tests/RegionFileTests.cpp`.

## The tool

```
make -C unreal/server build/autocraft-region
autocraft-region new --style S --seed N --size Z [--id ID] [-o FILE]
autocraft-region validate FILE        problems one a line; exit 1 if any
autocraft-region build FILE           summary (ore, wells, open ground) + problems
autocraft-region preview FILE -o OUT.png [--scale PX]
autocraft-region edges FILE [--depth D]   the ground's level along each edge
autocraft-region format FILE              rewrite it in the tool's layout
```

The preview: north up, a labelled grid every 8 cells, height as colour bands
(level 0 olive, 1 tan, 2 pale), cliffs red-brown, rocks dark grey, plants
green dots, ore violet, MH wells silver, pads yellow rings, spawns green
diamonds, blueprint sites gold squares (named, with kind and blueprint), openings cyan
(their floor level as L0…L4), the border's outline dotted, each problem a red ring numbered as in the list
below the map.

## Coordinates

Cells from the region's **north-west corner**: `[x, z]`, x east, z south
(down the preview). Sizes: small 96×64, medium 144×96, large 200×128,
huge 272×176. Every point must lie inside. In the game's ground coordinates
the region is centred: ground = (x − width/2, z − depth/2).

Levels: 0 low ground, 1 high ground, 2 higher, up to 4 for a landmark's
mesa or spire. One level is 2.4 high.

## The file

```json
{
  "id": "cinder-steps",                 // slug: a-z 0-9 -
  "name": "Cinder Steps",
  "size": "medium",                     // small | medium | large | huge
  "base": {"style": "highlands",        // highlands | openField | ridge | badlands
           "seed": 1,                   // whole number; leave out for the game's own RTS map
           "drop": ["resources", "doodads"]},  // optional: the base without its ore and wells / rocks and plants
  "ground": "rust",                     // optional: the ground's palette (below)
  "height": [ ... ],
  "deposits": [{"at": [40, 30], "amount": 1800, "angle": 90}],
  "wells": [{"at": [52, 30], "amount": 2250}],
  "doodads": [{"kind": "boulder", "at": [60, 70], "rotation": 30, "scale": 1.5}],
  "pads": [{"name": "Landing", "at": [72, 48]}],
  "spawns": [{"name": "Gate", "at": [72, 56]}],
  "sites": [{"name": "Lander wreck", "kind": "wreck", "at": [50, 30], "blueprint": "derrick"}],
  "openings": {"north": [[64, 80]], "west": [[40, 56]]}
}
```

Only `id`, `name`, `size` and `base.style` are required. Unknown keys are
errors (with the nearest known key). Comments are not JSON: leave them out.

## height

Edits apply in order, each on the ground the ones before left, over the
base's plateaus and ramps.

| op | keys | does |
|---|---|---|
| `raise` | `area`, `level`, `slope`? | ground in the area up to the level (never down) |
| `lower` | `area`, `level`, `slope`? | ground in the area down to the level (never up) |
| `flatten` | `area`, `level`, `slope`? | ground in the area exactly at the level, no roll |
| `ridge` | `line` [[x, z], …] ≥ 2, `width`, `level`, `slope`? | a wall along the line, `width` across its top |
| `ramp` | `from`, `to`, `width`, `fromLevel`, `level` | a drivable slope from `from` (on `fromLevel`) to `to` (on `level`) |

- `area`: corners `[[x, z], …]` (≥ 3), `{"rect": [x0, z0, x1, z1]}` or
  `{"circle": [x, z, r]}`. Everything inside the outline is at the level.
- `slope`: cells past the outline (a ridge: past its sides) the ground takes
  to reach the level. Left out or 0: a **cliff** (1.2 cells, not drivable).
  Drivable from about 4 cells a level.
- A ramp cuts through cliffs written before it. Its ends must stand on their
  levels 1.5 cells past each end; it needs about 5 cells of length a level.
  Write it after the cliffs it crosses.

## Resources and props

- `deposits`: an ore patch, 2×1 cells. `amount` 1…100000 (the bases use
  1800 and 900). `angle` degrees, the long axis from east toward north.
- `wells`: an MH well; a Derrick stands on it (needs flat ground 1.5 cells
  round). `amount` 1…100000 (the bases' hold 2250).
- `doodads`: `kind` rock | boulder | rockSpire | crate | crateStack | plant |
  bush | debris | deadTree | tower, or a landmark (below); `rotation`
  degrees; `scale` 0.25…4. Plants, bushes and debris are walkable; the
  rest is solid.
- The base's rocks and plants give way to what the file puts down: deposits,
  wells, pads, spawns and opening lanes clear them.

## ground

The palette the game paints the region's ground with (W3), so crossing an
edge feels like arriving somewhere new. Left out: `plain`, the game's own
brown dirt.

| ground | looks like | regions |
|---|---|---|
| `plain` | the brown dirt | Prospect Landing, Tern Ridge, Cinder Steps |
| `rust` | rust-red earth, darker iron stains | Rust Hollow |
| `salt` | pale salt crust in plates, grey brine in the cracks | Brine Pans |
| `ash` | soft grey ash, paler flecks | Ashen Shelf |
| `glass` | dark glassy ground in facets that catch the sun | Glass Flats |
| `slag` | black slag, cracks that glow like embers (more at night) | Slag Pockets |
| `frost` | cool blue-grey stone, frost on it from night to mid-morning | Cold Fork |
| `ochre` | ochre in bands by height, the cliffs banded like strata | Dust Terraces |

The game blends the last 16 cells to each edge halfway toward the
neighbour's palette (from the World message), so both sides of an edge meet
in the same colour, and the scenery past the edge is the neighbour's.

## Landmarks

Four doodad kinds that make a place: put a few where they add to the
region's landmark, not as clutter. Their feet are solid (`NavGrid::footprint`).

| kind | is | size at scale 1 | feet |
|---|---|---|---|
| `arch` | a natural stone arch | 7 across, 5 high | two legs 5.2 apart: drive under it along its own z (rotation 0: north–south) |
| `sunkHauler` | a dead ore hauler half sunk in the ground, nose down | 5 long | its length along x |
| `derrickRuin` | the steel lattice of a derrick tower, bent, a fallen piece beside it | 9 high | 2.5 across |
| `relayDish` | a crashed relay dish on its side, its mast snapped | 4 across | the dish and its mast |

The First Reach has two in each region (the region files' `doodads`). Glass Flats' Glass Needle rises to level 4
and Brine Pans' Brine Stack to level 3 (sheer, under their rock crowns).

The models: `unreal/Tools/Editor/models/landmarks.py` (concepts in
`art/models/landmarks/concepts/`).

Pictures: `-AcMmoShots=DIR -AcMmoShotsW3` (AcMmoShots.cpp) visits each region
of `-AcMmoW3Regions=a,b,c` (default start, rust-hollow, glass-flats,
slag-pockets) by pad flights and shoots it toward its nearest landmark at
noon and at dusk, and the start region's south edge looking into Brine Pans.

## Places

- `pads`: named landing pads, radius 3: flat (heights within 0.6), no
  cliff, rock, ore or well on them, joined to the open ground.
- `spawns`: named points where a pilot appears: open ground joined to the
  rest.

## sites

Blueprint sites ([prospector-progression.md](prospector-progression.md),
"Blueprint sites"): each holds one building's blueprint, and every pilot finds
it for themselves.

- `name`: unique in the region. `kind`: `crate` (walk into it), `wreck`
  (weld it open), `cache` (buried; drill it out), `vault` (on high ground).
  `at`: the point. `blueprint`: a building's code name ([naming.md](naming.md)):
  `citadel`, `derrick`, `habDome`, `foundry`, `lab`, `spacedock`, `bastion`,
  `sentinel`. The Garrison is held back, so it has no blueprint.
- A site stands on open ground. It must not be on a cliff face, in rock, on
  ore or a well, on a pad's plate (keep it 5 cells from a pad's centre) or on
  another site. It must be joined to the region's open ground. Base rocks
  within 2 cells give way to it.
- A `vault` stands on high ground (level 1 or 2). It may be cut off from the
  open ground when Cliff hop can reach it. `validate` prints how each vault is
  reached ("by a ramp", "by a cliff hop"), and `build` prints how every site
  is. A crate, wreck or cache that only Cliff hop reaches is a problem, and so
  is a vault that neither a ramp nor a cliff hop reaches.
- Settings that suit each kind: a wreck on a rise (`raise` a circle with a
  `slope` of 5), a cache in a hollow (`lower` a circle on high ground, or
  a horseshoe of `ridge` round it on low ground), a vault on a pocket a level
  up (`raise` a circle with no slope), with a `ramp` or with none.

## openings

Spans of each edge (`north` z = 0, `south` z = depth, `west` x = 0, `east`
x = width) a machine drives across to the neighbour region: `[from, to]`
cells along the edge (x on north and south, z on east and west), ≥ 4 long,
not overlapping. The lane 2 cells in must be open the whole span and joined
to the region's main open ground.

A span can carry its passage's floor level (W4):
`{"span": [41, 55], "level": 0}`. The build then lays a flat mouth at that
level from 2 cells past the edge to 5 cells in (2 cells past each end of the
span), joining the ground over 5 more cells, so both sides of a shared edge
meet at exactly the same height. Write the level on both sides.

## border

The region's natural border (W4, docs/mmorpg-m8.md): the outline of its
playable land, and the walls outside it, so a region is never a rectangle
and its edges never show as a line or a fence.

```json
"border": {"outline": [[0, 41], [6, 30], [14, 22], ...], "level": 4}
```

- `outline`: the playable land's corners (≥ 3, no side crossing another),
  region cells. Outside it the ground rises into a wall `level` levels
  high (1…4, default 4), its cliff face at the outline, its top rising a
  level or so further out, rocks, boulders and spires strewn over it (the
  build adds them, about one every 4.5 cells). Walls are no ground: nothing
  walks on them, a cliff hop does not land on them.
- Round each opening the outline goes out to the edge: the passage is a gap
  in the walls, its faces 1.5 cells past each end of the span.
- Within 4 cells of an edge the walls follow the edge's openings alone (a
  wall everywhere but in the gaps, exactly the wall's level at the edge), so
  two regions' walls meet without a seam and the world reads as one piece.
- A unit walks up to an open edge and up to 1 cell past it
  (`RegionEdges::overhang`); the server hands it to the neighbour there
  (docs/worlds.md, "Edges").
- Everything placed (pads, spawns, sites, deposits, wells) lies inside the
  outline by at least a cell.

`autocraft-region edges FILE [--depth D]` prints the ground's level along
each edge, D cells in, so a passage goes where both neighbours' ground is
at one level; `autocraft-region format FILE` rewrites a file in the tool's
own layout. The First Reach's borders and passages were drawn by a script
(an outline inset 6–20 cells from the rectangle by a smooth noise, out to
the edge round each passage, clear of what the region holds), then checked
by eye on the previews; edit the outline by hand from there.

## Problems

`validate` (and `build`, `preview`) reports, each with its place in the file
and the region position:

- parse: bad JSON (line, column), missing or unknown key, wrong type, a
  name not on the list, a number out of range, a point out of bounds, a
  shape too small, a name used twice, a span off its edge, short or
  overlapping;
- a deposit or well on a cliff face, in rock, on top of another, cut off;
- a pad on a cliff, on uneven ground, in rock, on ore or a well, cut off;
- a spawn on a cliff, in rock, on ore or a well, cut off;
- a site on a cliff, in rock, on ore, a well, a pad's plate or another
  site, or cut off (a vault: off high ground, or with no ramp and no
  cliff hop to it);
- a ramp end on the wrong level, a ramp too steep;
- an opening blocked (a cliff, a rock, ore, a well) or cut off;
- a border whose outline crosses itself or closes a passage, and a pad,
  spawn, site, deposit or well outside it.

"Cut off" means not joined to the region's largest open area on the
walking grid. A problem never stops `build` or `preview`.

## The satellite map

The world map (M; docs/mmorpg-play.md) shows each region as a picture of its
real ground from above: the game's terrain, its materials, cliffs, rocks and
plants, ore and wells, lit by the world's day at a held hour (10:30) in clear
weather, with no units and no fog. The pictures are baked, one a region, 8
pixels a cell (1152 × 768 for a medium region), north up; ground out of play
(`MapDefinition::inPlay`, so any outline a region gets) is transparent.

**Rebake after a region file changes** (or a new region joins the world):

```
unreal/Tools/build.sh                 # the module first
uv run unreal/Tools/bake_map.py       # every region of worlds/first.json
uv run unreal/Tools/bake_map.py --only start,cold-fork   # some of them
```

It runs two hidden runs through `hidden_run.py`: the game photographs each
region (`AcMapBake.h`: the region built as a client builds it, an
orthographic scene capture straight down) into
`unreal/Content-src/map/<id>.png` (not committed), then an editor script
(`Tools/Editor/import_map.py`) imports them as
`/Game/UI/Map/T_AcMap_<id>` (dashes as underscores; committed). Options:
`--px` pixels a cell, `--hour` the light, `--world` another world file,
`--no-import`. A region with no picture is drawn in its style's colour, and
the log says to bake it.

## What the build makes

An `ac::MapDefinition`: the generated map renamed, its `version` a hash of
the file (so a changed file never meets a cached walking grid), the height
edits in `heightEdits`, deposits and wells in `deposits` and `wells` (after
the bases' in the game state), crafted doodads after the base's. The game's
own maps leave those three lists empty, so they stay as they were. Pads,
spawns, sites and openings come beside the map (`region::Built`), in ground
coordinates.
