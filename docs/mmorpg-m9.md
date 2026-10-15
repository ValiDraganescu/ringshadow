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
- Two workers at a time, on files that don't overlap.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| P1 | Frontier pilots: 4–6 server-run pilots per world with callsigns, levels and a habit each (miner, builder, defender, hauler), living by the pilot's rules; seen on the map, in toasts ("Pilot Vesper built a Hab Dome"), on the KSTR radio; they join canyon waves near them; saved | M8 | |
| Z3 | Two more canyons: one in a level-2 region, one in a level-3 region, each with a twist (a three-arm junction, a bridge over the prize, a flooded floor that slows, a Gloamwing roost); validated, previewed, map rebaked | Z2 | |
| A2 | Siege sound: wave and Matriarch music layers over the radio (ducked like narration), radio voice for KSTR goals and canyon news, Dropship flight sounds; offline checks only | Z2, F2, L1 | |
| B2 | Balance pass: M8's economy against the pace model (tax, shop prices, mercenary wages and output, mission and wave pay, turret marks), tests that pin the curves, a table in docs | M8 | |
| K3 | A local packaged Mac build (not a release): first Dropship call, frame times in the canyon and at the nest, startup time; fixes for what only shows packaged | Q2 | |

Done when: in a hidden run, frontier pilots mine and build around a
pilot, one of them joins a wave at The Fork, a second canyon plays its
waves, the siege music rises with a wave, and the balance tests and the
three suites pass.
