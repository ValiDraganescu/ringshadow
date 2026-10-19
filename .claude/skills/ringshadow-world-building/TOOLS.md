# The tools

All from the worktree root. `T=unreal/server/build/autocraft-region`, built
by `make -C unreal/server build/autocraft-region` (plain clang++, `-Werror
-Wshadow`, seconds). Its usage text is the authority for flags.

## autocraft-region

| command | does | prints |
|---|---|---|
| `$T new --style S --seed N --size small\|medium\|large\|huge [--id ID] [-o FILE]` | a file with only the base (`openField` for landform regions) | the file |
| `$T validate FILE` | parses, expands, checks (LANDFORMS.md "The rules" and docs/regions.md "Problems") | one problem a line with its region position, then `ok` or `N problems`; notes (a vault reached by a ramp or a cliff hop) are lines before `ok`, not problems; exit 1 on problems |
| `$T build FILE` | builds the map | size, base, ground counts, ore/MH, doodads, pads, spawns, sites (how reached), openings, then the measures (LANDFORMS.md "The measures"), the problems |
| `$T preview FILE -o OUT.png [--scale PX]` | the top-down map: levels by colour, cliffs red-brown, rocks dark discs (drawn larger than they are), ore violet, wells, pads (yellow ring), spawns (green), sites, the dotted border outline, cyan passages with `L<level>`, problems ringed red and listed under | the path, the problem count |
| `$T view FILE -o OUT.png [--from AZ\|n\|e\|s\|w] [--pitch DEG] [--at X,Z] [--dist CELLS] [--fov DEG] [--size WxH] [--eye]` | a software ray-marched relief at an angle: hillshade from a sun at 300°/35°, cast shadows, haze, cliffs darker rock, walls grey, islands warm rock, rocks as domes, spires as cones (3 × scale tall, a guess), ore/wells/pads/spawns/sites marked, the preview's 8-cell grid faint. Default: from the south, 35° down, framing the whole region. `--eye`: 1.3 cells over the first pad (or `--at`), looking toward `--from`, 2° down, 75° FOV. 0.2-0.6 s | the path |
| `$T expand FILE` | the file with its landforms drawn in | the JSON to stdout; `style: …` lines to stderr |
| `$T edges FILE [--depth D]` | the ground's level along each edge, D cells in (default 6): a digit every 2 cells, `#` a cliff, `^` under an opening | four lines |
| `$T levels FILE [--bare]` | the ground a cell at a time: a level digit, `#` a cliff, `o` rock or wall in the way; `--bare` without the border | a grid |
| `$T format FILE` | rewrites the file in the tool's layout | `wrote FILE` |

`view`'s scenery past the bounds is `borderHeight`, not the neighbours: in
game the neighbouring regions stand there instead.

## sculpt.py (`unreal/Tools/regions/sculpt.py`)

`uv run unreal/Tools/regions/sculpt.py REGION [--grow 8] [--level 1] [--slope 5] [--rocks SPEC] [--seed 1] [--no-widen] [--dry]`

Edits files in place, then formats and validates each it wrote (prints
each `validate` result). Phases, in order:

1. **Widen** (skipped with `--no-widen`): for each edge shared with a
   neighbour (the world's grid is hard-coded in `GRID`, the First Reach's
   3 × 3; edit it for another world), the
   bare ground (`levels --bare`) 5-14 cells in on both sides; every run of
   cells level 0 on both sides, trimmed 2.5 at each end, kept ≥ 10 cells
   from the corners, ≥ 10 long, is merged with the existing spans (spans
   closer than 10 cells merge). Both files get the new `openings` and each
   outline gains a strip from the edge in to the land over every span
   (span ± 1.5 + 0.5). Writes the neighbours' files too.
2. **Grow**: the outline grown by `--grow` cells over walkable bare level-0
   ground, kept 3 cells inside the rectangle, its passages' strips kept,
   smoothed (buffer ±2.5), simplified (0.35).
3. **Rim**: `border.level = --level`, `border.slope = --slope`.
4. **Rocks**: `--rocks "kind:level,…"` places free-standing islands (S2,
   the first trial's way). Pass `--rocks ""` always: rock comes from
   landforms now.

Gotchas:
- A widened span that reaches into a corner leaves its strip disconnected
  from the land; the outline keeps only the biggest piece and `validate`
  says "the border's outline closes the passage here". Trim the span in
  both files of the pair to where the land is.
- Run `--no-widen` when other workers edit neighbouring files: widening
  rewrites them.
- It reads the bare ground through the built tool: rebuild the tool after
  a core change first.

## critic.py (`unreal/Tools/regions/critic.py`)

`uv run unreal/Tools/regions/critic.py REGION [--ref IMAGE] [--shots DIR] -o SHEET.jpg`

REGION is an id or a path. One JPEG: the top-down preview, four oblique
views (n, e, s, w), two `--eye` views from the first pad toward its two
most open directions (lowest skyline within 60 cells, ≥ 90° apart), the
reference image, up to 15 in-game stills from `DIR/<region>/game/*.png`,
each labelled, and `$T build`'s text under them. It builds the tool
first; pictures go to `unreal/server/build/critic/<id>/`. About 5 s.
The user's open-desert reference: ask the user for it, or reuse the one
the brief names.

## In-game stills (`-AcTerrainLabShots`)

```
uv run unreal/Tools/hidden_run.py --out unreal/Saved/Terrain/world --res 1280x720 --timeout 3000 -- \
  -AcMap=highlands-small -AcNoAI -AcTerrainLabShots={out} \
  -AcTerrainLabRegions=start,rust-hollow,… -AcTerrainLabHours=17 -AcTerrainLabWeathers=clear \
  -AcTerrainLabViews=feet,across,third
```

Named poses for before-and-after stills of one spot:
`-AcTerrainLabPoses="cold-fork:forktop@106,18,rts;cold-fork:forkwest@84,24,1,-0.1,-3"`
(`REGION:NAME@X,Z,rts`: the RTS camera over region cell X,Z;
`REGION:NAME@X,Z,DX,DZ,PITCH`: the eye there looking along DX,DZ), each
NAME listed in `-AcTerrainLabViews`.

Writes `DIR/<region>/game/NN-<view>-<HH>-<weather>.png` and
`DIR/<region>/views.txt` (where each view stood). Views, found on each
region's own ground (`unreal/Source/Autocraft/AcTerrainLab.cpp`):
`feet` (1.3 cells up, 20° down, flat ground past the pad), `cliff` (at a
cliff's foot looking up), `across` (the highest flat ground clear of rock
doodads, toward the way the eye sees furthest), `third` (behind a
Prospector half way up a ramp: a region with no ramp gets a black still),
`top` (the RTS camera over a well). Hours default 9,17,23; weathers
clear,rain. Never launch the game any other way; one hidden run at a time
on the Mac (`hidden_run.py` queues).

## bake_map.py

`uv run unreal/Tools/bake_map.py [--only id,id]` photographs every region
as the client builds it and imports `/Game/UI/Map/T_AcMap_<id>`. Run it
after any region's ground changes. Its import re-saves every map texture;
`git checkout` the ones whose region did not change.

## Tests that read the ground

- `make -C unreal/server test FILTER=world_load`: every shared edge's
  passages line up and the ground meets.
- `make -C unreal/server test` (~380 tests): many scenarios stand on the
  First Reach. When the ground changes, a test that placed something by
  coordinates fails. Make it choose by a rule, with the helpers that exist:
  - `rig::rimGunSpot` (`unreal/server/tests/GunAim.h`): the buildable rim
    spot whose shots reach most of a named canyon path;
  - `rig::citadelSpot`, `rig::citadelStand`: where a Citadel fits;
  - `openGround(f)` (ScarcityTests): a spot > 12 cells from every mound
    and well that adds no validate problem;
  - `nav->nearestFree`, `nav->walkable`, `sim.pilotCanStand`.
  `RimTests` checks every region of the world for straight runs, rows of
  wall rocks, and the canyon paths on open floor.
  Region-shaped facts in tests: `WorldTests` names Start's east span in
  two messages (update the text when the span changes) and pairs Start
  with its real east neighbour for the walled-edge cases;
  `RegionFileTests` checks wall heights against `RegionEdges::edgeLevel`.
- `make -C unreal/core-tests`, `make -C unreal/net-tests`: must pass
  unchanged.
- The game's own Citadel raid test (`defence_citadel_turrets_beat_a_raid`)
  is the canary for rock near a pad: it fails when a form stands in the
  guns' field of fire.
