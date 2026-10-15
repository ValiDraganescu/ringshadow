# The realm under siege: the M8 showreel

> The M8 showreel: 67 seconds of the Brood, the Citadel's guns, the Dropship, The Fork and the Matriarch, cut the morning after I asked the agents for all of it.

Source: https://ringshadow.com/blog/realm-under-siege

15 October 2026 · M8 · The showreel

On the evening of 14 October I played the online mode up to level 10 and asked "now what?". By 08:14 the next morning the agents had built the answer and cut a reel of it: 67 seconds of the Brood, the Citadel's guns, the Dropship, The Fork and the Matriarch, to one of our own songs.

[Video: The M8 showreel: a passage into the next valley, the Citadel's screens, the Brood at the ore, the corner guns, the Dropship's flight, the satellite map, a wave down The Fork, the Matriarch felling the turrets and her nest](https://ringshadow.com/blog/media/realm-under-siege/showreel.mp4)

*The M8 showreel, 67 seconds, recorded from the game by an agent in hidden runs. The music is Rising Pressure, one of our own songs.*

Every beat of the reel has its own post on this blog, so this one is the tour. The times are where each beat starts.

## 0:00 · Dawn, and a valley without seams

Prospect Landing at dawn under the ringed giant, then out through a passage into the next valley. Region borders follow the land now, 14 passages join the 9 regions, and crossing on foot hands you from one region to the next without a seam. [The passages](https://ringshadow.com/blog/passages) and [the valleys](https://ringshadow.com/blog/valleys).

## 0:08 · The Citadel

The realm screen, the shop, the missions board and the Journal. The Citadel takes 10% of every unload into a realm wallet, sells four consumables and every blueprint, and offers missions in three tiers, each with a narration. The reel's one spoken line is the Warden's: "When the ring's shadow crosses the land, the ground goes cold and the dark comes alive." [The realm](https://ringshadow.com/blog/the-realm) and [the missions](https://ringshadow.com/blog/missions).

## 0:17 · The Brood

Shardlings at the ore, a Gloamwing diving, a Lodeback hauling ore off in its sac. Four kinds of bug, drawn to the places worth being, and with 17 to 19 of them on screen the mean frame time went from 10.42 ms to 10.77 ms. [The Brood](https://ringshadow.com/blog/the-brood) and [their art pass](https://ringshadow.com/blog/brood-art).

## 0:24 · The raid

KSTR calls a swarm heading for the Citadel, and its four corner guns answer. Then the gatling gun's red-hot amalgam rounds. [The defences](https://ringshadow.com/blog/defences) and [the turret upgrades](https://ringshadow.com/blog/turrets).

## 0:28 · The Dropship

Board at a Landing Pad, a banked turn past the giant, down on the pad in a new region, 16 to 20 seconds without a fade. [The Dropship](https://ringshadow.com/blog/dropship).

## 0:36 · The map

The world map is a satellite view of the real ground of every region you've seen, with tooltips on everything in it. [The world map](https://ringshadow.com/blog/world-map).

## 0:42 · The Fork

A wave down the canyon's arm, guns on the rim. Ten waves, 34 bugs in the last, and a wave breaks through when it carries off 300 ore of the prize. [The Fork](https://ringshadow.com/blog/the-fork) and [holding it](https://ringshadow.com/blog/canyon-game).

## 0:52 · The Matriarch

She comes out of the tunnel the wave came from, fells every turret in the canyon nearest first, and goes home to her nest in Slag Pockets, which is where the reel ends. [The Matriarch](https://ringshadow.com/blog/matriarch).

## Stills from the reel

- ![A Prospector at Prospect Landing at dawn, long shadows, the ringed giant low in the sky](https://ringshadow.com/blog/media/realm-under-siege/01-dawn.webp) Prospect Landing at dawn.
- ![The Prospector walking out through a passage between canyon walls into the next valley](https://ringshadow.com/blog/media/realm-under-siege/02-passage.webp) Out through a passage into the next valley.
- ![The Citadel's SHOP tab: blueprints and consumables with their prices](https://ringshadow.com/blog/media/realm-under-siege/03-shop.webp) The Citadel's shop: every blueprint, and four consumables.
- ![The Journal open on the mission Stand in the Shadow, its briefing playing, subtitled](https://ringshadow.com/blog/media/realm-under-siege/04-journal.webp) The Journal, with the Warden's briefing playing.
- ![Shardlings at an ore deposit, a Gloamwing diving, a Lodeback hauling ore off](https://ringshadow.com/blog/media/realm-under-siege/05-brood.webp) The Brood at the ore.
- ![The Citadel's corner guns firing at a swarm, KSTR: BROOD SWARM on the HUD](https://ringshadow.com/blog/media/realm-under-siege/06-corner-guns.webp) The raid: the Citadel's corner guns answer.
- ![A gatling gun firing red-hot amalgam rounds at a Gloamwing](https://ringshadow.com/blog/media/realm-under-siege/07-gatling.webp) The gatling gun's amalgam rounds.
- ![The Dropship in a banked turn past the ringed giant](https://ringshadow.com/blog/media/realm-under-siege/08-banked-turn.webp) The Dropship: a banked turn past the giant.
- ![The world map as a satellite view of the discovered regions](https://ringshadow.com/blog/media/realm-under-siege/09-world-map.webp) The satellite map.
- ![The opening wave coming down The Fork's arm, a Sentinel waiting on the rim](https://ringshadow.com/blog/media/realm-under-siege/10-fork-wave.webp) The Fork: a wave down the arm, guns on the rim.
- ![The Matriarch on the canyon rim between the turrets, her crown spread](https://ringshadow.com/blog/media/realm-under-siege/11-matriarch.webp) The Matriarch comes for the turrets.
- ![The Matriarch at her nest in Slag Pockets, egg pods glowing, Shardlings on guard](https://ringshadow.com/blog/media/realm-under-siege/12-nest.webp) Her nest in Slag Pockets.

## How the reel was made

An agent recorded each beat with a shot script that drives the game through hidden runs: the game runs invisible and silent, one run at a time on the whole Mac, and a clip mode drops the HUD for the clean moments. A cut file lists each shot's source, in and out points, zoom, focus, kicker and caption; a script pans, subtitles and joins them with half-second crossfades, and puts the end card over the key art. The music is Rising Pressure, one of our Suno songs, mixed at −16 LUFS. Between the first task of the milestone and the reel, 137 commits landed on the integration branch.

Before the reel, a stability pass fixed the flaky checks at their causes and reran everything on the current build: 27 shot sets, 6 flows, 92 of 92 automation tests, and the suites at 496 core, 267 server and 88 network tests, all passing.

The next milestone is a living frontier. Its first piece landed the morning after the reel: five pilots of the server's own in every world, with callsigns, paints and habits, playing by the same rules as you. That post is next.
