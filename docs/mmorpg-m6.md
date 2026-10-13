# MMORPG milestone 6: a pilot's long road, under a living sky

Goal: the Prospector has a progression worth a hundred hours (100 levels,
a skill tree, buildings found in the world), and the world runs its own
day, night and weather for every pilot. Log: [mmorpg-log.md](mmorpg-log.md).

## The week (2026-10-13 to 2026-10-20)

The user handed the manager the mode for a week: a beautiful, complete
game, unique, addictive and jaw-dropping. The arc, one milestone after
another, adjusted as results come in:

- **M6** (this): the progression, the world's clock and weather, and a
  review of the first hour.
- **M7 The world worth crossing**: blueprint sites, rare finds, world
  events under the giant (eclipses, meteor falls, storm fronts), more
  regions with character.
- **M8 Company**: AI pilots who live in the world, and cities forming
  where buildings stand close.
- **M9 The first hour and the feel**: onboarding, the juice of every
  action, sound and music, the menus, a packaged build.

## Decided (2026-10-13)

- User: points + gates tree, MH respec, 100 levels in ~100 h, blueprints
  found in the world (see the log).
- Manager: the server owns the world's clock; the client's sky and the
  server's sight follow it. Weather comes from the server too.
- Manager: a review of the first hour in pictures turns into fix tasks.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| P0 | Design: docs/prospector-progression.md (XP curve, five branches, actives, respec, blueprints, buildings' MMO jobs, migration, plan) | | in progress |
| C1 | World clock and weather from the server: one clock per world, saved; snapshots carry the hour and weather; the client's sky, lamps, fog and rain follow; a dev flag to hold the hour (server side) | | in progress |
| R1 | First-hour review: a new world from the home screen to a Derrick pumping, in pictures; a ranked list of what bores, confuses or looks wrong | | in progress |
| P1… | Implementation tasks from P0's plan | P0 | |

Done when: a new pilot levels through a tree that changes how they play,
finds a blueprint in the world and builds it; night falls for the server's
sight and the client's sky at the same moment; all suites pass; the
pictures read right.
