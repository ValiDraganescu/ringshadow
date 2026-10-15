# MMORPG milestone 9: a living frontier

Goal: the world feels inhabited and every evening has a reason to log in:
other pilots at work around you, more canyons to hold, a soundtrack for the
siege, numbers that hold together, and a build a player could run.
Log: [mmorpg-log.md](mmorpg-log.md).

Follows M8 (the realm under siege), which closes with Q2 and S3.

## Decided (2026-10-15, manager)

- A world with one human pilot reads as empty. Company comes from frontier
  pilots the server runs: named, with their own callsigns, levels and habits,
  mining, building realm buildings, taking missions, fighting bugs, calling
  on the KSTR radio, and turning up for canyon waves. They follow the same
  rules as a pilot (no cheats) and use the realm's buildings like anyone.
- More canyons, each with its own twist, in level-2 and level-3 regions.
- The siege gets its own sound: music layers for waves and the Matriarch,
  radio lines for the goals, the Dropship's flight.
- A balance pass checks M8's numbers (tax, prices, wages, rewards, wave pay)
  against the pace model with isolated tests, never a whole game.
- A local packaged Mac build to measure what the editor can't (the first
  Dropship call, frame times); building it is not a release: publishing
  stays the user's call.
- Canyons have no build spots (user, 2026-10-15): the pilot builds anywhere in
  the zone and packs the defence as tight or loose as they like; the Brood
  routes round what it can and chews through what blocks it.
- Two workers at a time, on files that don't overlap.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| P1 | Frontier pilots: 4–6 server-run pilots per world with callsigns, levels and a habit each (miner, builder, defender, hauler), living by the pilot's rules; seen on the map, in toasts ("Pilot Vesper built a Hab Dome"), on the KSTR radio; they join canyon waves near them; saved | M8 | done ddf0508 |
| Z3 | Two more canyons: one in a level-2 region, one in a level-3 region, each with a twist (a three-arm junction, a bridge over the prize, a flooded floor that slows, a Gloamwing roost); validated, previewed, map rebaked | Z2 | done e09d8be |
| A2 | Siege sound: wave and Matriarch music layers over the radio (ducked like narration), radio voice for KSTR goals and canyon news, Dropship flight sounds; offline checks only | Z2, F2, L1 | done 7f68374 |
| B2 | Balance pass: M8's economy against the pace model (tax, shop prices, mercenary wages and output, mission and wave pay, turret marks), tests that pin the curves, a table in docs | M8 | done 4b73be0 |
| Z4 | Free placement in canyons (user, 2026-10-15): no build spots; a pilot places any building anywhere standable in the zone, packed as close or far as they like, under the ordinary build rules (footprint, reach, price); the Brood walks round what it can and attacks what blocks its path; the three canyons' region files, validate, preview, the map, the DEFENCE tab and the canyon docs updated; shot sets rerun | Z3 | done 07909c1 |
| R2 | Realm projects: the realm wallet's sink. At the Citadel's REALM tab any human starts a project when the wallet covers it: Beacon (Dropship fares halved realm-wide), Relay (night sight widened in the realm), Armoury (every turret fights one mark up), Dock Crew (mercenaries mine at 1/12); each lasts one world day, costs 2–4 hours of one pilot's tax in ore and MH, rising per repeat; a realm-wide toast and a KSTR line on start and end; frontier pilots never start one; saved and sent; balance test pinned | B2 | done a8e74a6 |
| K3 | A local packaged Mac build (not a release): first Dropship call, frame times in the canyon and at the nest, startup time; fixes for what only shows packaged | Q2 | done a8a2507 |

Done when: in a hidden run, frontier pilots mine and build around a
pilot, one of them joins a wave at The Fork, a second canyon plays its
waves, the siege music rises with a wave, and the balance tests and the
three suites pass.
