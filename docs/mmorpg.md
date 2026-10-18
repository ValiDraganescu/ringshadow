# MMORPG mode

Status, 2026-10-13: a first draft. The first decisions are in. This doc explains
what an MMORPG is, checks what Ringshadow already has toward one, proposes a
direction and lists the decisions to make, in order. "Decided" fills up as
the answers come in. How to play the test mode today: [mmorpg-play.md](mmorpg-play.md).

## What an MMORPG is

MMORPG means "massively multiplayer online role-playing game". Each word
names a part of the design:

- **Massively multiplayer online.** Many players share one world at once,
  over the internet. "Many" ranges from a few dozen in one place to
  thousands on one server. The world runs on servers the studio hosts, not
  on a player's machine.
- **Role-playing.** The player is one character, not an army. The character
  grows: levels, skills, gear. That growth is the main reason to keep
  playing.
- **Persistent.** The world goes on when the player logs off. The character,
  the inventory and anything built in the world are still there next time.

Most MMORPGs are built from the same pieces:

| Piece | What it is |
|---|---|
| Character | The player's one avatar: a class or role, a level, skills, gear. |
| Progression | XP, levels, skill trees, better gear. Slow, long and permanent. |
| The world | Regions with their own danger level. Hubs (towns, stations) where players meet safely. |
| Content | Quests (tasks with a reward), dungeons (hard instanced fights for a group), world events, bosses. |
| PvE and PvP | Fighting the game (player versus environment) or other players. Many games keep PvP to set zones or to opt-in. |
| Social | Groups (a party for one outing), guilds (lasting clubs), chat, friends. |
| Economy | Gathering, crafting, trading between players, a market. Gold sinks that keep prices from inflating. |
| Live service | Servers running all the time, accounts, patches, moderation, anti-cheat, new content every few months. |

Two words come up a lot:

- **Shard.** One copy of the world. When too many players join, a game runs
  several copies side by side and each player lives on one.
- **Instance.** A private copy of one place, made for one group and thrown
  away after. A dungeon is usually an instance.

### What is hard about it

MMORPGs are the most expensive genre to make and to run. The reasons, so we
can design around them:

1. **Servers cost money every hour**, forever, whether or not anyone plays.
2. **The server must be the authority.** A player's machine can be hacked,
   so the server decides every hit, every item and every coin. Cheating is
   the first thing that ruins an online game.
3. **Content runs out.** Players finish hand-made quests far faster than
   anyone can make them.
4. **An empty world is dead.** A world built for a thousand players feels
   broken with ten.
5. **Persistence means migrations.** Every change to an item or a skill
   has to carry over the saved characters of every player.

## What Ringshadow already has

More than it might seem. The game's core idea, "the AI runs the army, you
dive into any unit", is close to an MMORPG already: the player is already
one fighter on a battlefield of hundreds.

| MMORPG piece | In Ringshadow today |
|---|---|
| A character to play | Piloting: any unit, first or third person, with a weapon and an ability ([README](../README.md)). |
| Progression | Pilot leveling: XP for work done, a pick of two upgrades at every level, 216 picks across twelve kinds ([leveling.md](leveling.md)). It resets every game. |
| A living world | AI commanders that build, mine, expand and fight on their own. Day and night, weather on the game clock ([weather.md](weather.md)). |
| Gathering | Stardust Ore and Metallic Hydrogen, mined and pumped by Prospectors. |
| A setting and a voice | Ashfall Reach, the gas giant Ringshadow in the sky, the Oracle, the radio KSTR 88.7 Stardust with its ads ([naming.md](naming.md)). |
| A simulation a server could run | The whole game rules are engine-free C++ in `AutocraftCore`, deterministic: the same inputs give the same game ([PORTING.md](../unreal/PORTING.md)). |
| Somewhere to host | The Ringshadow AWS account, already used for the Windows and Linux builds ([builds.md](builds.md)). |

## What is missing

Everything online. Today the game is one player against AIs on one Mac:

- **No networking.** There is no multiplayer of any kind yet, not even two
  players in one match. This is the biggest piece of work and comes first
  whatever else we decide.
- **No server.** No dedicated server build, no hosting.
- **No accounts.** No login, no player identity.
- **No lasting character.** Levels and picks reset every game. Saves are
  local files.
- **No social layer.** No chat, groups or guilds.
- **No content beyond the battle.** No quests, hubs, dungeons or bosses.

## The direction

What we have decided so far ("Decided", below) gives this shape.

**The player is a pilot.** You make one character, a prospector-pilot who
lives in the ringshadow. You never command an army. You drive machines:
any of the twelve kinds, in first or third person, as you do today.

**Your level is yours for good.** Pilot leveling becomes permanent: the
levels and picks of each kind stay with your character between sessions.
Driving a Longbow for a month makes you a Longbow ace.

**Thousands of pilots share one world.** Ashfall Reach becomes a large
persistent world, far bigger than today's maps, and thousands of players
are in it at once. This is the classic MMORPG scale, and the most
expensive answer on every count: the world has to be split across many
servers ("Thousands in one world", below).

**Players build the cities together.** A city is not handed out finished.
Its pilots raise it: they bring ore and MH, and put in hours of labour
(driving Prospectors to build and repair), and each building goes up when
its share is paid. A city grows in levels, and each level unlocks more: a
Spacedock, a Lab, better machines, a market. Cities are the safe places:
where pilots log in, trade, meet and take on work. A pilot belongs to one
city at a time.

**No-man's land is the fight.** Between the cities lies open ground with
the deposits, the wells and dwellings: outposts, homesteads, derricks a
pilot or a crew can claim and live from. These are taken and held by
force. Cities and crews fight each other over them; that is where PvP
happens, while the cities themselves are safe.

**AI pilots live there too.** The AI commanders of today, who run whole
armies, are replaced by AI characters who are pilots themselves: they
have a character, a city and a level like a player, and they mine, build,
take contracts and fight. They keep the world busy where players are few,
and they fill the gaps at night and in quiet regions. Open: how many,
whether they can be told apart from players, and whether they can lead a
crew.

### Thousands in one world

What that scale asks of the technology, so it is not a surprise later:

- **The world is split across servers.** One server cannot simulate
  thousands of pilots with their machines. The map is cut into regions,
  each run by its own server, and a pilot crossing a border is handed from
  one server to the next. This hand-off is the hardest piece of the whole
  project.
- **Today's simulation does not fit as it is.** `AutocraftCore` simulates
  one whole match, every unit every tick, on one machine. A large world
  needs the rules to run per region, with the regions talking to each
  other.
- **Crowds in one place.** A siege where five hundred pilots meet in one
  region is the worst case: every machine sees every other one. Most
  games of this size cap a region's crowd, slow its tick, or copy a busy
  region into several shards.
- **A database, accounts, a login and a lobby** in front of the servers.
- **Costs grow with the players.** Each region server runs all the time.
  We do not have a cost estimate yet; it needs a measured server first
  (step 1 below), not a guess.

A world of a few hundred players is a useful stop on the way: one large
map, possibly one server, no hand-offs. The design can aim at thousands
and get there in stages.

## The world: a map of regions

The world is one **world map** cut into **regions**, side by side on a
grid. A region is a piece of Ashfall Reach with its own ground, deposits
and wells, and later the piece one server runs.

- **Regions are generated or crafted.** Most are generated like today's
  maps, from a style and a seed, so every player gets the same one. Some
  are crafted: AI agents build them at the developer's instructions with a
  tool made for agents (a region file plus a preview they can look at),
  not a human editor.
- **Two ways to travel.** Drive across a region's edge into the neighbour,
  or fly from a pad to a far region.
- **Build wherever it fits.** There are no fixed plots. A pilot builds
  anywhere the building fits and pays its price to the game.

### The pilot's kind (2026-10-18)

A pilot drives the Prospector, and only the Prospector has a skill tree
today; the server no longer assumes it. The kind is the pilot's own
(`PilotProgress::kind`, saved in its progress blob as `"kind"` only when it
is not the Prospector, so every save from before reads as one), and one
accessor, `WorldMap::pilotKind`, gives it to every spawn of the pilot's
unit: the start pad, the return with its unit gone, the respawn at home,
the dev starfall's victims. The Dropship takes any kind that boards
(`Rules::boards`). Mercenaries and frontier pilots drive
`kMercenaryKind` and `kFrontierKind` (`World.h`), the Prospector
([kind-registry.md](kind-registry.md), "MMORPG: the pilot's kind").

### Cities (proposal, not built yet)

A city is not placed: it forms where pilots build close together. A group
of buildings counts as a city when enough of them stand within a distance
of each other and the group includes a required set of kinds (for
example a Citadel, a Hab Dome and a Spacedock). The distance, the count
and the set are open; they get decided when cities are built.

## The world runs on

The user, 2026-10-14: "The game should progress in background even if I
quit the game (the server should keep running and do its thing as long as
my computer is on)." Built in S2 (docs/mmorpg-m8.md).

- **The server outlives the game.** With a save, the game starts
  `autocraft-server --detached`: a session of its own, so a quit, a crash
  or a closed terminal never takes it along. It keeps the world clock, the
  sky, starfalls, the Foundry and Lab jobs, contracts and missions going,
  and saves every 5 s as before. Leaving the mode (QUIT TO HOME, quitting
  the app) hands the unit back, says goodbye and leaves it running.
- **The run file.** Beside the save, `first.sqlite` gets `first.run`
  (`net::RunFile`, mode 600): the server's pid, port, protocol, build (the
  program's size and modification time), the one-time secret, and when it
  started. The server removes it when it stops. A file whose pid is gone or
  is no world server (a crash, a restart of the Mac) is stale and removed
  when read.
- **Reattach, or replace; never two.** On start the game reads the run file
  (`net::reattachOrReplace`): a live server of its own protocol and build is
  reattached to (its port and secret from the file; the dev flags of the
  new start do not apply). An older one (a rebuild, an updated app) is
  asked to save and stop (SIGTERM, up to 10 s), then the game starts its
  own. The save's lock refuses a second server on the same file anyway.
  NEW WORLD stops the server on the save before it deletes it.
- **Idle.** With no client connected the server stops stepping at 60 Hz:
  every 0.5 s it moves the world's clock on by the real time that passed
  and runs what lives on it (`WorldMap::idle`: the sky, the sites,
  contracts and missions, starfalls, respawns, the rig's jobs, the wells
  growing back, the Brood's director). With no pilot near, the director
  spawns and raids nothing and a region's bugs go two minutes after the last
  pilot left, as at 60 Hz; bugs are not saved, and a catch-up tells no raid.
  The regions' simulations wait, as while the server is
  down: units stand where they were. A datagram wakes it at once (the
  socket's `wait`), and the first pilot on line puts it back to 60 Hz.
  Idle, it saves every 60 s (every 5 s on line) and on stop, and logs an
  `idle stats` line every 5 min (`--idle-stats`; see "Soak" below).
  Measured on first.json (9 regions), no one on line: 0.01 s of CPU
  in 120 s idle (about 0.008% of a core, the resolution of `ps`), against
  0.3-0.5% stepping at 60 Hz as before (0.04-0.08 ms a tick).
- **Asleep, then caught up.** No launchd agent, nothing installed: after the
  Mac restarts, or after STOP, the world sleeps until the game starts it.
  Every save stamps the Mac's clock (`saved_at` in the save's `meta` table;
  no schema step). On start the server catches up the time it was down
  (`WorldMap::catchUp`): the world's clock jumps on that far, at most a day
  (`WorldParams::catchUpCap`, 24 h); the jobs and timers on the clock are
  due at once; starfalls skipped in the gap are not told (as on any jump of
  the clock); the regions resume as saved. Then `WorldMap::offlineWork`
  (the seconds, capped): H2's mercenaries work the gap, their fields
  emptied, the ore to their pilots' wallets, the wages at each dawn
  (docs/prospector-progression.md "Mercenaries").
- **Stopping it.** The home screen shows "World server running · since
  21:40" with STOP while one runs on the save; STOP saves and ends it.
  From a terminal: `autocraft-server --stop <save>` (or a folder: every
  server with a run file in it); exit 0 stopped, 1 none running, 3 still
  running after 15 s.
- **Tests never leave one behind.** Tests, shot and automation runs start
  it attached (`-AcMmoServerAttached`, which `hidden_run.py` adds to every
  run, and the game's own rule for `-AcFlowTest`, `-AcMmoShots` and
  automation); only `-AcMmoServerDetached` (the S2 shot set) runs it
  detached, and that set stops it at its end. `hidden_run.py` names a
  world server still running on the run's folder.
- **Regions on threads: off (2026-10-18).** The world can step its regions
  with a pilot on several threads (`WorldParams::stepThreads`, or
  `AUTOCRAFT_STEP_THREADS` when that is 0: -1 half the cores, at most 8;
  N that many), the world's own work after the steps still on one thread in
  the same order, the same state to the bit. It stays off: a region goes to
  another thread only when its last two steps each took longer than waking
  one costs (8-12 µs), and The First has no two regions that busy at once
  (with a pilot at the nest the whole world steps in 15 µs; docs/perf.md
  "Server", rounds 18-19). Turn it
  on (one line: `stepThreads = -1`) when a world has several crowded
  regions at once; `make -C unreal/server test-tsan` and spreadbench say
  whether it still holds.

## Soak (Q3, 2026-10-15)

The world server alone, as it runs with the game closed: no client on
line, the 2 Hz idle path. Never AIs playing a game. `uv run
unreal/Tools/soak.py run --out DIR --minutes 120` makes the save
(`build/soak-save`, The First: a pilot's rig, two mining mercenaries and a
guard, the Beacon under way, The Fork held, the pilot off line with its
ledger, the five frontier pilots landed), serves it at nice 10 and writes
`DIR/soak.csv` every 5 min. The run of 2026-10-15, 2 h of wall clock on
the M4 Max under a load of about 11 (`unreal/server/build/q3/soak/soak.csv`):

| min | RSS MB | CPU s | save KB | WAL KB | log KB | idle call mean / max ms | ledger rows | deposits ore |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 21.9 | 5.7 | 140 | 0 | 1.2 | - | 2 | 876,805 |
| 15 | 22.1 | 6.7 | 140 | 929 | 1.6 | 0.55 / 4.52 | 9 | 880,990 |
| 30 | 22.7 | 7.0 | 140 | 1,786 | 2.0 | 0.06 / 1.30 | 10 | 879,805 |
| 45 | 10.0 | 7.2 | 140 | 2,728 | 2.8 | 0.06 / 2.01 | 11 | 889,602 |
| 60 | 10.9 | 7.3 | 140 | 3,645 | 3.4 | 0.08 / 4.36 | 11 | 893,792 |
| 75 | 11.5 | 7.5 | 140 | 483 | 3.9 | 0.07 / 2.51 | 11 | 898,022 |
| 90 | 12.5 | 7.7 | 140 | 1,400 | 4.5 | 0.19 / 3.30 | 11 | 902,212 |
| 105 | 13.2 | 8.2 | 140 | 2,318 | 5.1 | 0.21 / 4.33 | 11 | 906,452 |
| 120 | 13.2 | 8.6 | 140 | 3,223 | 5.5 | 0.20 / 3.47 | 11 | 905,667 |

Flat, in numbers: the save stays at 140 KB, and the WAL stays under
4 MB, since SQLite's checkpoint at 1000 pages folds it back (a 23 ms save
at minute 70). The resident memory stays under 23 MB; macOS compressed
the idle pages to 10 MB at minute 40. CPU is 2.9 s in 2 h (0.04% of a
core). The idle call's mean stays under 1 ms and its max under 25 ms. The
ledger stays at most 120 rows, and the log grows 2.2 KB an hour (the
stats and the starfall lines). The ore goes up when a starfall lands
(about 5,400 each) and down with the frontier's and the mercenaries'
booked mining.

Past the 2 h: `build/soak-save SAVE 36` idles the same world 36 world
days (24 h of wall clock) in the server's 0.5 s calls. The idle call's
mean stays 0.5-0.9 ms on the days the frontier works, with no trend. The
starfalls kept grow by about 95 a day (2-3 a world day, kept until their
deposit is emptied, and nobody mines them with nobody on line), and the
save grows by about 8 KB a day with them. On E1's scarce world (the head
after Q3's rebase, 119k ore in the deposits), the same 36 world days hold
the idle call's mean at 0.064-0.070 ms and the save at 120 KB, with
36 starfalls kept (one a world day). The 2 h soak above ran before E1.

**On E1's head** the catch-up check passes the same way: 24 h of 30,
a 0.75 s start, the mercenaries 837-1,017 ore each, the frontier none, the
week once, the ledger 8 to 10 rows.

**The catch-up** (`soak.py catchup`): the save served a minute, then
started as if down 30 h (`--dev-down 108000`). The clock moved 24 h (the
cap), in 4.9 ms, and the start took 0.75 s with the world loaded. Each of
the three mercenaries delivered 2,088-2,218 ore for the gap, under a
day's rate (about 7,110 each at level 30), because their fields ran out. The
frontier pilots booked nothing (docs/frontier.md "Server downtime"). The
realm weeks that ended in the gap (five) were told once, and the ledger
went from 9 to 11 rows. The server test
`runon_catch_up_of_a_day_books_the_mercs_and_stays_bounded` holds the
same rules.

**Fixed by Q3.** Each fix has a test, except the idle saves, which the
soak's WAL column measures.

- A week nobody hauled in was a ledger row every 7 world days (4 h 40 m
  of wall clock). They are now one row, its time the last, and past 120
  rows the routine rows (weeks, frontier deeds) go first
  (`away_ledger_bounded_on_a_long_idle`).
- The idle saves came every 5 s and wrote the WAL at 10 KB a second.
  They now come every 60 s (`ServerOptions::idleSaveEvery`); a crash
  while idle loses a minute at most.
- `server.log` is never cut while it runs. Past 8 MB at start it moves to
  `server.log.1` (`rotateLog`, `runon_log_rotates_past_its_size`).
- The idle path's own timing: the `idle stats` line, and the catch-up
  line's time.

## Single player first, on a local server

The mode starts as a single-player game: one pilot and the AI pilots, in
the persistent world. But the game is an online game from the first day,
and it treats the local server exactly like the production server. There
is no single-player path through the code: **only the host changes.**

- **The server is its own program.** The world logic (regions,
  cities, the AI pilots, the saves) lives in a server program, written
  for production. It is the same program the public world will run.
- **In single player the game starts it on the player's machine.** The
  server program ships inside the game's package (Mac, Windows, Linux).
  The game launches it in the background, connects to it at `127.0.0.1`
  through the same network code it will use for a public world, and
  stops it on quit. Joining a public world later only changes the
  address.
- **The game is only a client.** It sends inputs and requests ("drive
  this unit", "build here") and draws what the server sends back.
  It never runs the world rules itself and never reads the server's
  state.
- **The world is saved by the server**, in its own database. Locally that
  is a file on the player's machine.
- **Releases go on.** The single-player game keeps shipping as it is
  built; the public server comes when it is ready.

Today's RTS mode is different: the Unreal game runs the whole simulation
in its own process (`UAcSimSubsystem`). The MMORPG mode does not reuse
that path. Its game layer draws from what the server sends, which is new
work in the game module. The RTS mode stays as it is.

### Networking: our own, on UDP

The networking is our own code, chosen for efficiency and control:

- **Our own protocol on UDP**, not Unreal's replication. Unreal's
  replication would make the server an Unreal dedicated server: the whole
  engine in every server process, Unreal's object model on the wire, and
  its limits on players per server. Our server is the engine-free C++ of
  `AutocraftCore` grown into a server: it builds with plain clang for
  Linux, carries no engine, and every byte it sends is ours to shape.
- **Not lockstep.** In lockstep every player simulates the whole world.
  That breaks at this size: each client would need the whole world, would
  see all of it (no hiding anything from a cheater), and one slow player
  would stall everyone. The deterministic core still helps: the client
  can run the same movement code to predict its own machine.
- **UDP, not TCP.** TCP delivers in order, so one lost packet holds up
  everything behind it while the game's picture goes stale. On UDP the
  protocol decides: inputs and world updates go unreliable (a newer one
  replaces a lost one), requests and chat go on a reliable channel.

How it works, in the usual way for an authoritative server:

1. The client sends its pilot's inputs many times a second.
2. The server runs the world at a fixed tick and settles every move, shot
   and request.
3. Each client gets a snapshot of only what is near it (interest
   management), sent as the difference from the last snapshot it
   confirmed, so a quiet scene costs almost nothing.
4. The client shows other machines slightly in the past, smoothly between
   two snapshots, and runs its own machine ahead of the server (prediction),
   corrected when the server disagrees.
5. The server judges a shot against where the target was on the
   shooter's screen (lag compensation), within a limit.

On `127.0.0.1` there is no lag, which would hide every bug in prediction
and correction. The network code needs a setting that adds lag, jitter and
packet loss on purpose, and tests that run with it.

[networking.md](networking.md) designs it: the transport (plain UDP
sockets, written by us), the messages, the tick, interest, prediction,
shots and saves. Still open there: the rates, the encryption library and
the public world's database.

## The decisions still open

1. **What persists in the world?** Cities and their levels for sure. Does
   a claim in no-man's land last for weeks, or is no-man's land reset on a
   cycle (a "season")?
2. **Factions or only cities?** Do cities band together into two or
   three sides, or does every city stand alone and make its own alliances?
3. **Can a city be attacked?** Safe always, safe only for newcomers, or
   open in a siege season.
4. **What does a pilot own?** Machines of their own (lost when destroyed,
   or repaired?), paint, a dwelling, a share in the city.
5. **The AI pilots.** How many, how they show up, what they may do.
6. **Does it pay for itself?** The game is CC0 and free. The official
   world's server cost needs a plan: donations, a paid official world, or a
   small official world with most play on self-hosted ones.
7. **Platforms.** The Mac, Windows and Linux builds already exist; an online
   mode would want all three to play together.

## The order of work

Each step ships in the single-player game:

1. **The server and the network.** The server program, our UDP
   protocol, the game as a client of it, launched locally, and a save in
   the server's database ([networking.md](networking.md)).
2. **The world map and regions**: generated and crafted regions, travel
   across edges and by pad, building wherever it fits.
3. **A lasting pilot.** A character saved by the server, levels and picks
   that persist.
4. **AI pilots.** The new AI that plays a pilot character instead of a
   whole army.
5. **Cities** forming where buildings stand close (the proposal above).
6. **More regions** and travel between them.
7. **No-man's land**: claims, dwellings, fights over deposits and wells.

Then the online part:

8. **The public server**: the same server program on a public host,
   accounts and login, other players in the world.
9. **The market, crews and chat.**
10. **Thousands**: regions on separate servers, the hand-off between
    them, copies of crowded places.
11. **Hosting**: the official world on AWS and the server build for anyone
    to host.

## Decided

- **The player is one pilot** (2026-10-13). No commanding an army.
- **AI pilots replace the AI commanders** (2026-10-13), in this mode:
  AI characters who are pilots themselves. Details open.
- **Thousands of players in one world** (2026-10-13), as the goal. How
  the big games get there is still being worked out as we go.
- **Cities built by their players together** (2026-10-13), and they are
  the safe hubs.
- **PvP is over no-man's land** (2026-10-13): the dwellings and the
  resources between the cities.
- **Both official and self-hosted worlds** (2026-10-13): an official
  world on the Ringshadow AWS account, and a server build anyone can run.
- **A world map with regions** (2026-10-13). Regions generated or
  crafted by AI agents with an agent tool; travel across edges and by
  pad.
- **No lots: build wherever it fits**, paying the price to the game
  (2026-10-13, replaces "buildable lots").
- **Cities form from buildings close together** (2026-10-13): a distance
  and a set of buildings, a proposal for later.
- **Single player first, on a local server** (2026-10-13): one pilot and
  AI pilots, on the production server shipped with the game. The
  game keeps releasing in single player until the public server is ready.
- **The local server is the production server** (2026-10-13): the same
  program, started by the game on the player's machine and reached over
  the network like any server. Only the host changes.
- **Our own networking on UDP** (2026-10-13), chosen for efficiency and
  control: an authoritative server on the engine-free core, not Unreal's
  replication and not lockstep.

## Words

| Word | Meaning |
|---|---|
| Authoritative server | The server decides what happens; clients only ask. Stops cheating. |
| Client | A player's copy of the game. |
| Dedicated server | A build of the game with no graphics that only runs the world. |
| Local server | The server program, started by the game on the player's own machine. |
| Instance | A private copy of a place for one group. |
| Lag compensation | The server judges a shot by where the target was on the shooter's screen. |
| Lockstep | Every machine runs the same deterministic simulation and only inputs are sent. |
| Prediction | The client moves its own machine at once, before the server confirms. |
| PvE / PvP | Player versus environment / player versus player. |
| Region | One piece of the world map; later, the piece one server runs. |
| Replication | Unreal's way of copying the server's objects to the clients. |
| Snapshot | What the server sends a client each update: the state of what is near it. |
| Shard | One full copy of the world. |
| Sink | Anything that removes money from the economy (repairs, fees), keeping prices stable. |
| UDP / TCP | The two ways to send data over the internet: UDP sends single packets with no guarantee, TCP a reliable stream in order. |
| Tick rate | How many times a second the server updates the world. |
