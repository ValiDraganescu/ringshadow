# MMORPG milestone 8: the realm under siege

Goal: the Prospector is no longer alone and no longer safe. Buildings belong
to the realm, the Citadel taxes, sells and defends, alien bugs swarm the
places worth being, and the ship carries the pilot between regions that join
without seams. Log: [mmorpg-log.md](mmorpg-log.md).

Comes from the user's remarks after playing to level 10 (2026-10-14).

## Decided (2026-10-14, the user)

- Buildings belong to the realm, not the pilot: every pilot in the world
  uses every building (depots, respawn, pads, defences).
- The Citadel takes a tax on every unload into a realm wallet; pilots may
  donate too. The realm wallet buys Citadel upgrades, its weapons included.
- The Citadel has a shop: consumable items, and every blueprint, bought from
  the pilot's own wallet at a rising price. A blueprint is used up by one
  building; they stack.
- Enemies: alien bugs drawn to points of interest (sites, deposits,
  buildings, pads). They linger, attack a pilot inside their site and attack
  buildings; they mine unprotected veins, then leave and vanish. One or many.
- A giant brood queen lives in one region (not Prospect Landing) with many
  bugs around her.
- The Citadel's weapons fire at bugs on their own; the pilot builds an AA
  tower (the Sentinel model) and a new gatling gun firing red-hot
  ore-and-MH amalgam rounds, at ground targets and air at close range.
  The Prospector's own weapon stays weak: it builds, it doesn't fight well.
- Regions stop looking like rectangles; the passage between two regions is
  visible and seamless on foot.
- The Prospector can no longer fly on its own. A transport ship (the
  Dropship model) carries it: board at a landing pad, lift into the sky,
  turn, speed away, then a seamless approach and landing at the target.
  Only to regions already discovered, and only between landing pads the
  pilot has built: one in the region it leaves, one in the region it goes to.
- One worker at a time (later: two are fine).

## Decided (2026-10-14, the user, second round)

- Missions at the Citadel: the pilot picks from a board, easier ones with
  smaller rewards, harder ones with bigger. A picked mission goes into a
  Journal: its text, instructions, its place on the map, and a narration to
  replay. A voice narrates each mission, adventurous; grave and fearless, or
  fearful, by the mission's kind.
- The map (M) becomes a satellite view of the real ground, whatever the
  regions' shapes; hovering a POI, building or deposit shows a tooltip with
  its name and what is worth knowing (ore left, who built it).
- Mercenary prospectors hired at the Citadel mine an area slowly: in 24
  hours what the pilot mines in one.
- The world goes on while the game is closed: the server keeps running as
  long as the computer is on.

## Decided (2026-10-14, the user, third round: canyon defence)

- Some zones are canyons: the bugs come in waves along defined paths to
  reach a field of ore or MH, to mine it for themselves. Pilots build AA
  towers and gatling guns along the paths (tower defence), and turrets
  have upgrades.
- The first wave replays until the pilots beat it, with turrets or units;
  beating it starts the zone's game, waves growing harder. If the pilots
  fail to defend, the brood queen comes, destroys every turret in the zone,
  and it starts over from the first wave.

## Decided (2026-10-14, manager)

- The "Prospector only, no AI units" focus ends here: bugs are the first
  AI the MMO gets. Pilot ownership stays for the pilot's wallet, XP and
  skills.
- Blueprint sites stay: opening one gives a free blueprint (one use), so the
  hunt still pays.
- Landing pads replace the free pad-to-pad flight, the Ringwalker capstone
  and the Spacedock's private pad; the Ringwalker node gets a new effect
  (decided in F2).
- W3 (ground palettes, landmarks) folds into W4; X1 (shots split) waits:
  with one worker at a time the shots file no longer conflicts.
- Citadel missions are the Citadel's board; KSTR's daily radio contracts
  stay. Narration is generated offline per mission template (no key in the
  game) and shipped as sound assets. Mercenaries are the pilot's: their ore
  goes to its wallet, taxed like any unload. When the server was stopped
  anyway, it catches up the mercenaries' work on the next start.
- Canyon defence: a canyon zone is part of a region file (entrances, the
  bugs' paths, the prize field, build spots along the rims). First zone:
  one canyon in a level-2 region; more follow when it plays well. A wave
  fails when its bugs carry off a set share of the prize; the queen who
  comes is the Matriarch (N3's model and attacks), and she leaves when the
  turrets are down. Each cleared wave pays every pilot who fought in it;
  clearing the last wave holds the canyon for a world day (the prize field
  is the pilots' to mine, no waves). Turret upgrades are paid from the
  pilot's wallet at the turret; the Citadel's own guns from the realm
  wallet (D1).
- Added by the manager: after the opening chain, KSTR points at the next
  goal (L1), because a pilot at level 10 asked "now what?".

## Tasks

Two workers at a time (the user, 2026-10-14), on files that don't overlap.

| # | Task | Needs | Status |
|---|---|---|---|
| B1 | Bugs: mining sparks stop when drilling stops; the Prospector moves smoothly (no lag) when its AI drives it and the pilot isn't | | done 0a42c5a |
| R1 | Realm buildings: every building belongs to the world; any pilot unloads, respawns, lands and is defended by any building; limits per realm; protocol and save migrate; old saves load | | done 1b6611c |
| C1 | Citadel tax and realm wallet: a share of every unload (start 10%) goes to the realm wallet, a donate action, the realm wallet shown at the Citadel and in the HUD | R1 | done e4611e7 |
| C2 | Citadel shop: consumables (repair kit, MH canister, shield charge, scanner pulse; first set decided in the task) and every blueprint, from the pilot's wallet, prices rising per blueprint level and per copy bought; blueprints stack and are used up by building; a shop screen | C1 | done f26ab88 |
| N1 | Bug design and models: docs/creatures.md (kinds: a ground swarmer, a flyer, the brood queen; hp, speed, damage, attraction to POIs, linger, aggro, mining, leaving), models through the model pipeline, sounds offline | | done 0fb7047 (blockout models) |
| N1b | The Brood's models refined toward the concept art (the Matriarch's sac and crown, opal quills, the Lodeback's glass sac), painted materials | N2 | |
| N2 | Bugs in the world: server spawns at POIs, linger, attack pilots in their site and buildings, mine unprotected veins and leave; snapshots; client draws, animates, sounds; buildings take damage and can fall | N1, R1 | done 591206b |
| D1 | Citadel weapons: the Citadel fires at bugs in range; realm-wallet upgrades for fire rate, damage and range | N2, C1 | |
| D2 | Defence buildings: AA tower (Sentinel model, air only) and the gatling gun (new model, amalgam rounds, ground and close air), blueprints, prices, build menu | N2, C2 | |
| N3 | The brood queen: a giant in a region other than Prospect Landing, a nest with many bugs, her own attack and sound, a reward for killing her | N2 | |
| D3 | Turret upgrades: per AA tower and gatling, marks for fire rate, damage and range (and the gatling's air reach), paid at the turret from the pilot's wallet; shown on the turret | D2 | |
| Z1 | The first canyon zone: a canyon cut into a level-2 region (walls, rims to build on, 2–3 entrances, paths that meet at a prize field of rich ore and MH); region file support for zones, paths and build spots; validate and preview | N2 | |
| Z2 | The canyon game: waves along the paths to the prize, wave 1 replays until beaten, then escalating waves; a failed wave brings the Matriarch, who fells every turret in the zone and resets it to wave 1; rewards per wave, the held canyon after the last wave; wave HUD, warnings, narration lines | Z1, D2, N3 | |
| W4 | Seamless regions: region borders follow the land (not rectangles), visible passages between regions, crossing on foot without a seam; folds in W3's ground palettes and landmarks | | |
| F2 | The transport ship: the pilot can't fly alone; build a landing pad, board the Dropship, lift, turn, speed away, seamless approach and landing at a pad in a discovered region; flight menu shows only reachable pads | R1, W4 | |
| L1 | What next after the opening: KSTR goal contracts by level (Hab Dome, Foundry hunt, the shop, the first defences) | C2, D2 | |
| K2 | Citadel missions and the Journal: a MISSIONS tab on the Citadel screen with tiers (easy, hard, perilous) and rewards to match; picked missions in a Journal screen (text, instructions, map marker, replayable narration); narrated lines per mission template, voices by mood, offline WAV checks only | C2 | done 6683b5d |
| M2 | The satellite map: M shows a top-down render of the real ground of every discovered region, stitched, any shape; hover tooltips on POIs, buildings, deposits (name, ore left, builder) | | |
| S2 | The world runs on: the local server keeps running after the game quits (as long as the Mac is on), the game reattaches to it on start; offline catch-up when it was stopped; tests never leave a server behind | | |
| H2 | Mercenary prospectors: hire at the Citadel (a MERCS tab) for ore and MH; each mines an area at 1/24 of the pilot's rate, delivers to the pilot's wallet (taxed), shown on the map; work goes on while the game is closed | S2, C2 | |
| U1 | HUD declutter (from M7): one toast lane, Q/C/G clear of the pawn, compact contract panel, banners off the panels, no "CONTRACT DONE: CONTRACT" | | |

Each task is done when the three suites pass, its hidden-run pictures
(or a headless flow) show the behaviour, and its commit is merged on
`worktree/MMORPG-variant`.

Done when: in a hidden run a pilot at Prospect Landing pays the Citadel's
tax, buys a blueprint and a consumable, builds a gatling gun that kills
bugs drawn to a deposit, walks through a passage into the next region
without a seam, and rides the Dropship from one built pad to another; all
suites pass.
