# MMORPG milestone 7: the world worth crossing

Goal: every minute in the world has a reason and a sight: regions with
character, contracts that give each session a purpose, and world events
under the giant that pilots talk about. Log: [mmorpg-log.md](mmorpg-log.md).

Runs alongside the end of M6 (P4b–P7): workers freed from M6 start here.

## Decided (2026-10-14, manager)

- R1's blocker "nothing to do after minute 5" is answered by three loops:
  the skill tree (M6), contracts from the KSTR radio (K1), and world events
  on the server's clock (E1, E2).
- The signature event is the ringshadow itself: the giant's rings throw a
  shadow band that sweeps across the land on the world clock. The game is
  named after it, so it must be the most striking thing a pilot sees.
- Events and contracts are server-owned and the same for every pilot in a
  world; the client only draws and announces them.
- Regions get character before more regions: The First Reach first.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| W1 | The First Reach with character: deposits in veins and clusters that follow the land (not lines), a landmark silhouette per region, places worth walking to; validate ok, previews and hidden-run pictures | P4a | done |
| E1 | The ringshadow sweep: a band of shadow crosses the region on the world clock (server schedule in snapshots, client sky/light/fog); under it ore veins flare and drilling pays more; an announcement before it comes | C1 | done |
| E2 | Starfall: the server drops meteors at times and places it picks; a streak in the sky, a crater with a rare rich deposit, first come first served, an announcement | C1 | done; polish with S1 |
| K1 | KSTR contracts: the radio offers 3 contracts a day per pilot (deliver ore/MH, reach a site, build, survive an event); rewards XP and MH; a contract panel and a radio voice line | P3 | server done; panel with P5 |
| F1 | Flight worth watching: the pad-to-pad flight and jump jets burn MH, with thrust, dust and a camera that sells it | P3 | done (pad flight); jump jets and sound in F1b |
| G1 | Rig screen (J / RIG tab): fitting, Foundry, Lab with prices, countdowns, refusals; module icons | P7 | done |
| G2 | Client screens: KSTR contract panel, respawn countdown, Hab Dome depot, own pads and Ringwalker in the flight menu, worldKinds build menu; P5 follow-ups | K1, P6 | done |
| J1 | Sound for the new moments: ringshadow approach and pass, starfall streak and impact, crater hum, level-up, blueprint found, contract done, module made; offline WAV checks only | E1, E2, P5 | done |
| W2 | Blueprint sites you can see: a model per site kind (crate, wreck, cache, vault) at every site, opened state, beacon at distance; the contract panel compact by default | P4b, G2 | in progress |
| H1 | The home screen worth opening: key art of the Prospector under the giant and its rings (no war art), the RINGSHADOW title, the MMO entry first, a living background | R1 | in progress |
| S1 | Fast picture runs: a dev-only clock jump mid-run (no waiting for night), a starfall lead of ~8–10 s, `-AcMmoShotsOnly=` to shoot chosen moments; Starfall set under 1 min with the same 9 pictures (user-approved, via the build-train session) | E2 | done |

Done when: in a hidden run a pilot starts at Prospect Landing, takes a
contract, sees a ringshadow sweep and a starfall, and the pictures make a
reader want to play; all suites pass.
