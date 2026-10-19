---
name: ringshadow-world-building
description: Build or re-make a Ringshadow world end to end — the grid of regions, their low rims and wide passages, each region's ground said as `sculpt` landforms (styles, rules), judged on the critic sheet and in game, then tested, baked and merged. Use when asked to create a new world, recreate or re-sculpt the regions, open up the land, add or change landforms, or change the ground's look (terrain material, rock faces, clutter).
---

# Building a world

A world is a grid of region files (`unreal/server/worlds/<id>.json`, each
cell a `unreal/server/regions/<id>.json`). Each region's ground is **said,
not drawn**: a `sculpt` block names a style and landforms in words, and
`expand` (`unreal/Source/AutocraftCore/Private/Landforms.cpp`) draws their
shapes at build time, on the server and the client alike. A polygon, a
circle or a rock set by hand reads as placed: the user rejected exactly that
("they look too manually placed"). The user's brief for the land: open
floor to the horizon, free-standing buttes, mesas and spires, low shelves,
scattered stones; "canyons are cool, but not that cool"; never "always
surrounded by cliffs".

You judge with your eyes: the **critic sheet** (top view, four oblique
views, two pilot's-eye views, the reference, the in-game stills, the
measures) is the truth; the JSON is how you move things.

Reference, read when its step sends you there:
- [LANDFORMS.md](LANDFORMS.md): every form, style, rule and measure, with
  their numbers, how `expand` seeds and orders them, and how the field draws
  rims and islands.
- [TOOLS.md](TOOLS.md): every command (`autocraft-region`, `sculpt.py`,
  `critic.py`, `bake_map.py`, the in-game sheet, the tests) and what it
  prints.
- [GROUND-LOOK.md](GROUND-LOOK.md): the ground's material, texture arrays,
  rock faces and clutter, how to regenerate them, the GPU budget.
- [HICCUPS.md](HICCUPS.md): what went wrong before. Read it before step 4;
  add to it the moment something surprises you.

The file format, key by key, is [docs/regions.md](../../../docs/regions.md)
(its "border", "sculpt", "Scarcity", "zones", "openings", "Seeing a region"
sections) and the world file is [docs/worlds.md](../../../docs/worlds.md);
why the land is shaped this way is [docs/world-sculpting.md](../../../docs/world-sculpting.md).
Per-region crafting details (bases, places, vaults, canyon zones) live in
the `ringshadow-region-crafting` skill.

## Steps

Run everything from your worktree's root (`ringshadow-dev-agent` skill: own
worktree, `unreal/Tools/build.sh`, one hidden run at a time). Set
`T=unreal/server/build/autocraft-region`. In zsh a command held in a
variable does not split: put multi-command chains in a bash script.

1. **Read and build.** Read the docs above and HICCUPS.md; `make -C
   unreal/server build/autocraft-region`. Done when `$T` prints its usage
   with `view` and `expand` in it.

2. **Plan the world.** From the brief, a table: each grid cell's region id,
   name, `brood` level (0-4; a level-4 region has the Matriarch's `nest`),
   `ground` palette (plain, rust, salt, ash, glass, slag, frost, ochre), a
   one-line character, its `sculpt.style` (`open-desert`, `badlands`,
   `mesa-country`; vary them across the grid), its 3-6 own landforms, and
   whether it holds a canyon zone (at most one or two in a world: the
   canyon is a feature, not the land). Name the start region and its pad
   in the world file (`grid` rows north to south, cells west to east, a
   cell a region path or `null`). The game, the map bake and `sculpt.py`
   read `unreal/server/worlds/first.json` (`AcMmo.cpp`, `AcMapBake.cpp`)
   and `sculpt.py`'s own `GRID`: a new world either replaces `first.json`
   or changes those three, and `GRID` must match the world's grid. Done
   when every cell has a row and no two neighbours share a character.

3. **Lay the regions.** For a new region: `$T new --style openField --seed
   N --size medium --id ID -o unreal/server/regions/ID.json`, then
   `"drop": ["resources", "doodads"]` in `base` (the generated plateaus of
   the other base styles are cliff-ringed tables; build the ground from
   landforms instead). Place in order: pads (with a `flatten` circle of
   radius 16 at level 0, slope 5 under each: the only hand-written ground
   besides canyon zones), spawns, sites, veins and the well (the scarcity
   rules: docs/regions.md "Scarcity"; each vein a ring of 3-5 mounds),
   the nest, landmarks. Canyon zones: their `raise`/`cut`/`ramp` edits and
   `zones` entry, as `ringshadow-region-crafting` "Canyon zones" says. For a
   re-made region: keep pads, spawns, sites, deposits (order kept: zone
   prizes are indexes), wells, nest, zones and landmarks; delete every
   other `height` edit and every `rock`/`boulder`/`rockSpire` doodad.
   Done when each file validates with only `sculpt`-less ground.

4. **Open the land.** For each region, in grid order: `uv run
   unreal/Tools/regions/sculpt.py ID --rocks "" --grow 8 --level 1 --slope 5`.
   It grows the land over level ground, lowers and slopes the rim, and
   opens every shared edge wide wherever both sides are level 0, writing
   both files of each pair (TOOLS.md, "sculpt.py"). Fix every
   "closes the passage" problem by trimming that span in both files (it ran
   into a corner). Done when all files validate and `make -C unreal/server
   test FILTER=world_load` passes.

5. **Say the land.** Per region, write the `sculpt` block: the style and the
   region's own forms where its character wants them (LANDFORMS.md for every
   form's size, reach and the rules). Then loop:
   `$T validate FILE` → `$T build FILE` → `uv run
   unreal/Tools/regions/critic.py ID --ref BRIEF_IMAGE -o
   unreal/server/build/critic/ID-sheet.jpg` → Read the sheet. Move, resize,
   reseed or drop forms until the region is done:
   - `validate` prints ok (no `sculpt.forms[i]` breach);
   - `build`: wall and cliff ≤ 25% (a canyon region ≤ 35%), straight cliff
     runs 0 outside canyon zones, open horizon at every pad ≥ 50%, 4-10
     free-standing rocks, every raid line has a way;
   - the sheet, beside the brief's reference: from the pad's eye an open
     floor with rock standing free, no ring of wall, nothing that reads as
     placed, the region's character visible from the oblique views.
   Regions are independent here: split them across workers (3 regions a
   worker), each editing only its own files and never `border` or
   `openings` (shared with neighbours).

6. **Grow where the rim still hugs.** A region over its wall target with
   the forms already sparse: `sculpt.py ID --rocks "" --grow 14 --no-widen`
   (touches only that file), then step 5's checks again. Done when every
   region meets step 5's targets or the report names why not (a canyon
   zone's straight cuts, a vault's ramp that needs a hand-written rise).

7. **Test the world.** `make -C unreal/core-tests`, `make -C unreal/server
   test`, `make -C unreal/net-tests`. Tests that place things by coordinates
   lean on the old ground: make them pick their spot by a rule (TOOLS.md,
   "Tests that read the ground"). A test that shows the game broken there
   (no Citadel fits by a pad, a gun can't reach a path, a raid has no way)
   is fixed in the region, not the test. Done when all three suites pass.

8. **See it in game.** `unreal/Tools/build.sh`, then the in-game sheet for
   every region (TOOLS.md, "In-game stills") and look at each region's
   feet, across and third stills. If the ground's look or the scenery
   changed, bench it (GROUND-LOOK.md, "Budget"). Done when every region's
   stills read like its character and the bench is within budget.

9. **Bake and land.** `uv run unreal/Tools/bake_map.py` (all regions:
   the satellite maps the client shows), commit by explicit paths with no AI
   attribution, three-way merge into the integration branch, build there.
   Tell the user to press NEW WORLD: a save made on the old ground does not
   fit the new one. Done when the integration branch builds and the report
   lists each region's style, forms and measures.

## Report

A table: region, style, own forms, wall and cliff, free-standing rock,
straight runs, open horizon at the pad, before and after; what was kept by
hand and why; the critic sheets' and stills' paths; test and bench lines.
