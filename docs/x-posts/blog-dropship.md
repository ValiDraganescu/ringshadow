---
status: proposed
date:
angle: unit
video: website/blog/media/dropship/cover.jpg
posted:
xdraft: xd_3075cecb
---

## Post

The Prospector in Ringshadow's online mode now rides a ship between regions. You build a Landing Pad, call the Dropship, walk up its ramp and watch the flight: lift-off in a ring of dust, two banking turns under the ringed giant, a dash through cloud, a landing. 16 to 20 seconds, with no loading screen and no fade. A longer trip only makes the dash longer.

Ringshadow is an open-source Mac game where you are one pilot in a frontier world that lasts, built by AI agents while I direct.

## Thread

### 2

media: website/blog/media/dropship/01-menu.webp

Flights go only between Landing Pads that pilots have built, to regions you've been to. A pad's blueprint is 80 ore at the Citadel's shop, and it costs 60 ore to build. A flight is 10 MH plus 5 for each step across the map's grid, so 15 MH to the next region over. From a Spacedock it's half.

### 3

media: website/blog/media/dropship/05-turn.webp

The region changes during the approach, and drawing it takes 1.6 ms. Through the whole flight frames run 10.8 ms at the median and 12.2 ms at the 95th percentile.

Still open: the first call in a session stalls once, for 522 ms, the first time the ship is drawn.

## Reply

The devlog post, with 10 pictures of a flight from Prospect Landing to Cold Fork: ringshadow.com/blog/dropship

The code, art and the agents' skills are public domain (CC0): github.com/ValiDraganescu/ringshadow

## Notes

A thread from the devlog post ringshadow.com/blog/dropship (website/blog/dropship.md), facts from it only. 3 pictures from website/blog/media/dropship/: the cover on post 1, then 01-menu.webp, 05-turn.webp. The post has 10 pictures in all (cover included).
