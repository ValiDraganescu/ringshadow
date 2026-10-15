# Playing the MMORPG test mode

How to play Ringshadow's MMORPG test mode as it is on 2026-10-13: one
Prospector in a world of nine regions, run by a server on your own Mac. No
AI units and no other players yet. The design is in [mmorpg.md](mmorpg.md).

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
| R + hold click | Repair (a Derrick too) |
| V | First or third person |
| B | Build menu |
| B, then a number, then click | Hold a building out (1 Citadel, 2 Derrick), then put it down. Esc cancels. |
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
3. **Derrick.** Face an MH well (a rock mound with a silver pool), B, 2,
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

## The Citadels

Since R3 (2026-10-15) every region may have one Citadel, built like the
first (B, 1, click) from a Citadel blueprint, sold at any Citadel's shop
(dearer each copy). A second in the same region is refused: "This region has
its Citadel: one a region". Every Citadel is a full home:

- unload there: the wallet gets the load, the realm the tax;
- fall in its region and the Prospector comes back at the nearest Citadel
  or Hab Dome;
- respec there;
- H opens the Citadel screen on the realm's one wallet: the shop, the
  missions, the defence, the mercenaries (one hired there works the field
  nearest that Citadel) and the realm projects;
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
| `-AcMmoStarfall=40` | A starfall lands 40–58 cells from the first pilot that drives, 40 s after it drives, told at once (the server's `--starfall`). Without it the falls come 2–3 times a world day on the world's dice ([weather.md](weather.md), "Starfall"). |
| `-AcMmoClockHour=17` | Moves the world's clock on to that hour at the start and lets it run (the server's `--clock-hour`, 0–24), unlike `-AcMmoHour`, which holds it. |
| `-AcMmoDevXp=55` | A new pilot's Prospector gets this much XP at once (the server's `--dev-xp`). Level 2 is at 503 XP (`Progression::xp(2)`). |
| `-AcMmoServer=PATH` | Dev: starts the local server from PATH instead of `unreal/server/build/autocraft-server`, when the file is there. The build train passes its own server, built from the same tree as the module. |
| `-AcMmoPerfPanel` | Shows the perf panel (FPS, GPU, CPU) in the mode. It is off for players; ⌘D toggles it in the mode and the choice is kept (`ac.PerfPanelMmo`), apart from the RTS's own panel. |
| `-AcMmoShots=DIR` | The picture harness: plays a new world through every moment and saves a PNG of each to `DIR`, with its own pilot and world in `DIR/mmo` ([mmorpg-shots.md](mmorpg-shots.md), `unreal/Source/Autocraft/AcMmoShots.cpp`). |
| `-AcMmoShotsOnly=sf-02,sf-08` | With `-AcMmoShots`: shoots only those moments (names, numbers or a set's prefix and a number: `sf-02-streak-high`, `02`, `sf-2`); the others are passed over, their waits too where the set can ([mmorpg-shots.md](mmorpg-shots.md), "The run"). |
