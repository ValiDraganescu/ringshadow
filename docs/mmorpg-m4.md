# MMORPG milestone 4: hand check by the manager, and polish

The user is away (2026-10-13) and asked the manager to do all the testing.
Goal: every MMO moment checked in pictures from hidden runs, what looks or
plays wrong fixed, and the known polish items done. Still Prospector only,
no AI units. Log: [mmorpg-log.md](mmorpg-log.md).

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| Q1 | Shot harness: `-AcMmoShots=DIR` stages the MMO moments and saves a PNG of each (HUD on); the agent reviews every picture as a player and lists the problems | | done |
| Q2 | Fix what Q1 found, re-shoot, compare | Q1 | running |
| Q3 | Flight: a lift-off and landing the player sees (the Prospector rises off the pad and comes down on the other), no frozen view | | running |
| Q4 | World map by keyboard (arrows move the pad pick, Return flies); MMO HUD shows only what applies | | done |

Moments for Q1: spawn on the pad (3rd person), top-down over the start
region, the world map, the build menu and ghost, a Citadel going up, a
Citadel finished, drilling ore, unloading at the Citadel, the level-up
cards, the edge crossing, landing in another region, after a resume.

Done when: every moment's picture reads right to a player; all suites and
the MMO flow and automation tests pass.
