# The ground's look

Promoted from the terrain lab on 2026-10-19 (docs/terrain-lab.md, its
"Where it lives now" table). The sim never reads any of this: it is how the
client draws the field.

## The material

`/Game/Terrain/M_Terrain`, made by
`uv run unreal/Tools/hidden_run.py --out unreal/Saved/Terrain/mat --python unreal/Tools/Editor/make_terrain_material.py`.

- Inputs: `unreal/Content-src/terrain/TL_<Layer>_CH.png` (rgb colour, a
  height) and `_NR.png` (rg normal xy DirectX, b roughness, a occlusion),
  1024 px, made by `uv run unreal/Tools/terrain_textures.py [--sheet OUT.jpg]`
  from nine CC0 sets cached in `unreal/Content-src/terrain/raw/` (fetched
  once; sources and licences in `unreal/Resources/Textures/terrain/SOURCES.md`;
  Content-src is git-ignored). It also writes `means.json`, each layer's
  mean colour, which the material bakes in as constants.
- Layers, in the arrays' order: 0 Dirt (plain; rust and ochre tint it), 1
  Grass, 2 Highland, 3 Rock (cliffs), 4 Salt, 5 Ash, 6 Glass, 7 Slag, 8
  Frost. A region's palette picks its own by `GroundLayer`, its neighbours'
  by `NbLayer` (`unreal/Source/Autocraft/AcGroundLook.cpp`).
- The script imports the slices into `/Game/TerrainScratch`, builds the two
  arrays `/Game/Terrain/Textures/T_TerrainCH` (sRGB) and `T_TerrainNR`
  (linear), BC7, never streamed, clears their `source_textures` without
  notifying (each keeps its own copy of the source), deletes the scratch
  folder, then deletes and re-creates `M_Terrain` (a material rebuilt in
  place keeps its old textures among its dependencies and fails to load
  when they are gone).
- 1K, not 2K: a 2K array was 110 MB, over GitHub's 100 MB a file. At 1K
  they are 24 and 30 MB.
- The shader (a Custom node with a `Lab` struct of helpers): every layer
  hex-tiled (stochastic tiling; cells weighing under 0.03 skipped), a near
  scale (2.5 m) cross-faded to a far one (11 m, colour only) by distance,
  each layer recoloured to the game's palette colour by its mean, macro
  colour noise, sand ripples, layers meeting by height, parallax within
  `ParallaxRange` (8-12 m) faded out below a 30° look and never shifting
  more than 6 cm (at grazing angles it showed stacked slices swimming with
  the camera's height), the relief bake (`Relief`: occlusion and curvature,
  baked per region on a worker in `AcTerrain.cpp`, ~4 ms), wet ground,
  haze, the walls' tops in one world rock above `WallZ`.

## Rock faces and clutter

`AAcTerrainScenery` (`unreal/Source/Autocraft/AcTerrainScenery.cpp`), one
per ground set, kept by `UAcTerrainSceneryWorld`; `ac.TerrainScenery 0`
takes them away (for measuring). Its plan runs on a worker from the map
(`TerrainField` heights), 40-550 ms a region, then 3,000 instances a frame
are added (a region arriving never hitches).

- **Faces**: every point (0.5-cell grid, jittered) steeper than 1.3 ×
  `NavGrid::maxSlope`, ≥ 1.7 cells apart, not within 2.5 cells of a ramp
  (base or height-edit): a stack of slabs (one per 2.6 cells of rise, at
  most 5) from the face's foot to its top, leaning with the face, set 0.45
  cells in so the front stays under 0.3 cells out; X scale 0.55-0.8.
  Faces under 0.6 level of rise get none. Colour: the region's rock tinted
  by its palette (walls 85% tinted, its own cliffs fully).
- **Talus**: two stones a face 0.1-1.4 cells out from its foot, ≥ 1.3 apart.
- **Crowns**: a `spire_*` mesh on each `kind: spire` island, 1.5 × its
  mean radius wide.
- **Clutter**: 0-2 a cell on gentle ground (slope ≤ 0.35), off walls,
  plating and 5.5 cells round pads, deposits and wells, culled at 35 m, no
  shadow, no GI: plain/rust/ochre pebbles and talus, dry tufts where grass
  > 0.35 (70%); salt crust; ash flakes; glass shards (scale 0.3-0.7); slag
  clinker; frost rime.
- **No distance fields** on any of it (`bAffectDistanceFieldLighting`
  false): with them the night scene cost 6 ms in distance-field shadows.
- The kit: 48 meshes in `/Game/Terrain/Rocks` (slab 8, talus 6, spire 4,
  pebble 6, crust, shard, clinker, flake, rime, tuft 4 each), made by
  `uv run unreal/Tools/hidden_run.py --out unreal/Saved/Terrain/rocks --python unreal/Tools/Editor/models/terrain_rocks.py`
  (own Geometry Script buffers, no model catalog; `--preview art/terrain/rocks.jpg`
  renders a contact sheet without the editor, back faces red). Their
  material `/Game/Terrain/M_TerrainRock` (`make_terrain_rock_material.py`):
  the arrays on three world planes, light and dark only (the texture's own
  hue fought the palettes), coloured per instance (custom data 0-2 colour,
  3 slice).

Regenerate in this order after changing a script: textures → material →
rock material → rock kit; then `unreal/Tools/build.sh` if C++ changed.

## Budget

`uv run unreal/Tools/bench/bench.py --mmo --gpu --rounds 3 --size 5120x1378@2 --out unreal/Saved/Bench/<name> --variant base= --variant x=<flags>`
(never under `hidden_run --raw`; note the load, interleave, repeat). The
ground's four rounds together cost +0.0-0.7 ms GPU busy piloting and
+1.3-1.4 ms in the fight at 5120 × 1378 (budget 2.5 ms in all); the
arrival hitch stays at the base's (283 vs 412 ms max). `ac.TerrainScenery 0`
as a variant isolates the faces and clutter.
