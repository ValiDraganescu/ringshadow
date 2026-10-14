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
- One worker at a time.

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
- Added by the manager: after the opening chain, KSTR points at the next
  goal (L1), because a pilot at level 10 asked "now what?".

## Tasks

Run in this order, one worker at a time.

| # | Task | Needs | Status |
|---|---|---|---|
| B1 | Bugs: mining sparks stop when drilling stops; the Prospector moves smoothly (no lag) when its AI drives it and the pilot isn't | | done 0a42c5a |
| R1 | Realm buildings: every building belongs to the world; any pilot unloads, respawns, lands and is defended by any building; limits per realm; protocol and save migrate; old saves load | | done 1b6611c |
| C1 | Citadel tax and realm wallet: a share of every unload (start 10%) goes to the realm wallet, a donate action, the realm wallet shown at the Citadel and in the HUD | R1 | done e4611e7 |
| C2 | Citadel shop: consumables (repair kit, MH canister, shield charge, scanner pulse; first set decided in the task) and every blueprint, from the pilot's wallet, prices rising per blueprint level and per copy bought; blueprints stack and are used up by building; a shop screen | C1 | |
| N1 | Bug design and models: docs/creatures.md (kinds: a ground swarmer, a flyer, the brood queen; hp, speed, damage, attraction to POIs, linger, aggro, mining, leaving), models through the model pipeline, sounds offline | | |
| N2 | Bugs in the world: server spawns at POIs, linger, attack pilots in their site and buildings, mine unprotected veins and leave; snapshots; client draws, animates, sounds; buildings take damage and can fall | N1, R1 | |
| D1 | Citadel weapons: the Citadel fires at bugs in range; realm-wallet upgrades for fire rate, damage and range | N2, C1 | |
| D2 | Defence buildings: AA tower (Sentinel model, air only) and the gatling gun (new model, amalgam rounds, ground and close air), blueprints, prices, build menu | N2, C2 | |
| N3 | The brood queen: a giant in a region other than Prospect Landing, a nest with many bugs, her own attack and sound, a reward for killing her | N2 | |
| W4 | Seamless regions: region borders follow the land (not rectangles), visible passages between regions, crossing on foot without a seam; folds in W3's ground palettes and landmarks | | |
| F2 | The transport ship: the pilot can't fly alone; build a landing pad, board the Dropship, lift, turn, speed away, seamless approach and landing at a pad in a discovered region; flight menu shows only reachable pads | R1, W4 | |
| L1 | What next after the opening: KSTR goal contracts by level (Hab Dome, Foundry hunt, the shop, the first defences) | C2, D2 | |
| U1 | HUD declutter (from M7): one toast lane, Q/C/G clear of the pawn, compact contract panel, banners off the panels, no "CONTRACT DONE: CONTRACT" | | |

Each task is done when the three suites pass, its hidden-run pictures
(or a headless flow) show the behaviour, and its commit is merged on
`worktree/MMORPG-variant`.

Done when: in a hidden run a pilot at Prospect Landing pays the Citadel's
tax, buys a blueprint and a consumable, builds a gatling gun that kills
bugs drawn to a deposit, walks through a passage into the next region
without a seam, and rides the Dropship from one built pad to another; all
suites pass.
