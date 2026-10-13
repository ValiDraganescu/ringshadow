# MMORPG milestone 1: drive through the local server

Goal: drive one machine on one region through the local server, under the
lag simulator, and have it feel like today's game. Design:
[networking.md](networking.md). Log: [mmorpg-log.md](mmorpg-log.md).

## Scope

In: the network library, the server program (Mac), one region = today's
generated map with its AI armies, one pilot driving one unit, snapshots
with deltas and interest, the game as a client, prediction of the driven
machine, the lag simulator.

Out (later milestones): shots and lag compensation, encryption, saves,
Windows and Linux servers, accounts, AI pilots, lots, more regions.

## Layout

| Path | What |
|---|---|
| `unreal/Source/AutocraftNet/` | Network library, engine-free C++20; an Unreal module and a clang build. Sockets, packets, acks, channels, handshake, bit writer, lag simulator, the protocol's messages. |
| `unreal/net-tests/` | Its tests, plain clang, like `core-tests`. |
| `unreal/server/` | The server program and its world code (engine-free: `AutocraftCore` + `AutocraftNet`), its Makefile and tests. |

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| T1 | `AutocraftNet` transport + lag simulator + tests | | running |
| T2 | Server world: headless region on `ac::Simulation`, one pilot's input, per-pilot snapshot data (fog, near first), no network | | running |
| T3 | Map how the game layer reads `ac::Simulation` (what a client mirror must stand in for) | | done |
| T4 | Protocol: messages, bit-packed snapshots with deltas; server program wiring T1+T2: port on stdout, parent watch, log | T1, T2 | todo |
| T5 | Game client mode: launch the server, connect, mirror, draw, send input | T3, T4 | todo |
| T6 | Prediction + reconciliation of the driven machine; lag setting in the game | T5 | todo |

Done when: in the game, the MMORPG mode drives a unit on the local server
at 150 ms lag and 5% loss with no visible jitter; net-tests, server tests
and core-tests pass.

## T3 findings: what the game layer reads (for T4, T5)

Game reads via `UAcSimSubsystem`: `Simulation()`, `State()`, `Shown()` (fogged copy, `sim.shown(0)`), `Map()`, `Issue()`, `LastFrame()` (`FAcFrame` events, gated by `Gate()` with sight).

- Works on a const state + `MapDefinition`: renderer, life bars, minimap, rays, doodads, terrain (built from `Map()`), lamps, effects (given events), HUD, RTS pawn, pointer.
- Needs Simulation queries on state: `weapon`, `has`, `maxHP`, `boost`, `cargoSlots`, `slotsUsed`, `bastionCapacity`, `cone`, `strikeDamage`, `levels`, `pilotSight`, `pilotTarget`, `pilotAbility`, `pilotRepair`, `placementRefusal`/`snap` (nav grid + field).
- Needs fog: `vision` grid, `sights[P].seen/.ids`, `intel`, `unseen()`, `sees()`.
- Mutations → become requests: `Issue` (card, command map, Pick), `take` (AcPilotPawn.cpp:404,455,507), `pilotBuild`, `lookNow`, game flow (`SetFog`, `NewGame`, `Restart`: disable in this mode).
- Pilot input: `AAcPilotPawn::Steer` (AcPilotPawn.cpp:697-736) writes `Simulation().pilot` each frame; `act`/`ability` on key press (:541,:553); crosshair measured client-side by `CastCrosshair` (:754-794).
- Dev-only state writers (Playground, PilotStage, ShotScenes, Bench): not in this mode.

Plan from it: the client mirror is an unstepped `ac::Simulation` built from the same map; snapshots overwrite its state (units, structures, patches, players' upgrades/levels, pilot); the server also sends the pilot's fog bits. Prediction steps only the driven unit.
