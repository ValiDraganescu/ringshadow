# The canyon game

Canyon defence in the MMORPG mode (docs/mmorpg-m8.md, Z2). A canyon zone of a
region file (Z1, [regions.md](regions.md) "zones") is a field of rich ore and an
MH well at the end of canyons. The Brood comes down those canyons in waves to
mine it for itself. The pilots build turrets on the rims to hold it. The first
zone is **The Fork** in Cold Fork: a mesa with two arms, the North tunnel's and
the East tunnel's, cut to a junction where the prize lies.

Since E1 (scarcity, [regions.md](regions.md) "Scarcity") the prize is its
region's one MH well and its rich vein: five mounds in a ring 4 cells round
the well (The Fork and The Drowned Cut 1,700 ore each, The Roost 2,600). The
region's other vein lies outside the canyon, smaller. The prize fills up to
its full ore at a reset and at the end of HELD, as before.

The server runs the game (`unreal/server/world/WorldCanyon.cpp`). The sky
carries each zone's state to every pilot (`Sky::zones`). The client shows it
in the wave panel, the warnings, the map and the Journal (`AcCanyon.cpp`).

## The user's words (2026-10-14)

"The bugs come in waves; if the players do not manage to defend, the bug
queen comes and destroys all the turrets and then it restarts from the first
wave. The first wave keeps replaying until the players manage to defeat it
with turrets or units. Destroying the easiest wave starts the zone game. The
goal of the bugs remains to mine the ore for themselves." Placed turrets cost
what they cost anywhere; their upgrades are the pilot's (D3).

## States

Each zone has one of four states. The server owns it and the save keeps it.

| State | What happens | Ends |
|---|---|---|
| **WAITING** | Wave 1, "the opening wave", comes a minute after the zone goes quiet (the server's start, a reset, the hold's end). It then comes again every 3 minutes until the pilots beat it. It is 4 Shardlings down one arm, and the arm turns with each replay. When it breaks through, nothing is lost: it simply comes again. | Wave 1 beaten: RUNNING at wave 2 |
| **RUNNING** | Waves 2 to 10, one at a time, with a 30 s break between two. The panel counts the break down. | The last wave beaten: HELD. A wave breaks through: FAILED |
| **FAILED** | The Matriarch's siege. She comes out of the tunnel the wave came from with 6 Shardlings. She fells every turret of the zone, nearest first, and goes home. | She is home (every turret down): the reset. Felled out there: the reset, and the turrets left stand. 10 world minutes without her there: every turret left falls, then the reset |
| **HELD** | The canyon is the pilots' for a world day (40 minutes). No waves come, and the prize is theirs to mine. | The day is over: WAITING |

**The reset** returns the zone to WAITING and wave 1, with the prize full again.
Wave 1 then comes after 3 minutes, which gives the pilots time to rebuild. The
prize also fills up at the end of HELD. While FAILED or HELD, the pilots mine
what is left.

If the Matriarch is down (N3: killed, and not back for three world days), she
cannot come. A wave that breaks through then only resets the zone, and the
turrets stand. The world hears "spared". Killing her pays off in the canyon too.

### The waves wait with no pilot near

A zone's waves run only while a pilot is in its region or next to it, as the
Brood's director does. The world runs on with nobody there (S2), and it must
not lose turrets to a wave no one sees.

- **WAITING and RUNNING**: the countdown to the next wave stops.
- **A wave out stays where it is.** If the region then sleeps (no pilot near
  for 2 minutes, `WorldBrood::idleDespawn`), the director takes its bugs away.
  The same wave then comes again, whole, after a break once a pilot is back:
  - after 30 s while RUNNING;
  - after a minute while WAITING.
- **FAILED and HELD** end on the world's clock, nobody there or not.
- **A save** keeps each zone's state, wave and next change; a wave out comes
  again after its break on load.

## A wave

A wave is a group of bugs, or several, sent down the zone's paths
(`WorldMap::sendAlongPath`, Z1). Each group comes out of a tunnel's mouth and
walks its path's waypoints along the canyon's floor (Gloamwings fly them) to
the prize. There it lingers, as any group at its site: it bites a pilot inside
the site and turns on a turret that shoots it, within its leash. A wave's
groups never leave on their own. Its Lodebacks eat until their sacs are full
and then carry the ore away out of sight.

**Beaten** when every bug of the wave is dead (by turrets or units) and it has
taken less than the mark; or (C2, the guns' rounds fly real arcs) when its
time is out with no bug of it on the prize: the survivors (a Gloamwing
hovering where no round reaches it) leave, and the prize held.

**Broken through** on either of:
- **the ore it takes**, 300 of the prize (`WorldCanyon::failOre`). That counts:
  - the ore in its Lodebacks' sacs, carried off when one gets away out of sight;
  - what its Shardlings gnaw: 0.5 ore a second each, while on the prize with
    nothing to fight.

  A killed Lodeback's sac goes back into its deposit and stops counting.
- **its time**: the walk down its longest path at its slowest bug's march, plus
  90 s. Past that, one bug of the wave on the prize (within 9 cells of its
  middle) breaks it through.

The fail mark comes first for a hoard of Lodebacks. Wave 5 takes it in about
67 s, with 34 s left on its clock. The time comes first for Shardlings alone:
wave 2 runs out after 97 s, having gnawed about 270.

The director's own groups keep off the canyon zones' prizes: only waves come
for them.

### The waves of The Fork

Path 0 is the North arm and path 1 the East arm. A zone with one path sends
everything down it.

| Wave | North arm | East arm | Bugs | Notes |
|---|---|---|---|---|
| 1 | 4 Shardlings | | 4 | the opening wave; it turns arm with each replay |
| 2 | 6 Shardlings | | 6 | |
| 3 | 5 Shardlings | 5 Shardlings | 10 | both arms at once |
| 4 | | 7 Shardlings, 3 Gloamwings | 10 | the first flyers: AA towers |
| 5 | 2 Lodebacks + 6 Shardlings | 4 Gloamwings | 12 | **mini-boss**: the hoard goes for the ore |
| 6 | 8 Shardlings, 4 Gloamwings | 4 Shardlings | 16 | |
| 7 | 7 Shardlings | 7 Shardlings, 1 Lodeback + 3 Shardlings | 18 | |
| 8 | 5 Gloamwings | 5 Gloamwings, 8 Shardlings | 18 | a flock down each arm |
| 9 | 10 Shardlings | 2 Lodebacks + 4 Shardlings, 5 Gloamwings | 21 | |
| 10 | 3 Lodebacks + 10 Shardlings, 4 Gloamwings | 3 Lodebacks + 10 Shardlings, 4 Gloamwings | 34 | **mini-boss**: the great hoard |

The mini-boss waves are hoards: Lodebacks (120 hp, armour 1) heavily escorted.
They go straight for the ore, and the panel calls them MINI-BOSS. No new kind
of bug was made for them.

## The canyons after The Fork (Z3)

Two more canyons, each in its own region and each with a twist. Both play
the same game as The Fork, with their own ten waves.

### The Drowned Cut (Brine Pans, brood level 2)

**Where.** Brine Pans is the salt flats, so its canyon is flooded. It sits on a
level-2 mesa over the region's north-east (x 70–126, z 21–51 in region
cells):
- **Salt arm.** A long arm from the Salt tunnel in the mesa's west wall winds
  east-south-east to a basin.
- **Gull arm.** A short arm from the Gull tunnel in the north drops south to
  the same basin.
- **Basin.** The prize lies in the basin: five 1,700-ore mounds in a ring
  round an MH well of 6000 (E1; five of 6,000 before).
- **Gorge.** A gorge runs south out to the salt pan.
- **Landmark.** A half-drowned hauler lies at the basin's east shore.
- **Ramps** lead up to the north and south rims.
- **Two ways in.** The Salt tunnel's mouth opens onto the ground outside
  the mesa, so a walker can also go out, round the mesa's foot and up the
  gorge into the basin: a wall across the Salt arm alone is walked round.
  The flooded canyon's second twist; hold the gorge too.

**The twist: the brine.** The zone's `flood` covers the basin and the lower
Salt arm:
- The Brood's **walkers** wade through it at **half speed**. Each canyon think
  (every 0.25 s) renews their slow (`WorldCanyon::floodHold`).
- Its **Gloamwings** fly over it unslowed.
- So the walkers spend longer under the rims' guns, but the flyers do not.
- Pilots' Prospectors are not slowed. Their movement is predicted on the
  client, and a server-only slow would rubber-band them.

The client draws the brine as a sheet of dark salt-green water over the
flood's outline at the floor's height (`AAcBrine`; the MH pool's material,
its sky dimmed). The rims stand over it, so it shows only on the floor.

**Its waves** are walker-heavy, with Lodebacks from the mini-boss wave:

| Wave | Salt arm | Gull arm | Bugs | Notes |
|---|---|---|---|---|
| 1 | 4 Shardlings | | 4 | the opening wave; it turns arm with each replay |
| 2 | 8 Shardlings | | 8 | |
| 3 | 5 Shardlings | 5 Shardlings | 10 | |
| 4 | 3 Gloamwings | 6 Shardlings | 9 | flyers over the brine |
| 5 | 2 Lodebacks + 6 Shardlings | 3 Gloamwings | 11 | **mini-boss** |
| 6 | 10 Shardlings | 6 Shardlings | 16 | |
| 7 | 8 Shardlings, 4 Gloamwings | 1 Lodeback + 4 Shardlings | 17 | |
| 8 | 6 Gloamwings | 6 Gloamwings, 8 Shardlings | 20 | |
| 9 | 12 Shardlings | 2 Lodebacks + 5 Shardlings, 4 Gloamwings | 23 | |
| 10 | 3 Lodebacks + 12 Shardlings, 6 Gloamwings | 3 Lodebacks + 10 Shardlings | 34 | **mini-boss** |

### The Roost (Dust Terraces, brood level 3)

**Where.** Dust Terraces is stepped sandstone with high ledges, where the
Gloamwings roost. Its canyon is a level-2 mesa over the south-west terrace
(x 15–48, z 60–88):
- **Ochre arm.** From the Ochre tunnel in the south-west corner, it winds
  east then north to the basin at (30, 72).
- **Dust arm.** From the Dust tunnel in the north-east, it runs down to the
  same basin.
- **Prize.** Five 2,600-ore mounds in a ring round an MH well of 7500 (E1;
  five of 7,500 before).
- **Gorge.** It runs north out onto the terrace.
- **Landmark.** A level-3 spire rises out of the mesa at the basin's west
  edge, with the roost's nest of chitin on its top.
- **Access.** The Dust vault's knoll joins the mesa by a bridge of level-2
  ground; ramps lead up from the terrace and from the basin's floor to the
  south-west rim.

**The twist: the roost.** One of the zone's entrances is a **roost**
(`"roost": true`): a ledge, not a tunnel.
- **Every Gloamwing** of a wave comes off the roost, a level or more over the
  prize, and flies its path down to the prize over the walls.
- **Walkers** come out of the two tunnels.
- So the flyers come from the one place the tunnels' guns don't cover. The
  AA towers belong by the spire.

The roost's mark is a small nest of chitin (`broodNest`, half size) on the
spire, not a tunnel's mouth. The warnings name it as it is: "14 bugs from
the Dust tunnel and The Roost".

**Its waves** are heavier (level 3) and carry nearly twice The Fork's flyers, all
off the roost. Their pay is × 1.3 (`WorldCanyon::payScale`).

| Wave | Ochre arm | Dust arm | Off the roost | Bugs | Notes |
|---|---|---|---|---|---|
| 1 | 5 Shardlings | | | 5 | the opening wave |
| 2 | | 6 Shardlings | 3 Gloamwings | 9 | |
| 3 | 6 Shardlings | 6 Shardlings | 4 Gloamwings | 16 | |
| 4 | | 8 Shardlings | 6 Gloamwings | 14 | |
| 5 | 2 Lodebacks + 8 Shardlings | | 6 Gloamwings | 16 | **mini-boss** |
| 6 | 10 Shardlings | 6 Shardlings | 6 Gloamwings | 22 | |
| 7 | 8 Shardlings | 1 Lodeback + 5 Shardlings | 8 Gloamwings | 22 | |
| 8 | | 10 Shardlings | 10 Gloamwings | 20 | |
| 9 | 12 Shardlings | 2 Lodebacks + 6 Shardlings | 8 Gloamwings | 28 | |
| 10 | 3 Lodebacks + 12 Shardlings | 3 Lodebacks + 12 Shardlings | 12 Gloamwings | 42 | **mini-boss** |

### The same for every canyon

- **Rules.** The states, the fail mark, the Matriarch's siege, the hold and
  the pay are The Fork's. A wave of a level-3 region pays × 1.3.
- **Frontier pilots.** A frontier defender goes to any canyon whose wave a
  human holds (P1, [frontier.md](frontier.md)).
- **Save.** It keeps each zone's row, sorted by region id; an unchanged world
  writes nothing.
- **Dev line.** `brood zone here ZONE` takes the pilot to a named zone's
  region. The `canyon …` lines work on the zone of the pilot's region.
- **Pictures.** `-AcMmoShotsZ3` (phases 2800-2812) shoots eight:
  1. The Drowned Cut from above;
  2. waders in the brine;
  3. its wave panel;
  4. The Roost from above;
  5. Gloamwings off the roost;
  6. its wave panel;
  7. the map on each, The Drowned Cut;
  8. then The Roost.

## Building (Z4)

The user (2026-10-15): "on the TD area I don't want to have explicit slots
for what and where to build, the players can place whatever anywhere, they
can pack the buildings up as close or far as they want."

- **No build spots.** A pilot places any building anywhere standable in a
  zone (the rims, the floor, the paths themselves) under the ordinary build
  rules only: the footprint (level, standable, nothing in the way), the
  build reach, the price, the floor and the realm's limits. No spacing
  beyond the footprints: guns pack side by side. Turrets and their marks are
  paid as anywhere.
- **The walkers route round** what the pilots build while any way to their
  next path point is open, and **bite through** the nearest of the pilots'
  buildings when there is none (they look every 0.5 s,
  `Brood::breachEvery`). Gloamwings fly over as before.
- **Two cells to funnel, none to wall.** Buildings snap to whole cells and
  the walk keeps 0.35 of a cell off every footprint (`NavGrid::clearance`),
  so the narrowest gap a walker uses is 2 cells: pack guns tight to wall a
  way off (a 1-cell gap is a wall, bitten through), leave two cells to
  funnel a wave past them.
- **A wave always resolves.** A wave's bugs leave by their own tunnel, and a
  leaving bug that can get no nearer its exit (4 s without half a cell of
  headway) goes where it stands. Before, a fleeing Lodeback walked to the
  region's nearest edge, behind The Fork's wall, and stood there until the
  wave's time ran out (B2's 19-minute wave).
- **Frontier pilots** build no turrets in a canyon (P1's defenders fight
  there and patrol the prize); their builders keep their own 6-cell habit
  elsewhere.

## Pay

A wave beaten pays every pilot who fought it, split by the damage each did to
the wave's bugs. That damage is its Prospector's, plus that of the turrets it
built while it was on line (the Matriarch's rule, N3). Each fighter gets at
least a tenth. This comes on top of the kill XP every bug pays as anywhere.

| | Wave n | Wave 10 |
|---|---|---|
| XP | 80 × n | 1,600 |
| Ore | 60 × n | 1,200 |
| MH | 16 × n | 320 |
| Blueprint | | one rare copy each (one of the three with the highest floors) |

B2 ([balance.md](balance.md)) set the pay to 0.4 of Z2's 200, 150 and 40 a
wave, so a held canyon and the Citadel board bring an active pilot to about
twice the mining curve. A level-3 region's zone pays × 1.3 (Z3).

Each fighter is told "YOUR SHARE: THE FORK WAVE 3", with "+180 ore · +48 MH ·
+240 XP" (news `canyon`, "wave/<n>/<MH>[/<blueprint>]").

KSTR tells the whole world two things, in the banner slot:
- **THE FORK HELD**, when the last wave falls ("held", with how many fought it);
- **THE FORK HAS FALLEN**, when a running wave breaks through ("failed", with
  the wave).

## On the screen

- **The zone's banner**: on coming into the zone's region, "THE FORK" over
  "CANYON ZONE · WAVE 3 / 10", with what it is doing.
- **The wave panel**, under the compass (under the boss bar while the
  Matriarch's is up), whenever the zone is in the pilot's region or next to it:
  - the zone's name, and its title:
    - "WAVE 1 · THE OPENING WAVE";
    - "WAVE 5 / 10 · MINI-BOSS";
    - "FALLEN";
    - "HELD".
  - a line saying what it is doing:
    - "7 bugs left · 120 / 300 ore taken";
    - "Next wave in 0:24";
    - "Wave 1 comes in 2:31 · beat it to start";
    - "The Matriarch comes for the turrets";
    - "No waves · the prize is yours · waves again in 31:20";
    - "Waiting for a pilot".
  - while a wave is out, a slim bar of the ore taken against the mark. It is
    amber, and turns red past three quarters.

  The panel is orange, red while a wave is out or the zone has fallen, and
  green while it is held.
- **Warnings** in the toast lane, with the swarm's sound:
  - "THE FORK: WAVE 4 OF 10", "10 bugs from the East tunnel", when a wave comes;
  - "THE FORK: WAVE 3 BEATEN" with the countdown;
  - "THE FORK: THE OPENING WAVE GOT THROUGH".
- **The world map** (M): a ring over each zone's prize, as wide as the zone's
  reach, with three chevrons pointing in. It is orange, and red while a wave is
  out or the zone has fallen. Its tooltip gives the zone, its title and its line.
- **The Journal** (L): each zone's state in the header.

## Dev and shots

The server's dev control (`--dev-control`) takes these lines, all on the first
pilot's region's first zone:

| Line | What it does |
|---|---|
| `canyon send N` | sends wave N now (RUNNING; wave 1 while WAITING is its replay) |
| `canyon fail` | breaks the wave out through |
| `canyon beat` | kills the wave out; its fighters are paid as they fought |
| `canyon hold` | the last wave beaten |
| `canyon reset` | back to WAITING and wave 1 |
| `canyon break S` | the next wave in S seconds |
| `canyon turrets G [A [FIRST]]` | G finished Gatling Guns and A AA towers of the realm beside the zone's paths (the rims first, the floor's edge next, the arms in turn), the first FIRST free places passed over, built by the pilot |
| `canyon place gatling\|sentinel X Y [X Y …]` | finished guns at those region cells, built by the pilot, under the ordinary build rules (a refused place is passed over) |

`brood zone here` (Z1) takes the pilot into The Fork's gorge.

`-AcMmoShotsZ4` (phases 3100-3103) takes one picture: a block of Gatling Guns
packed on The Fork's floor by the prize, two AA towers beside it, wave 3
coming at it.

`-AcMmoShotsZ2` (phases 2500-2525) takes twelve pictures:
1. wave 1 down the north arm;
2. the wave panel;
3. the rim's guns firing;
4. wave 1 beaten;
5. Gloamwings meeting AA fire;
6. THE FORK HAS FALLEN;
7. the Matriarch out of the tunnel;
8. her felling the turrets;
9. the reset;
10. THE FORK HELD;
11. the map;
12. the Journal.

## The siege heard (A2)

While a wave is out near the pilot, a wave music layer rises over KSTR 88.7,
louder with the wave's number; the Matriarch's own layer plays while she
comes for the turrets (FAILED). Both fade out on HELD, and the radio ducks
under them and comes back over 2 s after. KSTR's announcer reads the
canyon's news: a wave coming, a wave beaten, the canyon held, the canyon
fallen with the Matriarch coming, one line each per canyon, queued behind
any narration. Details: [music.md](music.md), "Siege layers".

## Wire and save

- **Protocol 27** (26 is F2's):
  - `Sky::zones`, one per zone: region, zone, state, wave of waves, bugs left,
    the next change's and the wave's end on the world's clock, ore taken and
    the mark, the tunnels as bits, paused, mini-boss;
  - news kind `canyon`;
  - the atlas's `Kind::zone`.
- **Schema 20** (19 was kept for F2, which needed none): the `canyons` table,
  one row per zone: region, zone, state, wave, next. A save from before has
  every zone WAITING.

## Choices

The decisions left these open; each is the simplest that fits them.

- **Wave 1 breaking through while WAITING loses nothing.** No Matriarch comes:
  there is no game to lose yet, and it simply comes again.
- **The fail mark** is 300 ore a wave, whatever the wave.
- **Shardlings gnaw the prize** (0.5 ore a second each), so a wave without
  Lodebacks can still take the mark, and the panel's bar moves. Gloamwings
  don't gnaw; they fail a wave by its time.
- **"Bugs on the prize"** means within 9 cells of its middle.
- **The zone's turrets**, which the siege fells, are every Sentinel and Gatling
  Gun of the realm within the zone's reach of the prize's middle: its
  outline's farthest corner (Z4; The Fork 34 cells, the Drowned Cut 43, the
  Roost 21; before, the farthest build spot and 3). The Citadel's guns are
  never in a canyon.
- **She comes out of a tunnel.** The siege brings the Matriarch out of the
  tunnel of the wave that broke through, not in at the region's edge. Her
  escort fights only turrets, and a pilot that hits it.
- **FAILED lasts at most 10 world minutes.** If she cannot finish (the region
  slept, or she is stuck), every turret left falls at its end.
- **Mini-boss waves are Lodeback hoards**, not a new kind of bug.
- **The reset gives 3 minutes** before wave 1, to rebuild.
- **Offline builders go unpaid.** A defence's damage counts for its builder
  only while it is on line; one away is not paid, as with the Matriarch.
- **No missions point at The Fork yet** (K2 and L1 were optional). The map's
  ring and the panel are the pointers.
