# Frontier pilots

A world with one human pilot reads as empty. The server runs **frontier
pilots**: four to six pilots of its own in every world, each with a callsign,
a paint and a habit. They mine, build realm buildings, take Citadel
missions, fight bugs and call on the KSTR radio, by the same rules as a human
pilot. Milestone 9, task P1 ([mmorpg-m9.md](mmorpg-m9.md)).

Code: `unreal/server/world/WorldFrontier.cpp` (numbers in `WorldFrontier`,
WorldMap.h). Tests: `unreal/server/tests/FrontierTests.cpp`,
`unreal/net-tests/tests/FrontierWireTests.cpp`, `atlas_frontier_pilots`.

## Who they are

| # | Habit | What it does |
|---|---|---|
| 1, 5 | Miner | Drills and hauls. Puts a Derrick on the well by its field and pumps a tank every fourth trip. Builds a Hab Dome by a field more than 24 cells from any depot. |
| 2, 6 | Builder | Builds realm buildings where they help: a Gatling Gun by a field the Brood lingered at in the last 15 world minutes (when no turret guards it), a Landing Pad in a region with a Citadel or a canyon and no pad, a Hab Dome where the haul is long. Otherwise it drills to earn its keep. |
| 3 | Defender | Answers THE HUNT first (N4): to its place on the ring round the Matriarch's nest, walking through the openings or flying the Dropship when both regions have a Landing Pad and the fare leaves 10 MH; it holds there, fighting the bugs that come at it, and goes in with the party at the charge. Joins a canyon wave whenever a human is in that canyon's region. Fights bugs within 16 cells of the realm's buildings, mends a building below 90% hp, and patrols from one realm building to the next, a minute each. Every other minute it drills. |
| 4 | Wanderer | Opens the region's sites it has not opened (a vault only at its floor). At the realm's Citadel it takes a survey of a region next door. Otherwise it moves on to a region it has not seen, on foot through the openings or by Dropship when both regions have a Landing Pad and the fare leaves 10 MH. |

- **Pilots per world:** `--frontier N`, 0 to 6, default 5. `0` turns them off.
- **Callsigns:** drawn from Vesper, Kestrel, Corvid, Tamsin, Brindle, Sorrel, Moraine, Talus, Lumen, Calder, Drover and Larkspur, in the order the world's dice give.
- **Paints:** green, gold, violet, teal and pink, from the team palette. Never the pilots' blue, the red, or the mercenaries' orange.
- **Tag:** the HUD shows "VESPER · LV 14" over each frontier Prospector.

## The rules they keep

A frontier pilot follows the same rules as a human pilot:

- **Wallet and level.** It has its own wallet, starting at a new pilot's 600 ore plus 40 MH. It earns XP at the world's rates (`WorldXp`: 1 an ore, 2 an MH, the building feats, the sites' blueprint XP, kills by `Brood::xp`, a defence's 50% share). It levels on the same curve.
- **Skills.** On each new level it spends its points down its habit's branch in the tree's order: Deep vein, Torchwright, Hardshell or Wayfinder. When that branch is full it spends them anywhere.
- **Blueprints.** It buys them at the realm's Citadel, standing where it unloads, at the shop's price (`ProspectorProgress::blueprintPrice`). The next copy costs more. It buys a blueprint only when the wallet holds the building's price too, plus 50 ore and 10 MH. Sites give it blueprint copies the way they do for any pilot.
- **Buildings.** It builds with a blueprint copy at the blueprint's floor, under the realm's limit (`buildRefusal`). It pays the world's price (`Progression::worldOre`, `worldHydrogen`), off any pad's plate, on ground the core allows, within reach of its Prospector. The building is the realm's; its builder is the frontier pilot.
- **Tax.** The Citadel's tax (`Progression::realmTax`) comes off every unload into the realm wallet, as on any pilot's.
- **Dropship.** It flies from a realm Landing Pad it stands on to one in the target region, at the fare (`Flight::price`).
- **Canyon pay.** A canyon wave pays it its share by damage, from the same split as the humans' (Z2).
- **No skill boosts.** Its Prospector works at an unskilled Prospector's rates: the core gives skills only to the driven unit. Its points still show in its level.

## Caps

These keep the realm from being overbuilt and leave its limits to the humans:

| What | Cap |
|---|---|
| Hab Domes of theirs | 1 in the world |
| Derricks | 2 |
| Gatling Guns | 2, only by a raided field with no realm turret within 12 cells |
| Landing Pads | 1 per region |
| All their buildings | 8 in the world |
| Any kind | only while the realm has two or more under its limit: one is always left for a human |
| Spacing | none within 6 cells (plus both radii) of another realm building |

They never build a Citadel, Foundry, Lab, Spacedock, Bastion or AA tower.

## When they come, and the opening chain

The roster forms once a human pilot (on line or not) has finished the
opening chain (O1, the first ten minutes).

- **Arrival.** They come by Dropship 45 world seconds later, one every 40 s, each told to every human ("Vesper landed at …") with a hello on the radio.
- **Where they land.** Each lands at a realm Landing Pad in the realm Citadel's region, or else at the region's pad. A wanderer picks a region by the dice.
- **The chain's places.** While a human on line is on the chain, nothing of theirs comes within 14 cells of its places in the start region:
  - the start pad;
  - the chain's two sites;
  - the deposit nearest the pad;
  - the well nearest the Derrick's wreck.

  They don't land there, drill there, open those sites or build there.

## Live or booked

- **Live.** Where a human can see them (a human in the region or next to it), their Prospectors are realm units in the simulation. The server steers them with the core's own orders:
  - to a deposit, home with a load, into a Derrick, mending;
  - an errand to a point, a hunt.

  A load hauled to a Citadel leaves the region's bank and goes into the frontier pilot's own wallet, as a mercenary's does. A Hab Dome unload is booked by the server, since the core's Prospector hauls only to Citadels.
- **Booked.** Everywhere else their work runs on the world's clock, also with nobody on line (S2's idle):
  - a walk is a straight line at a Prospector's speed;
  - drilling gives `Rules::carry` ore over the drill time and a 12-cell walk each way (0.41 ore a second), taken from a real deposit and taxed;
  - a building rises as its welding would.
- **Server downtime.** The server's down time is not caught up for them (unlike the mercenaries).
- **When a region runs dry (E1).** Regions hold one or two veins now ([regions.md](regions.md), "Scarcity"), so they run dry. Frontier pilots and mercenaries mine the same veins by the same rules as a human, and none of them is given ore of its own:
  - a frontier miner takes the nearest mound with ore that has a depot; with none left in its region it walks to the realm Citadel's region ("to the Citadel's fields"), and with none there either it idles ("no field with a depot") until a mound grows back (a tenth of its ore a world day);
  - a mercenary stays at its field: mined out, it brings its load home and stands, unpaid (no ore owed), until the field grows back, then takes what has grown (booked or live). It is never moved to another field by itself; its pilot hires it again for another;
  - starfall deposits are a human's to find: neither drills them unless one stands in its field.
- **Falling.** A Prospector that falls comes back at the depot nearest where it fell, after `WorldParams::respawnDelay`, and loses its load.

## What the humans see

| Where | What |
|---|---|
| In the world | Their Prospectors in their paints, outlined, with a tag "VESPER · LV 14" |
| Satellite map (M) | A dot in each one's paint. The tooltip shows "VESPER · LV 14", "Frontier pilot · Miner" and "Mining in Rust Hollow". Pilots on their way don't show. |
| Toasts, "ON THE FRONTIER" | Notable deeds: "Vesper built a Hab Dome in Rust Hollow", "Kestrel opened the Fork wreck in Cold Fork", "Corvid finished a Citadel survey of Glass Flats". A frontier building's "built" news is this deed, not the plain one. |
| Standings (S4, the REALM tab and the map) | Their week beside the humans': the ore they unloaded (booked or live, before the tax), the bugs their Prospector and their turrets felled, the canyon waves they were paid a share of, the buildings they finished. Booked mining runs at 1,489 ore an hour at most ([balance.md](balance.md)), so a frontier miner hauls at most about 6,900 ore a week, less where the veins have run dry (E1); a driven pilot at 8,334 an hour passes it in an hour. See mmorpg-play.md "Standings". |
| KSTR radio, text toasts "KSTR · VESPER" | A hello on landing, a raid warning when the Brood's raid is told, a cheer when a canyon wave is beaten, a cry for help when one is badly hurt or falls, chatter at most every 10 world minutes. At most one line every 45 s, except the hello and the cheer. |

**Lines.** All written for the game:

- "{name}: swarm moving on {where}. Guns up, pilots."
- "Wave down at {where}. {name} owes somebody a drink."

## Wire and save

- **Protocol 28.**
  - `Sky::frontier` carries, for each pilot that has come: its region (none while on its way), its place in region cells, its unit (0 when booked), its level and what it is doing.
  - The Progress message's `frontier` is the roster: callsign, habit, paint.
  - `NewsKind::frontier`: "radio", "deed", "come", with the line in `by` and the pilot's id in `value`.
- **Schema 21.**
  - The `frontier` table holds one row of JSON, the whole roster: progress, wallet, tax, region, place, load, building, mission, counts.
  - Their Prospectors are saved as units under their keys (`frontier:vesper`).
  - An old save has none. They come after the opening chain, as in a new world, and a save whose pilot finished the chain gets them on its first step.

## Dev lines and pictures

- **Dev lines.** The server's `frontier …` dev lines (`--dev-control`):
  - `frontier here HABIT [SIDE]`: one of that habit beside the first pilot;
  - `frontier build KIND`: the nearest one puts up a KIND, its blueprint and price given, welded to its last second;
  - `frontier radio KIND`;
  - `frontier level N`;
  - `frontier list`.
- **Pictures.** `-AcMmoShotsP1` shoots:
  - a frontier miner drilling near the human;
  - a builder's Hab Dome and its deed toast;
  - a radio line;
  - the map with their markers and a tooltip;
  - a defender fighting beside the human at The Fork.
