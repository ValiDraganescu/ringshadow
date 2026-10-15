---
status: proposed
date:
angle: world
video: website/blog/media/passages/cover.jpg
posted:
xdraft: xd_78e7140e
---

## Post

Ringshadow is an open-source Mac game where you are one pilot in a world of nine regions, each run by its own simulation on the server. You can now walk from one region into the next, and at the crossing nothing moves, reloads or jumps.

Each region also got a ground of its own, cliff walls around it, and 14 passages through the walls across the world. The gap you see from one side lines up with the gap on the other.

AI agents build it while I direct.

## Thread

### 2

media: website/blog/media/passages/01-palettes.webp

The nine grounds: rust-red earth with iron stains in Rust Hollow, pale salt crust in plates at Brine Pans, soft grey ash on Ashen Shelf, black slag with cracks that glow like embers in Slag Pockets, frost on stone until mid-morning in Cold Fork. The last 16 cells before each edge blend halfway toward the neighbour's ground.

### 3

media: website/blog/media/passages/09-walk-4.webp

The server hands a unit to the neighbouring region at exactly the same point, and the client keeps one world frame and builds the next region's ground ahead of you. The other way, shifting the whole world's origin, measured 20 to 27 ms a crossing, longer than a frame. On a walk from Tern Ridge into Prospect Landing the longest frame at the crossing was 13.2 ms.

### 4

media: website/blog/media/passages/10-walk-5.webp

The camera's largest step at the crossing was 0.08 cells, and the walk never strayed more than 0.05 cells from its line. The region's name in a banner is the only sign you crossed.

## Reply

The devlog post, with 11 pictures of the grounds, the walls and the walk through: ringshadow.com/blog/passages

The code, art and the agents' skills are public domain (CC0): github.com/ValiDraganescu/ringshadow

## Notes

A thread from the devlog post ringshadow.com/blog/passages (website/blog/passages.md), facts from it only. 4 pictures from website/blog/media/passages/: the cover on post 1, then 01-palettes.webp, 09-walk-4.webp, 10-walk-5.webp. The post has 11 pictures in all (cover included).
