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
| E1 | The ringshadow sweep: a band of shadow crosses the region on the world clock (server schedule in snapshots, client sky/light/fog); under it ore veins flare and drilling pays more; an announcement before it comes | C1 | in progress |
| E2 | Starfall: the server drops meteors at times and places it picks; a streak in the sky, a crater with a rare rich deposit, first come first served, an announcement | C1 | in progress |
| K1 | KSTR contracts: the radio offers 3 contracts a day per pilot (deliver ore/MH, reach a site, build, survive an event); rewards XP and MH; a contract panel and a radio voice line | P3 | server done; panel with P5 |
| F1 | Flight worth watching: the pad-to-pad flight and jump jets burn MH, with thrust, dust and a camera that sells it | P3 | |
| S1 | Fast picture runs: a dev-only clock jump mid-run (no waiting for night), a starfall lead of ~8–10 s, `-AcMmoShotsOnly=` to shoot chosen moments; Starfall set under 1 min with the same 9 pictures (user-approved, via the build-train session) | E2 | |

Done when: in a hidden run a pilot starts at Prospect Landing, takes a
contract, sees a ringshadow sweep and a starfall, and the pictures make a
reader want to play; all suites pass.
