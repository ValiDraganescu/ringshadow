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

### Round 3: the fog's look, and three dangling references (2026-10-18)

A world region looks every 0.1 s (`Simulation::look`): what each side sees.
By day (S3) a pilot's daylight is one circle over the whole region, about
55,000 cells, and Time Profiler on `savebench` put `Vision::stamp` at half
of the world's step. The new `lookbench` times the look alone in three
regions of The First, at noon and at midnight.

In the core's `Vision`, all bit for bit:

- `stamp` fills each row's span of the circle at once. A row's cells in
  the circle are one unbroken run (a cell's `dx` grows with `x`, rounding
  keeps the order, and the test grows with `|dx|`); its ends come from the
  half chord, set right by the same test, so the cells between need no
  test. A cone (a headlamp) keeps the cell-by-cell loop.
- The explored bits of a span are gathered in a register, a word at a
  time, and written once (they were a read-modify-write of one word, cell
  after cell, chained through memory), with restrict pointers.
- In the air or the daylight (every level): `near` and `seen` by `memset`,
  and the "not seen yet" bits 8 cells at a time: `seen` holds 0 or 1, so
  `(~w & 0x0101010101010101) * 0x0102040810204080 >> 56` packs the eight
  flipped low bits into a byte (byte k to bit k; checked on all 256
  patterns).
- `look` fills the last look's `Sight` arrays again (`Vision::sightInto`)
  instead of allocating two region-sized arrays a side each time, and
  moves the sides' `Intel` in and out instead of copying it.

Tests: `vision_stampSpansAsEveryCell` (2,400 circles of every size, in and
off the map, on every level and in the air, stamped over each other: the
same bytes as the cell-by-cell loop), `vision_sightIntoAUsedSightIsAFreshOne`;
the vision goldens and every core test pass.

| `lookbench 2000`, 2 runs each in turns, load 4-6 | before µs | after µs |
|---|---:|---:|
| start, noon (one pilot) | 62.3-65.0 | 9.4-9.5 |
| start, midnight | 7.7 | 4.9 |
| nest, noon (29 bugs, the Matriarch) | 124.4-125.2 | 25.9-26.0 |
| nest, midnight | 18.9-19.3 | 15.1-15.2 |
| Cold Fork, noon | 62.6-64.3 | 9.4 |

| `daybench 20`, the whole server tick by day, 3 runs each in turns, load 6 | before ms | after ms |
|---|---:|---:|
| nest, noon, daylight | 0.07 (p99 0.18-0.21) | 0.05 (p99 0.15-0.16) |
| canyon wave, noon, daylight | 0.06 (p99 0.21-0.22) | 0.04 (p99 0.09) |

**The dangling references.** Proving the look equal, the nest scene came out
with 29 bugs or 33 from run to run of one binary, and only of the new one.
An AddressSanitizer build found the cause in the base code: `nestThink`
held the Matriarch as a pointer into the region's units, then the horde's
spawns and a hatchling added units and the vector moved, so her site and
tether went to freed memory and her foe was read from a stale copy;
`roar` read her position after the hatchlings it adds; and the pilots near
her were pointers to loop-local copies (`GameState::unit` returns one).
What the step did hung on whether the freed block had been used again, so
any change in allocation could flip it. Fixed in their own commit: only
the steps that read freed or dead memory change. The same sweep found
reads of dead memory in three tests (a null unit, a temporary's array, a
temporary simulation's grid and a temporary copy's guns), fixed in the
tests.

`make -C unreal/server test-asan` (not the default) builds the server's
tests with AddressSanitizer and UBSan in `build/asan/` and stops at the
first finding: a few minutes to build, half a minute to run. The server,
core and net suites all pass under both now.

### Round 4: the fog on the wire (2026-10-18)

Time Profiler on `daybench`'s server tick by day put `SnapshotWriter::write`
at 31% of it, most in its own body. The new `snapbench` times a pilot's
snapshot apart from its writing, 20 a second with every part acknowledged,
and hashes every byte written. The writing was 40-55 µs a snapshot even at
midnight with nothing near: with the fog left out it was 3.5-15 µs. Timed
inside, nearly all of the fog was building its chunks (37 µs a snapshot:
the 27 chunks of 2,048 cells a region, 0.67 ns a cell), not comparing them
with their baselines (2 µs) nor writing the ones that changed (0.1 µs).

`fogChunk` (AutocraftNet's `NetSnapshot.cpp`) built a cell's byte with two
branches and a division each. Where `near` and `explored` cover the chunk
(the server's own fog, always) it now takes eight cells to a word: a byte's
bits folded into its lowest bit say "not 0" for `seen` and `near`, and a
256-entry table spreads the explored word's byte into bit 2 of each cell.
Short arrays and a ragged tail keep the cell-by-cell loop.

| `snapbench 20`, 3 runs each in turns, load 31-36 | before µs | after µs |
|---|---:|---:|
| start, noon, writing | 48.0-69.9 | 17.1-20.5 |
| start, midnight | 43.5-53.0 | 13.4-14.0 |
| nest, noon | 53.4-56.0 | 24.6-25.2 |
| nest, midnight | 39.4-49.4 | 13.9-27.7 |

The wire is the same: every scene's hash of all the bytes written is
equal before and after (`fd5e5104ff08f662`, `467fab1c80dd1394`,
`35325d9463465925`, `d4d978dd55fbf064`). Test
`protocol_fog_chunks_of_any_bytes_and_sizes_round_trip` (net-tests): fogs of
1 to 4,613 cells with any byte values, short `near` and `explored` arrays,
whole and as deltas, come back as the fog quantised. A pilot's snapshots
cost the server 0.4 ms a second instead of 0.9.

### Round 5: a crowd's fog, kept (2026-10-18)

The new `crowdbench` puts 1, 4 and 16 pilots in the start region at noon,
each driving, and builds and writes every pilot's snapshot 20 times a
second: with 16 the writing was the cost (29 µs a pilot a snapshot, the
world's step 46 µs). The fog is still most of a write, and it changes only
when the region looks (every 0.1 s, every other snapshot) and where it
looks; each writer built all 27 chunks every time and threw away the ones
equal to their baselines.

`SnapshotWriter` keeps each fog chunk it built with what it was built
from (the three arrays' sizes and their stretches over the chunk): the
same inputs give the same bits, so a chunk is built again only when its
cells changed, and a baseline that is the very chunk kept needs no
comparing. Test `protocol_fog_chunks_of_any_bytes_and_sizes_round_trip`
adds a fog of another size through the same writer.

| 2 runs each in turns, load 3 | before µs | after µs |
|---|---:|---:|
| `snapbench`: start, noon, writing | 18.6-20.2 | 11.7-11.8 |
| start, midnight | 14.0-15.3 | 7.8-8.0 |
| nest, noon | 27.3-28.3 | 19.5-20.3 |
| nest, midnight | 15.1-15.9 | 8.0-8.1 |
| `crowdbench`: 1 pilot, the whole tick | 15.7 | 11.2-13.1 |
| 16 pilots, a pilot's writing | 29.1-29.3 | 21.5-22.0 |
| 16 pilots, the whole tick | 230-231 | 188-193 |

The wire is the same (the four scenes' hashes as in round 4).

Measured, not changed:

- `pathbench` (new): 20,000 searches on the nest's grid, 5 to 40 cells,
  10.6 µs and 228 cells closed a search, every path hashed. The nest's
  bugs make about 100 searches a second (541 cells each), half its step.
  Keeping each cell's heuristic for the search (it was worked out again at
  every push) gave the same paths and no time back: the cost is the
  neighbour visits and the 4-ary heap, already unrolled and kept between
  searches.
- `travel` copied a walking unit's whole route every step; editing it in
  place kept `worldbench`'s state hash and moved nothing measurable in the
  nest (0.054 against 0.055 ms), so it stays as it was.
- The save: `savebench 300` (it now prints its slowest writes with the
  WAL's size) has the WAL growing about 55 KB a save for some 22 rows, and
  SQLite's checkpoint at 4 MB (every 70 saves, 6 minutes with pilots on
  line) makes that save 3-6 ms instead of 0.23. Inside one 16.7 ms tick of
  about 0.05 ms that is no hitch; left for now.

`worldbench` takes a region (`worldbench 30 worlds/first.json nest`) and
prints a hash of every unit's position, heading and hp at the end, the
check that a core change leaves the world's simulation bit for bit.

### Round 6: the writer's items, no allocation for the unchanged (2026-10-18)

Counted in `crowdbench`, a snapshot has about 60 items (the fog's chunks,
the sites, deposits and wells, the players, the units near), and about 42
of them come out the same as their baselines. Each one was written to a new
`shared_ptr<ItemBits>` (three allocations) only to be found equal and
thrown away, and the bit writer went a byte at a time with a `push_back`
for each.

- `canonInto` writes an item's bits into a scratch the writer keeps; the
  scratch is compared with the baseline, and the shared copy is made only
  for an item that is written.
- `BitWriter::writeBits` ORs a value's bytes in one pass after one
  resize (the bytes past the end are always zero: only these writes add
  to them); `BitReader::readBits` gathers them the same way; `appendBits`
  copies 56 bits at a time, reading only the bytes they lie in. Test
  `bitStream_wordWritesAreTheBitsOneByOne`: values of every width at every
  bit offset, and stretches appended anywhere, the same bytes as a writer
  of one bit at a time, read back the same.

| 2 runs each in turns, load 7 | before µs | after µs |
|---|---:|---:|
| `snapbench`: start, noon, writing | 9.9 | 7.2-7.8 |
| start, midnight | 6.7-6.8 | 5.6-5.7 |
| nest, noon | 17.0-17.2 | 12.4-12.7 |
| nest, midnight | 6.8-6.9 | 6.1 |
| `crowdbench`: 16 pilots, a pilot's writing | 18.2-18.7 | 14.8 |
| 16 pilots, the whole tick | 160-166 | 144 |

The wire is the same (the four scenes' hashes as before). Since round 3 a
pilot's snapshot writing went from 44-70 µs to 6-13 µs, and 16 pilots in
one region from 230 µs a tick to 144.

### Round 7: a crowd's daylight, stamped once (2026-10-18)

Time Profiler on `crowdbench`'s step with 16 pilots: the look was 43% of
it, `Vision::stamp` alone 31%. By day every driver gets a daylight circle
over the whole region (W5), and the test that skips a circle inside
another one drawn never fires for circles round different units, so all
16 were stamped over every cell, on top of the units' own circles.

`Vision::sightInto` stops stamping once a circle on every level has taken
in every cell: the grid is then all seen, near and explored, and nothing
left can change it. `Vision::coversGrid` says whether a circle does,
exactly: `stamp`'s test (the same sums) passing at the four corner cells,
since it grows with `|dx|` and `|dz|` and those are largest at the
corners. Test `vision_aCircleOverTheWholeGridLeavesNothingToStamp`:
`coversGrid` agrees with what `stamp` marks for circles a hair inside and
outside the farthest corner, from centres in and off the map, and after a
covering circle no stamp, on any level or in a cone, changes a byte.
`crowdbench` now hashes every byte written and every unit at the end.

| `crowdbench 10`, in turns, load 7.5 | before µs | after µs |
|---|---:|---:|
| 4 pilots, the step | 13.2 (p99 54.5) | 9.8 (p99 34.3) |
| 16 pilots, the step | 41.5 (p99 173) | 22.8 (p99 53) |
| 16 pilots, the whole tick | 152.8 | 135.8 |

The wire and state hashes are equal for 1, 4 and 16 pilots, and
`worldbench`'s state hash (the start and the nest) too.

Left: with several drivers, `boost` (read for each unit's speed every step
and its sight every look) swaps the other driver's whole context in and
back for each of its units (`swapDriver`: the `Pilot` copied three times,
`PilotProgress` and the side's record moved three times), about 2.7 µs a
tick with 16 drivers. Reading the driver's fields in place would need
`boost`, `addSkills`, `skilled` and `burst` to take the context; not done.

### Round 8: the empty regions (2026-10-18)

Every region steps every tick (P5), pilot or none. Stepped alone, an empty
region of The First cost 0.42-0.45 µs a step; Time Profiler put a third of
it in `NavGrid::setDynamic`, which built its key (the buildings, the
deposits with ore, the doodads and wells put down) into a new vector every
step only to find it equal to the last. The key is now read against the
last one as it goes and written out only when it changed: the same answer,
no allocation. (Half of the rest is the look zeroing its arrays; other
code may write into them between looks, so they stay zeroed.)

| in turns, load 11 | before | after |
|---|---:|---:|
| an empty region's step (8 regions, 20,000 steps) | 0.415-0.445 µs | 0.261-0.271 µs |
| `worldbench 30` (one pilot, all 9 regions): mean | 0.007 ms | 0.005-0.006 ms |
| p50 | 0.003 ms | 0.001 ms |

The state hashes of `worldbench` (start, nest) and the wire and state
hashes of `crowdbench` (1, 4, 16 pilots) are equal. After round 1 this
world's step was 0.030-0.034 ms: the rounds since took it to 0.005.

### Round 9: the start's fog tiers on every core (2026-10-18)

After round 2 the world's start was `loadWorld` (about 300 ms, the walking
grids already spread over the cores) and `WorldMap` (126-133 ms on one
core): each region's `Vision` samples the terrain's height at every cell's
centre for its tiers (a cell's level), 55,000 cells a region. Each cell is
its own, as in `NavGrid::buildGround`, which already gives each thread its
own rows; `Vision` now does the same.

| `loadbench 3`, 3 runs each in turns, load 10 | before | after |
|---|---:|---:|
| first `WorldMap`, wall | 125.2-133.4 ms | 20.9-21.7 ms |
| its CPU (all threads) | 125-133 ms | 156-157 ms |
| first `loadWorld`, wall | 271-279 ms | 281-326 ms (the same work; load) |

The tiers are the same (the vision goldens, every core test; `worldbench`
and `crowdbench` hashes equal). The server's start is now about 300 ms of
which 280 are the walking grids' ground.

Tried and left out: two exact changes to the path search, the goal test's
root replaced by the largest square whose root is still within reach, and
the search's per-cell state cut from 12 to 8 bytes (where a cell came
from kept apart). The same paths (`pathbench`'s hash), and no time back
beyond the noise (10.5-12.9 µs against 11.0-13.0 at load 26).

The Makefile rebuilt `build/bench/SoakSave.o` on no header change (its
dependency file was not included), so since round 1 changed `WorldRegion`
a stale one made `soak-save` crash; it is included now.

The short soak after this round (`soak.py run --minutes 6 --every 60`, a
fresh save in `unreal/server/build/soak-r9`, load about 10): resident
memory flat at 21.3 MB, CPU 2.8 s at the start and 0.03 s a minute after
it, the WAL 62 KB a minute, the idle call 0.18-0.35 ms mean and 0.74-3.7 ms
at most (Q3's soak: 0.06-0.55 and up to 4.5).
