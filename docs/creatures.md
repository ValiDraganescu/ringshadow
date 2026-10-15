# The Brood: Ringshadow's creatures

The first enemies of the MMORPG mode (docs/mmorpg-m8.md, N1 to N3): alien
bugs native to Ashfall Reach. They are drawn to the places worth being, linger
there, bite a Prospector that walks into their site, chew on the realm's
buildings and eat unguarded ore. One may come, or a swarm. A giant queen, the
Matriarch, nests in Slag Pockets with a horde around her.

This doc is the design N2 (bugs in the world), D1 and D2 (the defences) and N3
(the Matriarch) build from. N1 made the names, the concept art, the models
and the sounds; no gameplay code yet.

## The user's words (2026-10-14)

"I need some enemies while I go around to mine ore and MH. Some alien bugs of
some sort, they are always attracted to the POIs in the game. They come,
linger around and live, if I am in their site they attack. It could be one,
it could be more." "The creatures can attack the buildings so I have to
defend them." "The creatures mine the ore for themselves depleting
unprotected veins (for now after mining the creatures go out of sight and
then they disappear)." "The creatures should have a brood queen, giant, in
one of the regions, not the initial one, lots of creatures there."

## The family

The **Brood** (code `brood`; "brood" is also the side in the core). Four
kinds, each with its own glow so they read apart from the high camera:

| Name | Role | Glow | Size (cells) | Code |
|---|---|---|---|---|
| **Shardling** | Ground swarmer, comes in packs | cold teal | 1.45 long, 0.85 tall | `shardling` |
| **Gloamwing** | Flyer, spits acid from above | violet and magenta | 1.9 wingspan, 1.1 long, flies 2.5 up | `gloamwing` |
| **Lodeback** | Ore eater: fills its back sac at a deposit and leaves | warm gold, opal sac | 2.4 long, 2.4 wide, 1.05 tall (a full sac to 1.9) | `lodeback` |
| **the Matriarch** | The giant brood queen, never leaves her nest | deep violet, teal veins | 12 long with her egg sac, 8 wide, 9.5 tall | `matriarch` |

For scale: the Prospector is 45 hp, walks 3.94 cells a second and its
cutting torch does 5 a strike every 1.07 s in melee reach (about 4.7 a
second), at the ground only (`UnitStats::byKind`, `prospector`). A full
Hardshell pilot has 125 hp and a 70 hp shield (docs/prospector-progression.md).
Buildings have 1 armour; a Hab Dome or a Lab has 400 hp, a Derrick 500, a
Citadel 1,500.

### Look and feel

They belong to this world: dark chitin the colour of the ore rock, with the
ore's opal flecks crusted into the shell, and light from inside. Each is a
hard, matte carapace of big overlapping plates, charcoal with a violet-black
sheen, the glow coming through the plate seams, the eyes and the soft parts.
At night and under the ringshadow band they are lights moving in the dark
before they are shapes. Concept art: `art/models/<kind>/concepts/` (three
directions each, rest pose and attack pose side by side), contact sheets in
`art/models/<kind>/previews/concepts-c1.jpg`.

- **Shardling**: a low, six-legged bug the size of a big dog, two heavy
  mandibles, a ridge of opal crystal quills on its back glowing teal at
  their roots (c1-quill).
- **Gloamwing**: a small armoured thorax, four long translucent violet wings,
  long trailing barbed legs, and a swollen glassy abdomen that hangs under it
  like a lantern lit from inside (c1-lantern). From above it reads as a
  moving violet light.
- **Lodeback**: a crab-like body on four thick legs with two short digging
  claws by the jaw, carrying a glassy sac on its back where the swallowed
  ore shows through, gold-lit; the sac swells and brightens as it fills
  (c1-hermit).
- **The Matriarch**: a towering segmented body raised high, a great fan
  crown of glowing violet spines behind her head (seen from across a
  region), egg pods clustered on her flanks and a huge glowing egg sac
  behind her (c1-crowned).

The user picked these four from the variant page (2026-10-14).

## The kinds

### Shardling, the swarmer

The common bug. It is what a pilot meets at a deposit, a site or a pad.

| | |
|---|---|
| hp | 20, no armour (four torch strikes; two with the Cutting torch perk) |
| Speed | 1.6 wandering, 4.4 chasing (faster than a Prospector: running out of its site works, outrunning it on open ground doesn't) |
| Bite | 2 to a unit, 3 to a building (2 after armour), every 1.2 s, reach 0.3 cells past its radius; ground only |
| Groups | 1 to 3 at a deposit or well in Prospect Landing, 3 to 6 elsewhere, 6 to 12 on a raid, up to 24 round the Matriarch |
| Drawn by | deposits, wells, blueprint sites, pads, buildings (raids), craters |

The arithmetic: one Shardling is no threat (1.7 a second against 4.7), two
cost a fresh pilot about half its hp, five kill it in about five seconds.
That is the point: a pilot reads the group before it walks in. A pack of six
on a Hab Dome takes it down in about 40 s, a Derrick in about 50 s, the
Citadel in about two and a half minutes, so a raid must be answered, but
there is time to get there.

### Gloamwing, the flyer

Flocks hang over sites and buildings at rooftop height. The torch can't reach
them in the air: the AA tower and the gatling gun are the answer.

| | |
|---|---|
| hp | 30, no armour; `air` |
| Speed | 2.0 hovering about its site, 5.0 on the move; flies over cliffs |
| Acid spit | 3 to a unit, 6 to a building (5 after armour), every 1.6 s, range 4 |
| The dive | to spit at a unit it swoops low: from the windup to 0.6 s after the spit it is at torch height and counts as a ground target (`low`), so the pilot can strike it by timing the swing |
| Groups | 2 to 4; never in Prospect Landing's first raid tier (below); with Shardlings on mixed raids |
| Drawn by | buildings first (they are raiders), then pads and sites |

A flock of three costs a pilot about 5.6 hp a second, so a pilot alone
backs away or fights the dives. Against buildings a flock of four does
about 12.5 a second: a Hab Dome in half a minute. That is why the AA tower
(D2: the Sentinel model, air only) exists: at range 7 it outranges the spit
(4), and its missiles (12 × 2 a second) kill a Gloamwing in two volleys.
The gatling gun (D2) hits ground and air at close range: it should reach
air at 4 or more, the spit's range, so a flock attacking it gets shot.

### Lodeback, the ore eater

A slow, heavy forager. It does not hunt: it eats ore, and the ore is gone.

| | |
|---|---|
| hp | 120, armour 1 (the torch does 4 a strike: about 32 s alone) |
| Speed | 1.8, 3.0 fleeing |
| Bite | 8 to a unit, every 2.0 s, reach 0.4 past its radius; only when cornered (below). It never attacks buildings |
| Eats | 1.5 ore a second from a deposit, up to 60 in its sac; the ore leaves the deposit as it eats |
| Groups | 1, or 2 together; with 2 to 4 Shardlings as escort outside Prospect Landing |
| Drawn by | unprotected deposits (below), most of all a starfall crater's |

**Unprotected** means no finished building of the realm within 10 cells of
the deposit, and no Prospector within 8. A Lodeback picks only unprotected
deposits, so building near your ore (a Hab Dome, a Sentinel, a gatling gun)
protects it. Once it is eating, a Prospector walking up doesn't stop it:
the pilot must drive it off.

How it mines and leaves: it walks to the deposit, eats for up to 40 s (its
sac fills, swells and glows brighter, 0 to 60 ore), then walks away along
the shortest way out of every pilot's sight and out of every building's
sight, and vanishes there (despawns, the ore with it). A Lodeback that is
hit stops eating and flees at once with what it has. It is **cornered**
when a unit hits it and it has no way out (its flee path is blocked or it
has fled 10 s and is still being hit): then it turns and bites until it
can run again.

Killing it pays: its sac bursts and the ore inside goes back into the
deposit it came from, if that deposit is still there (else to the nearest
deposit within 10 cells, else it is lost). So a pilot who catches a full
Lodeback gets the vein back.

### The Matriarch, the brood queen

One giant in the world, in her nest in **Slag Pockets** (the world's
south-east corner, two regions from Prospect Landing). She never leaves the
nest on her own; her horde does, and a canyon's failed wave calls her out
(Z2's siege, below). As built: "As built (N3)".

| | |
|---|---|
| hp | 6,000, armour 3 (the torch does 2 a strike: a lone pilot can't kill her; the realm's defences built at the nest can) |
| Speed | 0.8, never more than 6 cells from her nest's centre; she turns to face her target |
| Footprint | radius 3.5 cells; she blocks pathing like a building |
| Slam | 40 to every ground unit within 3 cells in front of her, every 4 s; buildings within reach take 60 |
| Acid spit | 18 to a unit, 30 to a building, range 12, every 3 s, ground and air; it leaves a puddle 2 cells wide that does 4 a second for 4 s |
| Roar | every 30 s in a fight (and once when a pilot first comes within 25 cells): every bug of the nest within 40 cells turns on the roared-at target, and 4 Shardlings hatch from her egg pods (at most 24 Shardlings, 8 Gloamwings in the nest) |
| Mends | 20 hp a second after 20 s without a hit |
| The nest | a ring of 30 cells round her: 16 to 24 Shardlings and 4 to 8 Gloamwings linger there always; the nest refills one bug every 20 s while it is below its count |
| Reward | for N3: a rare blueprint, a burst of 3,000 ore spilled as a crater-style deposit where she falls, and the region's bugs scatter for a world day |

Readable from far: she is 9.5 cells tall, her crown spines glow violet and
pulse with her breath, and her roar is heard across the region.

## Where they come from and what draws them

Bugs are spawned by the server's **brood director**, region by region, and
only where pilots can meet them.

### Points of interest

Each kind weighs the region's POIs; a group picks one by weight (a dice roll
over the region's POIs, not the nearest):

| POI | Shardling | Gloamwing | Lodeback |
|---|---|---|---|
| Ore deposit | 2 | 0.5 | 3, only unprotected |
| MH well (with or without a Derrick) | 1 | 0.5 | 0 |
| Blueprint site, unopened | 3 | 1 | 0 |
| Landing pad | 1 | 1 | 0 |
| A realm building (a raid, below) | 2 | 3 | 0 |
| Starfall crater, cooled | 5 | 2 | 8 |

A POI already held by a group is skipped. A POI within 15 cells of the
Citadel never draws a lingering group (raids aside), so the pilot's home
stays quiet.

### Brood level

Each region has a brood level that sets how many bugs, how often and which
kinds. It goes in the region file (`"brood": 0..4`, N2), with these
defaults:

| Level | Regions | Alive at most | A new group every | Kinds |
|---|---|---|---|---|
| 1 | Prospect Landing | 6 | 90 s | Shardlings in ones to threes, a lone Lodeback |
| 2 | the four next to it | 14 | 60 s | Shardlings, Gloamwings, Lodebacks with escort |
| 3 | the three other corners | 22 | 45 s | as 2, bigger groups |
| 4 | Slag Pockets | 22 outside the nest, plus the nest | 45 s | as 3, plus the Matriarch's nest |

The director runs only in a region with a pilot in it or next to it. When
no pilot has been in or next to a region for 2 minutes, its bugs despawn
(the nest is kept as a count and comes back full).

### Spawn

A new group appears out of sight: at a point on walkable ground (or in the
air for Gloamwings) at least 18 cells from every Prospector (the Prospector
sees 13) and 12 from every building, preferably at the region's edge or a
cliff foot, as if it came from elsewhere. It then walks (or flies) to its
POI. A group never spawns within 25 cells of a pilot that respawned in the
last 30 s.

### Linger, aggro and leash

- **Site.** A group's site is its POI's centre and a radius: 6 cells for a
  deposit, well, pad or site; the building's footprint plus 5 for a
  building; 8 for a crater.
- **Linger.** Inside the site the bugs wander slowly, stop, groom, chitter
  (the idle sounds and poses). A group lingers 3 to 6 minutes, longer at
  night (× 1.5), then leaves: it walks out of every pilot's sight and
  vanishes.
- **Aggro.** A Prospector inside the site is attacked by the whole group
  (Lodebacks excepted). A Prospector that hits any bug of a group, anywhere,
  turns the whole group on it.
- **Leash.** A bug chases at most 8 cells past its site's edge. Past that
  it drops the chase and walks back; it mends to full after 8 s back in its
  site without a hit. A pilot that runs out of the site gets away, bitten.
- **Buildings in the site.** A lingering group ignores a building next to
  it; only a raid attacks buildings.

### Raids on buildings

A raid is a group whose POI is a building: it comes straight in and
attacks it until it falls or the group dies, the bugs turning on any
Prospector that hits them.

- How often: at most one raid per region every 4 minutes (level 1: every
  10 minutes, and only 3 or 4 Shardlings).
- Which building: weighted by its cost; in Prospect Landing the Citadel
  itself (its corner turrets answer, D1) or buildings at least 25 cells
  from it.
- Warning: 20 s before a raid reaches its building, every pilot in the
  region and the ones next to it hears the swarm coming (`broodswarm`) and
  gets a toast: "BROOD SWARM · heading for the Hab Dome · 140 m NE".
- A building that falls is destroyed (N2 decides what remains: a wreck the
  pilot can rebuild at a discount is the suggestion). Bugs never eat
  buildings or carry anything off.

### The ringshadow sweep

The bugs love the dark. While a point is under the band:

- groups there are bolder: their site radius grows by half and their
  leash by 4 cells;
- the director's next group in the region spawns under the band if it can,
  and its POI is chosen among deposits under the band (so the × 1.5 ore
  under the sweep comes with company);
- their glow brightens (× 1.6), which the client draws from the same
  `ac::Ringshadow` band it already computes; nothing new on the wire.

### Starfall

- **The impact** kills every bug within 4 cells and scares every bug within
  20: they flee out of the crater's way for 10 s.
- **The hot crater** (its first 3 minutes) draws nothing: bugs gather at
  its rim and wait.
- **The cooled crater** is the richest POI in the region (the weights
  above): a Lodeback pair with escort heads for it as soon as it cools.
  First come, first served: a pilot who wants the crater's ore races the
  Lodebacks to it, or guards it.

### Day and night

At night groups linger half again as long and the director's interval
shrinks by a quarter. By day nothing changes. The weather does nothing to
them.

### Despawn

A bug vanishes when its group leaves (linger over, a Lodeback with its sac
full or fleeing) and it is out of every pilot's sight; when its region has
had no pilot in or next to it for 2 minutes; never in sight of a pilot. A
dead bug's body stays 6 s (the death pose), then fades.

## XP and rewards

A kill pays XP like damage dealt (docs/prospector-progression.md, "XP
sources"), with these values standing in for a unit's cost: Shardling 20,
Gloamwing 30, Lodeback 50, the Matriarch 2,500. Defences built by the pilot
(D1, D2) share the XP with their builder. A killed Lodeback's sac goes back
into its deposit (above).

## What the defences are tuned against

D1 and D2 set their own numbers; these are the bugs' side of it.

- The **Citadel's guns** (D1) should reach farther than a raid's
  Gloamwings spit (4), on ground and air. As built they are four corner
  turrets, rapid rotary close-in guns (below, "As built (D1, D2)").
- The **AA tower** (D2, the Sentinel model, air only): the Sentinel's 12 × 2
  a second at range 7 kills a Gloamwing in two volleys.
- The **gatling gun** (D2): ground and close air (4 cells or more), about
  15 a second, so it shreds a Shardling pack of six in about 8 s and a
  Lodeback in about 9 s.
- The **Prospector** stays weak: it holds off one to three Shardlings,
  chases a Lodeback off its ore, swats Gloamwings in their dive, and builds
  the guns for the rest.

### As built (D1, D2, 2026-10-14)

The defences are the core's buildings with a weapon (`Defence.h`,
`Simulation::stepTurret`); every number below is in `Rules.h` and `Defence.h`.

| Defence | Weapon | Reach | What it kills |
|---|---|---|---|
| The Citadel's four corner turrets (D1) | rapid rotary close-in guns, 2.5 every 0.25 s each (10 a second), ground and air | 7.5 cells from each turret, on the Citadel's corner pylons | a Shardling in 2 s a turret; with four, a raid of 8 Shardlings and 3 Gloamwings in about 10 s |
| The AA tower (the Sentinel, D2) | missile pods, 12 × 2 a second, air only (unchanged) | 7 from its edge | a Gloamwing in two volleys; it never sees a Shardling, nor a Gloamwing low in its dive |
| The gatling gun (D2) | amalgam rounds, 4 every 0.25 s (16 a second) | ground 6, air 4.5 (past the spit's 4) | a Shardling in 1.25 s, a pack of six in about 8 s, a Lodeback in about 10 s |

- **The Citadel's turrets** are part of every Citadel of a world: built with
  it, no blueprint, no limit. The server arms the realm's Citadels with the
  realm's marks each step (`Structure::marks`, `WorldMap::armCitadels`); a
  Citadel without marks (the strategy game's) never fires. Each turret keeps
  its own target, cooldown and aim (`Structure::guns`, `CornerGun`); only the
  aims go on the wire.
- **Marks** (the DEFENCE tab, H): fire rate +20%, damage +25%, range +1 cell
  each, three of each, from the realm wallet: 250 ore, then 500 + 50 MH, then
  1,000 + 150 MH (`Defence::citadelPrice`). Any pilot buys them at the
  Citadel; refused away from it, when the realm wallet is short, and at the
  last mark. The same marks fit placed turrets (D3, below, paid by the pilot).
- **Bugs turn on the defence that shoots them**: a hit from a building sets
  its group on that building while it stands within the leash
  (`BroodGroup::turret`), before its raid's building. A defence's builder
  earns half the XP of the damage it does (`WorldMap::defenceXpShare`).
- **Prospect Landing's Citadel can be raided now** that it defends itself;
  its other buildings still only beyond 25 cells of it.
- **Dev and shots**: `defence wallet|purse|blueprints|citadel|gatling|aa|air|flyers`
  (`WorldMap::devDefence`); `-AcMmoShotsD` makes the five pictures,
  `-AcMmoShotsD3` the four of the turret upgrades.

### As built (D3, 2026-10-15): turret upgrades

Every placed Sentinel (the AA tower) and Gatling Gun takes marks on
`Structure::marks`, as the Citadel's guns do, three of each stat:

| Stat | A mark | Sentinel | Gatling Gun |
|---|---|---|---|
| Fire rate | +20% shots a second | 1 → 1.6 volleys a second | 4 → 6.4 shots a second |
| Damage | +25% a shot | 12 → 21 a missile | 4 → 7 a round |
| Range | +1 cell | 7 → 10 (its air reach) | ground 6 → 9 |
| Air reach | +0.5 cell (Gatling Gun only) | (none: its range is its reach) | 4.5 → 6, never as far as the AA tower's 7 |

A Gatling Gun's range marks leave its air reach be: the air is the AA
tower's job. Prices, from the upgrading pilot's own wallet, no tax
(`Defence::turretPrice`):

| Mark | Gatling Gun (each stat) | Sentinel (each stat) |
|---|---|---|
| 1 | 100 ore + 50 MH | 150 ore + 75 MH |
| 2 | 300 ore + 200 MH | 450 ore + 300 MH |
| 3 | 800 ore + 500 MH | 1,200 ore + 750 MH |
| All marks | 4,800 ore + 3,000 MH (four stats) | 5,400 ore + 3,375 MH (three stats) |

A whole Gatling Gun is about an hour and a half of pumping MH at a Derrick
(2,000 MH an hour), the same scale as a rig at Mk III: the MH sink of levels
20 to 30, when the canyon waves (Z2) want the guns.

- **The turret card** (U): near a turret of the realm (within 4 cells of
  its edge; the one under the crosshair first, else the nearest) U opens a
  small card: its name, hp, kills and builder, a column a stat with its
  marks as chevrons, now and next, the price, UPGRADE (keys 1-4). Refused:
  too far ("Too far: stand within 4 cells of the Gatling Gun"), the wallet
  short ("Your wallet is short: 300 MH needed, you have 120"), the last
  mark ("Air reach is at its last mark").
- **Any pilot upgrades any turret** of the realm: it defends everyone.
  Every pilot in the region gets the toast "GATLING GUN UPGRADED · Air
  reach mark 2 · upgraded by Pilot 045B" (`NewsKind::upgraded`).
- **On the turret**: a small plate at the front and back of its base, a
  column of three chevrons a stat, lit in the stat's colour (amber fire
  rate, ember damage, ice range, sky-blue air reach) once bought; the card
  draws the same chevrons (`Tools/Editor/models/turret_marks.py`,
  `PoseMarks`).
- **Kills**: each defence counts the bugs it kills (`WorldMap::kills_`),
  saved with it; the card shows them (`ProgressMessage::turretKills`).
- **Saved** with the building (schema 16: `mark_rate`, `mark_damage`,
  `mark_range`, `mark_air`, `kills` on `buildings`). A felled turret loses
  its marks and kills, no refund: a new one in its place starts bare (Z2's
  Matriarch fells a canyon's turrets this way).
- **Wire** (protocol 22): request 19 `UpgradeTurret{turret, stat}`; the
  marks travel with the structure (four 2-bit stats).
- **Fixed with D3**: the client's pose table held nine building kinds, so
  the Gatling Gun (the tenth) was never posed: its head never turned, its
  barrels never spun, no muzzle flash (and the table was written one past
  its end). It is sized from `StructureKind` now (`AcPose.cpp`).
- **Shots**: `-AcMmoShotsD3` (phases 1800-1809): the bare gun beside
  Gloamwings it can't reach, the turret card after seven marks, the lit
  chevrons, the marked gun hitting a Gloamwing past its bare air reach. The
  dev lines `defence air` and `defence flyers` set Gloamwings hovering 5 to
  5.8 cells from a gatling gun's edge (farther on lower ground, past its
  high-ground bonus), a group that never turns on anyone
  (`BroodGroup::still`).

## Server and client (N2)

The server owns every bug; the client only draws, animates and plays
sounds.

### On the server

- The brood is a side in the core simulation: four new unit kinds
  (`shardling`, `gloamwing`, `lodeback`, `matriarch`) with their
  `UnitStats` (hp, armour, speed, damage, cooldown, range, air) owned by
  the brood side, so damage, death, targeting by the Sentinel and the
  defences, and the snapshots reuse what units already have.
- The brood director (`unreal/server/world/WorldBrood.cpp` is the
  suggestion): brood levels, the POI weights, spawn points, groups, linger
  timers, raids and their warnings, the leash, the Lodeback's eating and
  fleeing, despawn, the sweep and starfall rules, the nest. All of that
  stays on the server; none of it goes on the wire.
- Deposits lose ore to a Lodeback on the server, like a Prospector's drill;
  the deposit's amount already goes on the wire.
- Tests: isolated scenarios (one group at one POI, one raid, one Lodeback
  eating and fleeing, the leash), never whole games.

### On the wire

A bug is a unit in the snapshot, like any other, with these fields (most
exist already for units):

| Field | Bits | Why |
|---|---|---|
| id, kind | as units | the model to draw |
| position, heading | as units | |
| height (Gloamwing) | 6 | its hover and its dive |
| hp | as units | the health bar and the hurt flash |
| state | 3 | idle, move, attack, eat, flee, dive, roar, dead: picks the pose loop and the sound |
| target id | as units | where the attack pose and the spit's bolt aim |
| attack counter | 4 | goes up by one each strike, spit or slam, so the client plays every one even when a snapshot is lost |
| sac fill (Lodeback) | 4 | 0 to 15: the sac's size and glow |

And two events in the world's message stream (reliable, like the starfall's
warnings): **raid incoming** (region, building id, bearing, seconds to
arrival) for the toast and the swarm sound, and **Matriarch roar** (region,
position) so every pilot in earshot hears it, in sight or not.

About 12 bytes per bug when it moves; a busy region of 24 bugs at 20 Hz is
under 6 KB/s before deltas, less with them.

### On the client

- Draw each kind from its model (`/Game/Models/<kind>/`, one instanced set
  per kind and part, as units are); no team colour: the brood's glow
  colour is fixed per kind (its glow material).
- Animate with a pose function per kind (`AcPoseBrood.cpp` is the
  suggestion), from the state, speed, heading, target and attack counter
  (below).
- Play the sounds below on the state's edges and the attack counter.
- The health bar, a red outline when a bug targets the pilot, and the
  toasts.

### As built (N2, 2026-10-14)

What N2 built, and the choices the design left open.

- **Core.** `UnitKind` gets the four kinds appended (16 kinds: the wire's 4
  bits are full; a fifth Brood kind widens them). `UnitStats::byKind` holds
  their hp, armour, top speed, bite and range; `Brood` (Brood.h) the rest:
  wander and march speeds, building damage, XP, the dive, the Lodeback's
  meal. `Unit::bug` carries the wire's fields and the director's orders;
  `Simulation::stepBug` carries the orders out each step: the scare, the
  chase within the leash, the raid, the Lodeback's meal, flight and
  cornered bite, the way in, the way out, the wander and the mending. A
  bite goes through `Simulation::hit`, so the pilot's shield charge and
  Hardened works stop it as any hit. A hovering Gloamwing is in the air
  (`Unit::airborne`): the torch reaches it only in its dive. A world with a
  Brood side never declares a winner.
- **Bodies.** A dead bug lies 6 s as a body (`Bug::State::dead`, hp 0, out
  of every fight), told once as a death, so the client plays the death pose
  from the snapshots.
- **The director** (`server/world/WorldBrood.cpp`). Every step: the hits
  on bugs (the hitter's XP: the damage's share of the bug's hp × its XP
  value; the group turns on it), a dead Lodeback's ore back, a starfall's
  impact; a pilot's kill counts for its hunt missions (K2's
  `WorldMap::broodKill`; `WorldParams::huntMissions` is on now). Every
  0.25 s: each group's prey (a Prospector in its site),
  orders, leaving and taking away out of sight, the raids' warnings, the
  next group. `WorldParams::brood` (`--brood 0`) turns it off.
- **Choices.**
  - Group kinds by level: level 1, a Shardling group (1–3) 4 times in 5,
    else a lone Lodeback; levels 2–4, Shardlings half the time, Gloamwings
    and Lodebacks a quarter each. Groups at levels 3–4 are 4–8 Shardlings
    or 3–4 Gloamwings. A raid off level 1 brings 2–4 Gloamwings with its
    6–12 Shardlings, in one group.
  - A raid is the director's group whose POI came out a building: buildings
    are POIs only while the region's raid clock allows one; their weight is
    the kind's weight × the building's cost / 100.
  - A group on its way marches: a Shardling 2.8 cells a second (between its
    1.6 wander and its 4.4 chase), a Gloamwing at 5, a Lodeback at 1.8.
  - A hit from outside the leash turns the group, but the leash still
    holds: a bug chases at most 8 cells past its site's edge.
  - "Out of sight" for taking a bug away: 16 cells from every Prospector and
    14 from every realm building's edge.
  - A hot crater draws nothing (no waiting at the rim).
  - A raid is told from its swarm's distance at the march speed: at once if
    it is under 20 s away. The toast and the swarm's sound go to the pilots
    in the raid's region and the four next to it.
  - A fallen building is gone (no wreck yet); the realm loses it.
- **The opening's rule.** In a level-1 region (Prospect Landing) the
  director holds while any pilot in it or next to it is on the first ten
  minutes' chain, and its groups never linger within 25 cells of the start
  pad nor 15 of a Citadel. The chain's places stay calm until it is done.
- **Wire** (protocol 17, after K2's 16). `Unit::bug` last in the unit's fields: the state
  (3 bits), the height (6 bits, 0.1 cells), the strikes (4 bits) and the
  sac (4 bits); the orders never leave the server, and a bug's reload is
  left out of its snapshot. A bug on the driven unit is ranked with what
  fights it (every snapshot). The raids and the Matriarch's roars ride the
  sky (`net::Sky::raids`, `roars`), in every snapshot while they last, like
  the starfalls; the roars stay empty until N3.
- **Client.** `AcPoseBrood.cpp` (the four poses), the renderer's red
  outline for a bug on the pilot, the life bar (a Gloamwing's at its
  height), the glow × 1.6 under the band (`FAcPoseContext::Sweep`), the
  sounds (`FAcAudioRules`: a strike sound on each strike, a hurt cry, the
  chitter, the deaths; a Gloamwing's wings and a Lodeback's chewing as
  loops, the nearest three), and `AAcBrood` (AcBrood.h): the raid's KSTR
  toast, "KSTR: BROOD SWARM" over "heading for the Hab Dome  ·  140 m NE",
  and the swarm in the ear.
- **Dev and shots.** The server's dev control takes `brood linger`,
  `brood attack`, `brood dive`, `brood lodeback`, `brood raid` and
  `brood clear` (`WorldMap::devBrood`); `-AcMmoShotsN2` stages its five
  pictures with them.

### As built (N3, 2026-10-15): the Matriarch and her nest

Her numbers are `ac::Matriarch` (Brood.h); the core steps her
(`Simulation::stepMatriarch`), the server keeps her nest
(`server/world/WorldNest.cpp`).

- **The nest.** A region file's `"nest": {"at": [x, z]}` (docs/regions.md,
  "nest"). Slag Pockets has it on its east plateau at [122, 44], 50 cells from
  its pad. The build puts down the `broodNest` doodad there:
  - a ring mound 12 cells across, of chitin plates, dark rock and opal ore;
  - egg pods on its flanks and seven spines up to 5 cells round its rim;
  - walked over: she stands in its bowl.
  
  The model is `Tools/Editor/models/brood_nest.py`, from the concept
  `art/models/broodNest/concepts/c1-mound.jpg`.
- **Her horde.** While a pilot is in her region or next to it, she stands at
  the nest's centre with her horde round her, on top of the region's cap of
  22:
  - four packs of Shardlings 11 cells out, 16 to 24 in all;
  - 4 to 8 Gloamwings over her.
  
  Their counts are drawn once. One hatchling every 20 s keeps the numbers up.
  A pack turns on a pilot inside its 6-cell site and chases within the
  nest's 30-cell ring.
- **Her foe.** Whoever hit her last, else the nearest Prospector, else a
  building of the realm, within her 6-cell tether plus her spit's 12.
- **Slam.** Both forelimbs rise over 0.8 s, then crash down on every ground
  foe within 3 cells of her edge in front of her: 40 to a unit, 60 to a
  building, before armour. Every 4 s.
- **Spit.** At ground or air within 12 cells: 18 to a unit, 30 to a
  building. It leaves a puddle 2 cells wide that does 4 a second for 4 s
  (`Simulation::puddles`). Every 3 s.
- **Roar.** Once when a pilot first comes within 25 cells, and every 30 s in a
  fight. She rears for 2 s. Every bug of the nest within 40 cells turns on her
  foe and runs ×1.3 for 10 s (`Bug::rallyUntil`). 4 Shardlings hatch from her
  pods, the nest holding at most 24. The roar goes on the wire
  (`Sky::roars`, told 4 s).
- **Mending.** She mends 20 a second after 20 s unhit.
- **Seen from far.** Every pilot within 60 cells is sent her, in sight or not
  (`Interest`), so her crown reads over the nest's rim.
- **Her death.**
  - XP as for any bug, by each hit's share of her 6,000 hp × 2,500.
  - Every pilot who hurt her, with its Prospector or a defence it built,
    gets its share of 3,000 ore and 1,500 MH by damage (5% at least) and one
    rare blueprint copy (one of the three with the highest floors).
  - Every pilot hears "THE MATRIARCH IS DOWN" (`NewsKind::matriarch`
    "down"); each fighter gets "YOUR SHARE OF THE MATRIARCH".
  - Her horde scatters with every group of the region, and no new group comes
    for a world day.
  - **She comes back three world days after her fall** (`Matriarch::returnDays`;
    2 hours of the world's clock), saved across restarts (schema 17,
    `nests`). The nest grows back from empty, one hatchling every 20 s, and
    every pilot hears "THE MATRIARCH STIRS".
  - A region left alone two minutes keeps the nest as its counts: it comes
    back full and she whole.
- **The siege hook (for Z2).** `WorldMap::matriarchSiege(region, at, radius,
  escort)`: she leaves the nest with `escort` Shardlings.
  - From her own region she walks there; to another region she comes in at
    its edge out of sight.
  - She fells every Sentinel and Gatling Gun of the realm within `radius`,
    nearest first, her escort on the same one.
  - Then she goes home: to the nest, or out at the edge to be back at the
    nest when its region next wakes.
  - Not wired to waves yet (Z2 does that).
- **Hunt.** One perilous hunt in three is "The Matriarch" (docs/missions.md).
- **Wire** (protocol 24):
  - the World message's regions carry `nest`;
  - `NewsKind::matriarch` ("down", "share/<MH>/<blueprint>" with the ore in
    `value`, "back"); the news kind went from 4 to 5 bits, since the
    sixteenth kind filled 4;
  - her last strike rides in `Bug::sac` on the wire's own steps: 20 a slam,
    40 a spit (`Matriarch::slammed`, `spat`).
- **Client.**
  - Her pose (`AcPoseBrood.cpp`): the breath; the slam's rise and crash; the
    spit's draw and lunge; the roar's rear, the crown fanning, the pods
    flaring; the death's sink and the crown folding.
  - Her slam's dust and rings and her spit's glob and green puddle
    (`UAcEffects::MatriarchSlam`, `MatriarchSpit`, from `AAcBrood`).
  - Her strike sounds (`matriarchslam`, `matriarchspit`).
  - Her roar heard across her region and faintly next door, the view shaking
    within 35 cells for its 2 s (`AAcBrood`, `UAcEffects::Rumble`).
  - The boss bar at the top of the HUD (`SAcProgressHud::FBoss`), within 35
    cells or while she is wounded, flaring while she roars.
  - The toasts.
- **Choices.**
  - She stays within her tether (6 cells), so she does not block paths the
    way a building would: she is a unit of radius 3.5.
  - The nest's mound has no solid feet.
  - The rewards go to wallets, not a spilled deposit, so every fighter gets
    its share.
  - A defence's damage counts for its builder while online.
  - One giant in the world: the first region with a `nest`.
- **Dev and shots.** The server's dev lines:
  - `matriarch here D` takes the first pilot to her region, D cells off;
  - `near D [BEARING]`;
  - `calm` (the horde still, her spit held);
  - `roar`, `slam`, `hurt SHARE`, `down`, `back`, `siege`.
  
  `-AcMmoShotsN3` (phases 1900-1911) takes five pictures:
  1. the nest from 55 cells;
  2. her roar with the horde and the boss bar;
  3. a slam;
  4. the bar with her wounded;
  5. her down with the toasts.

## Models and how to drive them

`unreal/Tools/Editor/models/<kind>.py`, meshes in `/Game/Models/<kind>/`,
catalog entries in `ModelCatalog.json`. Units face +X. Each moving part is a
rigid part that pivots at its root; the pose code finds it by name.

What N2's pose functions drive (the part names are the models'):

- **Shardling** (`body`, `head`, `mandible_0`/`_1`, `leg_0`…`leg_5`, the
  quills on the body): idle: the body bobs, the mandibles twitch apart;
  move: a tripod gait (legs 0, 2, 4 swing against 1, 3, 5; 0 to 2 are
  the left side front to back, 3 to 5 the right; a yaw swing of
  about 25° and a lift), its speed with the ground speed; attack: on each
  counter tick the body lunges 0.3 cells forward and back over 0.25 s, the
  head dips, the mandibles open wide and snap shut; hurt: a flinch back;
  death: it rolls on its side, legs curl in, the glow fades over 2 s.
- **Gloamwing** (`thorax`, `head`, `abdomen` (the lantern), `wing_0`…`_3`,
  `leg_0`…`_3`): wings flap fast (a sine on their roll, about 18 Hz, with
  the two pairs out of phase); the body bobs at its height; the abdomen
  sways behind; move: it pitches 15° nose-down; dive: pitches 35° down and
  drops to its dive height, the abdomen flares bright on the spit; death:
  wings stop and it falls, tumbling.
- **Lodeback** (`body`, `head`, `jaw`, `claw_0`/`_1`, `leg_0`…`_3`, `sac`):
  a slow four-leg walk; eat: the head and claws dig in a loop and the jaw
  grinds; the sac's scale goes from 0.55 to 1.0 with the sac fill and its
  glow with it; bite: the head thrusts and the jaw snaps; flee: a faster
  walk; death: the sac bursts (it scales to 0 with a flash of gold) and the
  body sinks.
- **The Matriarch** (`abdomen` (the egg sac), `body`, `head`, `jaw`,
  `crown`, `forelimb_0`/`_1`, `leg_0`…`_5`, `pods`): idle: a slow breath
  (the body and the egg sac swell by 3% at about 0.2 Hz, the crown's glow
  with them); slam: both forelimbs rise over 0.8 s and crash down; spit:
  the head draws back and lunges; roar: the body rears up 1 cell, the crown
  fans open (scale out) and the jaw gapes for 2 s; death: she sinks, the
  crown folds, the glow dies over 4 s.

## Pictures

Hidden runs of the model row (`unreal/Tools/Editor/make_brood_lineup.py` makes
the two lineup levels; the commands are in its docstring). Each bug at rest
on the row's floor under its sun, and the lineups beside the Prospector for
scale (the Prospector is the right-hand model in both):

- [shardling.jpg](creatures/shardling.jpg), [gloamwing.jpg](creatures/gloamwing.jpg)
  (perched: the pose lifts it to its hover), [lodeback.jpg](creatures/lodeback.jpg)
  (its sac full), [matriarch.jpg](creatures/matriarch.jpg)
- [lineup.jpg](creatures/lineup.jpg): all four and the Prospector
- [lineup-small.jpg](creatures/lineup-small.jpg): without the Matriarch

N1b painted them (`Tools/Editor/make_brood_materials.py`): the egg pods and
sacs are a lit glassy shell over a glow from inside, deepest through the middle,
crossed by teal veins; opal chitin (quills, crystals, the ore crusted on the
shells) shifts teal, violet and gold with the view; wings and the Lodeback's
sac are tinted glass, lit at the rim, with what is inside showing through. The
Matriarch's crown is dark spines with a lit violet core, a violet membrane
between them and teal tips; her nest is a mound in three tiers of plates with
ore veins glowing in the seams, opal crystals and clusters of pods.

They are blockout-grade models from the concepts (pipeline steps 0, 1, 6, 7);
the turnaround painting and refinement (steps 2 to 5) can follow when N2 shows
them in the game's own camera.

## Sounds

Made with ElevenLabs (`Tools/AudioGen/brood.manifest.json`), every take kept
in `art/models/<kind>/sounds/`, the picks in `unreal/Resources/Sounds/sfx/`
under the game's names (`sfx/<name>_<n>`). Checked offline with
`Tools/Audio/check_sounds.py`, never played aloud.

| Name | When |
|---|---|
| `shardlingidle` | a lingering Shardling now and then (a few times a minute a group, not each bug) |
| `shardlingbite`, `shardlinghurt`, `shardlingdeath` | on its bite, when hit, when it dies |
| `gloamwingidle` | the wing buzz, a loop at each flock |
| `gloamwingspit`, `gloamwinghurt`, `gloamwingdeath` | its spit, hit, death |
| `lodebackidle` | a lingering or walking Lodeback now and then |
| `lodebackchew` | a loop while it eats |
| `lodebackbite`, `lodebackhurt`, `lodebackdeath` | |
| `matriarchidle` | her breath, a loop at the nest |
| `matriarchroar`, `matriarchslam`, `matriarchspit`, `matriarchhurt`, `matriarchdeath` | |
| `broodswarm` | a raid coming: the 20 s warning, from the raid's bearing |

`Tools/AudioGen/brood_pick.py` copies every take to `art/models/<kind>/sounds/`
(`broodswarm` with the Shardling's), and levels the picks (to about −22 LUFS
for idles and loops, −15 for bites, spits, hurts and deaths, −9 for the
Matriarch's roar, the gain capped at a −1 dBFS peak). Two to three picks a
name; one for `lodebackhurt` (the other takes were near silent) and
`matriarchidle` (the other jumped at its loop seam).

Importing them into `unreal/Content/Audio/Sfx/` and `Sounds.json` comes
with N2, when the game plays them.
