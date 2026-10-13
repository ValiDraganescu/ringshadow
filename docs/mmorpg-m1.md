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
| T1 | `AutocraftNet` transport + lag simulator + tests | | todo |
| T2 | Server world: headless region on `ac::Simulation`, one pilot's input, per-pilot snapshot data (fog, near first), no network | | todo |
| T3 | Map how the game layer reads `ac::Simulation` (what a client mirror must stand in for) | | todo |
| T4 | Protocol: messages, bit-packed snapshots with deltas; server program wiring T1+T2: port on stdout, parent watch, log | T1, T2 | todo |
| T5 | Game client mode: launch the server, connect, mirror, draw, send input | T3, T4 | todo |
| T6 | Prediction + reconciliation of the driven machine; lag setting in the game | T5 | todo |

Done when: in the game, the MMORPG mode drives a unit on the local server
at 150 ms lag and 5% loss with no visible jitter; net-tests, server tests
and core-tests pass.
