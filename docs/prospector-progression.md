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
| Finding a blueprint | 50 × its level floor | Derrick 150, Foundry 750, Sentinel 2,750. Once per blueprint. |
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

Five branches, 13 nodes each, 65 nodes in all.

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
| 2 | **Cliff hop** | 1 | jump up and down cliffs (passive) | | `CliffJump` |
| 3 | Jump jets | 3 | jump reach +1 cell | +3 | `Stat::JumpReach` |
| 3 | Pad link | 3 | fly from within 8 / 12 / 16 cells of a pad's centre, up from 4 | 16 | new |
| 3 | Homing beacon | 3 | your Citadel counts as a pad; then your Hab Domes too; then your Derricks too | all three | new |
| 4 | Lucky strike | 5 | survey chips come 10% more often | +50% | new: chip chance |
| 4 | **Sprint** (active) | 1 | 6 s of walk +60%; cooldown 30 s | | `Burst` |
| 4 | Short hop | 3 | flights take 20% less time | 60% less (4 s → 1.6 s) | new: flight time |
| Cap | **Ringwalker** | 1 | fly from anywhere, not only a pad, once every 120 s | | new |

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
needs its blueprint, found in the world, and the pilot at or above its
**level floor**. A blueprint found below the floor is kept and shows
"opens at level N".

| Building | What it does for the pilot | Floor | Limit | Cost (ore + MH) | Status |
|---|---|---|---|---|---|
| Citadel | Home: unload point, respawn point, where the respec is made | 1 | 1 | 400 | as today; respawn and respec are proposals |
| Derrick | Pumps MH on a well (as today) | 3 | 4 | 75 | as today |
| Hab Dome | A field depot: unload ore and MH into the wallet away from home, and a respawn point | 8 | 3 | 150 + 50 | proposal |
| Foundry | Makes rig modules: drill head, cargo pod, plate, torch tip, survey lens (open question 3) | 15 | 1 | 300 + 150 | proposal |
| Lab | Researches better module marks (Mk II, Mk III) for MH and time | 25 | 1 | 200 + 200 | proposal |
| Spacedock | A private pad: fly from it and land on it from any pad; later, routes to far worlds | 35 | 1 | 400 + 300 | proposal |
| Bastion | A bunker: the Prospector climbs in, takes no damage, its torch fires from it at +2 range. For holding a claim in no-man's land | 45 | 2 | 250 + 100 | proposal, matters later |
| Sentinel | A missile turret that guards your buildings, ground and air | 55 | 4 | 150 + 100 | proposal, matters later |
| Garrison | Trains other kinds in the strategy game. Held back: no blueprint in the world while the MMO is Prospector only | | | | held back |

Limits are per pilot, across the world. Overbuild raises the Hab Dome,
Derrick and Sentinel limits. The Citadel and the Derrick stay ore only, so the
first loop costs no MH.

Hours to each floor at the model's pace: Derrick 13 min, Hab Dome 49 min,
Foundry 1.95 h, Lab 4.6 h, Spacedock 9.1 h, Bastion 15.6 h, Sentinel 24.4 h.

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

A site whose blueprint the pilot already has gives 100 ore and 20 MH instead,
once, and no XP.

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
| Sentinel | 55 | vault in Tern Ridge | vault in Dust Terraces |

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
- **Foundry modules and Lab research**, if confirmed (open question 3):
  modules at 100 ore + 50 MH (Mk I), 300 + 200 (Mk II), 800 + 600 (Mk III).

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
- Two tabs, **SKILLS** and **BLUEPRINTS**; Tab switches.
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

### The HUD

- The level number and the XP bar stay where they are; the bar runs from
  this level's XP to the next.
- A points badge by the bar when points wait: "3 POINTS · K".
- The slot row, bottom centre: Q, C, G with the active's icon and a
  cooldown ring.
- The Z/X cards go in the MMO mode.
- A toast for a find: "BLUEPRINT FOUND: FOUNDRY (opens at level 15)",
  "SURVEY CHIP: a site marked in Cold Fork".
- The build menu (B) lists only the blueprints the pilot has, in a fixed
  order; one under its floor is greyed "level 15".
- The world map marks the sites the pilot knows of, found or not.

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
| P7 | Foundry modules and Lab research, if the user wants them | P6, open question 3 | a module made, fitted and its stat applied, in a server test |

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
   beside the tree. Recommendation: yes, after P1–P6, as P7.
4. **Respec only at your Citadel?** Recommendation: yes. It gives home a
   purpose and stops a mid-fight rebuild.
5. **MH runs out in a lasting world.** Wells don't grow back. Recommendation:
   in worlds a well regrows 1 MH every 10 s while no one pumps it, up to its
   2,250; Wellspring adds on top.
6. **The building roles and numbers** in the buildings table (Hab Dome
   depot, Spacedock pad, Bastion bunker, Sentinel turret, the limits, the MH
   costs, Garrison held back). Recommendation: take them as written for now
   and tune after play.
