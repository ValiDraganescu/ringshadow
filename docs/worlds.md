# Worlds

A world of the MMORPG mode ([mmorpg.md](mmorpg.md), "The world";
[mmorpg-m2.md](mmorpg-m2.md)) is a JSON file in `unreal/server/worlds/<id>.json`:
region files ([regions.md](regions.md)) side by side on a grid. The server runs
one with `autocraft-server --world <file>`.

Code: `unreal/server/world/WorldMap.h` (load, check, run), tests in
`unreal/server/tests/WorldTests.cpp`, `make -C unreal/server bench` times it
(`build/worldbench`).

## The file

```json
{
  "id": "first",
  "name": "The First Reach",
  "grid": [
    ["../regions/ashen-shelf.json", "../regions/cold-fork.json", "../regions/glass-flats.json"],
    ["../regions/rust-hollow.json", "../regions/start.json", "../regions/tern-ridge.json"],
    ["../regions/dust-terraces.json", "../regions/brine-pans.json", "../regions/slag-pockets.json"]
  ],
  "start": {"region": "start", "pad": "landing"}
}
```

- `grid`: rows north to south, each west to east; a cell is a region file
  path relative to the world file, or `null` (no region there).
- `start`: a region id and the name of a pad in it: where new pilots appear.

## Checks at load

Any problem stops the server (exit 2, the problems on stderr):

- the JSON, unknown keys, rows of unequal length;
- every region file readable, parsed and valid (`region::validate`), its id
  used once;
- every region has at least one pad;
- every opening on an edge shared with a neighbour overlaps one of the
  neighbour's on the facing edge (the convention: the middle 40–60% of
  every edge, `[58, 86]` on north and south, `[39, 57]` on east and west
  for a medium region);
- the start region and its pad exist.

## The rules

- Every region is wild ground: the map's ore and wells, no bases, no AI, one
  player (the pilots' side). Every region steps every tick.
- A new pilot appears on the start pad driving a new Prospector, with a
  wallet of 600 ore and 0 MH. The wallet is its side's bank in the region it
  is in (the snapshot's `players[0]`), and goes with it, as do its levels.
- Edges: the driven unit crosses once its centre is within 2 cells of an
  edge, inside an opening that meets the neighbour's, moving out (or steered
  out while pressed against it; a Prospector stops 1.56 cells from an edge).
  It comes in 2.5 cells inside the neighbour at the same point along the
  edge, with its load, hit points and levels. Elsewhere the edge is a wall.
- Flights: a Prospector within 4 cells of a pad's centre may fly to any
  region's pad (by region id and pad name). It lifts off, 4 s pass, it lands
  on that pad. Refused away from a pad, in flight, where another pilot
  drives, or when the wallet can't pay the flight's MH (10 + 5 a grid step;
  docs/prospector-progression.md, "MH sinks").
- One pilot drives in a region at a time (the core drives one unit a
  region); a crossing into a region where another pilot drives stops at the edge, a
  landing there circles until the ground is free.

## In the game

The MMORPG mode (the home screen's ENTER THE REACH, or `-AcMmo`) starts
`autocraft-server --world unreal/server/worlds/first.json`; the game no
longer starts it with `--map`. The pilot starts seated in the Prospector on
Prospect Landing's pad.

The world lasts (docs/mmorpg-m3.md, S4): the game passes `--save
~/Library/Application Support/Autocraft/Unreal/mmo/first.sqlite` and joins
with the pilot key it keeps in `pilot.key` beside it (made once, 128 random
bits; the logs show only its first 8 hex digits). ENTER THE REACH resumes:
the same region and spot, the same Prospector, buildings and wallet.
NEW WORLD asks first, then deletes that save (and its `-wal`
and `-shm` files, each by name) and starts over with the same pilot, new at
the start pad (`-AcMmoNewWorld` with `-AcMmo`). A save that does not open
(not a database, held by another server) shows the server's reason and its
log on the banner. Tests keep their own key and save: `-AcMmoDir=PATH`, or
`-AcSaveDir=DIR`'s `mmo` folder.

- A RegionChange (over an edge, or a landing) gives the client a new mirror
  of the new region: the prediction and the blending start over, and the
  game rebuilds on it (terrain, doodads, minimap, fog: `OnGameStarted`). The
  pilot keeps the seat: the view holds where it was until the new region's
  first snapshot shows the unit, then follows it. Nothing of the old region
  stays.
- The banner at the top shows the region the pilot is in and its style, or
  "Flying to …" while in flight. The top bar counts the pilot's units (no
  supply count in this mode).
- Each pad is drawn as a round foundation plate 4 cells across its centre
  (`region::padReach`), the reach a flight leaves from; the generated
  base's start plates are not drawn in a region. Server and client measure
  "at a pad" with one rule (`region::nearestPad`, `net::padsOnGround`).
- M opens the world map (in place of the command map): the grid of regions
  with their names, styles and pads, the pilot's region ringed. Standing at
  a pad, click a pad and FLY (or Return); away from a pad it says how far
  the nearest pad is. M or Esc closes it. The unit stands while it is open.
- Only the player gives the unit back (Esc): a region change never does,
  and the server's seat after a landing is taken only then, so an old
  snapshot never takes the unit back again. A view held for travel gives
  way to the top-down camera after 15 s.
- A flight is shown (`AcPilotFlight.h`; only the picture, the server's
  4 s stay the rule): the Prospector rises off the pad (1.5 s, dust and a
  thrust glow), the camera goes up behind it and pulls back; in transit the
  region falls away under it as the camera turns slowly round it, dimming
  near the landing ("Flying to ..." on the banner). With the new region's
  first snapshot it comes down onto the pad (1.5 s) and the camera settles
  into the pilot's view; the input is held until it touches down. A late
  landing keeps the transit view moving, up to the 15 s above.
  `-AcMmoFlightShots=DIR` with `mmofly` saves a picture of each stage.
- A flight's sound (F1b; `Tools/AudioGen/flight.manifest.json`, ElevenLabs
  sound effects in `unreal/Resources/Sounds/sfx/`; `FAcAudioRules::Flight`
  and `Cruise`, tested in `Autocraft.Audio.Rules.Flight`), all in the ear:
  the engines' whine spooling up on the pad (`flspool`), the roar as the
  jets take hold (`flroar`, after the 0.7 s spool), the wind and engine
  drone of the cruise (`flcruise`, a loop that fades in as the ship climbs
  and out as it comes down), the retro thrust of the landing (`flretro`)
  and the touchdown's thud with dust hissing out (`fltouch`).
- Automation tests (hidden): `Autocraft.Mmo.CrossEdgeEast` (drive east over
  the start region's edge into Tern Ridge) and `Autocraft.Mmo.FlyToPad`
  (fly to Cold Fork's pad), in `AcMmoTests.cpp`. Through the pilot pawn and
  the world map, in a hidden game run: `-AcFlowTest=mmoresume,mmocross,mmofly,mmohandback`
  (`AcMmoFlowTest.cpp`; the network read only every 0.2 s while travelling,
  so a RegionChange and its first snapshot land in the same frame).
