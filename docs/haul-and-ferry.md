# Haul and ferry (C4)

The user's C4 request (2026-10-19): the Dropship becomes a ship you walk
into, close, steer to its windshield and launch by hand; the Foundry sells
the **Mule**, a hauler the Prospector drives, the mobile ore and MH depot
that alone books a haul into the wallet once it is driven into a Citadel.
Concepts and the user's picks: [art/models/dropship/concepts](../art/models/dropship/concepts)
and [art/models/cart/concepts](../art/models/cart/concepts) (prompts.md beside
them). The user's words are the spec; numbers marked *assumed* are the
manager's, for the user to tune by feel.

## 1. The user's rulings

- **Dropship**: today's Dropship, its look kept (`art/models/zfight/dropship/after-3q.jpg`),
  made bigger so the Prospector and the Mule fit inside, with a rear ramp and
  an interior. Bay layout: concept **B2** (`shuttle-bay-2.jpg`): an orange
  ramp post by the opening, a green take-off button on the console under the
  glass. Windshield: **W2** (`shuttle-window-2.jpg`), three panes, wipers.
- **Bay inside**: 2 cells wide, 4 long, 2 tall (clear floor to ceiling).
- **Mule**: the driven hauler, concept **H2** (`cart-hauler-2.jpg`): six
  wheels, the Prospector standing at two levers on a front rail perch, an
  open ore hopper, MH tanks along the flanks. Bought at the Foundry.
- **Mule speed**: 30% of the Prospector's walk speed (`Rules::prospectorSpeed`): slow.
- **Mule capacity**: 1000, ore and MH combined (one ore = 1, one MH = 1).
  The load that fills it may go over 1000; at or over 1000 no new load goes in.
- **Mule screen**: a load screen on the Mule's side showing how much room is
  left (and what is in it).
- **Wallet**: mined ore and MH go into the Mule and stay booked to the Mule
  until it is driven into a Citadel's hall; only then do they reach the
  wallet. The Hab Dome takes no unloads any more.
- **Grab**: the ore or MH the Prospector holds in its gripper is drawn 2×
  bigger than before C4 (the user's ruling after seeing it 4×).
- **Citadel gate**: the button by the entrance lowers the gate, but it can
  then be closed neither from outside nor from inside. It must close again,
  from both sides (the manager's ruling: the outside button closes it too,
  so the C1 rule "closes only from inside" ends).

## 2. The Mule (core, net, server, save)

- A unit kind `mule` (drivable, a machine, `steers`), owned by the pilot who
  bought it. Only its owner drives it. *Assumed:* hp 300, the Brood can hit
  it, the Prospector repairs it like a building (the torch, the repair cost);
  destroyed, its load is lost; one Mule per pilot at a time.
- Size: it fits the bay (2 wide, 4 long, 2 tall inside) with its driver
  aboard, with room to drive in and out: body about 1.6 wide, 3.0 long, its
  driver's top under 1.9. It fits the Citadel doorway (3.0 wide, 3.0 tall).
- Bought at a finished realm Foundry: the Foundry's card (and its E menu in
  first person) offers "Mule". *Assumed price:* 600 ore + 150 MH from the
  buyer's wallet; it rolls out of the Foundry's door in about 20 s.
- Board: the Prospector at the Mule's perch (reach 2.0) presses E: it climbs
  on and drives the Mule (the generic drive of any unit). E again climbs off
  beside it. Space/click keep their acts.
- Fill: a Prospector carrying a load presses E at the Mule's hopper (reach
  2.0, on foot): the whole load goes in, shown in the hopper and on the
  screen. No XP and no wallet yet. Refused when the Mule holds 1000 or more
  ("The Mule is full"). The Mule never mines.
- Book: the Mule driven so its centre is inside a complete realm Citadel's
  hall (`footprint.inside()`): its whole load leaves it for the driver's
  wallet (ore and MH) with today's XP for an unload (1 an ore, 2 an MH),
  the feats and the skills that touch an unload (Stardust heart). One event
  "Booked" with the amounts, a toast.
- A Prospector walking its own load into a Citadel still unloads as today
  (the Citadel is the only depot). Hab Domes keep their supply and modules;
  the server's Hab Dome unload booking ends (the frontier pilots haul to
  Citadels).
- Wire: the Mule's load (ore, MH) on its unit in the snapshot; requests
  BuyMule, MuleFill (or the generic act); events Booked. Protocol bumps.
  Save: the Mule and its load persist with the pilot's units.

## 3. The Dropship (core, net, server)

The flight becomes the player's, in steps:

1. **Call**: at a realm Landing Pad the pilot calls the Dropship (as today,
   E at the pad). No destination and no fare yet. It comes down and lands;
   its ramp lowers.
2. **Board**: anyone walks up the ramp (a Prospector on foot, a Prospector
   driving its Mule). The bay is a walkable interior like the Citadel's
   hall (walls, ceiling, ramp as slabs that move with the ship).
3. **Ramp button** (the orange post inside by the opening): E closes the
   ramp; refused while someone stands on the ramp. On the ground with the
   ramp shut, E opens it.
4. **Take-off button** (the green button on the console under the
   windshield): E opens the flight menu (reachable pads, prices as today);
   choosing one pays the fare and lifts. Refused while the ramp is open
   ("Close the ramp first").
5. **Flight**: lift, turns, dash, approach as `Ferry` today. Everyone and
   everything in the bay rides along, untouchable, and can walk about the
   bay and look out of the windshield: the land and sky outside are real
   and go by. Rain on the windshield with its wipers (the visor's
   `ac::Wipers` and lens look on the glass).
6. **Land**: it touches down on the target pad with the ramp shut. The ramp
   button opens it; out they walk or drive.
7. **Leave**: once the bay is empty and the ramp has been open 5 s
   (*assumed*), the ramp closes and the Dropship lifts away.

The frontier pilots keep flying: the server presses the buttons for them.

## 4. The look (game)

- The Dropship model: today's, scaled up with a ramp that hinges down,
  the bay (B2), the ramp post, the console and its button, the windshield
  (W2, three panes) with two wipers, cabin lamps. The first-person camera
  inside works like the Citadel's hall (walls and ceiling stop it).
- The Mule model (H2) with the side load screen (room left, ore, MH), its
  hopper heap growing with the load, lamps, drive sounds.
- HUD: "In the Mule: N ore, M MH" while driving or near it.
- The held load in the gripper 2× bigger.
  As built (c4-gate): `AcPoseInfantry::HeldLoadScale` (2) on the model
  (`PoseProspector`) and in the first-person cockpit (`AcCockpitKinds`);
  `HeldLoadX` pushes the load out along the gripper as it grows so its back
  stays at the tines and clear of the hull (ore 0.117 cells further out, MH
  0.083). Before and after: [art/models/prospector/c4-grab](../art/models/prospector/c4-grab)
  (`*-3q`: the pose preview; `*-fp`, `*-tps`: the game by day,
  `-AcFlowTest=mmocitadel -AcMmoShots=DIR -AcMmoShotsGrab -AcMmoClockHour=11`,
  the load from the server's `carry ore|mh N` dev line). 4× was tried first:
  in first person it covered the lower middle of the view up to the
  crosshair, and the user ruled 2× on the model and in the cockpit alike.

## 5. Workers

| Worker | Scope |
|---|---|
| c4-gate | The Citadel gate closes from both sides (the bug); the held load 4× bigger |
| c4-mule-sim | Section 2: core, net, server, save, tests |
| c4-mule-look | Section 4's Mule: the model, the screen, the heap, the HUD, the game's drive and E acts on the sim's requests |
| c4-ship-sim | Section 3: core, net, server, the frontier pilots, tests |
| c4-ship-look | Section 4's Dropship: the model, the bay, the buttons, the windshield and wipers, the camera aboard, the view out in flight |
