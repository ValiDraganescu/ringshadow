# The world map: the real ground, seen from above

> Press M and Ringshadow's world map shows every region you've been to as a picture of its real ground from above, with live pilots, mercenaries, the Brood, buildings and deposits, and a tooltip on each.

Source: https://ringshadow.com/blog/world-map

15 October 2026 · M8 · The realm under siege

The map on M used to be a drawing: flat colours, one per region. Now it's a picture of the actual ground taken from straight above, with everything that's alive drawn over it, and you can point at any of it to ask what it is.

![The world map: nine regions in a three by three grid, each a top-down picture of its ground, from grey ash and glittering glass to rust, salt and slag](https://ringshadow.com/blog/media/world-map/cover.jpg)

*The whole world on M, after the regions got their own grounds.*

## A photograph of every region

Each region is baked into one picture. The game builds the region the way a client would and photographs it with a camera pointing straight down: its terrain, cliffs, rocks and plants, its ore and wells, lit at half past ten in the morning in clear weather, with no units and no fog. That's 8 pixels a cell, 1152 × 768 for a medium region, north up. A script does the whole world in two hidden runs, so when a region changes the map is rebaked in one command.

The pictures lie side by side as the regions do. A region you haven't been to stays dark, with its outline and its name, UNCHARTED. The map opens centred on you; the wheel zooms, dragging pans, and the world keeps running while it's open.

## The map, twice

- ![The world map when it first landed: six regions in desert colours, three still dark and UNCHARTED](https://ringshadow.com/blog/media/world-map/01-world-m2.webp) When it landed: six regions seen, three still uncharted, before each region had a ground of its own.
- ![The world map rebaked: all nine regions, each in its own ground](https://ringshadow.com/blog/media/world-map/02-world-palettes.webp) Rebaked a few hours later, once each region had a [ground of its own](https://ringshadow.com/blog/passages).
- ![Zoomed in on Prospect Landing: the Citadel, pads, deposits in violet, plants and cliffs](https://ringshadow.com/blog/media/world-map/03-prospect-landing.webp) Zoomed in on Prospect Landing. Violet marks are deposits, the rings are pads.

## What's alive on it

Over the pictures the map draws what's happening now: you as a cyan arrow, other pilots, your mercenaries in orange, the Brood in red groups with their count, the realm's buildings, deposits in violet that shrink as they empty, MH wells, the blueprint sites you know of in gold, pads as yellow rings, starfalls and their craters, the missions in your Journal and the ringshadow band while it sweeps across. Live units, buildings and deposits show only in the region you're in. Elsewhere the map shows what you know: sites, pads, missions, and your mercenaries at their fields.

Point at anything and a tooltip says what's worth knowing. A deposit shows how much ore is left, and whether a Lodeback is eating it. A building shows who built it and its HP. A site shows its blueprint and whether it's been opened. A mission shows its tier and instructions, and a mercenary its field and what it has delivered. Clicking a mission or a site points your compass at it.

## Tooltips

- ![The map with a tooltip on an ore deposit](https://ringshadow.com/blog/media/world-map/04-tip-deposit.webp) A deposit and the ore left in it.
- ![The map with a tooltip on the Citadel: Built by you, HP 1,500 of 1,500](https://ringshadow.com/blog/media/world-map/05-tip-building.webp) The Citadel: built by you, HP 1,500 of 1,500.
- ![The map with a tooltip on a mission marker: Mark the Ground, an easy survey mission in Prospect Landing](https://ringshadow.com/blog/media/world-map/06-tip-mission.webp) Mark the Ground, an easy survey mission. Click it to steer the compass there.
- ![The map with a tooltip on a mercenary named Zinc, working field 2 in Prospect Landing](https://ringshadow.com/blog/media/world-map/07-tip-merc.webp) Zinc, a [mercenary](https://ringshadow.com/blog/mercenaries), just hired: field 2, nothing delivered yet.

## What isn't right yet

The pictures show the regions as near rectangles, because that's still how they read from above: their walls run along the cells' edges. A second pass on the outlines is next, and the map will be rebaked after it.

The test suites passed when it landed: 473 core, 216 server and 81 network.
