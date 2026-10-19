# Landforms, styles, rules, measures

The source of truth is `unreal/Source/AutocraftCore/Private/Landforms.cpp`
(`expand`, `sculptProblems`, `draw`, `reach`, `rules`, `styleForms`); the
numbers below are its, as of 2026-10-19. When they disagree, the code wins:
fix this file.

Positions are region cells from the north-west corner: x east, z south (down
the preview). A level is 2.4 cells high (`map.levelHeight`). One cell = 1 m.

## The block

```json
"sculpt": {
  "style": "open-desert",
  "seed": 1,
  "forms": [
    {"form": "escarpment", "along": [[100, 22], [118, 17], [136, 24]], "face": "south", "height": 2},
    {"form": "butte", "at": [30, 74], "size": "medium", "height": 4, "seed": 3},
    {"form": "spires", "at": [114, 62], "size": "small", "count": 4}
  ],
  "keep": [{"at": [90, 60], "radius": 10}]
}
```

- `style`: `none` (default), `open-desert`, `badlands`, `mesa-country`.
- `seed`: 0-1000000, default 1. Changes the style's placements and every
  form's noise.
- `forms[]`: `form` (required); `at` (butte, mesa, spires, shelf, swell,
  boulders) or `along` (escarpment, wash; ≥ 2 points); `size` small |
  medium (default) | large; `height` 1-6 levels (0/absent: the kind's own);
  `count` 1-40 (spires, boulders); `face` north | east | south | west
  (escarpment: its LOW side, default south); `seed` 0-1000000 (this form's
  own variation).
- `keep[]`: `{at, radius 0.5-100}`, ground no landform may take.
- Unknown keys are errors with a "did you mean". `$T format` rewrites the
  block in the tool's layout; `$T expand FILE` prints the file with the
  forms drawn in (stderr: what the style placed).

## How `expand` works

1. A copy of the file with `sculpt` cleared.
2. The file's own forms, in order: form `i` drawn with seed
   `fnv1a64(id) ^ (sculpt.seed * 0x9e3779b97f4a7c15) + 0x1000 * (i + 1) +
   form.seed`. Drawn even when they break a rule (`validate` reports it).
3. The style's forms, in the style's order, each placed by up to 900
   random tries: the first 300 at its size, then a size smaller, then
   smallest. A try is kept when it breaks no rule and stands clear of
   every polygon the file already has (its raise/lower/flatten edits, its
   border islands) by its reach + 6 (a swell: + 0) and of every placed form
   by both reaches. Whatever finds no room is skipped and noted.
4. Each form appends to the plain layers: `height` edits (after the file's
   own, in order), `border.islands` with `islandLevels`/`islandKinds`
   (needs a `border`; without one a butte/mesa/spire becomes a cliff-sided
   `raise`), and `doodads` (rock, boulder, rockSpire).

`build` and `validate` both read the file through `expand`, so the server,
the client, the satellite bake and every test see the same land. The same
file gives the same land; any change to the rules' inputs (a pad, a
deposit, a site moved) can move the style's placements, since the rules
decide where tries land.

## The forms

Sizes are small / medium / large. "Reach" is how far from `at` (or from
every point of `along`, sampled every 2 cells) the form takes ground: the
rules and the style's spacing measure with it.

| form | draws | radius / spread | height (default) | reach |
|---|---|---|---|---|
| `butte` | a talus apron (raise, level 1, slope 3.5, radius × 1.25) under a free-standing island `kind: butte`; rocks round its foot (≈ 2.2 per cell of radius, 0.6-2.6 cells out, 30% boulders) | 3 / 4.5 / 6 | 3 / 4 / 4 levels | radius × 1.75 + 3.5 |
| `mesa` | an apron (radius × 1.1) under a stretched island `kind: mesa` (stretch 0.5-0.8, 2-3 erosion bites); its faces step at a shelf | 6 / 8 / 11 | 2 / 2 / 3 | radius × 1.54 + 3.5 |
| `spires` | `count` (3 / 5 / 7) islands `kind: spire`, radius 1.5-2.4 each, ≥ 4.2 cells apart within the spread, heights `height` (default 5) minus 0-2, never under 2; one shared apron (spread × 0.8); 3 rocks at each foot | spread 5 / 7 / 10 | top 5 | spread × 1.12 + 3.5 |
| `escarpment` | a `raise` (level `height`, default 1, a cliff) whose foot meanders ±2.5 cells along `along`, facing `face`; erosion bays 1-3 cells deep, one per 10 cells; a back `depth` cells behind (8 / 12 / 18) that wanders ±35% and rounds off at both ends; a `ramp` up (width 5, 5 cells a level + 1 out, 4 in) where the face is least bitten in its middle third; rocks along the foot every 2 cells (70%) | depth 8 / 12 / 18 | 1 | 3 (from the line) |
| `shelf` | a long low bench: `raise` level `height` (1), slope 2.5, outline stretched 0.32-0.55; rocks at its edges | 5 / 8 / 12 | 1 | radius + 2 |
| `swell` | a broad gentle rise: `raise` level 1, slope 6 / 8 / 10 (drivable), outline radius × 0.55 | 8 / 12 / 18 | 1 | radius + 4 |
| `wash` | a `cut` at level 0, width 2 / 3 / 4.5, slope 1.5, its line meandering ±3 cells: a darker dry bed; it carves through raised ground (a swell, an apron) | width 2 / 3 / 4.5 | 0 | 2 / 3 / 4.5 |
| `boulders` | `count` (4 / 8 / 14) doodads within the spread: 8% a small `rockSpire` (scale 0.8-1.6), 42% boulders, the rest rocks (scale 0.5-1.8) | spread 3 / 5 / 8 | – | spread |

Every outline is a star-shaped blob of 30 points (spires 12, swells 20):
radius bent by two scales of noise (roughness 0.18-0.3) and bitten by
erosion notches, never under 35% of the radius, so no edge runs straight
and no outline crosses itself.

What a form is for:
- **butte, spires**: the reference's free-standing rock. Nothing stands on
  them; they are walls to the walking grid.
- **mesa**: big free rock; same, its top unreachable.
- **escarpment**: walkable high ground with a way up. Put a vault site on
  its top; a canyon zone's rims can sit on one.
- **shelf**: the reference's low rock benches.
- **swell**: rolling ground; drives over.
- **wash**: texture on the floor, carving only raised ground: on level-0
  ground it shows as a darker band and barely reads from above.
- **boulders**: scattered stones, clustered.

## How the field draws islands and rims

`TerrainField::walled` and `wallAt` (`unreal/Source/AutocraftCore/Private/TerrainField.cpp`):

- A free-standing island (`kind` butte/mesa/spire) rises to its own
  `level` (1-6, `maxIslandLevel`). A butte or mesa top is flat with a
  0.12-level roughness; a spire's top rises half a level to its middle,
  broken. Faces are sheer and craggy (a spire's sheerer: full height 0.8
  cells in from its outline). A mesa's faces step at a shelf 1.5-4 cells
  wide where noise says.
- An island without a `kind` (`"wall"`, a bare outline) is drawn like the
  border's walls (W4b).
- The rim: `border.level` 1-4 and `border.slope` (cells, 0-12; 0 a cliff).
  With a slope over 1.2 the ground climbs over that many cells, rough and
  without terraces; the walking grid still ends a cliff's width (1.2
  cells) past the outline. A rim's skyline and ridge past the face scale
  with `min(level, edgeLevel) / edgeLevel`.
- Within 4 cells of a shared edge (`RegionEdges::edgeBand`) every wall
  climbs to `RegionEdges::edgeLevel` (2), whatever the region's rim, so two
  neighbours meet exactly. The First Reach uses rim level 1, slope 5.
- Islands, rims and canyon walls get the ground's rock faces in game
  (GROUND-LOOK.md, "Rock faces").

## The rules

`sculptProblems` names each breach of the file's own forms as
`sculpt.forms[i] at (x, z): a <form> in <what>`; the style never places a
form that breaks one. A form breaks a rule when any of its probe points
(its `at`, or `along` every 2 cells) is nearer a rule's point than:

| rule (point) | solid forms (butte, mesa, spires, escarpment) | gentle forms |
|---|---|---|
| each pad's field of fire (the pad) | 30 + reach | swell 26 + reach; shelf, wash, boulders 16 + reach |
| spawn points | 8 + reach | 8 + reach |
| sites | 9 + reach | 9 + reach |
| deposits (veins) | 7 + reach | 7 + reach |
| wells | 8 + reach | 8 + reach |
| the nest | 30 + reach | 30 + reach |
| landmarks (any doodad but rock, boulder, rockSpire, plant, bush, debris, deadTree) | 6 + reach | 6 + reach |
| ramp ends (the file's own `ramp` edits) | 6 + reach | 6 + reach |
| each passage's lane (every 3 cells along its span, 3 and 10 cells in) | 7 + reach | swell, wash: none; shelf, boulders: 7 + reach |
| canyon zones (outline points 6, path points 5) | that + reach | that + reach |
| `sculpt.keep` | radius + reach | radius + reach |
| the land: inside `border.outline` by | 0.6 × reach (escarpment 2) | wash 0, others 0.6 × reach |

Why the pad's ring is 30: a butte inside it shields a raid from the
Citadel's corner guns (a test fired 139 rounds into one, 0 hits). Why
gentle forms keep 16 (a swell 26): room for a 12 × 12 Citadel beside the
pad, and a swell 30 cells off hid a pilot's headlamp from the other.

Also checked by `validate` on the drawn land, as for any region: ore,
wells, pads, spawns and sites off cliffs and rock, inside the outline and
outside every island by a cell; ramps meeting their levels; passages open
2 cells in along their span; sites reachable (a vault: by a ramp or a cliff
hop, printed as a note, not a problem).

## Styles

Placed after the file's forms, in this order (each line: form, size, how
many):

| `open-desert` | `badlands` | `mesa-country` |
|---|---|---|
| butte large 1 | shelf large 4 | mesa large 2 |
| butte medium 2 | shelf medium 4 | mesa medium 2 |
| mesa medium 1 | butte medium 2 | butte large 2 |
| spires medium 2 | spires large 2 | spires small 2 |
| shelf medium 3 | swell medium 3 | swell large 3 |
| swell large 3 | boulders large 5 | boulders medium 4 |
| swell medium 3 | wash large 2 | |
| boulders medium 5 | | |
| wash medium 1 | | |

A medium region (144 × 96) with one pad loses most of its middle to the
pad's 30-cell ring: expect the style to place a third to a half of its
list, mostly in the outer lobes. `build` prints each line as "placed N of
M". If `badlands` rings the rim with cliff-sided shelves (it did in Ashen
Shelf), use another style or add `keep` discs along the rim.

## The measures (`$T build`)

| line | what | target |
|---|---|---|
| `open ground` | the walking grid's free share, and its main connected area | ≥ 70% |
| `wall and cliff` | cells steeper than a machine climbs (`NavGrid::maxSlope` 0.9) or not free | ≤ 25% (canyon region ≤ 35%) |
| `free-standing rock` | islands with a kind, after `expand` | 4-10 |
| `landforms` / `style: …` | the file's own count; each style line placed N of M | – |
| `straight cliff runs (8 cells or more)` | edges ≥ 8 cells of cliff-sided raise/lower/flatten polygons and islands: a hand-drawn tell | 0 (canyon zones' own cuts excepted) |
| `raid: X passage to pad "P": N cells, S s` | a walker's path from 3 cells in at each passage's middle (and the nest) to each pad, at a shardling's march | every line has a way; know the shortest (15 s is the First Reach's) |
| `open horizon at pad "P"` | of 64 directions from 1.3 cells over the pad, those where nothing within 60 cells stands over 3° above the eye (a ray leaving the region counts open) | ≥ 50% |

Before the re-make the First Reach measured 41-52% wall and cliff and
14-38% open horizon; after, 15-33% and 45-80% (docs/world-sculpting.md,
section 9).

## Hand-written ground that stays

- Each pad's `flatten` circle (radius 16, level 0, slope 5): a raised pad
  (level 1) on an `openField` base became a mound no Citadel fits beside.
- Canyon zones' `raise`/`cut`/`ramp` edits (The Fork, The Drowned Cut, The
  Roost): their cut walls' shelves are seeded by the base's noise, so
  changing a canyon region's `base.style` moves where its rim guns can shoot
  from: re-check its gun cover (TOOLS.md, "Tests that read the ground").
- A gentle `raise` (slope ≥ 5) when a zone's ramps need ground at a level
  no landform can give there (Dust Terraces keeps one).
- A level-1+ high ground under a vault, if no `escarpment` can hold it.
