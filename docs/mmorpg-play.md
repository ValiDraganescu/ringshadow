# Playing the MMORPG test mode

How to play Ringshadow's MMORPG test mode as it is on 2026-10-13: one
Prospector in a world of nine regions, run by a server on your own Mac. No
AI units yet; a second human can join on the same Mac ("Two pilots", below).
The design is in [mmorpg.md](mmorpg.md).

## Starting it

| How | What it does |
|---|---|
| The Orchestrator's **Ringshadow-MMO** run | Runs `make mmo`. |
| `make mmo` (repo root) | Builds the server (`make -C unreal/server`) and the game module, then starts the game straight in the mode (`-AcMmo`). |
| Home screen, **ENTER THE REACH** | The first button, highlighted, so Return picks it. Starts the local server and resumes your saved world. On the first run you get a new pilot at the start pad. The line under it says whether a saved world waits. |
| Home screen, **NEW WORLD** | Asks first (**DELETE AND START OVER** or **BACK**). It deletes the saved world: your buildings, your wallet and where you stand. Your pilot key stays, so you start again at the start pad. |

A packaged Mac build has the server inside the app. An editor run uses
`unreal/server/build/autocraft-server`, so build it first.

The world is `unreal/server/worlds/first.json`: The First Reach, a 3×3 grid
of regions with the start region, Prospect Landing, in the middle. A new pilot
lands on the start region's pad with 600 ore and 0 MH.

## Controls

Driving the Prospector (mouse captured):

| Key | Does |
|---|---|
| W A S D or the arrows | Walk |
| Mouse | Look and turn |
| Hold click (or E, Space) | Drill ore at a deposit, strike, repair |
| Click (or E, Space) | Enter your Derrick (pump MH), pick up an unfinished building, place a building |
| E (or click, Space) at the Citadel's door | Open the door (from either side, "E · OPEN"); close it from inside the hall ("E · CLOSE"). Inside the hall you are safe from the Brood. |
| E (or click, Space) at a gun station in the hall | Take that corner gun ("E · TAKE GUN 1 SE"): you see from the gun, the mouse aims it within its arc |
| Click (or E, Space) at a gun | Fire; hold to fire at the gun's rate |
| Esc or F at a gun | Leave the gun (you stay in the Prospector) |
| R + hold click | Repair (a Derrick too) |
| V | First or third person (not at a gun: there you always see from the gun) |
| Wheel | Third person: zoom the camera in (up) or out (down), out to a view over a building site; kept for the next ride |
| B | The build wheel: a ring of the buildings round the middle of the view, the pointer hidden. Move the mouse toward one (its name, price and what it needs light up) and click to hold it out; or hold B, aim, and let go. Esc or B closes it. |
| B, then a number, then click | Hold a building out by its number on the wheel (1 Citadel, 2 Landing Pad, 3 Derrick), then put it down. Esc cancels. |
| B, a gold LEARN slice, click | Learn a blueprint you hold (once: from then on that building costs only its ore). The wheel stays open on it; "DERRICK LEARNED" comes up. |
| B, TARGET DUMMY, click | A practice target: 10 ore, no blueprint. Shoot it (a Citadel's corner gun is good): it counts its hits ("HITS 37"), falls at 0 and stands again 3 s later. |
| Z or X | Pick a level-up card |
| Hold Option | Free the pointer to click a card |
| M | World map (open or close) |
| Esc or F | Leave the Prospector for the top-down view (Esc closes the build menu first) |

In the top-down view, click your Prospector to drive it again. Esc there opens
the pause menu (RESUME, SETTINGS, QUIT TO HOME).

The world map is the world seen from above: every region you have been to
as a satellite picture of its real ground, side by side as they lie, the
others dark with only their outline and name (UNCHARTED). It opens centred on
you. Over the pictures, live: you (a cyan arrow), other pilots, mercenaries
(orange), the Brood in groups (red, with their count), the realm's buildings,
deposits (violet, smaller as they empty), MH wells, the blueprint sites you
know of (gold), the realm's Landing Pads (cyan squares; dim in a region you
have not been to) and the regions' own pads (small dim rings), starfalls and their craters,
your Journal's missions (a ring in their tier's colour), KSTR's places, and
the ringshadow band while it sweeps. Live units, buildings and deposits show
in the region you are in; elsewhere the map shows what you know (sites, pads,
missions, your mercenaries at their fields).

Point at anything for a tooltip: its name and kind, and what is worth knowing:
a deposit's ore left (and "Being eaten by a Lodeback"), a building's builder
and HP, a site's kind, blueprint and whether it is opened, a mission's title,
tier and instructions, a mercenary's name, field and the ore it delivered.

| Key | Does |
|---|---|
| Wheel | Zoom about the pointer |
| Drag | Pan |
| Click a Landing Pad, then FLY | Pick a Landing Pad and call the Dropship to fly there |
| Click a mission or a site | The compass points at it (click again: no longer) |
| Arrows or W A S D | Move the pick between Landing Pads |
| Return | Fly to the picked Landing Pad |
| Esc or M | Close |

The world goes on while the map is open. Nothing pauses.

## The loop

1. **Ore.** Walk to a deposit (the crystal mounds) and hold click to drill.
   A load is about 25 ore.
2. **Citadel.** B, 1, click: 400 ore, paid from the wallet. Stand by it and
   the Prospector welds it up (about 35 s). It must stand on flat open ground,
   off the ore and off the pad's plate. Then walk into it to unload, and the
   ore goes into the wallet.
3. **Derrick.** Face an MH well (a rock mound with a silver pool), B, 3,
   click: 75 ore. The ghost snaps onto the well ahead (the nearest within 4
   cells of a spot 3 cells in front). Weld it up (about 10 s).
4. **MH.** Walk up to the finished Derrick until the dashboard reads
   "Click enter the Derrick", then click. The Prospector goes inside and
   pumps (PUMPING MH, about 3.5 s), then comes out with 20 MH. Walk into the
   Citadel and the wallet's MH goes up ("+20 MH").
5. **Leveling.** Work earns XP: ore unloaded, MH unloaded, welding, repairs.
   In a world XP is a tenth of the RTS game's rate, so a Prospector levels
   over many sessions. Each level offers two cards: Z or X.

The wallet (top right: ore, then MH) is the pilot's own and goes with it to
every region. Today a world builds only a Citadel and a Derrick, and both are
paid in ore, so MH only adds to the wallet and earns XP.

## Ore runs dry

A region holds one or two veins (a tight ring of 3 to 5 mounds) and one MH
well: 7,200 ore in Prospect Landing, 11,000 or so in the regions next to it,
17,000 in the far ones ([regions.md](regions.md), "Scarcity"). A pilot
alone mines the start region dry in about an hour and a quarter (less
with the drill and road skills). A mined-out mound grows back a tenth of its
ore a world day, so a vein refills in ten world days: the world never dies,
but a pilot who stays earns little. Move on: fly or walk to the next
region, or hunt a starfall. Frontier pilots and mercenaries mine the same
veins, so a shared region runs dry sooner; a mercenary at a dry field
stands unpaid until it grows back.

## Starfalls

About once a world day a star falls somewhere in the world. KSTR warns
every pilot a minute before: "STARFALL IN 60", the region, its bearing and
distance, and how far its blast reaches ("get clear"). It is seen from
anywhere in its region: the streak across the sky, the flash, the
shockwave racing out, the ground shaking under you. Its blast flattens
everything within about 7 cells of the impact, buildings and units alike,
and hurts what stands out to 18 (a medium region's quarter); nothing is
spared except the home: the start region's Citadel and 25 cells round it.
Its crater holds 100 to 1,000 ore, first come first served, that never grows
back, and drilling it pays half again in XP ([weather.md](weather.md),
"Starfall").

## The HUD

Laid out so the middle of the view, where the Prospector and the action
are, stays clear (U1; the pieces in `AcProgressHud.h`, `SAcLevelUp.h`,
`HudLanes.h`):

- **Top centre.** The compass, the marks over and under it, and the plate
  of what you aim at. Under them the **boss bar** when the Matriarch is near:
  her name and hp in one line over a slim bar.
- **The banner slot**, under the compass (or under the boss bar): a level
  reached, a raid's warning ("KSTR: BROOD SWARM"), "THE MATRIARCH IS DOWN".
  One at a time; the next waits its turn. While a screen is open (K, J, H,
  L, M, U) it waits, and shows when the screen closes.
- **The toast lane**, upper left under the region's name: everything else
  (a contract, mission or goal done, KSTR's next goal, a find, a load home).
  Three at most, newest on top, about 4 s each; more wait their turn. The
  same news again merges into the one shown ("SURVEY CHIP ×2"), and loads
  home add up: "+24 ORE" three times reads "+72 ORE".
- **The contract panel**, right, over the dashboard: compact by default
  (the titles in one line and the NEXT line); T shows it whole.
- **Q, C, G** over the dashboard left of the Prospector; the supplies' cells
  (1-4) right of it.

## Settings

**SETTINGS** on the home screen and on the pause menu (Esc in the top-down
view) open the same page (U2; `SAcMenu`). Up and Down move the highlight,
Left and Right move a slider or a choice, Return picks, the mouse clicks
and drags. Esc closes it.

| Row | What it does |
|---|---|
| **MASTER, MUSIC, VOICE, EFFECTS, ENVIRONMENT** | Volumes, 0–100 in steps of 5, heard at once. VOICE is the spoken words: KSTR's radio lines and the narrations. EFFECTS covers shots, tools and the units' report-ins; ENVIRONMENT covers wind, rain and hum. MASTER scales all of them. |
| **MOUSE TURN** | The driven unit's mouse turn, 0.25× to 3×. |
| **SCREEN** | FULL SCREEN or WINDOW. |
| **RESOLUTION** | The display's own modes from 1280 × 720 up. Full screen offers sizes up to the display's own, marked NATIVE. A window offers only sizes smaller than the display, so it fits on the screen. A display that lists no modes gets the common 16:9 and 16:10 sizes. |
| **LAG SIMULATOR** | OFF, or two_clients.py's preset: 150 ms each way, up to 20 ms more, 5% of packets lost (`ac.MmoLag`). It holds the game's link to the world server back. The orange line under it says it is for testing. A `-AcMmoLag=` on the command line wins over it. |
| **KEY CARD** | The keys as the HUD's TIP line names them, all at once: on foot, building, the Reach, music. Read only: no rebinding yet. Esc or BACK returns to the page. |
| **OK** | Applies SCREEN and RESOLUTION, stores everything and closes the page. |

The volumes, the mouse turn and the lag switch apply as they change. OK
and Esc both store them. Only OK applies a new screen; Esc keeps the one in
use. Full screen at the display's own resolution is borderless and leaves
the display alone. Full screen at another size switches the display to it,
as the engine does on a Mac.

**Where it is kept.** In the game's config, `GameUserSettings.ini`
(`UAcSettings`, section `[/Script/Autocraft.AcSettings]`), read at start.
The pilot key and the save are not in it: they are files in the save folder
(`pilot.key`, see "Saving"). The engine's own keys hold the screen
(`FullscreenMode`, `ResolutionSizeX`/`Y`). Ours hold the volumes
(`MasterVolume`, `MusicVolume`, `VoiceVolume`, `SfxVolume`,
`AmbientVolume`), `LookSpeed` and `bMmoLag`. A packaged game keeps the file
in `~/Library/Application Support/Epic/Autocraft/Saved/Config/Mac/`. An
editor run keeps it in `unreal/Saved/Config/MacEditor/`.

**A first run** (no file yet): full screen at the display's own resolution,
music 80, voice, effects and the rest 100, the lag off
(`ac::PlayerSettings`, `PlayerSettings.h`).

**Two things the engine does itself.** It copies a window's size into the
settings whenever the window is resized, and it saves the settings when the
game quits. So the stored size is always the size the window really got.

**Tests.**
- `make -C unreal/core-tests FILTER=PlayerSettings` covers the defaults, the
  steps, the resolution lists and the screen's mode.
- `--tests Autocraft.Settings` writes the config and reads it back.
- The flows: `-AcFlowTest=mmosettings` (with `-AcSettingsFirstRun`, on a
  fresh config) opens the page from both places, sets VOICE to 55 and a
  WINDOW at the smallest size (`-AcSettingsLargest` takes the largest),
  stores it, plays in the field, checks the lag switch and the key card, and
  quits to home. `-AcFlowTest=mmosettingsback`, run on the same config after
  a restart, reads them back.

Run both hidden with their own config: `-UserDir=DIR/ -AcRemember`. Hidden
runs store nothing without `-AcRemember`. `-AcMmoShotsU2 -AcMmoShots=DIR`
takes the pictures.

## The Citadels

Since R3 (2026-10-15) every region may have one Citadel, built like the
first (B, 1, click) once you have learned the Citadel from its blueprint
(sold once at any Citadel's shop, learned on the build wheel). A second in the same region is refused: "This region has
its Citadel: one a region". Every Citadel is a full home:

- unload there: the wallet gets the load, the realm the tax;
- fall in its region and the Prospector comes back at the nearest Citadel
  or Hab Dome;
- respec there;
- H opens the Citadel screen on the realm's one wallet: the shop, the
  missions, the defence, the mercenaries (one hired there works the field
  nearest that Citadel) and the realm projects;
- a mercenary is a miner or a guard (N4): HIRE or HIRE A GUARD on the MERCS
  tab, and its GUARD / MINER button switches it any time, anywhere. A guard
  mines as a miner, for the same wage, until THE HUNT is called; then it
  walks to the hunt's ring (on foot, through the openings), holds there,
  fights her with the party, and walks back to its field after. It mines
  nothing while it is away, so its week is a miner's less the hunts it
  answered;
- its four corner turrets fire with the realm's marks.

Frontier pilots never build a Citadel. The goals that send a pilot to a
Citadel mark the one in its region, else the realm's first.

## Realm projects

The realm wallet (every pilot's tax and donations, top right under the
wallet as "REALM") buys a world day (40 minutes) of something the whole
realm feels. Open the Citadel (H, standing where you unload); the REALM
tab shows four cards under the wallet, keys 7, 8, 9 and 0 or a click on
START:

| Project | Effect, realm-wide, for a world day | First price (realm wallet) |
|---|---|---|
| Beacon | Dropship fares half (a quarter from a Spacedock) | 2,000 ore · 100 MH |
| Relay | every pilot's night sight half again as wide (the headlamp as it was) | 2,500 ore · 150 MH |
| Dock Crew | mercenaries mine twice as fast (1/12 of your rate, not 1/24) | 3,000 ore · 200 MH |
| Armoury | every turret of the realm, the Citadel's corner guns too, fights one mark up in each stat (to the cap of 3) | 4,000 ore · 300 MH |

- **Who.** Any pilot starts one; the realm wallet pays, not the pilot.
  Frontier pilots never do. One of each kind at a time; several kinds can
  run together.
- **Price.** About 2 to 4 hours of one mining pilot's tax
  ([balance.md](balance.md)); each time the realm starts the same kind
  again it costs a fifth more.
- **On screen.** A running card says who started it and the minutes left,
  with a bar; the HUD's realm strip names the projects running with a gold
  bar each. Every pilot on line gets a toast and KSTR's line when one
  starts ("THE REALM LIT THE BEACON") and when it runs out ("THE BEACON IS
  OUT").
- **The clock.** A project ends a world day after it starts, on the world's
  clock: while the game is closed too.
- **The server** owns them (`WorldProjects.cpp`, `RealmProjects.h`), saves
  them (schema 22) and sends them in the Progress message (protocol 29).
  Pictures: `-AcMmoShots=DIR -AcMmoShotsR2`.

## Travel

- **Passages.** A region is open land ringed by natural walls: cliff bands
  with rock fields on top, the neighbours' ground beyond them. Where two
  regions meet the walls part in a passage, the same gap from both sides
  (one or two on each shared edge). Walk through it and you are in the
  neighbour: no fade, no stop, the camera and your controls carry on. The
  region's name comes up on the banner as you arrive.
- **The Dropship.** Your Prospector can't fly by itself: the Dropship
  carries it between Landing Pads. Build a Landing Pad (B; level 1, 80 ore
  for the blueprint at the Citadel, two in a region, any pilot uses any) in
  the region you leave and one in the region you go to. Stand on yours
  (within 4 cells of its centre), press M, pick a Landing Pad in a region
  you have been to and press Return. The Dropship comes down onto the pad,
  your Prospector walks up its ramp, and it lifts off, banks through two
  turns, dashes across the sky and comes down onto the other pad, where you
  step out: 16 to 20 s. Aboard, nothing can hurt you. It costs 10 MH and 5
  a grid step, half from a Spacedock (the big pad), nothing with
  Ringwalker; half again while the realm's Beacon burns (a quarter from a
  Spacedock). The pads each region came with are where new pilots come
  down; the Dropship doesn't land there. Off a Landing Pad, or with no
  Landing Pad at the other end, the map says why it can't fly.

## Coming back

The world runs on while you're away (a detached server, or the time it
was down caught up when it starts again). The server keeps a ledger for
every pilot off line, a row for each thing it saw happen, never made up:

- what each of your mercenaries delivered (added up, one row a
  mercenary);
- the frontier pilots' deeds (built, opened, surveyed);
- raids: a raid told on a realm building, a building it took;
- the canyons: waves beaten (added up, one row a canyon), a canyon held, a
  canyon fallen;
- the Matriarch down (who hit her hardest) or back in her nest;
- realm projects started (by whom) and run out;
- the realm weeks that ended (their top hauler); the weeks nobody hauled
  in are one row, its time the last;
- with the time you were away and the realm wallet's change.

The frontier pilots' deeds happen only while the server runs: the time it
was down is caught up for the mercenaries, not for them
([frontier.md](frontier.md) "Server downtime"). A ledger keeps 120 rows at
most; past that the oldest routine row (a frontier deed, a week) goes
first, so a canyon held or the mercenaries' deliveries stay (Q3, "Soak" in
[mmorpg.md](mmorpg.md)).

It is saved with the world (schema 23), and it comes in the first
Progress after you enter (protocol 31), once; then it starts again empty.

- **The home screen.** Under ENTER THE REACH: "Your world ran 6 h 12 m."
  in amber and the most telling things under it ("Slate delivered 1,240
  ore · The Fork held · Sorrel built a Derrick in Cold Fork · 2 more"). The
  server writes these two lines beside the save (`first.away`, one row a
  pilot) on each idle tick and save, so the home screen reads them with no
  connection. They are what the server knew when it last wrote them.
- **The toast.** On entering: WHILE YOU WERE AWAY, the time, the first
  thing and how many more, "L JOURNAL". KSTR reads a welcome: a building or
  a canyon lost or the Matriarch back (`welcome-brood`), else a canyon
  held (`welcome-held`), else the mercenaries' deliveries
  (`welcome-mercs`).
- **The Journal.** L opens on WHILE YOU WERE AWAY while it is unread, the
  first row of the list (Up from the first mission goes back to it): the
  time away, the realm wallet's change, and every row, newest first, with
  how long before your return.

## Standings

Every pilot's work over the realm week (seven world days, 4 h 40 m), the
humans and the frontier pilots alike, counted where the server already sees
it, never made up: **ore** delivered (each unload at a realm building, before
the tax; your mercenaries' deliveries count to you), **bugs** felled (by your
Prospector, or by a turret you built), **waves held** (a canyon wave beaten
that you fought, as the pay split counts the fighters), **buildings** raised
(yours, finished). A frontier pilot's counts are its own work, booked or
live, by the same rules as its pay. It is saved with the world (schema 24),
with the week's start on the world clock; a save from before starts its
week at the load. The server sends each pilot the table in a message of its
own, at most once a second when it changed (protocol 33).

- **The REALM tab.** Under the projects, STANDINGS · THIS WEEK: the top 8
  pilots by ore (then bugs, waves, buildings), the four columns, and your
  own row in gold, after a gap when you are not among them; a frontier
  pilot's dot in its paint with FRONTIER beside its name, a human's pale,
  as the map draws them; the week's time left.
- **The map.** A frontier pilot's tooltip and yours (M) add your rank and
  the leading stat: "3rd · 4,200 ore this week" (ore when there is any, else
  the first count that has some).
- **The week's end.** Every pilot on line gets THE WEEK IS COUNTED: "This
  week's top hauler: Vesper, 18,400 ore" (no hauler: "No ore was hauled this
  week"), and KSTR's `week-counted`: "The week is counted, pilots! See the
  standings at the Citadel." A pilot off line finds the same line in its
  away report. Then the counts start again. A world down for weeks tells
  the end once. KSTR's line is one take for every week: a pilot's name
  (often "Pilot 1A2B") and a number don't read well spoken, so the toast
  carries them.

## The hunt

The Matriarch (N3) is the realm's raid. At level 20 KSTR's goals lead to
**Bring Her Down**: where her nest is, marked, and that THE HUNT is called on
the Citadel's REALM tab.

- **The call.** Any human pilot calls THE HUNT on the REALM tab (beside the
  standings), standing at a Citadel: free, once a world day for the whole
  realm (the next call at dawn). Refused with the reason when she is down
  ("She is down: back in 1 h 59 m": N3's three world days), out on a siege,
  a hunt is already on, or the day's hunt was called. A frontier pilot never
  calls it. Every pilot on line gets THE HUNT IS CALLED and KSTR's
  `hunt-called`.
- **The party.** Chosen at the call: every frontier defender that has come,
  and every guard mercenary of every pilot, on line or off. Each takes a
  place on the ring 30 cells round her nest (6 cells apart, on the side of
  the region's pad) and goes there by the realm's rules: a defender walks
  through the openings, or flies the Dropship between the realm's pads when
  both regions have one and its wallet has the fare and 10 MH over; a guard
  walks. Nobody jumps: one that can't get there in time comes late, one with
  no way comes not at all, one whose Prospector was down at the call comes
  when it is back. At the ring they hold, and fight the bugs that come at
  them. In the First Reach a defender in the next region is there in about
  30 s, one from the far corner in about 2 minutes.
- **Seeing it.** The REALM tab's hunt box: its state (GATHERING, THE
  ASSAULT, SHE FELL, THE HUNT FAILED), the time left, the party (the caller,
  each defender in its paint, each guard with its hirer), AT THE RING, ON THE
  WAY or DOWN. The map (M): a red ring round her nest, the guards and the
  caller as dots ringed red (green at the ring), a defender's tooltip says
  "On the hunt · at the ring". In the world, the party is there to see.
  When all the party is at the ring (or the time is up) KSTR says
  `hunt-gathered` with THE HUNT IS GATHERED.
- **The assault.** At 2 minutes, or when the caller steps inside the ring
  if that is later, they go in together: THE HUNT GOES IN and KSTR's
  `hunt-charge`; the defenders and the guards fight her. The fight is N3's:
  her slam, spit and roar, her pay to every pilot who hurt her by its share
  of the damage (a guard's hits count to its hirer on line, as a turret's
  to its builder), her return in three world days.
- **Her fall.** THE HUNT BROUGHT HER DOWN ("The Matriarch is down ·
  the hardest blows: Pilot 1A2B") to every pilot on line, KSTR's `hunt-down`, N3's banner and
  each share; an away row for every pilot off line ("Pilot 1A2B's hunt in
  Slag Pockets brought her down"). In the standings she counts as one bug
  felled to her hardest hitter, as N3 names them (her killing blow no
  longer counts to whoever struck it).
- **It fails** when ten minutes pass after the call and she stands, or when
  the caller and every one of the party that went in are down: THE HUNT
  FAILED, KSTR's `hunt-failed`, an away row. She stands and the day's call
  is spent.
- **Saved** with the world (schema 25): the hunt on, its party and their
  places, the day's call; a guard's role and its walk with the mercenaries.
  The server sends each pilot the hunt in a message of its own, at most once
  a second when it changed (protocol 35: the Hunt message, the CallHunt
  request, the role on a mercenary).

## Saving

The server saves the world on its own: the changed parts every 5 s, and
everything when it stops (QUIT TO HOME, quitting the game). A crash loses at
most 5 s. You come back where you left, with your wallet, levels, picks and
buildings. Leave the mode with Esc (top-down), Esc again, QUIT TO HOME.

| File | Where |
|---|---|
| The world's save | `~/Library/Application Support/Autocraft/Unreal/mmo/first.sqlite` |
| Your pilot key | `~/Library/Application Support/Autocraft/Unreal/mmo/pilot.key` |
| The game's log | `~/Library/Logs/Autocraft/autocraft.log` |
| The server's log | `~/Library/Logs/Autocraft/server.log` |

Deleting the save is what the home screen's NEW WORLD does. Deleting the key makes
you a new pilot. `-AcSaveDir=DIR` moves both to `DIR/mmo`, and `-AcMmoDir=DIR`
moves them to `DIR`.

## Two pilots

Two humans can play one world (W5, 2026-10-15). Each drives a Prospector of
its own, with its own wallet, levels, skills and buildings. The realm is
shared: its wallet, its Citadel and the other buildings, its projects, the
canyons' waves.

What the second human needs:

| What | How |
|---|---|
| A second pilot key | A second game with a folder of its own: `-AcMmoDir=DIR` (or `-AcSaveDir=DIR`, which uses `DIR/mmo`). The key is made there the first time it joins. Two games sharing one folder would share one key, and so one pilot. |
| The world to join | The first game starts the server as always and names it in a file: `-AcMmoShare=FILE` writes the port, the one-time token and the server's dev-control file, a line each. The second game joins with `-AcMmoJoin=FILE`. It waits up to 180 s for the file, then connects with that port and token. |

On one Mac, for a test:

```sh
# The first pilot: the world server, shared.
UnrealEditor unreal/Autocraft.uproject -game -AcMmo -AcMmoShare=/tmp/ringshadow-join.txt
# The second pilot: its own key, joining.
UnrealEditor unreal/Autocraft.uproject -game -AcMmo -AcMmoDir=$HOME/ringshadow-second -AcMmoJoin=/tmp/ringshadow-join.txt
```

What each one sees:

- The other's Prospector in the realm's blue, with a tag over it: its callsign, `PILOT 2B3C`. Like the frontier pilots' tags, it shows only when nothing is in the way: no ground, no building.
- The other's Dropship flights. The ship comes down on the other's pad, the Prospector walks aboard, the ship lifts off and goes. Where it lands, the ship comes in and touches down. The tag is gone while the other is aboard.
- The realm's news: "PILOT 443E BUILT A HAB DOME", and projects started ("THE REALM LIT THE BEACON"). The world map shows the other's buildings, and the tooltip says who built each one.
- The realm's tax. A load unloaded at the other's Hab Dome pays the realm, and both see the realm wallet go up.
- At a canyon, each is paid its share of a beaten wave, by the damage it did.

Leaving: the second game's QUIT TO HOME (or quitting it) only disconnects it.
It says goodbye on the way out, so its Prospector goes back at once; if the
goodbye is lost, the server lets it go after 10 s without a word from it.
The server goes on with the first game. Joining again with the same folder
brings the pilot back where it left, with its wallet. The first game owns the
server: when it quits, the world stops for both.

Not yet:

- **Two Macs on a LAN.** The game starts the server bound to 127.0.0.1, and a joining game connects to 127.0.0.1. The server program has `--bind`, but the game never passes another address, and the share file names no host.
- **Seeing who is who.** Both humans are in the same blue; only the tag tells them apart. There is no chat and no party.
- **Dev lines for the second pilot.** The server's dev lines act on the first pilot. Only the test harness writes `as 2 LINE` for the second.

The check is `unreal/Tools/two_clients.py`, under the hidden-run slot. It runs
the flow step `mmotwo` (`AcMmoTwo.cpp`) in two games at once, through the lag
simulator (150 ms, 5% loss) on the second. It is headless, or takes pictures
with `--shots DIR` (the `-AcMmoShotsW5` set, `w5-NN-a|b-*.png`).

## Dev flags

Add these to the game's command line.

| Flag | Does |
|---|---|
| `-AcMmo` | Starts straight in the mode (what `make mmo` passes). |
| `-AcMmoNewWorld` | With `-AcMmo`: a new world, as NEW WORLD does. |
| `-AcMmoLag=150,20,5` | Lag on the connection, each way: delay ms, jitter ms, loss %. The console's `ac.MmoLag 150 20 5` changes it while playing; `off` or empty: none. |
| `-AcMmoHour=22` | Holds the world's hour on the server (its `--hour`, 0–24): every region's sight at night and the sky follow it. Without it the world's clock runs: a 40-minute day, about 30 minutes light and 10 night (`ac::WorldClock`), saved with the world. `ac.Hour` does not move the MMO sky. |
| `-AcMmoWeather=storm` | Holds the world's weather on the server (its `--weather`: `clear`, `cloudy`, `rain`, `storm`, or 0–3). Without it the weather follows its schedule on the world's clock, seeded by the world's id, the same in every region. `ac.Weather` does not move the MMO weather. |
| `-AcMmoRingshadow=70` | Starts a ringshadow sweep 70 s after the server starts (its `--ringshadow`: the world's clock moves on to it). Without it the sweeps come at 11:30 and 16:00 world time ([weather.md](weather.md), "Ringshadow sweep"). |
| `-AcMmoStarfall=40` | A starfall lands 40–58 cells from the first pilot that drives, 40 s after it drives, told at once (the server's `--starfall`). Without it about one a world day falls on the world's dice ([weather.md](weather.md), "Starfall"). |
| `-AcMmoClockHour=17` | Moves the world's clock on to that hour at the start and lets it run (the server's `--clock-hour`, 0–24), unlike `-AcMmoHour`, which holds it. |
| `-AcMmoDevXp=55` | A new pilot's Prospector gets this much XP at once (the server's `--dev-xp`). Level 2 is at 503 XP (`Progression::xp(2)`). |
| `-AcMmoServer=PATH` | Dev: starts the local server from PATH instead of `unreal/server/build/autocraft-server`, when the file is there. The build train passes its own server, built from the same tree as the module. |
| `-AcMmoPerfPanel` | Shows the perf panel (FPS, GPU, CPU) in the mode. It is off for players; ⌘D toggles it in the mode and the choice is kept (`ac.PerfPanelMmo`), apart from the RTS's own panel. |
| `-AcMmoShots=DIR` | The picture harness: plays a new world through every moment and saves a PNG of each to `DIR`, with its own pilot and world in `DIR/mmo` ([mmorpg-shots.md](mmorpg-shots.md), `unreal/Source/Autocraft/AcMmoShots.cpp`). |
| `-AcMmoShotsOnly=sf-02,sf-08` | With `-AcMmoShots`: shoots only those moments (names, numbers or a set's prefix and a number: `sf-02-streak-high`, `02`, `sf-2`); the others are passed over, their waits too where the set can ([mmorpg-shots.md](mmorpg-shots.md), "The run"). |
