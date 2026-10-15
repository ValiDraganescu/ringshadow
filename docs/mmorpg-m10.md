# MMORPG milestone 10: the long game

Goal: a second human on the world is seen and fought beside, and every
return has a reason: what happened while you were away, where you stand in
the realm, and a hunt the whole realm can call.
Log: [mmorpg-log.md](mmorpg-log.md).

Follows M9 (a living frontier), which closes with R2 and K3's leftovers.

## Decided (2026-10-15, manager)

- The mode is called an MMORPG and has only ever been played by one human
  at a time. Two game clients on one server, in a hidden run, is the proof
  that the realm's rules hold for two: tags, buildings, the map, toasts,
  flights, a wave fought together and its pay split.
- Coming back must feel like the world went on: an away report on the home
  screen and on entering, from the server's own bookkeeping (mercenaries,
  frontier deeds, raids, canyons, the Matriarch), never invented.
- Standings over a realm week (7 world days) give the frontier pilots and
  the human something to beat; they reset weekly with a KSTR line.
- The Matriarch becomes the realm's raid: a human calls the hunt at the
  Citadel, the frontier defenders and the mercenaries with a fighting role
  gather at her ring, KSTR narrates, her N3 death pay stands.
- The settings menu becomes real: volumes (music, voice, effects), window
  and resolution, the key card. A packaged first run must need nothing else.
- Soaks measure only the background server's idle path, which is what runs
  when the game is closed: never AIs playing a game.
- Two workers at a time, on files that don't overlap.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| W5 | Two humans: a hidden run with two game clients on one local server (two pilot keys, two save dirs for the clients, one world); each sees the other's Prospector, paint and tag, buildings with `built_by`, map dot and tooltip, deeds as toasts; a Dropship flight seen from the ground; The Fork's wave fought by both with the pay split; pictures from both clients; fix what breaks; a flow test `mmotwo` | P1, Z4 | core done 6a09229; clients in progress |
| X2 | Away report: on the home screen under ENTER THE REACH ("Your world ran 6 h 12 m: Slate delivered 1,240 ore, Sorrel built a Derrick in Cold Fork, The Fork held, a raid took the Hab Dome's roof") and as a Journal page on entering; from the server's bookkeeping since the pilot's last visit, saved per pilot; a KSTR welcome line | H2, P1, Z2 | |
| S4 | Realm standings: ore delivered, bugs felled, waves held, buildings raised, per pilot (humans and frontier) over the realm week; a STANDINGS list on the REALM tab and a line in the map tooltips; the weekly reset with a KSTR line ("This week's top hauler: Vesper, 18,400 ore"); saved | P1, R2 | |
| N4 | The hunt: at level 20 the goals lead to "bring her down"; a human calls THE HUNT on the REALM tab (free, once a world day); frontier defenders and fighting mercenaries gather at the nest's 30-cell ring over 2 min, then go in with the pilot; KSTR narrates the gathering, the roar and the fall; her N3 pay and her return in three world days stand; a flow test and pictures | N3, P1 | |
| U2 | Settings: music, voice and effects volumes (the radio's duck rules kept), window or full screen and resolution, the key card, the lag simulator's switch; saved in the game's config; the pause menu's SETTINGS opens it; a packaged first run checked from the home screen to the field and back with hidden_run --app | K3 | |
| Q3 | Idle soak: the background server alone for 2 h of wall clock at the 2 Hz idle tick with the frontier's booked work, mercenaries and canyons; memory, save size and tick time flat; the 24 h catch-up cap exercised; fixes | P1, K3 | |

Done when: in a hidden run two clients fight a wave at The Fork together
and both are paid; a pilot who comes back after the server ran alone reads
what happened; the standings show the week's hauler; a called hunt gathers
the frontier at the nest and the Matriarch falls; the settings hold across a
packaged restart; the soak's numbers are flat; the three suites pass.
