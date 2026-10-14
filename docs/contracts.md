# KSTR contracts

The KSTR radio, the frontier station on the dashboard, offers each pilot
short jobs. Each job pays XP and MH and gives the session a purpose: haul
ore home, drill out past the fence, fly somewhere new, put a building up.
Task K1 in [mmorpg-m7.md](mmorpg-m7.md). The server owns contracts and the
client only shows them.

Code: `unreal/server/world/WorldContracts.cpp`, with `WorldContracts` and the
contract members in `WorldMap.h`. Wire: `net::Contract` in
`unreal/Source/AutocraftNet/Public/NetProtocol.h`.

## The day

- **Three slots per pilot.** The server draws them on the pilot's first step
  of a world day.
- **The world day** starts at dawn (`Rules::sunrise`, 5:30) on the world
  clock. A day is 2,400 s (`WorldClock::dayLength`).
- **A finished contract** is paid at once and stays in its slot, marked
  done, until the next dawn. A new contract replaces it then.
- **Abandoning** an open contract costs nothing. Its slot stays empty
  ("abandoned") until the next dawn.
- **An open contract** carries over from one day to the next.
- **The dice** are seeded by the world (`WorldParams::contractSeed`, or the
  world id), the pilot's key and the day. The same pilot gets the same draw
  on the same day, even after a restart.
- **Variety:** a slot avoids a kind already open in another slot, while
  another kind is left.

## Kinds

| Kind | Asks | Counted from | Drawn when | Weight |
|---|---|---|---|---|
| deliverOre | 300 ore at level 1, +2% a level, rounded to 50 | ore unloaded by the pilot's Prospector | the pilot owns a finished Citadel or Hab Dome | 3 |
| deliverHydrogen | 60 MH, +2% a level, rounded to 10 | MH unloaded | as above, and a finished Derrick | 2 |
| drill | 6 + level/20 ore loads in a named region | ore loads drilled there | a region has ore left (another region than the pilot's, if one has ore) | 3 |
| reach | stand at a named place | the Prospector within 2 cells of a site, or on a pad's plate (4 cells) | a site the pilot hasn't opened and can walk to (a vault that only Cliff hop reaches: with Cliff hop), or a pad in another region | 3 |
| build | finish a building of a named kind | the pilot's own building of that kind finished | a blueprint the pilot has, at its floor, under its limit, that a Prospector builds | 2 |
| sweep | 6 + level/20 ore loads under a ringshadow sweep | loads drilled while `WorldMap::underSweep` | only with `WorldParams::sweepContracts` (off: see below) | 2 |

A reach contract names a site two times in three. It names a pad when no
site is left.

So a contract can always be done at the pilot's level. A new pilot without
a Citadel never gets a delivery. A build is always one its blueprints and
level allow. A place is always walkable, or a pad with a flight to it.

**Sweep is a stub.** E1's `Ringshadow::cover` is not committed yet. When it
is, `WorldMap::underSweep` should be true while the driven unit drills at
cover ≥ `Ringshadow::bonusCover`, and `sweepContracts` turned on.

## Pay

The reward is "8–12 minutes of normal play at that level". It uses the pace
model in [prospector-progression.md](prospector-progression.md): 4,500 XP an
hour at level 1, +2% a level, so 75 XP a minute at level 1.

| Kind | Minutes | XP at level 1 | Level 10 | Level 50 | Level 100 |
|---|---|---|---|---|---|
| deliverOre | 8 | 600 | 710 | 1,190 | 1,790 |
| deliverHydrogen | 10 | 750 | 885 | 1,485 | 2,235 |
| drill | 8 | 600 | 710 | 1,190 | 1,790 |
| reach | 12 | 900 | 1,060 | 1,780 | 2,680 |
| build | 10 | 750 | 885 | 1,485 | 2,235 |
| sweep | 10 | 750 | 885 | 1,485 | 2,235 |

XP is rounded to 5. MH is 10 + 2 × level, rounded to 5: 10 at level 1, 30
at level 10, 110 at 50, 210 at 100. The work itself still earns its own XP
on top, for example 1 XP for each ore delivered.

Three contracts a day at level 1 pay about 2,250 XP, about half an hour of
play. A pilot who plays one day (40 minutes) and does all three earns
about 1.8 times the normal rate for that day. Contracts lift play; they
don't replace it.

## The radio's voice

Each kind has a few title and pitch templates. `{n}` is the amount,
`{where}` the region's name, `{what}` the place or the building. The voice
is a frontier station: dry, warm, a bit wry, with no real-world names.

- **deliverOre**
  - *Stardust for the Stacks*: "KSTR, morning shift. The furnaces want {n} ore at your Citadel. Mind the dust."
  - *Full Bins*: "Somebody has to fill the bins, and today it's you: {n} ore, home before dark if you can."
  - *Ore Run*: "Plain work, honest pay. Haul {n} ore to your Citadel and the station sends something back."
- **deliverHydrogen**
  - *Quicksilver Order*: "We're low on the silver stuff. {n} MH to your Citadel keeps the lights on out here."
  - *Tank and Back*: "Fill up at your Derrick, bring {n} MH home. Don't spill it. It's expensive."
  - *Cold Fuel*: "The night freighters are thirsty: {n} MH, delivered. KSTR thanks you, and so do they."
- **drill**
  - *Survey Cores*: "Geology wants samples from {where}. Drill {n} loads there and tell them it's rock."
  - *Prove the Vein*: "Word is {where} runs rich. Drill {n} loads and prove the rumour, or bury it."
  - *Out Past the Fence*: "{where} hasn't heard a drill in a while. Give it {n} loads' worth of noise."
- **reach, a site**
  - *Something in {where}*: "A signal pings from the {what} in {where}. Go and stand on it. We'll wait."
  - *Lost and Found*: "The {what}, out in {where}. Nobody's been. Be the first, then tell us about it."
- **reach, a pad**
  - *Postcard from {where}*: "Land at the {what} pad in {where}. The view's on us."
  - *Check the Pad*: "The {what} pad in {where} hasn't answered in days. Fly over and wave."
- **build**
  - *Put Something Up*: "The Reach could use a {what}. You have the plans; we have faith. Mostly."
  - *Raise a {what}*: "A {what} of your own, welded and finished. The frontier goes up one bolt at a time."
- **sweep**
  - *Under the Shadow*: "When the ringshadow passes, keep drilling. {n} loads in the dark and the giant owes you."

## On the wire (protocol 7)

`ProgressMessage` (reliable, sent whenever it changes) carries:

- `contracts`: a list of `net::Contract`, three once drawn.
- `contractsDawn`: the world time (`Sky::time`) of the next dawn refresh.

Each `net::Contract` has:

| Field | Type | What it holds |
|---|---|---|
| `id` | uint32 | the pilot's own number, never reused |
| `kind` | Kind | deliverOre, deliverHydrogen, drill, reach, build or sweep |
| `state` | State | open, done or abandoned |
| `title`, `pitch` | strings | ready to show |
| `target` | string | drill: a region id. reach: `<region id>/<site name>` or `<region id>/pad/<pad name>`. build: a structure code name. Empty for the deliveries and sweep. |
| `region` | string | where to point on the world map (empty: anywhere) |
| `need`, `have` | int64 | the progress |
| `xp`, `hydrogen` | number, int64 | the pay |

Two more pieces:

- **News:** `NewsKind::contract` reports a finished contract. Its `id` is
  the contract's number as text, `xp` the XP it paid, and `value` the MH.
- **Request:** `RequestMessage::Abandon{contract}`, sent from
  `net::Client::abandon(id)`. The refusal texts are "No such contract",
  "That contract is done already" and "That contract is dropped already".

## Saved

Schema v5 adds `pilots.contracts TEXT`, holding JSON:
`{"day": n, "next": n, "list": [{id, kind, state, title, pitch, target,
region, need, have, xp, hydrogen}]}`. A pilot saved before v5 has no
contracts; its first step draws them.
