# World sculpting

Status, 2026-10-19: S1 and S2 are built, with Prospect Landing as the trial
region, on the terrain lab's branch only. Nothing is promoted: the user's
word comes first. S3 (rolling ground) and S4 (far land) are not started.

The user, 2026-10-18: "I hate that I am always surrounded by cliffs, should be a bit more variety, canyons are cool, but not that cool." Their reference is a painted cockpit view of Prospect Landing:

- open desert floor out to the horizon;
- buttes, mesas and spires standing free on it;
- distant mesas in the haze;
- low rock shelves, scattered stones, rippled sand and darker gravel.

The terrain lab ([terrain-lab.md](terrain-lab.md)) changes only how the ground is drawn, and the sim's ground never changes there. This chapter is the opposite: it changes the ground itself. That means the region files, `TerrainField`, the nav grid, the satellite maps and the goldens. So it is a chapter of its own, with its own checks.

## 1. Why every region feels walled in

Every region of the First Reach is a bowl:

- **The rim:** its `border` outline is walled four levels high all round (`border.level` 4 in all nine files). A level is 2.4 m, so the face is about 10 m.
- **Further out:** the wall's top climbs most of a level more along the skyline and up to 1.5 levels more past the face (`TerrainField::walled`: `skyline`, `ridge`), so 13 to 14 m.
- **The way out:** each region opens to its neighbours through 2 to 4 gaps of 14 to 16 cells each (`openings`).

`autocraft-region levels` on four regions shows how much of each region is wall or cliff:

| region | level 0 | level 1–2 | cliff `#` | wall rock `o` |
|---|---|---|---|---|
| start | 51% | 8% | 11% | 30% |
| glass-flats | 46% | 2% | 12% | 41% |
| tern-ridge | 38% | 17% | 12% | 34% |
| rust-hollow | 41% | 8% | 12% | 40% |

About 40% of every region is wall or cliff, and it stands round the edge. Wherever a pilot stands, whichever way they look, a wall at least 10 m high closes the view within 20 to 60 cells. The reference turns that round: the floor runs out to the horizon, and the rock stands up out of it in pieces.

## 2. The moves

### S1. A low rim

- **The wall's height:** `border.level` goes to 1 or 2 in most regions.
- **The rise beyond the face:** `skyline` and `ridge` in `TerrainField::walled` scale with the wall's level instead of adding up to 2.3 levels on top of any wall. On a level-1 rim the land beyond rises gently and doesn't tower.
- **New key `border.slope`** (cells, like a height edit's `slope`): the rim becomes a rise of talus and broken rock instead of a cliff face. Where a stretch of rim is a slope, a machine still can't drive past the outline: the nav grid keeps the outline as the edge, as it does today.
- **Shared edges:** the gaps on edges shared with a neighbour widen to half the edge or more, wherever both sides' ground meets at one level (`autocraft-region edges`). Two neighbours then read as one open plain with a few rock fingers between them, instead of two bowls joined by a door.
- **The world's outer rim:** the edges that face out of the First Reach keep a taller wall (level 3 to 4), set back where the outline allows.

### S2. Buttes, mesas and spires

These are free-standing rock on the open floor, the reference's main feature.

- **Buttes and spires** are islands of rock with no ground on top, like a wall. Two new keys on each island:
  - `level`: its height, from 2 up to 6, so up to 14 m.
  - `kind`: `butte` (steep, flat-topped, 3 to 8 cells across), `spire` (1 to 2.5 cells across, tall, a broken top) or `mesa` (wide, flat-topped).

  Their faces get the walls' craggy noise and the terraces. Talus and boulders gather at their feet (the build's border rocks, about one every 3.4 cells). Nothing stands on them, so the nav grid treats them as it treats walls today.
- **Climbable mesas** stay as `raise` edits (level 1 to 2) with a ramp: the high ground that is ground, with an outlook and a fight on top.
- **How many:** four to eight per region, of mixed size and height, none closer than about 10 cells to a pad, a site, a vein or a well, and none within 16 cells of an opening.

### S3. Rolling ground and shelves

- **Low shelves:** `raise` at level 1 with `slope` 2 to 3, the half-buried rock benches of the reference. They are steep enough to read as rock, but a machine climbs the gentle sides.
- **Swells:** wide, low `raise` edits with `slope` 6 to 10, so the floor isn't a table.
- **Gravel patches:** the ground's darker tone, already in the palettes. The lab's rounds draw the texture.

### S4. Far land

A ring of hazy mesa silhouettes past the world's outer rim, 1 to 3 km out, drawn by the client only:

- one cheap mesh ring with no shadows;
- tinted by the sky's haze (the terrain's `Haze` parameter);
- no sim and no server.

It gives the open floor a horizon to run to, like the reference's distant mesas. It is a lab round in all but name, so it follows the lab's rules: it is benched, and it is promoted on the user's word.

### S5. Canyons where a zone needs one

Canyons stay only where a canyon zone needs one:

- Cold Fork's The Fork;
- Brine Pans' The Drowned Cut;
- Dust Terraces' The Roost.

No other region cuts a canyon. The canyon zones keep their rims and walls. Those rims are the one place in the world where cliffs are meant to close the view.

## 3. Targets

Every reworked region must meet these. `autocraft-region build` prints them, alongside "open ground".

| measure | today (start) | target |
|---|---|---|
| wall and cliff, share of the region | 41% | 20% or less (canyon zones excepted) |
| open horizon at the pad: the share of 64 directions where a pilot's eye sees 60 cells or more | not measured | 50% or more |
| free-standing rock (buttes, spires, mesas) | 0 | 4 to 8 |
| open ground (the tool's own measure) | 58% | 70% or more |

The open horizon is new: `autocraft-region build` casts 64 rays from 1.3 m over each pad across `TerrainField`'s height and counts the ones that reach 60 cells.

## 4. What it touches

- **Core (`AutocraftCore`):**
  - `RegionFile` reads `border.slope` and the islands' `level` and `kind`.
  - `RegionEdges` carries them.
  - `TerrainField::walled` and `wallAt` draw them.
  - The nav grid needs no new rule: walls and islands stay no ground.
- **The region tool:** validate checks the new keys; preview draws the islands by height; build prints the open horizon.
- **Region files:** all nine of the First Reach. Deposits keep their index, because saves keep deposits by index: move a deposit, never delete one. Rerun `uv run unreal/Tools/regions/shape.py`, or edit by hand where a shape changes.
- **The world:** wider gaps must match on both sides of each shared edge (`make -C unreal/server test FILTER=world_load`).
- **Satellite maps:** `uv run unreal/Tools/bake_map.py` for every region changed.
- **Goldens and shots:** the MMO shots and any golden that sees a region's ground change on purpose. Reshoot only the regions changed.
- **Tests:** `make -C unreal/core-tests`, `make -C unreal/server test`, `make -C unreal/net-tests`.
- **The Brood:** its walkers path round the new rock. Check each region's nest and its paths to the pads with `levels --bare` and the preview.

## 5. Order

1. **S1 and S2 in the core,** with Prospect Landing as the one trial region. Shoot it with the lab's sheet (`-AcTerrainLabShots`, the feet and across views) beside today's. Show it on the lab's page.
2. **The user's word.** Then roll S1 to S3 across the other eight regions, a few at a time, each validated, previewed and looked at.
3. **S4, far land,** benched like a lab round.
4. **Then the lab's round 3 (cliff faces).** Its faces follow `TerrainField::PolygonRuns`, so they will follow the new cliffs, and the buttes and spires become its best subjects. Doing round 3 before this chapter would dress cliffs that are about to move.

## 6. Trial sketch

A first sketch exists only in the scratchpad: Prospect Landing with `border.level` 1 and five buttes and spires as `raise` edits at levels 3 to 4. It validates, open ground is 58%, and the preview shows the low rim and the rock standing free on the floor. It uses today's keys only, so the buttes there have ground on top. S2's islands replace them.

## 7. The trial (Prospect Landing)

Built: `border.slope` and island `level` and `kind` in the core
(`RegionFile`, `RegionEdges`, `TerrainField::walled`; a level-4 wall with no
slope draws exactly as before, so the other regions are unchanged);
`RegionEdges::edgeLevel` 4 along every shared edge, so a low rim still meets
its neighbour; `autocraft-region build` prints the measures; and
`uv run unreal/Tools/regions/sculpt.py start` did the rest:

- the land grew 8 cells over the old walls' level ground; the rim is level
  1 with a 5-cell slope;
- the gaps widened in both files of each pair where both sides are level
  0: north 43.5-80 (was 64-80), east 35.5-86 (was 41-55), south 45.5-134
  (was two gaps of 16); the west stays 41-55 (no level ground on Rust
  Hollow's side);
- two buttes (levels 4 and 3), a mesa (2) and two spires (5 and 4), kept
  30 cells from the pad.

| measure | before | after | target |
|---|---|---|---|
| wall and cliff | 41% | 22% | 20% or less |
| open horizon at the pad | 38% | 73% | 50% or more |
| free-standing rock | 0 | 5 | 4 to 8 |
| open ground | 59% | 78% | 70% or more |

Tests: core 561 pass, net 120 pass. Server: the edge tests now pair Start
with its real east neighbour, and the Lodeback test keeps its bug out of a
vein's ring (the open ground made it pick a mound in the middle of one).
One fails and is left for the user: `defence_citadel_turrets_beat_a_raid`.
The raid now crosses the open south straight to the Citadel, and the
gunner leaves one of six at 3 hp. Before the rule that keeps rock 30 cells
from a pad, a butte 23 cells from the Citadel stopped every round
(139 fired, 0 hit). Open land changes the defence: it may want more time
for the gunner, a second gun, or a rise left south of the pad.

The satellite maps of the five regions changed are rebaked.

## 8. Sculpting for an AI (the landforms)

The user, 2026-10-19, on the trial's hand-placed rocks: "they look too
manually placed … When I say sculpting I am generally thinking of a way
that would allow AIs as you to better create such worlds." A region file
written as polygons and coordinates comes out as rectangles, circles,
straight ridges and rocks set one by one, whoever writes it. So the AI says
what the land does and code draws its shape. Four parts:

### Landforms (S6)

`sculpt.forms` in a region file (docs/regions.md, "sculpt"): a landform in
words, `expand` (`Landforms.cpp`) draws it into the plain layers (height
edits, border islands, rocks) at build time, on the server and the client
alike, the same for the same seed:

| form | where | draws |
|---|---|---|
| `butte` | `at` | a free-standing island of rock, levels 3-4, on a talus apron; rocks round its foot |
| `mesa` | `at` | a wider, stretched island, level 2-3, its faces stepping at a shelf, on an apron |
| `spires` | `at` | 3-7 narrow spires, levels 3-5, clustered, on a shared apron |
| `escarpment` | `along`, `face` | a raised bench (level 1-2) whose cliff runs along the line facing `face`; bays bitten into the face, a back that wanders, ends that round off; talus along the foot |
| `shelf` | `at` | a long low bench, level 1, its sides steep rock (slope 2.5) |
| `swell` | `at` | a broad gentle rise, level 1 over 6-10 cells: rolling ground |
| `wash` | `along` | a dry bed meandering along the line: a shallow cut, its floor darker |
| `boulders` | `at` | a field of boulders and rocks, now and then a small spire |

Each takes `size` (small, medium, large), and may take `height` (levels),
`count` and `seed`. Every outline is bent by noise at two scales and bitten
by erosion, so no edge runs straight; rocks gather at feet and round spires
instead of being placed one by one.

### Styles

`sculpt.style` places a style's own forms after the file's, wherever the
rules and the file's own raises and islands leave room:

- `open-desert` (the user's reference): a large and two medium buttes, a
  mesa, two spire clusters, three shelves, six swells, five boulder fields,
  a wash;
- `badlands`: eight shelves, two buttes, two large spire clusters, three
  swells, five boulder fields, two washes;
- `mesa-country`: four mesas, two large buttes, two small spire clusters,
  three swells, four boulder fields.

`autocraft-region build` says what each style placed and what found no room.

### Rules

No landform may take ground the game needs (`sculptProblems`; validate
names each breach and the build of a style's forms skips it):

- each pad's field of fire, 30 cells plus the form's reach (a swell, wash
  or boulder field only 8): a butte there shields a raid from the Citadel's
  guns;
- the spawn points (8), sites (9), veins (7), wells (8), the nest (30),
  landmarks (6) and the ramps' ends (6);
- each passage's lane, 16 cells in along its span (a swell or a wash may
  cross it, boulders may not);
- the canyon zones' outlines and paths;
- `sculpt.keep`: more ground the file keeps clear;
- the land itself: inside the border's outline by the form's reach.

The build's measures say whether the land works: wall and cliff, the open
horizon at each pad, free-standing rock, straight cliff runs (a hand-drawn
tell), and the raid's way in: how far a walker goes from each passage and
the nest to each pad, and how long that takes.

### Eyes

- `autocraft-region view FILE --from s|n|e|w|AZ [--pitch] [--eye --at X,Z]`:
  the region's ground rendered at an angle (a clay relief with hillshade
  and shadows; rocks, pads, ore marked), or from a pilot's eye. About 0.4 s.
- `uv run unreal/Tools/regions/critic.py REGION [--ref IMAGE] [--shots DIR]`:
  one sheet to judge a region by: the top view, four oblique views, two
  eye views from the pad, the reference image, the lab's in-game stills,
  and the build's measures. The AI's loop: write the forms, validate, look
  at the sheet beside the brief's reference, change, repeat.

### The proof: Prospect Landing

Prospect Landing is re-made with landforms only: no polygon and no rock set
by hand. It has an escarpment along the north-east, two buttes, a spire
cluster and the `open-desert` style. The hand-drawn raises, ridges, ramps
and the 50 rocks set one by one are gone. Kept: the pad's flat circle and
the lander wreck's mound.

## Log

| date | step | note |
|---|---|---|
| 2026-10-18 | plan | the user's reference; the measures above |
| 2026-10-19 | S1+S2 trial | Prospect Landing opened (above); four neighbours' gaps widened to match |
