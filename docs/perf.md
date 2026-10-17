# Performance

The standing goal since 2026-10-18: make the client and the server faster,
a measured step at a time. Each round names the bench, the numbers before
and after (run in turns on the same Mac, with its load), and what stays
equal. The client's rounds are under "Client", the server's under "Server".

## Client

The game as a player runs it in the MMORPG world (`unreal/Source/Autocraft`,
the core as the client runs it, the renderer settings). What the player
sees stays the same, and the core stays bit for bit. Only the MMO counts
(the RTS mode is legacy).

**The MMO bench** is the standard: `uv run unreal/Tools/bench/bench.py
--mmo [--rounds N] [--gpu] [--variant NAME=FLAGS ...]`. Each run is one
world client through `hidden_run.py` (its own slot, so A/B variants take
turns run by run) on highlands-small's world with clear weather, running
the `-AcMmoShotsBench` set (AcMmoShots.cpp). It has four scenes, each
settled, timed `--seconds` (10) on the wall clock and then pictured:

- `mmo-home-noon`: Prospect Landing at noon, facing the pilot's Citadel.
- `mmo-home-night`: the same at midnight, with the lamps and the headlamp.
- `mmo-nest-noon`: Slag Pockets at noon, 55 cells from the nest, her horde in sight.
- `mmo-fork-wave`: Cold Fork at noon, wave 10 down The Fork, facing the nearest bug.

`FAcBenchMeter` (AcBenchMeter.h) times the scenes. It reports the wall
frame (p50, p95, p99, max, frames over 33 ms), the game, render and RHI
threads, the GPU frame and the driver's GPU busy time, the game's tick and
its parts, and memory. With `--gpu` it adds `stat gpu`'s passes, read
after the timed frames. `report.md` has the table and the passes. On Metal
a pass's time can include earlier GPU work: trust an A/B toggle over a
single pass. `ac.HudHide 1` hides the whole HUD, to measure what it costs.
The numbers come from the editor binary (`UnrealEditor -game`); a cooked
game skips the editor's own passes (`SkyAtmosphereEditor`, 0.1 ms) and its
checks.

### Round 1: the bench, and the ISM materials kept (2026-10-18)

**Where the frame goes.** The MMO client is GPU-bound in every scene. At
1920×1080 the game thread takes 2.3-3.4 ms of an 11-16 ms frame, and the
render thread waits on the GPU. The GPU passes at home at noon (`--gpu`,
ms a frame): shadow depths 3.7, deferred lighting 3.5, Lumen screen probes
2.0, post 1.5, TSR 1.2, lights 0.9, clouds 0.6. At midnight the shadow
depths take 5.0 ms. The HUD costs no measurable GPU time: with `ac.HudHide
1` the GPU frame and the 1.8 ms "Slate UI" pass stay the same (Metal puts
earlier work under that marker). The HUD costs about 0.3 ms of the game
thread. The console's warp (`ac.ConsoleWarp 0`) changes nothing measurable.

The game thread (xctrace, nest at noon, 18 s) is 22 % busy. Of that, the
game's tick and its frame stages are 26 %, Slate 18 % and the engine's
end-of-frame updates 15 %.
In those updates every instanced mesh with moved instances works out its
materials' property descriptor again from each material's relevance
(`GetUsedMaterialPropertyDesc` -> `GetUsedMaterials`), 5.4 % of the game
thread. On the RTS bench's late8 (1,450 units) it is 5.3 % of a
game-thread-bound frame.

**The change.** `UAcInstancedMesh` (the renderer's batches, the life bars,
the ore, the effect pools, the particles) overrides
`BuildComponentInstanceData`. It does the engine's work line for line but
keeps the descriptor. It works it out again when the mesh, the override
materials or the shader platform change, and every
`ac.RenderMaterialDescFrames` (64) updates in case a shader finished
compiling. 0 is the engine's path, every update, for A/B. Conservative
bounds are never on for these components, so the engine's other branch
never runs.

| MMO bench, 2 rounds in turns, load 7.6-7.7 | game p50 | game p95 | RHI p50 | frame p50 |
|---|---:|---:|---:|---:|
| home noon: engine / kept | 3.5 / 3.0 | 3.9 / 3.3 | 2.0 / 1.7 | 13.9 / 13.6 |
| home midnight | 4.1 / 3.3 | 4.5 / 3.7 | 2.3 / 1.8 | 16.7 / 15.7 |
| nest at noon | 2.6 / 2.4 | 3.0 / 2.5 | 1.7 / 1.5 | 11.3 / 11.1 |
| Fork, wave 10 | 2.9 / 2.6 | 3.1 / 2.8 | 1.8 / 1.6 | 11.7 / 11.7 |

The game thread's p95 drops by 10-18 %. The frame stays where the GPU puts
it. The pictures are the same: the descriptor is the one the engine would
work out.

**Before the MMO-only rule**, the RTS late8 bench (1920×1080, 1,450
units, load 6.5-7) was game-thread-bound: GPU p50 13.2 ms, game p95 22.6
ms. Its game thread was the sim step (37 %: `Vision::sight`/`stamp` 12 %,
the server worker's), the renderer's sync (15 %) and the engine's instance
updates (18 %).

**Next.** The biggest GPU cost is the virtual shadow maps. The shadow
lights turn a little every frame with the world's clock, and any turn
throws away the whole shadow cache. With the sun held still, shadow depths
drop from 3.65 to 1.31 ms at home and from 2.81 to 0.95 ms at the nest, so
the GPU frame at home goes from 13.1 to 10.8 ms. `r.Shadow.Virtual.Cache=0`
measures the same as today. Round 2 turns the shadow lights in small steps.

## Server

The MMORPG server (`unreal/server`), its endpoint (`unreal/Source/AutocraftNet`)
and the core as the server steps it. Results stay equal: the world's
deeds, the ledgers, the save, the wire bytes and the simulation's hashes.
The benches are `make -C unreal/server bench` (-O3); the M4 Max is shared,
so every number carries the load average it ran under.

### Round 1: the first board's walking grids (2026-10-18)

`worldbench 30` (The First, one pilot driving, 1,800 ticks) showed a max
step of 12.6 ms against a mean of 0.026 ms, always on tick 1. The new
"slowest steps" line in the bench named it, and timing the world's step
phases found `missions()`: a pilot's first mission board asks which sites
it can reach, and `region::siteReach` built a terrain field, a walking grid
and a game state for every region with sites, inside the 60 Hz tick. It
was cached afterwards, so it hit the first pilot after each server start
(and the first contract board too), a stall for every pilot in the world.

The world's load already works out the same reach while it checks each
region (`region::validate` walks the same grid with the same `Reach`). It
now hands it back (`validate(file, built, reach)`), the load keeps it on
`WorldRegion::siteReach`, and `WorldMap` starts with it. The lazy path
stays for a world put together without a load. The load does no more work
than before.

| `worldbench 30`, 4 runs each in turns | load | mean ms | p95 ms | max ms |
|---|---:|---:|---:|---:|
| before | 11-12 | 0.042-0.051 | 0.154-0.192 | 20.6-24.8 |
| after | 11-12 | 0.030-0.034 | 0.153-0.173 | 0.31-0.75 |

At a load of 8 the before binary's max was 12.3-12.7 ms. The mean drops
by the one tick it no longer spends. The core's `RegionFile` gained the
overload; the simulation is untouched (core-tests 521/521, the goldens
equal). Test: `sites_reach_from_the_load_is_site_reach` (the load's reach
equals `region::siteReach` in every region of The First).

### Round 2: the walking grids at the start (2026-10-18)

Two new benches: `savebench` (the soak's world with its pilot on line,
saved every 5 s of game time as the server saves) and `loadbench` (the
world's start: `loadWorld`, then `WorldMap`). The save costs 0.5 ms every
5 s (0.23 ms building the save, 0.28 ms writing about 20 rows), not worth a
round yet. The start was: Time Profiler on `savebench` put 79% of the
process's CPU in `NavGrid::buildGround`, the walking grid sampled from the
terrain (16 threads, kept per process in `NavGrid::cache`), and 45% of
everything in one function, `landOutside`: the signed distance to a
region's outline (130-175 edges), asked some 20 times a cell (the slope
test's heights each walk the region's walls) over 55,000 cells a region.

All in `TerrainField`, all bit for bit:

- `PolygonRuns`: the outline's and islands' edges in runs of 8, each with a
  box. The nearest edge: the nearest box first, then only the boxes not
  further than the nearest edge so far (the minimum does not hang on the
  order, and a margin covers the rounding); the inside: only the runs whose
  boxes span the point's row, found by 32 bands across the polygon. Each
  edge goes through the same two helpers as `signedDistance` itself.
- `openLand(p)`: inside the outline, out of every island and off every
  edge's band, `wall(p).coverage` is exactly 0: known from the sign alone.
  `walled`, `materials` and the grid's wall test ask it first.
- `wallNear(p)`: `walled`, `materials` and the grid read the distance only
  through smoothsteps that stand at 0 on the land and at 1 past 11.7 cells
  (the deepest terrace's shelf plus a cliff's width), so they take it
  capped at `wallReach()` (12.7): exact below, the runs past it not
  measured. `wall(p)` stays exact for everyone else.
- Noise not sampled where its weight is exactly +0: a zero product leaves
  any sum but -0 as it is (`plusNoise`): the crag on flat ground, a height
  edit's crag inside it (coverage 1), the wall's faces at coverage 0 or 1,
  its ridge and roughness where `away` is 0; and the plateau wobble's two
  octaves only once a plateau is within reach.

Checked: the ground and cliff layers of all 10 region files, and `height`,
`wall`, `materials`, `borderHeight` and `landOutside` on a quarter-cell grid
over each region and 4 cells past it, hashed by their bits: the same
before and after. Test `RegionFile_outlineRunsMeasureAsThePolygon` holds
the runs, `landOutside`, `wallNear` and `openLand` to the plain answers on a
half-cell grid in every region file. core-tests (the goldens), the server
and net suites pass.

| 10 region files' grids, 3 runs each in turns | load | wall ms | CPU ms (all threads) |
|---|---:|---:|---:|
| before | 7-10 | 501-515 | 5,435-5,592 |
| after | 7-10 | 264-270 | 2,695-2,718 |

| `loadbench 3`, 4 runs each in turns, load 8.4-8.5 | first `loadWorld` | its CPU | first `WorldMap` | warm `loadWorld` |
|---|---:|---:|---:|---:|
| before | 564-603 ms | 5.2-5.5 s | 261-263 ms | 68.5-69.3 ms |
| after | 311-323 ms | 2.59-2.63 s | 130-132 ms | 58.6-59.3 ms |

The server's start (The First, the grids cold) goes from about 850 ms to
450 ms, and its CPU from 5.5 s to 2.7 s; the client's local server and its
terrain sampling read the same code. Tried and left out: `Noise::hash`'s
division by 2^32-1 built from bits (h/(2^32-1) is the binary fraction
0.hhhh..., so its correctly rounded double is the pattern's top 53 bits,
rounded once, never a tie): equal on all 2^32 inputs, and slower than the
M4's divider (2.57 against 1.58 ns for four). What is left of the start:
`Noise::fbm` (29% of it), the runs' distance and inside (23%), and
`WorldMap`'s single-threaded 130 ms (each region's `Vision` samples the
terrain).
