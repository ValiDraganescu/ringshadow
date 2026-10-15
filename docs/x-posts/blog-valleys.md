---
status: proposed
date:
angle: world
video: website/blog/media/valleys/cover.jpg
posted:
xdraft: xd_cb695c8d
---

## Post

Ringshadow's online world is nine regions in a three by three grid, and from above they looked like it: nine squares. The agents reshaped each one into a natural basin with bays, rock spurs, necks and islands of rock, and the high ground runs on between them as one land. The walls step down in terraces, and every passage narrows toward its gap like a canyon mouth.

Ringshadow is an open-source Mac game where you are one pilot in a frontier world that lasts. AI agents build it while I direct.

## Thread

### 2

media: website/blog/media/valleys/01-map-before.webp, website/blog/media/valleys/02-map-after.webp

A new tool draws the shapes. For each region it tries 40, checks that every pad, deposit and site can still be reached on foot from the landing pad, and keeps the valid one with the shortest straight wall. Each basin takes 50 to 66% of its square, and no straight wall runs longer than about 12 to 15 cells.

### 3

media: website/blog/media/valleys/05-bay.webp

The walls step: a shelf a few cells wide halfway up, then the rest of the climb. Their tops share one rock colour across the world, whatever the ground below, so on the map the high land reads as one piece with basins cut into it.

### 4

media: website/blog/media/valleys/09-nest.webp

Two bugs came in with the walls, and both are fixed: the Matriarch's nest mound had stopped drawing, and a black slab stood near her in Slag Pockets. One cause for both: ground built away from the world's origin placed its scenery twice, the second copy offset.

## Reply

The devlog post, with 11 pictures, the map and the regions before and after: ringshadow.com/blog/valleys

The code, art and the agents' skills are public domain (CC0): github.com/ValiDraganescu/ringshadow

## Notes

A thread from the devlog post ringshadow.com/blog/valleys (website/blog/valleys.md), facts from it only. 5 pictures from website/blog/media/valleys/: the cover on post 1, then 01-map-before.webp, 02-map-after.webp, 05-bay.webp, 09-nest.webp. The post has 11 pictures in all (cover included).
