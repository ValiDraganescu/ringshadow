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
  neighbour's on the facing edge; beside a region with a border (W4,
  docs/regions.md, "border") it is the same span with the same floor level
  on both sides, and the ground of the two regions meets along the whole
  shared edge (within a centimetre: the walls and the passages' floors);
- the start region and its pad exist.

## The rules

- Every region is wild ground: the map's ore and wells, no bases, no AI, one
  player (the pilots' side). Every region steps every tick.
- A new pilot appears on the start pad driving a new Prospector, with a
  wallet of 600 ore and 0 MH. The wallet is its side's bank in the region it
  is in (the snapshot's `players[0]`), and goes with it, as do its levels.
- Edges (W4, seamless): in a region with open edges a unit walks up to an
  opening's edge and up to a cell past it. The driven unit crosses once its
  centre is past the edge, inside an opening that meets the neighbour's,
  and comes in at that same point of the world, just inside the
  neighbour, with its load, hit points and levels, its input in use and
  the ones not used yet: it walks on at the same pace, no stop, no jump
  (`world_crossing_keeps_position_and_speed`). Inputs its client made
  before it heard of the change still steer it there. Elsewhere the walls
  stop it. (A region with no openings at all keeps the old rule: within 2
  cells of the edge, in 2.5 cells inside the neighbour.)
- Flights (F2, docs/mmorpg-m8.md): the Prospector does not fly by itself.
  The realm builds Landing Pads (a building, level 1, two in a region, any
  pilot uses any; the Spacedock is the big one); a Prospector within 4 cells
  of one's centre (Pad link: 8, 12, 16) calls the Dropship to a Landing Pad
  in a region the pilot has been to (by region id and pad name, "Landing
  Pad 1"). The region files' pads are where new pilots appear, not flown
  to. The Prospector boards at once (out of every region: nothing touches
  it), the pilot moves into the target region when the approach starts,
  and the Prospector stands on the target pad at the touch down, 16 to 20 s
  after the call (`ac::Ferry`: descend 3, board 1.5, lift 2.5, turns 3, a
  dash of 2 s and 1 s a grid step up to 6, approach 4). Refused off a
  Landing Pad, aboard, to a region not seen, to a region with no Landing
  Pad, where another pilot drives, or when the wallet can't pay (10 + 5 a
  grid step, half from a Spacedock, nothing with Ringwalker;
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

- Walking over an edge is seamless (W4, docs/mmorpg-m8.md). The game draws
  every region in one frame for the whole session (the world frame,
  `AcSpace::Origin`: the pilot's region's centre on the world's grid less
  the session's first region's), and the regions round the pilot's are
  ready before it gets there: their ground (terrain, palette, doodads, the
  walls' rocks: `AAcRegionGround`, a step a frame), their minimap pictures
  and, across each passage, their mirror (`Client::prepare`), all made on
  worker threads a couple of seconds after each arrival, one at a time.
  The ground beyond the walls is the neighbours' own; only at the world's
  rim does a region draw its border scenery. At the RegionChange the
  client swaps in the mirror made ahead, carries the prediction over
  (`Predictor::carryOver`: the unit, its inputs not yet used, the pilot's
  controls), and the game takes the ground built ahead and draws on: no
  fade, no hold, the seat and the camera kept, the unit's model kept under
  its new id. The crossing is measured by `-AcMmoShotsW4` (`w4: crossing:`
  in the log, `w4-crossing.csv`): frame times and the unit's pace round it.
- The Dropship's approach (F2) changes region seamlessly too: from the call
  the client makes the target region ahead (its map, its ground set at its
  place on the world's grid, its mirror), so the region change moves the
  world frame under the ship and nothing is built then. The pilot keeps the
  seat with nothing driven until the touch down's snapshot shows the
  Prospector (adopted as the one wanted).
- The banner at the top shows the region the pilot is in and its style, or
  "Flying to …" while in flight. The top bar counts the pilot's units (no
  supply count in this mode).
- Each region file's pad is drawn as a round foundation plate 4 cells
  across its centre (`region::padReach`); the generated base's start plates
  are not drawn in a region. A Landing Pad is a building: a 3 by 3 deck the
  Prospector walks onto (`Rules::walkable`). Server and client measure "at
  a pad" with one rule (`region::nearestPad`, `net::padsOnGround`).
- M opens the world map (in place of the command map), the flight menu:
  standing on a Landing Pad, click a Landing Pad in a region been to (a
  cyan square; one in a region not seen yet dim, a region file's pad a
  small dim circle, neither picked) and FLY (or Return); off one it says how
  far the nearest Landing Pad is. M or Esc closes it. The unit stands while
  it is open.
- Only the player gives the unit back (Esc): a region change never does,
  and the server's seat after a landing is taken only then, so an old
  snapshot never takes the unit back again. A view held for travel gives
  way to the top-down camera after 40 s.
- The Dropship's flight is shown (`AcPilotFlight.h`; the server's stages
  are the rule, the picture runs on the real-seconds clock from the call):
  the Dropship comes in over a low camera and down onto the Landing Pad,
  a copy of the Prospector walks up its ramp, it lifts off in a ring of
  dust, climbs through two banking turns from the giant's way onto the
  target's (the camera chasing), dashes across the sky through streaking
  cloud, then slows and comes down onto the target pad, seen wide, and the
  Prospector walks off where the server put it while the camera settles
  into the pilot's view; control is back then, and the ship lifts away by
  itself. Never a fade. The Landing Pads' lamps flash while it is over
  them. `-AcMmoFlightShots=DIR` with `mmofly` saves a picture of four
  stages; `-AcMmoShotsF2` is the picture set and the frame times
  (`f2 perf:` in the log).
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
  (the Dropship to Cold Fork's Landing Pad), in `AcMmoTests.cpp`. Through the pilot pawn and
  the world map, in a hidden game run: `-AcFlowTest=mmoresume,mmocross,mmofly,mmohandback`
  (`AcMmoFlowTest.cpp`; the network read only every 0.2 s while travelling,
  so a RegionChange and its first snapshot land in the same frame).
