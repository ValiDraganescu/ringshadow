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
3. **Plan on the grid.** Read the base's preview (Read tool on the PNG) and
   write down, in cells, where each thing of the brief goes: cliffs and
   ramps first, then pads, spawns, deposits, wells, openings.
4. **Edit the file** in small batches: one layer at a time (ground, then
   resources, then places, then openings).
5. **Validate**: `$T validate FILE`. Fix every problem it names; it gives the
   region position.
6. **Preview and look**: `$T preview FILE -o /tmp/<id>.png`, then Read the PNG.
   Check every item of the brief against the picture: is it where the brief
   says, does the ground read the way it was asked (a cliff where a wall was
   wanted, a ramp a machine can climb), are the problems ringed in red gone.
7. **Iterate** 4 → 6 until `validate` prints `ok` and every brief item is
   visible where it belongs.
8. **Report**: the file path, the final preview's path, and `$T build FILE`'s
   summary (ore, MH, open ground).

## Pitfalls

- Positions are cells from the **north-west corner**, z grows **south**
  (down the picture). Read positions off the grid labels, not by guessing.
- A cliff edge falls away **outside** the outline: an area's outline is the
  top edge, the cliff face is the 1.2 cells past it. Keep pads and wells
  3+ cells inside an outline.
- A ramp must be written **after** the cliffs it crosses and needs about 5
  cells of length a level. Its ends are checked 1.5 cells past each end:
  start it a little out on the low ground and end it on the top.
- `slope` under about 4 cells a level is still a cliff to a machine.
- Flatten a pad's ground first (`flatten` with a circle of radius 5+) when it
  sits on rolling or base ground; a pad needs heights within 0.6.
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
