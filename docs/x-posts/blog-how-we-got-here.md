---
status: proposed
date:
angle: build
video: website/blog/media/how-we-got-here/showreel.mp4
posted:
xdraft: xd_df09b442
---

## Post

Two days ago Ringshadow's online mode was a list of decisions in a log file. Tonight a pilot lands on a frontier world, drills opal ore under a ringed gas giant and watches a meteor come down where the radio said it would.

Ringshadow is an open-source Mac game built by AI agents while I direct. The online mode makes you one pilot in a world that lasts.

The agents wrote our own networking on UDP. The server steps the world 60 times a second and sends each pilot what is near them, 20 times a second. Even single player runs through that same server.

## Thread

### 2

media: website/blog/media/how-we-got-here/06-ringshadow.webp

The game is named after the giant in its sky, so the signature event had to be its shadow. The rings throw a band of shadow that sweeps across the land on the world clock. Under it the ore veins flare and drilling pays more.

### 3

media: website/blog/media/how-we-got-here/07-starfall.webp

Then starfall: two or three times a world day a meteor streaks in, and its crater holds a deposit about three times richer than the usual one. First come, first served. KSTR 88.7, the frontier's only radio station, puts up the banner: STARFALL DOWN.

### 4

media: website/blog/media/how-we-got-here/09-liftoff.webp

On 13 October I handed the mode to a manager agent for a week, with one goal: a complete, beautiful game by 20 October. It runs workers in their own copies of the repository and merges what they finish.

The first stretch gave the Prospector 100 levels and 65 skill nodes in five branches: about 1.1 hours to level 10, 97 to level 100.

## Reply

The whole post, with the one-minute showreel and 9 pictures: ringshadow.com/blog/how-we-got-here

The code, art, models, sounds and music are public domain (CC0): github.com/ValiDraganescu/ringshadow

## Notes

A thread made from the blog post. Source: website/blog/how-we-got-here.md. Post 1 carries the showreel (showreel.mp4, 4.0 MB, 1 min, the game's own sound). Pictures: 06-ringshadow, 07-starfall, 09-liftoff.
