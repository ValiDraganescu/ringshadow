# The server

The MMORPG mode's server ([docs/networking.md](../../docs/networking.md),
[docs/mmorpg-m1.md](../../docs/mmorpg-m1.md), [docs/mmorpg-m2.md](../../docs/mmorpg-m2.md)): a world
of regions ([docs/worlds.md](../../docs/worlds.md)), or one generated region, behind our protocol on
UDP. Engine-free: `AutocraftCore` and `AutocraftNet`.

| Path | What |
|---|---|
| `world/World.h` | `Region`: one `ac::Simulation` on a generated map with its AIs, stepped at 60 Hz; pilots join, take a unit, send inputs; `snapshot()` gives each pilot what its side may know. |
| `world/Region.cpp` | The tick, the pilots and their inputs, the fog gate on events (`mayKnow`). |
| `world/WorldMap.h` | `WorldMap`: the world file loaded and checked, every region stepped every tick, the pilots (start pad, wallet, levels), edge crossings and pad flights, building anywhere it fits with the driven Prospector, paid from the wallet (`Region::build`: the core's own hand-build); the M1 server is a `WorldMap` of one region. |
| `world/WorldStore.h` | The world's save: one SQLite file per world (system libsqlite3), schema v1 with a migration runner (`schema_version`; each step in its own transaction), writes of the changed rows only, the file held by one server. |
| `world/WorldMapSave.cpp` | Pilots by key (`WorldMap::seat`: a known key back where it left, an unknown one new at the start pad), parked off line, and the world saved and restored. |
| `world/Interest.cpp` | The snapshot: fog first, rank (driven, engaged, nearby), distance bands, the budget with aging. |
| `program/Server.h` | `Server`: the world behind `net::ServerEndpoint`. A connection is a pilot; each tick reads, steps every region, tells the pilots that moved (RegionChange, World), and every third tick sends each client its events and its snapshot. |
| `program/main.cpp` | `autocraft-server`, the program the game starts. |
| `client/HeadlessClient.h` | The tests' and the bench's client: the game's own `net::Client` (`AutocraftNet/Public/NetClient.h`), a mirror of the region, inputs and acks at 60 Hz, requests. |
| `region/` | `autocraft-region` (build/autocraft-region): region files ([docs/regions.md](../../docs/regions.md)) made, validated, built and drawn from above (`Preview.h`, PNG by `ThirdParty/stb_image_write.h`, public domain). |
| `regions/` | The region files. |
| `worlds/` | The world files (`first.json`: 3×3, the start region in the middle). |
| `tests/` | Isolated scenes, `make -C unreal/server test` (about 7 s: the end-to-end ones run in real time). |
| `bench/` | `make -C unreal/server bench`: the region's step and snapshot times, the protocol's bytes a second, the world's step (all regions). |

The protocol itself is in `AutocraftNet` (both ends need it): `NetProtocol.h` (the messages),
`NetSnapshot.h` (snapshots as deltas), `NetMirror.h` (the client's mirror), `NetClient.h` (the
client's end), `NetInterpolator.h` (others drawn in the past), `NetPredictor.h` (the driven machine
predicted, put right by each snapshot), `NetLocalServer.h` (the game starts
this program).

## The program

```
autocraft-server --world <file>
                 | --map <highlands|openField|ridge|badlands> --size <small|medium|large|huge> [--starts 2|4|8]
                 [--bind 127.0.0.1] [--port 0] [--parent-pid pid] [--log path] [--save file]
                 [--detached | --attached]
autocraft-server --stop <save | folder>
```

`--detached` (with `--save`; S2, [docs/mmorpg.md](../../docs/mmorpg.md) "The world runs on") writes
the run file beside the save (`first.sqlite`: `first.run`, `net::RunFile`) and removes it on stop;
the game starts it so, in a session of its own, and reattaches by the file. `--attached`, the
default, ends with `--parent-pid`'s process. `--stop` asks the server on that save (or every one with
a run file in the folder) to save and stop: exit 0 stopped, 1 none running, 3 still running after 15 s.
With no client on line a world idles: its clock and timers move on every 0.5 s, the regions wait, a
datagram wakes it. A save loaded catches the world up on the time since it was written (its
`saved_at`), at most 24 h.

`--world` runs a world ([docs/worlds.md](../../docs/worlds.md)); a bad world file stops it with
exit 2 and its problems on stderr. `--save` (a world only) keeps it in a SQLite file: loaded before
the first tick, the changed rows written every 5 s and everything on stop (SIGTERM, SIGINT, the
parent gone), so a `kill -9` loses at most 5 s. A file that is not a database, is held by another
server, has a newer schema or is another world's stops the server with exit 1, the file untouched,
after printing `FAILED save <why>` on its standard output (the game shows it, `net::LocalServer::error`).
Without `--save` nothing is kept.

A connection is seated by its Join (the pilot's 128-bit key, `net::Client::join`): no Welcome
before it. A known key comes back where it left (region, spot, its own Prospector, wallet,
levels); an unknown one is a new pilot at the start pad with 600 ore. The log names the key's
first 8 hex digits (`join pilot 3 id 1a2b3c4d: restored in cold-fork`). `--map` runs the M1 region: today's generated map with its AIs.

It reads the one-time token as the first line of its standard input, binds (port 0: the system
picks), prints exactly `PORT <n>` on its standard output, and ticks at 60 Hz, sleeping between
ticks. It stops on SIGTERM or SIGINT, or once the parent pid is gone (checked twice a second), and
tells its clients. The log (`~/Library/Logs/Autocraft/server.log` unless `--log`) has a line per
start, stop, connect, disconnect and refusal (a bad token, a take refused), and every 10 s a stats
line: clients, the tick's mean and max time, the regions' step (all of them) mean and max, bytes a
second out per client. In a world, a line per flight, refusal and region change.

## Inputs

A client sends one `PilotInput` per tick it runs, stamped with its tick, with the last three again
for loss. The region keeps them in tick order, drops repeats and old ones, and uses one each server
tick. With none waiting, the last one holds (walk, heading, held keys); presses (`act`, `ability`)
count once. The snapshot says which input was used last (`lastInputTick`), for the client's replay.
The walk goes in 255ths and the heading in 16 bits; the server applies exactly what arrives
(`net::quantised`).

## Snapshots

Entities are the core's own `ac::Unit` and `ac::Structure`, so a client writes them back into the
`GameState` of an unstepped `ac::Simulation` on the same map (`net::Mirror`). An enemy unit's plans
(goal, path, errand, build order) are cleared. Enemy buildings the side remembers come as
`remembered`. Beside them: the players (the side's whole, the others' upgrades and team), patches
and wells in sight, the side's fog (`seen`, `near`, `explored`, site times), the ids gone since,
and the gated events (Event messages).

On the wire every unit, building, player (core, a record per kind driven, a stamp per level),
patch, well, fog chunk (2,048 cells) and site time is an item with its own baseline: the last
state the client acked. Changed fields only, whole when there is no baseline, nothing when it is
unchanged and acked. Positions in centimetres (a step from the baseline in 12 bits a coordinate
when it is small), angles in 16 bits, hit points in 12 bits of twice the kind's own, times in
milliseconds. A snapshot fills what the byte budget has left after the reliable messages, the
driven unit and what it fights first; entities left out wait and gain priority (`Region::deferred`).

Core limits for M1: `Simulation` has one `pilot`, and `Pilot::player` is a constant, so every
pilot is on side 0 and one drives at a time, a region. In a world, pilots in one region share the
side's bank.
