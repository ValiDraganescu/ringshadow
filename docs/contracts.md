# KSTR contracts

The KSTR radio, the frontier station on the dashboard, offers each pilot
short jobs. Each job pays XP and MH and gives the session a purpose: haul
ore home, drill out past the fence, fly somewhere new, put a building up.
Task K1 in [mmorpg-m7.md](mmorpg-m7.md). The server owns contracts and the
client only shows them.

The realm's Citadel posts missions of its own on a board (K2,
[missions.md](missions.md)): a separate system that counts the same kind
of work. These daily radio contracts are unchanged.

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

**Later: make a module.** Since P7 the Foundry makes rig modules
([prospector-progression.md](prospector-progression.md), "Foundry and Lab").
A future kind could ask for one ("make a Mk II cargo pod"), counted on the
`made` news as the build kind counts finished buildings, and drawn only for
a pilot who owns a Foundry. Not drawn yet.

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

## On screen

- **Compact.** By default the panel is one small line a contract over the
  dashboard's right edge ("KSTR · CONTRACTS", "T more"): its key, the title
  and a thin bar, or PAID / ABANDONED. **T** opens the whole panel and
  closes it again; a contract that changes (more ore home, done, a new one)
  opens it for 6 s by itself, and an armed abandon keeps it open.
- **The panel.** The three slots on a panel that stands on the dashboard
  over its right well and grille ("KSTR 88.7 · CONTRACTS", "T less"). Each row: its
  key (1, 2, 3), the title, the pay ("+600 XP · +15 MH"), the radio's line,
  and a bar with what is left ("120 / 300 ORE HOME", "2 / 6 LOADS · COLD
  FORK", "REACH LANDER WRECK · COLD FORK"). A done one says PAID and "NEW
  ONE AT DAWN, IN 23:41"; an abandoned one ABANDONED and "BACK AT DAWN, IN
  23:41", against the world's clock. The build menu (B) takes the panel
  while it is open.
- **Abandon.** X arms it (the open rows ringed red, "ABANDON WHICH? 1 2 3");
  1, 2 or 3 then drops that slot's contract (`Client.abandon`); X again or
  Esc keeps them, and it disarms by itself after 6 s.
- **Done.** A toast: "CONTRACT DONE: PROVE THE VEIN", "+710 XP · +30 MH".
- **Where.** Open reach and drill contracts ring their region green on the
  world map with "KSTR · <title>"; a reach's site or pad gets a green
  target. In the pilot's region a reach's place shows on the compass
  ("KSTR 23").

Code: `AcProgressHud::Contracts`, `ContractMarks`, `ContractGoals`;
`SAcConsole::PaintContracts`, `PaintContractsCompact`; X and T in `AcCommandMap.cpp`.

## First ten minutes

A new pilot lands knowing nothing. Before the daily three, KSTR walks it
through the game with a chain of six short jobs, one at a time (task O1).
Each job is a contract in the panel, so it uses the same toast, marks and pay
as any other. Each one opens the moment the last is paid:

| Step | Title | The job | Done when | Pays |
|---|---|---|---|---|
| 1 | Touchdown | walk to the Landing crate by the pad | the Citadel's blueprint is found (the crate opens on touch) | 100 XP, 5 MH |
| 2 | A Roof over Your Head | build the Citadel | the pilot owns a finished Citadel | 150 XP, 5 MH |
| 3 | First Haul | drill a load of ore, bring it home | 5 ore unloaded at its buildings | 200 XP, 10 MH |
| 4 | The Wreck on the Rise | hold the act key 5 s on the Lander wreck by the wells | the Derrick's blueprint is found (that wreck, or Brine Pans' Barge wreck) | 150 XP, 10 MH |
| 5 | Liquid Gold | build a Derrick on a well, pump it once | a finished Derrick (1 of 2), then a tank filled in it (2 of 2) | 300 XP, 15 MH |
| 6 | Make It Yours | spend a skill point (K) | any point spent | 100 XP, 10 MH |

After step 6 the daily three are drawn at once. The design put the wreck
last, but a new pilot's only Derrick blueprint is in that wreck, so it comes
before the Derrick.

- **Levels.** On top of the chain's pay, the work pays by itself: the
  Citadel's blueprint 50, its welding 200 and feat 150, a load 5, the
  Derrick's blueprint 150. That gives 150 XP after step 1, then 650 (level
  2, a point to spend), 855, and 1,155 after step 4 (level 3, the Derrick's
  floor). The Derrick's welding and feat with step 5's pay make about 1,590
  (level 4), and the chain ends near 1,690. The server test plays the whole
  chain: 1,693 XP, level 4, 55 MH.
- **In order.** Only the open step counts. Work done early for a later step
  (the wreck opened first, a point spent at level 2) makes that step pass as
  soon as it opens, and it is paid then. Steps 1, 2, 4 and 6 are checked
  against what the pilot has (a blueprint, a building, a point spent), so
  they survive a restart. Step 3 counts ore unloaded, like a delivery. Step
  5 counts the Derrick, then the first tank filled while the step is open.
- **One at a time.** While the chain runs, the pilot's contracts are the
  open step and, under it, the step just paid (PAID), so the client can
  name it in the "CONTRACT DONE" toast. The chain's steps can't be abandoned
  ("The radio's first jobs can't be dropped").
- **Who gets it.** Every new pilot starts at step 1. A pilot kept from a
  save made before the chain skips it: its contracts JSON has no `"first"`.
  A dev-rig pilot (`--dev-rig`) skips it too. Dev and pictures:
  `--first-step N` on the server, `-AcMmoFirstStep=N` in the game (1–6; 0
  for none).
- **On screen.**
  - The panel's head reads "KSTR · FIRST STEPS 3 / 6" in gold.
  - The open step is drawn in gold with its hint, for example "DRILL A LOAD,
    BRING IT HOME 2 / 5 ORE" or "HOLD CLICK ON THE LANDER WRECK · 5 S". In
    the compact panel the hint is a second line under the title.
  - The cockpit's tip line under the card is the step's tip alone, for
    example "Hold click drill ore at a deposit · walk into your Citadel to
    unload".
  - Each step that opens shows a toast "KSTR: FIRST HAUL" with its hint, and
    the panel opens for 6 s.
  - The compass and the world map mark the step's place: the crate and the
    wreck (reach), the pad to build by (2), the nearest ore (3), and the
    nearest well, then the pilot's Derrick (5).
  - The radio says where: a step's site (the crate, the wreck) goes on the
    pilot's map when the step opens, so its gold column stands over it.
- **The radio's lines.** `{what}` is the site's name.
  1. *Touchdown*: "KSTR to the new pilot: welcome down. There's a crate by the pad with your name on it, more or less. Go and open it."
  2. *A Roof over Your Head*: "Those are Citadel plans. Put one up near the pad. Nights out here are long, and the dust gets into everything."
  3. *First Haul*: "Find the ore, drill a load, bring it home to your Citadel. That's the whole job. Some pilots do it for years."
  4. *The Wreck on the Rise*: "See the {what} up by the wells? A lander that didn't. Hold on its hatch; whatever's inside is yours now."
  5. *Liquid Gold*: "Derrick plans, nice. Build one on a well and pump it once. Metallic hydrogen flies you and pays you. Don't spill it."
  6. *Make It Yours*: "You've earned a skill point. Spend it on yourself: nobody out here stays the pilot they landed as. KSTR out, for now."

Code: `WorldMap::firstTen`, `firstContract` and `pumped`, and
`WorldContracts::firstXp` and `firstHydrogen` (WorldContracts.cpp); tests
in `unreal/server/tests/FirstTenTests.cpp`. Client:
`AcProgressHud::FirstStep`, `FirstHint` and `FirstTip`; pictures from
`-AcMmoShotsO1`.

## On the wire (protocol 7)

`ProgressMessage` (reliable, sent whenever it changes) carries:

- `contracts`: a list of `net::Contract`, three once drawn.
- `contractsDawn`: the world time (`Sky::time`) of the next dawn refresh.

Each `net::Contract` has:

| Field | Type | What it holds |
|---|---|---|
| `id` | uint32 | the pilot's own number, never reused |
| `kind` | Kind | deliverOre, deliverHydrogen, drill, reach, build, sweep or skill (spend a point; the first ten minutes only) |
| `step` | uint8 (3 bits) | the first ten minutes' step, 1–6; 0 for a daily contract (protocol 11) |
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
region, need, have, xp, hydrogen, step}], "first": n}`. A pilot saved before v5 has no
contracts; its first step draws them. `"first"` is the open step of the
first ten minutes. A pilot without it is not on the chain: an old save's
pilot skips it.
