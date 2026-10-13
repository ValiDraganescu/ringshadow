# The server's world

The world half of the MMORPG mode's server ([docs/networking.md](../../docs/networking.md),
[docs/mmorpg-m1.md](../../docs/mmorpg-m1.md), task T2). Engine-free: `AutocraftCore` only,
no network yet.

| Path | What |
|---|---|
| `world/World.h` | `Region`: one `ac::Simulation` on a generated map with its AIs, stepped at 60 Hz; pilots join, take a unit, send inputs; `snapshot()` gives each pilot what its side may know. |
| `world/Region.cpp` | The tick, the pilots and their inputs, the fog gate on events (`mayKnow`). |
| `world/Interest.cpp` | The snapshot: fog first, rank (driven, engaged, nearby), distance bands, the budget with aging. |
| `tests/` | Isolated scenes, `make -C unreal/server test` (under a second). |
| `bench/RegionBench.cpp` | Step and snapshot times, `make -C unreal/server bench`. |

## Inputs

A client sends one `PilotInput` per tick it runs, stamped with its tick. The region keeps them in
tick order, drops repeats and old ones, and uses one each server tick. With none waiting, the last
one holds (walk, heading, held keys); presses (`act`, `ability`) count once. The snapshot says which
input was used last (`lastInputTick`), for the client's replay.

## Snapshots

Entities are the core's own `ac::Unit` and `ac::Structure`, so a client writes them back into the
`GameState` of an unstepped `ac::Simulation` on the same map. An enemy unit's plans (goal, path,
errand, build order) are cleared. Enemy buildings the side remembers come as `remembered`. Beside
them: the players (the side's whole, the others' upgrades and team), patches and wells in sight,
the side's fog (`seen`, `near`, `explored`, site times), the ids gone since, and the gated events.

Core limits for M1: `Simulation` has one `pilot`, and `Pilot::player` is a constant, so every
pilot is on side 0 and one drives at a time.
