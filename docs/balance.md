# Balance (B2)

M8's economy checked against the pace model
([prospector-progression.md](prospector-progression.md), "The pace model").
Every number below has three sources:

- the rule it comes from;
- the target the pace model sets;
- a measured value, from isolated scenes, never a whole game.

The server suite's `BalanceTests.cpp` pins every curve, so a later change
breaks a test before it breaks the game. A change to a number goes into this
table with its before and after.

**The decision (manager, 2026-10-15):** risk and play pay better than mining,
but do not collapse the curve. A pilot who mines, takes the Citadel board and
holds a canyon runs about ×2 the mining curve, and never over ×2.5.

## How it is measured

`make -C unreal/server bench` runs `build/balancebench 10`
(`unreal/server/bench/BalanceBench.cpp`). It works on the first world, with no
Brood, contracts, starfalls, frontier pilots or mercenaries, and has three
parts:

1. **Mining.** A driven Prospector runs for 10 game minutes at levels 1, 5,
   10, 15 and 20.
   - It mines from a Citadel east of the start's ore field, 15 cells from the
     deposits. Each trip goes to the nearest deposit with a straight walk,
     drills a full load and unloads at the Citadel's edge.
   - Three builds:
     - **none:** no points spent;
     - **economy:** every point in Quick drill, Big haul, Light frame, Deep
       bore, Ore sled and Dense pack, as far as the tree allows;
     - **average:** half the points there, the pace model's "average build".
2. **A canyon cycle.** The Fork's waves run from WAITING onward, for a
   level-15 pilot standing south of the canyon, with its own turrets on the
   zone's build spots (8+3, 10+4, then 12+5 Gatling Guns and AA towers). The
   bench records each wave's time out and its pay.
3. **The all-in pilot.** An hour of play is three quarters work (the pace
   model's 45 minutes). The bench fills it in this order:
   - **The board:** 6 offers a world day of 40 minutes, so 9 an hour. Each
     takes about 1, 2.5 and 5 minutes for easy, hard and perilous: the walk
     to its place, or the next region, plus its ask.
   - **From level 10, one canyon cycle every 70 minutes:** its waves and
     breaks, then a 40-minute hold during which the pilot mines.
   - **Mining** at the average build for the rest.

   It counts XP only, since XP sets the level. The bench also times level 1
   to 10 at that rate, a minute at a time. One-off XP (regions, pads, feats)
   is left out, as in the pace model.

An XP-equivalent counts 1 ore as 1 XP and 1 MH as 2 XP (`WorldXp`: 1 an ore,
2 an MH).

## The table

| Number | Rule | Target (pace model) | Measured | Change |
|---|---|---|---|---|
| Mining, level 1 | 25 ore a load, 6 s drill, walk at 3.94 × 1.2 cells/s | 6,000 XP an hour all work (100 a minute) | 7,884 ore an hour unloaded (306 trips, 15.2-cell walk), 6,948 kept | none |
| Mining, levels 5 / 10 / 15 / 20, average build | the drill and road skills | +2% a level: 6,480 / 7,080 / 7,680 / 8,280 | 8,334 / 8,634 / 11,172 / 12,624 | none |
| Mining, levels 5 / 10 / 15 / 20, all economy | every point in the drill and road | "levels faster" | 8,634 / 12,624 / 13,956 / 13,956 | none |
| Realm tax | 10% of each load, rounded to nearest, at least 1, never the whole (`Progression::realmTax`) | 10% | 936 to 1,218 ore an hour from one mining pilot; 7 to 12% of a 25 to 49-ore load (whole numbers: 34 pays 3) | none |
| Pace rate | `4,500 × (1 + 0.02 × (L − 1))` XP an hour | | 75 / 81 / 88.5 / 103.5 XP a minute at 1 / 5 / 10 / 20 | none |
| Blueprint copy | `(50 + 25F) × 1.5ⁿ` ore, `5F × 1.5ⁿ` MH (none for the Citadel, Derrick, Landing Pad), to 10 | the first copy is a few minutes of income at its floor | Citadel 80 ore = 1.2 min of what a pilot keeps at its floor; Derrick 1.9; Gatling 3.3; Hab Dome 4.3; Sentinel 5.0; Foundry 6.8; Lab 9.4; Spacedock 11.4; Bastion 12.9 (the price rises with the floor, income 2% a level) | none |
| Consumables | repair kit 120 + 10 MH, MH canister 150, shield charge 100 + 30 MH, survey flare 80 + 15 MH | a couple of minutes at level 1 | 2.1 / 2.2 / 2.4 / 1.6 minutes of what a level-1 pilot keeps | none |
| Mercenary output | the pace model's hour over a real day (1/24 of the pilot's rate) | never out-earns the pilot | 202.5 ore an hour at level 5, 240 at 15, 296 at 30; all three at most 1/8 of the pilot | none |
| Mercenary fee and wage | 500 ore + 50 MH doubling with each held; `5 + L/5` MH a world day (36 a real day) | pays back sensibly | fee back in 3.7 h (first, level 5), 6.3 h (second, 15), 10.3 h (third, 30) of its work, net of tax and wage; the wage is 9 to 13% of its output | none |
| Mission XP | minutes of play at the pace rate: easy / hard / perilous | about ×3 the time a mission takes (≈1 / 2.5 / 5 min) | **before:** 8 / 20 / 40 min = ×8 to ×16; **after:** 3 / 8 / 15 min = ×3; 225 / 600 / 1,125 XP at level 1 | **8 / 20 / 40 → 3 / 8 / 15 minutes** |
| Mission ore | 150 / 450 / 1,000 +2% a level | half | **after:** 75 / 225 / 500, rounded to 25 | **halved; rounding 50 → 25** |
| Mission MH and copies | 10 + 2L / 30 + 4L / 60 + 8L MH; 1 and 2 copies or supplies | | | none |
| Canyon wave pay | `xp × n`, `ore × n`, `MH × n`; wave 10 ×2 and a rare copy; ×1.3 in a level-3 region (Z3); each fighter at least a tenth | ×0.4 | **before:** 10,623 XP + 6,750 ore + 1,800 MH for waves 1–9 in 27.8 min (×8 the pace); **after:** 5,307 XP + 2,700 ore + 720 MH in 25.9 min (waves 1–9, 12 + 5 guns) | **200 / 150 / 40 → 80 / 60 / 16 a wave** |
| Turret marks | Gatling 100 + 50, 300 + 200, 800 + 500 a stat; AA tower ×1.5; Citadel 250, 500 + 50, 1,000 + 150 (realm wallet) | a mark pays back within a held cycle's kills | a cycle's kill XP to the guns' builder: The Fork 1,825, The Drowned Cut 1,845, The Roost 2,420. A first mark (200 XP-eq) pays back within one cycle; a stat's three (2,700) within two | none |
| Frontier booked mining | `5 / (6 + 2 × 12 / 3.94)` ore a second | neither starve nor flood | 0.41 ore/s = 1,489 an hour, a quarter of a pilot; five give the realm at most 745 ore an hour | none |
| Realm projects (R2) | Beacon 2,000 + 100 MH, Relay 2,500 + 150, Dock Crew 3,000 + 200, Armoury 4,000 + 300; × 1.2 each repeat of a kind (ore to 50, MH to 10) | the realm wallet's sink, beside the Citadel's marks: 2 to 4 hours of one mining pilot's tax (936 to 1,218 ore an hour) | the first of each 2.0 / 2.5 / 3.0 / 4.0 hours of 1,000 ore of tax an hour; the fourth Armoury 6,900 ore + 520 MH (`balance_realm_projects`) | new (R2) |
| **All-in pilot** | mining + the board + a canyon cycle | ×2 ± 0.3 of the curve | **before B2:** about ×4 (the board alone 15,300 XP an hour at level 1); **after:** ×1.87 / 1.86 / 2.02 / 1.95 / 1.89 at levels 1 / 5 / 10 / 15 / 20 | from the two changes above |
| **Level 1 to 10** | | inside the first hour, around 30 min | **before:** about 18 min; **after:** 36 min (the pure-mining curve: 66) | from the two changes above |

The all-in hour, XP:

| Level | Curve | Mining | Board | Canyon | All-in | × curve |
|---|---|---|---|---|---|---|
| 1 | 4,500 | 2,562 | 5,850 | | 8,412 | 1.87 |
| 5 | 4,860 | 2,709 | 6,330 | | 9,039 | 1.86 |
| 10 | 5,310 | | 6,172 | 4,549 | 10,721 | 2.02 |
| 15 | 5,760 | | 6,694 | 4,549 | 11,243 | 1.95 |
| 20 | 6,210 | | 7,203 | 4,549 | 11,752 | 1.89 |

From level 10 the board and the canyon fill the 45 minutes of work, so
mining shows nothing. The canyon's XP is the best cycle the bench's guns
reached (waves 1–9). Wave 10 never paid, so this is a low mark.

## Seen, not changed

- **The guns alone don't hold The Fork.** With 12 Gatling Guns and 5 AA
  towers and the pilot standing, waves 9 and 10 break through. One wave in
  each run stayed out 1,066 to 1,150 s: a straggler that never came to the
  guns nor took the prize, until the wave's time ran out. Worth a look in
  the canyon's code (Z2/Z3).
- **The realm wallet's only sink was the Citadel's marks:** 12 of them, 7,000
  ore and 800 MH in all, bought by one mining pilot's tax in about 6 to 7
  hours. R2's realm projects are the sink since (the row above): a world
  day of each kind costs 2 to 4 hours of that tax, rising with each repeat.
- **Booked pumping** (`frontierBookWork`) uses the ore load (5) and drill time
  in place of the MH load (4) and pump time (3.5 s). It comes to the same
  rate within 1% (0.414 against 0.417 MH/s), so it was left.
- [mmorpg-play.md](mmorpg-play.md) said level 2 was at 60 XP; it is at 503
  (corrected).
