# The canyon game

Canyon defence in the MMORPG mode (docs/mmorpg-m8.md, Z2). A canyon zone of a
region file (Z1, [regions.md](regions.md) "zones") is a field of rich ore and an
MH well at the end of canyons. The Brood comes down those canyons in waves to
mine it for itself. The pilots build turrets on the rims to hold it. The first
zone is **The Fork** in Cold Fork: a mesa with two arms, the North tunnel's and
the East tunnel's, cut to a junction where the prize lies.

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
taken less than the mark.

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

## Pay

A wave beaten pays every pilot who fought it, split by the damage each did to
the wave's bugs. That damage is its Prospector's, plus that of the turrets it
built while it was on line (the Matriarch's rule, N3). Each fighter gets at
least a tenth. This comes on top of the kill XP every bug pays as anywhere.

| | Wave n | Wave 10 |
|---|---|---|
| XP | 200 × n | 4,000 |
| Ore | 150 × n | 3,000 |
| MH | 40 × n | 800 |
| Blueprint | | one rare copy each (one of the three with the highest floors) |

Each fighter is told "YOUR SHARE: THE FORK WAVE 3", with "+450 ore · +120 MH ·
+600 XP" (news `canyon`, "wave/<n>/<MH>[/<blueprint>]").

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
| `canyon turrets G [A [FIRST]]` | G finished Gatling Guns and A AA towers of the realm on the zone's build spots from FIRST on, built by the pilot |

`brood zone here` (Z1) takes the pilot into The Fork's gorge.

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
  Gun of the realm within the zone's reach of the prize's middle: its farthest
  build spot and 3 cells (The Fork: 27 cells). The Citadel's guns are never in
  a canyon.
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
