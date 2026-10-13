# MMORPG milestone 2: the world map and regions

Goal: a world of several regions on the local server. The pilot starts at
a pad, drives across region edges, flies between pads, and builds wherever
a building fits, paying its price. Design: [mmorpg.md](mmorpg.md) "The
world". Log: [mmorpg-log.md](mmorpg-log.md).

## Decided (2026-10-13)

- Regions generated (style + seed) or crafted by AI agents with an agent tool.
- Travel: across edges to the neighbour, and pad-to-pad flights.
- No lots: build wherever it fits; the pilot pays the price.
- Cities: proposal only (mmorpg.md "Cities").

## Proposed (confirm with the user)

- P1 Regions run without today's RTS AI armies; wild ground only. AI pilots later.
- P2 Pilot wallet: ore and MH; starts 400 ore; grows when the Prospector unloads at the pilot's own Citadel.
- P3 A new pilot starts at the start region's pad with one Prospector.
- P4 First world: 3×3 grid, Medium regions; the center one crafted (the start region), the rest generated.
- P5 The server steps every region every tick (the world goes on); measured, not assumed.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| R1 | Region file + agent crafting tool (validate, generate, top-down preview PNG) + agent skill | | running |
| R2 | Server world: world file, many regions, spawn at pad, edge hand-off, pad flights, region change in the protocol | T6, R1 | todo |
| R3 | Client: region change (rebuild the mirror), world map screen, flight at a pad | R2 | todo |
| R4 | Build anywhere: pilot build request in the MMO mode, wallet, server placement check, price | R2 | todo |
| R5 | The start region crafted by an agent with R1's tool | R1 | todo |

Done when: in the game the pilot starts at the pad, drives into a
neighbour region, flies back, and builds a Citadel paid from the wallet;
all test suites pass.
