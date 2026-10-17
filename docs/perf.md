# Performance

The standing goal since 2026-10-18: make the client and the server faster,
a measured step at a time. Each round names the bench, the numbers before
and after (run in turns on the same Mac, with its load), and what stays
equal. The client's rounds are under "Client", the server's under "Server".

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
