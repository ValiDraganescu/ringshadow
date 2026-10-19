# World sculpting

Status, 2026-10-19: the First Reach is re-made (section 9): all nine
regions said in landforms, every rim low and sloped, the passages wide; no
edge runs straight (section 10).
The user: "let's integrate all this in the game and let's recreate our
worlds. I don't care about my current game, we can start fresh." S4 (far
land) is not started.

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
horizon at each pad, free-standing rock, straight runs (a hand-drawn
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

## 9. The First Reach re-made

All nine regions, 2026-10-19:

1. `sculpt.py` on every region: the land grown over the old walls' level
   ground, the rim level 1 with a 5-cell slope, every shared edge open
   wherever both sides are level 0 (both files of each pair). Shared edges'
   walls stand at `RegionEdges::edgeLevel` 2 (it was 4).
2. Each region's ground re-said as landforms by three workers, each region
   its own style and character, looked at on the critic sheet beside the
   user's reference. Removed everywhere: hand-drawn raises, ridges and
   circles, every rock set by hand (some 250). Kept: the canyon zones'
   edits, the pads' flats, landmarks. Four regions moved to the `openField`
   base (its generated plateaus were cliff-ringed tables).
3. `sculpt.py --grow 14 --no-widen` again where the old outlines still hugged
   the land.

| region | style | own forms | wall and cliff (before) | free rock | straight runs | horizon at pad (before) |
|---|---|---|---|---|---|---|
| start | open-desert | 9 | 15% (41%) | 6 | 0 | 77% (38%) |
| rust-hollow | badlands | 5 | 25% | 6 | 0 | 73% |
| ashen-shelf | mesa-country | 4 | 29% | 6 | 0 | 62% |
| tern-ridge | badlands | 4 | 19% (46%) | 5 | 0 | 80% (36%) |
| cold-fork | mesa-country | 5 | 21% (46%) | 9 | 5 (The Fork's) | 77% (38%) |
| glass-flats | open-desert | 3 | 18% (52%) | 9 | 0 | 59% (14%) |
| brine-pans | open-desert | 5 | 20% | 5 | 9 (The Drowned Cut's) | 45% |
| dust-terraces | badlands | 3 | 33% | 1 | 9 (The Roost's) | 50% |
| slag-pockets | badlands | 7 | 18% | 6 | 0 | 67% |

Left as they are, for the user:
- Dust Terraces keeps a hand-written gentle rise north of The Roost, since
  no landform fits there and the zone's ramps need it. Its old rock island
  is now a level-3 mesa.
- Old saves don't match the new land: start a new world (NEW WORLD).

## 10. No straight edge

The user, 2026-10-19, on a top-down view of The Fork
(`art/terrain/ref/straight-rim-the-fork-2026-10-19.png`): "the landscape
delimiting regions is again too straight, I could survive without it than
having it look so artificial." Three things still ran straight after
section 9: every region's rim (`sculpt.py` keeps the outline 3 cells in from
each edge, so it ran along the edges in long straight lines), the canyon
zones' hand-written plateaus and cuts, and the walls' rocks, set on a
jittered grid that left them in rows wherever the wall was a strip.

`expand` now ends with **erosion** (`Erosion.cpp`, `erode`), for every
sculpted region, on the server and the client alike:

- **The rim** wanders along its run: two waves (12-18 cells and 4.5-6.5
  cells long) and noise move each point of the outline up to 1.8 cells out
  and 3.6 in. It stays 1.6 cells inside the region, still in a passage's
  lane (its span and 3 cells past each end, tapering over 6 more), and
  clear of pads (16), spawns, sites, veins, wells, landmarks, ramps, the
  nest and the canyons' tunnels and paths. Where the wave meets the limit
  it folds back in, so it never runs flat along it.
- **A canyon plateau's outer faces** (the raise that holds a zone's paths)
  take the rims' rule: a rough slope, 2.5 cells a level they fall, 3 to 5
  cells. The face is set back by half the slope (in a pad's field of fire,
  the whole slope: the guns see as far as before) and wanders 0-3.5 cells
  more. It stays 4 cells clear of the cuts' walls, 9.5 of the paths (the
  rims the guns stand on) and 2 cells round a ramp's top. A cut that ran out
  through the old face runs on through the slope, so its mouth stays open.
  Fallen blocks (a boulder and up to two rocks) lie at its foot every 9-16
  cells.
- **The lanes** (the cliff-walled cuts) keep their cliffs. A lane the
  walkers walk (a zone's path runs in it) keeps its line and width: the
  rims' guns are placed and aimed by that wall (a gun on The Fork's north
  arm found no ground once the wall moved a cell). A straight run of it gets
  one bow, 0.5 cells deep at its middle, to either side in turn. Elsewhere
  (a gorge, a basin) a cut bends 1.2 cells either side of its line, held 3
  cells at each end. Every 7-12 cells a stretch of wall 3-6 cells long
  steps 0.8-1.8 cells out (offset segments), never more than 2.8 cells from
  a path (the wall stands 2.5 from it). Where the wall is two levels deep
  or more and 8 cells from any path, a notch is bitten into its top beside
  it, half as deep. Neither comes near a ramp.
- **Any other hand-written raise** wanders the same way (Dust Terraces'
  rise and the bridge to its vault).
- **Last, anything still straight is bent**: a landform's long side, a
  stretch of rim held still. A 9-cell wave, 0.8 cells deep, goes across
  every run left (only inward on the rim and on islands).

**The walls' rocks** (`build`) gather in clusters of one to four, a
boulder or spire first and smaller rocks round it. Most stand at the rim's
foot and fewer up the wall, with gaps where noise says, and never three in a
row (`inRow`: one within 0.6 cells of the line between two others, a third
of the way or more from each, those two at most 14 cells apart; the file's
own rocks count). None reaches into the ground units walk: a rock too big
for its spot is made smaller. 430 in the First Reach, where the old grid
set 494 (some of those stood in the walkable ground at the foot).

**The measure** (`straightRuns`, `autocraft-region build`'s "straight
runs") is stricter. A run is any stretch of an edge that stays within 0.25
cells of one straight line for 8 cells or more (`straightRun`), however many
points it has. It now counts the rim's outline (not where it lies along the
region's edge) and the cliff-walled cuts, as well as the cliffs and
islands.

Per wall:

| canyon | wall | done |
|---|---|---|
| The Fork (Cold Fork) | the plateau's west and south faces (the user's screenshot) | the rims' rule: a 5-cell slope, worn, set back; fallen blocks at the foot |
| The Fork | its north and east faces | under the region's rim at the edge, unchanged (nothing shows) |
| The Fork | the north and east arms | walked lanes: cliffs on their line (the rims' guns), the east arm's straight run bowed; offset segments only away from the paths |
| The Fork | the south gorge (no path) | a cliff, bent, offset segments, notches |
| The Drowned Cut (Brine Pans) | the plateau's long north face, west, east and south faces | the rims' rule, a 5-cell slope; the Salt tunnel's cut carried on through the slope, so its mouth stays open to the floor outside |
| The Drowned Cut | the Salt and Gull arms | walked lanes: cliffs on their line, the Salt arm's three straight runs bowed |
| The Drowned Cut | the gorge (no path) | a cliff, bent, offset segments, notches |
| The Roost (Dust Terraces) | the plateau's faces | the rims' rule, a 3-cell slope (it stands a level over the rise) |
| The Roost | the bridge to the Dust vault | a cliff, worn (it holds the vault up) |
| The Roost | the Ochre and Dust arms | walked lanes: cliffs on their line |
| The Roost | the cut north toward the vault (no path) | a cliff, bent, offset segments (one level deep: no notches) |

Measured with `autocraft-region build`. Before: the old code, its
straight runs counted by the new measure. B: the plateaus' outer faces worn
as cliffs instead (the other choice, one line in `Erosion.cpp`).

| region | wall and cliff before | after | (B) | straight runs before | after | horizon at the pad before | after | (B) |
|---|---|---|---|---|---|---|---|---|
| start | 15.5% | 15.4% | | 7 | 0 | 76.6% | 76.6% | |
| rust-hollow | 24.8% | 24.8% | | 9 | 0 | 73.4% | 73.4% | |
| ashen-shelf | 29.0% | 28.8% | | 8 | 0 | 62.5% | 62.5% | |
| tern-ridge | 19.2% | 19.1% | | 9 | 0 | 79.7% | 79.7% | |
| cold-fork | 21.1% | 22.2% | 21.4% | 14 | 0 | 48.4% | 48.4% | 48.4% |
| glass-flats | 18.0% | 18.0% | | 8 | 0 | 59.4% | 59.4% | |
| brine-pans | 19.9% | 22.2% | 20.2% | 18 | 0 | 45.3% | 42.2% | 45.3% |
| dust-terraces | 33.0% | 34.0% | 33.5% | 23 | 0 | 50.0% | 50.0% | 50.0% |
| slag-pockets | 18.5% | 18.3% | | 9 | 0 | 67.2% | 67.2% | |

Open ground, free-standing rock and every raid line are as before (each
raid line has its way). The six regions without a canyon measure as they
did or a little better. The canyon regions pay 1 to 2.3 points of wall for
their sloped faces (a slope is wider than a cliff; plain worn cliffs, B,
cost 0.3 to 0.5, since a bent face is longer than a straight one). Brine Pans' pad loses one
of its 64 directions to the slope of the plateau's corner, 12 cells off.
All are within the targets (wall ≤ 25%, a canyon region ≤ 35%).

Stills, before and after, at the same spots (`-AcTerrainLabPoses`): The
Fork from above and its west face from the open floor, its north arm,
The Drowned Cut from above and its north face, Dust Terraces' wall rocks
(`art/terrain/rims/*-before.jpg`, `*-after.jpg`). The satellite maps are rebaked. A world saved on the
old land does not match: start a new world (NEW WORLD).

## Log

| date | step | note |
|---|---|---|
| 2026-10-18 | plan | the user's reference; the measures above |
| 2026-10-19 | S1+S2 trial | Prospect Landing opened (above); four neighbours' gaps widened to match |
| 2026-10-19 | S6 | landforms, styles, rules, eyes (section 8) |
| 2026-10-19 | re-made | the First Reach in landforms (section 9); the lab's rounds 1-4 promoted with it |
| 2026-10-19 | no straight edge | the user: the rims and The Fork's cut "too straight". Erosion (section 10): the rims wander, the canyon plateaus' outer faces take the rims' slope (A; the user may ask for B, worn cliffs), lanes worn, wall rocks in clusters never in rows; straight runs 7-23 a region → 0. Wall: Cold Fork 21.1 → 22.2% (B 21.4%), Brine Pans 19.9 → 22.2% (B 20.2%), Dust Terraces 33.0 → 34.0% (B 33.5%), the others as before or lower; Brine Pans' pad horizon 45.3 → 42.2% (B 45.3%). Start a new world |
