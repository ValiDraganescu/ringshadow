# Ringshadow

> Ringshadow: a real-time strategy game where the AI runs your army, and you can dive into any unit and fight it yourself. Built in public with AI agents, public domain (CC0).

Source: https://ringshadow.com/

The pitch

## Live in the giant's shadow

On a frontier world, Prospectors drill Stardust Ore and pump Metallic Hydrogen while armies of up to eight players clash over the deposits.

Your team's AI commander runs the army while you steer it from above. When a fight matters, you take over a unit and play it like a shooter. Overhead hangs Ringshadow, the huge ringed gas giant the game is named for.

[Video: The camera dives from the top-down view into a Ranger, who walks out in first person](https://ringshadow.com/assets/media/dive.mp4)

*Click a unit and the camera dives in. Recorded in the game.*

How you play

## Three ways, switch any time

01 ![The top-down view: the Citadel, its Stardust Ore line and two Metallic Hydrogen wells](https://ringshadow.com/assets/media/top.jpg)

### Your buildings

Click a building and press its buttons: train units, research, build. If it's a job the AI can do for you, the button queues it for your team's commander.

02 ![The command map: the bases of the map and the team's objectives](https://ringshadow.com/assets/media/command-map.jpg)

### The command map `M`

Give your AI orders: a stance, objectives (attack, defend, hold, man a Bastion), expansions and requests. It plans the rest.

03 [Video: Two armies of twelve meet in the desert, seen from above as the camera sweeps in](https://ringshadow.com/assets/media/fight.mp4)

### Drive a unit

Click any of your units and dive in. `WASD` walks, the mouse aims, a click fires, `R` uses its ability, `V` switches first and third person, `Esc` hands it back to the AI.

The roster

## Every unit is playable

Twelve kinds, each with its own cockpit, weapon and ability.

- Prospector: Mines ore, pumps MH, builds, repairs
- Ranger: Rifle infantry
- Comet: Jump-pack raider, clears cliffs
- Juggernaut: Heavy grenadier
- Firefly: Fast flame buggy
- Dropship: Healing transport
- Longbow: A tank that anchors into artillery
- Kestrel: Rocket gunship
- Hailstorm: Flak half-track
- Peregrine: Air-superiority fighter
- Atlas: Heavy assault walker
- Scorpion: The twelfth: a burrowing mine. And any unit can be driven over the shoulder too `V`.

[Video: Driving a Comet in first person](https://ringshadow.com/assets/media/comet.mp4)

*A Comet in first person.*

[Video: A Ranger walking in third person](https://ringshadow.com/assets/media/third.mp4)

*A Ranger in third person.*

Pilot leveling

## Units level up while you drive them

The longer you drive a kind of unit in a game, the better every unit of that kind gets, but only while you're the one driving: units the AI runs never level.

Every level offers two upgrades and you keep one: 216 of them across the twelve kinds ("Every Prospector pumps MH 20% faster"). It gives you a reason to drive the units that are dull to play, the Prospector first.

![A level-up: two upgrade cards over the Prospector's cockpit](https://ringshadow.com/assets/media/level-cards.jpg) [Video: A Prospector drilling Stardust Ore in first person](https://ringshadow.com/assets/media/mining.mp4) The world

## Day, night, weather and the radio

The sun crosses the sky, and at night the beacons and lamps come on. The weather runs on the game clock: sun, broken cloud, rain and storms with lightning, wet ground and hulls.

The music plays as a radio station, KSTR 88.7 Stardust, with original songs and in-world ads between them: Quicksilver Cola (pumped from Metallic Hydrogen wells, "not a beverage"), Opal Glow face scrub (side effects: levitation, being mined) and Citadel Timeshares (ash view guaranteed).

[Video: The camera pans across the base at dusk](https://ringshadow.com/assets/media/flyover.mp4) ![The base at night, its beacons lit](https://ringshadow.com/assets/media/top-night.jpg) In the works: the MMORPG mode

## One pilot in a world that lasts

A persistent online world is being built next to the strategy game: you are one pilot who lives in the ringshadow and drives the machines yourself.

### Your level is yours

Your Prospector climbs to level 100 over about a hundred hours, with a skill tree of 65 nodes, and keeps every level between sessions.

### Cities built together

Pilots raise their cities with ore, MH and hours of labour. Cities are the safe places to log in, trade and meet.

### No-man's land

Between the cities lie the deposits, the wells and dwellings, taken and held by force. That's where the fighting happens.

### AI pilots

AI characters who are pilots themselves, with a city and a level, keep the world busy where players are few.

You can play it today in a test mode, single player, on a local server that is the same program a public world will run. So far it has our own encrypted server on UDP, a world of regions that the server saves, the ringshadow sweep and starfall, a realm whose Citadel taxes, sells and hands out missions, mercenaries who mine while the game is closed, and the Brood: alien bugs that raid your buildings and eat your ore.

[Read how it's being built](https://ringshadow.com/blog/) [The roadmap](https://ringshadow.com/roadmap) Built in public with AI agents

## How it's made

### An engine-free core

The whole simulation, from the economy and pathing to combat and the AI commanders, is plain C++20 with no Unreal in it. Its tests build with clang and run in about 7 seconds. The Unreal layer draws the frames, plays the sounds and takes the input.

### Made by AI coding agents

Ringshadow is made with AI coding agents, and the skills they work from are in the repository: concept art to an Unreal model, the buildings' light language, which tests a change needs, and the recorder that makes every clip on this page from the real game.

### From concept to model

Each unit starts as concept paintings. The one picked becomes a six-view turnaround, and the in-engine model is built part by part with Geometry Script and fitted against those views.

![The Atlas concept painting](https://ringshadow.com/assets/media/atlas-concept.jpg)

*Concept*

![The Atlas six-view turnaround](https://ringshadow.com/assets/media/atlas-turnaround.jpg)

*Turnaround*

![The Atlas model in the engine](https://ringshadow.com/assets/media/atlas-model.jpg)

*In the engine*

Public domain

## CC0: take any of it

Everything in the repository is dedicated to the public domain under CC0 1.0: the code, the art, the models, the sounds, the music and the docs. Use any of it for anything, with no credit needed.

A few third-party files keep their own licences, listed in the README. The fonts on this site, Barlow Condensed and Exo 2, are under the SIL Open Font License ([Barlow](https://ringshadow.com/assets/fonts/OFL-BarlowCondensed.txt), [Exo 2](https://ringshadow.com/assets/fonts/OFL-Exo2.txt)). The sky at the top is drawn with three.js, under the [MIT License](https://ringshadow.com/assets/js/vendor/three.LICENSE.txt). The game is built with Unreal Engine 5, which has its own licence from Epic Games.

[The source on GitHub](https://github.com/ValiDraganescu/ringshadow)
