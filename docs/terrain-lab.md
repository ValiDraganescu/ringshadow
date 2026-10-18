# The terrain lab

Status, 2026-10-18: the lab is built (the switch, the sheet, the material
script); round 1 is in its first iterations. The user's ask: the ground needs to look
better, and the work happens in a sandbox first, a test game of its own,
and only what we like is copied into the game.

This page is the contract: where the lab lives, what it may touch, the
rounds, how each is judged, and how a round's result is promoted. Where the
code and this page disagree while the work runs, the worker's code wins and
this page is corrected.

## 1. Where the ground stands

- The ground is a runtime mesh built at map load from `ac::TerrainField`
  (plateaus, ramps, cliffs, noise), 4 vertices a cell, 32-cell tiles
  (`AAcTerrain`, `AAcRegionGround`, `AcTerrain.*`). One cell is 1 m.
- Its look is `M_Terrain`, the Swift ground shader as one Custom HLSL node
  (`Tools/Editor/make_terrain_material.py`): five 1024 px JPEGs from the Swift
  game (dirt, grass, highland, cliff rock, plating), each at two scales, Sobel
  normals (`Tools/terrain_textures.py`), triplanar cliffs by slope, a splat map
  (grass, highland, plating, scorch), a ±6 % large-scale tint, wet ground
  (docs/weather.md), the region palettes as tint and effects on top
  (`AcGroundLook.*`, docs/regions.md "ground"), one rock colour for the
  walls between regions (`WallZ`).
- Rocks, boulders, spires and plants are instanced doodads (`AAcDoodads`,
  `FAcSceneryBatches`), about one every 3.4 cells on the base's ground.
- What the cockpit sees (docs/media/pilot-ore.jpg): a flat sheet of one
  texture with visible repetition, cliffs as soft lumps (a heightfield has no
  rock faces; the cliff is a 0.6-cell smoothstep), nothing between the feet
  and the horizon: no pebbles, tufts, cracks, ripples, strata.

## 2. The rules

1. **The sim's ground never changes.** `TerrainField` is shared bit for bit
   by the server and every client; it drives paths, rays, placement and the
   goldens. Everything the lab makes is drawn on top of it: material,
   instanced scenery, decals, textures. A displacement in the material
   stays under 3 cm so feet stand on the ground.
2. **Nothing of the game's terrain changes until a round is promoted.**
   The lab's assets live in `unreal/Content/TerrainLab/`, built by their own
   scripts (`Tools/Editor/make_terrain_lab_material.py`,
   `Tools/terrain_lab_textures.py`), beside the game's. The game's
   `M_Terrain`, `AAcDoodads` and their scripts are read, not edited.
3. **One switch.** `ac.TerrainLab 0|1` (and `-AcTerrainLab` on the command
   line) picks the lab's material (`/Game/TerrainLab/M_TerrainLab`, swapped
   in place on the ground already built: `AAcRegionGround::Rematerial`) and,
   from round 3, the lab's scenery actor over the game's, so an A/B is one
   shot each, in the same build. Off by default. The palettes tell the lab
   which of its textures is a region's own (`AcGroundLook`'s `Layer`, the
   `GroundLayer` and `NbLayer` parameters; M_Terrain has neither and is
   unchanged).
4. **Only what the game can ship.** No Landscape, no Nanite, no distance
   fields, no editor-baked meshes of a region: they need the editor's
   builders and our maps are built at load. Lumen sees the ground by screen
   traces only, so occlusion must be baked by us (round 2).
5. **CC0 only.** The repo is CC0. Textures come from ambientCG or Poly Haven
   (both CC0) or are made here (Grok Imagine, `mcp__orchestrator__grok_image`,
   then the Sobel step for normals). Each set is listed with its source in
   `unreal/Resources/Textures/lab/SOURCES.md`. Nothing from Fab or Megascans.
   2K; 4K stays out. While a round is tried, the lab's assets
   (`unreal/Content/TerrainLab/`, about 380 MB of texture arrays) stay out of
   git: `uv run unreal/Tools/terrain_lab_textures.py` downloads and packs the
   sets, the material script makes them again. A promoted round's textures
   go in with it.
6. **Every round is measured.** `unreal/Tools/bench/bench.py --mmo` before
   and after, the cockpit scene and the fight scene; see section 5.
7. The worker's own worktree (the `ringshadow-dev-agent` skill), branch
   `terrain-lab`, built with `unreal/Tools/build.sh`, every run through
   `uv run unreal/Tools/hidden_run.py`. Never relaunch the editor.

## 3. The lab run

`-AcTerrainLabShots=DIR` (`AcTerrainLab.cpp`, a world subsystem like
`-AcCloudShots`) runs the sheet: for each region of
`-AcTerrainLabRegions=` (default `start`; `brine-pans` for salt and
`slag-pockets` for slag) it builds the ground as a client builds it, from
the region file with no server (`ac::net::Mirror`, as the map bake does),
with the region's own palette, stands a Prospector on the ramp nearest the
pad, and saves every still with the lab on, then with it off when asked
(`-AcTerrainLabAB`, or `-AcTerrainLabLooks=lab,game`). Materials change
through the headless editor script (`hidden_run.py --python`), so a texture
or material round needs no C++ build and no editor restart: edit, script,
shot, look.

```sh
uv run unreal/Tools/hidden_run.py --python unreal/Tools/Editor/make_terrain_lab_material.py --out unreal/Saved/TerrainLab/mat
uv run unreal/Tools/hidden_run.py --out unreal/Saved/TerrainLab/r1 --res 1280x720 -- \
  -AcMap=highlands-small -AcNoAI -AcTerrainLabShots={out} -AcTerrainLabAB \
  -AcTerrainLabRegions=start,brine-pans,slag-pockets
```

`-AcTerrainLabViews=`, `-AcTerrainLabHours=`, `-AcTerrainLabWeathers=` and
`-AcTerrainLabFrames=` (90) narrow or change the sheet. The stills land in
`DIR/<region>/<look>/`, and `DIR/<region>/views.txt` says where each view
stood.

The sheet, each still `NN-<view>-<hour>-<weather>.png` at 1280×720, NN
the same in both looks:

| view | where | why |
|---|---|---|
| `feet` | cockpit (62° vertical), eye 1.3 m up, looking 20° down at the nearest flat, clear ground out past the pad, facing away from it | tiling, parallax, clutter at arm's length |
| `cliff` | cockpit at the foot of the cliff nearest the pad, looking 18° up the face | rock faces, strata, occlusion |
| `across` | cockpit on the region's highest flat ground, looking 7° down toward its far side | mid and far scale, the haze, repetition at distance |
| `third` | the third-person boom behind a Prospector half way up the ramp nearest the pad (a ramp edit of the region file) | the ground under a unit, shadows |
| `top` | the RTS camera at the game's zoom (`DefaultCover`) over the well nearest the pad | the map read: nothing the lab adds may muddy it |

Hours 9, 17 and 23 (`-AcHour`), weather clear and rain (`-AcWeather`), so
one region makes 30 stills; the 23 h stills show the lamps and the slag's
embers. The worker puts every round's sheet, lab beside game, in an Artifact
page for the user to pick from (never paths alone).

## 4. The rounds

Each round: a sheet, a bench, an Artifact page, a line in the table in
section 7, then the user says promote, iterate or drop. Rounds 1 and 2 are
material only. Rounds 3 and 4 add `AAcTerrainLabScenery`, one actor of
instanced meshes and decals, client-only, deterministic from the region's
seed (every player sees the same pebbles), no collision, culled by distance.

### Round 1: textures and tiling

- A CC0 PBR set per ground: dirt, grass, highland, cliff rock, plus one each
  for salt, ash, glass, slag and frost (today a tint on the dirt). Albedo,
  normal, roughness, height, 2K.
- Stochastic tiling (hex or texture bombing) on every ground layer: no
  visible repeat from the `feet` view to the `across` view.
- A near scale (0.5 m) and a far scale (8 m) cross-faded by distance, and a
  macro colour noise over 30-80 m: lighter crests, darker hollows, by the
  field's own height.
- Sand ripples as a mid-scale normal on slopes, wind along the region's x.
- Keep: the splat map, the palettes' tint and effects, the wet ground, the
  haze, `WallZ`. The lab material starts as a copy of `M_Terrain`'s script.
- Accept when: the `feet` and `across` stills show no repeat, the `top` still
  reads as today's map, GPU within budget.

### Round 2: depth and occlusion

- Parallax occlusion mapping from the height maps within 12 m of the camera,
  8-16 steps, off past it; cracks, pebbles and plates get depth.
- An occlusion and curvature map baked from the heightfield at map load, on
  the CPU, 4 px a cell like the minimap's bake (`AcMinimapBake::Terrain`),
  as a second texture to the material: cliff feet, gullies and the inside of
  craters darken, crests lighten; cavity darkens the cliff's cracks.
- No strata bands on the cliffs (the user, 2026-10-18: today's horizontal
  bands repeat too much; round 1's cliffs have none and stay so). Depth on
  the rock comes from its own height map and the cavity bake, and a darker
  lip at the plateau's edge only where the bake finds one.
- Accept when: the `cliff` and `feet` stills have depth at noon, nothing
  pops when the parallax cuts out.

### Round 3: cliff faces

- The field knows its cliff runs (`TerrainField::PolygonRuns`). Along each
  run, rock-face meshes: 3-4 slabs of layered rock (the model pipeline or
  Blender through nix, exported as the game's models are), 2-4 cells long,
  sunk into the face and rotated to it, instanced, with the region's wall
  colour where the run is a wall and the ground's rock otherwise. Talus
  (small rocks) at the foot, every 1.5 cells.
- The faces lean out over the ground at most 0.3 cells: the sim's cliff is
  still where the ray says, a unit at the foot still stands where it stood.
- Ramps get no faces (their sides are drivable slopes).
- The meshes are a rock kit, not a unit: no concept sheet and no Grok
  turnaround. A Geometry Script file like the doodads'
  (`unreal/Tools/Editor/models/terrain_lab_rocks.py`, through
  `ac_modelkit`) writes 6-8 slabs, 4-6 talus stones and, for the
  sculpting chapter's buttes, 3-4 spire tops, each a box or a prism with
  layered, noise-cut sides, to `/Game/TerrainLab/Rocks/`. Hiccups go into
  the model pipeline skill as usual.
- Accept when: the `cliff` still shows a rock wall, not a lump; the `top`
  still keeps the cliff's line; units on a ramp pass with no face in the way.

### Round 4: clutter and decals

- Clutter within 40 m of the camera by palette: pebbles and tufts on plain
  and rust, crust plates on salt, flecks on ash, shards on glass, clinker on
  slag, rime patches on frost. 1-3 instances a cell, 4-6 meshes a kind,
  never on a mound, a pad, a well or a building's footprint.
- Decals: ore stain round each mound, scorch under impacts (the Impact
  event), cracks on flat high ground, tyre ruts behind a hauler (later).
- Accept when: the `feet` still has something to look at at every palette;
  the `top` still is not noisier than today.

### Round 5: rocks in the ground

- A runtime virtual texture of the ground colour; rocks, boulders, spires,
  talus and the lab's clutter sample it at their base so they sit in the
  ground instead of on it.
- Accept when: no hard dark line at a rock's foot in the `feet` and `cliff`
  stills; cost within budget.

## 5. Budget

GPU busy, `bench.py --mmo`, median of 3 interleaved rounds, load noted, the
weather chapter's baseline (docs/weather.md): the fight 13.5 ms, the pilot
16.2 ms (5120×1378). The lab may add at most **1.0 ms a round in the pilot
scene, 0.5 ms in the fight, 2.5 ms in all** over the five rounds. A round
over its budget is not promoted until it is under. The terrain's build time
(`AAcRegionGround::BuildSeconds`) may grow by at most 20 %: the world
builds regions while the pilot walks.

## 6. Promotion

When the user says promote, in the worker's worktree, as one commit a round:

1. The lab script's outputs take the game's paths (`/Game/Terrain/M_Terrain`,
   the textures), the game's script is replaced by the lab's, the old
   textures stay in git history only. `ac.TerrainLab` keeps picking the lab
   until every round is in, then it goes.
2. The scenery actor's default flips on; `AAcDoodads` stays.
3. The bench before and after is written into docs/perf.md.
4. The shot tests (`-AcMmoShots`, docs/mmorpg-shots.md) get the new look's
   stills: the user looks at the sheet before the merge.
5. The manager merges `terrain-lab` into the MMORPG branch with a three-way
   merge; `make -C unreal/core-tests` must still pass unchanged (the lab
   touches no core).

## 7. The log

| date | round | result | GPU pilot / fight | promoted |
|---|---|---|---|---|
| 2026-10-18 | plan | this page | 16.2 / 13.5 baseline | – |
| 2026-10-18 | 1 | nine CC0 sets, hex tiling, near and far scale, recoloured to the game's colours; first cut over budget (base pass +0.8 to +1.7 ms) | +0.9 to +1.7 GPU busy, MMO bench 5120×1378, load 5-9 | – |
| 2026-10-18 | 1+2 | weak hex cells skipped, far scale colour only, means as constants; parallax within 12 m, the relief bake (occlusion, curvature; 4 ms a region on a worker); no strata bands | +0.1 to +0.5 GPU busy (base pass +0.3 to +0.9), load 4.3 | – |

## 8. Open

- Which palette shows the lab first besides plain: salt (Brine Pans) and
  slag (Slag Pockets) are the plan; the user may want frost (Cold Fork).
- Tyre ruts and footprints: a later round, with the sim's Moved events.
- World sculpting (the user, 2026-10-18: "always surrounded by cliffs";
  canyons are fine, not everywhere): not this lab, since it changes the sim's
  ground (rule 1). Its own chapter: [world-sculpting.md](world-sculpting.md),
  low rims, free-standing buttes and spires, rolling ground, far land. It
  comes before round 3, whose faces follow the cliffs wherever they end up.
- Far mountains past the world's rim (the haze hides a flat horizon today):
  not this lab; a note for the world's edge.
