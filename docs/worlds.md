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
  wallet of 400 ore and 0 MH. The wallet is its side's bank in the region it
  is in (the snapshot's `players[0]`), and goes with it, as do its levels.
- Edges: the driven unit crosses once its centre is within 2 cells of an
  edge, inside an opening that meets the neighbour's, moving out (or steered
  out while pressed against it; a Prospector stops 1.56 cells from an edge).
  It comes in 2.5 cells inside the neighbour at the same point along the
  edge, with its load, hit points and levels. Elsewhere the edge is a wall.
- Flights: a Prospector within 4 cells of a pad's centre may fly to any
  region's pad (by region id and pad name). It lifts off, 4 s pass, it lands
  on that pad. Refused away from a pad, in flight, or where another pilot
  drives.
- One pilot drives in a region at a time (the core drives one unit a
  region); a crossing into a region where another pilot drives stops at the edge, a
  landing there circles until the ground is free.

## In the game

The MMORPG (TEST) mode (the home screen's item, or `-AcMmo`) starts
`autocraft-server --world unreal/server/worlds/first.json`; the game no
longer starts it with `--map`. The pilot starts seated in the Prospector on
Prospect Landing's pad.

- A RegionChange (over an edge, or a landing) gives the client a new mirror
  of the new region: the prediction and the blending start over, and the
  game rebuilds on it (terrain, doodads, minimap, fog: `OnGameStarted`). The
  pilot keeps the seat: the view holds where it was until the new region's
  first snapshot shows the unit, then follows it. Nothing of the old region
  stays.
- The banner at the top shows the region the pilot is in and its style, or
  "Flying to …" while in flight.
- M opens the world map (in place of the command map): the grid of regions
  with their names, styles and pads, the pilot's region ringed. Standing at
  a pad, click a pad and FLY (or Return); away from a pad it says how far
  the nearest pad is. M or Esc closes it. The unit stands while it is open.
- Automation tests (hidden): `Autocraft.Mmo.CrossEdgeEast` (drive east over
  the start region's edge into Tern Ridge) and `Autocraft.Mmo.FlyToPad`
  (fly to Cold Fork's pad), in `AcMmoTests.cpp`.
