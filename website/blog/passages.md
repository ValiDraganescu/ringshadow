# Through the walls: walking between regions

> Ringshadow's regions now have grounds of their own, cliff walls and 14 passages between them, and walking from one region into the next happens without a reload or a jump.

Source: https://ringshadow.com/blog/passages

15 October 2026 · M8 · The realm under siege

The world is nine regions, each run by the server on its own. Until now you could tell where one stopped: the ground looked the same everywhere, and the edge was a line. Now each region has a ground of its own, walls of cliff around it and passages through them, and you walk from one into the next without anything moving or reloading.

![A passage between two cliff walls, seen from Prospect Landing, the next region's ground visible through the gap](https://ringshadow.com/blog/media/passages/cover.jpg)

*A passage out of Prospect Landing, with Tern Ridge through the gap.*

## A ground for each region

Arriving somewhere new should look like it. Prospect Landing and Tern Ridge keep the brown dirt. Rust Hollow is rust-red earth with darker iron stains. Brine Pans is pale salt crust in plates, grey brine in the cracks. Ashen Shelf is soft grey ash. Glass Flats is dark glassy ground in facets that catch the sun. Slag Pockets, where [the Matriarch](https://ringshadow.com/blog/matriarch) nests, is black slag with cracks that glow like embers. Cold Fork has frost on its stone until mid-morning, and Dust Terraces has ochre in bands by height.

The last 16 cells before each edge blend halfway toward the neighbour's ground, so both sides of a border meet in the same colour.

## The nine grounds

- ![Nine regions from above in a grid: Ashen Shelf, Cold Fork, Glass Flats; Rust Hollow, Prospect Landing, Tern Ridge; Dust Terraces, Brine Pans, Slag Pockets](https://ringshadow.com/blog/media/passages/01-palettes.webp) Top row: Ashen Shelf, Cold Fork, Glass Flats. Middle: Rust Hollow, Prospect Landing, Tern Ridge. Bottom: Dust Terraces, Brine Pans, Slag Pockets.

## Walls, and the ways through them

Each region now has a border: the outline of its playable land, with cliff walls rising outside it, rock-strewn on top. Nothing walks on the walls. Fourteen passages cut through them across the world, and each one is laid out on both sides at the same floor height, so the gap you see from one region lines up with the gap on the other.

## Walls and passages

- ![Looking down over a cliff wall into a grey region, rocks and spires along the wall's top](https://ringshadow.com/blog/media/passages/02-over-the-wall.webp) Over the wall into a grey-grounded region, its rim strewn with rock.
- ![A banded cliff wall seen from the ground inside a region](https://ringshadow.com/blog/media/passages/03-wall-inside.webp) The wall from the inside.
- ![The passage seen from Prospect Landing, a pad in the foreground](https://ringshadow.com/blog/media/passages/04-passage-start.webp) The passage from Prospect Landing…
- ![The same passage seen from Tern Ridge, the start's beacon of light visible beyond it](https://ringshadow.com/blog/media/passages/05-passage-tern-ridge.webp) …and the same passage from Tern Ridge, with the start's beacon beyond it.

## Crossing without a seam

Each region has its own simulation on the server. When a unit walks up to an open edge, the server hands it to the neighbour at exactly the same point. The client keeps one world frame for everything and builds the neighbouring region's ground ahead of you, so at the moment you cross nothing moves, nothing reloads and the camera doesn't jump. The other way to do it, shifting the whole world's origin to the new region, cost 20 to 27 ms a crossing when it was measured, longer than a frame.

Measured on a walk from Tern Ridge into Prospect Landing:

- 13.2 ms: the longest frame at the crossing; 13.1 ms is a normal 95th percentile
- 4.63: cells a second, the walk before the crossing; 4.55 to 4.95 in each frame round it
- 0.08: cells, the camera's largest step
- 0.05: cells, the most the walk strayed from its line

## Walking through

- ![The Prospector in Tern Ridge, walking toward the passage between two cliff walls](https://ringshadow.com/blog/media/passages/06-walk-1.webp) In Tern Ridge, heading for the passage.
- ![The Prospector between the cliff walls of the passage](https://ringshadow.com/blog/media/passages/07-walk-2.webp) Between the walls.
- ![The Prospector past the walls, open ground ahead](https://ringshadow.com/blog/media/passages/08-walk-3.webp) Through the gap.
- ![The Prospector in Prospect Landing, the beacon ahead](https://ringshadow.com/blog/media/passages/09-walk-4.webp) Over the edge: now in Prospect Landing.
- ![The banner PROSPECT LANDING, Open Field, shown as the Prospector arrives](https://ringshadow.com/blog/media/passages/10-walk-5.webp) The region's name is the only sign you crossed.

## What isn't right yet

From above, the regions still read as rectangles: their walls follow the cells' edges in long straight runs. A second pass that gives each region an irregular outline is next, and after it the first canyon gets cut into a region for the [turrets](https://ringshadow.com/blog/turrets) to hold.

Every test suite passes: 488 core, 233 server and 84 network, and all nine regions validate.
