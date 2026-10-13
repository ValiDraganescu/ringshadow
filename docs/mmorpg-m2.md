# MMORPG milestone 2: the world map and regions

Goal: something playable with only the Prospector. A world of several
regions on the local server; the pilot drives a Prospector from a pad,
mines, drives across region edges, flies between pads, and builds wherever
a building fits, paying its price. No other unit kinds, no AI-controlled
units. Design: [mmorpg.md](mmorpg.md) "The
world". Log: [mmorpg-log.md](mmorpg-log.md).

## Decided (2026-10-13)

- Regions generated (style + seed) or crafted by AI agents with an agent tool.
- Travel: across edges to the neighbour, and pad-to-pad flights.
- No lots: build wherever it fits; the pilot pays the price.
- Cities: proposal only (mmorpg.md "Cities").

## Confirmed (2026-10-13)

- P1 No AI-controlled units, no AI armies; wild ground only. AI pilots later.
- P0 Prospector only: no other unit kinds; buildings train no other units.
- P2 Pilot wallet: ore and MH; starts 400 ore; grows when the Prospector unloads at the pilot's own Citadel.
- P3 A new pilot starts at the start region's pad with one Prospector.
- P4 First world: 3×3 grid, Medium regions; the center one crafted (the start region), the rest generated.
- P5 The server steps every region every tick (the world goes on); measured, not assumed.

## Conventions

- Edge openings: every region opens the middle of each edge, from 40% to 60% of its length, unless the world file says otherwise; neighbours' openings must overlap.
- Every region has at least one pad. The first world's 9 regions are region files in unreal/server/regions/; 8 are base + pad + openings, the start region is crafted.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| R1 | Region file + agent crafting tool (validate, generate, top-down preview PNG) + agent skill | | done |
| R2 | Server world: world file, many regions, spawn at pad, edge hand-off, pad flights, region change in the protocol | T6, R1 | done |
| R3 | Client: region change (rebuild the mirror), world map screen, flight at a pad | R2 | done |
| R4 | Build anywhere: pilot build request in the MMO mode, wallet, server placement check, price | R2 | done |
| R5 | The start region crafted by an agent with R1's tool | R1 | done |

Done when: in the game the pilot starts at the pad in a Prospector, builds a
Citadel paid from the wallet, mines ore into the wallet, drives into a
neighbour region and flies back; no other units anywhere; all test suites
pass.
