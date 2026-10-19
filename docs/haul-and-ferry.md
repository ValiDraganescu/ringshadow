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

### As built (c4-mule-sim, 2026-10-19)

The manager's rulings on the gaps (Q1 to Q6) and what the code settled:

- **The kind.** `UnitKind::mule` (raw `mule`, title "Mule"), appended after
  the Matriarch: 17 kinds, so a kind takes 5 bits on the wire. A new kind
  table flag `hauler`: not Brood, trained by no building (no RTS card, no
  AI), no leveling picks, not a soldier, still while nobody drives it.
  Drivable, `steers` with no turret (W/S drive, A/D steer: since c4-drive
  it turns as a car, see "The drive" below), a machine; hp 300, armour 1; 1.182 cells/s (0.3 ×
  `Rules::prospectorSpeed`), the driven hero's speed not added. Radius 0.8
  (half its width). Its numbers and points are `Mule.h`: capacity 1000,
  reach 2.0, body 3.0 long × 1.6 wide × 1.4 tall, the perch deck 0.35 up
  (the driver's top 1.85), the perch at (+1.0, 0) and the hopper's middle at
  (−0.55, 0) in its own frame (x ahead), price 600 ore + 150 MH, roll-out
  20 s, the toast `Mule::bookedToast` ("Booked · +300 ore, +40 MH").
- **The load.** `Unit::haul` (`Haul{ore, hydrogen}`), set on every Mule.
- **The rider.** Aboard, the pilot's Prospector has task `aboard` with
  `structure` = the Mule, rides on it (its position kept on the Mule's) and
  is out of reach of everything (as a Dropship's passenger); the Mule's
  `cargo` is {the rider}. Handing the Mule back (leaving it, going off line,
  the Mule switched for anything) sets the rider down beside the perch, on
  foot. Boarding needs the Prospector on foot within 2.0 of the perch and
  the seat free.
- **Fill.** `MuleFill{mule}`: the pilot's own Prospector, on foot with a
  load, within 2.0 of the hopper, its own Mule only ("Not your Mule");
  refused "Nothing to put in", "Get to the Mule's hopper", "The Mule is
  full" (at 1000 or more; the load that crosses 1000 goes in whole).
- **Book** (Q1). Each step, a driven Mule with a load whose centre is in a
  complete Citadel's hall (`Footprint::inside`: not the doorway) books it
  all to its driver's bank: Stardust heart and Silver tide count it, the
  realm's 10% is cut from ore and MH each (as today's unload), and
  `GameEvent::Booked{unit, ore, hydrogen, at, taxOre, taxHydrogen}` goes to
  everyone who may know (the game shows the toast to the Mule's owner).
  The server pays the unload's XP (1 an ore, 2 an MH), the first-MH feat,
  the deliver contracts, the haul missions and the week's ore.
- **Board and leave.** No new request: `Take{mule}` climbs aboard (its owner
  only, "Not your Mule"); `Take{own Prospector}` or `HandBack` while driving
  it climbs off.
- **Buy.** `BuyMule{foundry}` at a finished realm Foundry in the pilot's
  region (from anywhere in it, like the Foundry's modules), from the
  pilot's wallet; it rolls out 20 s later (the world's clock, on line or
  not) at the Foundry's door (+y), facing out, with a `Trained` event. One
  a pilot: "Your Mule is rolling out", "You have a Mule already"; a
  destroyed one frees the slot. The Progress message's `mule` tells the
  pilot its Mule: region, unit (0 rolling out), Foundry, `readyAt`.
- **Edges** (Q2). Driven over a region edge, the Mule crosses with its
  rider and load (`WorldMap::arriveMule`; the Dropship's bay can reuse it).
- **Destroyed** (Q3). Its load is lost, its rider set down unhurt, the
  pilot drives the Prospector again.
- **Off line** (Q4). The rider climbs off; the Mule stays where it stands,
  the Brood may hit it. Saved as a unit of the pilot's with its load
  (schema 29: `units.haul_ore`, `haul_hydrogen`); one still rolling out is
  saved out of the door; a rider is saved on its feet beside the perch.
- **Repair** (Q6). The driven Prospector mends it as a building: the key
  held at it within its work reach, a whole bar in 20 s at the hero's ×2
  (10 s), costing `Rules::repairShare` of 600 + 150 in whole units as it
  goes (`Repaired{mule, by, ore, hydrogen}`).
- **Hab Domes** take no loads: the core's unload, the frontier pilots'
  depot (their "Hab Dome by a far field" plans are gone), the deliver
  contracts and haul missions now need a Citadel. Hab Domes keep their
  supply, modules and respawn.
- **Wire** (protocol 41): requests BuyMule 26, MuleFill 27; event Booked 23;
  the Dropship's are 42 (requests from 28, events from 24).
- **Dev lines** (`WorldMap::devMule`): `mule foundry` (a complete Foundry
  ahead, the Prospector by its door), `mule here` (the Mule out at once
  beside the Prospector), `mule ready`, `mule door` (the Mule outside the
  realm Citadel's door facing in, the Prospector by its hopper), `mule load
  N [MH]`; since c4-drive `mule slope` and `mule steep` ("The drive").
- **Tests.** Core `MuleTests.cpp` (fill, full, overfill, board and ride and
  leave, speed, book in the hall not the doorway, the cut and Stardust
  heart, the Prospector's own unload, destroyed, repair, the Brood, the
  roll-out), `ProgressionEffects_habDomeTakesNoLoad`; net
  `MuleWireTests.cpp`; server `MuleTests.cpp` (buy and roll-out, owner
  only, fill and book with XP and the cut, full, an edge crossing,
  destroyed, saved and loaded, rolling out saved).

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

### How it is built (c4-ship-sim)

**The ship.** `ac::Shuttle` (Types.h) in `GameState::shuttles`: not a unit
or a building. Its frame is a point on the pad with a heading
(`position`, `heading`, `ground`); the bay's walls stand there through the
whole flight, so the riders keep ordinary unit positions and walk the bay with
the core's own pilot walk. The frame moves to the target pad at the dash's
end, the riders with it, each where it stood in the bay. Its `stage`
(descend, ground, lift, turns, dash, approach, depart) and `clock`, the ramp
(`Door`: 0 shut … 1 down, and the way it moves), the flight's `plan` and its
two pads (`from`, `to`, in the region's frame), the `riders` from the lift
to the touch down. Not saved: a pilot saved in flight comes back on the target
pad, as before.

**The shape** (`ShuttleBay`, Shuttle.h; ship frame: +x the nose, +y its
left, z up from the ground under the landed ship; c4-ship-look's model's
numbers): the deck at z 0.5; inside x −2.5 … 2.36 (the bay 4 long and the
cockpit nook up to the console's face), y −1 … 1, 2 tall; the hull from the
jambs at x −2.72 to the nose tip at 4.1, the nacelles out to ±2.48; the nose
block from x 2.62; the console x 2.36 … 2.68, |y| < 0.96, its desk up to z 1.08 under the lowered windshield; the ramp post in
its niche x −2.42 … −2.10, y −1.10 … −0.88; the ramp hinged at x −2.5,
z 0.5, 2 long, its foot on the ground at x −4.44, shut as a slab
x −2.70 … −2.50. The ramp button at (−2.26, −0.84, 1.13), the take-off
button at (2.50, 0, 1.20). `ShuttleBay::footprint()` gives the cameras the
same slabs; `ShuttleBay::floor` the deck's and the ramp's height to stand a
rider on.

**The pose** (`shuttlePose`, Shuttle.h): F2's path, moved from the client
into the core: down onto the pad from high behind it, on the pad, up off it,
two banking turns from its heading onto the way to the target pad, the dash,
the approach (landing with its nose along the way), lifting away empty. The
client calls it every frame with its own clock between snapshots.

**The walk.** `Simulation::pilotCanStand` asks the ships first
(`shuttleWalk`): the hull's sides, the nose, the console, the post and the
shut ramp (walkable from open 0.95, as the Citadel's door) block; on the
deck or a ramp that is down the ground under it does not matter. A rider in
flight walks only in its bay. A Mule meets them with its whole body (its
middle and its two ends along its 3.0 length), so in the bay its middle
stays between x −1.0 and 0.86. The AI's grid has a grounded ship's hull and
ramp solid (the Brood and the mercenaries never walk in); nobody aboard is
pushed out; nobody aboard crosses a region's edge on foot.

**The steps.** Call: the request `CallShuttle` (the pad the pilot stands
on, Pad link's reach), the ramp toward the caller, turned a quarter at a time
until the hull and the ramp lie inside the region; one ship to a pad ("The
Dropship is on its way", "The Dropship is here"). Its touch down moves
whoever stands under the hull out by its nearest side. Ramp: the request
`Ramp { open }`, within 1.5 cells of the ramp button inside (*assumed*;
measured from the nearest point of the pressing unit's body, a driven
Mule's box too); refused "Someone is on the ramp" (anyone's body over the
ramp's strip behind the opening), "Too far from the ramp button", "Wait for
the landing". Take-off: today's `Fly { region, pad }`, from within 1.5 cells
of the take-off button (*assumed*), the ramp fully shut ("Close the ramp
first"); the one who presses pays (half from a Spacedock, nothing with
Ringwalker, as F2); everyone whose middle is in the bay rides. In flight
every rider is untouchable (`Simulation::sheltered`), builds and demolishes
nothing ("In flight"). At the dash's end the server moves the ship and its
riders into the target region (each human's region change, its inputs
walking on; a frontier pilot's unit; anything else aboard as it stands). It
touches down with the ramp shut. Leave: nobody in the bay or on the ramp for
5 s (60 s after a call that nobody boarded; *assumed*), the ramp shuts (3 s,
*assumed*; it opens again for anyone who comes), it lifts away over 6 s and
is gone.

**Wire** (protocol 42; 41 and the requests 26, 27 and the event 23 kept for
the Mule): `Snapshot::shuttles`, the region's ships whole in every message's
header (one bit when there are none), into the mirror's state;
`RequestMessage::CallShuttle` (28), `RequestMessage::Ramp` (29);
`Client::callShuttle()`, `Client::ramp(bool)`; the World message's
`flyingTo` names the target region for a rider from the lift to the touch
down (the client builds that ground ahead). Its `ferry` and the sky's
`Human::ferry` are no longer filled.

**Frontier pilots** (docs/frontier.md): live, they call the ship and the
server presses the buttons for them; booked, they fly as before.

**Dev lines** (`ferry ...`): `pads` (a Landing Pad under the pilot, one in a
region next door, seen, 200 MH; the reply names the target), `call`,
`board` (a ship on the pad at once, shut, the Prospector at its take-off
button), `ramp open|shut`, `mule` (the pilot's Mule, made if it has none,
on the grounded ship's axis 3.5 cells behind the ramp's foot, facing it,
its Prospector on foot beside the perch: the game takes it).

**Tests.** Core: ShuttleTests.cpp (the shape, the landing, the walk up the
ramp, the ramp's and the take-off's refusals, the riders untouchable, a hop
carrying its riders, the waits, the pose, the AI's grid). Net:
ShuttleWireTests.cpp. Server: ShuttleTests.cpp (two riders, refusals, the
World message), RolesTests' and WorldTests' flights, FrontierTests'
`Frontier_fliesTheWalkInDropshipWhereSeen`, the end-to-end world flight.

## 4. The look (game)

- The Dropship model: today's, scaled up with a ramp that hinges down,
  the bay (B2), the ramp post, the console and its button, the windshield
  (W2, three panes) with two wipers, cabin lamps. The first-person camera
  inside works like the Citadel's hall (walls and ceiling stop it).
  As built (c4-ship-look, 2026-10-19):
  - **The model** `ferry_blue` (`unreal/Tools/Editor/models/ferry.py`, a
    Geometry Script model; the strategy game's `dropship_blue` is left as
    it is): today's look grown round the bay, about 6.8 cells long, 5 wide
    over the nacelles and 4.5 high to the fins (grey hull, the team's blue
    side panels and four leg pods at the corners, the nacelles high at the
    back with open octagonal chrome intakes and exhausts, fans and vanes
    in them, twin fins with red beacons, white crosses on the roof and
    the sides, orange lamps). Its frame is the sim's (`ShuttleBay`): the
    deck at 0.5, the bay x −2.5 … 1.5 with the cockpit nook to the console
    at 2.36, ±1.0 across the ribs, 2.0 to the ribs' undersides. Inside
    (B2): octagonal ribs, wall boxes with cyan lamps, pipe runs, four warm
    `cabinLamps_k` with their lights, chrome tie-down rails and hazard deck
    edges flush with the deck; the orange ramp post in a niche left of the
    opening (its hazard plate, the button, `rampLamps_0` orange = open,
    flashing while it moves, `rampLamps_1` green = shut); the console
    under the glass (x 2.36 … 2.68, desk 0.96 … 1.08) with switches, two
    screens and the green `takeoffButton` (a glossy dome with a lit core in
    a dark bezel on a chrome housing, a hazard collar; the core lit while
    the ship can go). The windshield (W2): three panes (the side ones swept
    back) in a riveted frame from 1.20 up to 2.48 (low, so the ground ahead
    shows through it; the nose under it drops away), pillars in blue and
    white bands, two wipers on the centre pane.
    The rear ramp hinges at the deck's back edge: down it lies on the
    ground 2.0 behind (104.5°). Renders with a Prospector for scale:
    [art/models/dropship/c4](../art/models/dropship/c4) (`model-*`).
  - **The pose** (`AcPoseFerry.h`, tested in `Autocraft.Pose.Ferry.*`):
    the ramp on its hinge, the buttons pushed in 1.5 and 2 cm for a
    quarter second on each press, the lamps, the wipers turning in the
    glass's plane on `ac::Wipers`'s sweep (mapped onto −78° … 62°), the
    fans and jets with the thrust, the beacons.
  - **Drawn** by `AAcShuttleShip` (`AcShuttleShip.h`), one actor per ship
    of the shown state, every frame at `ac::shuttlePose` with the stage's
    clock run on between snapshots (`AcShuttleView.h`). Everything aboard
    is worked out in the ship's frame on the pad and carried up by
    Frame⁻¹·Pose: the riders (the world renderer stands them on the deck
    or the ramp and lifts them), and the pilot's first- and third-person
    view (the pawn sweeps its cameras against the bay's walls,
    `ShuttleBay::footprint` added to `AcPilotWalls` as turned slabs, then
    carries the view), so the bay stays still round the pilot while the
    ship banks. At the switch into the target region the ship leaves the
    shown state for a moment: it is carried on from its last state in the
    new world frame (up to 3 s) until the target region's copy comes, and
    its actor is kept under the new number.
  - **The acts** (C5: one rule for all of them since 2026-10-19,
    [acting.md](acting.md): the crosshair on the call post's button, the ramp
    post or the take-off button, within reach; `AcShuttleAct` is gone into
    `AcActPick`). Before C5, on E when nothing else took it: on a
    finished realm Landing Pad with no ship on it, "E · CALL THE DROPSHIP";
    in the bay by the ramp post "E · OPEN THE RAMP" / "E · CLOSE THE RAMP";
    at the console "E · TAKE OFF", which opens the world map as the flight
    menu (FLY sends `Fly`). The core's refusals ("Someone is on the ramp",
    "Close the ramp first", "Wait for the landing", "In flight") are the
    prompt in the blocked tone and the key sends nothing; the server's
    come back as notes (`FAcMmoClient::CallShuttle`, `Ramp`,
    `ShuttleRefusal`). Driving a Mule (the manager's rule), E works a
    ship's button, or a Citadel's door (shut or moving: the door; open: its
    panels), only with the crosshair on it, and climbs off otherwise; the
    prompt says which (`AcShuttleAct::DriverAim`, set by the pawn each
    frame; `Autocraft.Pilot.ShuttleAct`).
  - **The windshield's rain** (`M_AcWindshield`, docs/weather.md "The
    Dropship's windshield"): the visor's water on the three panes in the
    glass's own frame, two blades in tandem, the side panes never wiped,
    driven by each ship's own `ac::Wipers`; the dashboard's WIPERS plate
    shows them aboard. **No rain in the bay**: the ship's bay is a turned
    shelter box (docs/weather.md "Shelter"): no streaks, no wet deck, the
    visor dry and the rain heard through the hull in the bay; the ramp
    is open sky.
  - **The view out** is the world itself: the ground and the giant, the
    sky and the weather, at the ship's height; the target region is made
    ahead from `WorldMessage::flyingTo`. The flight's effects ride on the
    ship: the lift jets under the nacelles, the lift-off's blast and
    scorch, dust under it near the ground, the touch down's dust, cloud
    streaks flung past in the dash, the pad's lamps alternating.
  - **Sounds** (`FAcAudioRules::Shuttles`; Tools/AudioGen/shuttle.manifest.json):
    each ship's engines a loop at the ship, in your ear and low-passed
    aboard (as the rain through the Citadel's walls); the ramp's hydraulics
    as it starts to move (`rampmove`), a clunk on each button (`buttonclunk`),
    the lift-off's roar, the approach's retro burst and the touch down's
    thud (F2's), its wipers heard aboard.
  - **The flow** `-AcFlowTest=mmoferry` (AcMmoFerryFlowTest.cpp): call,
    walk round the landed ship and up its ramp, "Close the ramp first" at
    the console, close at the post, take off from the console's flight
    menu, the lift, dash and approach seen through the windshield, the
    switch into Cold Fork, open the ramp, walk out. `-AcMmoShots=DIR
    -AcMmoShotsFerry -AcMmoWeather=rain` takes its pictures (`ferry-N-*`).
    `mmoferrymule`: the Mule staged behind the landed ship's ramp (the
    server's `ferry mule`), taken at its perch, driven straight up the ramp
    into the bay (it fits, its driver aboard), the ramp closed from the
    perch with the crosshair on the post, "E · CLIMB OFF" with it off.
    F2's cinematic flight (the camera chasing the ship, `mmofly`,
    `-AcMmoShotsF2`) is gone.
- The Mule model (H2) with the side load screen (room left, ore, MH), its
  hopper heap growing with the load, lamps, drive sounds.
- HUD: "In the Mule: N ore, M MH" while driving or near it.

### The Mule as built (c4-mule-look, 2026-10-19)

- **Model** `mule_blue` (`unreal/Tools/Editor/models/mule.py`, a Geometry
  Script model from concept H2, the coplanar check on): 3.00 long, 1.58
  wide over the hubs, the hopper's rim 1.30, the beacon 1.72, a full heap's
  crystals 1.78; the perch deck 0.35 up between the front wheel arches,
  the driver standing at (+0.74, 0). Six knobbly wheels (`wheels_0..5`,
  the front pair under `steers_0/1`), two levers with red knobs
  (`levers_0/1`), a railed perch, a console, the nose with its grille,
  four headlamps and the hazard bumper, the riveted deep-blue hopper (the
  team colour) with a flared lip, two chrome MH tanks at its front corners
  with a sight glass (`mhLevel_k`) and a dial (`needles_k`) each, the stack
  (`exhaust`) and the orange `beacon`. Icon `mule` (art/models/mule/icon.py).
- **The heap** (`heap`): Stardust Ore's dark rock and violet crystals,
  hidden while empty, grown up from the hopper's floor with the load's
  share of 1000 (a full load pokes over the rim). The sight glasses and
  dials show the MH's share of 1000.
- **The load screen**, one on each flank of the hopper: a fixed face
  (`screens`, `models/mule_screen.py`: ROOM LEFT, "/ 1000", ORE, MH) and
  live readouts on it: `roomDigits_k` (room left, four digits), `roomBar_k`
  (ten cells, one a hundred of room), `oreDigits_k`, `mhDigits_k`. A
  readout is drawn by the material `M_AcReadout`
  (`unreal/Tools/Editor/make_readout_material.py`) from its part's
  emission (custom data 1), so `PoseMule` shows a number by setting the
  part's emission to it: seven-segment digits or a bar, no texture per
  value, nothing but the instanced renderer.
- **The pose** (`AcPoseVehicles.cpp:PoseMule`): the wheels roll with the
  distance driven (held to 0.35 of a lug a frame, 18 lugs), the front pair
  steers with the turn, both levers go forward with the drive (back to
  reverse) and the left one further to turn right; the heap, gauges and
  screens follow the load. The building lamps' language: headlamps 1.2 and
  the screens lit with a driver aboard, dim parked; the orange beacon
  steady with a driver, turning and glowing on and off while it rolls.
- **The driver**: the rider (the pilot's Prospector, aboard) is drawn on
  the perch by the renderer (`AcPoseVehicles::MuleRiderPlacement`, the
  rider found through the Mule's `cargo`, which the snapshot carries), its
  gripper and drill on the levers' knobs; hidden with the Mule in first
  person.
- **Cameras**: first person from the driver's eyes on the perch
  (`cockpit_mule_blue`, the whole Mule copied by `models/cockpit_mule.py`
  and posed by `PoseMule`; the eye at (0.45, 1.55), pitched 0.45 down, the
  rail and the levers' knobs at the bottom of the view; it rocks as it
  rolls); third person on a boom of 0.7 × the default (the Mule drawn as big
  as a Firefly).
- **E** (C5: the crosshair on the perch, the hopper or the Foundry's call
  post, within reach, [acting.md](acting.md); before C5 `AcMuleAct` by
  distance, before the Citadel's acts): "E · CLIMB ON" at the
  perch, "E · CLIMB OFF" driving it (so in the hall E climbs off; open the
  door on foot), "E · FILL THE MULE" at the hopper while carrying (the
  hopper wins over the perch then), "E · BUY MULE · 600 ORE · 150 MH" at a
  finished Foundry with none of one's own. Board and leave are takes
  (`FAcMmoClient::Want`), fill and buy `FAcMmoClient::MuleFill` and
  `BuyMule`. Refusals are toasts (the prompt says them too): the client's
  ("Not your Mule", "You have a Mule already", "Your Mule is rolling out",
  the core's board and fill refusals) and the server's
  (`TakeMuleRefusals`). The MMO card of a finished Foundry has MULE (its
  price and roll-out) before DEMOLISH.
- **HUD**: "In the Mule: N ore, M MH" under the reticle, driving it or
  within 4 cells of one's own (`FAcPilotInfo::MuleLoad`); the dashboard's
  bay shows its load and the room left. A Prospector carrying near its own
  Mule is told "Tip it into your Mule's hopper, or walk into the Citadel".
  The booking's toast (`Mule::bookedToast`, the realm's cut under it) only
  for the pilot's own Mule.
- **Effects and sounds**: dust from behind the back wheels (`dust_0/1`) and
  a thin smoke from the stack while it rolls (`EAcKindBurst::MuleDust`);
  ElevenLabs sounds (`Tools/AudioGen/mule.manifest.json`, picks by
  `mule_pick.py`, takes in art/models/mule/sounds): `mulemove` as it pulls
  away, the drive loop `muleengine` while it rolls (a murmur parked with a
  driver), `mulegear` (a lever's clunk) as it starts or stops, `mulefill`
  as a load is tipped in, the booking rings like an unload; the driver's
  lines `vmule` and `muledeath` in the Prospector's voice. Its wreck
  shatters into 12 chunks, the heap and readouts left out.
- **Pictures**: [art/models/mule/renders](../art/models/mule/renders): the
  model row (`art/models/mule/shots.sh`: 3/4 in both team colours, the
  side at empty, half and full, the screens, front, top) and the game
  (`-AcFlowTest=mmomuleshots -AcMmoShots=DIR -AcMmoClockHour=11`,
  `AcMmoMuleShots.cpp`: beside a Prospector, the screen, the driver on the
  perch, third and first person, the dust, the Citadel's doorway, the
  booking's toast).
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

## The drive (c4-drive, 2026-10-19)

The user, driving the Mule: "The Mule is a car but it does not drive as a
car, it traverses in place like a tank. To be clear for now and the future:
wheeled vehicles should drive like wheeled vehicles and tracked vehicles
should drive like tanks." And: "going on an inclined terrain the vehicles
should match their normal to the terrain inclination; if the terrain is too
steep then the vehicles should not be able to climb."

**The rule, for every vehicle now and to come.** A kind says what it runs
on: `UnitStats::gear` (`RunningGear`: `wheels`, `tracks`, `legs`, `air`;
core `Rules.h`). Read it from the model: wheels under it drive as a car,
tracks as a tank; a new vehicle's row sets its gear, its `turnRadius`
(wheels) and its `wheelbase` (front contact to back), and the drive, the
AI, the climb limit, the tilt and the steering wheels follow from them.

| Gear | Kinds (by their models) | Turns | Climbs |
|---|---|---|---|
| wheels | Firefly (4 wheels), Hailstorm (a half-track: it steers by its front wheels), Mule (6 wheels) | only while rolling, on an arc no tighter than `turnRadius`; backing up the other way | `Rules::wheelsClimb` 25° |
| tracks | Longbow (four track pods) | on the spot, as before | `Rules::tracksClimb` 35° |
| legs | Prospector, Ranger, Comet, Juggernaut, Atlas (walker), Scorpion (crawler), the Brood | as before | what it can stand on (as before) |
| air | Dropship, Kestrel, Peregrine, Gloamwing | as before | – |

Turning radii (`Rules::muleTurnRadius` 4.5, `fireflyTurnRadius` 2.7,
`hailstormTurnRadius` 3.2: about 1.5 × the length) and the climb limits are
the user's to tune by feel.

- **The pilot's drive** (`Simulation::pilotFace`, `pilotWalk`; first and
  third person alike, the client's prediction through the same calls): W/S
  drive, A/D steer. On wheels the step goes along the arc the steer sets
  (curvature `steer / turnRadius`), the hull turning by the distance it
  rolled: nothing at a standstill, nothing while blocked, the other way
  backing up (a car's geometry). Tracks pivot on A/D at `hullTurnRate`.
- **The AI** (`Simulation::move` → `roll` for wheels): it steers onto the
  arc through its target (`ac::steerTo`, `Drive.h`); a target behind it it
  turns to on full lock; one inside its tightest circle it backs away from on
  the other lock (a three-point turn). It cuts its path's corners by
  0.4 × its radius (at most 1.2) rather than back and fill to touch each.
  `Simulation::turn` turns no wheels on the spot (a wheeled kind at ease
  keeps its heading).
- **Too steep** (`Simulation::climbRefused`): a step is refused when, at
  its end, the ground under the vehicle's front contact rises over its back
  one (`wheelbase` apart, along the step) steeper than its gear's limit.
  Down or across always goes. Refused, it stands (wheels and tracks do not
  crab up a hill; turned across the slope it goes on) and the HUD says "Too
  steep for its wheels" (or tracks) for 1.5 s (`Simulation::pilotSteep`,
  `Pilot::tooSteep`). The AI's paths leave such ground out: the nav grid
  marks each open cell steeper than a gear's limit (`NavGrid::steep`, the
  slope over 1.5 cells; bits `steepWheels`, `steepTracks` in `passable`),
  and a vehicle's search and straightening skip them (`NavGrid::steepFor`).
  The terrain alone counts: a step onto the Dropship's deck or ramp is
  never refused (its ramp rises 14.5°), nor one aboard.
- **Tilt** (the game): a vehicle is drawn pitched and rolled to the ground
  under its four contacts (`ac::groundTilt` over `ac::Contacts`, half its
  `wheelbase` ahead and behind, 0.8 of its radius to each side: a crest or
  a ditch reads as its wheels feel it), eased over a tenth of a second
  (`AcPose::VehicleTilt`, in `RestUnit`); a contact on a Dropship's deck
  or ramp reads the ship's floor, so the Mule pitches up the ramp. The
  Mule's rider stands on the perch as drawn, on the deck and in flight too
  (the renderer re-places riders after the poses). The first
  person view keeps a level horizon; its eye rides the tilted hull.
- **Steering wheels**: the Mule's and the Firefly's front pairs steer to the
  arc they drive, `atan(base × turn / distance)` (`SteerFromArc`; bases
  1.65 and 1.02, axle to axle), held while parked; the Mule's levers follow
  the steer as a share of full lock.
- **Regions** (`server/tests/DriveRegionTests.cpp`): in every region the
  Mule reaches each pad and opening a walker reaches; 0.8 to 4.1% of the
  open ground is too steep for wheels. Two sites are not on wheels: the
  Dust vault (dust-terraces, a cliff-hop vault) and the Lander wreck
  (start).
- **Pictures**: [art/models/mule/drive](../art/models/mule/drive):
  `before-after-top.png` (from above: W + D for 8 s and D alone standing,
  before and now, from the drive's numbers); `turn-top.gif` and
  `turn-top-sheet.jpg` (the game, from a camera over its turning circle, a
  frame every 2 s); `c4-drive-turn-tps`, `c4-drive-standstill`;
  `c4-drive-slope` and `c4-drive-slope-back` (pitched and rolled on a slope
  it climbs); `c4-drive-steep` and `c4-drive-steep-tps` (standing at a face
  too steep, the dashboard's refusal). The flow `-AcFlowTest=mmomuledrive
  -AcMmoShots=DIR -AcMmoClockHour=11` (`AcMmoMuleDriveShots.cpp`) takes them
  and checks no turn at a standstill, the circle's radius and the refusal;
  the server's dev lines `mule slope` and `mule steep` set the pilot's Mule
  short of a slope it climbs, or of a face too steep for it.
- **Tests**: core `DriveTests.cpp` (the gear of every kind; no turn at a
  standstill; the turning circle; reverse; tracks pivot; the AI reaching a
  point behind without pivoting, the Mule backing and filling; the tilt on
  a plane; too steep refused, allowed below, down and across; the AI's
  paths off too steep ground); `PilotTests_fireflySteersOnlyWhileRolling`.
  The simulation goldens `skirmish` and `playground` were re-recorded: their
  Fireflies and Hailstorm drive on wheels now (`duel` and `mining` are as
  they were).

## Solid bodies (c5-solid, 2026-10-20)

The user, playing: "my prospector can pass right through the mule, this
should not be possible", and the rule for the project: anything that does
not make sense to pass through is not passed through.

- **Bodies.** Every unit on the ground is solid to every other one on the
  ground: walkers (Prospector, Ranger, Comet, Juggernaut), vehicles
  (Firefly, Hailstorm, Longbow, Atlas, Scorpion, Mule) and the Brood
  (Shardling, Lodeback, the Matriarch). The Mule's body is its box,
  `Mule::length` × `Mule::width` (3.0 × 1.6) turned with its heading;
  everyone else's is the circle the walk already gives it,
  `Rules::radius` × 0.8. No bodies: the dead, flyers (the Dropship, the
  Kestrel, the Peregrine, a Gloamwing), anyone aboard (a Mule's rider on its
  perch, a Dropship's carried), in a Derrick or a Bastion, a Comet mid-jump,
  a buried Scorpion (under the ground). `Simulation::bodyBlocks`.
- **The pilot's walk and drive** (`Pilot+Bodies.cpp`; `pilotCanStand`, so
  the server and the client's prediction agree). A step that takes the
  driven unit's body into another's is refused, and it slides along it as
  along a rock or a wall (`pilotContact` gives the body's normal): a walker
  square into the Mule stops at its surface, aslant it slides along it and
  round its end; a driven Mule stops at a Prospector, a Ranger, a bug or
  another Mule. On wheels the body is tested turned as the arc turns it.
- **The Mule's ends** (the manager's Q3). Against buildings, rocks and ore
  its nose and tail count as its middle does (three circles along it, as the
  Dropship's bay already tested): its nose stops at a wall, not its middle
  1.5 cells back. The ground under its ends is its wheels' to judge (the
  climb limit, "The drive"): a cliff cell at a slope's foot under its nose
  held it from a slope it climbs (`mmomuledrive`). An end already in something where
  it stands (set down against a wall) holds it no longer, so it drives out.
- **Never stuck.** A body it already overlaps (spawned on it, a Mule rolled
  out onto it) holds only a step that goes deeper. A driven unit inside
  another's body is pushed out of the deepest one at 3 cells a second
  (`bodyPushPace`, `bodyPush`, at the start of `pilotWalk`, standing still
  too), straight out or turned up to 90° onto ground it can stand on.
- **The AI's units.** `Simulation::separate` (the AI's push apart after
  each step) now takes the Brood too (apart from the others; a swarm still
  crawls over itself: held apart, five Shardlings jammed in a gap of Cold
  Fork's prize mound ring, `Zone_aGroupWalksItsPathToThePrize`), in a world (`inWorld`) the AI's
  Prospectors (the frontier pilots') too, and no longer the driven unit: its
  own walk keeps it out of the others, and the others are held off it.
  After it, `holdOff`: the bodies the AI never moves (a driven unit, a Mule
  parked or driven) put any other ground unit inside them out at their
  surface, onto walkable ground, every step: an AI Ranger sent through the
  pilot goes round it, a swarm stands round it, nobody jams. A landed
  Dropship is solid on the AI's grid already (C4).
- **Kept possible.** E at the perch from beside the Mule (2.0 reach; its
  flank is 0.8 off its middle), filling at the hopper from beside or
  behind, the rider on the perch (aboard, no body), climbing off beside the
  perch (1.33 off its axis, clear of the box), the Mule rolling out of the
  Foundry's door (0.2 clear of the wall; anyone there is pushed out), the
  Mule up the ramp into the bay.
- **Left passable.** In the strategy game only, the AI's Prospectors pass
  each other and its soldiers as before (the classic RTS worker crowd at
  the ore; its economy and goldens hang on it); flyers over everything; a buried
  Scorpion; the Brood's nest mound and tunnel mouths and the build spots'
  markers (walked over by design). The Dropship's bay is 2.0 wide: a walker
  cannot pass a Mule parked in it (c5-ship widens it).
- **Tests**: core `SolidBodyTests.cpp` (square into the Mule from every
  side; aslant along it; climb on and fill from beside, off and away; a
  driven Mule stopped at a Prospector and at another Mule; a Firefly at a
  Prospector, a Prospector at a Ranger and a Shardling; flyers, a buried
  Scorpion and a rider no bodies; pushed out of a Mule, a Mule rolled out
  onto the pilot, a Mule driven out of another; the Mule's nose at a wall;
  an AI Ranger held off the pilot, a bug apart from a Ranger, two bugs over
  one another; the Dropship's hull from
  outside; the AI's Prospectors apart in a world, not in the strategy game); `MuleTests_mule_boardRideAndLeave` now puts its second
  Prospector beside the perch, not inside the Mule.

## 5. Workers

| Worker | Scope |
|---|---|
| c4-gate | The Citadel gate closes from both sides (the bug); the held load 4× bigger |
| c4-mule-sim | Section 2: core, net, server, save, tests |
| c4-mule-look | Section 4's Mule: the model, the screen, the heap, the HUD, the game's drive and E acts on the sim's requests |
| c4-ship-sim | Section 3: core, net, server, the frontier pilots, tests |
| c4-ship-look | Section 4's Dropship: the model, the bay, the buttons, the windshield and wipers, the camera aboard, the view out in flight |
| c4-drive | "The drive": wheels drive as cars, tracks as tanks; the climb limit; the tilt on the ground |
| c5-solid | "Solid bodies": nothing on the ground walks or drives through another; the Mule's ends against walls |
