---
name: ringshadow-region-crafting
description: Craft or change a region of Ringshadow's MMORPG world (a region file in unreal/server/regions/) with the autocraft-region tool: write the file, validate it, render the top-down preview and look at it, iterate. Use when asked to make, edit, fix or review a region, place deposits, wells, pads, spawn points or edge openings, or shape the ground of a region (cliffs, ridges, ramps, flat areas).
---

# Crafting a region

A region is a JSON file: a generated **base** plus crafted **layers**. The
format, every key and every check: [docs/regions.md](../../../docs/regions.md).
Read it before the first edit.

You craft with your eyes: the preview is the truth, the JSON is how you
move things. Every change ends with you looking at a fresh preview.

## Steps

Run from the repo root; `T=unreal/server/build/autocraft-region`.

1. **Build the tool**: `make -C unreal/server build/autocraft-region`.
2. **Pick a base.** Render candidates and choose the one whose ground
   already fits the brief: `$T new --style S --seed N --size Z --id ID -o /tmp/<id>-N.json`,
   then `$T preview` each. Styles: highlands (high plateaus, a low
   corridor), openField (flat), ridge (a high spine), badlands (high pockets,
   low ground). For a clean slate add `"drop": ["resources", "doodads"]`.
   Done when one base is chosen and copied to `unreal/server/regions/<id>.json`.
3. **Say the land, don't draw it.** Shape the ground with `sculpt`
   (docs/regions.md, "sculpt"; every form's numbers and the rules:
   `ringshadow-world-building`'s LANDFORMS.md; a whole world: that skill): a style for the brief and a few landforms
   where the brief wants them (an escarpment along there, a butte here,
   spires, a wash). Write polygons, circles and single rocks only for what
   a landform cannot say (a pad's flat, a canyon zone): hand-drawn shapes
   read as placed. `validate` names a form that breaks a rule; `build`
   says what the style placed, the straight cliff runs (keep 0) and the
   raid's way in.
4. **Plan on the grid.** Read the base's preview (Read tool on the PNG) and
   write down, in cells, where each thing of the brief goes: cliffs and
   ramps first, then pads, spawns, deposits, wells, openings.
5. **Edit the file** in small batches: one layer at a time (ground, then
   resources, then places, then openings).
6. **Validate**: `$T validate FILE`. Fix every problem it names; it gives the
   region position.
7. **Preview and look**: `$T preview FILE -o /tmp/<id>.png`, then Read the PNG.
   Check every item of the brief against the picture: is it where the brief
   says, does the ground read the way it was asked (a cliff where a wall was
   wanted, a ramp a machine can climb), are the problems ringed in red gone.
8. **See it at an angle**: the top-down preview hides how tall a wall is
   and what a pilot sees. `$T view FILE -o /tmp/<id>-s.png` (an overview
   from the south; `--from n|e|s|w`) and `$T view FILE --eye -o
   /tmp/<id>-eye.png --from n` (a pilot's eye on the first pad). Once a
   round of edits is done, make the critic's sheet and judge the region on
   it against the brief: `uv run unreal/Tools/regions/critic.py ID [--ref
   IMAGE] [--shots DIR] -o /tmp/<id>-sheet.jpg`. Look at it with the Read
   tool. Ask whether the ground reads as asked from the side and from the
   pad: an open horizon or a bowl, rock standing free or walling in, ramps
   and shelves where the brief put them ([docs/regions.md](../../../docs/regions.md),
   "Seeing a region").
9. **Iterate** 5 → 8 until `validate` prints `ok` and every brief item is
   visible where it belongs.
10. **Report**: the file path, the final preview's and sheet's paths, and `$T build FILE`'s
   summary (ore, MH, open ground).

## Borders and passages (W4)

A world's region is never a rectangle: its `border` outlines the playable
land and walls rise outside it ([docs/regions.md](../../../docs/regions.md),
"border"). Every opening on a shared edge needs the same span and floor
level in both files (`{"span": [a, b], "level": n}`), and the outline must
reach the edge round it.

1. Pick each passage where both neighbours' ground is level:
   `$T edges FILE --depth 8` prints a level digit every 2 cells along each
   edge (`#` a cliff) for both files.
2. Draw the outline 6–20 cells in from the rectangle, irregular, out to
   the edge round each passage, clear of everything placed (a pad, a site,
   ore and wells need a cell inside it).
3. `$T validate` both files, then load the world (`make -C unreal/server
   test FILTER=world_load`, or start the server on it): it checks that the
   passages line up and the ground meets along every shared edge.
4. Preview and look: the dotted outline, the cyan passages with their L
   levels, the walls (level 3–4 colour) outside.
5. Rebake the satellite map (`uv run unreal/Tools/bake_map.py`).

## Canyon zones (Z1)

A canyon zone is a tower-defence ground in a region: the Brood comes out of
tunnels and walks fixed paths along a canyon's floor to a prize; pilots
build turrets on the rims ([docs/regions.md](../../../docs/regions.md),
"zones"). Cold Fork's The Fork is the first.

1. Raise the zone's ground into a mesa (`raise`, level 2) and cut the arms
   into it with `cut` (a line, `width` 5–6, level 0); a `cut` with a short
   line and a wide width makes the junction's basin. Ramps up from the low
   ground and from the floor to the rims.
2. The `zones` entry: entrances at the arms' ends against a wall (`facing`
   into the canyon), one path from each along the floor's middle, ending
   just short of the prize (not on a deposit), the prize by index into the
   file's `deposits` and `wells` (append them: saves keep deposits by
   index, so move a deposit the canyon takes rather than delete it).
3. No build spots (Z4): pilots build anywhere standable in the zone. Leave
   room for it: flat rims a gatling gun's reach (6 cells) from the paths,
   reached by ramps, and floor wide enough to pack guns on. Know every way
   into the prize: a walker routes round buildings while any way is open
   (the Drowned Cut's Salt tunnel opens onto the outside ground, so its
   basin is reached up the gorge too).
4. `$T validate`, `$T preview` (paths, chevrons toward the prize, tunnels,
   the prize ringed), rebake the map.

## Pitfalls

- **Scarcity (E1).** A world region (brood 1 or more) holds at most two ore
  veins of 3 to 5 mounds each, within 4.5 cells of the vein's middle, one MH
  well, and its ore in all by brood level (6,000–8,000 at 1, 10,000–12,000
  at 2, 15,000–20,000 at 3 and 4); `base.drop` must have `resources`.
  `validate` names each breach. Lay a vein as a ring round an open middle
  (2.6–3.2 cells out, each mound's `angle` along its radius) so a Prospector
  never gets boxed in between mounds; a canyon's prize is the region's well
  with a ring 6 cells round it (clear of the 7x7 Derrick, C5; one vein by
  rule). The numbers per region are in
  docs/regions.md, "Scarcity"; changing a region's veins means rebaking its
  satellite map (`bake_map.py --only ID`).
- Positions are cells from the **north-west corner**, z grows **south**
  (down the picture). Read positions off the grid labels, not by guessing.
- A cliff edge falls away **outside** the outline: an area's outline is the
  top edge, the cliff face is the 1.2 cells past it. Keep pads and wells
  3+ cells inside an outline.
- A ramp must be written **after** the cliffs it crosses and needs about 5
  cells of length a level. Its ends are checked 1.5 cells past each end:
  start it a little out on the low ground and end it on the top.
- `slope` under about 4 cells a level is still a cliff to a machine.
- Flatten a pad's ground first (`flatten` with a circle of radius 10+) when it
  sits on rolling or base ground; a pad needs heights within 0.6 over its
  whole plate (8.5 round since C5: the Dropship lands on it aligned) and
  its ramp's apron 2.5 cells in front of its +y edge. Keep spawns and sites
  off the plate.
- An opening needs open ground 2 cells in along the whole span, joined to the
  main area: keep cliffs, ore and crafted rocks out of that lane and give
  the lane a way into the region.
- Generated base ore lines sit where the base put them; `drop` them rather
  than working round them when the brief places its own.
- A ring of cliffs makes the inside a separate area: anything inside is
  "cut off" unless a ramp leads in.
- Preview labels are drawn east of their marker: a spawn within about 6
  cells east of a pad on the same row prints over the pad's name. Put the
  spawn south of the pad (or a row off) so both names read.
- `build`'s "main open area" a point under "open ground" is usually a
  ridge top with no ramp: a wall, not a cut-off place.
- A border's outline must not cross itself; with big lobes, cut the corners
  (the outline's points drop inside a neighbouring side's wall band).
- The First Reach's shapes come from `uv run unreal/Tools/regions/shape.py
  REGION` (W4b): rerun it after moving a passage or adding a place near the
  rock, or pass `--seed REGION=N` to try another shape; it validates every
  try. `autocraft-region levels FILE --bare` shows where the bare ground
  walks (a level digit), its cliffs (`#`) and rocks (`o`).
- Walls are not ground: nothing stands or lands on them. A thing left
  outside the outline is a problem, not a hidden prize.
