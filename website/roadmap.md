# The ring of Ringshadow

> The Ringshadow roadmap: what is done, what comes next and what comes later, from the strategy game to the MMORPG mode.

Source: https://ringshadow.com/roadmap

The roadmap

Every milestone is a moon on the giant's ring. What is done orbits in the sunlight. What comes next is turning into the shadow, and what comes later is still deep inside it. Scroll, and the ring turns.

- DONE: built, in the sunlight
- NEXT: the work in line, at the edge of the shadow
- LATER: planned, deep in the shadow

In the sunlight · done

## The strategy game

The real-time strategy game, playable today.

### A frontier RTS (Done)

Prospectors drill Stardust Ore and pump Metallic Hydrogen on Ashfall Reach, while armies of up to eight players clash over the deposits.

- An engine-free core: the economy, pathing, combat and the AI in plain C++20
- Its tests build with clang and run in about 7 seconds

### Twelve playable units (Done)

Prospector, Ranger, Comet, Juggernaut, Firefly, Dropship, Longbow, Kestrel, Hailstorm, Peregrine, Atlas and Scorpion, each with its own cockpit, weapon and ability.

- The last three joined the roster designed and built: an air-superiority fighter, a heavy assault walker, a burrowing mine
- The Peregrine pilot and the Atlas crew speak with voices of their own

### Dive into any unit (Done)

Click any of your units and the camera dives in. Drive it like a shooter, in first or third person, and hand it back to the AI with Esc.

- Shooter feel and night sight

### The AI commanders (Done)

Your team's AI commander runs the army. The command map gives it a stance, objectives, expansions and requests, and it plans the rest.

- Building buttons the AI can handle queue a request for it

### Pilot leveling (Done)

Drive a kind of unit and every unit of that kind gets better. Each level offers two upgrades and you keep one: 216 of them across the twelve kinds.

### Day, night and weather (Done · 2026-10-12)

The sun crosses the sky and the beacons come on at night. The weather runs on the game clock: clear, cloudy, rain and storms.

- Volumetric clouds, cloud shadows, wet ground and hulls, lightning and thunder

### KSTR 88.7 Stardust (Done)

The music plays as a radio station with original songs and in-world ads between them.

In the sunlight · done

## The engine and the name

### The Unreal port (Done · 2026-10-05)

The game began as a Swift and SceneKit live wallpaper. It now lives in Unreal Engine 5 only, and the Swift version is retired.

- Open source and public domain (CC0) from 2026-10-06

### It's called Ringshadow (Done · 2026-10-07)

The game was called Autocraft. It's named now for the huge ringed gas giant in its sky, the one the prospectors live in the shadow of.

- The GitHub repository took the name on 2026-10-09

### Three platforms, v0.1.0 (Done · 2026-10-09)

Release builds for Windows and Linux on a build machine in AWS, and the Mac build at home. The first pre-release, v0.1.0, is on GitHub.

In the sunlight · done · 2026-10-13

## The MMORPG mode, built so far

One pilot in a lasting world. Single player first, on a local server that is the same program a public world will run: only the host changes. Playable today as a test mode.

### Drive through a server (Done · M1)

The server is its own program, and the game is only its client. You drive a Prospector through it, over our own protocol on UDP.

- An authoritative server on the engine-free core
- Snapshots of only what is near you, sent as differences
- Prediction and correction, tested under made-up lag, jitter and loss

### The world map and regions (Done · M2)

A world of regions side by side, generated or crafted by AI agents with a region tool. Drive across an edge into the next region, or fly from pad to pad.

- The first world: three by three regions
- Build wherever it fits, paid from your wallet

### A world that lasts (Done · M3)

The server saves the world, and your pilot comes back as you left it: levels and picks included.

- A save every 5 seconds and on stop
- Resume your world, or start a new one

### Flights and polish (Done · M4)

Flights lift off, cross and land. The world map takes the keyboard, and the HUD is cut down to one pilot.

- Every screen checked picture by picture with a shot harness

### Ready to ship (Done · M5)

The traffic is encrypted, and the server travels inside the Mac package. The Metallic Hydrogen loop is proven: pump at a Derrick, unload at your Citadel, fill the wallet.

- X25519 and XChaCha20-Poly1305 through Monocypher (CC0)

In the sunlight · done · 2026-10-14

## A pilot's long road, under a living sky

A hundred hours of progression and a world with its own day, weather and events. The story is on the [blog](https://ringshadow.com/blog/how-we-got-here).

### A hundred levels and a living sky (Done · M6 · 2026-10-14)

The Prospector levels to 100, with a skill tree of 65 nodes in five branches. Buildings come from blueprints found in the world. The server owns the clock and the weather.

- Level 10 in about an hour, level 100 in about a hundred
- A 40-minute world day: 30 of light, 10 of night
- Foundry modules and Lab research

### A world worth crossing (Done · M7 · 2026-10-14)

The ringshadow sweep, starfall, KSTR contracts, blueprint sites you can see from afar, flights that burn MH, a guided first ten minutes and a new home screen.

- Starfall: two or three a world day, a crater with a rich deposit
- Three radio contracts a day

The terminator · next

## M8: the realm under siege

Buildings become the realm's, and the Prospector gets enemies. Parts have landed; the rest is being built.

### The realm, the Brood and the defences (Next · M8)

Buildings belong to the realm, the Citadel taxes, sells and sends you on missions, and alien bugs swarm the places worth being. Now the realm has guns to hold them off, and the Brood has its queen.

- Landed: the realm's buildings, the tax and the shop, missions and the Journal, the Brood, a world server that runs on after you quit, mercenary prospectors, the satellite map, the Citadel's turrets, the AA tower and the gatling gun, turret upgrades, the Matriarch at her nest
- Then: canyon defence in waves, regions that follow the land, the Dropship between landing pads

The terminator · next

## Next, in single player

Each step ships in the single-player game, on the local server.

### AI pilots (Next)

AI characters who are pilots themselves replace the AI commanders: a character, a city and a level, like a player. They mine, build, take contracts and fight, and keep the world busy where players are few.

### Cities (Next)

A city forms where pilots build close together. Its pilots raise it with ore, MH and hours of labour, and each level opens more. Cities are the safe places.

### More regions (Next)

A bigger world map, and travel between its regions.

### No-man's land (Next)

The open ground between the cities: claims, dwellings and the fight over the deposits and wells.

Deep in the shadow · later

## Later, online

The same server program, out in the world.

### The public server (Later)

The server on a public host, accounts and login, and other players in your world.

### The market, crews and chat (Later)

Trade between pilots, crews that ride together, and a way to talk.

### Thousands of pilots (Later)

Regions on separate servers, a pilot handed from one to the next at the border, and copies of the crowded places.

### Hosting (Later)

The official world on the Ringshadow AWS account, and a server build anyone can run for a world of their own.

Taken from the design docs and the work log in the [repository](https://github.com/ValiDraganescu/ringshadow). Nothing after the sunlit moons has a date yet.
