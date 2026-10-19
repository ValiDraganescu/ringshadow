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
autocraft-region levels FILE [--bare]     the ground a cell at a time (# cliff, o rock)
autocraft-region format FILE              rewrite it in the tool's layout
autocraft-region view FILE -o OUT.png [--from AZ|n|e|s|w] [--pitch DEG] [--at X,Z]
                 [--dist CELLS] [--fov DEG] [--size WxH] [--eye]   the ground at an angle
```

The preview: north up, a labelled grid every 8 cells, height as colour bands
(level 0 olive, 1 tan, 2 pale), cliffs red-brown, rocks dark grey, plants
green dots, ore violet, MH wells silver, pads yellow rings, spawns green
diamonds, blueprint sites gold squares (named, with kind and blueprint), openings cyan
(their floor level as L0…L4), the border's outline dotted, each problem a red ring numbered as in the list
below the map.

## Seeing a region

The preview flattens the ground: a cliff is a red line, a wall a pale band.
Two more ways to look show it the way a player meets it.

`autocraft-region view` renders the region's ground at an angle in software
(`unreal/server/region/View.cpp`, under a second at 1280x720). It ray-marches
the built `TerrainField`, with the scenery past the bounds from
`borderHeight`. A low sun from the west-north-west (35° up) lights it, with
cast shadows and haze toward the sky. The ground takes its `ground`
palette, a little lighter each level up. Cliffs (steeper than a machine
climbs) are darker rock, the border's walls grey, its islands warm rock.
Rocks, boulders and spires are bumps and cones, other solid doodads violet
bumps, ore violet mounds and wells silver discs. Pads are yellow rings and
spawn points green, both named; sites are gold squares. The preview's
8-cell grid is drawn faintly on the region, so a spot can be found in both.

- Default: an overview from the south, 35° down, framing the whole region.
  `--from` is the side the camera stands on (n, e, s, w or compass degrees),
  `--pitch` how far down it looks, `--at X,Z` (region cells) what it looks
  at, `--dist` how far off it stands, in cells.
- `--eye`: a pilot's eye, 1.3 cells over the ground at `--at` (default: the
  first pad), looking toward `--from`, 2° down, a 75° field of view. The pad
  it stands on is left unpainted.

`uv run unreal/Tools/regions/critic.py REGION [--ref IMAGE] [--shots DIR] -o SHEET.jpg`
puts everything about a region on one contact sheet, each picture labelled:

- the top-down preview;
- the four oblique views;
- two eye views from the first pad, toward its two most open directions
  (the lowest skyline within 60 cells on `levels`);
- the brief's reference image (`--ref`);
- the terrain lab's in-game stills, `DIR/<region>/lab/*.png` as
  `-AcTerrainLabShots` writes them (`--shots`; it never starts the game);
- under them, the text of `autocraft-region build`.

REGION is an id or a file's path. The pictures go to
`unreal/server/build/critic/<id>/`.

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
  "openings": {"north": [[64, 80]], "west": [[40, 56]]},
  "zones": [ ... ]                      // canyon zones (Z1, below)
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
| `cut` | `line` [[x, z], …] ≥ 2, `width`, `level`, `slope`? | a canyon along the line, `width` across its floor, down to the level (never up); its floor a darker tone |

- `area`: corners `[[x, z], …]` (≥ 3), `{"rect": [x0, z0, x1, z1]}` or
  `{"circle": [x, z, r]}`. Everything inside the outline is at the level.
- `slope`: cells past the outline (a ridge: past its sides) the ground takes
  to reach the level. Left out or 0: a **cliff** (1.2 cells, not drivable).
  Drivable from about 4 cells a level.
- A `cut` two levels deep or more steps its walls in stretches (Z1): a shelf
  half way up, 1.5 to 2.5 cells wide, then the rest of the climb, where the
  terrain's noise says (`TerrainField::cutShelf`; about a quarter of a
  wall). A short line with a wide `width` makes a round basin.
- A ramp cuts through cliffs written before it. Its ends must stand on their
  levels 1.5 cells past each end; it needs about 5 cells of length a level.
  Write it after the cliffs it crosses.

## Resources and props

- `deposits`: an ore patch (a mound), 2×1 cells. `amount` 1…100000.
  `angle` degrees, the long axis from east toward north. A world region's
  mounds come in veins ("Scarcity", below).
- `wells`: an MH well; a Derrick stands on it (needs flat ground 1.5 cells
  round). `amount` 1…100000 (the bases' hold 2250). One a world region.
- `doodads`: `kind` rock | boulder | rockSpire | crate | crateStack | plant |
  bush | debris | deadTree | tower, or a landmark (below); `rotation`
  degrees; `scale` 0.25…4. Plants, bushes and debris are walkable; the
  rest is solid.
- The base's rocks and plants give way to what the file puts down: deposits,
  wells, pads, spawns and opening lanes clear them.

## Scarcity

E1 (the user, 2026-10-15: "the resources are too abundant, I am not
incentivized to hunt for fallen stars"). A region should run dry so a
pilot moves on, and a fallen star should be worth the chase. A region of a
world (brood level 1 or more) holds:

- one or two ore **veins**. A vein is a tight cluster of 3 to 5 mounds,
  each within 4.5 cells of the vein's middle; mounds within 5.5 cells of
  each other are one vein (`region::veins`). The world's veins are rings
  round an open middle (2.6 to 3.2 cells out), each mound's long axis along
  its radius, so the gaps between them stay walkable: a mound mined to
  nothing lets a Prospector walk over it, and growing back it would box one
  in (the server sets units clear of a mound that grows back under them);
- one MH **well**;
- its ore in all by its brood level: about 6,000–8,000 at level 1,
  10,000–12,000 at level 2, 15,000–20,000 at levels 3 and 4;
- none of the base's own ore and wells (`base.drop` has `resources`).

A canyon's prize is its region's well and a rich vein in a ring round it
(4 cells out); the region's other vein lies outside the canyon, smaller.

`validate` refuses a third vein, a vein of fewer than 3 or more than 5
mounds, a mound off its vein's middle, a second well, ore outside the
level's range, and the base's ore (brood 0, the example file, has no
limit). The numbers are `region::veinsMost`, `moundsFewest`, `moundsMost`,
`wellsMost`, `veinLink`, `veinReach` and `oreRange` in `RegionFile.h`.

| region | id | brood | ore | veins (mounds × ore, middle) | well (at, MH) |
|---|---|---|---|---|---|
| Prospect Landing | start | 1 | 7,200 | 5 × 800 at (52, 50); 4 × 800 at (95, 38) | (60, 36), 2,250 |
| Rust Hollow | rust-hollow | 2 | 11,000 | 5 × 1,200 at (52, 35); 4 × 1,250 at (110, 68) | (62, 30), 2,250 |
| Tern Ridge | tern-ridge | 2 | 11,000 | 5 × 1,200 at (97, 56); 4 × 1,250 at (30, 30) | (66, 34), 2,250 |
| Cold Fork | cold-fork | 2 | 11,200 | 5 × 1,700 round the well (the Fork's prize); 3 × 900 at (58, 68) | (118, 26.5), 6,000 |
| Brine Pans | brine-pans | 2 | 11,200 | 5 × 1,700 round the well (the Drowned Cut's prize); 3 × 900 at (54, 69) | (108, 38.5), 6,000 |
| Ashen Shelf | ashen-shelf | 3 | 17,000 | 5 × 2,000 at (55, 29); 4 × 1,750 at (116, 68) | (64, 30), 2,250 |
| Glass Flats | glass-flats | 3 | 17,000 | 5 × 2,000 at (102, 61); 4 × 1,750 at (44, 37) | (90, 52), 2,250 |
| Dust Terraces | dust-terraces | 3 | 17,000 | 5 × 2,600 round the well (the Roost's prize); 4 × 1,000 at (119, 34) | (31, 72), 7,500 |
| Slag Pockets | slag-pockets | 4 | 17,000 | 5 × 2,000 at (52, 27); 4 × 1,750 at (114, 55) | (86, 30), 2,250 |

**Regrowth.** A region file's mound grows back a tenth of its full ore a
world day (40 minutes), up to full, on the world's clock: while the
world idles with nobody on line and when a stopped server catches up too
(`WorldMap::regrowVeins`; Quick regrowth's share shortens it). The world
never dies, but a vein mined out takes ten world days to fill. A starfall's
deposit never grows back. The game's own rule (a dry patch full again
300 s on) is off in the server's regions (`Simulation::regrows`).

**An existing world** adopts its region files' veins on its next load
(schema 26 saves each deposit and well with its place; `WorldMap::adopt`):
a saved deposit at a mound of the file keeps its ore; one the file no
longer lays out is gone; the file's new mounds come in full. A save from
before schema 26 (by index only) of another layout takes the new veins in
full. Every building stays. A well a Derrick stands on is kept with its MH
(from an older save, full), so the pilot's Derrick still pumps; the file's
other old wells are gone. A new mound under a building is left empty for
good (it neither blocks nor grows). Tested in
`server/tests/ScarcityTests.cpp` and on a copy of a played save.

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
| `broodNest` | the Matriarch's hive mound: chitin plates, opal ore, egg pods, a crown of spines (put down by `nest`, below) | 12 across, spines to 5 high | none: walked over |
| `broodTunnel` | a Brood tunnel's mouth in a canyon wall: a black hollow, a frame of rock and chitin, a faint violet glow (put down by a zone's entrance, below) | 5 across, 4 high | none: walked through |
| `buildSpot` | retired (Z4: no build spots); a region file never puts one down | 2.4 across | none: walked over, built over |

The First Reach has two in each region (the region files' `doodads`). Glass Flats' Glass Needle rises to level 4
and Brine Pans' Brine Stack to level 3 (sheer, under their rock crowns).

The models: `unreal/Tools/Editor/models/landmarks.py` (concepts in
`art/models/landmarks/concepts/`); the nest's `brood_nest.py`, a zone's
marks `canyon.py`.

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

## nest

The Matriarch's nest ([creatures.md](creatures.md), "The Matriarch"; N3):
`"nest": {"at": [x, z]}`, at most one in the world (Slag Pockets has it, on
its east plateau at [122, 44]). The build puts the nest's mound there (the
`broodNest` doodad, walked over: she stands in its bowl), and the server keeps
her and her horde there.

- Only in a brood level 4 region; on open ground joined to the rest (high
  ground a ramp reaches is fine), not on a cliff face or in rock, inside the
  border's outline.
- No pad within 30 cells of it (her horde's ring). Rocks and plants within
  6 cells give way to the mound.
- The preview draws it violet, with its ring dotted.

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
"border": {"outline": [[0, 41], [6, 30], [14, 22], ...], "islands": [[[48, 52], [56, 50], ...]], "level": 4}
```

- `outline`: the playable land's corners (≥ 3, no side crossing another),
  region cells. Outside it the ground rises into a wall `level` levels
  high (1…4, default 4), its cliff face at the outline, its top rising a
  level or so further out, rocks, boulders and spires strewn over it (the
  build adds them, about one every 3.4 cells), the tops bare rock-strewn highland
  in one rock colour for the whole world, not the region's palette (W4b: the
  terrain material's `WallZ`), so the high ground between regions reads as
  one land. Walls are no ground: nothing
  walks on them, a cliff hop does not land on them. Along stretches of
  the wall its face steps: a shelf half way up, a few cells wide, then the
  rest of the climb (W4b).
- `islands` (optional, W4b): outlines of high rock inside the outline,
  walled the same way: the land runs round them. An island may instead be
  free-standing rock (S2, [world-sculpting.md](world-sculpting.md)):
  `{"outline": [...], "level": 2-6, "kind": "butte" | "mesa" | "spire"}`,
  its own height, a flat top (a spire's broken), sheer craggy faces (a
  spire's sheerer, a mesa's stepping at a shelf), no skyline rising past
  it. Nothing stands on it either.
- `slope` (optional, S1): the rim's rise past the outline, cells (0, the
  default: a cliff). More is a slope of broken rock that climbs to the
  wall's level over that many cells, no terraces; the walking grid still
  ends a cliff's width past the outline. A low `level` (1-2) lowers the
  skyline and ridge past the face in proportion; within 4 cells of a
  shared edge every wall climbs to level 4 (`RegionEdges::edgeLevel`), so
  neighbours meet whatever their rims.
- `uv run unreal/Tools/regions/sculpt.py REGION` opens a region up (S1 and
  S2): the land grown over the old walls' level ground, the rim lowered and
  sloped, every shared edge opened wide where both sides are level 0 (both
  files), and buttes, a mesa and spires placed clear of the pads' field of
  fire (30 cells), the places, the veins and the passages. It validates
  every file it writes. `autocraft-region build` prints the measures:
  wall and cliff, free-standing rock, the open horizon at each pad.
- Round each opening the outline goes out to the edge: the passage is a gap
  in the walls, its faces 1.5 cells past each end of the span.
- Within 4 cells of an edge the walls follow the edge's openings alone (a
  wall everywhere but in the gaps), their top along the edge a profile both
  sides compute alike from where along it and the edge's openings (flat at
  the wall's level near the corners), so two regions' walls meet without a
  seam or a flat band and the world reads as one piece.
- A unit walks up to an open edge and up to 1 cell past it
  (`RegionEdges::overhang`); the server hands it to the neighbour there
  (docs/worlds.md, "Edges").
- Everything placed (pads, spawns, sites, deposits, wells) lies inside the
  outline, and outside every island, by at least a cell.

`autocraft-region edges FILE [--depth D]` prints the ground's level along
each edge, D cells in, so a passage goes where both neighbours' ground is
at one level; `autocraft-region levels FILE [--bare]` prints the ground a
cell at a time (a level digit, `#` a cliff, `o` rock in the way; `--bare`
without the walls); `autocraft-region format FILE` rewrites a file in the
tool's own layout.

The First Reach's passages were placed by W4; its shapes (W4b) are drawn by
`uv run unreal/Tools/regions/shape.py [REGION ...] [--seed REGION=N]`: a
natural shape well inside the cell (bays cutting 10–30% in, rock spurs,
necks where two bays face each other, corners mostly rock, an island of
rock in some, no straight run of wall much over 12 cells), a valley
narrowing into each passage, land kept round everything that must stay
reachable and joined to the pad's land by the walkable way round
(`levels --bare`). Deposits the walls take move in onto the land near
where they were; plants and debris on the rock go. It validates each try
and keeps, of 40 seeds, the valid one with the shortest straight wall.
Rerunning it on a shaped file keeps what is already inside; edit the
outline by hand from there if a place needs it.

## sculpt

The land in words (docs/world-sculpting.md, "Sculpting for an AI"): a
style and landforms, drawn into the layers above when the region is built
(`autocraft-region expand FILE` prints the file as drawn). Write the land
this way, not as polygons: a hand-drawn polygon, a circle or a rock set by
hand reads as placed.

```json
"sculpt": {
  "style": "open-desert",            // none | open-desert | badlands | mesa-country
  "seed": 1,
  "forms": [
    {"form": "escarpment", "along": [[100, 22], [118, 17], [136, 24]], "face": "south", "height": 2},
    {"form": "butte", "at": [30, 74], "size": "medium", "height": 4},
    {"form": "spires", "at": [114, 62], "size": "small"}
  ],
  "keep": [{"at": [90, 60], "radius": 10}]
}
```

- `form`: butte, mesa, spires (`at`); escarpment, wash (`along`, ≥ 2
  points; an escarpment's `face` is its low side, default south, and it
  gets a ramp up); shelf, swell, boulders (`at`).
- `size`: small, medium (default), large; `height` levels (1-6), `count`
  (spires, boulders), `seed`.
- The rules (validate names a breach as `sculpt.forms[i]`): out of each
  pad's field of fire (30 cells; a swell, shelf, wash or boulder field only
  out of the base's 16), off the places, veins, wells, nest, landmarks,
  ramps' ends, passages' lanes and canyon zones, the rock inside the land.
  `keep` adds ground of the file's own.
- The style fills in after the file's forms where there is room, a size
  smaller when crowded; `build` says what it placed.

## zones

A canyon zone (Z1, docs/mmorpg-m8.md "canyon defence"): a tower-defence
ground in the region. The Brood comes out of tunnels in the canyon's walls
and walks fixed paths along its floor to a prize, a field of rich ore and
an MH well, to mine it for itself; pilots build turrets anywhere standable
in it (Z4: the rims, the floor, the paths themselves, packed as tight as the
footprints let them). Every zone plays the canyon game (Z2,
[canyon.md](canyon.md)): waves down its paths, the pilots' turrets, the
Matriarch when a wave breaks through.

```json
"zones": [{
  "id": "the-fork", "name": "The Fork",
  "outline": [[95, 3], [141, 3], [141, 39], [95, 38]],
  "entrances": [{"name": "North tunnel", "at": [107.5, 7.5], "facing": -90}],
  "paths": [{"from": "North tunnel", "line": [[107.5, 8], [105.6, 13.5], [110, 21], [113, 22.6]]}],
  "prize": {"deposits": [52, 53, 54], "wells": [5]}
}]
```

- `id` a slug, unique in the file; `name` shown to players.
- `outline`: the zone's ground (≥ 3 corners). Its paths and prize lie
  inside it, its entrances within 2 cells of it. Its farthest corner from the
  prize is the zone's reach: the Matriarch fells every turret within it
  when a wave breaks through.
- `entrances`: named tunnels' mouths. Each stands on open floor at the foot
  of a wall (a cliff or the border's rock within 4 cells), `facing` degrees
  from east toward north, into the canyon. The build puts a Brood tunnel's
  mouth there (`broodTunnel`, walked through: a black hollow framed by rock
  and chitin, a faint violet glow in its throat).
- `paths`: one or more from each entrance (`from` its name), a polyline on
  the canyon's floor. It starts within 3 cells of its entrance and ends
  within 8 of the prize's middle, short of the deposits; every half cell of
  it is open walkable ground (no cliff, rock or ore) joined to the region's
  pads.
- `prize`: indices into the file's `deposits` and `wells` (a deposit or a
  well at least). Each lies inside the zone, joined to the pads. Make it
  richer than anything round it (The Fork's: 6000 a deposit, an MH well of
  6000, against Cold Fork's 3600 at most).
- No `spots` (Z4, 2026-10-15): pilots build anywhere standable in a zone
  under the ordinary build rules (the footprint, the build reach, the
  price, the floor and the realm's limits). A file with them still parses;
  they are read and ignored, and not written back.

Z3 adds two twists:
- **`flood`** (optional): `{"outline": [[x, z], ...], "slow": 0.5}`, a
  flooded floor inside the zone (its corners within the outline). The
  Brood's walkers wade through it at `slow` of their speed (0.1–0.9); its
  flyers pass over. The client draws it as water at the floor's height.
- **`roost`** (on an entrance, `"roost": true`): a ledge the flyers come
  off, not a tunnel. Open ground a level or more over the prize, not on a
  cliff face, no wall needed. Its paths are flown: they must stay inside the
  zone, start near it and end near the prize, but may cross cliffs and walls.
  The build puts a small `broodNest` on it.

The preview draws the flood blue with its slow, and a roost as a nest with
"(roost: flyers)" by its name.

Saves keep deposits and wells by their index: append a zone's prize after
the others, and move (not delete) a deposit its canyon takes, so a save's
mined ore stays with its deposit.

The preview draws the outline dashed orange, each path red with chevrons
toward the prize, the tunnels dark with a violet rim, the prize ringed in
gold.

The server sends a group down a path (`WorldMap::sendAlongPath`,
docs/creatures.md "Canyon paths"): it comes out of the tunnel, walks the
waypoints in turn, then lingers at the prize, and a Lodeback eats it.

**The Fork** (Cold Fork, brood level 2): a level-2 mesa over the
region's north-east, two arms cut down to the floor from a tunnel in the
north wall and one in the east wall, winding to the junction's basin with
five rich mounds in a ring round its MH well (E1, "Scarcity"), a gorge from it out to the
passage valley, ramps up to the west rim, the south-east rim and the rim
between the arms (where the derrick ruin stands). Pictures:
`-AcMmoShots=DIR -AcMmoShotsZ1` (AcMmoShots.cpp).

## Problems

`validate` (and `build`, `preview`) reports, each with its place in the file
and the region position:

- parse: bad JSON (line, column), missing or unknown key, wrong type, a
  name not on the list, a number out of range, a point out of bounds, a
  shape too small, a name used twice, a span off its edge, short or
  overlapping;
- a deposit or well on a cliff face, in rock, on top of another, cut off;
- in a world region (brood 1 or more): more than two veins, a vein of
  fewer than 3 or more than 5 mounds, a mound off its vein's middle, more
  than one well, ore outside the level's range, the base's own ore
  ("Scarcity");
- a pad on a cliff, on uneven ground, in rock, on ore or a well, cut off;
- a spawn on a cliff, in rock, on ore or a well, cut off;
- a site on a cliff, in rock, on ore, a well, a pad's plate or another
  site, or cut off (a vault: off high ground, or with no ramp and no
  cliff hop to it);
- a ramp end on the wrong level, a ramp too steep;
- an opening blocked (a cliff, a rock, ore, a well) or cut off;
- a border whose outline crosses itself or closes a passage, an island
  outside the outline, and a pad, spawn, site, deposit or well outside the
  land (outside the outline or on an island);
- a zone's path that names no entrance, an entrance with no path, a prize
  index past the file's list; an entrance off the floor or out in the
  open; a path not walkable end to end, starting far from its entrance or
  ending far from the prize; a prize outside the zone or cut off.

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
