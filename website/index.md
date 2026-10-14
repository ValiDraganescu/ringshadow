# Ringshadow

> Ringshadow: a free real-time strategy game for Mac, Windows and Linux. Your AI commander runs the army, and you can dive into any unit and play it like a shooter. Built in public with AI agents, public domain (CC0).

Source: https://ringshadow.com/

[Video: The camera dives from the top-down view into a Ranger, who walks out in first person](https://ringshadow.com/assets/media/dive.mp4) ![The command map: the bases of the map and the team's objectives](https://ringshadow.com/assets/media/command-map.jpg) ![The top-down view: the Citadel, its Stardust Ore line and two Metallic Hydrogen wells](https://ringshadow.com/assets/media/top.jpg)

## Play it three ways

### Drive any unit

Click one of your units and the camera dives in. `W``A``S``D` walks, the mouse aims, a click fires, `R` uses its ability, `V` switches first and third person, and `Esc` hands it back to the AI.

### Command the AI

Press `M` for the command map and give your team's AI its orders: a stance, objectives (attack, defend, hold, man a Bastion), expansions and requests. It plans the rest.

### Press the buttons

Click a building and press its buttons: train units, research, build. If it's a job the AI can do for you, the button queues it for your team's commander.

[Meet the twelve](#units)[How units level](#levels)

## Twelve you can drive

Each kind has its own cockpit, weapon and ability. Prospectors drill Stardust Ore and pump Metallic Hydrogen while armies of up to eight players fight over the deposits.

### On the ground

9 kinds

Prospector Worker: Mines ore, pumps MH, builds and repairs. Dull to drive, and the reason leveling exists.

Ranger Infantry: Rifle infantry. Its upgrades lean to firing and staying alive.

Comet Raider: A jump-pack raider with pistols that clears cliffs.

Juggernaut Grenadier: A heavy grenadier that breaks armour and slows what it hits.

Firefly Buggy: A fast flame buggy. Its upgrades lean to burning and the flame's reach.

Longbow Tank: A tank that anchors into artillery. `R` anchors it.

Hailstorm Flak: A flak half-track that hits only flyers.

Atlas Walker: A heavy assault walker. `R` fires its Quake stomp; its crew speaks with voices of its own.

Scorpion Mine: A burrowing mine. `R` buries it and digs it out again.

### In the air

3 kinds

Dropship Transport: A healing transport. `R` loads and unloads.

Kestrel Gunship: A rocket gunship that strikes only the ground.

Peregrine Fighter: An air-superiority fighter. `R` is the afterburner; its pilot has a voice of its own.

## A cockpit for each

Every kind is driven from its own seat, with its own weapon or tool in view. These are frames from the game, as you see them when you dive in.

## Drive it and it levels

The longer you drive a kind of unit in a game, the better every unit of that kind gets. Units the AI runs never level, which gives you a reason to drive the dull ones.

![A level-up: two upgrade cards over the Prospector's cockpit](https://ringshadow.com/assets/media/level-cards.jpg)

- Drive: Only the units you drive yourself earn levels. Hand one back with `Esc` and it stops.
- Level: Each level lifts every unit of that kind on your side, for the rest of the game.
- Keep one: Every level offers two upgrades and you keep one: 216 across the twelve kinds, like "every Prospector pumps MH 20% faster".

## Life under the giant

[Video: A Prospector drilling Stardust Ore in first person](https://ringshadow.com/assets/media/mining.mp4)

*Ore and hydrogen A Prospector drills Stardust Ore in first person.*

![The base at night, its beacons lit](https://ringshadow.com/assets/media/top-night.jpg)

*Night The sun crosses the sky; after dark the beacons and lamps come on.*

KSTR 88.7 Stardust The music plays as a radio station, with original songs.

Weather Sun, cloud, rain and storms on the game clock, wet ground and hulls after.

[Video: The camera pans across the base at dusk](https://ringshadow.com/assets/media/flyover.mp4)

*Dusk The camera pans over a base as the light goes.*

Ads between the songs Pumped from Metallic Hydrogen wells. "Not a beverage."

Face scrub Side effects: levitation, being mined.

Real estate Ash view guaranteed.

Everything above is the strategy game, playable today. Down here in the ring's shadow, the online world is being built.

## One pilot. A world that lasts.

![The giant's rings casting a dark band across the sky and land of the MMORPG world](https://ringshadow.com/blog/media/how-we-got-here/06-ringshadow.webp)

### Playable now, as a test mode

Single player, on a local server that is the same program a public world will run. Your Prospector climbs to level 100 over about a hundred hours. A Citadel taxes, sells and hands out missions, mercenaries mine while the game is closed, and the Brood raids your buildings and eats your ore.

### Coming

Cities that pilots raise together with ore, MH and hours of labour. AI pilots with a city and a level of their own. And the no-man's land between the cities, where deposits and wells are taken and held by force.

[Read how it's being built](https://ringshadow.com/blog/)

## How it's made

### Concept to model

The model pipeline

![The Atlas six-view turnaround](https://ringshadow.com/assets/media/atlas-turnaround.jpg) Each unit starts as concept paintings. The one picked becomes a six-view turnaround, and the in-engine model is built part by part with Geometry Script and fitted against those views. The Atlas came to the game this way, and every snag along the road is written into the skill the agents follow.

[The model pipeline skill](https://github.com/ValiDraganescu/ringshadow/tree/main/.claude/skills/ringshadow-model-pipeline)

### An engine-free core

C++20, tests in 7 seconds

[Video: Two armies of twelve meet in the desert, seen from above](https://ringshadow.com/assets/media/fight.mp4) The whole simulation, from the economy and pathing to combat and the AI commanders, is plain C++20 with no Unreal in it. Its tests build with clang and run in about 7 seconds. The Unreal layer draws the frames, plays the sounds and takes the input.

[How the core is split](https://github.com/ValiDraganescu/ringshadow/blob/main/unreal/PORTING.md)

### Made by AI agents

The skills are in the repo

[Video: A Ranger walking in third person, recorded by the game's own recorder](https://ringshadow.com/assets/media/third.mp4) Ringshadow is made with AI coding agents, and the skills they work from are published with the code: the buildings' light language, which tests a change needs, and the recorder that makes every clip on this page from the real game.

[The skills](https://github.com/ValiDraganescu/ringshadow/tree/main/.claude/skills)

### Built in public

The blog and the roadmap

![A Prospector on its landing pad in the MMORPG mode, the ringed giant behind the clouds](https://ringshadow.com/blog/media/how-we-got-here/cover.jpg) The blog follows the work as it lands, with the real numbers and the pictures from the day. The roadmap shows what is done and what comes next, and nothing past the done work has a date.

[The blog](https://ringshadow.com/blog/) [The roadmap](https://ringshadow.com/roadmap)

## Take any of it.

The game is free, and everything in the repository is public domain under CC0 1.0: the code, the art, the models, the sounds, the music and the docs. Use any of it for anything, with no credit needed.

All three builds and their checksums are on the [0.1.1 release page](https://github.com/ValiDraganescu/ringshadow/releases/tag/v0.1.1).
