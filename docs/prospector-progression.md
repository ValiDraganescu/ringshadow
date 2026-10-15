# Prospector progression (MMORPG mode)

Status, 2026-10-13: a design, nothing built. It replaces the Prospector's
leveling in the MMORPG mode only. The strategy game's leveling
([leveling.md](leveling.md), `Leveling.cpp`) stays exactly as it is.

The focus is the Prospector alone: no other kinds, no AI pilots. The server
is the authority: XP, points, respecs and blueprints are the server's, and
the game only asks.

## Why

Today's MMO uses the strategy game's leveling: 10 levels, two cards a level.
Most cards help "nearby" or "every" Prospector, a team the MMO pilot doesn't
have, or things the MMO lacks (Hab Dome supply, unit costs). Ten levels are
gone in a few evenings. A lasting world needs a long climb, a real choice of
build, and something to go and find.

## Decided (the user, 2026-10-13)

1. **Points and gates.** Every level gives 1 skill point. Points go into five
   branches. Nodes have ranks. Deeper tiers, capstones and active abilities
   open at fixed levels. Builds must diverge: two level-60 Prospectors play
   differently.
2. **Respec** is allowed and costs MH. The first one is free; then the price
   rises with level.
3. **Pace:** level 10 in the first hour, about level 50 near 20 hours, level
   100 in about 100 hours, the last levels slow.
4. **Buildings come from blueprints found in the world** (crafted sites in
   regions, rare finds), each with a level floor. Not unlocked by level alone,
   and not bought.

## The XP curve

XP from level L to L + 1:

    xp(L → L+1) = 500 + 3 × L²

XP in all to reach level L:

    total(L) = 500 × (L − 1) + (L − 1) × L × (2L − 1) / 2

The cap is level 100. XP keeps adding up past it, with no effect.

### The pace model

XP per hour of play at level L:

    rate(L) = 4,500 × (1 + 0.02 × (L − 1))

- A level-1 driven Prospector that only mines earns 100 XP a minute,
  6,000 an hour ([leveling.md](leveling.md), "earning XP").
- An hour of play is not all work: walking to deposits, building, flying, the
  map. The model counts three quarters of it as work: 4,500 XP an hour at
  level 1.
- Skills, a Derrick and better routes raise the income about 2% a level: ×2
  at level 51, ×2.98 at level 100. This is the average build. An all-economy
  build levels faster, a Hardshell-heavy one slower. The model is an
  estimate: tune `500` and `3` once real hours are measured.

This curve is in **plain XP**: 1 XP an ore, as in the strategy game. The
world's ×0.1 (`Simulation::xpScale`, `kWorldXpScale`) goes back to 1, because
the curve now does the slowing (open question 1).

### The table

Hours are hours of play from a new pilot, work only, at `rate(L)`.

| Level | XP in all | XP to the next | Hours to reach | Minutes for the next level |
|---|---|---|---|---|
| 2 | 503 | 512 | 0.11 | 7 |
| 5 | 2,090 | 575 | 0.45 | 7 |
| 10 | 5,355 | 800 | 1.10 | 9 |
| 20 | 16,910 | 1,700 | 3.1 | 16 |
| 30 | 40,165 | 3,200 | 6.6 | 27 |
| 50 | 145,775 | 8,000 | 19.7 | 54 |
| 70 | 370,185 | 15,200 | 42.5 | 85 |
| 90 | 761,395 | 24,800 | 76.2 | 119 |
| 100 | 1,034,550 | | 97.3 | |

- **Level 10 in the first hour:** 1.10 h of work alone. The first hour's
  one-off XP (below: the Citadel and Derrick blueprints, the first buildings,
  the first MH and flight, a first neighbour region and its pad, about 850 XP)
  bring it under the hour: 0.91 h if they all came at the start.
- **Level 50 near 20 hours:** 19.7 h.
- **Level 100 in about 100 hours:** 97.3 h. Level 99 to 100 alone takes
  about 2 h 15 min (29,903 XP at 13,320 an hour).

Other levels the doc uses: level 3 at 0.22 h, 8 at 0.82 h, 15 at 1.95 h, 25
at 4.6 h, 35 at 9.1 h, 45 at 15.6 h, 55 at 24.4 h.

## XP sources

XP comes from work and from discovery. Nothing else earns XP: standing,
walking, flying, sitting in a building, the skill screen.

| Source | XP | Notes |
|---|---|---|
| Ore unloaded at your Citadel or a Hab Dome of yours | 1 an ore | As today. |
| MH unloaded | 2 an MH | Up from 1.25, so pumping earns near mining's rate: a 20 MH trip of about 35 s is 40 XP, about 70 a minute against mining's 100. |
| Welding a building of yours | ½ × its cost (ore + MH) × the share welded | A Citadel is 200 XP. Half of the strategy game's rate, since the ore it cost already earned XP when it was mined. |
| Repairing a building of yours | ¼ × its cost × the share of hp mended | Only hp lost to something else. Nothing damages buildings in the MMO yet, so this is 0 today. |
| Damage dealt | the target's cost × the share of hp taken, buildings × ½ | For no-man's land later; there is nothing to fight today. |
| First visit to a region | 250 | Once per region per pilot. The start region doesn't count. |
| First landing on a pad | 50 | Once per pad per pilot. The start pad doesn't count. |
| Finding a blueprint | 50 × its level floor | Derrick 150, Sentinel 500, Foundry 750. Once per blueprint. |
| A survey chip | 25 | The rare find while mining (see "Blueprint sites"). |
| Feats, once each | Citadel finished 150; first of each other building kind finished 100; first MH unloaded 50; first flight 50; first deposit emptied 50 | |

One-offs add up to about 16,000 XP in The First Reach: regions 2,000, pads
400, blueprints 9,350, feats 1,000, and about 3,000 in chips. That is under 2%
of the XP to level 100. The climb is work; the one-offs make the first hours
brisk and reward going places.

### Anti-farm

- **Once per ore.** Ore and MH earn XP only when unloaded, once. Cargo lost
  on a death earns none.
- **Your own paid buildings.** Welding XP only for a building the pilot paid
  for, within its building limit (see "Buildings in the MMO"). If demolishing comes, it
  refunds nothing and takes back the welding XP, so build-and-tear-down earns
  nothing.
- **Repairs** earn only for hp lost to someone or something else, and at most
  the building's welding XP per real day.
- **One-offs are once per pilot, ever.** They are saved by pilot id. A respec
  doesn't reset them.
- **Survey chips** are rolled on the server, at most one per 5 minutes.
- **No XP for what works alone.** MH a Derrick pumps by itself (Derrick
  tender) goes straight into the wallet, with no XP.
- **The server counts.** A client never sends XP or finds.
- A pilot who mines one deposit for hours is working; that is fine. The
  deposit runs dry and grows back after 300 s (`Rules::regrowDelay`).

## The skill tree

Five branches, 13 nodes each, 65 nodes in all. (F2 retired Homing beacon:
Wayfinder has 12 nodes and 34 points now, the tree 64 nodes and 182 points;
its tiers and capstone still open as below.)

### Points and gates

- Level L has L − 1 points: **99 points at level 100.**
- Every rank costs 1 point. A node's ranks are taken one at a time.
- A tier opens at a level **and** a number of points already in that branch:

| Tier | Opens at level | Points in the branch first |
|---|---|---|
| 1 | 2 | 0 |
| 2 | 10 | 5 |
| 3 | 25 | 12 |
| 4 | 45 | 20 |
| Capstone | 70 | 28 |

- Each branch has 36 points of nodes plus a capstone of 1 point: 37. The tree
  holds **185 points**. A full build takes 99 of them, **54%**.
- A capstone costs 29 points in its branch (28 to open it, 1 for it). At most
  three capstones fit (87 points, 12 left over); a build after capstones
  usually takes two (two whole branches, 74, and 25 elsewhere).
- Two capstones are possible from level 70 (69 points), three from level 88.

### What the effects are

The tree reuses the strategy game's building blocks ([leveling.md](leveling.md),
"What the picks are made of"): `Effect::Stat` on `Scope::Driven`, `EveryThird`,
`Burst`, `Shield`, `SelfMend`, `CliffJump`, `Grenade`. Percent changes add up
(`.percent`), loads and hp are `.plus`, and they come on top of today's hero
(×1.2 speed, 25 ore and 20 MH a trip, ×2 build and repair). Effects marked
**new** need a new block (listed in "How it fits the code").

### Deep Vein (the drill)

Theme: ore. Faster drilling, bigger loads, richer strikes.

| Tier | Node | Ranks | Per rank | At full | Block |
|---|---|---|---|---|---|
| 1 | Quick drill | 5 | drill +6% | +30% | `Stat::Drill` |
| 1 | Big haul | 3 | +3 ore a trip | 34 a trip | `Stat::OreCarry` |
| 1 | Clean cut | 3 | a load takes 5% less from the deposit | 15% less | new: deposit yield |
| 2 | Rich vein | 3 | every 5th / 4th / 3rd ore trip carries double | every 3rd | `EveryThird` as every-Nth |
| 2 | Core sampler | 3 | +1% chance a load turns up a survey chip | 3.5% (from 0.5%) | new: chip chance |
| 2 | **Overcharge** (active) | 1 | 10 s of drill +100%; cooldown 60 s | | `Burst` with drill |
| 3 | Deep bore | 3 | drill +8% | +24% | `Stat::Drill` |
| 3 | Ore sled | 3 | +5 ore a trip | +15 | `Stat::OreCarry` |
| 3 | Quick regrowth | 3 | deposits you empty grow back 15% sooner | 45% (300 s → 165 s) | new: regrow time |
| 4 | Motherlode | 5 | 2% chance a load carries triple | 10% | new: load chance |
| 4 | **Seismic charge** (active) | 1 | fills the cargo to full from a deposit within 3 cells; cooldown 120 s | | new |
| 4 | Dense pack | 3 | ore a trip +10% | +30% | `Stat::OreCarry` `.percent` |
| Cap | **Stardust heart** | 1 | ore unloaded counts +20% in the wallet (and its XP) | | new: unload yield |

Full Deep Vein: drill +54%, 25 × 1.3 + 9 + 15 = 56.5, 57 ore a trip (Dense pack on the
hero's 25, then the plus ranks, rounded).

### Quicksilver (the well)

Theme: MH. Faster pumping, bigger tanks, Derricks that tend themselves.

| Tier | Node | Ranks | Per rank | At full | Block |
|---|---|---|---|---|---|
| 1 | Rig master | 5 | pump +8% | +40% | `Stat::Hydrogen` |
| 1 | Big tank | 3 | +3 MH a trip | 29 a trip | `Stat::HydrogenCarry` |
| 1 | Cold seal | 3 | a pump takes 5% less from the well | 15% less | new: well yield |
| 2 | Derrick tender | 3 | your Derricks fill a 100 MH tank by themselves at 2 MH a minute; entering collects it straight into the wallet (no XP) | 6 MH a minute | new |
| 2 | Twin canister | 3 | every 5th / 4th / 3rd pump carries double | every 3rd | every-Nth on pumps |
| 2 | **Quickfill** (active) | 1 | the next pump in a Derrick is instant; cooldown 45 s | | new |
| 3 | Pressure valve | 3 | pump +10% | +30% | `Stat::Hydrogen` |
| 3 | Quicksilver stride | 3 | walk +5% while carrying MH | +15% | new: speed while carrying |
| 3 | Wellspring | 3 | a well under your Derrick grows back 5 MH a minute | 15 a minute | new |
| 4 | Mercury lungs | 5 | +4 MH a trip | +20 | `Stat::HydrogenCarry` |
| 4 | **Hydrogen boost** (active) | 1 | burns 10 MH (cargo first, then the wallet) for 6 s of walk +80%; cooldown 20 s | | `Burst` with an MH cost |
| 4 | Field pump | 3 | pump straight from a well without a Derrick, at 40% / 60% / 80% of a Derrick's rate | 80% | new |
| Cap | **Silver tide** | 1 | MH unloaded counts +20% in the wallet (and its XP) | | new: unload yield |

Full Quicksilver: pump +70%, 20 + 9 + 20 = 49 MH a trip.

### Torchwright (the torch)

Theme: building and repairing, and blueprints.

| Tier | Node | Ranks | Per rank | At full | Block |
|---|---|---|---|---|---|
| 1 | Fast welder | 5 | build +10% | +50% | `Stat::Build` |
| 1 | Patchwork | 3 | repair +15% | +45% | `Stat::Repair` |
| 1 | Prefab | 3 | buildings cost 4% less (ore and MH) | 12% less | `Stat::BuildingCost` |
| 2 | Reinforced | 3 | your buildings +10% hp | +30% | new: own buildings' hp |
| 2 | Long arm | 3 | weld and repair from 0.5 cells farther | +1.5 cells | new: work reach |
| 2 | **Arc surge** (active) | 1 | 8 s of build and repair +200%; cooldown 60 s | | `Burst` with build and repair |
| 3 | Site boss | 3 | build +10% | +30% | `Stat::Build` |
| 3 | Blueprint savant | 3 | blueprints open 2 levels below their floor | 6 levels below | new |
| 3 | Overbuild | 3 | +1 Hab Dome, +1 Derrick and +1 Sentinel allowed | +3 each | new: building limits |
| 4 | Hardened works | 5 | your buildings take 4% less damage | 20% less | new |
| 4 | **Emergency weld** (active) | 1 | a building of yours within 3 cells gets 30% of its hp back at once; cooldown 120 s | | new |
| 4 | Efficient plans | 3 | building MH costs 10% less | 30% less (42% with Prefab) | `Stat::BuildingCost`, MH only |
| Cap | **Head start** | 1 | a building you place starts 25% built | | new |

Full Torchwright: build +80% (on the hero's ×2: ×3.6).

### Wayfinder (the road)

Theme: moving, surveying, finding the sites.

| Tier | Node | Ranks | Per rank | At full | Block |
|---|---|---|---|---|---|
| 1 | Light frame | 5 | walk +5% | +25% | `Stat::Speed` |
| 1 | Surveyor | 3 | blueprint sites show on the HUD compass within 15 / 25 / 35 cells | 35 cells | new |
| 1 | Long sight | 3 | sight +10% | +30% | `Stat::Sight` |
| 2 | Dust runner | 3 | walk +5% while carrying nothing | +15% | new: speed while carrying |
| 2 | Scanner pulse | 3 | arriving in a region shows its sites on the world map: wrecks; then caches; then vaults | all three | new |
| 2 | **Cliff hop** | 1 | jump up and down cliffs (passive), 2 MH a jump | | `CliffJump` |
| 3 | Jump jets | 3 | jump reach +1 cell; at rank 3 a jump burns 1 MH | +3 | `Stat::JumpReach` |
| 3 | Pad link | 3 | call the Dropship from within 8 / 12 / 16 cells of a Landing Pad's centre, up from 4 | 16 | new |
| 3 | ~~Homing beacon~~ | | retired by F2 (the Dropship lands only on Landing Pads): old saves get its points back and a Landing Pad blueprint | | |
| 4 | Lucky strike | 5 | survey chips come 10% more often | +50% | new: chip chance |
| 4 | **Sprint** (active) | 1 | 6 s of walk +60%; cooldown 30 s | | `Burst` |
| 4 | Short hop | 3 | the Dropship's dash across the sky 20% shorter | 60% shorter | new: flight time |
| Cap | **Ringwalker** | 1 | flights cost no MH, from any Landing Pad, and the Dropship comes 30% faster (F2) | | new |

### Hardshell (the plate)

Theme: staying alive and the cutting torch as a weapon. It matters most
once no-man's land brings fights (open question 2).

| Tier | Node | Ranks | Per rank | At full | Block |
|---|---|---|---|---|---|
| 1 | Plating | 5 | +6 hp | +30 | `Stat::Hp` |
| 1 | Hard hat | 3 | +1 armor | +3 | `Stat::Armor` |
| 1 | Cutting torch | 3 | weapon damage +20% | +60% | `Stat::Damage` |
| 2 | Shield | 3 | a 10 / 20 / 30 hp shield; refills over 10 s after 7 s without a hit | 30 hp | `Shield` |
| 2 | Self mend | 3 | mends 1 / 2 / 3 hp a second after 6 s without a hit | 3 a second | `SelfMend` |
| 2 | **Pulse mine** (active) | 1 | a charge at the crosshair, 20 damage within 1.5 cells; cooldown 14 s | | `Grenade` |
| 3 | Long torch | 3 | weapon range +0.5 cells | +1.5 | `Stat::Range` |
| 3 | Bulwark | 3 | damage taken −5% | −15% | `Stat::DamageTaken` |
| 3 | Second wind | 3 | after a death, back 25% sooner and keep 25% of the cargo | 75% each | new |
| 4 | Rig armor | 5 | +10 hp | +50 | `Stat::Hp` |
| 4 | **Aegis** (active) | 1 | 6 s of damage taken −50%; cooldown 45 s | | `Burst` with damage taken |
| 4 | Torch rhythm | 3 | every 5th / 4th / 3rd strike does double damage | every 3rd | every-Nth on attacks |
| Cap | **Ironclad** | 1 | the shield gains +40 hp and refills in 5 s, not 10 | | `Shield` |

Full Hardshell: 45 + 30 + 50 = 125 hp, a 70 hp shield, +3 armor.

### Two level-60 Prospectors

59 points each:

- **The deep miner.** Deep Vein 32 (tiers 1–3 whole, Motherlode 5), Wayfinder
  20 (tiers 1–2 whole, Jump jets 2), Quicksilver 7 (Rig master 5, Big tank
  2). Lives at the ore lines, Overcharge on Q, cliff-hops to the rich
  deposits on the high ground.
- **The builder-surveyor.** Torchwright 30 (tiers 1–3 whole, Efficient plans
  3), Wayfinder 17 (tier 1 whole, Scanner pulse 3, Cliff hop, Dust runner 2),
  Hardshell 12 (tier 1 whole, Shield 1). Opens blueprints 6 levels early,
  spreads Hab Domes over the world (six with Overbuild), Arc surge on Q.

At level 100 a pilot might take Deep Vein and Quicksilver whole (74) and 25
in Wayfinder: two capstones.

## Actives

| Active | Branch, tier | Opens at level | Cooldown |
|---|---|---|---|
| Overcharge | Deep Vein, 2 | 10 | 60 s |
| Quickfill | Quicksilver, 2 | 10 | 45 s |
| Arc surge | Torchwright, 2 | 10 | 60 s |
| Pulse mine | Hardshell, 2 | 10 | 14 s |
| Seismic charge | Deep Vein, 4 | 45 | 120 s |
| Hydrogen boost | Quicksilver, 4 | 45 | 20 s (and 10 MH) |
| Emergency weld | Torchwright, 4 | 45 | 120 s |
| Sprint | Wayfinder, 4 | 45 | 30 s |
| Aegis | Hardshell, 4 | 45 | 45 s |

Nine actives, **three slots**: **Q** opens at level 10, **C** at level 25,
**G** at level 45. A learned active goes into a slot on the skill screen;
moving actives between slots is free at any time. R stays the repair key, E,
Space and click stay the work keys. The level-up cards' Z and X are unused in
the MMO mode once the cards go.

## Respec

- A respec takes back every point and empties the slots. One-offs, found
  blueprints and XP stay.
- **The first respec is free.** After it:

      price(L) = 0.4 × L² MH, rounded to 10

- It is made at your Citadel (standing in its unload range): proposal, open
  question 4.
- **Undo:** a point spent in the last 60 s can be taken back free, for a
  slip of the mouse.
- **A tree change gives everyone a free respec.** The server saves the
  catalogue's signature with each pilot; when it changes, the next respec is
  free.

| Level | Price |
|---|---|
| 10 | 40 MH |
| 20 | 160 MH |
| 30 | 360 MH |
| 34 | 460 MH |
| 50 | 1,000 MH |
| 70 | 1,960 MH |
| 100 | 4,000 MH |

For scale: a Derrick by the Citadel gives 20 MH a trip of about 35 s, about
2,000 MH an hour of pumping only, at level 1. A level-50 respec is about half
an hour at the well.

## Buildings in the MMO

In a one-pilot world a building serves the Prospector, not an army. Each
needs a copy of its blueprint (found at a site or bought at the Citadel's
shop, C2; building uses the copy up) and the pilot at or above its **level
floor**. A copy held below the floor is kept and shows "opens at level N".

| Building | What it does for the pilot | Floor | Limit | Cost (ore + MH) | Status |
|---|---|---|---|---|---|
| Citadel | Home: unload point, respawn point, where the respec is made | 1 | 1 | 400 | on the server (P6) |
| Derrick | Pumps MH on a well (as today) | 3 | 4 | 75 | as today |
| Gatling gun | A rotary cannon firing red-hot ore and MH amalgam rounds: ground targets in range 6, air within 4.5 (D2); marks from the pilot's wallet at the turret (U, D3; [creatures.md](creatures.md) "As built (D3)") | 5 | 6 | 125 + 25 | on the server (D2, D3) |
| Hab Dome | A field depot: unload ore and MH into the wallet away from home, and a respawn point | 8 | 3 | 150 + 50 | on the server (P6) |
| Foundry | Makes rig modules: drill head, cargo pod, plate, torch tip, survey lens ("Foundry and Lab") | 15 | 1 | 300 + 150 | on the server (P7) |
| Lab | Researches better module marks (Mk II, Mk III) for MH and time | 25 | 1 | 200 + 200 | on the server (P7), standing alone |
| Landing Pad | Where the Dropship sets down (F2): a walkable 3 by 3 deck; flights only between Landing Pads, in regions the pilot has been to. The limit is per region | 1 | 2 a region | 60 | on the server (F2) |
| Spacedock | The big pad (F2): a Landing Pad whose flights cost half the MH; later, routes to far worlds | 35 | 1 | 400 + 300 | on the server (P6, F2) |
| Bastion | A bunker: the Prospector climbs in, takes no damage, its torch fires from it at +2 range. For holding a claim in no-man's land | 45 | 2 | 250 + 100 | built (P6); its fight matters later |
| Sentinel | The AA tower: missile pods at the air only, range 7, outranging a Gloamwing's spit (D2); marks from the pilot's wallet at the turret (U, D3) | 10 | 4 | 150 + 100 | on the server (D2, D3) |
| Garrison | Trains other kinds in the strategy game. Held back: no blueprint in the world while the MMO is Prospector only | | | | held back |

Limits are the realm's, across the world (R1, below): the table's numbers
count every pilot's buildings together. Overbuild raises the Hab Dome,
Derrick and Sentinel limits. The Citadel and the Derrick stay ore only, so the
first loop costs no MH.

### The realm's buildings (R1)

Decided by the user (2026-10-14, docs/mmorpg-m8.md): the buildings a pilot
builds are not the pilot's, they are the realm's (the world's). Every pilot
in the world uses every building.

- **Owned by the world.** No pilot owns a building. Each keeps who built it
  (`WorldMap::builtBy_`, saved as `built_by`): the builder gets the
  building's feat and its build contract's credit, its skills that reach
  buildings (Reinforced, Head start, the Derrick and repair nodes) reach it,
  and the toasts name it. The welder earns the welding XP, as before.
- **Used by every pilot.** Any pilot unloads at any finished Citadel or Hab
  Dome, comes back at the nearest of them when it falls, respecs at the
  Citadel, fits modules at the Citadel or a Hab Dome, and starts Foundry and
  Lab jobs once the realm has one, and flies the Dropship between any of the
  realm's Landing Pads (F2). Later tasks build on this: the Citadel's tax
  into a realm wallet (C1), defences that guard the realm (D1, D2).
- **Limits per realm.** A build is refused once the realm has the table's
  number of that kind, whoever built them: one Citadel, four Derricks,
  three Hab Domes, one Foundry, one Lab, one Spacedock, two Bastions, four
  Sentinels (`WorldMap::realmCount`, `ProspectorProgress::buildRefusal`).
  **Overbuild still works**, for the pilot that has it: its ranks raise the
  limit its own builds are checked against (+1 Hab Dome, Derrick and
  Sentinel a rank), so a pilot with Overbuild 2 puts up a fifth and sixth
  Derrick where one without is refused at four. The extra buildings are the
  realm's like any other.
- **One Citadel a realm.** A new pilot in an empty world builds it (the
  opening chain's step 2, as O1 made it). A pilot new to a world that has
  its Citadel already gets step 2 as "The Realm's Citadel": a reach
  contract to it (`<region id>/Citadel`), naming who built it, done when its
  Prospector stands where it unloads there. If another pilot finishes the
  Citadel while a pilot's step 2 still says "build one", the step turns into
  the reach. A save from before with two pilots' Citadels keeps both
  standing; no third can be built.
- **Who sees what.** Every pilot sees the realm's buildings as friendly (one
  side, as before). Building limits in the build menu count the realm's
  ("REALM LIMIT"). A building finished anywhere in the world is told to every
  pilot ("HAB DOME BUILT", "Built by Pilot 045B · in the realm · every
  pilot's"). A building in sight names its builder ("Hab Dome · built by
  Pilot 045B", "built by you").
- **Callsigns.** A pilot has no name yet. Others see a callsign: "Pilot " and
  four hex digits of a hash of its key (`WorldMap::callsign`). The key
  never goes on the wire.

### The realm's tax (C1)

Decided by the user (2026-10-14, docs/mmorpg-m8.md): the Citadel takes a cut
of every unload into a realm wallet; pilots may donate too; the realm wallet
pays for the Citadel's upgrades (D1).

- **The tax.** Every load a pilot unloads into its wallet, ore or MH, at the
  realm's Citadel or at a Hab Dome (a depot is the realm's too), pays the
  realm `Progression::realmTaxPercent`, **10%**, one tunable constant. The
  wallet gets the rest; the realm wallet gets the cut.
- **The rounding rule.** The cut is 10% of the load to the nearest whole
  unit, halves rounding up, but **at least 1** and **never the whole load**
  (`Progression::realmTax`): a load of 1 pays nothing, 2 to 14 pay 1, 15 to
  24 pay 2, 25 ore pays 3, 20 MH pays 2, 100 pays 10. So the small loads a
  nearly empty deposit gives still pay something, and no load is taken whole.
  The cut is taken from the load after Stardust heart and Silver tide (they
  grow the load, the realm takes its share of the bigger one).
- **XP and contracts count the whole load.** The tax never slows levelling:
  the unload's XP, the "Stardust run" deliver contracts and the mined-ore
  tally all count the full load; only the wallet gets less. The tax is taken
  in the core (`Simulation::prospectorDeposit`, only for the driven
  Prospector in a world: the strategy game pays none), so the client's
  prediction agrees; the unload's event carries it
  (`GameEvent::Deposited::tax`, `hydrogen`), and the server banks it into
  the realm wallet from that event (`WorldMap::realmTaxes`), crediting the
  pilot that paid.
- **The realm wallet** is the world's: ore and MH, saved with the world
  (schema 9), told to every pilot. **Donations**: at the realm's Citadel,
  standing where it unloads, a pilot gives ore and MH from its own wallet
  (`WorldMap::donate`, `RequestMessage::Donate`). Refused away from the
  Citadel ("Donate at the realm's Citadel: stand where you unload"; a Hab
  Dome is a depot, not where one donates), for more than the pilot has ("Not
  enough ore: you have 40"), and for nothing.
- **Spending** is D1's: `WorldMap::spendRealm(cost)` takes ore and MH from
  the realm wallet, refusing (and taking nothing) when it is short.
- **Each pilot's tally**: what it has given in all, in tax and donated,
  saved with the pilot, shown on its Citadel screen.

On the wire (protocol 13, C1): `ProgressMessage::realmWallet`, `taxPercent`,
`taxed` and `donated` (ore and MH each); `RequestMessage::Donate`;
`GameEvent::Deposited` carries `tax` and `hydrogen`.

On screen:

- The unload toast says what reached the wallet and the realm's cut under
  it: "+22 ORE", "3 TO THE REALM". It is a toast in the one lane (U1);
  loads home while it is up add to it ("+66 ORE", "9 TO THE REALM").
- The resource bar, top right, shows the realm wallet small where the
  strategy game's supply count is: "REALM" over "1,240 · 56 MH".
- **The Citadel screen (H)**, at the realm's Citadel (`SAcCitadelScreen`): H
  is free in the MMO HUD (K the skills, J the rig, M the map, T the
  contracts, X abandon, B build, V the view, R the ability, Q C G the
  actives). Away from the Citadel H says "H opens the realm's Citadel: walk
  up to it, where you unload". The screen has the rig screen's chrome in the
  realm's gold: the title names the Citadel's builder; a **REALM** tab with
  the realm wallet (ore and MH, large), the tax ("TAX 10% of every unload,
  here and at the Hab Domes"), what this pilot gave (in tax, donated), and
  **DONATE**: the pilot's wallet and buttons for 100, 1,000 and all, of ore
  (keys 1 2 3) and of MH (4 5 6), dark when the wallet is short. The foot
  says what was given ("Given to the realm: 1,000 ore.") or why not. H or
  Esc closes it. The tabs are built to grow: C2's shop and D1's upgrades
  add theirs beside REALM (C2's SHOP is there now). The keys line, at the
  Citadel, says "H  at the Citadel: the realm's wallet, donate, the shop".

Tests: `core-tests/tests/ProgressionEffectsTests.cpp`
(`realmTaxRounding`, `realmTaxOnUnload`, the Hab Dome depot),
`server/tests/RealmWalletTests.cpp` (the tax at the Citadel and a Hab
Dome, small loads, XP and contracts on the whole load; donations in and out
of range and over the wallet; two pilots' taxes adding up; the wallet saved,
loaded and migrated). Pictures: `-AcMmoShotsC1`.

Hours to each floor at the model's pace: Derrick 13 min, Hab Dome 49 min,
Foundry 1.95 h, Lab 4.6 h, Spacedock 9.1 h, Bastion 15.6 h, Sentinel 24.4 h.

### The Citadel shop (C2)

Decided by the user (2026-10-14, docs/mmorpg-m8.md): "I can buy the items
from the citadel (items are consumables)." "I can unlock and buy any
blueprint from the citadel shop. There must be a price progression. A
blueprint can be used once so blueprints are stackable and consumable."

- **Blueprints are copies.** A pilot holds a stack of copies per building
  kind (`ProspectorProgress::blueprints`, kind → copies). Placing a building
  uses one up (`WorldMap::build`); with none the build is refused "No
  blueprint: buy one at the Citadel (H)". The level floor still holds: a
  copy bought or found below it waits ("opens at level N").
- **The shop** is the Citadel screen's **SHOP** tab (H, at the realm's
  Citadel, standing where it unloads; Tab switches REALM and SHOP). It is
  paid from the pilot's own wallet; the tax does not apply to purchases.
  Every building kind the MMO has is in it (the Garrison stays held back).
- **Sites still pay**: opening a site gives one copy of its blueprint, held
  already or not, with the find's XP (50 × its floor); a blueprint survey
  chip gives one copy too. The old "known already: 100 ore and 20 MH" rule
  is gone. The opening chain (O1) is unchanged: the Landing crate gives the
  Citadel copy that step 2 builds, the Lander wreck the Derrick copy step 5
  builds.

**The price** of the next copy of a blueprint grows with the building's
level floor F and with the copies n this pilot has bought of that kind
before (`Shop::blueprintPrice`, `ProspectorProgress::bought`):

    ore = (50 + 25 × F) × 1.5^n      MH = 5 × F × 1.5^n

each rounded to 10; the Citadel and the Derrick cost ore only, so the first
loop still costs no MH. Copies found at sites don't count as bought.

| Blueprint | Floor | 1st | 2nd | 3rd | 5th | 8th |
|---|---|---|---|---|---|---|
| Citadel | 1 | 80 | 110 | 170 | 380 | 1,280 |
| Derrick | 3 | 130 | 190 | 280 | 630 | 2,140 |
| Gatling gun | 5 | 180 + 30 | 260 + 40 | 390 + 60 | 890 + 130 | 2,990 + 430 |
| Hab Dome | 8 | 250 + 40 | 380 + 60 | 560 + 90 | 1,270 + 200 | 4,270 + 680 |
| Sentinel | 10 | 300 + 50 | 450 + 80 | 680 + 110 | 1,520 + 250 | 5,130 + 850 |
| Foundry | 15 | 430 + 80 | 640 + 110 | 960 + 170 | 2,150 + 380 | 7,260 + 1,280 |
| Lab | 25 | 680 + 130 | 1,010 + 190 | 1,520 + 280 | 3,420 + 630 | 11,530 + 2,140 |
| Spacedock | 35 | 930 + 180 | 1,390 + 260 | 2,080 + 390 | 4,680 + 890 | 15,800 + 2,990 |
| Bastion | 45 | 1,180 + 230 | 1,760 + 340 | 2,640 + 510 | 5,950 + 1,140 | 20,080 + 3,840 |

(ore + MH.) The first Hab Dome copy, 250 ore and 40 MH, with the dome's own
150 + 50, is a few minutes of mining at level 8, where it opens. Copies stay
a sink: the realm's limits cap what stands, but buildings fall once the bugs
come (N2), and every rebuild takes a dearer copy; by the fifth Hab Dome copy
the price is 1,270 + 200, by the eighth 4,270 + 680.

**The consumables** (`Shop::items`, `Item`), a first set of four, each held
in a stack and used from the field with a key (1-4 were free while driving;
the build menu takes the digits only while it is open):

| Key | Item | Price | Stack | Cooldown | Use |
|---|---|---|---|---|---|
| 1 | Repair kit | 120 ore + 10 MH | 5 | 30 s | mends half the Prospector's full hp at once; refused when not hurt |
| 2 | MH canister | 150 ore | 3 | 60 s | 25 MH into the wallet: fuel for jets (after the carried MH) and flights |
| 3 | Shield charge | 100 ore + 30 MH | 3 | 45 s | no damage taken for 8 s (`SkillClock::shield`): a Shardling bite (2) or a Gloamwing spit (3) does nothing while it runs |
| 4 | Survey flare | 80 ore + 15 MH | 5 | 20 s | every site within 40 cells goes on the pilot's map for good; for 60 s the minimap rings the 40 cells and draws the ore in them larger |

Server-authoritative (`unreal/server/world/WorldShop.cpp`): `WorldMap::buy`
and `WorldMap::use` check, pay and give; the cooldowns and a burning flare
are kept per pilot on the world's clock, not saved (a server restart readies
them); the shield runs on the region's clock in the unit's skill clock, and
crosses regions with it.

Refusals (the reply's text): "Buy at the realm's Citadel: stand where you
unload", "Not enough ore: 250, you have 100", "Not enough MH: 40, you have
10", "No Garrison blueprint in this world", "Repair kit: you hold 5, the
most"; using: "No repair kits: buy them at the Citadel (H)", "Shield charge
ready in 31 s", "Your Prospector is not hurt", "In flight", "Your Prospector
is down", "Drive your Prospector first".

On the wire (protocol 14, C2): the request's variant index takes **5 bits**
(4 before); `RequestMessage::Buy{blueprint | item}` is 14 and
`RequestMessage::Use{item}` 15; K2's `MissionAsk` took 16 (protocol 16),
so D1's request comes next, at 17.
`ProgressMessage::blueprints` carries copies (kind → count), and `bought`,
`items`, `itemReady` (world time), `shieldUntil` (world time) and `flare`
(region, centre, radius, until) are new. `net::Client::buyBlueprint`,
`buyItem`, `use`.

On screen:

- **SHOP** (`SAcCitadelScreen`, Tab from REALM): BLUEPRINTS, a row per
  building (keys 1-8): the copies held (×N), "opens at level N" below the
  floor, the next copy's price, BUY (dark when the wallet is short); and
  SUPPLIES: each consumable with its key, what it does, held of the stack,
  its price, BUY. The foot says "Bought: Hab Dome blueprint." or why not.
- **The build menu** (B) shows the copies by each building's name ("Hab
  Dome ×2"); with none, it is greyed "NO BLUEPRINT".
- **The HUD**: four small cells right of the Q/C/G slots, their key caps 1
  2 3 4, the count (×N), the cooldown's seconds, gold while a shield or a
  flare runs. The keys line: "1 2 3 4  supplies".
- The skill screen's BLUEPRINTS tab counts copies held ("×2 READY · B
  BUILDS IT", "NONE HELD") and points to the shop.

Tests: `core-tests/tests/ProgressionTests.cpp` (`blueprintPriceRises`,
`blueprintCopiesAndItems`, the old list read as copies),
`server/tests/ShopTests.cpp` (buying at rising prices, the refusals,
building uses a copy, the consumables' use, stacks and cooldowns, the stacks
saved), `server/tests/SitesTests.cpp` (a site gives a copy),
`server/tests/FirstTenTests.cpp` (the opening chain), the old-save tests
(the migration below), `net-tests/tests/ProtocolTests.cpp`. Pictures:
`-AcMmoShotsC2`.

### Citadel missions (K2)

The Citadel screen's third tab, **MISSIONS**, is a board of six offers,
two each at easy, hard and perilous, with rewards to match: XP, ore and MH,
and at hard and perilous a blueprint copy or supplies. The pilot signs up
for up to three at once and follows them in its **Journal (L)**: the
briefing, what to do, the place on the map (also a marker on the minimap
and the compass), the reward, and the narration to play again. The kinds
are haul, survey, salvage, hold under the ringshadow band, and hunt; hunts
are gated off until the Brood (N2). The whole design, the numbers, the
voices and the wire are in [missions.md](missions.md).

### Mercenaries (H2)

The user, 2026-10-14: "With enough resources I can hire mercenary
prospectors from the citadel, they mine the resources in the area but a
very slow rate (they mine in 24 hours what I mine in 1h). The game should
progress in background even if I quit the game." Built in H2
(docs/mmorpg-m8.md; `WorldMercs`, server/world/WorldMercs.cpp).

- **Hiring.** The Citadel screen's **MERCS** tab, at the realm's Citadel
  where the Prospector unloads. A pilot holds **1 mercenary from level 5, 2
  from 15, 3 from 30** (`WorldMercs::limit`). The fee comes from the pilot's
  wallet and doubles with each one held: **500 ore and 50 MH**, then 1,000
  and 100, then 2,000 and 200. Refused below level 5 ("Mercenaries sign on
  with pilots of level 5 and up"), at the limit ("You hold 1 mercenary, the
  most at level 12: 2 at level 15"), short of the fee, away from the
  Citadel, and with no field. **DISMISS** lets one go at any time, with no
  refund.
- **The wage.** At each world dawn (every world day, 40 minutes) each
  mercenary takes **5 + level / 5 MH** (`WorldMercs::wage`; 6 at level 5,
  11 at 30) from the pilot's wallet, in the order hired. One the wallet
  can't pay **leaves** (its unit with it), and the pilot is told
  ("MERCENARY LEFT: FLINT"). The first wage is due at the dawn after the
  hire.
- **The unit.** A Prospector of the realm, painted in the mercenaries'
  orange (palette slot 6) with a tag over it, "MERC · FLINT" (a name from
  `WorldMercs::names`, none twice for one pilot). It starts beside the
  realm's Citadel, walks to its field, drills, and hauls each load to the
  realm's Citadel in that region, as the strategy game's Prospector hauls
  (the core's `bases`: a Citadel; a Hab Dome is a depot for the pilot's own
  Prospector only). Bugs treat it as any Prospector: prey, and a guard
  against a Lodeback within 8 cells of it. Other pilots see it too.
- **Its field.** A region file's deposits within 8 cells of each other are
  one field (`WorldMap::fields`; starfall deposits are not). It starts at
  the field with ore nearest the realm's Citadel. On the tab the pilot picks
  another field (keys 1-8: every field with ore in a region with a realm
  Citadel) and **SEND**s it: in its own region it walks over; in another it
  is there at once, at that region's Citadel. Its field mined out, it hauls
  what it carries home and stands; the tab says so.
- **The rate.** In 24 hours of world time it delivers what its pilot mines
  in 1 hour at the pilot's level by the pace model: `rate(L)` ore a day,
  `WorldMercs::rate` = `WorldContracts::perMinute(L)` × 60 / 86,400 ore a
  second (202.5 ore an hour at level 5, 258.75 at 20, 558.75 at 100). The server keeps the ore it has worked
  (`owed`): the rate × the seconds it works (walking included, a fall's ten
  minutes and a mined-out field not). It drills until it has worked a load
  (**10 ore**, `WorldMercs::load`, at least `Rules::miningTime`), so the
  rate holds whatever the walk, and the deposit really loses that ore.
  Measured: 6 minutes at level 100, 50 ore delivered and 5.88 owed (55.875
  worked); a day at level 5, 4,374 delivered and 486 tax: 4,860, the
  pilot's hour.
- **Its ore** goes to the pilot's wallet, wherever the pilot is, on line or
  not, **taxed** like any unload (C1: 1 of each load of 10 to the realm
  wallet, counted in the pilot's tax tally). No XP, no contracts, no
  missions.
- **Off line and closed.** With the regions held (S2's idle, no pilot on
  line) its work is bookkeeping: the same rate, in loads of 10, from its
  field straight to the wallet, the wages at each dawn. A start after the
  server was down catches the time up (`WorldMap::offlineWork`, at most a
  day, S2's cap), dawn by dawn. Back on line the pilot is told: "WHILE YOU
  WERE AWAY: Mercs delivered 1,240 ore" (`NewsKind::merc` "away", the ore
  after the tax delivered while it was off line).
- **Downed.** Bugs can kill it while pilots are near (with no pilot near
  there are no bugs). It is back beside its Citadel at once, with its load,
  told ("MERCENARY DOWN"), and at work again after **10 world minutes**
  (`WorldMercs::downFor`). Nothing is lost.

On the wire (protocol 19, H2): `RequestMessage::Merc` (hire, dismiss,
assign; index 18, 17 being D1's); `ProgressMessage::mercs`, `mercFields`,
`mercLimit`, `mercFee`, `mercWage`, `mercRate`, `mercNextLevel`, and
`mercTags` (every mercenary's unit in the pilot's region, any pilot's, by
name); `NewsKind::merc` ("away", "left", "down").

Tests: `server/tests/MercsTests.cpp` (the rules; hire, limits, fees and
refusals; live work paced and taxed; a day the pilot's hour; wages and
leaving; downed and back; sent to a field; saved, loaded and caught up off
line; the v12 migration), `net-tests/tests/ProtocolTests.cpp`. Pictures:
`-AcMmoShotsH2`, then `-AcMmoShotsH2Away`.

### On the server (P6)

Code: `unreal/server/world/WorldRoles.cpp`, and the core where the rule is
shared (`Progression::worldOre` and `worldHydrogen`, `Simulation::buildCost`,
`prospectorDeposit`). Tests: `server/tests/RolesTests.cpp` and the last three
in `core-tests/tests/ProgressionEffectsTests.cpp`.

- **Prices.** A world's pilot pays the table's prices, less its skills
  (Prefab, Efficient plans). A build the wallet can't pay is refused: "Not
  enough ore", "Not enough MH". The strategy game's prerequisites don't
  apply in a world (a Bastion needs no Garrison): blueprints, floors and
  limits are the gates.
- **Respawn.** A fallen Prospector comes back after 10 s
  (`WorldParams::respawnDelay`) beside its home: the realm's finished
  Citadel or Hab Dome nearest where it fell (R1: any pilot's). One in the same region wins by
  distance; otherwise the one fewest grid steps away. A region another
  pilot drives is skipped. With no home it comes back at the world's start
  pad. Second wind cuts the wait (25%, 50%, 75%) and keeps that share of
  the cargo. A pilot who leaves while fallen comes back at home when it
  joins again.
- **Respec** only at the realm's Citadel, in its unload range (since
  P3). A Hab Dome is not enough.
- **Hab Dome depot.** Ore and MH walked up to any finished Hab Dome of the
  realm go into the wallet, with their XP and contract credit, as at the
  Citadel.
- **Landing Pads and the Spacedock** (F2). The realm's finished Landing
  Pads and its Spacedock are the Dropship's pads, named in their region
  "Landing Pad 1", "Landing Pad 2" (by building id) and "Spacedock"; they
  show in every pilot's World message with `building` set. The Dropship is
  called within Pad link's reach of a Landing Pad's centre, or within 2
  cells of the Spacedock's edge. A first landing on each pays 50 XP once.
- **Lab.** A world's Prospector builds one, standing alone and welded like
  any other building. The strategy game's Lab stays an add-on, and its
  build list is unchanged.
- **Bastion and Sentinel.** Only their build rules for now (blueprint,
  floor, limit, price). The bunker and the turret matter once the world
  has fights.

The Wayfinder's travel nodes:

- **Pad link:** the Dropship is called from within 8, 12 or 16 cells of a
  Landing Pad's centre.
- **Homing beacon:** retired by F2. Its place stays in the skill table with
  no ranks (`Progression::retired`); a save that has it gets its points back
  (out of the slots too) and one Landing Pad blueprint, on load.
- **Short hop:** the Dropship's dash takes 20% less a rank (`ac::Ferry`).
- **Ringwalker:** flights cost no MH, from any Landing Pad, and every stage
  of the flight is 30% faster (F2).
- **Surveyor and Scanner pulse:** wired in P4b (`WorldSites.cpp`).

On the wire (protocol 8):

- `ProgressMessage::respawn`: when the fallen Prospector comes back (world
  time), in which region, beside what (a Citadel, a Hab Dome, or nil for
  the start pad), with what cargo. Nil while it stands.
- `WorldMessage::Pad::building`: the kind of a pad of the realm's (F2: a
  Landing Pad or the Spacedock). The message is sent again whenever these
  pads change. (Protocol 26 dropped `ProgressMessage::ringwalkerReady`.)

On the wire (protocol 12, R1):

- `ProgressMessage::realm`: the realm's buildings across the world by kind,
  finished or not (the build menu's limits).
- `ProgressMessage::builders`: who built each building of the pilot's region
  (id → callsign), and `callsign`, the pilot's own.
- `NewsKind::built`: a realm building finished, told to every pilot: `id`
  its kind, `by` its builder's callsign, `value` 1 when in the pilot's
  region.
- `ProgressMessage::foundry` and `lab`: the realm has a finished one.

### Foundry and Lab

The gear layer beside the tree (open question 3, answered yes): the rig gets
better in steps a pilot works toward, and MH gets a steady sink.

**Modules.** The Prospector has five slots, one module each. A module has
three marks; each mark is a clear step, and it adds to the tree's nodes
through the same blocks (`Modules::effects`):

| Module | What it does | Mk I | Mk II | Mk III | Block |
|---|---|---|---|---|---|
| Drill head | drill speed | +8% | +16% | +25% | `Stat::Drill` `.percent` |
| Cargo pod | ore and MH a trip | +8% | +16% | +25% | `Stat::OreCarry`, `Stat::HydrogenCarry` `.percent` |
| Plate | hp, a share of the Prospector's own | +8% | +16% | +25% | `Stat::Hp` `.plus` |
| Torch tip | torch damage, and range | +8%, +0.25 cells | +16%, +0.5 | +25%, +0.75 | `Stat::Damage` `.percent`, `Stat::Range` `.plus` |
| Survey lens | sight, and survey chips | +8% each | +16% | +25% | `Stat::Sight` `.percent`, `SkillStat::chipChance` `.percent` |

So a Mk III drill head with Quick drill at rank 3 drills at ×1.43 on top of
the hero, and a Mk II cargo pod carries 29 ore a trip, not 25.

**The Foundry makes them.** For ore and MH from the wallet, on the world's
clock, one at a time (the limit is one Foundry). It goes on while the pilot
is away, and the made module waits in the pilot's stores.

| Mark | Cost (ore + MH) | Make time | Research first (Lab) |
|---|---|---|---|
| Mk I | 100 + 50 | 60 s | none |
| Mk II | 300 + 200 | 180 s | 300 MH, 10 min |
| Mk III | 800 + 600 | 480 s | 900 MH, 30 min, after Mk II |

**The Lab researches marks.** Mk II and Mk III of each module are researched
before the Foundry makes them, Mk III after Mk II. MH only, one research at a
time, on the world's clock as well. A whole rig at Mk III is 5 × (300 + 900)
= 6,000 MH of research and 5 × (800 + 600) of making: the long MH sink.

**Fitting** is done at home: standing where the Prospector unloads at its own
finished Citadel or Hab Dome (as the respec, which is Citadel only). The
module comes out of the stores, and the one it replaces goes back to them.
Mark 0 takes a module off. A fitted module takes effect at once.

Refusals (the reply's text): "Build a Foundry first", "Build a Lab first",
"The Foundry is busy: Drill head Mk I", "The Lab is busy: Plate Mk II",
"Plate Mk II needs research at the Lab first", "Plate Mk III needs Mk II
researched first", "Only Mk II and Mk III are researched", "Plate Mk II is
researched already", "Not enough ore", "Not enough MH", "No Drill head Mk
III in your stores", "Drill head Mk I is fitted already", "No Drill head
fitted", "Fit modules at your Citadel or a Hab Dome: stand where you
unload".

A job in progress keeps going if its building is lost: the world has no
fights yet. Revisit when it does.

Later, not now: a KSTR contract kind "make a module" (docs/contracts.md).
It would count `made` news like the build kind counts finished buildings.

Code: the core's `Modules` and `Rig` (`Progression.h`), held in
`ProspectorProgress::rig`, so `Simulation::setProgress` adds the fitted
modules' blocks to the skills' and `ProspectorProgress::skillBoost` counts
them (the chip chance). The server's side is `unreal/server/world/WorldRig.cpp`:
the requests, the jobs finished each step (`WorldMap::rigs`, pilots off line
too), the wallet. Tests: `server/tests/RigTests.cpp`,
`core-tests/tests/RigTests.cpp`.

On the wire (protocol 10):

- `ProgressMessage::rig`: the core's `Rig`: `fitted` (module → mark),
  `stores` (module, mark, count), `researched` (module → highest mark;
  missing means Mk I), `making` and `research` (module, mark, `done` in world
  time, `Sky::time`).
- `ProgressMessage::foundry`, `lab`: the pilot owns a finished one.
- News `made` (`id` the module's code name, `value` the mark) and
  `researched` (the same).
- Requests `Make{module, mark}`, `Research{module, mark}`, `Fit{module,
  mark}` (variants 10 to 12); `net::Client::make`, `research`, `fit`.

The save (schema 7): `pilots.rig`, the rig as JSON. A pilot saved before has
none.

### Blueprint sites

Regions get a new crafted layer, `sites` ([regions.md](regions.md)). A site
holds one blueprint. Each pilot finds each site for themselves: it stays in
the world for the next pilot. Site kinds:

| Kind | Looks like | To open |
|---|---|---|
| crate | a landing crate on open ground | walk into it |
| wreck | a crashed lander, seen from far off | hold click to weld it open, 5 s |
| cache | buried, unseen until within 6 cells (Surveyor and Scanner pulse show it sooner) | drill it out like a deposit, 8 s |
| vault | a sealed vault on hard ground: a ridge top, a high pocket | stand there at its blueprint's floor and hold click, 10 s; reaching it may take a ramp or Cliff hop |

Each site gives one copy of its blueprint (C2: blueprints stack and are used
up by building), held already or not, with the find's XP. Before C2 a site
whose blueprint the pilot already had gave 100 ore and 20 MH instead.

On the server (`WorldSites` in `unreal/server/world/WorldMap.h`): a site is
reached when the Prospector's centre is within 2 cells of its point. The hold
is the act key held (`PilotInput::hold`) within reach; stepping away or letting
go starts it over. A site goes on the pilot's map when seen: a crate, a wreck
or a vault within 12 cells (the Prospector's sight), a cache within 6, any
kind within Surveyor's radius. Sites are kept per pilot as
`<region id>/<site name>`.

On the ground (W2, `AAcSites`): every site of the pilot's region stands as
its kind's model (`Tools/Editor/models/sites.py`), the same for everyone,
its front toward the lowest ground round it:

- **crate**: a supply crate on skids, hazard bands, a beacon mast;
- **wreck**: a small lander nose-down in a berm, torn panels, a snapped leg,
  a cold engine bell, a side hatch;
- **cache**: a low mound under a dark tarp, strapped and weighted with rocks,
  a whip antenna;
- **vault**: an armoured double door in a bevelled frame set into a rock
  outcrop, two lamp pylons.

Not opened, its gold lamp blinks (the vault's seam glows round and down
its door) and lights the ground at night; known and not opened, a gold column
of light stands over it, widened with distance so it reads across the
region. Opened by this pilot, the lid lies beside the crate, the wreck's
hatch hangs open, the tarp is thrown back, the vault's leaves stand slid
apart, and every light is out. The opening ring sits on the model while
the hold runs.

**Every blueprint but the Citadel's has two sites**, so a pilot has a second chance and a choice
of route. Proposed for The First Reach (`unreal/server/worlds/first.json`):

| Blueprint | Floor | Site 1 | Site 2 |
|---|---|---|---|
| Citadel | 1 | crate at Prospect Landing's pad | (none: every pilot lands by it) |
| Derrick | 3 | wreck by Prospect Landing's wells | wreck in Brine Pans |
| Hab Dome | 8 | cache in Rust Hollow | wreck in Tern Ridge |
| Foundry | 15 | wreck in Cold Fork | cache in Dust Terraces |
| Lab | 25 | cache in Glass Flats | vault in Ashen Shelf |
| Spacedock | 35 | vault in Slag Pockets | wreck on Cold Fork's ridge top |
| Bastion | 45 | vault in Ashen Shelf | cache in Brine Pans |
| Sentinel | 10 | vault in Tern Ridge | vault in Dust Terraces |
| Gatling gun | 5 | (none: the shop only) | |

**The first-blueprint path.** A new pilot lands at Prospect Landing with 600
ore. The Citadel's crate is beside the pad: walk into it in the first minute.
The Derrick's wreck sits by the region's two wells, in sight of the pad: weld
it open on the first walk to the wells. It opens at level 3, about 13
minutes in, and the 200 ore left after the Citadel pays its 75. So the MH loop
starts in the first quarter hour.

**Rare finds: survey chips.** Every ore load has a 0.5% chance of turning up a
survey chip (Core sampler and Lucky strike raise it; at most one every 5
minutes). A chip marks the nearest site the pilot hasn't found, in this region
or a neighbour, on the world map. One chip in 20 is a blueprint itself: a
random blueprint the pilot lacks whose floor is at most its level + 10. At 4
loads a minute that is a chip about every 50 minutes of mining at the base
rate.

### What happens to Citadel and Derrick access today

- A **new pilot** finds both, as above.
- **Existing pilots** are given the Citadel and Derrick blueprints by the
  migration (they have built them already), and the feats for the buildings
  they own, without the XP.
- Buildings already standing keep working, blueprint or not.

## MH sinks

Nothing costs MH today. This adds:

- **Respecs**, from the second: 40 to 4,000 MH.
- **Building costs**: the Hab Dome, Foundry, Lab, Spacedock, Bastion and
  Sentinel have an MH part (50 to 300).
- **Hydrogen boost** burns 10 MH a use.
- **The Citadel shop** (C2, "The Citadel shop"): blueprint copies from the
  Hab Dome up have an MH part that grows 1.5× a copy bought (40 MH for the
  first Hab Dome, 280 for the first Sentinel); the repair kit, the shield
  charge and the survey flare cost 10 to 30 MH each.
- **Foundry modules and Lab research** (P7, "Foundry and Lab"): modules at
  100 ore + 50 MH (Mk I), 300 + 200 (Mk II), 800 + 600 (Mk III); research
  300 MH (Mk II) and 900 MH (Mk III) per module.
- **Dropship flights** (F1, F2): 10 MH for the lift-off and 5 MH more for
  each step across the world's grid (columns plus rows apart): a hop to
  another Landing Pad in the same region 10, a neighbour 15, the far corner
  of a 3 × 3 world 30. Half (rounded up) from a Spacedock; nothing with
  Ringwalker. Pad link and Short hop change where and how fast, not the
  price. The server takes it at the call (`WorldMap::fly`,
  `ac::Flight::price` in `Flight.h`) and refuses a short wallet: "Not enough
  MH to fly: 15 MH, you have 4". The world map shows each flight's price beside its pads, red
  when the wallet is short, and greys FLY. A new pilot starts with 0 MH, so
  its first flight waits for its first Derrick; `--dev-hydrogen N`
  (`-AcMmoDevHydrogen=N`) starts it with N MH, and the picture and flow
  runs (`-AcMmoShots`, `-AcFlowTest`) start with 200 unless told otherwise.
- **Jet pack jumps** (F1b): each jump over a cliff with Cliff hop burns 2
  MH, 1 MH with Jump jets at rank 3 (`ac::JetJump` in `Flight.h`,
  `Simulation::pilotJump`). The MH the Prospector carries goes first, then
  the wallet. Short, it stays at the cliff face: the HUD says "Not enough MH
  to jet: 2 MH, you have 1" for a moment and the jet pack clicks dry
  (`jetdry`); a jump puffs (`jetpuff`). The keys line says what a jump costs
  ("walk into a cliff  jet pack over it, 2 MH"). Only the server's step
  pays (`Simulation::pilotPays`): the client's prediction checks the price
  and leaves the paying to it. The strategy game's jumpers (the Comet, the
  Cliff hop pick) jump free.

Skill points never cost MH: the tree is points only.

MH itself is finite in a world: wells hold 2,250 and don't grow back
(`Simulation::regrow` only refills ore). Wellspring helps one well at a time;
the world needs a rule (open question 5).

## Old MMO saves

A pilot saved today has a Prospector `KindRecord` with XP earned at ×0.1 and
up to nine picks.

- **XP × 10**, to plain XP. An old level 5 (600 old XP) becomes 6,000: new
  level 10 (5,355 to 6,155) with 9 points. The work done counts at the new rate.
- **Picks are dropped**; their points come back as part of the new level's
  points, unspent. The first respec stays free.
- The Citadel and Derrick blueprints are given; feats for owned buildings are
  set without XP; regions and pads already visited are not known (the save
  doesn't record them) and earn their XP on the next visit.
- On the first login after the migration the pilot sees "Your Prospector's
  skills were reset: 9 points to spend (K)".

The save's schema goes to **v2** (`WorldStore::migrations`):

    {2, "ALTER TABLE pilots ADD COLUMN progress TEXT NOT NULL DEFAULT ''"}

`progress` is JSON (as `levels` is today, through `SessionStore::encode`). The
conversion from `levels` to `progress` runs on load in `WorldMapSave` when
`progress` is empty and `levels` isn't; the next save writes `progress`, and
`levels` is written empty from then on.

### The realm migration (schema v8, R1)

Before R1 a building's row named its `owner`, the pilot that built it, and
only that pilot used it. Schema **v8** renames the column:

    {8, "ALTER TABLE buildings RENAME COLUMN owner TO built_by"}

No row changes. An old save loads with every building the realm's: any
pilot unloads, respawns, respecs and flies at them, and the builder kept in
`built_by` still gets its credit (the v4 blueprint gift and the feats read
it). Tests: `server/tests/RealmTests.cpp` (`realm_old_save_buildings_become_the_realms`).

### The realm wallet migration (schema v9, C1)

Schema **v9** adds the realm wallet, one row, and each pilot's tally:

    CREATE TABLE realm_wallet (id INTEGER PRIMARY KEY CHECK (id = 1), ore INTEGER NOT NULL, hydrogen INTEGER NOT NULL);
    INSERT INTO realm_wallet (id, ore, hydrogen) VALUES (1, 0, 0);
    ALTER TABLE pilots ADD COLUMN taxed_ore …, taxed_hydrogen …, donated_ore …, donated_hydrogen … DEFAULT 0

An old save starts with an empty realm wallet and every pilot's tally at
nothing. Tests: `server/tests/RealmWalletTests.cpp`
(`realm_wallet_saved_and_loaded`).

### The blueprint copies migration (schema v10, C2)

Schema **v10** marks the pilots saved since blueprints stack:

    ALTER TABLE pilots ADD COLUMN stacked INTEGER NOT NULL DEFAULT 0

The copies live in the progress JSON (`blueprints` an object, kind →
copies; before, a list of the kinds found, read as one copy each). A pilot
saved before (`stacked` 0) gets, for each blueprint it had, **as many copies
as the realm may still build of that kind under the current limit** (the
pilot's, with Overbuild, less the realm's buildings of that kind across the
world, finished or not), **at least 1**: a pilot with the Derrick blueprint
in a realm with one Derrick holds 3; with the Citadel blueprint and the
Citadel standing, 1. So nobody loses a building they could have put up, and
nobody gets more than the realm can hold. Nothing was bought before, so
every price starts at the first copy's. Saved again, the pilot is
`stacked` 1. Tests: `server/tests/SitesTests.cpp`
(`sites_v3_pilot_keeps_the_blueprints_of_its_buildings`),
`OldSaveTests.cpp`, `SaveTests.cpp`.

### The Citadel missions migration (schema v12, K2)

Schema **v12** adds a pilot's Citadel missions (its board, Journal, day
and next number, as JSON):

    ALTER TABLE pilots ADD COLUMN missions TEXT NOT NULL DEFAULT ''

A pilot saved before has none; its first step draws its board. (v11 is the
Brood's, N2.) Tests: `server/tests/MissionsTests.cpp`.

### The mercenaries migration (schema v14, H2)

Schema **v14** adds a pilot's mercenaries (each one's name, field, unit,
the ore worked, a fall's end, its load, what it delivered; the wage day,
the next number, the ore delivered while away; as JSON):

    ALTER TABLE pilots ADD COLUMN mercs TEXT NOT NULL DEFAULT ''

A pilot saved before has none. Their units are saved with the pilot's own
(the `units` table, under its key). (v13 is D1's.) Tests:
`server/tests/MercsTests.cpp`.

## On screen

### The level-up

- A banner, "PROSPECTOR LEVEL 12", and a second line "+1 SKILL POINT · K",
  with the `levelup` sound. The game doesn't pause.
- At a gate level a third line: "TIER 2 OPEN", "SLOT C OPEN", "CAPSTONES
  OPEN".
- Points wait: nothing is lost by spending later.

### The skill screen (K)

- K opens and closes it; Esc closes it. The world goes on (as with the map);
  the Prospector stands still while it is open, and the pointer is free.
- Two tabs, **SKILLS** and **BLUEPRINTS**; Tab switches. A third, **RIG**,
  opens the rig screen in its place (Tab from BLUEPRINTS, or J).
- The header: "LEVEL 34 · 3 POINTS", and "RESPEC 460 MH" (greyed away from
  the Citadel, with why).
- SKILLS: five columns, left to right Deep Vein, Quicksilver, Torchwright,
  Wayfinder, Hardshell, each headed with its points ("DEEP VEIN 14"). Five
  rows, tier 1 at the top, the capstone at the bottom; each row's gate on the
  left ("LEVEL 25 · 12 IN BRANCH"). A node is a tile: icon, name, rank pips
  ("2/5"). Locked tiles are grey and say what they wait for; open ones are
  outlined; learned ones lit in the accent colour.
- A right-hand panel shows the picked node: what each rank does, what the
  next rank adds, and why it can't be taken if so.
- Click or Return spends a point on the picked node; arrows move the pick.
  Undo shows on a node spent in the last 60 s.
- The slot bar along the bottom: Q, C, G (closed slots show their level).
  Pick a learned active and press Q, C or G, or drag it.
- BLUEPRINTS: every building, found or not. Found: the floor, ready or
  "opens at level N", the site it came from. Not found: "?" and how many
  sites the pilot has marked.

### The rig screen (J)

The skill screen's sibling for the Foundry and Lab (`SAcRigScreen`, G1): the
same chrome, header and tabs (SKILLS · BLUEPRINTS · **RIG**). J or Esc closes
it, K swaps to the skill screen, Tab to SKILLS. The world goes on; the
Prospector stands. The header shows the wallet (ore and MH), which pays the
Foundry and the Lab.

- A row for each module across three panels, so a module's slot, its marks
  at the Foundry and its research line up. Its icon comes from
  `Tools/hud/module_icons.py`, drawn in the skill icons' hand.
- **FITTING**: the slot (its icon lit in the mark's colour, "MK II FITTED",
  `Modules::text`), then a chip per mark with the stores' count. Clicking a
  chip in stock fits that mark; clicking the lit chip takes it off. Away from
  home the panel says the server's words ("Fit modules at your Citadel or a
  Hab Dome: stand where you unload").
- **FOUNDRY**: a cell per mark with its price (a part in red when the
  wallet is short) and its time. The cell shows "LAB FIRST" until the mark is
  researched, "FOUNDRY BUSY" while another job runs, and "MAKE" otherwise. The
  running job has a bar and a countdown in its cell and in the panel's head.
- **LAB**: Mk II and Mk III, the same way: "RESEARCHED", "AFTER MK II", or
  "RESEARCH". A countdown runs while a job is on.
- Without a Foundry or a Lab, that panel is greyed under "BUILD A FOUNDRY
  FIRST" or "BUILD A LAB FIRST".
- The foot names the picked cell and says what Return or a click asks, or
  why it can't. The core's refusals use the server's words. A refusal the
  server sends back shows in red.
- Arrows move the pick; Return or a click asks. Marks are coloured Mk I
  steel, Mk II cyan, Mk III gold.
- Toasts: "MODULE MADE: PLATE MK II" and "RESEARCHED: TORCH TIP MK II"
  (AcProgressHud).

### The HUD

- The level number and the XP bar stay where they are; the bar runs from
  this level's XP to the next.
- A points badge by the bar when points wait: "3 POINTS · K".
- The slot row over the dashboard, left of the Prospector (U1: clear of the
  pawn in the middle): Q, C, G with the active's icon and a cooldown ring;
  the supplies' cells (1-4) right of it.
- The Z/X cards go in the MMO mode.
- A toast for a find: "BLUEPRINT FOUND: FOUNDRY (opens at level 15)",
  "SURVEY CHIP: a site marked in Cold Fork".
- The build menu (B) lists only the blueprints the pilot has, in a fixed
  order; one under its floor is greyed "level 15".
- The world map marks the sites the pilot knows of, found or not.
- The build menu's list stands on a panel over the dashboard's right well
  while B is open (big enough to read), with the world's price and its MH
  part. The client's mirror sim holds the pilot's progress as the server's
  Region does, so prices, full hp (Plating) and the depots agree.
- Fallen (P6): the view dims, "PROSPECTOR DOWN", "BACK IN 7 s", where it
  comes back ("AT YOUR CITADEL", "AT THE LANDING PAD, COLD FORK") and what
  Second wind kept.
- With a load, the prompt and the compass name the realm's finished
  Hab Domes as depots beside the Citadel ("Walk into your Citadel or a Hab
  Dome to unload").
- C1: the realm wallet small in the resource bar ("REALM", "1,240 · 56
  MH"); the unload toast's second line, "3 TO THE REALM"; H at the Citadel
  opens the Citadel screen ("The realm's tax (C1)").
- C2: the consumables' stacks right of the Q/C/G slots (keys 1-4, the
  count, the cooldown, gold while a shield or a flare runs); the build
  menu's names carry the copies held ("Hab Dome ×2"); the Citadel screen's
  SHOP tab; a survey flare rings its reach on the minimap ("The Citadel
  shop (C2)").
- K2: the Citadel screen's MISSIONS tab; the Journal (L); a taken
  mission's place as a diamond in its tier's colour on the minimap and
  the compass ("MISSION 15"); "MISSION DONE: MARK THE GROUND" with its pay
  ([missions.md](missions.md)).
- R1: a building in sight names its builder ("Hab Dome  ·  built by Pilot
  045B"); a realm building finished anywhere is a toast to every pilot; the
  build menu's limit tag reads "REALM LIMIT".
- The world map is the flight menu (F2): the realm's Landing Pads are cyan
  squares, picked only in regions the pilot has been to (dim elsewhere);
  the region files' pads are small dim circles, never picked. The Dropship
  is called within Pad link's reach of a Landing Pad ("At Landing Pad 1:
  pick a Landing Pad to fly to"; off one: "Not at a Landing Pad: Landing
  Pad 1 is 12 cells away").

## How it fits the code

### The core (`AutocraftCore`)

- **A new catalogue, beside `Leveling`:** `Progression.h/.cpp`. Engine-free,
  data-driven, MMO only; `Leveling.cpp` is left alone.
  - `enum class Skill : uint16_t`, append only (saves keep loading), and
    `enum class Branch { deepVein, quicksilver, torchwright, wayfinder,
    hardshell }`.
  - `SkillNode`: `skill`, `branch`, `tier`, `ranks`, the `effects` of one rank
    (rank r applies r times: `.percent` and `.plus` add up) or, for every-Nth
    and the site nodes, one row per rank; `active` (cooldown, what it does);
    title and text.
  - `Progression::xp(toReach)`, `level(xp)` (cap 100), `points(level)`,
    `gate(tier)`, `respecPrice(level)`, `signature` (FNV-1a of the table, as
    `Leveling::signature`).
  - `ProspectorProgress`: `xp`, `ranks` (skill → rank), `slots` (3), `respecs`,
    `freeRespec` (a tree change), `recent` (spends in the last 60 s),
    `blueprints`, `sites` found, `regions` and `pads` visited, `feats`,
    `chips`, `signature`. `refusal(spend)`, `refusal(slot)`, `refusal(respec)`.
- **The sim reads it** where it reads picks today: `Simulation::boost` adds the
  driven unit's skill effects when the region's sim has the pilot's progress
  (set by `Region` as it sets the side's `PilotRecord` today). With no
  progress set, nothing changes: the strategy game and its tests are as they
  are.
- **Reused blocks:** `Stat` (Drill, Hydrogen, OreCarry, HydrogenCarry, Build,
  Repair, BuildingCost, Speed, Sight, JumpReach, Hp, Armor, Damage, Range,
  DamageTaken) on `Scope::Driven`; `Shield`; `SelfMend`; `CliffJump`;
  `Grenade`; `Burst`.
- **Blocks to add:**
  - every-Nth (`EveryThird` with an N), on ore trips, pumps and strikes;
  - `Burst` channels for drill, build and repair, and damage taken, and an MH
    cost;
  - stats: deposit yield, well yield, regrow time, unload yield (ore, MH),
    load chance (triple), chip chance, speed while carrying (MH, nothing),
    work reach, flight time, building limit, own buildings' hp and damage
    taken, blueprint floor cut, built share on placing;
  - one-offs: Seismic charge, Quickfill, Emergency weld, Derrick tender,
    Wellspring, Field pump, Pad link, Homing beacon, Ringwalker, Surveyor,
    Scanner pulse, Second wind.

### Region files

- `sites`: `[{"name": "Lander wreck", "kind": "wreck", "at": [x, z],
  "blueprint": "derrick"}]` in `RegionFile.h`: parse, write, validate (open
  ground, not on ore or a well or a pad, joined; a vault may be cut off and is
  checked to be reachable by a ramp or a cliff hop), preview (a gold square,
  labelled). Into `region::Built` beside pads and spawns.

### The server

- **XP:** world regions use `Progression::level`, `xpScale` 1, MH 2 an MH,
  welding ½, and the one-offs (region, pad, blueprint, chip, feats) in
  `WorldMap`, which knows the pilot.
- **Requests** (`net::RequestMessage`): `Spend{skill}`, `Undo{skill}`,
  `Respec{}`, `Slot{index, skill}`. The server checks: a point free, the
  level and branch gate, a rank left; for undo, spent in the last 60 s; for a
  respec, at the Citadel, the MH in the wallet (free if due); for a slot,
  learned, an active, the slot open. Refusals say why, as today. `Pick` is
  refused in a world.
- **Actives are inputs:** `ac::Pilot` gains the slot pressed (0–3), so a press
  travels with the inputs and is repeated if lost. The server checks the
  cooldown.
- **Sites:** opening one is the act input held at a site, checked on the
  server: distance, time held, level for a vault. The find goes into the
  pilot's progress.
- **Building:** `WorldMap::build` refuses without the blueprint, under its
  floor (less Blueprint savant), or past the limit. `kWorldBuildKinds`
  becomes the per-pilot list.
- **The client is told:** a new reliable `Progress` message on every change
  (xp's level, points, ranks, slots, cooldown ends, respec price, blueprints,
  sites known and found). Snapshots keep only the XP. New events: blueprint
  found, chip found, region discovered, feat. The protocol version goes up.
- **The save:** schema v2 above, the conversion on load.

### Tests

Isolated scenarios only, never a played game:

- core: `total(10) = 5,355`, `total(50) = 145,775`, `total(100) =
  1,034,550`; `level` at the edges; points = level − 1; each gate (level and
  branch points) refuses and then allows; ranks stop at the max; respec price
  at 10, 50, 100; the signature changes when a node changes; 185 points in
  the tree, 37 a branch;
- core effects: a rank-3 Quick drill drills at ×1.18 on top of the hero;
  Rich vein rank 2 doubles every 4th trip; Overcharge's burst runs 10 s and
  then waits 60; one test per new block;
- the server: Spend, Undo, Respec and Slot accepted and refused; MH taken for a
  respec; a build refused without the blueprint and under the floor; a site
  opened by holding act; a one-off paid once only;
- the save: a v1 file with a level-5 Prospector loads as level 10 with 9
  points, the two blueprints, and saves as v2;
- region files: `sites` parse, write back the same, each problem reported.

## The plan

| # | Task | Needs | Done when |
|---|---|---|---|
| P1 | Core catalogue: `Progression.h/.cpp`, the 65 nodes, the curve, gates, respec price, `ProspectorProgress` and its refusals | | core tests above pass; `make -C unreal/core-tests` green |
| P2 | Core effects: skills in `boost`, the reused blocks, the new blocks for mining, pumping, building and the plate | P1 | a test per block passes; the strategy game's leveling tests unchanged and green |
| P3 | Server progression: the curve and `xpScale` 1 in worlds, the new XP sources, Spend/Undo/Respec/Slot, the `Progress` message, actives as inputs, schema v2 and the conversion | P1 | server and net suites pass, the migration test included |
| P4 | Blueprints: `sites` in region files (tool, preview, validate), the sites of The First Reach placed with the region-crafting skill, opening sites, survey chips, building gated by blueprint, floor and limit | P3 | `autocraft-region validate` ok on every region; server tests for finds and refused builds pass; previews show the sites |
| P5 | The game: the skill screen (K), the level-up banner, the HUD (points, slots, toasts), the build menu from blueprints, sites on the world map; the Z/X cards off in the MMO | P3, P4 | a hidden picture run (`-AcMmoShots`) shows each screen; the user checks by hand |
| P6 | Building roles confirmed by the user: Citadel respawn and respec, Hab Dome depot, Spacedock pad, MH costs and limits; the travel and site nodes' blocks (Pad link, Homing beacon, Ringwalker, Surveyor, Scanner pulse) | P4, user's answers | server tests for each role pass |
| P7 | Foundry modules and Lab research (done on the server: "Foundry and Lab") | P6, open question 3 | a module made, fitted and its stat applied, in a server test |

P1 then P2 and P3 side by side, then P4, then P5 and P6 side by side.

## Open questions for the user

1. **The ×0.1 XP rate goes back to 1 in worlds.** The curve does the slowing
   now, and 1 XP an ore keeps the numbers readable. Recommendation: yes.
2. **Hardshell has nothing to fight until no-man's land.** Ship it now, or
   hide the branch until fights come? Recommendation: ship it, marked "for
   the frontier", so builds that want it can plan; its tier 1 hp and armor
   already help with future hazards.
3. **A crafting layer: Foundry modules and Lab research.** It gives the
   Foundry and the Lab a job and MH a steady sink, but it is a second system
   beside the tree. Recommendation: yes, after P1–P6, as P7. Answered yes;
   built in P7 ("Foundry and Lab").
4. **Respec only at your Citadel?** Recommendation: yes. It gives home a
   purpose and stops a mid-fight rebuild.
5. **MH runs out in a lasting world.** Wells don't grow back. Recommendation:
   in worlds a well regrows 1 MH every 10 s while no one pumps it, up to its
   2,250; Wellspring adds on top.
6. **The building roles and numbers** in the buildings table (Hab Dome
   depot, Spacedock pad, Bastion bunker, Sentinel turret, the limits, the MH
   costs, Garrison held back). Recommendation: take them as written for now
   and tune after play.
