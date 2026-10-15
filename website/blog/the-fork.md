# The Fork: the first canyon to hold

> The first canyon in Ringshadow's world: The Fork in Cold Fork, two winding arms from Brood tunnels to a prize of 36,000 ore and 6,000 MH, with 12 build spots on the rims for the turrets that will hold it.

Source: https://ringshadow.com/blog/the-fork

15 October 2026 · M8 · The realm under siege

The canyon from the [plan a day ago](https://ringshadow.com/blog/defences) has been cut. It's called The Fork, it's in Cold Fork, and it's built for tower defence: the Brood comes out of tunnels at one end, the ore it wants is at the other, and the rims between are where pilots will put their guns. The waves come next.

![The Fork from above: a mesa cut by two winding canyon arms that meet at a basin with deposits round a well, yellow build spots along the rims, the old derrick ruin between the arms](https://ringshadow.com/blog/media/the-fork/cover.jpg)

*The Fork from above. The yellow rings on the rims are the build spots.*

## How it's laid out

The Fork is a mesa in the north-east of Cold Fork. Two arms wind down through it to the canyon floor, each starting at a Brood tunnel, one in the north wall and one in the east. They meet at a basin, and the prize is there: six deposits of 6,000 ore each round an MH well of 6,000. The richest deposit anywhere else in the region holds 3,600.

Ramps lead up to three rims with 12 build spots, marked in amber, overlooking the bends. The old derrick ruin stands on the rim between the arms. A gorge lets pilots walk down onto the floor, so they can fight with their own torches too.

- 2: arms, each from a Brood tunnel to the prize
- 36,000: ore in six deposits, and 6,000 MH in the well
- 12: build spots on three rims
- about 6 s: for a bug to walk 18 cells from tunnel to prize

## Down in The Fork

- ![From a rim: a build spot in the foreground, the canyon arm below, more spots along the far rim](https://ringshadow.com/blog/media/the-fork/02-from-a-rim.webp) From a rim: a build spot over the arm below.
- ![From the canyon floor looking up at a bend, stepped walls, build spot lamps on the rim above](https://ringshadow.com/blog/media/the-fork/03-floor.webp) From the floor, up at a bend. The lamps on top mark the spots.
- ![A Brood tunnel: a dark hollow framed by rock at the foot of the canyon wall](https://ringshadow.com/blog/media/the-fork/04-tunnel.webp) A Brood tunnel, where the waves will come from.
- ![Shardlings on the prize: rich deposits in a ring round an MH well at the junction](https://ringshadow.com/blog/media/the-fork/05-prize.webp) Shardlings sent down a path, already on the prize.
- ![Three build spots on a rim beside a tunnel's mouth](https://ringshadow.com/blog/media/the-fork/06-build-spots.webp) Build spots by a tunnel's mouth.
- ![The world map of Cold Fork, The Fork in its north-east corner](https://ringshadow.com/blog/media/the-fork/07-map.webp) Cold Fork on the [map](https://ringshadow.com/blog/world-map), The Fork top right.

## How it's built

Region files can now cut canyons, with stepped walls and a darker floor, and they can hold zones: a zone has its tunnels, the paths from each tunnel to the prize, the prize's deposits and well, and the build spots. The region tool checks all of it: that every path runs on open floor joined to the rest, that each tunnel stands at the foot of a wall, that the spots and the prize lie inside the zone. On the plan below the red lines are the Brood's paths.

Bugs sent down a path walk it from the tunnel to the prize, 18 cells in 6 to 6.7 seconds, never more than 1.4 cells off the line, and start mining when they get there.

## The plan

- ![The region tool's preview of Cold Fork: the zone The Fork outlined in the north-east, two red paths from the tunnels to the prize, numbered build spots](https://ringshadow.com/blog/media/the-fork/08-plan.webp) The region tool's preview of Cold Fork, The Fork outlined top right.

Every test suite passes: 494 core, 249 server and 85 network.

### Coming ([landed since](https://ringshadow.com/blog/canyon-game))

The waves. The first wave replays until the pilots beat it, then they grow harder, each paying the pilots who fought. If a wave carries off too much of the prize, [the Matriarch](https://ringshadow.com/blog/matriarch) comes out of her nest and fells every turret in The Fork, and it starts again from wave one.
