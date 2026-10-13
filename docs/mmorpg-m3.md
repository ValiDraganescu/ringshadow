# MMORPG milestone 3: a world that lasts

Goal: quit and come back to the same world. The server saves the world and
the pilot; the pilot's Prospector keeps its levels and picks; level-up
cards work in the MMO mode. Still Prospector only, no AI units.
Log: [mmorpg-log.md](mmorpg-log.md). Design: [networking.md](networking.md) "Saves".

## Decided (2026-10-13)

- The server owns the save: SQLite, one file per world. Locally `~/Library/Application Support/Autocraft/Unreal/mmo/<world>.sqlite`.
- Saved: per region the pilots' buildings (progress, hp), units (the Prospector: position, hp, cargo), deposits and wells (what is left); per pilot: id, wallet, region, driven unit, levels and picks per kind.
- Written every 5 s (what changed) and on stop; a crash loses at most 5 s.
- Versioned schema with migrations from v1 (persistence means migrations).
- Pilot identity before accounts: the game keeps a random pilot id in its save folder and sends it after connecting; an unknown id is a new pilot at the start pad. Accounts replace it later.
- Leveling as in today's game, per kind (only the Prospector here), kept for good: XP on the server, the card pick is a request.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| S1 | Server save: SQLite store, schema v1 + migrations, save regions and pilots, every 5 s + on stop, load at start, `--save` | | running |
| S2 | Pilot identity: the pilot id message, restore a known pilot where they left, new pilot at the start pad | S1 | running |
| S3 | Leveling online: XP on the server, levels in snapshots, Pick request, cards in the MMO HUD; saved with the pilot | S1 | running |
| S4 | Game: pass `--save` and the pilot id; home screen "MMORPG (TEST)" resumes, "MMORPG NEW WORLD (TEST)" starts over | S2 | todo |

Done when: in the game, build a Citadel, mine, level the Prospector and pick a card, cross into a neighbour; quit; start again → same region and spot, same Citadel, wallet, level and picks. Killing the server mid-play loses at most 5 s. All suites pass.
