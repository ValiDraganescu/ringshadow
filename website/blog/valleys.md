# Valleys, not tiles: the regions reshaped

> Ringshadow's nine regions reshaped as natural basins: bays, rock spurs, necks, islands of rock, terraced walls and passages that narrow like canyon mouths, with one high land of rock running between them.

Source: https://ringshadow.com/blog/valleys

15 October 2026 · M8 · The realm under siege

After the regions got [walls and passages](https://ringshadow.com/blog/passages), they still looked like what they are underneath: nine squares in a grid. Now each one is a basin with a shape of its own, and from above the high ground runs on between them as one land.

![Looking into a bay: banded canyon walls stepping down with a shelf halfway up, ore deposits on the grass at their foot](https://ringshadow.com/blog/media/valleys/cover.jpg)

*Into a bay: the wall steps down with a shelf halfway up.*

## A shape for each region

A new tool draws the shapes, and it's in the repo with the rest. Each region is now a natural basin taking 50 to 66% of its square. Bays cut 10 to 30% into the land, rock spurs push out between them, and where two bays face each other the land pinches into a neck. Three regions have islands of rock in the middle that the land runs round. No straight run of wall is longer than about 12 to 15 cells, and every passage narrows toward its gap like the mouth of a canyon.

For each region the tool tries 40 shapes, checks that every pad, deposit and site can still be reached on foot from the landing pad, and keeps the valid one with the shortest straight wall. Deposits the walls would have swallowed move onto the land nearby.

The walls step: along stretches of them there's a shelf a few cells wide halfway up, then the rest of the climb. Their tops share one rock colour across the whole world, whatever the ground below, so on the map the high land reads as one piece with basins cut into it.

## From above, before and after

- ![Before: the world map, nine regions as nine filled squares](https://ringshadow.com/blog/media/valleys/01-map-before.webp) Before: nine squares.
- ![After: the world map, each region an irregular basin inside one stretch of rocky high ground](https://ringshadow.com/blog/media/valleys/02-map-after.webp) After: nine basins in one land.
- ![Before: the nine regions seen at an angle, each filling its square](https://ringshadow.com/blog/media/valleys/03-grid-before.webp) Before, seen at an angle.
- ![After: the same nine regions, bays, spurs and islands of rock in each](https://ringshadow.com/blog/media/valleys/04-grid-after.webp) After: bays, spurs and islands.

## On the ground

- ![Banded canyon walls stepping down into a bay, a shelf halfway up](https://ringshadow.com/blog/media/valleys/05-bay.webp) A bay, with its shelf.
- ![Walking along a wall that bends out of sight, the far side of the basin beyond](https://ringshadow.com/blog/media/valleys/06-wall.webp) Along a wall: it bends before it gets long.
- ![The passage out of Prospect Landing between two banded walls, Tern Ridge beyond](https://ringshadow.com/blog/media/valleys/07-passage-start.webp) The passage from Prospect Landing.
- ![The same passage from Tern Ridge, the start's beacon beyond](https://ringshadow.com/blog/media/valleys/08-passage-tern-ridge.webp) The same passage from Tern Ridge.

## The nest is back

Two bugs came in with the walls, and both are fixed. [The Matriarch's](https://ringshadow.com/blog/matriarch) nest mound had stopped drawing, and a black slab stood near her in Slag Pockets. Both had one cause: a set of ground built away from the world's origin placed its scenery twice, the second copy offset.

## Slag Pockets, fixed

- ![The Matriarch's nest seen from the pad in Slag Pockets, the mound drawn again](https://ringshadow.com/blog/media/valleys/09-nest.webp) The nest from the pad, its mound back.
- ![The Matriarch felled at her nest, no black slab in the view](https://ringshadow.com/blog/media/valleys/10-down.webp) Her fall, with no slab in the way.

## The crossing, measured again

Walking from Tern Ridge into Prospect Landing on the new shapes, frames ran 11.6 ms at the median, and the longest at the moment of the crossing was 13.2 ms. The walk held between 4.55 and 4.99 cells a second against 4.79 before it, and the camera's largest step was 0.08 cells.

Every test suite passes: 494 core, 241 server and 85 network.
