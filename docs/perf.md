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
