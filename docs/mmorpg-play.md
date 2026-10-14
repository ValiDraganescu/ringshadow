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
know of (gold), pads (yellow rings; yours cyan), starfalls and their craters,
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
| Click a pad, then FLY | Pick a pad and fly to it |
| Click a mission or a site | The compass points at it (click again: no longer) |
| Arrows or W A S D | Move the pad pick |
| Return | Fly to the picked pad |
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

## Travel

- **Edges.** Walk off a region's edge through an opening and you arrive in
  the neighbour. The region's name comes up as you arrive. Not every stretch
  of edge is open: cliffs and dunes close the rest.
- **Pads.** Each region has at least one landing pad. Stand on one (within 4
  cells of its centre), press M, pick a pad in any region and press Return.
  The Prospector lifts off, flies for 4 s and lands on that pad. Off a pad the
  map says why it can't fly.

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
| `-AcMmoDevXp=55` | A new pilot's Prospector gets this much XP at once (the server's `--dev-xp`). Level 2 is at 60 XP. |
| `-AcMmoServer=PATH` | Dev: starts the local server from PATH instead of `unreal/server/build/autocraft-server`, when the file is there. The build train passes its own server, built from the same tree as the module. |
| `-AcMmoPerfPanel` | Shows the perf panel (FPS, GPU, CPU) in the mode. It is off for players; ⌘D toggles it in the mode and the choice is kept (`ac.PerfPanelMmo`), apart from the RTS's own panel. |
| `-AcMmoShots=DIR` | The picture harness: plays a new world through every moment and saves a PNG of each to `DIR`, with its own pilot and world in `DIR/mmo` ([mmorpg-shots.md](mmorpg-shots.md), `unreal/Source/Autocraft/AcMmoShots.cpp`). |
| `-AcMmoShotsOnly=sf-02,sf-08` | With `-AcMmoShots`: shoots only those moments (names, numbers or a set's prefix and a number: `sf-02-streak-high`, `02`, `sf-2`); the others are passed over, their waits too where the set can ([mmorpg-shots.md](mmorpg-shots.md), "The run"). |
