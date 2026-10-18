// -AcMmoShots=DIR: the MMORPG test mode's moments in pictures
// (docs/mmorpg-m4.md Q1, the review in docs/mmorpg-shots.md). A flow step of
// its own (`mmoshots`, AcGameFlow.h): it starts a new world on the local
// server (build it first: make -C unreal/server), plays it through the real
// pilot pawn, build menu, level cards and world map as AcMmoFlowTest.cpp
// does, and saves a PNG with the HUD at each moment, DIR/NN-moment.png, a
// log line each (`mmoshots: saved ...`):
//
//   01 spawn on the pad, third person   02 the same, first person
//   03 top-down over the start region   04 the world map
//   05 the build menu                   06 the Citadel's ghost near the ore
//   07 the level-up cards (none in the mode since P5: skipped; the run
//      gives the new pilot 1100 XP, -AcMmoDevXp, level 3 for the Derrick)
//   08 a Citadel going up               09 the Citadel finished
//   10 drilling ore                     11 unloading: before, 12 after
//   13 the world map, Cold Fork picked  14 the flight, mid-flight
//   15 landed at Cold Fork's pad        16 the edge crossing into the start
//                                          region, just after arrival
//   17 after leaving the mode and resuming
//   18 after a level-up pick (Z); before 01 and here the HUD's plates must
//      draw with their frames (FAcHudStyle::PlateProblem)
//   the MH loop, between 12 and 13 (docs/mmorpg-play.md "The loop"):
//   19 the Derrick's ghost on the well          20 the Derrick built
//   21 pumping MH inside it                     22 the MH unloaded at the
//                                                  Citadel, the wallet's MH up
//
// -AcMmoShotsP5 (with -AcMmoDevXp=7100, level 12): only the progression
// (docs/prospector-progression.md "On screen"), DIR/p5-NN-moment.png:
//   01 the HUD: the points badge, the slot row   02 the skill screen (K), empty
//   03 points spent, Overcharge in Q              04 the BLUEPRINTS tab
//   05 Q used: the slot's cooldown ring           06 the level-up banner (its
//      words from AcProgressHud for 9 to 10, shown directly) and a find toast
//   07 the build menu from the blueprints
//
// -AcMmoShotsG2 (it adds -AcMmoDevRig: a finished Citadel, level 25, a
// full wallet): contracts, respawn, depot and own pads (docs/contracts.md,
// docs/prospector-progression.md "On the server (P6)"), DIR/g2-NN-moment.png:
//   01 the KSTR contract panel      02 a contract done: its toast (the news
//      shown directly, its words from AcProgressHud)
//   03 X: an abandon armed          04 slot 3 abandoned: back at dawn
//   05 the respawn screen (shown directly for a Citadel, 40 ore kept)
//   06 the world map at a Landing Pad (F2; Homing beacon before it was
//      retired), the contracts' marks           07 the opening ring at the Lander wreck
//   08 the wreck opened: its news toast   09 the world map, the site opened
//   10 a Hab Dome built, a load drilled: the depot prompt
//   11 the load unloaded at the Hab Dome
//
// -AcMmoShotsW2 (it adds -AcMmoDevRig as G2 does): the blueprint sites on
// the ground and the compact contract panel (docs/mmorpg-m7.md W2),
// DIR/w2-NN-moment.png. 03-06 and 10-12 stand staged sites beside the
// region's own (AAcSites::Stage): the start region has a crate and a wreck
// only. 07-09 are the start region's own Lander wreck.
//   01 the contract panel, compact     02 T: the whole panel
//   03-06 crate, wreck, cache, vault near: shut (left), opened (right)
//   07 the Lander wreck, shut, known: its lamp and column
//   08 the opening ring on the wreck   09 the wreck opened: its lights out
//   10 four shut sites known, 18-44 cells off, by day: their columns
//   11 the same at dusk                12 the four shut, near, at dusk
//
// -AcMmoShotsO1 (a new pilot, no dev XP: the real levels): the first ten
// minutes (docs/contracts.md "First ten minutes", docs/mmorpg-m7.md O1)
// played through, a picture as each step opens (its "KSTR:" toast, the
// gold step on the panel, the tip line), DIR/o1-NN-moment.png:
//   01 Touchdown: the panel opened (T), the Landing crate's column
//   02 A Roof over Your Head: the crate opened, the Citadel's blueprint
//   03 First Haul: the Citadel finished   04 The Wreck on the Rise: a load
//      home, the Lander wreck's column ahead
//   05 Liquid Gold: the wreck opened, the Derrick's blueprint, level 3
//   06 Make It Yours: the Derrick on the north well, pumped once
//   07 a point spent on the skill screen  08 the daily three take over
//
// -AcMmoShotsL1 (it adds -AcMmoFirstStep=6 and level 4's XP): KSTR's next
// goal after the first ten minutes (docs/contracts.md "What next",
// docs/mmorpg-m8.md L1), DIR/l1-NN-moment.png:
//   01 the chain's last point spent: the first goal's toast, the NEXT line
//   02 the Journal (L): its NEXT section on top   03 the Derrick's goal
//      passed (dev `goal pass derrick`): spend your points, their count
//   04 the points spent: GOAL DONE and its pay, the next goal (a mission)
//   05 T: the whole panel, the goal with the radio's line
//   06 dev `goal level 10`: a level-10 pilot's goal (an active on Q)
//   07 the Journal's NEXT section for it
//
// -AcMmoShotsU1 (the dev rig: a Citadel, level 25): the HUD decluttered
// (docs/mmorpg-play.md "The HUD", docs/mmorpg-m8.md U1), DIR/u1-NN-moment.png:
//   01 dev `goal level 35`: the level-up banner and the NEXT toast at once
//   02 the contract panel compact   03 T: the panel whole
//   04 a busy moment: three unloads, a contract, a mission and a goal done
//   05-09 K, J, H, L, M opened over a banner and toasts
//   10 a raid on the Citadel with toasts   11 U: a gatling gun's card
//   12 the Matriarch's roar: the boss bar under the compass
//   13 the Matriarch down: her banner, the bar felled
//
// -AcMmoShotsRingshadow (with -AcMmoRingshadow=70: a sweep 70 s after the
// server starts; docs/weather.md "Ringshadow sweep"): only the sweep, third
// person on the start pad toward the giant, DIR/rs-NN-moment.png:
//   01 before: the radio's toast, the land in the sun
//   02 toward the giant: the band's edge 20-30 cells ahead over the berms
//   02b the same from a boom 4 times as long (ac.Pilot.ShotBoom), 6-19 off
//   03 its edge at the pilot, looking up: the ring's edge on the sun, god rays
//   04 under the band    05 under it, looking up: the sun through the ring
//   06 under it, first person at the nearest ore (the veins flaring)
//   07 after: the band gone east   08 looking east at it
// The giant's comet impacts are off meanwhile (ac.GasGiant.Events 0). The clock
// jumps to just before each moment (S1), about 40 s in all.
//
// -AcMmoShotsStarfall (it adds -AcMmoStarfall=10 -AcMmoClockHour=17 unless
// the run gives them: a starfall near the pilot 10 s after it comes, late in
// the afternoon; docs/weather.md "Starfall"): third person on the start pad
// toward the crater's spot, then at the crater; for the night the clock jumps
// on to 19:00 (FAcMmoClient::DevClock), about 40 s in all, DIR/sf-NN-moment.png:
//   01 told: the radio's toast         02 the streak high in the sky
//   03 the streak low, over the land   04 the impact's flash
//   05 the dust ring                   06 the crater and its hot deposit by day
//   07 the same, first person          08 at night, third person
//   09 at night, first person
//
// -AcMmoShotsE1 (it adds -AcMmoStarfall=30 -AcMmoClockHour=17, clear, the
// start been to, unless the run gives them): scarcity and the epic fall
// (docs/regions.md "Scarcity", docs/weather.md "Starfall"): the server puts
// the realm's buildings and Prospectors in the fall's way (`starfall victims`),
// third person on the start pad, DIR/e1-NN-moment.png:
//   01 the world map: Prospect Landing's two veins and its one well
//   02 what stands where it will fall, the radio's warning
//   03 the streak coming in      04 the impact: the flash
//   05 the shockwave racing out  06 the crater: what it destroyed, what it hurt
//
// -AcMmoShotsRig (with -AcMmoDevRig: the server gives the new pilot a
// finished Citadel, Foundry and Lab, the Prospector where it unloads, and a
// stocked rig with a job at each; docs/prospector-progression.md "Foundry
// and Lab"): the rig screen (J), DIR/rig-NN-moment.png:
//   01 home: unloading at the Citadel     02 the rig screen: fitting at home,
//      the Foundry making, the Lab researching
//   03 Drill head Mk III fitted from the stores, the Mk II back in them,
//      and the Foundry's refusal while busy   04 the skill screen's RIG tab
//   05 the Foundry's job done: the stores up, the Foundry idle
//   06 a Cargo pod Mk II asked for: the Foundry's new countdown
//
// -AcMmoShotsF2 (or -AcMmoShotsFlight; F2, AcPilotFlight.h): the Dropship
// from a Landing Pad by the start pad to one in Cold Fork (the `ferry` dev
// lines put them up), each moment on the flight's clock, DIR/f2-NN-moment.png
// (-AcMmoHour=12 for noon, 19 for dusk); the frame times aboard logged as
// `f2 perf:` (-AcMmoShotsOnly=none: the flight with no picture taken;
// -AcMmoShotsF2Twice: a second flight back; -AcMmoShotsF2Settled: the call
// only once every ground set round the region is built, Q2's measure;
// -AcMmoShotsF2CallAt=S: the call S s after seating, K3's measure, with the
// worst frame since the call logged as `f2 perf (flight 1): worst frame`;
// every run logs `k3: seated N s after the process started`):
//   01 the flight menu: only Landing Pads in regions been to
//   02 the Dropship coming down onto the pad   03 the Prospector boarding
//   04 the lift-off         05 a banking turn, the giant in view
//   06 the speed burst, clouds streaking       07 the approach, wide
//   08 stepping out at the target pad          09 down, the view back
//
// -AcMmoShotsB1 (it adds -AcMmoDevRig: a Citadel to unload at): the drill's
// sparks stopping and the AI's walk (docs/mmorpg-m8.md B1),
// DIR/b1-NN-moment.png. Each stop logs how long sparks lived on after it
// (`mmoshots: b1: ... sparks gone N s after`); the AI's walk logs the drawn
// unit's speed jitter (`mmoshots: b1: AI walk ...`):
//   01 drilling, third person    02 0.3 s after the key let go   03 1 s after
//   04 drilling, first person    05 0.3 s after the key let go   06 1 s after
//   07 0.3 s after walking off the vein, first person, the key still held
//   then handed back: 15 s of the AI mining, top-down, measured, no picture.
//
// -AcMmoShotsC1 (it adds -AcMmoDevRig: the realm's Citadel to unload at
// and a full wallet): the Citadel's tax and the realm wallet
// (docs/mmorpg-m8.md C1), DIR/c1-NN-moment.png:
//   01 a load unloaded at the Citadel: the toast with the realm's cut
//      ("+22 ORE", "3 TO THE REALM"), the realm wallet by the wallet
//   02 the Citadel screen (H): the realm wallet, the tax, what the pilot
//      gave, the donate buttons
//   03 after a donation of 1,000 ore: the realm wallet up, the foot's thanks
//
// -AcMmoShotsC2 (it adds -AcMmoDevRig: the realm's Citadel to stand at and a
// full wallet): the Citadel shop (docs/mmorpg-m8.md C2), DIR/c2-NN-moment.png:
//   01 the SHOP tab after buying a blueprint the pilot neither knows nor
//      holds (none when it knows them all) and a shield charge (and a
//      repair kit): KNOWN and LEARN tags, stacks, the foot's "Bought"
//   02 the build wheel (B): every blueprint, the bought one to LEARN
//   03 the shield charge used in the field (key 3): its stack lit and
//      counting down by the Q/C/G slots
//
// -AcMmoShotsK2 (it adds -AcMmoDevRig: the realm's Citadel to sign up at):
// the Citadel missions and the Journal (docs/mmorpg-m8.md K2,
// docs/missions.md), DIR/k2-NN-moment.png:
//   01 the MISSIONS tab: the board, two offers a tier
//   02 signed up for an easy survey (or hold) mission: the foot's "Signed
//      up", the sting and the narration logged (silent run)
//   03 the Journal (L): the mission's briefing, what to do, its place on the
//      world's grid, PLAY THE BRIEFING (pressed: the narration logged again)
//   04 in the field near its place: the marker on the compass and the minimap
//   05 the mission done in its ring (a hold: the clock moved under the
//      ringshadow band): the MISSION DONE toast
//
// -AcMmoShotsN2 (it adds -AcMmoDevRig: the realm's Citadel, a full wallet):
// the Brood in the world (docs/creatures.md; docs/mmorpg-m8.md N2), each
// scene staged by the server's dev control (`brood …` lines,
// WorldMap::devBrood), DIR/n2-NN-moment.png:
//   01 Shardlings lingering at the deposit nearest the pilot
//   02 Shardlings on the Prospector: its hp falling, the bugs outlined red
//   03 a Gloamwing in its dive, at torch height
//   04 a Lodeback with a full sac, leaving
//   05 a raid on a Hab Dome: the KSTR toast told, the dome's life bar falling
//
// -AcMmoShotsS2A, then -AcMmoShotsS2B (two runs on the same -AcMmoShots=DIR,
// each with -AcMmoServerDetached): the world runs on (docs/mmorpg-m8.md S2,
// docs/mmorpg.md "The world runs on"), DIR/s2-NN-moment.png:
//   01 (S2A) the first run in the world: its server detached (its run file),
//      the world's clock noted (DIR/s2-first.txt); the run ends, the server
//      runs on
//   02 (S2B) the second run reattached to that server (no new one): the
//      world's clock on by the time between the runs
//   03 home: "World server running · since …" with STOP
//   04 STOP picked: the server saved and stopped, the line gone
//
// -AcMmoShotsH2, then -AcMmoShotsH2Away (two runs on the same -AcMmoShots=DIR
// and -AcSaveDir, attached; both add -AcMmoDevRig, the second also
// -AcMmoDevDown=10800 unless told): mercenary prospectors (docs/mmorpg-m8.md
// H2), DIR/h2-NN-moment.png:
//   01 (H2) the Citadel's MERCS tab: no mercenary yet, the fee, the fields
//   02 (H2) HIRE: the mercenary listed, the foot's "Hired"
//   03 (H2) the mercenary drilling at its field: its orange, "MERC · NAME"
//      (what it delivered noted in DIR/h2-first.txt); the run ends, the
//      server saves and stops with it
//   04 (H2Away) the next start, three hours later by the save's clock: the
//      "WHILE YOU WERE AWAY" toast, the ore its mercenary delivered
//
// -AcMmoShotsD (it adds -AcMmoDevRig: the realm's Citadel, a full wallet):
// the defences (docs/mmorpg-m8.md D1, D2), each scene staged by the server's
// dev control (`defence …` lines, WorldMap::devDefence), DIR/d-NN-moment.png:
//   01 the Citadel's four corner turrets firing at a raid on it
//   02 the DEFENCE tab after a fire-rate mark bought from the realm wallet
//   03 an AA tower (the Sentinel) shooting down Gloamwings
//   04 a gatling gun firing amalgam rounds at Shardlings, its tracers
//   05 the build menu with the gatling gun and the AA tower
//
// -AcMmoShotsM2 (adds -AcMmoDevRig and -AcMmoDevSeen=start,cold-fork,
// rust-hollow,ashen-shelf,tern-ridge unless told): the satellite world map
// (docs/mmorpg-m8.md M2), DIR/m2-NN-moment.png:
//   01 the M screen at world zoom: the regions been to as their pictures,
//      the rest dark, uncharted
//   02 zoomed into Prospect Landing: the pilot, its mercenary, the Citadel,
//      the deposits, the pad, the mission taken
//   03 a deposit's tooltip: its ore left
//   04 the Citadel's tooltip: built by, hp
//   05 the mission's tooltip: its title, tier and kind (it is then the
//      compass's target)
//   06 the mercenary's tooltip: its name, field, what it delivered
//   07 the map closed: the mercenary drilling in the world, orange
//
// -AcMmoShotsD3 (with -AcMmoDevRig): turret upgrades (docs/mmorpg-m8.md D3),
// out past the Citadel's guns, a Gatling Gun placed ahead and Gloamwings
// hovering 5 to 5.8 cells from its edge (`defence air`), DIR/d3-NN-moment.png:
//   01 the bare gun: its chevrons dark, the Gloamwings out of its air reach
//   02 the turret card (U) after marks bought from the pilot's wallet
//   03 the gun with its marks: the chevrons lit on its base
//   04 the marked gun hitting a Gloamwing at its new air reach
//
// -AcMmoShotsN3 (with -AcMmoDevRig): the Matriarch (docs/creatures.md N3),
// the pilot taken to Slag Pockets by the server's dev line (`matriarch
// here`), her horde held still for the pictures, DIR/n3-NN-moment.png:
//   01 the nest from afar (55 cells): her crown over its rim
//   02 her roar on first sight, the horde round her, the boss bar
//   03 a slam: her forelimbs crashing down in front of the pilot
//   04 the boss bar with her wounded
//   05 her down: her body sinking, "THE MATRIARCH IS DOWN"
//
// -AcMmoShotsW4 (at 11:00, clear weather, every region been to unless told):
// seamless regions (docs/mmorpg-m8.md W4), DIR/w4-NN-moment.png:
//   p-<id> each region's palette from above, a free camera over its middle
//     (the nine: `w4/grid.py` puts them in a grid)
//   01 a border seen from inside: Prospect Landing's north wall, its cliff
//   02 the same wall from higher: over it, the neighbour's ground beyond
//   03 the east passage from Prospect Landing: the gap, Tern Ridge beyond
//   04 the same passage from Tern Ridge, looking back
//   15 into a bay of Prospect Landing: the land reaching in among the walls
//   16 along a wall of Prospect Landing: it meanders and steps (W4b)
//   (a measured walk east through it, nothing shot: the crossing's numbers,
//   frame times and the unit's speed, logged as `w4: crossing:`)
//   05-NN walking back west through it, a frame every 0.4 s, chase camera
//   then the world map: 20 the world, 21 Prospect Landing filling it
//
// -AcMmoShotsS3 (with -AcMmoDevRig): daylight sight (docs/creatures.md S3),
// by day the pilot's whole region seen and told, DIR/s3-NN-moment.png, and
// the frame and GPU times over -AcMmoShotsS3Perf=S s (8) each, logged as
// `s3 perf:`:
//   01 Slag Pockets at noon, 55 cells from the nest: her horde told
//   02 the same with `daylight off`: the sight before S3
//   03 the same at midnight: half sight and the headlamp, as before
//   04 Cold Fork at noon, wave 10 out of the canyon's mouths: all of it told
//   05 the same with `daylight off`
//
// -AcMmoShotsBench (with -AcMmoDevRig): the MMO bench (docs/perf.md), the
// client's standard scenes, each settled, then timed -AcMmoBenchSeconds=S
// (10) with FAcBenchMeter (AcBenchMeter.h) and logged as one `bench-result`
// line (bench.py --mmo reads them; -AcBenchGpu adds `stat gpu`'s passes),
// then its picture, DIR/NAME.png, to compare A with B:
//   mmo-home-noon    Prospect Landing at noon, facing the pilot's Citadel
//   mmo-home-night   the same at midnight: the lamps and the headlamp
//   mmo-nest-noon    Slag Pockets at noon, 55 cells from the nest, her horde told
//   mmo-fork-wave    Cold Fork at noon, wave 10 down The Fork, facing the nearest bug
// and the two arrivals, the frames from the server's teleport until the
// next scene's timing begins (the ground built, the models loaded):
//   mmo-arrive-nest  into Slag Pockets    mmo-arrive-fork  into Cold Fork
// The weather is the world's (-AcMmoWeather=clear for the standard runs).
// -AcBenchMemReport: `memreport -full` after each scene's timed frames.
// -AcMmoBenchHour=H puts the first scene at hour H (a dawn clip with
// -AcMmoShotsClip -AcMmoShotsOnly=mmo-home-noon -AcMmoShotsClipDwell=S).
//
// -AcMmoShotsR2 (with -AcMmoDevRig): realm projects (docs/mmorpg-play.md
// "Realm projects"), DIR/r2-NN-moment.png:
//   01 the realm-wide toast and the gold bar under the HUD's realm wallet
//   02 the Citadel's REALM tab: the Beacon started and running (who, time
//      left), the other projects' prices
//
// -AcMmoShotsR3 (with -AcMmoDevRig, as Z1's light): a Citadel in every
// region (docs/mmorpg-play.md "The Citadels"): a second Citadel's blueprint
// bought at home, the pilot taken to Cold Fork, DIR/r3-NN-moment.png:
//   01 the Citadel's ghost placed in Cold Fork (B, the build menu)
//   02 the second Citadel rising in Cold Fork, welded
//   03 finished, its four corner turrets on, from a free camera
//   04 the Citadel screen (H) at the Cold Fork Citadel: the realm's one
//      wallet, its tabs
//
// -AcMmoShotsX2 (with -AcMmoDevRig and -AcMmoServerDetached): coming back
// (docs/mmorpg-play.md "Coming back"): a mercenary hired at the dev rig's
// Citadel, QUIT TO HOME (the world server runs on), the world caught up on
// three hours as a restart after that long would (`catchup 10800`), then
// in again; the detached server stopped at the end, DIR/x2-NN-moment.png:
//   01 home: the two away lines under ENTER THE REACH, from the summary
//      file the server wrote beside the save
//   02 in again: the WHILE YOU WERE AWAY toast (KSTR's welcome read)
//   03 the Journal (L) opened on WHILE YOU WERE AWAY: every row, with when
//
// -AcMmoShotsS4 (with -AcMmoDevRig, the chain done, as P1's light): realm
// standings (docs/mmorpg-play.md "Standings"): three frontier pilots staged
// beside the pilot (`frontier here …`), a mercenary hired, the world idled
// two hours as with nobody on line (`idle 7200`: their work booked by the
// rules), DIR/s4-NN-moment.png:
//   01 the Citadel's REALM tab: the week's standings, the pilot's own row
//   02 the world map: a frontier pilot's tooltip with its rank this week
//   03 the week ended (`week end`): THE WEEK IS COUNTED, its top hauler
//
// -AcMmoShotsN4 (with -AcMmoDevRig, the chain done, as S4's), and the flow
// test mmohunt (`-AcFlowTest=mmohunt -AcMmoShots=DIR`, the same steps with
// their checks): the hunt (docs/mmorpg-play.md "The hunt"): a frontier
// defender beside the pilot, two guards hired at its Citadel, THE HUNT called
// on the REALM tab; the party walks to the ring round her nest by the rules;
// the pilot taken there (`matriarch here`), the gathering's two minutes
// jumped (`hunt gathered`), the pilot steps inside the ring, they go in; her
// fall cut short (`matriarch hurt`, `matriarch down`), DIR/n4-NN-moment.png:
//   01 the REALM tab: THE HUNT called, gathering, its party
//   02 the world map: the ring round her nest, the party on the way and there
//   03 the party gathered at the ring, from the pilot's place by it
//   04 the charge: THE HUNT GOES IN
//   05 her fall: THE HUNT BROUGHT HER DOWN, the share
//
// -AcMmoShotsZ4 (as Z1's): free placement in canyons (docs/canyon.md
// "Building"), DIR/z4-NN-moment.png:
//   01 a tightly packed defence on The Fork's floor by the prize, its guns'
//      footprints touching, wave 3 coming at it
//
// -AcMmoShotsZ1 (at 11:00, clear weather, every region been to): the first
// canyon zone, The Fork in Cold Fork (docs/regions.md "zones"), the pilot
// taken into its gorge by the server's dev line (`brood zone here`), a
// free camera, DIR/z1-NN-moment.png:
//   01 the canyon from a rim: the west rim over the north arm, to the junction
//   02 from the floor at the north arm's end, looking up it past its bend
//   03 a Brood tunnel's mouth at the end of the east arm
//   04 the prize field at the junction: the rich ore round the MH well, a
//      group sent down each path (`brood zone PATH`) on it
//   05 the zone from high up: both arms, the junction
//   06 the west rim over the north arm, close (Z4: no build spots; pilots
//      build anywhere)
//   07 the world map on Cold Fork: the canyon on the satellite picture
// (The preview with the paths is the region tool's: `autocraft-region
// preview regions/cold-fork.json`.)
//
// -AcMmoShotsZ2 (at 11:00, clear weather, every region been to): the canyon
// game in The Fork (docs/canyon.md), the pilot in its gorge, the waves and
// the turrets set by the server's dev lines (`canyon …`), a free camera
// (the HUD's own pictures with it), DIR/z2-NN-moment.png:
//   01 wave 1 coming down the north arm (an AA tower on the rim sees it)
//   02 the wave panel: the opening wave, its bugs on the prize, the ore taken
//   03 wave 1 again, the rim's Gatling Guns firing down on it
//   04 wave 1 beaten: the toast, the share, the panel's countdown to wave 2
//   05 wave 4's Gloamwings over the east arm meeting the AA towers' fire
//   06 the wave broken through: THE FORK HAS FALLEN
//   07 the Matriarch out of the east tunnel
//   08 her felling the zone's turrets
//   09 the reset: the turrets gone, the opening wave waiting again
//   10 THE FORK HELD (the last wave beaten by the dev line)
//   11 the world map on Cold Fork: the zone's ring and its state
//   12 the Journal: the zone's state in its header
//
// -AcMmoShotsMapPick (the dev rig, the chain done): the world map's pad
// pick and its refusals (the user, 2026-10-15: "I can't pick a landing on the
// map"): a Landing Pad only here, a click on Rust Hollow's own pad (DIR/
// mappick-01-only-here.png: "Only one Landing Pad in the realm, here…"), then
// a Landing Pad in Rust Hollow (been to): its own pad refused (02, "That is
// Rust Hollow's own pad…"), its Landing Pad picked (03).
//
// -AcMmoShotsA2 (-nosound; the dev rig, the chain done): the siege heard
// (docs/music.md "Siege layers"), checked from its cue timeline, never
// played aloud: a wave at The Fork (the wave layer up, the radio ducked,
// KSTR's "wave incoming"), the wave beaten (its line), a wave broken
// through (the Matriarch's layer, "has fallen"), the canyon held (both
// layers out, the radio back up over 2 s, "held"), a goal reached (KSTR's
// next-goal line), then a Dropship flight (lift, cruise, retro, touch).
// One picture at the end (the HUD after the landing); the timeline is
// logged as `mmoshots: a2: cue ...` lines.
//
// -AcMmoShotsZ3 (at 11:00, clear weather, every region been to): the canyons
// after The Fork (docs/canyon.md), staged by the server's dev lines
// (`brood zone here ZONE`, `canyon …`), DIR/z3-NN-moment.png:
//   01 The Drowned Cut (Brine Pans) from above: the two arms, the brine
//   02 a wave's Shardlings wading through the brine, slowed (no HUD)
//   03 the same with the HUD: the wave panel
//   04 The Roost (Dust Terraces) from above: the tunnels, the roost's spire
//   05 Gloamwings coming off the roost's ledge (no HUD)
//   06 the same with the HUD: the wave panel
//   07 the world map on Brine Pans: The Drowned Cut's ring
//   08 the world map on Dust Terraces: The Roost's ring
//
// -AcMmoShotsP1 (at 11:00, clear weather, every region been to, the opening
// chain done, the dev rig's Citadel): frontier pilots (docs/frontier.md),
// staged by the server's dev lines (`frontier …`), DIR/p1-NN-moment.png:
//   01 a frontier miner drilling near the pilot, its tag ("VESPER · LV 14")
//   02 a builder's Hab Dome finished: the deed's toast ("… built a Hab Dome in …")
//   03 a KSTR radio line from a frontier pilot
//   04 the world map: the frontier pilots' dots in their paints
//   05 a frontier pilot's tooltip: callsign, level, habit, what it does
//   06 at The Fork: a defender fighting the wave beside the pilot (no HUD)
//   07 the same with the HUD (its tag, the wave panel)
//
// -AcMmoShotsOnly=sf-02,sf-08 (names, numbers or a prefix and a number,
// comma-separated): only those moments are shot; the others are passed over
// (logged, counted as skipped), and in the Starfall set their waits too.
// A run that waits on the world's clock moves it at once instead
// (FAcMmoClient::DevClock: the server's --dev-control file, shot runs only).
//
// -AcMmoShotsClip (with -AcShot=PATH -AcShotRecord=S -AcShotRaw=FILE
// -AcShotUI, AcShot.h; the ringshadow-video skill): motion instead of
// pictures. The recorder is held until the first moment -AcMmoShotsOnly
// names; from there it records S seconds of the run as it goes on, every
// later moment played (and its waits), none of them shot, and the recorder
// ends the run. Each moment logs its world time
// (`mmoshots: clip: NAME at world T s`) against the recorder's
// `shot: recording starts at world T s`, to find it in the clip. Leave out
// -UseFixedTimeStep: the server runs on the wall clock, so the client must
// too; the clip's rate is the frames over the recording's wall seconds.
// -AcMmoShotsClipDwell=S holds each moment S s on screen before the set goes
// on (the world runs on meanwhile): a screen that closes at once (the
// Journal, a tooltip) stays long enough to read.
// The HUD's moments keep the HUD in the recording (with -AcShotUI), the
// clean ones (the free camera's scenery) drop it; the M2 set's zoom into
// Prospect Landing glides there over 4.5 s (SAcWorldMap::GlideToRegion).
//
// Its own folders (never the player's): the pilot key and the world's save
// in DIR/mmo, the window game's sessions in DIR/save (AcMmo.cpp, AcSaves.cpp).
//
// Run hidden (ringshadow-testing, "Hidden runs"):
//   "$BIN" "$PWD/unreal/Autocraft.uproject" /Game/Maps/Battlefield -game
//     -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=1000 -unattended
//     -nosplash -nosound "-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False"
//     -abslog=DIR/run.log -AcMap=highlands-small -AcMmoShots=DIR
// Done when the log's last line reads `flowtest: done, 0 failed`.
#include "AcGameFlow.h"
#include "AcDaylight.h"
#include "AcEffects.h"
#include "AcMusicPlayer.h"
#include "AcEffectsBursts.h"
#include "AcParticles.h"

#include "AcGasGiant.h"
#include "AcHudStyle.h"
#include "AcLeveling.h"
#include "AcLog.h"
#include "AcMissions.h"
#include "AcMmo.h"
#include "Flight.h"
#include "AcPilotBuild.h"
#include "AcPilotPawn.h"
#include "AcPilotText.h"
#include "AcProgressHud.h"
#include "AcBenchMeter.h"
#include "Dom/JsonObject.h"
#include "AcBrine.h"
#include "AcCanyon.h"
#include "AcShot.h"
#include "AcSimSubsystem.h"
#include "AcSites.h"
#include "SAcLevelUp.h"

#include "Brood.h"
#include "Defence.h"
#include "Pilot.h"
#include "Ringshadow.h"
#include "Rules.h"
#include "Starfall.h"
#include "Simulation.h"

#include "AcTerrain.h"
#include "AcSpace.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "RHI.h"
#include "RegionFile.h"
#include "WorldAtlas.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "RHI.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include <cmath>

namespace
{
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/// Seconds a phase may take before the run gives up on it.
	constexpr double PhaseLimit = 60;
	/// The Citadel's weld (71 s at twice the pace) and some.
	constexpr double WeldLimit = 90;

	const ac::Structure* CitadelOf(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::citadel && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// -AcMmoShotsP5: the points it spends, in order (9 of level 12's 11),
	/// how many are in, and when it last asked.
	constexpr ac::Skill P5Spends[] = {ac::Skill::quickDrill, ac::Skill::quickDrill, ac::Skill::quickDrill, ac::Skill::quickDrill,
		ac::Skill::quickDrill, ac::Skill::overcharge, ac::Skill::lightFrame, ac::Skill::lightFrame, ac::Skill::plating};
	int32 P5Spent = 0;
	double P5AskedAt = 0;

	/// -AcMmoShotsG2: the Hab Dome it builds.
	int64 G2Dome = -1;
	/// -AcMmoShotsW2: where the staged sites are stood from (ground, the
	/// heading), and the kind shown near.
	ac::Vec2 W2From;
	double W2Yaw = 0;
	int32 W2Kind = 0;
	/// Ground `Ahead` cells along `Yaw` from `From` and `Right` to its right.
	ac::Vec2 W2At(const ac::Vec2 From, const double Yaw, const double Ahead, const double Right)
	{
		return ac::Vec2(From.x + Ahead * std::cos(Yaw) - Right * std::sin(Yaw), From.y + Ahead * std::sin(Yaw) + Right * std::cos(Yaw));
	}

	const ac::Structure* HabDomeOf(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::habDome && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// -AcMmoShotsB1: when the drill was last stopped, when sparks were
	/// last alive after it, and the drawn unit's walk under its AI (wall
	/// seconds; the drawn position and the newest snapshot's).
	double B1StoppedAt = -1;
	double B1SparksAt = -1;
	/// Into first person before drilling again (after the third person's).
	bool B1ToFirst = false;
	/// -AcMmoShotsC1: the realm wallet's ore before the unload, and the
	/// pilot's donations before the gift.
	int64 C1RealmBefore = 0;
	int64 C1DonatedBefore = 0;
	/// The load it brought home.
	int64 C1Load = 0;
	/// -AcMmoShotsC2: the MH before buying, the Hab Dome copy's price and
	/// the copies before, the items used before.
	int64 C2Hydrogen = 0;
	ac::ShopPrice C2Price;
	/// -AcMmoShotsC2: the blueprint bought (one the pilot neither knows nor
	/// holds; unset: it knows them all, nothing to buy).
	TOptional<ac::StructureKind> C2Kind;
	int32 C2Used = 0;
	/// -AcMmoShotsK2: the offer taken (its index on the board, its number and
	/// template), and the Citadel's kept asks before it.
	int32 K2Index = -1;
	uint32 K2Id = 0;
	FString K2Key;
	int32 K2Kept = 0;
	/// Its place on the ground.
	ac::Vec2 K2Target{0, 0};
	/// When it was first seen done (the toast fades in).
	double K2DoneAt = -1;
	/// -AcMmoShotsK2: the drive's way out of a corner (Q2): backing up until,
	/// and how many times so far.
	double K2BackUntil = -1;
	int32 K2Unsticks = 0;
	/// -AcMmoShotsH2: the Citadel's kept asks before the hire, the
	/// mercenary's unit.
	int32 H2Kept = 0;
	int64 H2Unit = 0;
	/// X2: the home screen's away lines as shot.
	FString X2Ran;
	FString X2What;
	int32 X2Cues = 0;
	/// -AcMmoShotsM2: the mercenary's unit, the mission taken.
	int64 M2Unit = 0;
	uint32 M2Mission = 0;
	/// Zoomed into Prospect Landing; the phase whose item is hovered.
	bool M2Zoomed = false;
	int32 M2Hovered = -1;
	/// -AcMmoShotsN2: the lingering group staged, the pilot's hp when a
	/// scene was staged, the raided dome.
	bool N2Staged = false;
	double N2Hp = 0;
	int64 N2Dome = -1;
	/// -AcMmoShotsD: the Citadel's kept asks and the realm wallet's ore before
	/// the upgrade; the walk out staged.
	int32 DKept = 0;
	int64 DRealmOre = 0;
	bool DStaged = false;
	/// -AcMmoShotsW4: the free camera, the regions in grid order, the shot
	/// counter, and the crossing's samples (each frame: real seconds, its
	/// length, the frame number, the unit and the eye on the world's grid).
	TWeakObjectPtr<ACameraActor> W4Cam;
	TArray<int64> W4Order;
	int32 W4Next = 0;
	bool W4Set = false;
	struct FW4Sample
	{
		double At = 0;
		double Dt = 0;
		uint64 Frame = 0;
		ac::Vec2 Unit;
		ac::Vec2 Eye;
		/// The engine's last game thread, render thread and GPU frame, ms.
		double Game = 0, Render = 0, Gpu = 0;
	};
	TArray<FW4Sample> W4Walk;
	double W4LastAt = -1;
	double W4ShotAt = -1;
	int32 W4Frames = 0;
	/// -AcMmoShotsZ1: Cold Fork's file (its zone).
	TOptional<ac::region::RegionFile> Z1File;
	/// -AcMmoShotsZ2: the zone's turrets standing when the wave broke through.
	int32 Z2Turrets = 0;
	/// -AcMmoShotsA2: the cue timeline's length when the step began.
	int32 A2From = 0;
	/// -AcMmoShotsP1: the frontier miner the dev line staged (its id).
	uint32 P1Miner = 0;
	/// -AcMmoShotsD3: the turret card's kept asks, the keys pressed so far.
	int32 D3Kept = 0;
	int32 D3Step = 0;
	int32 D3Logged = -1;
	/// -AcMmoShotsN3: her strikes seen, when her slam began, when her roar began.
	int64 N3Strikes = -1;
	double N3SlamAt = -1;
	double N3RoarAt = -1;
	/// -AcMmoShotsN3Perf=S (N1b): frame and GPU times over S s after the roar's picture, the nest full of bugs in view.
	TArray<double> N3Frames, N3Gpu;
	double N3FrameAt = -1;
	/// -AcMmoShotsS3: the frame and GPU times of the scene being measured.
	TArray<double> S3Frames, S3Gpu;
	double S3FrameAt = -1;
	bool S3Set = false;
	/// -AcMmoShotsBench: the scene's meter, its step (0 its lines, 1 settling,
	/// 2 timed, 3 the stats shown, 4 hidden again) and when the step began.
	FAcBenchMeter BenchMeter;
	/// -AcMmoShotsBench: an arrival's frames, from the server's teleport until
	/// the next scene's timing begins (its ground, its models, its first looks).
	FAcBenchMeter ArriveMeter;
	bool bArriving = false;
	int32 BenchStep = 0;
	double BenchAt = 0;
	// -AcMmoShotsL1: when the skill points' goal was seen done.
	double L1DoneAt = 0;
	// -AcMmoShotsU1: when her roar began; the staged gun's id.
	double U1RoarAt = -1;
	/// -AcMmoShotsF2: the frame and GPU times aboard, each frame's stage, the
	/// frame the region change came in.
	TArray<double> F2Frames;
	TArray<double> F2Gpu;
	TArray<uint8> F2Stages;
	double F2Last = -1;
	int32 F2Switch = -1;
	/// -AcMmoShotsF2Settled: since when every ground set round the region is built.
	double F2SettledAt = -1;
	/// -AcMmoShotsF2Twice: 1 on the second flight.
	int32 F2Round = 0;
	/// When the pilot was first seated (K3: the startup time, and
	/// -AcMmoShotsF2CallAt's clock).
	double SeatedAt = -1;
	/// K3: every frame from the call on, aboard or not yet: the worst one and
	/// which it was (the first call's stall).
	double F2Prev = -1;
	double F2CallWorst = 0;
	int32 F2CallFrame = 0;
	int32 F2CallWorstAt = -1;
	/// The phase whose scene was staged (its first frame done).
	int32 U1Staged = -1;
	struct FB1Sample
	{
		double At;
		ac::Vec2 Drawn;
		ac::Vec2 Told;
	};
	TArray<FB1Sample> B1Walk;

	/// The AI's walk as drawn: over the frames where the server moved it
	/// steadily (its newest snapshot 0.1 cell on in every 0.3 s, 0.3 s
	/// either side), the speed each frame against the mean of the 0.5 s
	/// around it. Returns the line to log; `Jitter` the rms, a fraction.
	FString B1WalkStats(const TArray<FB1Sample>& W, double& Jitter, int32& Frames, int32& Stalls)
	{
		Jitter = 0;
		Frames = 0;
		Stalls = 0;
		const int32 N = W.Num();
		TArray<bool> Moving;
		Moving.SetNum(N);
		for (int32 I = 0, J = 0; I < N; ++I)
		{
			while (J < I && W[J + 1].At <= W[I].At - 0.3) ++J;
			Moving[I] = W[I].At - W[J].At >= 0.25 && ac::distance(W[I].Told, W[J].Told) > 0.1;
		}
		TArray<double> Speed;
		Speed.SetNum(N);
		for (int32 I = 1; I < N; ++I)
		{
			const double Dt = W[I].At - W[I - 1].At;
			Speed[I] = Dt > 1e-4 ? ac::distance(W[I].Drawn, W[I - 1].Drawn) / Dt : 0.0;
		}
		double Sum = 0, Sq = 0, Worst = 0, Mean = 0;
		for (int32 I = 1; I < N; ++I)
		{
			bool bSteady = true;
			double WSum = 0, WTime = 0;
			for (int32 K = I; K >= 1 && W[K].At >= W[I].At - 0.3 && bSteady; --K) bSteady = Moving[K];
			for (int32 K = I; K < N && W[K].At <= W[I].At + 0.3 && bSteady; ++K) bSteady = Moving[K];
			if (!bSteady) continue;
			for (int32 K = 1; K < N; ++K)
			{
				if (FMath::Abs(W[K].At - W[I].At) > 0.25) continue;
				WSum += ac::distance(W[K].Drawn, W[K - 1].Drawn);
				WTime += W[K].At - W[K - 1].At;
			}
			if (WTime <= 0 || WSum <= 0) continue;
			const double Around = WSum / WTime;
			const double Off = (Speed[I] - Around) / Around;
			Frames += 1;
			Sum += Speed[I];
			Sq += Off * Off;
			Worst = FMath::Max(Worst, FMath::Abs(Off));
			if (Speed[I] < 0.2 * Around) Stalls += 1;
		}
		if (Frames > 0)
		{
			Jitter = FMath::Sqrt(Sq / Frames);
			Mean = Sum / Frames;
		}
		return FString::Printf(TEXT("%d steady frames of %d, %.2f cells/s, speed jitter %.1f%% rms (worst %.0f%%), %d stalled frames"), Frames, N,
			Mean, 100 * Jitter, 100 * Worst, Stalls);
	}

	/// -AcMmoShotsClip: whether the recorder is held for the first moment,
	/// and whether it records (every moment wanted from then on).
	bool ClipHeld = false;
	bool Clipping = false;
	/// The set ended while recording: the recorder ends the run.
	bool ClipEnded = false;
	/// -AcMmoShotsClipDwell=S: the set waits until then after a moment.
	double ClipDwellUntil = 0;

	/// -AcMmoShotsOnly=sf-02,sf-08 (names or numbers, comma-separated): the
	/// moments to shoot; the others are passed over (counted as skipped),
	/// their waits too where the set can. Unset: every moment.
	bool Wanted(const TCHAR* Name)
	{
		FString Only;
		if (Clipping) return true;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsOnly="), Only, false) || Only.IsEmpty()) return true;
		const FString N(Name);
		// The moment's number: the first piece that starts with a digit
		// ("02" in sf-02-streak-high, "02b" in rs-02b-edge-high).
		FString Number;
		TArray<FString> Pieces;
		N.ParseIntoArray(Pieces, TEXT("-"));
		for (const FString& P : Pieces)
			if (!P.IsEmpty() && FChar::IsDigit(P[0]))
			{
				Number = P;
				break;
			}
		TArray<FString> Asked;
		Only.ParseIntoArray(Asked, TEXT(","));
		for (FString A : Asked)
		{
			A.TrimStartAndEndInline();
			if (A.IsEmpty()) continue;
			if (N.Equals(A, ESearchCase::IgnoreCase) || N.StartsWith(A + TEXT("-"), ESearchCase::IgnoreCase)) return true;
			// A number alone ("8", "08", "02b"), or a set's prefix and number ("sf-8").
			FString Tail = A;
			if (int32 Dash; A.FindLastChar(TEXT('-'), Dash)) Tail = A.Mid(Dash + 1);
			if (Tail.IsEmpty() || Number.IsEmpty() || !FChar::IsDigit(Tail[0])) continue;
			if (A != Tail && !N.StartsWith(A.Left(A.Len() - Tail.Len()), ESearchCase::IgnoreCase)) continue;
			if (Tail.Equals(Number, ESearchCase::IgnoreCase) || (Tail.IsNumeric() && Number.IsNumeric() && FCString::Atoi(*Tail) == FCString::Atoi(*Number)))
				return true;
		}
		return false;
	}

	/// The gatling gun nearest `At` in the mirror (null: none).
	const ac::Structure* NearestGatling(const ac::GameState& S, const ac::Vec2 At)
	{
		const ac::Structure* Out = nullptr;
		for (const ac::Structure& X : S.structures)
			if (X.kind == ac::StructureKind::gatling && (!Out || ac::distance(X.position, At) < ac::distance(Out->position, At))) Out = &X;
		return Out;
	}

	/// The live bug of `Kind` nearest `At` in the mirror (null: none).
	const ac::Unit* NearestBug(const ac::GameState& S, const ac::Vec2 At, const ac::UnitKind Kind)
	{
		const ac::Unit* Best = nullptr;
		for (const ac::Unit& X : S.units)
			if (X.kind == Kind && X.bug && X.hp > 0 && (!Best || ac::distance(X.position, At) < ac::distance(Best->position, At))) Best = &X;
		return Best;
	}

	/// How far up a building is, 0 to 1.
	double Built(const ac::Structure& B)
	{
		if (!B.buildLeft) return 1;
		return 1 - *B.buildLeft / ac::Rules::buildTime(B.kind);
	}

	/// -AcMmoHour= given: the run holds its own hour.
	bool HourGiven()
	{
		double Hour = -1;
		return FParse::Value(FCommandLine::Get(), TEXT("AcMmoHour="), Hour);
	}
}

void UAcGameFlowSubsystem::TickMmoShots()
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; };
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::GameState* S = M ? &M->simulation().state : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	const FString Here = Mmo && Mmo->Here() ? Utf(Mmo->Here()->id) : FString(TEXT("-"));
	const int64 Ore = S && !S->players.empty() ? S->players[0].ore : -1;
	const int64 Hydrogen = S && !S->players.empty() ? S->players[0].hydrogen : -1;

	auto Stop = [&](const double Yaw) {
		if (Pilot) Pilot->SetTestInput(FVector2D::ZeroVector, Yaw);
	};
	auto Finish = [&] {
		// -AcMmoShotsClip: the world goes on until the recorder ends the run.
		if (Clipping)
		{
			ClipEnded = true;
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: clip: the set is over; the recorder ends the run"));
			return;
		}
		if (Pilot)
		{
			Pilot->Press(false);
			Stop(Pilot->Yaw());
		}
		StopMmo();
		NextStep();
	};
	auto Where = [&] {
		return U ? FString::Printf(TEXT("at %.1f,%.1f in %s, %lld ore, %lld MH"), U->position.x, U->position.y, *Here, (long long)Ore,
					   (long long)Hydrogen)
				 : FString::Printf(TEXT("no unit, in %s"), *Here);
	};
	// A phase that never got there: say where it was, and stop.
	auto TimedOut = [&](const TCHAR* What, const double Limit = PhaseLimit) {
		if (Since() <= Limit) return false;
		Check(false, FString::Printf(TEXT("%s within %.0f s (%s; %s)"), What, Limit, *Where(), Mmo ? *Mmo->Message() : TEXT("no client")));
		Finish();
		return true;
	};
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven()
			&& PC && PC->GetPawn() == Pilot && U;
	};

	if (ClipEnded) return;
	// -AcMmoShotsClipDwell: the moment held on screen; the phase's clock starts after it.
	if (Clipping && Now < ClipDwellUntil)
	{
		T.PhaseAt = Now;
		return;
	}
	// -AcMmoShotsB1: after a drill stop, when sparks were last alive (every
	// frame, a picture being written or not).
	if (B1StoppedAt >= 0 && Sim)
	{
		const UAcEffects* Fx = UAcEffects::Get(this);
		const FAcParticles* P = Fx ? AcEffectsBursts::ParticlesOf(Fx) : nullptr;
		if (P && P->Alive(EAcParticle::WeldSparks, Sim->LastFrame().Time) > 0) B1SparksAt = Now;
	}
	// A picture asked for: wait until it is written (the writer is async).
	if (!Shots.Pending.IsEmpty())
	{
		++Shots.Waited;
		const int64 Size = IFileManager::Get().FileSize(*Shots.Pending);
		if (Size > 0 && Size == Shots.LastSize)
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: saved %s (%lld bytes; %s)"), *FPaths::GetCleanFilename(Shots.Pending), (long long)Size,
				*Where());
			Shots.Pending.Empty();
			Shots.Saved += 1;
			T.PhaseAt = Now;
			return;
		}
		Shots.LastSize = Size;
		if (Shots.Waited > 600)
		{
			Check(false, FString::Printf(TEXT("%s written"), *Shots.Pending));
			Shots.Pending.Empty();
			Finish();
		}
		return;
	}
	// -AcMmoShotsClip: a moment is a mark in the recording, not a picture.
	// The recording keeps the HUD for the HUD's pictures (with -AcShotUI)
	// and drops it for the clean ones (the free camera's scenery).
	auto Clip = [&](const TCHAR* Name, const FString& What, const bool bUI) {
		if (!ClipHeld && !Clipping) return false;
		UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this);
		if (!Clipping && Shot) Shot->Release();
		Clipping = true;
		ClipHeld = false;
		if (Shot) Shot->SetShowUI(bUI && FParse::Param(FCommandLine::Get(), TEXT("AcShotUI")));
		Shots.Saved += 1;
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: clip: %s at world %.3f s: %s"), Name, GetWorld()->GetTimeSeconds(), *What);
		Advance();
		if (double Dwell = 0; FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsClipDwell="), Dwell) && Dwell > 0) ClipDwellUntil = Now + Dwell;
		return true;
	};
	// Asks for a picture with the HUD; the phase moves on once it is written.
	auto Shoot = [&](const TCHAR* Name, const FString& What) {
		if (!Wanted(Name))
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: %s: passed over (-AcMmoShotsOnly)"), Name);
			Shots.Skipped += 1;
			Advance();
			return;
		}
		if (Clip(Name, What, /*bUI*/ true)) return;
		Shots.Pending = FPaths::Combine(Shots.Dir, FString(Name) + TEXT(".png"));
		IFileManager::Get().Delete(*Shots.Pending, false, true, true);
		Shots.LastSize = -1;
		Shots.Waited = 0;
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: %s: %s"), Name, *What);
		FScreenshotRequest::RequestScreenshot(Shots.Pending, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		Advance();
	};
	// The same without the HUD (-AcMmoShotsW4's free camera).
	auto ShootClean = [&](const TCHAR* Name, const FString& What) {
		if (!Wanted(Name))
		{
			Shots.Skipped += 1;
			Advance();
			return;
		}
		if (Clip(Name, What, /*bUI*/ false)) return;
		Shots.Pending = FPaths::Combine(Shots.Dir, FString(Name) + TEXT(".png"));
		IFileManager::Get().Delete(*Shots.Pending, false, true, true);
		Shots.LastSize = -1;
		Shots.Waited = 0;
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: %s: %s"), Name, *What);
		FScreenshotRequest::RequestScreenshot(Shots.Pending, /*bShowUI*/ false, /*bAddFilenameSuffix*/ false);
		Advance();
	};
	// Drives the route: 1 there, 0 on the way, -1 stalled (4 s without half a cell).
	auto Drive = [&]() -> int32 {
		if (!U || !Pilot) return 0;
		const FVector2D P(U->position.x, U->position.y);
		while (Shots.RouteAt < Shots.Route.Num() && FVector2D::Distance(P, Shots.Route[Shots.RouteAt]) < 1.0) Shots.RouteAt += 1;
		if (Shots.RouteAt >= Shots.Route.Num())
		{
			Stop(Pilot->Yaw());
			return 1;
		}
		const FVector2D W = Shots.Route[Shots.RouteAt];
		Pilot->SetTestInput(FVector2D(0, -1), std::atan2(W.Y - P.Y, W.X - P.X));
		if (FVector2D::Distance(P, Shots.StallAt) > 0.5)
		{
			Shots.StallAt = P;
			Shots.StallSince = Now;
		}
		return Now - Shots.StallSince > 4.0 ? -1 : 0;
	};
	auto Route = [&](std::initializer_list<FVector2D> Points) {
		Shots.Route = TArray<FVector2D>(Points);
		Shots.RouteAt = 0;
		Shots.StallAt = U ? FVector2D(U->position.x, U->position.y) : FVector2D::ZeroVector;
		Shots.StallSince = Now;
	};
	// To `P`, round the realm's Citadel when it stands in the way (Q2: since
	// W4b the dev rig's Prospector unloads on its west side and the nearest
	// ore lies behind it; the B1 and C1 sets drove into its wall).
	auto RouteRound = [&](const ac::Vec2 P) {
		const ac::Structure* C = S && U ? CitadelOf(*S) : nullptr;
		const ac::Vec2 A = U ? U->position : P;
		const ac::Vec2 D = P - A;
		const double L = ac::distance(A, P);
		const double R = C ? ac::Rules::radius(C->kind) + 2.0 : 0.0;
		const double T = C && L > 0 ? FMath::Clamp(ac::dot(C->position - A, D) / (L * L), 0.0, 1.0) : 0.0;
		if (!C || L <= 0 || ac::distance(A + D * T, C->position) >= R)
		{
			Route({FVector2D(P.x, P.y)});
			return;
		}
		// Past it on the side the Prospector stands, square to the way.
		ac::Vec2 N(-D.y / L, D.x / L);
		if (ac::dot(A - C->position, N) < 0) N = N * -1.0;
		const ac::Vec2 W = C->position + N * (R + 1.0);
		Route({FVector2D(W.x, W.y), FVector2D(P.x, P.y)});
	};
	// A walking-grid route made drivable (Q2): the grid keeps 0.35 cells off
	// obstacles, the Prospector's body needs more (`pilotCanStand`), so a leg
	// that cuts a corner gets its end moved to the nearest spot, up to 2.5
	// cells off, from which this leg and the next are open.
	auto Drivable = [&](TArray<FVector2D>& Way) {
		if (!M || !U) return;
		const ac::Simulation& Sim = M->simulation();
		// (From its far side: the start may be where the unit stands now,
		// pressed against what it unloads at.)
		auto Open = [&](const ac::Vec2 A, const ac::Vec2 B) {
			const double L = ac::distance(A, B);
			for (double D = 0.4; D <= L; D += 0.2)
				if (!Sim.pilotCanStand(*U, A + (B - A) * (D / FMath::Max(L, 1e-6)))) return false;
			return true;
		};
		ac::Vec2 From = U->position;
		for (int32 I = 0; I < Way.Num(); ++I)
		{
			const ac::Vec2 W(Way[I].X, Way[I].Y);
			const TOptional<ac::Vec2> Next = I + 1 < Way.Num() ? TOptional<ac::Vec2>(ac::Vec2(Way[I + 1].X, Way[I + 1].Y)) : TOptional<ac::Vec2>();
			if (Open(From, W) && (!Next || Open(W, *Next)))
			{
				From = W;
				continue;
			}
			TOptional<ac::Vec2> Best;
			for (double R = 0.5; R <= 2.5 && !Best; R += 0.5)
				for (int32 K = 0; K < 16 && !Best; ++K)
				{
					const double A = K * 2.0 * PI / 16;
					const ac::Vec2 C = W + ac::Vec2(std::cos(A), std::sin(A)) * R;
					if (Open(From, C) && (!Next || Open(C, *Next))) Best = C;
				}
			if (!Best)
				for (double R = 0.5; R <= 2.5 && !Best; R += 0.5)
					for (int32 K = 0; K < 16 && !Best; ++K)
					{
						const double A = K * 2.0 * PI / 16;
						const ac::Vec2 C = W + ac::Vec2(std::cos(A), std::sin(A)) * R;
						if (Open(From, C)) Best = C;
					}
			if (Best)
			{
				Way[I] = FVector2D(Best->x, Best->y);
				From = *Best;
			}
			else
				From = W;
		}
	};
	// K2's drive along the walking grid, wedged (the grid's path keeps no
	// room for the Prospector's width and cuts close by deposits): back up
	// 0.8 s and plan again from there, twice at most (Q2). True: handled.
	auto K2Unstick = [&]() -> bool {
		if (Pilot && Now < K2BackUntil)
		{
			Pilot->SetTestInput(FVector2D(0, 1), Pilot->Yaw());
			return true;
		}
		if (Now - Shots.StallSince <= 4.0 || K2Unsticks >= 2) return false;
		K2Unsticks += 1;
		K2BackUntil = Now + 0.8;
		Shots.Route.Reset();
		Shots.RouteAt = 0;
		Shots.StallSince = Now + 0.8;
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmoshots: k2: wedged at %s, backing up to plan again (%d)"), *Where(), K2Unsticks);
		return true;
	};
	auto Stalled = [&](const TCHAR* What) {
		Check(false, FString::Printf(TEXT("%s: stalled %s, heading for %s"), What, *Where(),
			Shots.Route.IsValidIndex(Shots.RouteAt) ? *Shots.Route[Shots.RouteAt].ToString() : TEXT("-")));
		Finish();
	};
	// A spot in region-file cells (docs/regions.md: from the map's corner).
	auto Cell = [&](const double X, const double Y) {
		const ac::GroundRect* B = M ? &M->simulation().map->bounds : nullptr;
		return B ? FVector2D(B->minX + X, B->minZ + Y) : FVector2D(X, Y);
	};
	auto Face = [&](const ac::Vec2 At) {
		return U ? std::atan2(At.y - U->position.y, At.x - U->position.x) : 0.0;
	};
	// -AcMmoShotsU1: a load home as the unload tells it.
	auto U1Gain = [&](const int64 Amount, const int64 Tax) {
		const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this);
		if (const TSharedPtr<SAcProgressHud> W = H ? H->Widget() : nullptr) W->Gain(TEXT("ORE"), Amount, Tax, FPlatformTime::Seconds());
	};

	// The level-up cards, the first time they are up (the weld's XP).
	UAcLevelingSubsystem* Leveling = UAcLevelingSubsystem::Get(this);
	const bool bCards = Leveling && Leveling->DrivenOffer().IsSet() && Leveling->Cards().IsValid()
		&& Leveling->Cards()->GetVisibility().IsVisible();

	switch (T.Phase)
	{
	case 0:
		if (Sim && Sim->IsMmo()) StopMmo();
		IFileManager::Get().MakeDirectory(*Shots.Dir, true);
		// -AcMmoShotsClip: the recorder waits for the first moment wanted.
		Clipping = false;
		ClipHeld = false;
		ClipEnded = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsClip")) && UAcShotSubsystem::IsRecordRun())
		{
			if (UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this))
			{
				Shot->Hold();
				ClipHeld = true;
				UE_LOG(LogAutocraft, Display, TEXT("mmoshots: clip: the recorder held for the first moment wanted"));
			}
		}
		// -AcMmoShotsL1: the first ten minutes' last step at level 4 (its XP),
		// so the chain ends with a point spent and the goals begin.
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsL1")))
		{
			if (int32 Step = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoFirstStep="), Step)) FCommandLine::Append(TEXT(" -AcMmoFirstStep=6"));
			if (double Xp = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp))
			{
				FCommandLine::Append(*FString::Printf(TEXT(" -AcMmoDevXp=%.0f"), ac::Progression::xp(4)));
			}
		}
		// -AcMmoShotsO1: the new pilot earns its levels (no dev XP).
		if (double Xp = 0; FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsO1")) && !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp))
		{
			FCommandLine::Append(TEXT(" -AcMmoDevXp=0"));
		}
		// The new pilot starts at level 3 (1100 XP): the Derrick's floor, so
		// the MH loop (19-22) can build one, unless the run says otherwise.
		if (double Xp = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp)) FCommandLine::Append(TEXT(" -AcMmoDevXp=1100"));
		// -AcMmoShotsG2: the dev rig's home (a Citadel, level 25, a full wallet),
		// for this step only (the flow steps before it in the run go without).
		if ((FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsG2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsW2"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsB1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsK2"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsH2"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsH2Away")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsD"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsM2"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsD3")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN3"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsU1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS3"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsR2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsR3"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsX2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS4"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN4")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsBench")))
			&& !FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig")))
		{
			FCommandLine::Append(TEXT(" -AcMmoDevRig"));
		}
		// -AcMmoShotsStarfall: the fall 10 s after the pilot drives (the
		// streak shows its last 10 s), late in the afternoon, unless the run
		// says otherwise.
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsStarfall")))
		{
			if (double In = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoStarfall="), In)) FCommandLine::Append(TEXT(" -AcMmoStarfall=10"));
			if (double Hour = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour)) FCommandLine::Append(TEXT(" -AcMmoClockHour=17"));
		}
		// -AcMmoShotsE1: a fall 30 s after the pilot drives (time to stage
		// what stands in its way), late afternoon, clear, the start charted.
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsE1")))
		{
			if (double In = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoStarfall="), In)) FCommandLine::Append(TEXT(" -AcMmoStarfall=30"));
			if (double Hour = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour)) FCommandLine::Append(TEXT(" -AcMmoClockHour=17"));
			if (FString W; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoWeather="), W)) FCommandLine::Append(TEXT(" -AcMmoWeather=clear"));
			if (FString Seen; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevSeen="), Seen)) FCommandLine::Append(TEXT(" -AcMmoDevSeen=start"));
		}
		// -AcMmoShotsW4: late morning, clear, every region been to (the map).
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsW4")))
		{
			if (double Hour = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour)) FCommandLine::Append(TEXT(" -AcMmoClockHour=11"));
			if (FString W; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoWeather="), W)) FCommandLine::Append(TEXT(" -AcMmoWeather=clear"));
			if (FString Seen; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevSeen="), Seen))
				FCommandLine::Append(TEXT(" -AcMmoDevSeen=start,cold-fork,glass-flats,tern-ridge,slag-pockets,brine-pans,dust-terraces,rust-hollow,ashen-shelf"));
		}
		// -AcMmoShotsA2 and MapPick: the chain done, the dev rig (its Citadel by the pad).
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsA2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsMapPick")))
		{
			if (int32 Step = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoFirstStep="), Step)) FCommandLine::Append(TEXT(" -AcMmoFirstStep=0"));
			if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig"))) FCommandLine::Append(TEXT(" -AcMmoDevRig"));
		}
		// -AcMmoShotsP1: as Z2's, and the opening chain done (the frontier
		// pilots come only after it), the dev rig's Citadel. S4 too.
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsP1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS4"))
			|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN4")))
		{
			if (int32 Step = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoFirstStep="), Step)) FCommandLine::Append(TEXT(" -AcMmoFirstStep=0"));
			if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig"))) FCommandLine::Append(TEXT(" -AcMmoDevRig"));
		}
		// -AcMmoShotsZ1 and Z2: as W4's, late morning, clear, every region been to.
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ2"))
			|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsP1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ3"))
			|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ4")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsR3"))
			|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS4")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN4")))
		{
			if (double Hour = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour)) FCommandLine::Append(TEXT(" -AcMmoClockHour=11"));
			if (FString W; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoWeather="), W)) FCommandLine::Append(TEXT(" -AcMmoWeather=clear"));
			if (FString Seen; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevSeen="), Seen))
				FCommandLine::Append(TEXT(" -AcMmoDevSeen=start,cold-fork,glass-flats,tern-ridge,slag-pockets,brine-pans,dust-terraces,rust-hollow,ashen-shelf"));
		}
		// -AcMmoShotsM2: some regions been to, the rest uncharted.
		if (FString Seen; FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsM2")) && !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevSeen="), Seen))
		{
			FCommandLine::Append(TEXT(" -AcMmoDevSeen=start,cold-fork,rust-hollow,ashen-shelf,tern-ridge"));
		}
		// -AcMmoShotsH2Away: three hours down by the save's clock.
		if (double Down = 0; FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsH2Away")) && !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevDown="), Down))
		{
			FCommandLine::Append(TEXT(" -AcMmoDevDown=10800"));
		}
		// -AcMmoShotsS2B: the world the first run left running, resumed.
		// -AcMmoShotsH2Away: the first H2 run's world, its mercenary in it.
		StartMmo(!FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS2B")) && !FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsH2Away")));
		Advance();
		return;
	case 1:
		// K3: the startup time, from the process's start to the first frame
		// the pilot drives.
		if (Seated() && Mmo->Here() && SeatedAt < 0)
		{
			SeatedAt = Now;
			UE_LOG(LogAutocraft, Log, TEXT("k3: seated %.2f s after the process started"), Now - GStartTime);
		}
		// Seated on the start pad; a few seconds for the ground, the light
		// and the exposure to settle.
		if (Seated() && Mmo->Here() && Since() > 1.0)
		{
			Shots.Unit = U->id;
			Check(Here == TEXT("start"), FString::Printf(TEXT("seated in Prospector #%lld %s"), (long long)U->id, *Where()));
			if (!Pilot->ChaseEye()) Pilot->ToggleView();
			Advance();
			// -AcMmoShotsLook: only the world's look (100-111).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsLook"))) T.Phase = 100;
			// -AcMmoShotsP5: only the progression (200-215).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsP5"))) T.Phase = 200;
			// -AcMmoShotsRingshadow: only the ringshadow sweep (300-315).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsRingshadow"))) T.Phase = 300;
			// -AcMmoShotsStarfall: only a starfall (400-414).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsStarfall"))) T.Phase = 400;
			// -AcMmoShotsG2: contracts, respawn, depot, own pads (600-629).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsG2"))) T.Phase = 600;
			// -AcMmoShotsW2: the sites on the ground, the compact panel (700-724).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsW2"))) T.Phase = 700;
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsO1"))) T.Phase = 800;
			// -AcMmoShotsRig: only the rig screen (500-513).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsRig"))) T.Phase = 500;
			// -AcMmoShotsF2 (or -AcMmoShotsFlight): only the Dropship's flight (2400-2413).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsF2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsFlight"))) T.Phase = 2400;
			// -AcMmoShotsB1: only the drill's sparks and the AI's walk (900-919).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsB1"))) T.Phase = 900;
			// -AcMmoShotsC1: only the Citadel's tax and the realm wallet (1000-1012).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1"))) T.Phase = 1000;
			// -AcMmoShotsC2: only the Citadel shop (1100-1108).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC2"))) T.Phase = 1100;
			// -AcMmoShotsK2: only the Citadel missions and the Journal (1200-1212).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsK2"))) T.Phase = 1200;
			// -AcMmoShotsN2: only the Brood (1300-1311).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN2"))) T.Phase = 1300;
			// -AcMmoShotsS2A / S2B: the world server that runs on (1400-1415).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS2A"))) T.Phase = 1400;
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS2B"))) T.Phase = 1410;
			// -AcMmoShotsH2 / H2Away: mercenaries (1500-1511).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsH2"))) T.Phase = 1500;
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsH2Away"))) T.Phase = 1510;
			// -AcMmoShotsD: only the defences (1600-1611).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsD"))) T.Phase = 1600;
			// -AcMmoShotsM2: the satellite world map (1700-1712).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsM2"))) T.Phase = 1700;
			// -AcMmoShotsD3: only the turret upgrades (1800-1809).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsD3"))) T.Phase = 1800;
			// -AcMmoShotsN3: only the Matriarch (1900-1911).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN3"))) T.Phase = 1900;
			// -AcMmoShotsW4: seamless regions (2000-2026).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsW4"))) T.Phase = 2000;
			// -AcMmoShotsL1: KSTR's next goal (2100-2112).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsL1"))) T.Phase = 2100;
			// -AcMmoShotsU1: the HUD decluttered (2200-2216).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsU1"))) T.Phase = 2200;
			// -AcMmoShotsZ1: the first canyon zone (2300-2311).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ1"))) T.Phase = 2300;
			// -AcMmoShotsZ2: the canyon game (2500-2525).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ2"))) T.Phase = 2500;
			// -AcMmoShotsMapPick: the map's pad pick and refusals (3000-3008).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsMapPick"))) T.Phase = 3000;
			// -AcMmoShotsA2: the siege heard (2900-2916).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsA2"))) T.Phase = 2900;
			// -AcMmoShotsZ4: free placement in canyons (3100-3103).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ4"))) T.Phase = 3100;
			// -AcMmoShotsR2: realm projects (3200-3206).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsR2"))) T.Phase = 3200;
			// -AcMmoShotsR3: a Citadel a region (3300-3310).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsR3"))) T.Phase = 3300;
			// -AcMmoShotsX2: coming back (3400-3411).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsX2"))) T.Phase = 3400;
			// -AcMmoShotsS4: realm standings (3500-3509).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS4"))) T.Phase = 3500;
			// -AcMmoShotsN4 (and mmohunt): the hunt (3600-3615).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsN4"))) T.Phase = 3600;
			// -AcMmoShotsE1: scarcity and the epic fall (3700-3709).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsE1"))) T.Phase = 3700;
			// -AcMmoShotsZ3: the canyons after The Fork (2800-2812).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsZ3"))) T.Phase = 2800;
			// -AcMmoShotsP1: frontier pilots (2600-2613).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsP1"))) T.Phase = 2600;
			// -AcMmoShotsS3: daylight sight (2700-2714).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsS3"))) T.Phase = 2700;
			// -AcMmoShotsBench: the MMO bench (4000-4009).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsBench"))) T.Phase = 4000;
		}
		else TimedOut(TEXT("seated"));
		return;
	case 2:
		if (Since() > 5.0)
		{
			const FString Problem = FAcHudStyle::PlateProblem();
			Check(Problem.IsEmpty(), FString::Printf(TEXT("the HUD plates draw with their frames (%s)"), *Problem));
			Shoot(TEXT("01-spawn-pad-third"), TEXT("on the start pad, third person"));
		}
		return;
	case 3:
		Pilot->ToggleView();
		Advance();
		return;
	case 4:
		if (Since() > 2.0) Shoot(TEXT("02-spawn-pad-first"), TEXT("on the start pad, first person"));
		return;
	case 5:
		// Esc: out to the top-down camera (the unit handed back).
		Pilot->Leave();
		Advance();
		return;
	case 6:
		if (Since() > 3.0) Shoot(TEXT("03-topdown-start"), TEXT("top-down over the start region"));
		return;
	case 7:
		Pilot->TakeOver(Shots.Unit);
		Advance();
		return;
	case 8:
		if (Seated() && Since() > 2.5)
		{
			if (!Pilot->ChaseEye()) Pilot->ToggleView();
			Check(OpenWorldMap(), TEXT("M opened the world map"));
			Advance();
		}
		else TimedOut(TEXT("taken again"));
		return;
	case 9:
		if (Since() > 1.0) Shoot(TEXT("04-world-map"), TEXT("the world map (M)"));
		return;
	case 10:
		CloseWorldMap();
		// West toward the ore, to stand a Citadel's reach east of it.
		Route({Cell(69, 48)});
		Advance();
		return;
	case 11:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the build spot"));
		return;
	case 12:
		if (Since() > 1.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("B opened the build menu"));
			Advance();
		}
		return;
	case 13:
		if (Since() > 1.0) Shoot(TEXT("05-build-menu"), TEXT("the build menu (B)"));
		return;
	case 14:
	{
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		// The Citadel's number in the mode's menu.
		const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::citadel) + 1;
		Check(Number > 0 && Build && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
			&& *Build->Placing() == ac::StructureKind::citadel, FString::Printf(TEXT("%d held out a Citadel"), Number));
		Advance();
		return;
	}
	case 15:
		if (Since() > 1.5) Shoot(TEXT("06-citadel-ghost"), TEXT("the Citadel's ghost near the ore"));
		return;
	case 16:
		// The act key places it (the Build request); paid from the wallet.
		Shots.OreBefore = Ore;
		Pilot->Press(true);
		Pilot->Press(false);
		Advance();
		return;
	case 17:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr)
		{
			Shots.Citadel = C->id;
			if (bCards && Shots.CardsSince < 0) Shots.CardsSince = Now;
			if (bCards && !Shots.bCardsShot && Now - Shots.CardsSince > 0.6)
			{
				// The banner is up for a moment: take it with the cards.
				Shots.bCardsShot = true;
				Shoot(TEXT("07-level-up-cards"), FString::Printf(TEXT("the level-up cards, the Citadel %.0f%% up"), Built(*C) * 100));
				T.Phase = 17;
				return;
			}
			if (!Shots.bCardsShot && !bCards && Built(*C) >= 0.4)
			{
				// A world without picks (the skill tree replaces them): no cards to shoot.
				UE_LOG(LogAutocraft, Display, TEXT("mmoshots: 07-level-up-cards skipped: no cards"));
				Shots.bCardsShot = true;
				Shots.Skipped += 1;
			}
			if (Shots.bCardsShot && Built(*C) >= 0.4)
			{
				Check(Ore == Shots.OreBefore - ac::Rules::cost(ac::StructureKind::citadel),
					FString::Printf(TEXT("the Citadel paid: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
				Shoot(TEXT("08-citadel-going-up"), FString::Printf(TEXT("a Citadel going up, %.0f%%"), Built(*C) * 100));
				return;
			}
		}
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("built, not refused: %s"), *Why));
		}
		TimedOut(TEXT("the Citadel 40% up with the cards shown"), WeldLimit);
		return;
	case 18:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr; C && C->complete())
		{
			if (Since() > 2.0) Shoot(TEXT("09-citadel-finished"), TEXT("the Citadel finished"));
			return;
		}
		TimedOut(TEXT("the Citadel finished"), WeldLimit);
		return;
	case 19:
	{
		// Round the Citadel's north side to the ore field's middle patch.
		const ac::Structure* C = CitadelOf(*S);
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			const ac::OreDeposit& P = S->patches[I];
			if (P.remaining <= 0) continue;
			if (Best < 0 || ac::distance(P.position, C->position) < ac::distance(S->patches[Best].position, C->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore near the Citadel"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Patch = Best;
		const ac::Vec2 P = S->patches[Best].position;
		Route({FVector2D(C->position.x + 5, C->position.y - 6), FVector2D(C->position.x - 5, C->position.y - 6), FVector2D(P.x + 1, P.y)});
		Advance();
		return;
	}
	case 20:
	{
		// Up to the patch until the act key would drill it.
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1)
		{
			// At the last waypoint and not in reach yet: straight at it.
			Shots.Route.Add(FVector2D(P.x, P.y));
		}
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 21:
		// Held: drilling.
		if (U && U->task == ac::Unit::Task::mining && Since() > 1.5)
		{
			Shoot(TEXT("10-drilling-ore"), TEXT("drilling ore, the act key held"));
			return;
		}
		TimedOut(TEXT("drilling"), 15);
		return;
	case 22:
		if (U && U->carrying > 0)
		{
			Pilot->Press(false);
			Shots.OreBefore = Ore;
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Shots.bShotBefore = false;
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 30);
		return;
	case 23:
	{
		// To the Citadel: a picture just before, then into it.
		const ac::Structure* C = CitadelOf(*S);
		const double Gap = U ? ac::distance(U->position, C->position) - ac::Rules::radius(C->kind) : 1e9;
		if (U && U->carrying > 0 && !Shots.bShotBefore && Gap < 3.0)
		{
			Shots.bShotBefore = true;
			Stop(Face(C->position));
			T.Phase = 30;
			T.PhaseAt = Now;
			return;
		}
		if (U && U->carrying == 0)
		{
			Stop(Face(C->position));
			Advance();
			return;
		}
		Drive();
		TimedOut(TEXT("unloaded at the Citadel"), 30);
		return;
	}
	case 30:
		// (From 23.) Stood still a moment, the load still aboard.
		if (Since() > 1.0)
		{
			Shoot(TEXT("11-unload-before"), FString::Printf(TEXT("at the Citadel with %lld ore aboard, wallet %lld"),
				U ? (long long)U->carrying : -1LL, (long long)Ore));
			T.Phase = 23;
		}
		return;
	case 24:
		if (Since() > 1.0)
		{
			Check(Ore > Shots.OreBefore, FString::Printf(TEXT("the wallet grew: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
			Shoot(TEXT("12-unload-after"), FString::Printf(TEXT("unloaded, wallet %lld -> %lld"), (long long)Shots.OreBefore, (long long)Ore));
			// Then the MH loop (50-58), and back to the pad (25).
			T.Phase = 50;
		}
		return;
	case 50:
	{
		// Round the Citadel's west side, north to stand east of the region's
		// one well (60, 36; E1), facing it: a Derrick's reach (3 cells) away.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x - 5, C->position.y - 5), Cell(63, 36)});
		Advance();
		return;
	}
	case 51:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the well"));
		return;
	case 52:
		if (Since() > 1.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::derrick) + 1;
			Check(Number > 0 && Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
				&& *Build->Placing() == ac::StructureKind::derrick, FString::Printf(TEXT("B, %d held out a Derrick"), Number));
			const TOptional<ac::Vec2> At = Build ? Build->Spot(ac::StructureKind::derrick) : TOptional<ac::Vec2>();
			Check(At.IsSet(), TEXT("the Derrick's ghost on a well"));
			Advance();
		}
		return;
	case 53:
		if (Since() > 1.5) Shoot(TEXT("19-derrick-ghost"), TEXT("the Derrick's ghost on the well"));
		return;
	case 54:
		Shots.OreBefore = Ore;
		Pilot->Press(true);
		Pilot->Press(false);
		Advance();
		return;
	case 55:
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("Derrick built, not refused: %s"), *Why));
		}
		for (const ac::Structure& B : S->structures)
		{
			if (B.kind != ac::StructureKind::derrick || B.owner != ac::Pilot::player) continue;
			Shots.Derrick = B.id;
			if (B.complete() && Since() > 2.0)
			{
				Check(Ore == Shots.OreBefore - ac::Rules::cost(ac::StructureKind::derrick),
					FString::Printf(TEXT("the Derrick paid: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
				Shoot(TEXT("20-derrick-built"), TEXT("the Derrick finished on the well"));
				return;
			}
		}
		TimedOut(TEXT("the Derrick finished"), WeldLimit);
		return;
	case 56:
	{
		// Up to the Derrick until the act key would go in; then a click.
		const std::optional<ac::Structure> D = S->structure(Shots.Derrick);
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (D && Target && Target->is<ac::PilotTarget::Derrick>())
		{
			Stop(Face(D->position));
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
			return;
		}
		if (D && Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(D->position));
		TimedOut(TEXT("at the Derrick"), 15);
		return;
	}
	case 57:
		// Inside, pumping: a picture a moment in.
		if (U && U->task == ac::Unit::Task::inDerrick && Since() > 1.0)
		{
			Shoot(TEXT("21-pumping"), TEXT("pumping MH inside the Derrick"));
			return;
		}
		TimedOut(TEXT("inside the Derrick"), 10);
		return;
	case 58:
		if (U && U->carrying > 0 && U->hydrogen == true)
		{
			Shots.HydrogenBefore = Hydrogen;
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("a load of MH pumped"), 15);
		return;
	case 59:
		if (U && U->carrying == 0)
		{
			Stop(Face(CitadelOf(*S)->position));
			Advance();
			return;
		}
		Drive();
		TimedOut(TEXT("the MH unloaded at the Citadel"), 30);
		return;
	case 60:
		if (Since() > 1.0)
		{
			Check(Hydrogen > Shots.HydrogenBefore && Hydrogen > 0,
				FString::Printf(TEXT("the wallet's MH grew: %lld -> %lld"), (long long)Shots.HydrogenBefore, (long long)Hydrogen));
			Shoot(TEXT("22-mh-unloaded"), FString::Printf(TEXT("MH unloaded, wallet %lld -> %lld MH"), (long long)Shots.HydrogenBefore,
				(long long)Hydrogen));
		}
		return;
	case 61:
	{
		// Back round to the Citadel's west side, where 25 sets off for the pad.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x - 5, C->position.y + 5), FVector2D(C->position.x - 5, C->position.y)});
		Advance();
		return;
	}
	case 62:
		if (const int32 D = Drive(); D == 1) T.Phase = 25;
		else if (D < 0) Stalled(TEXT("back round the Citadel"));
		return;
	case 25:
	{
		// Back round the Citadel's north side to the start pad.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x - 5, C->position.y - 6), FVector2D(C->position.x + 5, C->position.y - 6), Cell(76, 48)});
		Advance();
		return;
	}
	case 26:
	{
		// F2: at the start pad, a Landing Pad put up under it and one in
		// Cold Fork (the `ferry` dev lines), MH to fly.
		const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
		if (Near && Near->Within())
		{
			Stop(Pilot->Yaw());
			Advance();
			return;
		}
		if (const int32 D = Drive(); D == 1 && !Shots.PadsAsked)
		{
			Shots.PadsAsked = true;
			Check(Mmo->DevClock(TEXT("ferry pad here\nferry pad cold-fork\nferry mh 200")), TEXT("Landing Pads here and in Cold Fork"));
		}
		else if (D < 0) Stalled(TEXT("to the pad"));
		if (Shots.PadsAsked) TimedOut(TEXT("on a Landing Pad"), 15);
		return;
	}
	case 27:
		if (Since() > 1.0)
		{
			Check(OpenWorldMap(), TEXT("M opened the world map on the pad"));
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			Check(Map && Map->PickPad(TEXT("cold-fork"), TEXT("Landing Pad 1")), TEXT("picked Cold Fork's Landing Pad"));
			Advance();
		}
		return;
	case 28:
		if (Since() > 1.0) Shoot(TEXT("13-world-map-pick"), TEXT("the world map on the pad, Cold Fork picked"));
		return;
	case 29:
		if (const TSharedPtr<SAcWorldMap> Map = WorldMapWidget())
		{
			Shots.VisitWas = Mmo->Visit();
			Map->Fly();
			T.Phase = 40;
			T.PhaseAt = Now;
			return;
		}
		Check(false, TEXT("the world map still open for FLY"));
		Finish();
		return;
	case 40:
		// The Dropship's flight (16 s and more): a frame from its start.
		if (Mmo->FlyingTo())
		{
			if (Since() > 2.0) Shoot(TEXT("14-flight-mid"), FString::Printf(TEXT("in flight to %s"), *Utf(Mmo->FlyingTo()->id)));
			return;
		}
		TimedOut(TEXT("lifted off"), 10);
		return;
	case 41:
		if (Mmo->Visit() > Shots.VisitWas && Seated() && Since() > 5.0)
		{
			Check(Here == TEXT("cold-fork"), FString::Printf(TEXT("landed %s"), *Where()));
			Shoot(TEXT("15-landing-cold-fork"), TEXT("landed on Cold Fork's pad"));
			return;
		}
		TimedOut(TEXT("landed"));
		return;
	case 42:
		// South off the plateau by its ramp, over the south edge into the
		// start region.
		Shots.VisitWas = Mmo->Visit();
		Route({Cell(82, 56), Cell(84, 62), Cell(84, 76), Cell(73, 84), Cell(73, 104)});
		Advance();
		return;
	case 43:
		if (Mmo->Visit() > Shots.VisitWas)
		{
			Stop(Pilot->Yaw());
			Advance();
			return;
		}
		if (Drive() < 0) Stalled(TEXT("to Cold Fork's south edge"));
		else TimedOut(TEXT("crossed into the start region"), 90);
		return;
	case 44:
		if (Seated() && Since() > 1.0)
		{
			Check(Here == TEXT("start"), FString::Printf(TEXT("crossed %s"), *Where()));
			Shots.UnitAt = FVector2D(U->position.x, U->position.y);
			Shoot(TEXT("16-edge-crossing"), TEXT("just after crossing into the start region"));
			return;
		}
		TimedOut(TEXT("seated after the crossing"), 15);
		return;
	case 45:
		// Stood still a while (where it stands, now and 4 s later: the
		// server's unit and the drawn one must agree), then QUIT TO HOME
		// and MMORPG (TEST): the world resumed.
		if (Since() < 4.0) return;
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: before leaving: %s (%.2f cells from 4 s ago)"), *Where(),
			FVector2D::Distance(FVector2D(U->position.x, U->position.y), Shots.UnitAt));
		Shots.Region = Here;
		Shots.UnitAt = FVector2D(U->position.x, U->position.y);
		StopMmo();
		StartMmo(false);
		Advance();
		return;
	case 46:
		if (Seated() && Mmo->Here() && Since() > 6.0)
		{
			const double Off = FVector2D::Distance(FVector2D(U->position.x, U->position.y), Shots.UnitAt);
			Check(Here == Shots.Region && Off < 1.0, FString::Printf(TEXT("resumed where it left (%.2f cells off) %s"), Off, *Where()));
			Shoot(TEXT("17-resumed"), TEXT("after leaving the mode and resuming"));
			return;
		}
		TimedOut(TEXT("seated after the resume"));
		return;
	case 47:
		// A level-up pick (Z), then the HUD's plates must still draw with
		// their frames (Q5: they once drew as flat glass; FAcHudStyle::PlateProblem).
		if (!Leveling || !Leveling->DrivenOffer().IsSet())
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: 18-after-pick skipped: no cards"));
			Shots.Skipped += 1;
			T.Phase = 49;
			T.PhaseAt = Now;
			return;
		}
		Check(Leveling->PickKey(TEXT("z")), TEXT("Z picked a level-up card"));
		Advance();
		return;
	case 48:
		if (Since() > 1.5)
		{
			const FString Problem = FAcHudStyle::PlateProblem();
			Check(Problem.IsEmpty(), FString::Printf(TEXT("the HUD plates draw with their frames after the pick (%s)"), *Problem));
			Shoot(TEXT("18-after-pick"), TEXT("after a level-up pick (Z)"));
		}
		return;
	// The look (-AcMmoShotsLook, docs/mmorpg-m6.md V2): the spawn, the giant
	// from the cockpit, the ore up close and drilled, a well; the hour and
	// the weather from -AcMmoHour and -AcMmoWeather.
	case 100:
		if (Since() > 5.0) Shoot(TEXT("look-01-spawn-third"), TEXT("on the start pad, third person"));
		return;
	case 101:
	{
		Pilot->ToggleView();
		const FVector G = FAcGasGiant::Direction();
		Stop(std::atan2(G.Y, G.X));
		Pilot->Eye().Pitch = 0.12;
		Advance();
		return;
	}
	case 102:
		if (Since() > 2.5) Shoot(TEXT("look-02-first-giant"), TEXT("first person, facing the giant"));
		return;
	case 103:
	{
		Pilot->ToggleView();
		Pilot->Eye().Pitch = 0;
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			if (S->patches[I].remaining <= 0) continue;
			if (Best < 0 || ac::distance(S->patches[I].position, U->position) < ac::distance(S->patches[Best].position, U->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore in the start region"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Patch = Best;
		const ac::Vec2 P = S->patches[Best].position;
		const ac::Vec2 Off = P - U->position;
		const double L = std::max(ac::length(Off), 1e-6);
		Route({FVector2D(P.x - Off.x / L * 4.0, P.y - Off.y / L * 4.0)});
		Advance();
		return;
	}
	case 104:
		if (const int32 D = Drive(); D != 0)
		{
			Stop(Face(S->patches[Shots.Patch].position));
			Advance();
		}
		return;
	case 105:
		if (Since() > 2.0) Shoot(TEXT("look-03-ore-near"), TEXT("a Stardust deposit, a few cells off"));
		return;
	case 106:
	{
		// Up to it until the act key would drill it.
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(P));
		TimedOut(TEXT("at the ore"), 15);
		return;
	}
	case 107:
		if (U && U->task == ac::Unit::Task::mining && Since() > 1.5)
		{
			Shoot(TEXT("look-04-drilling"), TEXT("drilling ore up close"));
			return;
		}
		TimedOut(TEXT("drilling"), 15);
		return;
	case 108:
	{
		Pilot->Press(false);
		static const std::vector<ac::Well> None;
		const std::vector<ac::Well>& Wells = S->wells ? *S->wells : None;
		int32 Best = -1;
		for (int32 I = 0; I < (int32)Wells.size(); ++I)
		{
			if (Best < 0 || ac::distance(Wells[I].position, U->position) < ac::distance(Wells[Best].position, U->position)) Best = I;
		}
		Check(Best >= 0, TEXT("a well in the start region"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Derrick = Best;
		const ac::Vec2 W = Wells[Best].position;
		const ac::Vec2 Off = W - U->position;
		const double L = std::max(ac::length(Off), 1e-6);
		Route({FVector2D(W.x - Off.x / L * 7.0, W.y - Off.y / L * 7.0)});
		Advance();
		return;
	}
	case 109:
		if (const int32 D = Drive(); D != 0)
		{
			// A little to its side, so the Prospector does not hide the pool.
			Stop(Face((*S->wells)[size_t(Shots.Derrick)].position) - 0.3);
			Advance();
		}
		return;
	case 110:
		if (Since() > 2.0) Shoot(TEXT("look-05-well"), TEXT("a Metallic Hydrogen well"));
		return;
	case 111:
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsP5
	case 200:
		// The dev XP's progress in, first person.
		if (Mmo->Progress() && Since() > 4.0)
		{
			const ac::net::ProgressMessage& P = *Mmo->Progress();
			Check(P.level >= 10 && P.points >= (int64)UE_ARRAY_COUNT(P5Spends),
				FString::Printf(TEXT("the dev XP's progress: level %lld, %lld points"), (long long)P.level, (long long)P.points));
			Pilot->ToggleView();
			P5Spent = 0;
			P5AskedAt = 0;
			Advance();
		}
		else TimedOut(TEXT("the progress"));
		return;
	case 201:
		if (Since() > 2.0) Shoot(TEXT("p5-01-hud-points"), TEXT("the HUD: the points badge and the slot row"));
		return;
	case 202:
		Check(OpenSkillScreen() && SkillScreenWidget().IsValid(), TEXT("K opens the skill screen"));
		Advance();
		return;
	case 203:
		if (Since() > 1.5) Shoot(TEXT("p5-02-skill-screen-empty"), TEXT("the skill screen, nothing spent"));
		return;
	case 204:
	{
		// One point at a time through the screen, each once the last is in.
		const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!W || !P)
		{
			Check(false, TEXT("the skill screen stays open"));
			Finish();
			return;
		}
		if (P5Spent >= (int32)UE_ARRAY_COUNT(P5Spends))
		{
			Advance();
			return;
		}
		const ac::Skill Want = P5Spends[P5Spent];
		int64 Ranks = 0;
		for (int32 I = 0; I <= P5Spent; ++I) Ranks += P5Spends[I] == Want ? 1 : 0;
		const auto Has = P->ranks.find(Want);
		if (Has != P->ranks.end() && Has->second >= Ranks)
		{
			P5Spent += 1;
			P5AskedAt = 0;
			T.PhaseAt = Now;
			return;
		}
		if (!Mmo->SkillRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("spend %s: %s"), *Utf(std::string(ac::rawValue(Want))), *Mmo->SkillRefusal()));
			Finish();
			return;
		}
		// Asked, and again each second until the server has it.
		if (Now - P5AskedAt > 1.0)
		{
			W->PickSkill(Want);
			W->Learn();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("the points spent"), 20);
		return;
	}
	case 205:
	{
		const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (W && P && P->slots[0] == ac::Skill::overcharge)
		{
			W->PickSkill(ac::Skill::overcharge);
			Advance();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickSkill(ac::Skill::overcharge);
			W->SlotPicked(0);
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Overcharge in Q"), 10);
		return;
	}
	case 206:
		if (Since() > 1.5) Shoot(TEXT("p5-03-skill-screen-spent"), TEXT("points spent, Overcharge picked and in Q"));
		return;
	case 207:
		if (const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget()) W->SetTab(SAcSkillScreen::ETab::Blueprints);
		Advance();
		return;
	case 208:
		if (Since() > 1.0) Shoot(TEXT("p5-04-skill-screen-blueprints"), TEXT("the BLUEPRINTS tab"));
		return;
	case 209:
		// Out, and Q: Overcharge's cooldown on the slot row.
		CloseSkillScreen();
		if (ac::net::Mirror* Mine = Mmo->Mirror(); Mine && Mine->simulation().pilot) Mine->simulation().pilot->slot = 1;
		Advance();
		return;
	case 210:
		if (Since() > 1.5) Shoot(TEXT("p5-05-hud-slot-cooldown"), TEXT("Q used: the cooldown ring on the slot row"));
		return;
	case 211:
		// The banner as level 10 shows it (the real path's words), and a find.
		if (Leveling)
		{
			Leveling->ShowBanner(AcLeveling::BannerTitle(ac::UnitKind::prospector, 10), AcProgressHud::BannerSub(9, 10),
				AcProgressHud::BannerGate(9, 10));
		}
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && Hud->Widget())
		{
			Hud->Widget()->Toast(TEXT("BLUEPRINT FOUND: FOUNDRY"), TEXT("Opens at level 15  ·  +750 XP"), FAcHudStyle::LevelGold(), Now);
		}
		Advance();
		return;
	case 212:
		if (Since() > 0.8) Shoot(TEXT("p5-06-level-banner"), TEXT("the level-up banner and a find toast"));
		return;
	case 213:
		// The banner and the toast gone, then B.
		if (Since() > 5.0)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("B opened the build menu"));
			Advance();
		}
		return;
	case 214:
		if (Since() > 1.0) Shoot(TEXT("p5-07-build-menu"), TEXT("the build menu from the blueprints"));
		return;
	case 215:
		Check(Shots.Saved + Shots.Skipped == 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsG2
	case 600:
		// The dev rig's Citadel, the contracts drawn; first person.
		if (Mmo->Progress() && Mmo->Progress()->contracts.size() == 3 && S && CitadelOf(*S) && Since() > 4.0)
		{
			// Away from the Citadel's wall (the dev rig stands it there).
			Pilot->ToggleView();
			Stop(Face(U->position * 2.0 - CitadelOf(*S)->position));
			P5AskedAt = 0;
			G2Dome = -1;
			Advance();
		}
		else TimedOut(TEXT("the contracts and the dev rig's Citadel"));
		return;
	case 601:
		if (Since() > 2.0) Shoot(TEXT("g2-01-contracts"), TEXT("the KSTR contract panel"));
		return;
	case 602:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			const ac::net::Contract& C = Mmo->Progress()->contracts[0];
			ac::net::ProgressMessage::News N;
			N.kind = ac::net::ProgressMessage::NewsKind::contract;
			N.id = std::to_string(C.id);
			N.xp = C.xp;
			N.value = C.hydrogen;
			Hud->PreviewNews(N);
		}
		Advance();
		return;
	case 603:
		if (Since() > 0.8) Shoot(TEXT("g2-02-contract-done"), TEXT("a contract done: its toast"));
		return;
	case 604:
		if (Since() > 5.0)
		{
			UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
			if (Hud) Hud->ToggleAbandon();
			Check(Hud && Hud->AbandonArmed(), TEXT("X armed an abandon"));
			Advance();
		}
		return;
	case 605:
		if (Since() > 0.6) Shoot(TEXT("g2-03-abandon-armed"), TEXT("X: which contract to drop"));
		return;
	case 606:
	{
		UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
		Check(Hud && Hud->Abandon(2), TEXT("3 asked to abandon slot 3"));
		Advance();
		return;
	}
	case 607:
		if (Mmo->Progress()->contracts.size() == 3 && Mmo->Progress()->contracts[2].state == ac::net::Contract::State::abandoned)
		{
			if (Since() > 1.0) Shoot(TEXT("g2-04-abandoned"), TEXT("slot 3 abandoned: back at dawn"));
			return;
		}
		TimedOut(TEXT("slot 3 abandoned"), 10);
		return;
	case 608:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			ac::net::ProgressMessage::Respawn R;
			R.at = (Mmo->Sky() ? Mmo->Sky()->time : 0) + 8;
			R.region = Mmo->Here() ? Mmo->Here()->id : std::string();
			R.home = ac::StructureKind::citadel;
			R.cargo = 40;
			Hud->PreviewRespawn(R);
		}
		Advance();
		return;
	case 609:
		if (Since() > 1.2) Shoot(TEXT("g2-05-respawn"), TEXT("the respawn screen"));
		return;
	case 610:
		// F2: a Landing Pad under the pilot (Homing beacon is retired).
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->PreviewRespawn(std::nullopt);
		Check(Mmo->DevClock(TEXT("ferry pad here")), TEXT("g2: the server puts a Landing Pad under the pilot"));
		Advance();
		return;
	case 611:
	{
		// The Landing Pad in the World message.
		bool bPad = false;
		if (const ac::net::WorldMessage::Region* R = Mmo->Here())
		{
			for (const ac::net::WorldMessage::Pad& Q : R->pads) bPad |= Q.building == ac::StructureKind::landingPad;
		}
		if (bPad && Since() > 1.0)
		{
			ToggleWorldMap();
			Check(WorldMapWidget().IsValid(), TEXT("M opened the world map"));
			Advance();
			return;
		}
		TimedOut(TEXT("a Landing Pad of the realm's here"), 15);
		return;
	}
	case 612:
		if (Since() > 1.5)
		{
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			Check(Near && Near->Within(), FString::Printf(TEXT("at the Landing Pad: %s %.1f of %.1f"),
				Near ? *Near->Name : TEXT("-"), Near ? Near->Distance : -1.0, Near ? Near->Reach : -1.0));
			Shoot(TEXT("g2-06-map-own-pad"), TEXT("the world map at a Landing Pad, the contracts' marks"));
		}
		return;
	case 613:
		// To the Lander wreck (a 5 s hold opens it; a crate opens on touch).
		CloseWorldMap();
		Route({Cell(66, 34), Cell(54, 29.5)});
		Advance();
		return;
	case 614:
		if (const int32 D = Drive(); D == 1)
		{
			// Up to its hull, then hold the act key.
			const FVector2D Wreck = Cell(50, 30);
			const ac::Vec2 At(Wreck.X, Wreck.Y);
			if (U && ac::distance(U->position, At) <= 1.9)
			{
				Stop(Face(At));
				Pilot->Press(true);
				Advance();
				return;
			}
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.5), Face(At));
			TimedOut(TEXT("at the Lander wreck"), 40);
		}
		else if (D < 0) Stalled(TEXT("to the Lander wreck"));
		return;
	case 615:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->opening && P->opening->since > 0 && P->opening->needs > 0)
		{
			const double Share = ((Mmo->Sky() ? Mmo->Sky()->time : 0) - P->opening->since) / P->opening->needs;
			if (Share > 0.35 && Share < 0.85)
			{
				Shoot(TEXT("g2-07-opening-ring"), FString::Printf(TEXT("the opening ring at the Lander wreck, %.0f%%"), Share * 100));
				return;
			}
		}
		else if (P && P->opening && !P->opening->refusal.empty())
		{
			Check(false, FString::Printf(TEXT("the wreck opens: %s"), *Utf(P->opening->refusal)));
			Finish();
			return;
		}
		TimedOut(TEXT("the wreck opening"), 20);
		return;
	case 616:
		if (Mmo->Progress()->sitesFound.count("start/Lander wreck"))
		{
			Pilot->Press(false);
			Advance();
			return;
		}
		TimedOut(TEXT("the wreck opened"), 20);
		return;
	case 617:
		// The toast faded in.
		if (Since() > 0.8) Shoot(TEXT("g2-08-site-toast"), TEXT("the wreck opened: the news toast"));
		return;
	case 618:
		ToggleWorldMap();
		Advance();
		return;
	case 619:
		if (Since() > 1.5) Shoot(TEXT("g2-09-map-sites"), TEXT("the world map: the site opened, the contracts' marks"));
		return;
	case 620:
		// To the main run's build spot by the pad (its Citadel's ghost fits there).
		CloseWorldMap();
		Route({Cell(62, 34), Cell(69, 48)});
		Advance();
		return;
	case 621:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			if (Since() > 1.0) Advance();
		}
		else if (D < 0) Stalled(TEXT("to the build spot"));
		return;
	case 622:
	{
		// A Hab Dome there (B, its number, the act key).
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::habDome) + 1;
		Check(Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet(),
			FString::Printf(TEXT("%d held out a Hab Dome"), Number));
		Advance();
		return;
	}
	case 623:
		if (Since() > 1.0)
		{
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
		}
		return;
	case 624:
		if (const ac::Structure* D = S ? HabDomeOf(*S) : nullptr; D && D->complete())
		{
			G2Dome = D->id;
			Route({Cell(66, 50), Cell(61, 50.1), Cell(58.8, 50.1)});
			Advance();
			return;
		}
		for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("the Hab Dome built, not refused: %s"), *Why));
		TimedOut(TEXT("the Hab Dome finished"), WeldLimit);
		return;
	case 625:
		if (const int32 D = Drive(); D == 1)
		{
			// Up to the deposit until the act key would drill it.
			// The west vein's east mound (E1).
			const FVector2D Deposit = Cell(55.7, 50.1);
			const ac::Vec2 At(Deposit.X, Deposit.Y);
			const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
			if (Target && Target->is<ac::PilotTarget::Patch>())
			{
				Stop(Face(At));
				Pilot->Press(true);
				Advance();
				return;
			}
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(At));
			TimedOut(TEXT("at the ore"), 50);
		}
		else if (D < 0) Stalled(TEXT("to the ore"));
		return;
	case 626:
		if (U && U->carrying > 0 && U->task != ac::Unit::Task::mining)
		{
			Pilot->Press(false);
			if (const std::optional<ac::Structure> D = S->structure(G2Dome)) Stop(Face(D->position));
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 45);
		return;
	case 627:
		if (Since() > 1.5) Shoot(TEXT("g2-10-depot-prompt"), TEXT("a load, the Hab Dome as a depot: the prompt and the compass"));
		return;
	case 628:
		if (const std::optional<ac::Structure> D = S->structure(G2Dome))
		{
			if (U && U->carrying == 0)
			{
				Stop(Face(D->position));
				if (Since() > 0.4) Shoot(TEXT("g2-11-depot-unloaded"), TEXT("the load unloaded at the Hab Dome"));
				return;
			}
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.8), Face(D->position));
		}
		TimedOut(TEXT("unloaded at the Hab Dome"), 45);
		return;
	case 629:
		// After the last picture: the news no run brings, for their sounds
		// (J1; -AcAudioLog shows blueprint, made and research).
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			using K = ac::net::ProgressMessage::NewsKind;
			for (const auto& [NewsKind, NewsId] : {std::pair{K::blueprint, std::string("habDome")}, std::pair{K::made, std::string("drillHead")},
					 std::pair{K::researched, std::string("drillHead")}})
			{
				ac::net::ProgressMessage::News N;
				N.kind = NewsKind;
				N.id = NewsId;
				N.value = 2;
				Hud->PreviewNews(N);
			}
		}
		Advance();
		return;
	case 630:
		if (Since() < 1.5) return;
		Check(Shots.Saved + Shots.Skipped == 11, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsW2
	case 700:
		// The dev rig's home, the contracts drawn, noon; first person away from the Citadel.
		if (Mmo->Progress() && Mmo->Progress()->contracts.size() == 3 && S && CitadelOf(*S) && Since() > 4.0)
		{
			Mmo->DevClock(TEXT("hour 11"));
			Pilot->ToggleView();
			Stop(Face(U->position * 2.0 - CitadelOf(*S)->position));
			Advance();
		}
		else TimedOut(TEXT("the contracts and the dev rig's Citadel"));
		return;
	case 701:
		if (Since() > 2.5) Shoot(TEXT("w2-01-panel-compact"), TEXT("the contract panel, compact: one line a contract"));
		return;
	case 702:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
		Advance();
		return;
	case 703:
		if (Since() > 0.6) Shoot(TEXT("w2-02-panel-expanded"), TEXT("T: the whole contract panel"));
		return;
	case 704:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
		// Third person, a little down, for the sites.
		Pilot->ToggleView();
		Pilot->Eye().Pitch = -0.12;
		W2From = U->position;
		W2Yaw = Pilot->Yaw();
		W2Kind = 0;
		Advance();
		return;
	case 705:
		// Kind W2Kind near: shut on the left, opened on the right, facing the pilot.
		if (AAcSites* Sites = AAcSites::Find(GetWorld()))
		{
			const ac::SiteKind Kind = ac::SiteKind(W2Kind);
			const bool bBig = Kind == ac::SiteKind::vault || Kind == ac::SiteKind::wreck;
			const double Ahead = bBig ? 9.5 : 6.0, Side = bBig ? 4.2 : 2.4;
			Sites->Stage({{Kind, W2At(W2From, W2Yaw, Ahead, -Side), W2From, false, false},
				{Kind, W2At(W2From, W2Yaw, Ahead, Side), W2From, true, false}});
		}
		Stop(W2Yaw);
		Advance();
		return;
	case 706:
		if (Since() > 1.5)
		{
			static const TCHAR* const Names[] = {TEXT("w2-03-crate-near"), TEXT("w2-04-wreck-near"), TEXT("w2-05-cache-near"), TEXT("w2-06-vault-near")};
			Shoot(Names[W2Kind], FString::Printf(TEXT("staged %s: shut (left), opened (right)"), Names[W2Kind] + 6));
			if (++W2Kind < 4) T.Phase = 705;
		}
		return;
	case 707:
		// The start region's own Lander wreck: up to 8 cells off.
		if (AAcSites* Sites = AAcSites::Find(GetWorld())) Sites->Stage({});
		Route({Cell(66, 34), Cell(58, 33)});
		Advance();
		return;
	case 708:
		if (const int32 D = Drive(); D == 1)
		{
			const FVector2D Wreck = Cell(50, 30);
			Stop(Face(ac::Vec2(Wreck.X, Wreck.Y)));
			if (Since() > 2.0)
			{
				const AAcSites* Sites = AAcSites::Find(GetWorld());
				Check(Sites && Sites->Where(TEXT("start/Lander wreck")).IsSet(), TEXT("the Lander wreck stands in the start region"));
				Check(Mmo->Progress()->sitesKnown.count("start/Lander wreck") > 0, TEXT("the Lander wreck is known"));
				Advance();
			}
		}
		else if (D < 0) Stalled(TEXT("to the Lander wreck's view"));
		return;
	case 709:
		Shoot(TEXT("w2-07-wreck-shut"), TEXT("the Lander wreck, shut and known: its lamp and column"));
		return;
	case 710:
	{
		// Up to its hull, then hold the act key (a 5 s hold opens it).
		const FVector2D Wreck = Cell(50, 30);
		const ac::Vec2 At(Wreck.X, Wreck.Y);
		if (U && ac::distance(U->position, At) <= 1.9)
		{
			Stop(Face(At));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.5), Face(At));
		TimedOut(TEXT("at the Lander wreck"), 40);
		return;
	}
	case 711:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->opening && P->opening->since > 0 && P->opening->needs > 0)
		{
			const double Share = ((Mmo->Sky() ? Mmo->Sky()->time : 0) - P->opening->since) / P->opening->needs;
			if (Share > 0.4 && Share < 0.85)
			{
				Shoot(TEXT("w2-08-opening-ring"), FString::Printf(TEXT("the opening ring on the wreck, %.0f%%"), Share * 100));
				return;
			}
		}
		TimedOut(TEXT("the wreck opening"), 20);
		return;
	case 712:
		if (Mmo->Progress()->sitesFound.count("start/Lander wreck"))
		{
			Pilot->Press(false);
			Route({Cell(58, 33)});
			Advance();
			return;
		}
		TimedOut(TEXT("the wreck opened"), 20);
		return;
	case 713:
		if (const int32 D = Drive(); D == 1)
		{
			const FVector2D Wreck = Cell(50, 30);
			Stop(Face(ac::Vec2(Wreck.X, Wreck.Y)));
			if (Since() > 1.5)
			{
				Shoot(TEXT("w2-09-wreck-opened"), TEXT("the Lander wreck opened: the hatch open, its lights out"));
			}
		}
		else if (D < 0) Stalled(TEXT("back to the wreck's view"));
		return;
	case 714:
	{
		// Four shut, known sites far off toward the east, a fan 18-44 cells away.
		const FVector2D East = Cell(118, 40);
		W2From = U->position;
		W2Yaw = Face(ac::Vec2(East.X, East.Y));
		if (AAcSites* Sites = AAcSites::Find(GetWorld()))
		{
			TArray<AAcSites::FStaged> Far;
			const double Ahead[] = {18, 26, 34, 44}, Across[] = {-9, 6, -3, 10};
			for (int32 K = 0; K < 4; ++K) Far.Add({ac::SiteKind(K), W2At(W2From, W2Yaw, Ahead[K], Across[K]), W2From, false, true});
			Sites->Stage(Far);
		}
		Stop(W2Yaw);
		Pilot->Eye().Pitch = 0.0;
		Advance();
		return;
	}
	case 715:
		if (Since() > 2.0)
		{
			const AAcSites* Sites = AAcSites::Find(GetWorld());
			Check(Sites && Sites->Columns() >= 4, FString::Printf(TEXT("the four columns (%d)"), Sites ? Sites->Columns() : -1));
			Shoot(TEXT("w2-10-far-day"), TEXT("four shut sites known, 18-44 cells off, by day: their columns"));
		}
		return;
	case 716:
		Mmo->DevClock(TEXT("hour 18"));
		Advance();
		return;
	case 717:
		if (Since() > 3.0) Shoot(TEXT("w2-11-far-dusk"), TEXT("the same at dusk"));
		return;
	case 718:
		// The four shut, near, in a row.
		if (AAcSites* Sites = AAcSites::Find(GetWorld()))
		{
			TArray<AAcSites::FStaged> Row;
			// Left of the line (the dev rig's base stands to the right), the cache nearest.
			const double Across[] = {-4.5, -10.5, 2.0, -17.0}, Ahead[] = {8, 11, 7, 14};
			for (int32 K = 0; K < 4; ++K) Row.Add({ac::SiteKind(K), W2At(W2From, W2Yaw, Ahead[K], Across[K]), W2From, false, true});
			Sites->Stage(Row);
		}
		Pilot->Eye().Pitch = -0.1;
		Advance();
		return;
	case 719:
		if (Since() > 1.6) Shoot(TEXT("w2-12-near-dusk"), TEXT("the four shut, near, at dusk: lamps, the vault's seam"));
		return;
	case 720:
		if (Since() < 1.0) return;
		if (AAcSites* Sites = AAcSites::Find(GetWorld())) Sites->Stage({});
		Check(Shots.Saved + Shots.Skipped == 12, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsO1
	case 800:
		// Step 1 open, the panel whole (T) for the radio's line; toward the crate.
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 1 && Since() > 1.0)
		{
			if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
			const FVector2D Crate = Cell(70, 42);
			Stop(Face(ac::Vec2(Crate.X, Crate.Y)));
			Advance();
		}
		else TimedOut(TEXT("the first ten minutes' step 1"));
		return;
	case 801:
		if (Since() > 1.2) Shoot(TEXT("o1-01-touchdown"), TEXT("step 1, Touchdown: the toast, the gold step (T), the crate's column"));
		return;
	case 802:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
		Route({Cell(70, 42)});
		Advance();
		return;
	case 803:
	{
		// Into the crate: it opens on touch, step 2 opens.
		const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo);
		if (Step && Step->step == 2)
		{
			Stop(PI);
			Advance();
			return;
		}
		if (Drive() < 0) Stalled(TEXT("to the Landing crate"));
		TimedOut(TEXT("the crate opened, step 2"), 40);
		return;
	}
	case 804:
		if (Since() > 1.5) Shoot(TEXT("o1-02-a-roof"), TEXT("step 2: the crate opened, the Citadel's blueprint; the toast and the panel"));
		return;
	case 805:
		// The build spot east of the ore (as 10-16), the Citadel placed.
		Route({Cell(69, 48)});
		Advance();
		return;
	case 806:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Shots.bShotBefore = false;
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the build spot"));
		return;
	case 807:
		// Held out (B, its number), placed a moment later, once its ghost has a spot.
		if (!Shots.bShotBefore && Since() > 1.0)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::citadel) + 1;
			Check(Number > 0 && Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
				&& *Build->Placing() == ac::StructureKind::citadel, FString::Printf(TEXT("B, %d held out a Citadel"), Number));
			Shots.bShotBefore = true;
		}
		else if (Shots.bShotBefore && Since() > 2.5)
		{
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
		}
		return;
	case 808:
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("Citadel built, not refused: %s"), *Why));
		}
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 3 && S && CitadelOf(*S))
		{
			Shots.Citadel = CitadelOf(*S)->id;
			Advance();
			return;
		}
		TimedOut(TEXT("the Citadel finished, step 3"), WeldLimit);
		return;
	case 809:
		if (Since() > 1.5) Shoot(TEXT("o1-03-first-haul"), TEXT("step 3: the Citadel finished; the toast, the tip line"));
		return;
	case 810:
	{
		// To the nearest ore round the Citadel's north side (as 19).
		const ac::Structure* C = CitadelOf(*S);
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			const ac::OreDeposit& P = S->patches[I];
			if (P.remaining <= 0) continue;
			if (Best < 0 || ac::distance(P.position, C->position) < ac::distance(S->patches[Best].position, C->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore near the Citadel"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Patch = Best;
		const ac::Vec2 P = S->patches[Best].position;
		Route({FVector2D(C->position.x + 5, C->position.y - 6), FVector2D(C->position.x - 5, C->position.y - 6), FVector2D(P.x + 1, P.y)});
		Advance();
		return;
	}
	case 811:
	{
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1) Shots.Route.Add(FVector2D(P.x, P.y));
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 812:
		if (U && U->carrying > 0)
		{
			Pilot->Press(false);
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 30);
		return;
	case 813:
		// Home: unloaded, step 4 open; then toward the wreck on the rise.
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 4 && U && U->carrying == 0)
		{
			Advance();
			return;
		}
		Drive();
		TimedOut(TEXT("the load home, step 4"), 30);
		return;
	case 814:
	{
		// Off the Citadel's wall a few steps toward the wreck, then the picture.
		const FVector2D Wreck = Cell(50, 30);
		if (Since() < 1.6)
		{
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.8), Face(ac::Vec2(Wreck.X, Wreck.Y)));
			return;
		}
		Stop(Face(ac::Vec2(Wreck.X, Wreck.Y)));
		if (Since() > 2.1) Shoot(TEXT("o1-04-the-wreck"), TEXT("step 4: a load home; the toast, the Lander wreck's column ahead"));
		return;
	}
	case 815:
	{
		// Over to the wreck (as 707-710), then hold the act key on it.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x + 5, C->position.y - 6), Cell(66, 34), Cell(58, 33)});
		Advance();
		return;
	}
	case 816:
		if (const int32 D = Drive(); D == 1) Advance();
		else if (D < 0) Stalled(TEXT("to the Lander wreck"));
		return;
	case 817:
	{
		const FVector2D Wreck = Cell(50, 30);
		const ac::Vec2 At(Wreck.X, Wreck.Y);
		if (U && ac::distance(U->position, At) <= 1.9)
		{
			Stop(Face(At));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.5), Face(At));
		TimedOut(TEXT("at the Lander wreck"), 40);
		return;
	}
	case 818:
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 5)
		{
			Pilot->Press(false);
			const int64 Level = ac::Progression::level(Mmo->Progress()->xp);
			Check(Level >= ac::Progression::blueprintFloor(ac::StructureKind::derrick),
				FString::Printf(TEXT("at the Derrick's floor by step 5: level %lld"), (long long)Level));
			// Back a little, to see the opened wreck.
			Pilot->SetTestInput(FVector2D(0, 0.6), Pilot->Yaw());
			Advance();
			return;
		}
		TimedOut(TEXT("the wreck opened, step 5"), 15);
		return;
	case 819:
		if (Since() > 0.7) Stop(Pilot->Yaw());
		if (Since() > 1.8) Shoot(TEXT("o1-05-liquid-gold"), TEXT("step 5: the wreck opened, the Derrick's blueprint; the toast, the panel"));
		return;
	case 820:
		// East of the north well (60, 36), facing it: a Derrick's reach away.
		Route({Cell(58, 33), Cell(63, 33), Cell(63, 36)});
		Advance();
		return;
	case 821:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Shots.bShotBefore = false;
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the north well"));
		return;
	case 822:
		if (!Shots.bShotBefore && Since() > 1.0)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::derrick) + 1;
			Check(Number > 0 && Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
				&& *Build->Placing() == ac::StructureKind::derrick, FString::Printf(TEXT("B, %d held out a Derrick"), Number));
			Shots.bShotBefore = true;
		}
		else if (Shots.bShotBefore && Since() > 2.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Spot(ac::StructureKind::derrick).IsSet(), TEXT("the Derrick's ghost on the north well"));
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
		}
		return;
	case 823:
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("Derrick built, not refused: %s"), *Why));
		}
		for (const ac::Structure& B : S->structures)
		{
			if (B.kind != ac::StructureKind::derrick || B.owner != ac::Pilot::player || !B.complete()) continue;
			Shots.Derrick = B.id;
			Advance();
			return;
		}
		TimedOut(TEXT("the Derrick finished"), WeldLimit);
		return;
	case 824:
	{
		// Into it (a click), pumping until step 6 opens.
		const std::optional<ac::Structure> D = S->structure(Shots.Derrick);
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (D && Target && Target->is<ac::PilotTarget::Derrick>())
		{
			Stop(Face(D->position));
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
			return;
		}
		if (D && Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(D->position));
		TimedOut(TEXT("at the Derrick"), 15);
		return;
	}
	case 825:
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 6 && U && U->task != ac::Unit::Task::inDerrick)
		{
			if (const std::optional<ac::Structure> D = S->structure(Shots.Derrick)) Stop(Face(D->position));
			Advance();
			return;
		}
		TimedOut(TEXT("pumped once, step 6"), 25);
		return;
	case 826:
		if (Since() > 1.5) Shoot(TEXT("o1-06-make-it-yours"), TEXT("step 6: the Derrick pumped once; the toast, the panel"));
		return;
	case 827:
		Check(OpenSkillScreen() && SkillScreenWidget().IsValid(), TEXT("K opens the skill screen"));
		P5AskedAt = 0;
		Advance();
		return;
	case 828:
	{
		// A point in Quick drill, asked each second until the server has it.
		const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->ranks.count(ac::Skill::quickDrill))
		{
			Advance();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickSkill(ac::Skill::quickDrill);
			W->Learn();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("a point spent"), 15);
		return;
	}
	case 829:
		if (Since() > 1.0) Shoot(TEXT("o1-07-point-spent"), TEXT("a point spent on the skill screen: the chain's last step"));
		return;
	case 830:
		CloseSkillScreen();
		Advance();
		return;
	case 831:
	{
		// The chain over: the daily three, the panel whole a while.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->contracts.size() == 3 && !AcProgressHud::FirstStep(*Mmo) && Since() > 1.5)
		{
			bool bDaily = true;
			for (const ac::net::Contract& C : P->contracts) bDaily &= C.step == 0;
			Check(bDaily, TEXT("the daily three after the chain"));
			const int64 Level = ac::Progression::level(P->xp);
			Check(Level >= 3, FString::Printf(TEXT("level %lld after the first ten minutes (%.0f XP)"), (long long)Level, P->xp));
			Shoot(TEXT("o1-08-dailies"), FString::Printf(TEXT("the daily three take over, level %lld"), (long long)Level));
			return;
		}
		TimedOut(TEXT("the daily contracts"), 15);
		return;
	}
	case 832:
		if (Since() < 1.0) return;
		Check(Shots.Saved + Shots.Skipped == 8, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsRingshadow
	// The sweep from the start pad: the band comes out of the giant's side
	// of the sky (bearing 280°) and runs east over the pilot; the ring's arc
	// crosses the sun (late morning: 17° above the giant's limb).
	case 300:
	case 305:
	case 311:
	{
		// Third person toward the giant (300: low, the land ahead; 305: under
		// the band; 311: back from first person).
		if (T.Phase == 300)
		{
			// The giant's comet impacts are chance (one in 140-340 s, their shock
			// ring over the disc for ~20 s): none over these pictures.
			if (IConsoleVariable* Events = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.GasGiant.Events"))) Events->Set(0);
		}
		if (T.Phase == 311) Pilot->ToggleView();
		const FVector G = FAcGasGiant::Direction();
		Stop(std::atan2(G.Y, G.X));
		Pilot->Eye().Pitch = T.Phase == 305 ? 0.05 : -0.1;
		Advance();
		return;
	}
	case 303:
	case 307:
	{
		// Third person looking up at the sun, the giant's upper limb low in the
		// left of the picture.
		// The usual boom again after rs-02b.
		if (IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"))) Boom->Set(1.f);
		const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky();
		const FVector Sun = AAcDaylight::SunToward(AAcDaylight::HourAt(Sky ? Sky->Time : 0.0));
		Stop(std::atan2(Sun.Y, Sun.X) - 0.15);
		Pilot->Eye().Pitch = FMath::Clamp(std::asin(Sun.Z) - 0.42, 0.05, 0.75);
		Advance();
		return;
	}
	case 301:
		// The radio's toast, a minute before.
		if (const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky())
		{
			const double In = ac::Ringshadow::next(Sky->Time, Sky->DayLength).start - Sky->Time;
			// S1: the clock on to the radio's minute, not a wait for it.
			static bool bAhead = false;
			if (!bAhead && In > ac::Ringshadow::warning - 2.0 && Since() > 0.5 && Mmo)
			{
				bAhead = Mmo->DevClock(FString::Printf(TEXT("ahead %.2f"), In - (ac::Ringshadow::warning - 3.0)));
				T.PhaseAt = Now;
			}
			if (In > 0 && In < ac::Ringshadow::warning - 2.0 && Since() > 1.0)
			{
				Shoot(TEXT("rs-01-before"), FString::Printf(TEXT("before the sweep (in %.0f s), the radio's toast"), In));
				return;
			}
		}
		TimedOut(TEXT("a ringshadow sweep coming (-AcMmoRingshadow=70)"), 240);
		return;
	case 302:
	case 304:
	case 306:
	case 312:
	{
		// The band where the pilot stands: 302 its leading edge coming
		// toward the pilot over the far ground (rs-02, rs-02b); 304 the edge at the pilot (the arc's
		// edge on the sun); 306 deep in it; 312 its trailing edge 6-20 cells past.
		const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky();
		const std::optional<ac::Ringshadow::Band> B =
			Sky && M ? ac::Ringshadow::band(Sky->Time, M->simulation().map->bounds, Sky->DayLength) : std::nullopt;
		// S1: the clock on to 1.2 s before the phase's moment (the band
		// where it wants it), once a phase, instead of waiting for it.
		static int32 JumpedFor = -1;
		auto JumpTo = [&](TFunctionRef<bool(const ac::Ringshadow::Band&, double At)> Moment) {
			if (JumpedFor == T.Phase || !Sky || !M || !U || !Mmo) return;
			JumpedFor = T.Phase;
			for (double Tm = Sky->Time; Tm < Sky->Time + 600.0; Tm += 0.1)
			{
				const std::optional<ac::Ringshadow::Band> Bt = ac::Ringshadow::band(Tm, M->simulation().map->bounds, Sky->DayLength);
				if (!Bt || !Moment(*Bt, ac::dot(U->position, Bt->dir))) continue;
				if (Tm - 1.2 > Sky->Time + 2.0 && Mmo->DevClock(FString::Printf(TEXT("clock %.2f"), Tm - 1.2))) T.PhaseAt = Now;
				return;
			}
		};
		if (T.Phase == 302 && Shots.Saved + Shots.Skipped < 2)
			JumpTo([](const ac::Ringshadow::Band& Bt, const double At) { return At - Bt.lead > 20 && At - Bt.lead < 30; });
		if (T.Phase == 304)
			JumpTo([](const ac::Ringshadow::Band& Bt, const double At) { return At - Bt.lead < 1.5 && At - Bt.lead > -8; });
		if (T.Phase == 306)
			JumpTo([&](const ac::Ringshadow::Band& Bt, double) { return ac::Ringshadow::cover(Bt, U->position) > 0.99; });
		if (T.Phase == 312)
			JumpTo([](const ac::Ringshadow::Band& Bt, const double At) { return Bt.trail - At > 6 && Bt.trail - At < 20; });
		if (B && U)
		{
			const double At = ac::dot(U->position, B->dir);
			const double Cover = ac::Ringshadow::cover(*B, U->position);
			if (T.Phase == 302)
			{
				// Toward the giant, the band's leading edge coming over the berms:
				// rs-02 from the usual boom 20-30 cells off, then rs-02b from a
				// boom 4 times as long (about 13 cells up) 6-19 cells off.
				IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"));
				const bool bHigh = Shots.Saved + Shots.Skipped >= 2;
				if (bHigh && Boom && Boom->GetFloat() < 2.f)
				{
					Boom->Set(4.f);
					T.PhaseAt = Now;
				}
				const FVector G = FAcGasGiant::Direction();
				Stop(std::atan2(G.Y, G.X));
				Pilot->Eye().Pitch = bHigh ? -0.3 : -0.14;
				const double Off = At - B->lead;
				if (!bHigh && Off > 20 && Off < 30 && Since() > 1.0)
				{
					Shoot(TEXT("rs-02-edge-ahead"), FString::Printf(TEXT("toward the giant, the band's edge %.0f cells ahead"), Off));
					T.Phase -= 1;
					return;
				}
				if (bHigh && Off > 6 && Off < 19 && Since() > 1.0)
				{
					Shoot(TEXT("rs-02b-edge-high"), FString::Printf(TEXT("from up high, the band's edge %.0f cells ahead"), Off));
					return;
				}
			}
			if (T.Phase == 304 && At - B->lead < 1.5 && At - B->lead > -8 && Since() > 1.0)
			{
				Shoot(TEXT("rs-03-ring-edge-on-sun"), FString::Printf(TEXT("the band's edge at the pilot (%.1f cells), the ring's edge on the sun"), At - B->lead));
				return;
			}
			if (T.Phase == 306 && Cover > 0.99 && Since() > 1.0)
			{
				Shoot(TEXT("rs-04-under-band"), TEXT("under the band, toward the giant"));
				return;
			}
			if (T.Phase == 312 && B->trail - At > 6 && B->trail - At < 20 && Since() > 1.0)
			{
				Shoot(TEXT("rs-07-after"), FString::Printf(TEXT("after: the band %.0f cells past, east"), B->trail - At));
				return;
			}
		}
		else if (T.Phase == 312 && Sky && Since() > 2.0)
		{
			// The sweep over first: the land in the sun again.
			Shoot(TEXT("rs-07-after"), TEXT("after: the sweep over"));
			return;
		}
		TimedOut(T.Phase == 302 ? TEXT("the band's edge close")
			: T.Phase == 304   ? TEXT("the band's edge at the pilot")
			: T.Phase == 306   ? TEXT("the band over the pilot")
							   : TEXT("the band past"),
			150);
		return;
	}
	case 308:
		if (Since() > 1.5) Shoot(TEXT("rs-05-sun-through-ring"), TEXT("under the band, the sun through the ring"));
		return;
	case 309:
	{
		// First person at the nearest ore, its veins flaring in the shade.
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			if (S->patches[I].remaining <= 0) continue;
			if (Best < 0 || ac::distance(S->patches[I].position, U->position) < ac::distance(S->patches[Best].position, U->position)) Best = I;
		}
		if (Best >= 0) Stop(Face(S->patches[Best].position));
		Pilot->ToggleView();
		Pilot->Eye().Pitch = -0.08;
		Advance();
		return;
	}
	case 310:
		if (Since() > 1.5) Shoot(TEXT("rs-06-under-ore-first"), TEXT("under the band, first person at the nearest ore"));
		return;
	case 313:
	{
		const FVector G = FAcGasGiant::Direction();
		Stop(std::atan2(G.Y, G.X) + UE_DOUBLE_PI);
		Pilot->Eye().Pitch = -0.1;
		Advance();
		return;
	}
	case 314:
		if (Since() > 1.5) Shoot(TEXT("rs-08-after-east"), TEXT("after, looking east: the band going away"));
		return;
	case 315:
		Check(Shots.Saved + Shots.Skipped == 9, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		if (IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"))) Boom->Set(1.f);
		if (IConsoleVariable* Events = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.GasGiant.Events"))) Events->Set(1);
		Finish();
		return;
	// MARK: -AcMmoShotsF2 (and -AcMmoShotsFlight)
	// The Dropship from a Landing Pad by the start pad to one in Cold Fork,
	// its moments on the flight's clock; the frame times through it.
	case 2400:
	{
		// -AcMmoShotsF2Settled (Q2, the first call's stall): the call only
		// once every ground set round the region is built and drawn 3 s.
		bool bSettled = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsF2Settled")))
		{
			const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
			const bool bBuilt = Terrain && Terrain->NumSets() >= 9 && Terrain->NumSetsComplete() == Terrain->NumSets();
			if (!bBuilt) F2SettledAt = -1;
			else if (F2SettledAt < 0) F2SettledAt = Now;
			bSettled = F2SettledAt > 0 && Now - F2SettledAt > 3.0;
			if (!bSettled && Since() > 60.0) TimedOut(TEXT("f2: the ground round the region built"), 60);
		}
		// -AcMmoShotsF2CallAt=S: the call S s after seating (K3: 5 and 30),
		// the steps before it at once.
		double CallAt = -1;
		FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsF2CallAt="), CallAt);
		if (bSettled && Since() > (CallAt >= 0 ? 0.0 : 2.0))
		{
			// A Landing Pad under the pilot, one in Cold Fork (seen), one in
			// Slag Pockets (not seen: the menu leaves it out), MH to fly.
			Check(Mmo->DevClock(TEXT("ferry pad here\nferry pad cold-fork\nferry pad slag-pockets unseen\nferry mh 200")),
				TEXT("f2: the server puts up three Landing Pads and fills the MH"));
			Advance();
		}
		return;
	}
	case 2401:
	{
		const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
		bool bThere = false;
		if (const ac::net::WorldMessage* W = Mmo->World())
			for (const ac::net::WorldMessage::Region& G : W->regions)
				if (G.id == "cold-fork")
					for (const ac::net::WorldMessage::Pad& Q : G.pads) bThere |= Q.building == ac::StructureKind::landingPad;
		if (Near && Near->Within() && bThere && Mmo->HydrogenNow().value_or(0) >= 100 && Since() > 1.0)
		{
			Stop(Pilot->Yaw());
			Check(OpenWorldMap(), TEXT("f2: M opened the flight menu on the Landing Pad"));
			Advance();
			return;
		}
		TimedOut(TEXT("f2: on a Landing Pad, one in Cold Fork"), 15);
		return;
	}
	case 2402:
		if (Since() > 1.0)
		{
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			Check(Map && !Map->PickPad(TEXT("cold-fork"), TEXT("landing")), TEXT("f2: a region file's pad is not flown to"));
			Check(Map && !Map->PickPad(TEXT("slag-pockets"), TEXT("Landing Pad 1")), TEXT("f2: a Landing Pad in a region not seen is not flown to"));
			Check(Map && Map->PickPad(TEXT("cold-fork"), TEXT("Landing Pad 1")), TEXT("f2: picked Cold Fork's Landing Pad"));
			if (Map) Map->ShowWorld();
			Advance();
		}
		return;
	case 2403:
		if (Since() > 1.2) Shoot(TEXT("f2-01-menu"), TEXT("the flight menu: only the Landing Pads in regions been to, Cold Fork picked"));
		return;
	case 2404:
		if (double CallAt = -1; FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsF2CallAt="), CallAt) && Now - SeatedAt < CallAt)
		{
			TimedOut(TEXT("f2: the call's moment"), CallAt + 30);
			return;
		}
		if (const TSharedPtr<SAcWorldMap> Map = WorldMapWidget())
		{
			UE_LOG(LogAutocraft, Log, TEXT("k3: the Dropship called %.2f s after seating"), Now - SeatedAt);
			F2Frames.Reset();
			F2Gpu.Reset();
			F2Stages.Reset();
			F2Last = -1;
			F2Switch = -1;
			F2Prev = Now;
			F2CallWorst = 0;
			F2CallFrame = 0;
			F2CallWorstAt = -1;
			Map->Fly();
			Advance();
			return;
		}
		Check(false, TEXT("f2: the flight menu still open for FLY"));
		Finish();
		return;
	case 2405:
	case 2406:
	case 2407:
	case 2408:
	case 2409:
	case 2410:
	case 2411:
	{
		// Every frame since the call (K3), then the frame times aboard (and
		// the step out).
		if (F2Prev > 0)
		{
			++F2CallFrame;
			if (Now - F2Prev > F2CallWorst)
			{
				F2CallWorst = Now - F2Prev;
				F2CallWorstAt = F2CallFrame;
			}
		}
		F2Prev = Now;
		if (Pilot->InFlight())
		{
			if (F2Last > 0)
			{
				F2Frames.Add((Now - F2Last) * 1000.0);
				F2Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
				F2Stages.Add(uint8(Pilot->FlightStage()));
				if (F2Switch < 0 && Mmo->Ferry() && Mmo->MirrorIndex() == Mmo->Ferry()->ToIndex) F2Switch = F2Frames.Num() - 1;
			}
			F2Last = Now;
		}
		if (!Mmo->FlightRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("f2: flight refused: %s"), *Mmo->FlightRefusal()));
			Finish();
			return;
		}
		const ac::Ferry::Plan* Pl = Pilot->FlightPlan();
		const double Ft = Pilot->FlightSeconds();
		if (!Pl)
		{
			TimedOut(TEXT("f2: the Dropship called"), 10);
			return;
		}
		switch (T.Phase)
		{
		case 2405:
			if (Ft >= Pl->descend * 0.62) Shoot(TEXT("f2-02-descend"), TEXT("the Dropship coming down onto the Landing Pad, the giant beyond"));
			break;
		case 2406:
			if (Ft >= Pl->boardAt() + Pl->board * 0.45) Shoot(TEXT("f2-03-board"), TEXT("the Prospector walking up the ramp"));
			break;
		case 2407:
			if (Ft >= Pl->liftAt() + Pl->lift * 0.75) Shoot(TEXT("f2-04-liftoff"), TEXT("the lift-off, dust racing out under it"));
			break;
		case 2408:
			if (Ft >= Pl->turnsAt() + Pl->turns * 0.3) Shoot(TEXT("f2-05-turn"), TEXT("a banking turn, the ringed giant in view"));
			break;
		case 2409:
			if (Ft >= Pl->dashAt() + Pl->dash * 0.45) Shoot(TEXT("f2-06-dash"), TEXT("the speed burst across the sky, clouds streaking"));
			break;
		case 2410:
			if (Ft >= Pl->approachAt() + Pl->approach * 0.55 && Pilot->FlightStage() == EAcFlightStage::Approach)
				Shoot(TEXT("f2-07-approach"), TEXT("the approach: slowing, coming down onto Cold Fork's Landing Pad"));
			else if (Pilot->FlightStage() == EAcFlightStage::StepOut) Check(false, TEXT("f2: the approach passed before its picture"));
			break;
		default:
			if (Pilot->FlightStage() == EAcFlightStage::StepOut && Ft >= Pl->total() + ac::Ferry::stepOut * 0.5)
				Shoot(TEXT("f2-08-step-out"), TEXT("stepping out at the target pad"));
			break;
		}
		TimedOut(TEXT("f2: the flight's moment"), 40);
		return;
	}
	case 2412:
		if (!Pilot->InFlight() && Seated() && Since() > 0.8)
		{
			// (-AcMmoShotsF2Twice: the second flight, back to the start's pad.)
			const TCHAR* Into = F2Round == 0 ? TEXT("cold-fork") : TEXT("start");
			Check(Here == Into, FString::Printf(TEXT("f2: down in %s"), *Where()));
			Check(Sim->SeamlessChanges() >= 1 && Mmo->LastWasSeamless(), TEXT("f2: the approach's region change was seamless"));
			Check(Mmo->LastTookPrepared(), TEXT("f2: the target region's mirror was made ahead"));
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			Check(Near && Near->Within(), TEXT("f2: standing on the target Landing Pad"));
			// The frame times: the flight's and the region change's.
			auto Pct = [](TArray<double> V, const double P) { V.Sort(); return V.Num() ? V[FMath::Clamp(int32(P * V.Num()), 0, V.Num() - 1)] : 0.0; };
			double Sum = 0, Worst = 0, NearSwitch = 0;
			for (const double F : F2Frames)
			{
				Sum += F;
				Worst = FMath::Max(Worst, F);
			}
			if (F2Switch >= 0)
				for (int32 I = FMath::Max(0, F2Switch - 5); I < FMath::Min(F2Frames.Num(), F2Switch + 6); ++I) NearSwitch = FMath::Max(NearSwitch, F2Frames[I]);
			UE_LOG(LogAutocraft, Log, TEXT("f2 perf (flight %d): %d frames aboard: frame mean %.2f p50 %.2f p95 %.2f p99 %.2f max %.2f ms; round the region change (frame %d) max %.2f ms; gpu p50 %.2f p95 %.2f ms"),
				F2Round + 1, F2Frames.Num(), F2Frames.Num() ? Sum / F2Frames.Num() : 0.0, Pct(F2Frames, 0.5), Pct(F2Frames, 0.95), Pct(F2Frames, 0.99), Worst, F2Switch,
				NearSwitch, Pct(F2Gpu, 0.5), Pct(F2Gpu, 0.95));
			UE_LOG(LogAutocraft, Log, TEXT("f2 perf (flight %d): worst frame since the call %.2f ms (frame %d of %d)"), F2Round + 1, F2CallWorst * 1000.0,
				F2CallWorstAt, F2CallFrame);
			for (int32 St = 1; St <= int32(EAcFlightStage::StepOut); ++St)
			{
				TArray<double> In;
				for (int32 I = 0; I < F2Frames.Num(); ++I)
					if (F2Stages[I] == St) In.Add(F2Frames[I]);
				double Max = 0;
				for (const double F : In) Max = FMath::Max(Max, F);
				UE_LOG(LogAutocraft, Log, TEXT("f2 perf (flight %d): stage %d: %d frames, p50 %.2f p95 %.2f max %.2f ms"), F2Round + 1, St, In.Num(),
					Pct(In, 0.5), Pct(In, 0.95), Max);
			}
			if (F2Round == 0) Shoot(TEXT("f2-09-landed"), TEXT("down on Cold Fork's Landing Pad, the pilot's view back, the Dropship lifting away"));
			else Advance();
		}
		else TimedOut(TEXT("f2: down on Cold Fork's Landing Pad"), 15);
		return;
	case 2413:
		// -AcMmoShotsF2Twice: the same flight's frame times again, back to the
		// start's pad (what the first draw of the ship and its effects cost).
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsF2Twice")) && F2Round == 0)
		{
			if (Since() < 2.0) return;
			F2Round = 1;
			F2Frames.Reset();
			F2Gpu.Reset();
			F2Stages.Reset();
			F2Last = -1;
			F2Switch = -1;
			F2Prev = Now;
			F2CallWorst = 0;
			F2CallFrame = 0;
			F2CallWorstAt = -1;
			FString Why;
			Check(Mmo->Fly(TEXT("start"), TEXT("Landing Pad 1"), &Why), FString::Printf(TEXT("f2: the second flight asked for (%s)"), *Why));
			T.Phase = 2405;
			T.PhaseAt = Now;
			return;
		}
		Check(Shots.Saved + Shots.Skipped == (F2Round ? 16 : 9),
			FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsRig
	// The rig screen on --dev-rig's home: each ask through the screen.
	case 500:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->foundry && P->lab && P->rig.making && Since() > 4.0)
		{
			Check(P->rig.research.has_value(), TEXT("the dev rig: a Foundry, a Lab, a job at each"));
			Advance();
		}
		else TimedOut(TEXT("the dev rig"));
		return;
	case 501:
		if (Since() > 1.5) Shoot(TEXT("rig-01-home"), TEXT("home: where the Prospector unloads at the Citadel"));
		return;
	case 502:
		Check(OpenRigScreen() && RigScreenWidget().IsValid(), TEXT("J opens the rig screen"));
		Check(RigScreenWidget().IsValid() && RigScreenWidget()->GetInfo().FitWhyNot.IsEmpty(), TEXT("the screen has the pilot at home"));
		Advance();
		return;
	case 503:
		if (Since() > 1.5) Shoot(TEXT("rig-02-screen"), TEXT("the rig screen: fitting, the Foundry making, the Lab researching"));
		return;
	case 504:
	{
		// Drill head Mk III from the stores: the screen's own ask.
		const TSharedPtr<SAcRigScreen> W = RigScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (W && P && P->rig.fittedMark(ac::Module::drillHead) == 3)
		{
			Check(P->rig.stock(ac::Module::drillHead, 2) == 1, TEXT("Drill head Mk III fitted, the Mk II back in the stores"));
			// The Foundry is busy: its refusal, as the screen says it.
			W->PickCell({ac::Module::cargoPod, SAcRigScreen::EPanel::Foundry, 2});
			W->Act();
			Check(W->RefusalText().StartsWith(TEXT("The Foundry is busy")), FString::Printf(TEXT("the busy Foundry refuses: %s"), *W->RefusalText()));
			Advance();
			return;
		}
		if (!Mmo->RigRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("fit: %s"), *Mmo->RigRefusal()));
			Finish();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickCell({ac::Module::drillHead, SAcRigScreen::EPanel::Fitting, 3});
			W->Act();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Drill head Mk III fitted"), 10);
		return;
	}
	case 505:
		if (Since() > 1.0) Shoot(TEXT("rig-03-fitted-busy"), TEXT("Drill head Mk III fitted; the busy Foundry's refusal"));
		return;
	case 506:
		Check(OpenSkillScreen() && SkillScreenWidget().IsValid() && !RigScreenWidget().IsValid(), TEXT("K: the skill screen in the rig's place"));
		if (const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget()) W->SetTab(SAcSkillScreen::ETab::Blueprints);
		Advance();
		return;
	case 507:
		if (Since() > 1.0) Shoot(TEXT("rig-04-skill-tabs"), TEXT("the skill screen's tabs: SKILLS, BLUEPRINTS, RIG"));
		return;
	case 508:
		// Tab from BLUEPRINTS: the rig screen again.
		if (const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget()) W->NextTab();
		Check(RigScreenWidget().IsValid() && !SkillScreenWidget().IsValid(), TEXT("Tab on BLUEPRINTS opens the rig screen"));
		Advance();
		return;
	case 509:
		// The Foundry's Plate Mk II comes out (150 s after the pilot came).
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && !P->rig.making && Since() > 1.0)
		{
			Check(P->rig.stock(ac::Module::plate, 2) == 2, TEXT("Plate Mk II made, into the stores"));
			Advance();
		}
		else TimedOut(TEXT("the Foundry's job done"), 200);
		return;
	case 510:
		if (Since() > 2.0) Shoot(TEXT("rig-05-made"), TEXT("the Foundry's job done: the stores up, the Foundry idle"));
		return;
	case 511:
	{
		const TSharedPtr<SAcRigScreen> W = RigScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->rig.making && P->rig.making->module == ac::Module::cargoPod)
		{
			Advance();
			return;
		}
		if (!Mmo->RigRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("make: %s"), *Mmo->RigRefusal()));
			Finish();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickCell({ac::Module::cargoPod, SAcRigScreen::EPanel::Foundry, 2});
			W->Act();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Cargo pod Mk II asked for"), 10);
		return;
	}
	case 512:
		if (Since() > 3.0) Shoot(TEXT("rig-06-making"), TEXT("Cargo pod Mk II at the Foundry: its countdown"));
		return;
	case 513:
		CloseRigScreen();
		Check(Shots.Saved + Shots.Skipped == 6, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsStarfall
	// A starfall near the pilot (the server's --starfall): from the start
	// pad toward the crater's spot, the streak coming in over it.
	case 400:
	case 401:
	case 402:
	case 403:
	case 404:
	case 405:
	case 406:
	case 407:
	case 408:
	case 409:
	case 410:
	case 411:
	case 412:
	case 413:
	case 414:
	{
		const std::optional<ac::net::Sky>& Sky = Mmo ? Mmo->Sky() : std::optional<ac::net::Sky>();
		const ac::net::WorldMessage* Wm = Mmo ? Mmo->World() : nullptr;
		const ac::net::Sky::Starfall* F = nullptr;
		if (Sky && Wm)
			for (const ac::net::Sky::Starfall& X : Sky->starfalls)
				if (X.region == Wm->here) F = &X;
		if (!F || !U)
		{
			TimedOut(TEXT("a starfall told in the pilot's region (-AcMmoStarfall=40)"), 120);
			return;
		}
		const double In = F->impact - Sky->time;
		const double Dark = ac::Rules::dark(Sky->hour());
		// The streak in frame wherever it comes from: its head in the left
		// third (the contracts panel covers the right, the radio's toast the
		// middle), looking up half its height.
		auto AimAtStreak = [&] {
			const ac::Vec3 H = ac::Starfall::head(FMath::Clamp(In, 0.3, ac::Starfall::streak), F->from);
			const ac::Vec2 Head = F->at + ac::Vec2(H.x, H.z);
			Stop(Face(Head) + 0.45);
			Pilot->Eye().Pitch = FMath::Clamp(0.5 * std::atan2(H.y, FMath::Max(ac::distance(Head, U->position), 1.0)), 0.04, 0.35);
		};
		switch (T.Phase)
		{
		case 400:
		{
			// Between the crater and the way the streak comes from: both in view.
			const double ToCrater = Face(F->at);
			const double ToSky = Face(F->at + ac::Starfall::toward(F->from) * 400.0);
			Stop(ToCrater + 0.5 * FMath::UnwindRadians(ToSky - ToCrater));
			Pilot->Eye().Pitch = 0.16;
			Advance();
			return;
		}
		case 401:
			AimAtStreak();
			if (Since() > 1.5 || !Wanted(TEXT("sf-01-told")))
				Shoot(TEXT("sf-01-told"), FString::Printf(TEXT("told: the radio's toast, impact in %.0f s"), In));
			return;
		case 402:
			// None of the fall's own moments wanted: the clock on past the
			// impact and the dust (S1), straight to the crater.
			if (!Wanted(TEXT("sf-02-streak-high")) && !Wanted(TEXT("sf-03-streak-low")) && !Wanted(TEXT("sf-04-impact-flash"))
				&& !Wanted(TEXT("sf-05-dust-ring")))
			{
				if (In > 0 && Mmo->DevClock(FString::Printf(TEXT("ahead %.2f"), In + 12.0)))
				{
					Shots.Skipped += 4;
					T.Phase = 407;
					T.PhaseAt = Now;
				}
				else TimedOut(TEXT("the clock moved past the impact"), 10);
				return;
			}
			AimAtStreak();
			if (In <= 5.0) Shoot(TEXT("sf-02-streak-high"), FString::Printf(TEXT("the streak in the sky, %.1f s to go"), In));
			else TimedOut(TEXT("the streak"), 90);
			return;
		case 403:
			AimAtStreak();
			if (In <= 1.6) Shoot(TEXT("sf-03-streak-low"), FString::Printf(TEXT("the streak low over the land, %.1f s to go"), In));
			return;
		case 404:
			Stop(Face(F->at));
			Pilot->Eye().Pitch = 0.02;
			Advance();
			return;
		case 405:
			if (-In >= 0.12) Shoot(TEXT("sf-04-impact-flash"), FString::Printf(TEXT("the impact's flash, %.2f s after"), -In));
			return;
		case 406:
			if (-In >= 2.5) Shoot(TEXT("sf-05-dust-ring"), TEXT("the dust ring thrown out"));
			return;
		case 407:
		{
			// Up to the crater's edge on foot (25 s at most: rocks may stand
			// in the way), then a look at it.
			static double Halted = -1.0;
			if (ac::distance(F->at, U->position) > 9.0 && Since() < 25.0)
			{
				Pilot->SetTestInput(FVector2D(0.0, -1.0), Face(F->at));
				Halted = -1.0;
				return;
			}
			if (Halted < 0.0) Halted = Now;
			Stop(Face(F->at));
			Pilot->Eye().Pitch = -0.12;
			if ((-In >= 14.0 && Now - Halted > 1.5) || !Wanted(TEXT("sf-06-crater-day")))
				Shoot(TEXT("sf-06-crater-day"), FString::Printf(TEXT("the crater and its hot deposit by day (%.0f cells off, deposit %s)"),
					ac::distance(F->at, U->position), F->patch ? TEXT("landed") : TEXT("missing")));
			return;
		}
		case 408:
			Pilot->ToggleView();
			Pilot->Eye().Pitch = -0.1;
			Advance();
			return;
		case 409:
			if (Since() > 1.5 || !Wanted(TEXT("sf-07-crater-day-first"))) Shoot(TEXT("sf-07-crater-day-first"), TEXT("the crater by day, first person"));
			return;
		case 410:
			// Night: back to third person, the clock on to 19:00 at once (S1:
			// it used to wait three minutes for the dark), or to the end when
			// no night moment is wanted.
			if (!Wanted(TEXT("sf-08-crater-night")) && !Wanted(TEXT("sf-09-crater-night-first")))
			{
				Shots.Skipped += 2;
				T.Phase = 414;
				return;
			}
			Pilot->ToggleView();
			if (Dark < 0.97) Mmo->DevClock(TEXT("hour 19"));
			Advance();
			return;
		case 411:
			// A few seconds for the sky to take the jump and the eye to adapt.
			Pilot->Eye().Pitch = -0.06;
			if ((Dark > 0.97 && Since() > 3.0) || !Wanted(TEXT("sf-08-crater-night"))) Shoot(TEXT("sf-08-crater-night"), FString::Printf(TEXT("the crater at night (hour %.1f)"), Sky->hour()));
			else TimedOut(TEXT("night (the clock moved on to 19:00)"), 30);
			return;
		case 412:
			Pilot->ToggleView();
			Pilot->Eye().Pitch = -0.1;
			Advance();
			return;
		case 413:
			if (Since() > 1.5 || !Wanted(TEXT("sf-09-crater-night-first"))) Shoot(TEXT("sf-09-crater-night-first"), TEXT("the crater at night, first person"));
			return;
		default:
			Check(Shots.Saved + Shots.Skipped == 9, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
			Finish();
			return;
		}
	}
	// MARK: -AcMmoShotsE1
	case 3700:
	case 3701:
	case 3702:
	case 3703:
	case 3704:
	case 3705:
	case 3706:
	case 3707:
	case 3708:
	case 3709:
	{
		const std::optional<ac::net::Sky>& Sky = Mmo ? Mmo->Sky() : std::optional<ac::net::Sky>();
		const ac::net::WorldMessage* Wm = Mmo ? Mmo->World() : nullptr;
		const ac::net::Sky::Starfall* F = nullptr;
		if (Sky && Wm)
			for (const ac::net::Sky::Starfall& X : Sky->starfalls)
				if (X.region == Wm->here) F = &X;
		if (!F || !U)
		{
			TimedOut(TEXT("e1: a starfall told in the pilot's region (-AcMmoStarfall=30)"), 120);
			return;
		}
		const double In = F->impact - Sky->time;
		// Facing the impact from the pad, the streak's side of the sky in
		// the frame's left part (as sf-'s).
		auto AimAtStreak = [&] {
			const ac::Vec3 H = ac::Starfall::head(FMath::Clamp(In, 0.3, ac::Starfall::streak), F->from);
			const ac::Vec2 Head = F->at + ac::Vec2(H.x, H.z);
			Stop(Face(Head) + 0.45);
			Pilot->Eye().Pitch = FMath::Clamp(0.5 * std::atan2(H.y, FMath::Max(ac::distance(Head, U->position), 1.0)), 0.04, 0.35);
		};
		switch (T.Phase)
		{
		case 3700:
			if (Since() < 1.0) return;
			Check(OpenWorldMap(), TEXT("e1: M opened the world map"));
			Advance();
			return;
		case 3701:
			if (Since() > 2.5 || !Wanted(TEXT("e1-01-veins-map")))
				Shoot(TEXT("e1-01-veins-map"), TEXT("the world map: Prospect Landing's two veins and its one well"));
			return;
		case 3702:
			// Toward the fall, to 30 cells (its blast reaches 18), 20 s at most.
			CloseWorldMap();
			if (ac::distance(F->at, U->position) > 30.0 && Since() < 20.0 && In > 8.0)
			{
				Pilot->SetTestInput(FVector2D(0.0, -1.0), Face(F->at));
				return;
			}
			Stop(Face(F->at));
			Pilot->Eye().Pitch = 0.04;
			Advance();
			return;
		case 3703:
			// What stands in its way, put there a few seconds before (so the
			// Prospectors have no time to be moved).
			Stop(Face(F->at));
			if (In > 7.0) return;
			Check(Mmo->DevClock(TEXT("brood clear\nstarfall victims")), TEXT("e1: the realm's buildings and Prospectors in the fall's way"));
			Advance();
			return;
		case 3704:
			Stop(Face(F->at));
			if (Since() > 1.2 || !Wanted(TEXT("e1-02-in-the-way")))
				Shoot(TEXT("e1-02-in-the-way"), FString::Printf(TEXT("what stands where it will fall, %.0f s to go (%.0f cells off)"), In,
					ac::distance(F->at, U->position)));
			return;
		case 3705:
			AimAtStreak();
			if (In <= 2.5 || !Wanted(TEXT("e1-03-streak")))
				Shoot(TEXT("e1-03-streak"), FString::Printf(TEXT("the streak coming in, %.1f s to go"), In));
			else TimedOut(TEXT("e1: the streak"), 60);
			return;
		case 3706:
			Stop(Face(F->at));
			Pilot->Eye().Pitch = 0.08;
			if (-In >= 0.6 || (!Wanted(TEXT("e1-04-impact")) && In <= 0))
				Shoot(TEXT("e1-04-impact"), FString::Printf(TEXT("the impact: the fireball and its flash, %.2f s after"), -In));
			return;
		case 3707:
			Stop(Face(F->at));
			if (-In >= 1.3 || !Wanted(TEXT("e1-05-shockwave")))
				Shoot(TEXT("e1-05-shockwave"), FString::Printf(TEXT("the shockwave racing out, %.1f s after"), -In));
			return;
		case 3708:
		{
			// Closer once the smoke has thinned, looking down over the
			// blast's ground.
			static double Halted = -1.0;
			if (ac::distance(F->at, U->position) > 22.0 && Since() < 20.0)
			{
				Pilot->SetTestInput(FVector2D(0.0, -1.0), Face(F->at));
				Halted = -1.0;
				return;
			}
			if (Halted < 0.0) Halted = Now;
			Stop(Face(F->at));
			Pilot->Eye().Pitch = -0.35;
			if ((-In >= 14.0 && Now - Halted > 1.5) || !Wanted(TEXT("e1-06-crater")))
				Shoot(TEXT("e1-06-crater"), FString::Printf(TEXT("the crater: what it destroyed, what it hurt (%.0f cells off, deposit %s)"),
					ac::distance(F->at, U->position), F->patch ? TEXT("landed") : TEXT("missing")));
			return;
		}
		default:
			Check(Shots.Saved + Shots.Skipped == 6, FString::Printf(TEXT("e1: %d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
			Finish();
			return;
		}
	}
	// MARK: -AcMmoShotsB1
	case 900:
	{
		// The dev rig's home; the nearest ore with some left.
		if (!(S && CitadelOf(*S) && Since() > 3.0))
		{
			TimedOut(TEXT("the dev rig's Citadel"));
			return;
		}
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			if (S->patches[I].remaining <= 0) continue;
			if (Best < 0 || ac::distance(S->patches[I].position, U->position) < ac::distance(S->patches[Best].position, U->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore in the start region"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Patch = Best;
		RouteRound(S->patches[Best].position);
		Advance();
		return;
	}
	case 901:
	case 906:
	case 911:
	{
		// Up to the vein until the act key would drill it, then hold it.
		if (B1ToFirst)
		{
			B1ToFirst = false;
			Pilot->ToggleView();
		}
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1) Route({FVector2D(P.x, P.y)});
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 902:
	case 907:
	case 912:
	{
		auto Sparks = [&] {
			const UAcEffects* Fx = UAcEffects::Get(this);
			const FAcParticles* P = Fx ? AcEffectsBursts::ParticlesOf(Fx) : nullptr;
			return P && Sim ? P->Alive(EAcParticle::WeldSparks, Sim->LastFrame().Time) : -1;
		};
		// Drilling a moment: the sparks fly.
		if (!(U && U->task == ac::Unit::Task::mining && Since() > 1.5))
		{
			TimedOut(TEXT("drilling"), 15);
			return;
		}
		const int32 Alive = Sparks();
		Check(Alive > 0, FString::Printf(TEXT("b1: sparks fly while drilling (%d alive)"), Alive));
		if (T.Phase == 912)
		{
			// Walked off the vein, the key still held: that stops the drill.
			Stop(Face(S->patches[Shots.Patch].position));
			Pilot->SetTestInput(FVector2D(0, 1), Pilot->Yaw());
			B1StoppedAt = Now;
			B1SparksAt = Now;
			T.Phase = 915;
			T.PhaseAt = Now;
			return;
		}
		Shoot(T.Phase == 902 ? TEXT("b1-01-drilling-third") : TEXT("b1-04-drilling-first"),
			FString::Printf(TEXT("drilling, %d sparks alive"), Alive));
		return;
	}
	case 903:
	case 908:
		// The key let go.
		Pilot->Press(false);
		B1StoppedAt = Now;
		B1SparksAt = Now;
		Advance();
		return;
	case 904:
	case 905:
	case 909:
	case 910:
	case 915:
	case 916:
	{
		const UAcEffects* Fx = UAcEffects::Get(this);
		const FAcParticles* P = Fx ? AcEffectsBursts::ParticlesOf(Fx) : nullptr;
		const int32 Alive = P && Sim ? P->Alive(EAcParticle::WeldSparks, Sim->LastFrame().Time) : -1;
		const bool bFirst = T.Phase >= 909;
		const bool bLate = T.Phase == 905 || T.Phase == 910 || T.Phase == 916;
		const double At = bLate ? 1.0 : 0.3;
		if (T.Phase == 915 && Now - B1StoppedAt > 0.4) Stop(Pilot->Yaw());
		if (Now - B1StoppedAt < At) return;
		const TCHAR* What = T.Phase == 904 ? TEXT("b1-02-stopped-third") : T.Phase == 905 ? TEXT("b1-03-stopped-third-1s")
			: T.Phase == 909 ? TEXT("b1-05-stopped-first") : T.Phase == 910 ? TEXT("b1-06-stopped-first-1s")
			: T.Phase == 915 ? TEXT("b1-07-walked-off-first") : nullptr;
		const TCHAR* How = T.Phase >= 915 ? TEXT("walked off the vein") : TEXT("let go of the key");
		if (bLate)
		{
			// Sparks die in 0.35-0.45 s (WeldSparks' life), after the server has the stop: gone by 0.7 s.
			const double Gone = B1SparksAt - B1StoppedAt;
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: b1: %s, %s: sparks gone %.2f s after (task %d, paused clock %.1f s)"),
				bFirst ? TEXT("first person") : TEXT("third person"), How, Gone, U ? int32(U->task) : -1, Sim ? Sim->Clock() - Sim->GameTime() : -1.0);
			Check(Gone < 0.7, FString::Printf(TEXT("b1: %s, %s: the sparks stopped within 0.7 s (%.2f s)"),
				bFirst ? TEXT("first person") : TEXT("third person"), How, Gone));
			if (T.Phase == 916)
			{
				Pilot->Press(false);
				Stop(Pilot->Yaw());
				Advance();
				return;
			}
			// First person next (906), after the picture.
			if (T.Phase == 905) B1ToFirst = true;
		}
		if (T.Phase == 915)
		{
			Shoot(What, FString::Printf(TEXT("%s, 0.3 s on: %d sparks alive"), How, Alive));
			T.Phase = 916;
			return;
		}
		Shoot(What, FString::Printf(TEXT("%s, %.1f s on: %d sparks alive"), How, At, Alive));
		return;
	}
	case 917:
		// Handed back: its AI drives it (top-down), measured, no picture.
		if (Since() < 1.0) return;
		Pilot->Leave();
		B1StoppedAt = -1;
		B1Walk.Reset();
		Advance();
		return;
	case 918:
	{
		const ac::Unit* Told = M ? M->unit(Shots.Unit) : nullptr;
		const ac::Unit* Drawn = nullptr;
		if (Sim)
			for (const ac::Unit& V : Sim->State().units)
				if (V.id == Shots.Unit) Drawn = &V;
		if (Drawn && Told && !Mmo->Driven()) B1Walk.Add({Now, Drawn->position, Told->position});
		if (Since() < 16.0) return;
		double Jitter = 0;
		int32 Frames = 0, Stalls = 0;
		const FString Line = B1WalkStats(B1Walk, Jitter, Frames, Stalls);
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: b1: AI walk: %s"), *Line);
		Check(Frames >= 60, FString::Printf(TEXT("b1: the AI walked it (%d steady frames)"), Frames));
		Check(Stalls == 0 && Jitter < 0.15, FString::Printf(TEXT("b1: the AI's walk drawn smoothly (%s)"), *Line));
		Advance();
		return;
	}
	case 919:
		Check(Shots.Saved + Shots.Skipped == 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsC1
	case 1000:
	{
		// The dev rig's home (the realm's Citadel); the nearest ore with some left.
		if (!(S && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0))
		{
			TimedOut(TEXT("the dev rig's Citadel"));
			return;
		}
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			if (S->patches[I].remaining <= 0) continue;
			if (Best < 0 || ac::distance(S->patches[I].position, U->position) < ac::distance(S->patches[Best].position, U->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore in the start region"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Check(Mmo->Progress()->taxPercent == ac::Progression::realmTaxPercent,
			FString::Printf(TEXT("c1: the tax told: %lld%%"), (long long)Mmo->Progress()->taxPercent));
		Shots.Patch = Best;
		RouteRound(S->patches[Best].position);
		Advance();
		return;
	}
	case 1001:
	{
		// Up to the vein until the act key would drill it, then hold it.
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1) Route({FVector2D(P.x, P.y)});
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 1002:
		// A load: home to the Citadel with it.
		if (U && U->carrying > 0 && U->task != ac::Unit::Task::mining)
		{
			Pilot->Press(false);
			Shots.OreBefore = Ore;
			C1Load = U->carrying;
			C1RealmBefore = Mmo->Progress()->realmWallet.ore;
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 45);
		return;
	case 1003:
	{
		const ac::Structure* C = CitadelOf(*S);
		if (U && U->carrying == 0)
		{
			Stop(Face(C->position));
			// The toast: up at once, held 1.2 s.
			if (Since() > 0.35)
			{
				const int64 Tax = ac::Progression::realmTax(C1Load);
				Check(Ore == Shots.OreBefore + C1Load - Tax, FString::Printf(TEXT("c1: the wallet got the load less the tax: %lld -> %lld (load %lld, tax %lld)"),
					(long long)Shots.OreBefore, (long long)Ore, (long long)C1Load, (long long)Tax));
				Shoot(TEXT("c1-01-unload-tax"), FString::Printf(TEXT("unloaded %lld ore: +%lld, %lld to the realm"), (long long)C1Load,
					(long long)(C1Load - Tax), (long long)Tax));
			}
			return;
		}
		Drive();
		TimedOut(TEXT("unloaded at the Citadel"), 40);
		return;
	}
	case 1004:
		// The server's realm wallet has the cut.
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->realmWallet.ore > C1RealmBefore)
		{
			Check(P->realmWallet.ore - C1RealmBefore == ac::Progression::realmTax(C1Load),
				FString::Printf(TEXT("c1: the realm wallet got the cut: %lld -> %lld ore"), (long long)C1RealmBefore, (long long)P->realmWallet.ore));
			Check(P->taxed.ore >= ac::Progression::realmTax(C1Load), FString::Printf(TEXT("c1: the pilot's tax counted: %lld ore"), (long long)P->taxed.ore));
			Advance();
			return;
		}
		TimedOut(TEXT("the realm wallet's cut"), 10);
		return;
	case 1005:
		Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("c1: H opens the Citadel screen at the Citadel"));
		Check(CitadelScreenWidget().IsValid() && CitadelScreenWidget()->GetInfo().DonateWhyNot.IsEmpty(), TEXT("c1: the screen has the pilot at the Citadel"));
		Advance();
		return;
	case 1006:
		if (Since() > 1.5) Shoot(TEXT("c1-02-citadel-screen"), TEXT("the Citadel screen: the realm wallet, the tax, the donate buttons"));
		return;
	case 1007:
	{
		// 1,000 ore given: the screen's own ask (key 2).
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!W || !P)
		{
			Check(false, TEXT("c1: the Citadel screen stays open"));
			Finish();
			return;
		}
		C1RealmBefore = P->realmWallet.ore;
		C1DonatedBefore = P->donated.ore;
		Shots.OreBefore = Ore;
		Check(W->Key(EKeys::Two), TEXT("c1: key 2 gives 1,000 ore"));
		Advance();
		return;
	}
	case 1008:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->donated.ore >= C1DonatedBefore + 1000 && Since() > 1.0)
		{
			Check(P->realmWallet.ore == C1RealmBefore + 1000, FString::Printf(TEXT("c1: the realm wallet took the gift: %lld -> %lld ore"),
				(long long)C1RealmBefore, (long long)P->realmWallet.ore));
			Check(Ore == Shots.OreBefore - 1000, FString::Printf(TEXT("c1: the wallet gave it: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
			Check(CitadelScreenWidget() && !CitadelScreenWidget()->KeptText().IsEmpty(), TEXT("c1: the foot thanks the pilot"));
			Shoot(TEXT("c1-03-donated"), TEXT("after a donation of 1,000 ore: the realm wallet up"));
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("c1: donate: %s"), *Mmo->CitadelRefusal()));
			Finish();
			return;
		}
		TimedOut(TEXT("the donation kept"), 10);
		return;
	case 1009:
		CloseCitadelScreen();
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsC2
	case 1100:
		// The dev rig's home: the Prospector where it unloads at a Citadel.
		if (S && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0)
		{
			Check(AtRealmCitadel(), TEXT("c2: the pilot stands at a Citadel"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("c2: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget())
			{
				Check(W->Key(EKeys::Tab) && W->Tab() == SAcCitadelScreen::ETab::Shop, TEXT("c2: Tab shows the SHOP"));
			}
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel"));
		return;
	case 1101:
	{
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!W || !P || Since() < 1.0) return;
		const FAcCitadelScreenInfo& I = W->GetInfo();
		Check(I.Blueprints.Num() == int32(ac::Progression::blueprintKinds().size()) && I.Items.Num() == 4,
			FString::Printf(TEXT("c2: the shop lists %d blueprints and %d supplies"), I.Blueprints.Num(), I.Items.Num()));
		Check(I.ShopWhyNot.IsEmpty(), TEXT("c2: the shop sells here"));
		Shots.OreBefore = Ore;
		C2Hydrogen = Hydrogen;
		// C2 (docs/guns-and-practice.md §4): the shop sells each blueprint
		// once, at its first copy's price, and none the pilot knows or holds.
		C2Kind.Reset();
		const TArray<ac::StructureKind> Kinds(ac::Progression::blueprintKinds().data(), int32(ac::Progression::blueprintKinds().size()));
		for (int32 X = 0; X < Kinds.Num() && !C2Kind; ++X)
			if (!P->learned.count(Kinds[X]) && !P->blueprints.count(Kinds[X])) C2Kind = Kinds[X];
		for (int32 X = 0; X < Kinds.Num(); ++X)
		{
			if (!P->learned.count(Kinds[X])) continue;
			Check(I.Blueprints[X].bKnown && W->BuyWhyNot(false, X).StartsWith(TEXT("You know the")),
				FString::Printf(TEXT("c2: the shop won't sell a known %s (\"%s\")"), *I.Blueprints[X].Name, *W->BuyWhyNot(false, X)));
			break;
		}
		C2Price = C2Kind ? ac::Shop::blueprintPrice(*C2Kind, 0) : ac::ShopPrice();
		// That blueprint (its key on the SHOP tab: its line's number), a
		// repair kit, a shield charge.
		static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight};
		if (C2Kind)
		{
			const int32 Line = Kinds.IndexOfByKey(*C2Kind);
			if (Line < 8) Check(W->Key(Digits[Line]), FString::Printf(TEXT("c2: key %d buys a %s blueprint"), Line + 1, *I.Blueprints[Line].Name));
			else W->Buy(false, Line);
		}
		else UE_LOG(LogAutocraft, Display, TEXT("flowtest: c2: the pilot knows every blueprint: none bought"));
		Mmo->BuyItem(ac::Item::repairKit);
		W->Buy(true, int32(ac::Item::shieldCharge));
		Advance();
		return;
	}
	case 1102:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		auto Has = [&](ac::Item I) { return P && P->items.count(I) && P->items.at(I) > 0; };
		const bool bBlueprint = !C2Kind || (P && P->blueprints.count(*C2Kind) && P->blueprints.at(*C2Kind) == 1);
		if (P && Has(ac::Item::repairKit) && Has(ac::Item::shieldCharge) && bBlueprint && Mmo->CitadelKept() >= (C2Kind ? 3 : 2)
			&& Since() > 1.0)
		{
			const ac::ItemInfo& Kit = ac::Shop::item(ac::Item::repairKit);
			const ac::ItemInfo& Shield = ac::Shop::item(ac::Item::shieldCharge);
			Check(Ore == Shots.OreBefore - C2Price.ore - Kit.ore - Shield.ore,
				FString::Printf(TEXT("c2: the wallet paid in ore: %lld -> %lld"), (long long)Shots.OreBefore, (long long)Ore));
			Check(Hydrogen == C2Hydrogen - C2Price.hydrogen - Kit.hydrogen - Shield.hydrogen,
				FString::Printf(TEXT("c2: the wallet paid in MH: %lld -> %lld"), (long long)C2Hydrogen, (long long)Hydrogen));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget(); W && C2Kind)
			{
				const auto& Kinds = ac::Progression::blueprintKinds();
				const int32 Line = int32(std::find(Kinds.begin(), Kinds.end(), *C2Kind) - Kinds.begin());
				Check(W->BuyWhyNot(false, Line).StartsWith(TEXT("You hold a")),
					FString::Printf(TEXT("c2: the shop won't sell a second copy (\"%s\")"), *W->BuyWhyNot(false, Line)));
			}
			Check(CitadelScreenWidget() && !CitadelScreenWidget()->KeptText().IsEmpty(), TEXT("c2: the foot says what was bought"));
			Shoot(TEXT("c2-01-shop"), TEXT("the SHOP tab after buying a Hab Dome blueprint, a repair kit and a shield charge"));
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("c2: buy: %s"), *Mmo->CitadelRefusal()));
			Finish();
			return;
		}
		TimedOut(TEXT("the purchases kept"), 10);
		return;
	}
	case 1103:
	{
		CloseCitadelScreen();
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("c2: B opens the build menu"));
		if (C2Kind)
		{
			Check(AcProgressHud::CanLearn(Mmo, *C2Kind),
				FString::Printf(TEXT("c2: the wheel offers the bought %s to learn"), *AcPilotText::Title(*C2Kind)));
		}
		Advance();
		return;
	}
	case 1104:
		if (Since() > 1.0) Shoot(TEXT("c2-02-build-menu"), TEXT("the build wheel: every blueprint, the bought one to LEARN"));
		return;
	case 1105:
	{
		if (UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this)) Build->Key(TEXT("esc"));
		// Turned away from the Citadel's wall: the field, then the shield (key 3's ask).
		Stop(Face(U->position * 2.0 - CitadelOf(*S)->position));
		Check(Mmo->UseItem(ac::Item::shieldCharge), TEXT("c2: key 3 asks to use the shield charge"));
		C2Used = Mmo->ItemsUsed();
		Advance();
		return;
	}
	case 1106:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->shieldUntil > 0 && Since() > 1.0)
		{
			Check(!P->items.count(ac::Item::shieldCharge), TEXT("c2: the shield charge's stack is used up"));
			Check(Mmo->ItemsUsed() > C2Used, TEXT("c2: the server kept the use"));
			Check(P->itemReady.count(ac::Item::shieldCharge) == 1, TEXT("c2: the shield charge cools down"));
			Shoot(TEXT("c2-03-shield-used"), TEXT("the shield charge running: its stack lit by the slots"));
			return;
		}
		TimedOut(TEXT("the shield charge used"), 10);
		return;
	case 1107:
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsS2A, -AcMmoShotsS2B
	case 1400:
	{
		if (Since() < 3.0 || !Mmo->Sky()) return;
		const std::optional<ac::net::RunFile> Run = FAcMmoClient::RunningServer(Mmo->SavePath);
		Check(Mmo->Detached() && !Mmo->Reattached() && Run.has_value(),
			FString::Printf(TEXT("s2: the first run started a detached world server (pid %d, port %d)"), Run ? Run->pid : 0, Run ? (int32)Run->port : 0));
		const double Clock = Mmo->Sky()->time;
		const int64 Wall = FDateTime::UtcNow().ToUnixTimestamp();
		Check(FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.3f %lld %d\n"), Clock, (long long)Wall, Run ? Run->pid : 0),
				  *FPaths::Combine(Shots.Dir, TEXT("s2-first.txt"))),
			FString::Printf(TEXT("s2: the world's clock %.1f s at %lld noted"), Clock, (long long)Wall));
		Shoot(TEXT("s2-01-first-run"), TEXT("the first run in the world, its server detached"));
		return;
	}
	case 1401:
		Check(Shots.Saved + Shots.Skipped == 1, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: s2: the first run ends; its world server runs on"));
		Finish();
		return;
	case 1410:
	{
		if (Since() < 3.0 || !Mmo->Sky()) return;
		const std::optional<ac::net::RunFile> Run = FAcMmoClient::RunningServer(Mmo->SavePath);
		FString First;
		TArray<FString> Parts;
		if (FFileHelper::LoadFileToString(First, *FPaths::Combine(Shots.Dir, TEXT("s2-first.txt")))) First.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(" "));
		Check(Parts.Num() == 3, TEXT("s2: the first run's note read"));
		const double Clock = Mmo->Sky()->time;
		const double Then = Parts.Num() == 3 ? FCString::Atod(*Parts[0]) : 0;
		const int64 Wall = FDateTime::UtcNow().ToUnixTimestamp();
		const double Between = Parts.Num() == 3 ? double(Wall - FCString::Atoi64(*Parts[1])) : 0;
		const int32 FirstPid = Parts.Num() == 3 ? FCString::Atoi(*Parts[2]) : -1;
		Check(Mmo->Reattached() && Run && Run->pid == FirstPid,
			FString::Printf(TEXT("s2: the second run reattached to the first run's server (pid %d, the first's %d)"), Run ? Run->pid : 0, FirstPid));
		Check(Clock - Then > Between - 6 && Clock - Then < Between + 6,
			FString::Printf(TEXT("s2: the world's clock ran on between the runs: %.1f s to %.1f s (%.1f s on, %.0f s between)"), Then, Clock,
				Clock - Then, Between));
		Shoot(TEXT("s2-02-reattached"), TEXT("the second run, reattached: the world went on"));
		return;
	}
	case 1411:
		// QUIT TO HOME: the server runs on; the home screen says so.
		StopMmo();
		OpenMenu(EAcMenuMode::Home);
		Advance();
		return;
	case 1412:
		if (Since() < 2.0) return;
		Check(MenuWidget() && MenuWidget()->ServerText().StartsWith(TEXT("World server running")),
			FString::Printf(TEXT("s2: home shows \"%s\""), MenuWidget() ? *MenuWidget()->ServerText() : TEXT("no menu")));
		Shoot(TEXT("s2-03-home-server-running"), TEXT("home: the world server running, STOP"));
		return;
	case 1413:
		Check(MenuWidget() && MenuWidget()->Highlight(EAcMenuAction::StopServer), TEXT("s2: STOP is on the home screen"));
		OnMenuPick(EAcMenuAction::StopServer);
		Check(!FAcMmoClient::RunningServer().has_value(), TEXT("s2: STOP saved and stopped the world server"));
		Advance();
		return;
	case 1414:
		if (Since() < 1.5) return;
		Check(MenuWidget() && MenuWidget()->ServerText().IsEmpty(), TEXT("s2: the line is gone"));
		Shoot(TEXT("s2-04-home-stopped"), TEXT("home after STOP"));
		return;
	case 1415:
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsH2, -AcMmoShotsH2Away
	case 1500:
		// The dev rig's home: the Prospector where it unloads at a Citadel.
		if (S && CitadelOf(*S) && Mmo->Progress() && !Mmo->Progress()->mercFields.empty() && Since() > 3.0)
		{
			Check(AtRealmCitadel(), TEXT("h2: the pilot stands at a Citadel"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("h2: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget())
			{
				for (int32 K = 0; K < SAcCitadelScreen::TabCount && W->Tab() != SAcCitadelScreen::ETab::Mercs; ++K) W->Key(EKeys::Tab);
				Check(W->Tab() == SAcCitadelScreen::ETab::Mercs, TEXT("h2: Tab reaches the MERCS"));
			}
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel and the mercenaries' fields"));
		return;
	case 1501:
	{
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		if (!W || Since() < 1.0) return;
		const FAcCitadelScreenInfo& I = W->GetInfo();
		Check(I.Mercs.IsEmpty() && I.MercLimit >= 1 && !I.MercFields.IsEmpty(),
			FString::Printf(TEXT("h2: MERCS: none hired, %lld at level %lld, %d fields"), (long long)I.MercLimit, (long long)I.Level, I.MercFields.Num()));
		Check(W->HireWhyNot().IsEmpty(), FString::Printf(TEXT("h2: HIRE lit (%s)"), *W->HireWhyNot()));
		H2Kept = Mmo->CitadelKept();
		Shoot(TEXT("h2-01-mercs-tab"), TEXT("the MERCS tab: the fee, the wage, the fields"));
		return;
	}
	case 1502:
		if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->Hire();
		Advance();
		return;
	case 1503:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->mercs.size() == 1 && P->mercs[0].unit > 0 && Mmo->CitadelKept() > H2Kept && Since() > 1.0)
		{
			H2Unit = P->mercs[0].unit;
			Check(CitadelScreenWidget() && CitadelScreenWidget()->KeptText().StartsWith(TEXT("Hired")), TEXT("h2: the foot says hired"));
			Check(P->mercTags.count(H2Unit) == 1, FString::Printf(TEXT("h2: %s tagged here"), *Utf(P->mercs[0].name)));
			Shoot(TEXT("h2-02-hired"), TEXT("hired: the mercenary listed, its field"));
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("h2: hire: %s"), *Mmo->CitadelRefusal()));
			Finish();
			return;
		}
		TimedOut(TEXT("the hire kept"), 10);
		return;
	}
	case 1504:
		CloseCitadelScreen();
		Shots.Route.Reset();
		Shots.RouteAt = 0;
		Advance();
		return;
	case 1505:
	{
		// It walks to its field and drills: round the Citadel to the field's
		// side (the Citadel stands between them), past it, and a look back.
		const ac::Unit* Merc = M ? M->unit(H2Unit) : nullptr;
		if (Merc && Merc->task == ac::Unit::Task::mining && U && S && CitadelOf(*S))
		{
			if (Shots.Route.IsEmpty())
			{
				const ac::Vec2 C = CitadelOf(*S)->position;
				const ac::Vec2 D = ac::normalize(Merc->position - C);
				const ac::Vec2 Side(-D.y, D.x);
				const double R = ac::Rules::radius(ac::StructureKind::citadel) + 4.5;
				const double Turn = ac::dot(U->position - C, Side) >= 0 ? 1.0 : -1.0;
				const ac::Vec2 A = C + Side * (R * Turn);
				const ac::Vec2 B = Merc->position + Side * (6.0 * Turn) + D * 2.0;
				Route({FVector2D(A.x, A.y), FVector2D(B.x, B.y)});
			}
			const int32 Got = Drive();
			if (Got == 1 || ac::distance(U->position, Merc->position) < 7)
			{
				Stop(Face(Merc->position));
				Advance();
				return;
			}
			if (Got < 0) Stalled(TEXT("round the Citadel to the mercenary"));
			return;
		}
		TimedOut(TEXT("the mercenary drilling"), 90);
		return;
	}
	case 1506:
	{
		const ac::Unit* Merc = M ? M->unit(H2Unit) : nullptr;
		if (!Merc || Since() < 2.0) return;
		const ac::net::ProgressMessage* P = Mmo->Progress();
		Check(P && P->mercTags.count(H2Unit) == 1 && Merc->task == ac::Unit::Task::mining, TEXT("h2: the mercenary drills at its field, tagged"));
		Check(FFileHelper::SaveStringToFile(FString::Printf(TEXT("%lld\n"), (long long)(P && !P->mercs.empty() ? P->mercs[0].delivered : 0)),
				  *FPaths::Combine(Shots.Dir, TEXT("h2-first.txt"))),
			TEXT("h2: what it delivered noted"));
		Shoot(TEXT("h2-03-merc-mining"), TEXT("the mercenary drilling at its field: its orange, its tag"));
		return;
	}
	case 1507:
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: h2: the first run ends; the server saves and stops with it"));
		Finish();
		return;
	case 1510:
	{
		// Seated again: the news came with the first progress.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
		const int64 Told = Hud ? Hud->AwayToasted() : 0;
		// While its toast is up (4.5 s), past its fade-in.
		const double Age = Hud && Told > 0 ? FPlatformTime::Seconds() - Hud->AwayToastedAt() : -1;
		if (!P || (Told <= 0 && Since() < 6.0) || (Told > 0 && Age < 0.6)) return;
		Check(Age < 3.8, FString::Printf(TEXT("h2: the toast still up (%.1f s old)"), Age));
		FString First;
		FFileHelper::LoadFileToString(First, *FPaths::Combine(Shots.Dir, TEXT("h2-first.txt")));
		const int64 Before = FCString::Atoi64(*First.TrimStartAndEnd());
		const int64 After = P->mercs.empty() ? 0 : P->mercs[0].delivered;
		Check(P->mercs.size() == 1 && After > Before,
			FString::Printf(TEXT("h2: its mercenary delivered %lld ore while away (%lld before, %lld now)"), (long long)(After - Before),
				(long long)Before, (long long)After));
		Check(Told > 0, FString::Printf(TEXT("h2: \"while you were away\" told (%lld ore)"), (long long)Told));
		Shoot(TEXT("h2-04-while-away"), TEXT("the next start: WHILE YOU WERE AWAY, the ore its mercenary delivered"));
		return;
	}
	case 1511:
		Check(Shots.Saved + Shots.Skipped == 1, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsM2
	case 1700:
	{
		// The dev rig's home: hire a mercenary, take a mission here.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!(S && CitadelOf(*S) && P && !P->mercFields.empty() && !P->board.empty() && U && Since() > 3.0))
		{
			TimedOut(TEXT("the dev rig's Citadel, the fields and the board"));
			return;
		}
		Check(P->regionsSeen.count("cold-fork") == 1 && P->regionsSeen.count("start") == 1,
			FString::Printf(TEXT("m2: %d regions been to"), int32(P->regionsSeen.size())));
		ac::net::RequestMessage::Merc Hire;
		FString Why;
		Check(Mmo->Merc(Hire, &Why), FString::Printf(TEXT("m2: hire asked (%s)"), *Why));
		int32 Best = -1;
		double Near = 1e18;
		for (int32 X = 0; X < int32(P->board.size()); ++X)
		{
			const ac::net::CitadelMission& O = P->board[size_t(X)];
			// With a place on the map, in a region been to; a point here first.
			const bool bPoint = O.kind == ac::net::CitadelMission::Kind::survey || O.kind == ac::net::CitadelMission::Kind::hold;
			if (O.region.empty() || !P->regionsSeen.count(O.region)) continue;
			const double D = ac::distance(O.at, U->position) + (Utf(O.region) == Here ? 0 : 1e9) + (bPoint ? 0 : 1e12);
			if (D < Near)
			{
				Near = D;
				Best = X;
			}
		}
		FString Board;
		for (const ac::net::CitadelMission& O : P->board)
		{
			Board += FString::Printf(TEXT(" %s/%d@%s"), *AcMissions::KindName(O.kind), int32(O.tier), *Utf(O.region));
		}
		Check(Best >= 0, FString::Printf(TEXT("m2: a survey or hold offer to take (board:%s)"), *Board));
		if (Best >= 0)
		{
			M2Mission = P->board[size_t(Best)].id;
			Check(Mmo->TakeMission(M2Mission, &Why), FString::Printf(TEXT("m2: mission asked (%s)"), *Why));
		}
		Advance();
		return;
	}
	case 1701:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		bool bTaken = false;
		if (P)
			for (const ac::net::CitadelMission& X : P->journal) bTaken |= X.id == M2Mission && X.state == ac::net::CitadelMission::State::taken;
		if (P && P->mercs.size() == 1 && P->mercs[0].unit > 0 && bTaken)
		{
			M2Unit = P->mercs[0].unit;
			Advance();
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("m2: %s"), *Mmo->CitadelRefusal()));
			Finish();
			return;
		}
		TimedOut(TEXT("the mercenary hired and the mission taken"), 10);
		return;
	}
	case 1702:
	{
		// It walks to its field and drills; then M.
		const ac::Unit* Merc = M ? M->unit(M2Unit) : nullptr;
		if (Merc && Merc->task == ac::Unit::Task::mining)
		{
			Check(OpenWorldMap() && WorldMapWidget().IsValid(), TEXT("m2: M opened the world map"));
			if (const TSharedPtr<SAcWorldMap> Map = WorldMapWidget()) Map->ShowWorld();
			Advance();
			return;
		}
		TimedOut(TEXT("the mercenary drilling"), 90);
		return;
	}
	case 1703:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map || Since() < 1.5) return;
		Check(Map->PicturesShown() == 5, FString::Printf(TEXT("m2: %d regions drawn from their pictures (5 been to)"), Map->PicturesShown()));
		Shoot(TEXT("m2-01-world"), TEXT("the world map at world zoom: the regions been to, the rest uncharted"));
		return;
	}
	case 1704:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map) return;
		if (!M2Zoomed)
		{
			if (Clipping && Since() < 2.0) return;  // the world a moment first
			M2Zoomed = true;
			// A clip glides there slowly (the showreel); a picture jumps.
			Check(Clipping ? Map->GlideToRegion(TEXT("start"), 1.0, 4.5) : Map->ShowRegion(TEXT("start")), TEXT("m2: zoomed into Prospect Landing"));
			return;
		}
		if (Since() < (Clipping ? 5.5 : 1.0)) return;
		Shoot(TEXT("m2-02-prospect-landing"), TEXT("Prospect Landing: the pilot, the mercenary, the Citadel, the deposits, the pad, the mission"));
		return;
	}
	case 1705:
	case 1706:
	case 1707:
	case 1708:
	{
		// A tooltip each: a deposit with ore, the Citadel, the mission, the mercenary.
		namespace atlas = ac::net::atlas;
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map) return;
		const int32 Which = T.Phase - 1705;
		if (M2Hovered != T.Phase)
		{
			M2Hovered = T.Phase;
			bool bHover = false;
			if (Which == 0)
			{
				// A deposit with most of its ore, near the mercenary (not its own).
				const ac::Unit* Merc = M ? M->unit(M2Unit) : nullptr;
				const std::vector<atlas::Place> Places = Mmo->World() ? atlas::layout(*Mmo->World(), {}) : std::vector<atlas::Place>{};
				const atlas::Place* Mine = Mmo->World() ? atlas::placeOf(Places, Mmo->World()->here) : nullptr;
				const ac::Vec2 By = Merc && Mine ? Mine->fromGround(Merc->position) : ac::Vec2(0, 0);
				double Best = 1e9;
				int64 Ref = -1;
				for (const atlas::Item& It : Map->GetInfo().Items)
				{
					const double D = ac::distance(It.at, By);
					if (It.kind == atlas::Kind::deposit && It.size > 0.5 && !It.flag && D > 3 && D < Best)
					{
						Best = D;
						Ref = It.ref;
					}
				}
				bHover = Map->HoverWhere([Ref](const atlas::Item& It) { return It.kind == atlas::Kind::deposit && It.ref == Ref; });
			}
			else if (Which == 1)
			{
				bHover = Map->HoverWhere([](const atlas::Item& It) { return It.kind == atlas::Kind::building && It.building == ac::StructureKind::citadel; });
			}
			else if (Which == 2)
			{
				const int64 Id = int64(M2Mission);
				bHover = Map->HoverWhere([Id](const atlas::Item& It) { return It.kind == atlas::Kind::mission && It.ref == Id; });
			}
			else
			{
				const int64 Unit = M2Unit;
				bHover = Map->HoverWhere([Unit](const atlas::Item& It) { return It.kind == atlas::Kind::merc && It.ref == Unit; });
			}
			Check(bHover, FString::Printf(TEXT("m2: hovering %s"), Which == 0 ? TEXT("a deposit") : Which == 1 ? TEXT("the Citadel") : Which == 2 ? TEXT("the mission") : TEXT("the mercenary")));
			// Closer in: the item a little left of the middle, its tooltip beside it.
			if (const atlas::Item* It = Map->Hovered()) Map->SetView(It->at + ac::Vec2(8, 3), 13);
			return;
		}
		if (Since() < 1.0) return;
		const FString Tip = Map->TooltipText();
		const FString Flat = Tip.Replace(TEXT("\n"), TEXT(" / "));
		if (Which == 0)
		{
			Check(Tip.StartsWith(TEXT("Ore deposit")) && Tip.Contains(TEXT("ore left")), FString::Printf(TEXT("m2: the deposit's tooltip: %s"), *Flat));
			Shoot(TEXT("m2-03-tooltip-deposit"), TEXT("hovering a deposit: its ore left"));
		}
		else if (Which == 1)
		{
			Check(Tip.StartsWith(TEXT("Citadel")) && Tip.Contains(TEXT("Built by")) && Tip.Contains(TEXT("HP ")), FString::Printf(TEXT("m2: the Citadel's tooltip: %s"), *Flat));
			Shoot(TEXT("m2-04-tooltip-building"), TEXT("hovering the Citadel: built by, hp"));
		}
		else if (Which == 2)
		{
			Check(Tip.Contains(TEXT("mission ·")), FString::Printf(TEXT("m2: the mission's tooltip: %s"), *Flat));
			// A click on it: the compass's target.
			if (const atlas::Item* It = Map->Hovered()) TargetFromMap(*It);
			Check(Mmo->GetMapTarget() && Mmo->GetMapTarget()->Key == FString::Printf(TEXT("mission:%u"), M2Mission),
				TEXT("m2: the mission is the compass's target"));
			Shoot(TEXT("m2-05-tooltip-mission"), TEXT("hovering the mission: title, tier, kind"));
		}
		else
		{
			Check(Tip.StartsWith(TEXT("MERC · ")) && Tip.Contains(TEXT("ore delivered")), FString::Printf(TEXT("m2: the mercenary's tooltip: %s"), *Flat));
			Shoot(TEXT("m2-06-tooltip-merc"), TEXT("hovering the mercenary: name, field, delivered"));
		}
		return;
	}
	case 1709:
		CloseWorldMap();
		Shots.Route.Reset();
		Shots.RouteAt = 0;
		Advance();
		return;
	case 1710:
	{
		// To the mercenary: round the Citadel to the field's side, and a look.
		const ac::Unit* Merc = M ? M->unit(M2Unit) : nullptr;
		if (Merc && U && S && CitadelOf(*S))
		{
			if (Shots.Route.IsEmpty())
			{
				const ac::Vec2 C = CitadelOf(*S)->position;
				const ac::Vec2 D = ac::normalize(Merc->position - C);
				const ac::Vec2 Side(-D.y, D.x);
				const double R = ac::Rules::radius(ac::StructureKind::citadel) + 4.5;
				const double Turn = ac::dot(U->position - C, Side) >= 0 ? 1.0 : -1.0;
				const ac::Vec2 A = C + Side * (R * Turn);
				const ac::Vec2 B = Merc->position + Side * (6.0 * Turn) + D * 2.0;
				Route({FVector2D(A.x, A.y), FVector2D(B.x, B.y)});
			}
			const int32 Got = Drive();
			if (Got == 1 || ac::distance(U->position, Merc->position) < 7)
			{
				Stop(Face(Merc->position));
				Advance();
				return;
			}
			if (Got < 0) Stalled(TEXT("round the Citadel to the mercenary"));
			return;
		}
		TimedOut(TEXT("the mercenary in view"), 60);
		return;
	}
	case 1711:
		if (Since() < 2.0) return;
		Shoot(TEXT("m2-07-merc-orange"), TEXT("the mercenary drilling in the world: orange, outlined"));
		return;
	case 1712:
		Check(Shots.Saved + Shots.Skipped == 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsW4
	case 2000:
	{
		// Every ground set round Prospect Landing built (the eight
		// neighbours' too), the minimap's pictures, a moment for the light.
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (!(U && Mmo->World() && Sets >= 9 && Ready == Sets && Since() > 3.0))
		{
			TimedOut(TEXT("w4: the nine regions' ground built"), 120);
			return;
		}
		UE_LOG(LogAutocraft, Display, TEXT("w4: %d ground sets ready in %.1f s"), Sets, Since());
		// The regions in the grid's order, north-west first.
		W4Order.Reset();
		const ac::net::WorldMessage& W = *Mmo->World();
		for (int64 Row = 0; Row < W.rows; ++Row)
			for (int64 Col = 0; Col < W.columns; ++Col)
				for (size_t I = 0; I < W.regions.size(); ++I)
					if (W.regions[I].column == Col && W.regions[I].row == Row) W4Order.Add(int64(I));
		W4Next = 0;
		W4Set = false;
		// The free camera's pictures without the pilot's distance veil (it
		// is the pilot's view, far from the free camera).
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(0, ECVF_SetByCode);
		Advance();
		return;
	}
	case 2001:
	{
		// Each region's palette from above: a free camera south of its middle.
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		if (W4Next >= W4Order.Num() || !Pc || !Terrain)
		{
			W4Next = 0;
			W4Set = false;
			T.Phase = 2010;
			T.PhaseAt = Now;
			return;
		}
		const ac::net::WorldMessage& W = *Mmo->World();
		const std::vector<ac::net::atlas::Place> Places = ac::net::atlas::layout(W, {});
		const ac::net::atlas::Place* There = ac::net::atlas::placeOf(Places, W4Order[W4Next]);
		if (!There) return;
		const ac::Vec2 Mid = There->centre() - Terrain->CurrentCentre();
		if (!W4Set)
		{
			if (!W4Cam.IsValid())
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
				W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
				W4Cam->GetCameraComponent()->SetFieldOfView(70.f);
			}
			const FVector Eye = AcSpace::ToWorld(Mid + ac::Vec2(0, 62), 72);
			const FVector At = AcSpace::ToWorld(Mid, 2);
			W4Cam->SetActorLocationAndRotation(Eye, (At - Eye).Rotation());
			W4Set = true;
			W4ShotAt = Now;
		}
		if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		if (Now - W4ShotAt < 2.0) return;
		const FString Id = Utf(W.regions[size_t(W4Order[W4Next])].id);
		W4Next += 1;
		W4Set = false;
		ShootClean(*FString::Printf(TEXT("w4-p-%s"), *Id), FString::Printf(TEXT("%s's ground from above: its palette, its walls"), *Id));
		T.Phase = 2001;
		return;
	}
	case 2010:
	case 2011:
	case 2012:
	case 2013:
	case 2014:
	case 2015:
	{
		// The free camera at the border and the passage, Prospect Landing's
		// cells (past 144: Tern Ridge's ground beyond its east edge); then
		// (W4b) a bay of land among the walls and a stretch of wall.
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		if (!Pc || !Terrain || !W4Cam.IsValid()) return;
		if (!W4Set)
		{
			// The north wall: the outline's corner furthest north between x 16
			// and 52 (west of the north passage).
			ac::Vec2 Wall(34, 10);
			// The bay: the outline's corner nearest the cell's edge away from
			// the passages, the land reaching in among the walls; the wall
			// walked along: a dozen corners of the outline round the north-west.
			ac::Vec2 Bay(20, 80), AlongFrom(30, 20), AlongTo(50, 16);
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("server/regions/start.json"))))
			{
				const ac::region::Parsed P = ac::region::parse(TCHAR_TO_UTF8(*Text));
				if (P.file && P.file->border)
				{
					const std::vector<ac::Vec2>& O = P.file->border->outline;
					double Best = 1e9, Edge = 1e9, Far = -1;
					for (const ac::Vec2 Q : O)
						if (Q.x > 16 && Q.x < 52 && Q.y > 2 && Q.y < Best) Best = Q.y, Wall = Q;
					const auto NearPassage = [&](const ac::Vec2 Q) {
						for (size_t E = 0; E < 4; E++)
							for (const ac::region::Span& Sp : P.file->openings[E])
							{
								const double Along = E % 2 == 0 ? Q.x : Q.y;
								if (Along > Sp.from - 24 && Along < Sp.to + 24) return true;
							}
						return false;
					};
					for (const ac::Vec2 Q : O)
					{
						const double D = FMath::Min(FMath::Min(Q.x, 144.0 - Q.x), FMath::Min(Q.y, 96.0 - Q.y));
						if (D > 3 && D < Edge && !NearPassage(Q)) Edge = D, Bay = Q;
					}
					// Along: the corner deepest in from the north-west quarter's
					// edges, and the one a dozen corners on.
					for (size_t I = 0; I < O.size(); I++)
					{
						const ac::Vec2 Q = O[I];
						const double D = FMath::Min(Q.x, Q.y);
						if (Q.x < 60 && Q.y < 40 && D > Far && !NearPassage(Q))
							Far = D, AlongFrom = Q, AlongTo = O[(I + 12) % O.size()];
					}
				}
			}
			const auto G = [&](const double X, const double Y) { const FVector2D V = Cell(X, Y); return ac::Vec2(V.X, V.Y); };
			const auto Ground = [&](const ac::Vec2 P) { return Terrain->FieldHeight(P); };
			ac::Vec2 EyeAt, LookAt;
			double EyeH = 0, LookH = 0;
			switch (T.Phase)
			{
			case 2010:  // The wall from inside, a few metres up.
				EyeAt = G(Wall.x - 6, Wall.y + 22);
				LookAt = G(Wall.x, Wall.y);
				EyeH = Ground(EyeAt) + 4.0;
				LookH = Ground(EyeAt) + 6.5;
				break;
			case 2011:  // Over it: the neighbour's ground beyond.
				EyeAt = G(Wall.x - 6, Wall.y + 26);
				LookAt = G(Wall.x, Wall.y - 45);
				EyeH = Ground(EyeAt) + 34.0;
				LookH = 0.0;
				break;
			case 2012:  // The east passage from Prospect Landing.
				EyeAt = G(118, 48);
				LookAt = G(170, 48);
				EyeH = Ground(EyeAt) + 4.0;
				LookH = Ground(G(140, 48)) + 1.0;
				break;
			case 2013:  // The same passage from Tern Ridge, looking back.
				EyeAt = G(170, 48);
				LookAt = G(118, 48);
				EyeH = Ground(G(143, 48)) + 4.0;
				LookH = Ground(G(140, 48)) + 1.0;
				break;
			case 2014:  // Into the bay: from the open land toward the pad.
			{
				// From the pad's side, 34 cells out and a little up: the land
				// reaching into the bay, its walls closing round it.
				const ac::Vec2 Pad(76, 48);
				const ac::Vec2 In = Bay + ac::normalize(Pad - Bay) * FMath::Min(34.0, ac::distance(Pad, Bay) - 4.0);
				EyeAt = G(In.x, In.y);
				LookAt = G(Bay.x, Bay.y);
				EyeH = Ground(EyeAt) + 9.0;
				LookH = Ground(LookAt) + 2.0;
				break;
			}
			default:  // Along the wall: on the land beside it, looking along.
			{
				const ac::Vec2 Pad(76, 48);
				const auto Off = [&](ac::Vec2 P, const double Clear) {
					for (int32 K = 0; K < 60 && Terrain->GetField().wall(G(P.x, P.y)).outside > -Clear; K++)
						P = P + ac::normalize(Pad - P) * 0.5;
					return P;
				};
				const ac::Vec2 From = Off(AlongFrom, 7.0), To = Off(AlongTo, 4.0);
				EyeAt = G(From.x, From.y);
				LookAt = G(To.x, To.y);
				EyeH = Ground(EyeAt) + 3.5;
				LookH = Ground(EyeAt) + 4.5;
				break;
			}
			}
			const FVector Eye = AcSpace::ToWorld(EyeAt, EyeH);
			const FVector At = AcSpace::ToWorld(LookAt, LookH);
			W4Cam->SetActorLocationAndRotation(Eye, (At - Eye).Rotation());
			W4Set = true;
			W4ShotAt = Now;
		}
		if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		if (Now - W4ShotAt < 2.0) return;
		W4Set = false;
		static const TCHAR* const Names[] = {TEXT("w4-01-wall-inside"), TEXT("w4-02-over-the-wall"), TEXT("w4-03-passage-from-start"),
			TEXT("w4-04-passage-from-tern-ridge"), TEXT("w4-15-into-a-bay"), TEXT("w4-16-along-a-wall")};
		static const TCHAR* const Whats[] = {TEXT("Prospect Landing's north wall from inside: the cliff band, its rocks"),
			TEXT("over the wall: Cold Fork's ground beyond it"), TEXT("the east passage from Prospect Landing: the gap, Tern Ridge beyond"),
			TEXT("the same passage from Tern Ridge, looking back into Prospect Landing"),
			TEXT("into a bay of Prospect Landing: the land reaching in among the walls"),
			TEXT("along a wall of Prospect Landing: it meanders and steps")};
		ShootClean(Names[T.Phase - 2010], Whats[T.Phase - 2010]);
		return;
	}
	case 2016:
	{
		// Back in the pilot's chase view; to the east passage's mouth: east
		// along the pad's row, round the well at (128, 50) on its north side.
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		if (Pc && Pilot && Pc->GetViewTarget() != Pilot) Pc->SetViewTarget(Pilot);
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(1, ECVF_SetByCode);
		if (Since() < 1.0) return;
		Route({Cell(118, 48), Cell(124, 46), Cell(134, 47)});
		Mmo->ClearPredictionStats();
		W4Walk.Reset();
		W4LastAt = -1;
		Advance();
		return;
	}
	case 2017:
	{
		const int32 R = Drive();
		if (R < 0) return Stalled(TEXT("w4: to the east passage"));
		if (R == 0) return;
		Advance();
		return;
	}
	case 2018:
	{
		// The measured walk: east through the passage, nothing shot, every
		// frame sampled from 8 cells before the edge to 14 into Tern Ridge.
		if (!Pilot || !U) return;
		Pilot->SetTestInput(FVector2D(0, -1), 0.0);
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const APlayerController* Pc2 = GetWorld()->GetFirstPlayerController();
		if (Terrain && Pc2 && Pc2->PlayerCameraManager)
		{
			const std::optional<ac::Unit> Shown = Mmo->Prediction().shown();
			const ac::Vec2 Unit = (Shown ? Shown->position : U->position) + Terrain->CurrentCentre();
			const FVector Cam = Pc2->PlayerCameraManager->GetCameraLocation() + FVector(GetWorld()->OriginLocation);
			W4Walk.Add({Now, FApp::GetDeltaTime(), GFrameCounter, Unit, ac::Vec2(Cam.X / 100.0, Cam.Y / 100.0),
				FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
				FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())});
		}
		const ac::GroundRect& B = M->simulation().map->bounds;
		const double X = U->position.x - B.minX;
		// (The mirror's region: the World message may say the next a frame early.)
		const TOptional<FAcRegionAround> Now4 = Mmo->HereGround();
		if (Now4 && Now4->Id == TEXT("tern-ridge") && X > 14)
		{
			Stop(0.0);
			Advance();
		}
		else TimedOut(TEXT("w4: walked through into Tern Ridge"), 40);
		return;
	}
	case 2019:
	{
		// The crossing's numbers: frame times and the unit's pace round the
		// frame the change came in, against the walk before it.
		const uint64 Change = Sim->SeamlessFrame();
		int32 At = INDEX_NONE;
		for (int32 I = 0; I < W4Walk.Num(); ++I)
			if (At == INDEX_NONE && W4Walk[I].Frame >= Change) At = I;
		Check(Sim->SeamlessChanges() >= 1 && At != INDEX_NONE && At > 40 && At + 30 < W4Walk.Num(),
			FString::Printf(TEXT("w4: a seamless change in the walk (%d changes, at sample %d of %d)"), Sim->SeamlessChanges(), At, W4Walk.Num()));
		Check(Mmo->LastWasSeamless() && Mmo->LastTookPrepared(), TEXT("w4: the change was seamless, its mirror made ahead"));
		if (At != INDEX_NONE && At > 40 && At + 30 < W4Walk.Num())
		{
			TArray<double> Before;
			for (int32 I = 1; I < At - 5; ++I) Before.Add(W4Walk[I].Dt * 1000.0);
			Before.Sort();
			const double Median = Before[Before.Num() / 2], P95 = Before[FMath::Min(Before.Num() - 1, int32(Before.Num() * 0.95))];
			// The change's frames: from 2 before to 8 after, as far as its
			// work reaches (the game thread's at +1, the render thread's a few
			// frames on; K3: a check ending at +3 missed a 32 ms frame at +5
			// in every run and failed only on the +1 frame's chance).
			double Worst = 0, WorstNear = 0;
			for (int32 I = At - 2; I <= At + 8; ++I) Worst = FMath::Max(Worst, W4Walk[I].Dt * 1000.0);
			for (int32 I = At - 30; I <= At + 30; ++I) WorstNear = FMath::Max(WorstNear, W4Walk[I].Dt * 1000.0);
			// The pace in input ticks (K3): the unit steps on the 60 Hz ticks,
			// not on the frames, so a frame moves it 0, 1 or 2 steps. Over any
			// window of frames the count of steps is the clock's, give or take
			// the window's ends; a pace over a fixed time window swung 7% with
			// where the frames fell against the ticks, and a slow frame took a
			// tick more. So: the step before the change (the median of the
			// frames that moved), each frame's move a whole number of steps
			// round it (a jump is not), each step of the steady size (the
			// pace), and the steps keeping the clock (a stop loses some).
			TArray<double> Moves;
			for (int32 I = At - 40; I < At - 10; ++I)
				if (const double D = ac::distance(W4Walk[I - 1].Unit, W4Walk[I].Unit); D > 1e-3) Moves.Add(D);
			Moves.Sort();
			const double Step = Moves.Num() ? Moves[Moves.Num() / 2] : 0.0;
			const double Mean = Step * 60.0;
			double Lo = 1e9, Hi = 0, EyeLo = 1e9, EyeHi = 0, Jump = 0, Odd = 0;
			int32 Ticks = 0;
			for (int32 I = At - 10; I <= At + 20; ++I)
			{
				const double D = ac::distance(W4Walk[I - 1].Unit, W4Walk[I].Unit);
				const int32 K = Step > 0 ? FMath::RoundToInt32(D / Step) : 0;
				Ticks += K;
				Odd = FMath::Max(Odd, FMath::Abs(D - K * Step));
				if (K > 0)
				{
					Lo = FMath::Min(Lo, D / K * 60.0);
					Hi = FMath::Max(Hi, D / K * 60.0);
				}
				const double E = ac::distance(W4Walk[I - 1].Eye, W4Walk[I].Eye);
				const int32 KE = Step > 0 ? FMath::RoundToInt32(E / Step) : 0;
				if (KE > 0)
				{
					EyeLo = FMath::Min(EyeLo, E / KE * 60.0);
					EyeHi = FMath::Max(EyeHi, E / KE * 60.0);
				}
				Jump = FMath::Max(Jump, E);
			}
			const double Span = W4Walk[At + 20].At - W4Walk[At - 11].At;
			const double Clock = Span * 60.0;
			// And how far it strays from the line it walked before, for the
			// record only: the passage turns it a little as it slides along a
			// wall, so a straight line's distance is no measure of a stop.
			double Lag = 0;
			{
				const FW4Sample& A0 = W4Walk[At - 60 > 0 ? At - 60 : 1];
				const FW4Sample& A1 = W4Walk[At - 5];
				const double SpanL = A1.At - A0.At;
				const ac::Vec2 Vel = SpanL > 0 ? (A1.Unit - A0.Unit) / SpanL : ac::Vec2(0, 0);
				const double Speed = ac::length(Vel);
				const ac::Vec2 Dir = Speed > 1e-6 ? Vel / Speed : ac::Vec2(1, 0);
				for (int32 I = At - 5; I <= At + 40; ++I)
				{
					const double Expected = Speed * (W4Walk[I].At - A1.At);
					const double Went = ac::dot(W4Walk[I].Unit - A1.Unit, Dir);
					Lag = FMath::Max(Lag, FMath::Abs(Went - Expected));
				}
			}
			const FString Line = FString::Printf(
				TEXT("w4: crossing: frames before it %.1f ms median, %.1f ms 95th percentile; at the change (2 before to 8 after) %.1f ms at most, "
					 "%.1f ms within 30 frames; the unit %.2f cells/s before (%.4f cells a tick), %.2f..%.2f cells/s each tick round it, "
					 "%d ticks over %.1f of the clock, its moves %.4f cells off whole ticks; the eye %.2f..%.2f cells/s, "
					 "its largest step %.3f cells; at most %.3f cells off the line it walked before"),
				Median, P95, Worst, WorstNear, Mean, Step, Lo, Hi, Ticks, Clock, Odd, EyeLo, EyeHi, Jump, Lag);
			const ac::net::Predictor::Stats& Ps = Mmo->Prediction().stats();
			UE_LOG(LogAutocraft, Display, TEXT("%s"), *Line);
			UE_LOG(LogAutocraft, Display, TEXT("w4: crossing: the prediction corrected %lld times over the walk, by %.4f cells at most (%lld snaps)"),
				(long long)Ps.corrections, Ps.errorMax, (long long)Ps.snaps);
			FFileHelper::SaveStringToFile(Line + TEXT("\n"), *FPaths::Combine(Shots.Dir, TEXT("w4-crossing.txt")));
			// Each frame round it, for the record.
			FString Rows = TEXT("frame,ms,unit_x,unit_y,eye_x,eye_y,game_ms,render_ms,gpu_ms\n");
			for (int32 I = FMath::Max(1, At - 60); I < FMath::Min(W4Walk.Num(), At + 60); ++I)
				Rows += FString::Printf(TEXT("%lld,%.2f,%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f\n"), (long long)(int64(W4Walk[I].Frame) - int64(Change)),
					W4Walk[I].Dt * 1000.0, W4Walk[I].Unit.x, W4Walk[I].Unit.y, W4Walk[I].Eye.x, W4Walk[I].Eye.y, W4Walk[I].Game, W4Walk[I].Render,
					W4Walk[I].Gpu);
			FFileHelper::SaveStringToFile(Rows, *FPaths::Combine(Shots.Dir, TEXT("w4-crossing.csv")));
			Check(Worst <= FMath::Max(P95 * 1.25, Median + 4.0),
				FString::Printf(TEXT("w4: no hitch at the change (%.1f ms against %.1f ms median, %.1f ms 95th)"), Worst, Median, P95));
			Check(Lo > Mean * 0.9 && Hi < Mean * 1.1, FString::Printf(TEXT("w4: the unit's pace held (%.2f..%.2f against %.2f cells/s a tick)"), Lo, Hi, Mean));
			Check(Odd < Step * 0.25 && FMath::Abs(Ticks - Clock) <= 2.0,
				FString::Printf(TEXT("w4: no stop, no jump (%d ticks over %.1f of the clock; moves %.4f cells off whole ticks of %.4f)"), Ticks, Clock, Odd, Step));
			Check(Jump < 0.5, FString::Printf(TEXT("w4: no camera jump (largest step %.3f cells)"), Jump));
		}
		Advance();
		return;
	}
	case 2020:
		// A breath, then back west through it, a picture every 0.4 s.
		if (Since() < 1.0) return;
		W4Frames = 0;
		W4ShotAt = -1;
		Advance();
		return;
	case 2021:
	{
		if (!Pilot || !U) return;
		Pilot->SetTestInput(FVector2D(0, -1), ac::pi);
		const ac::GroundRect& B = M->simulation().map->bounds;
		const double X = U->position.x - B.minX;
		const TOptional<FAcRegionAround> Now4 = Mmo->HereGround();
		const FString In4 = Now4 ? Now4->Id : FString();
		// From 9 cells before Tern Ridge's west edge to 9 into Prospect Landing.
		const bool bIn = (In4 == TEXT("tern-ridge") && X < 9) || (In4 == TEXT("start") && X > 135);
		if (In4 == TEXT("start") && X <= 135)
		{
			Stop(ac::pi);
			T.Phase = 2022;
			T.PhaseAt = Now;
			return;
		}
		// Each picture starts the phase again, so its timeout never comes: a
		// unit stuck in the passage took pictures until the run was killed
		// (K3). 60 is 24 s of walk; it takes 6-8.
		if (W4Frames >= 60)
		{
			Check(false, FString::Printf(TEXT("w4: walked back into Prospect Landing within %d pictures (%s)"), W4Frames, *Where()));
			Stop(ac::pi);
			T.Phase = 2022;
			T.PhaseAt = Now;
			return;
		}
		if (bIn && (W4ShotAt < 0 || Now - W4ShotAt > 0.4))
		{
			W4ShotAt = Now;
			W4Frames += 1;
			Shoot(*FString::Printf(TEXT("w4-%02d-walk-%s"), 4 + W4Frames, *In4),
				FString::Printf(TEXT("walking west through the passage, frame %d, %s"), W4Frames, *Where()));
			T.Phase = 2021;
			return;
		}
		TimedOut(TEXT("w4: walked back into Prospect Landing"), 60);
		return;
	}
	case 2022:
		Check(W4Frames >= 6, FString::Printf(TEXT("w4: %d frames of the walk through"), W4Frames));
		ToggleWorldMap();
		Advance();
		return;
	case 2023:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map || Since() < 1.5) return;
		Map->ShowWorld();
		Advance();
		return;
	}
	case 2024:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map || Since() < 1.0) return;
		Check(Map->PicturesShown() == 9, FString::Printf(TEXT("w4: %d regions drawn from their pictures"), Map->PicturesShown()));
		Shoot(TEXT("w4-20-world-map"), TEXT("the world map: the nine regions' new shapes, walls and passages, stitched"));
		return;
	}
	case 2025:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map) return;
		if (!W4Set)
		{
			W4Set = true;
			Check(Map->ShowRegion(TEXT("start"), 1.6), TEXT("w4: the map on Prospect Landing"));
			return;
		}
		if (Since() < 1.0) return;
		W4Set = false;
		Shoot(TEXT("w4-21-world-map-start"), TEXT("the world map round Prospect Landing: its walls, its passages, the neighbours'"));
		return;
	}
	case 2026:
		ToggleWorldMap();
		if (W4Cam.IsValid()) W4Cam->Destroy();
		Check(Shots.Saved + Shots.Skipped >= 9 + 6 + 6 + 2, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsZ1
	case 2300:
	{
		// Seated at Prospect Landing; Cold Fork from its file (its zone), then
		// the server takes the pilot there, to the gorge's mouth.
		if (!(U && Mmo->World() && Since() > 2.0)) return;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("server/regions/cold-fork.json"))))
		{
			const ac::region::Parsed P = ac::region::parse(TCHAR_TO_UTF8(*Text));
			if (P.file && !P.file->zones.empty()) Z1File = *P.file;
		}
		Check(Z1File.IsSet(), TEXT("z1: Cold Fork's file has its zone"));
		if (!Z1File) return Finish();
		Check(Mmo->DevClock(TEXT("brood clear\nbrood zone here")), TEXT("z1: the server takes the pilot to Cold Fork's canyon"));
		Advance();
		return;
	}
	case 2301:
	{
		// In Cold Fork, its ground and its neighbours' built; its tunnels drawn
		// (Z4: no build spots' markers).
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const TOptional<FAcRegionAround> Ground = Mmo->HereGround();
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (!(U && Ground && Ground->Id == TEXT("cold-fork") && Sets >= 4 && Ready == Sets && Since() > 3.0))
		{
			TimedOut(TEXT("z1: in Cold Fork, its ground built"), 120);
			return;
		}
		Check(Terrain->DrawnAt(ac::Doodad::Kind::broodTunnel).IsSet(), TEXT("z1: a Brood tunnel's mouth drawn in Cold Fork"));
		Check(!Terrain->DrawnAt(ac::Doodad::Kind::buildSpot).IsSet(), TEXT("z1: no build spot markers in Cold Fork (Z4)"));
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(0, ECVF_SetByCode);
		if (!W4Cam.IsValid())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
			W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			W4Cam->GetCameraComponent()->SetFieldOfView(70.f);
		}
		W4Set = false;
		Advance();
		return;
	}
	case 2305:
		// Two groups down the paths, one from each tunnel: at the prize by
		// its picture.
		if (!W4Set)
		{
			Check(Mmo->DevClock(TEXT("brood zone 0 shardling 5\nbrood zone 1 shardling 5")), TEXT("z1: a group sent down each path"));
			W4Set = true;
			return;
		}
		if (Since() < 9.0) return;
		W4Set = false;
		Advance();
		return;
	case 2302:
	case 2303:
	case 2304:
	case 2306:
	case 2307:
	case 2308:
	{
		// A free camera in Cold Fork: its cells, its ground's heights.
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		if (!Pc || !Terrain || !W4Cam.IsValid() || !Z1File) return;
		if (!W4Set)
		{
			const ac::region::Zone& Z = Z1File->zones[0];
			const auto G = [&](const ac::Vec2 R) { const FVector2D V = Cell(R.x, R.y); return ac::Vec2(V.X, V.Y); };
			const auto Ground = [&](const ac::Vec2 R) { return Terrain->FieldHeight(G(R)); };
			ac::Vec2 Prize(0, 0);
			for (const int64 I : Z.prizeDeposits) Prize = Prize + Z1File->deposits[size_t(I)].at / double(Z.prizeDeposits.size());
			const std::vector<ac::Vec2>& North = Z.paths[0].points;
			const std::vector<ac::Vec2>& East = Z.paths.size() > 1 ? Z.paths[1].points : North;
			// Camera places on The Fork's rims (Z4: the old build spots'
			// cells, kept for the pictures; pilots build anywhere).
			static const ac::Vec2 Rims[] = {{99, 13}, {100, 19}, {102.5, 22.5}, {106.5, 26.5}, {112.5, 8.5}, {112.5, 14},
				{115, 18.5}, {129, 13}, {132.5, 11.5}, {136, 20.5}, {133, 24}, {129.5, 27.5}};
			const auto SpotNear = [&](const ac::Vec2 P) {
				ac::Vec2 Best = Rims[0];
				for (const ac::Vec2 Spot : Rims)
					if (ac::distance(Spot, P) < ac::distance(Best, P)) Best = Spot;
				return Best;
			};
			ac::Vec2 Eye, Look;
			double EyeH = 0, LookH = 0;
			switch (T.Phase)
			{
			case 2302:  // From the west rim over the north arm's bend, to the junction.
			{
				const ac::Vec2 Rim = SpotNear(North[North.size() / 2]);
				Eye = Rim + ac::normalize(Rim - North[North.size() / 2]) * 2.0;
				Look = Prize;
				EyeH = Ground(Eye) + 3.0;
				LookH = Ground(Look) + 0.5;
				break;
			}
			case 2303:  // On the floor at the north arm's end, looking back up it past its bend.
				Eye = North.back();
				Look = North[North.size() / 2];
				EyeH = Ground(Eye) + 1.4;
				LookH = Ground(Look) + 3.2;
				break;
			case 2304:  // Up the east arm's floor to its tunnel.
			{
				const ac::Vec2 Mouth = Z.entrances.size() > 1 ? Z.entrances[1].at : Z.entrances[0].at;
				Eye = East[FMath::Min<size_t>(3, East.size() - 1)];
				Look = Mouth;
				EyeH = Ground(Eye) + 1.6;
				LookH = Ground(Mouth) + 1.2;
				break;
			}
			case 2306:  // The prize from the rim between the arms, the bugs on it.
			{
				const ac::Vec2 Rim = SpotNear(Prize + ac::Vec2(0, -10));
				Eye = Rim + ac::normalize(Rim - Prize) * 1.5;
				Look = Prize;
				EyeH = Ground(Eye) + 4.0;
				LookH = Ground(Prize);
				break;
			}
			case 2307:  // The whole zone from high up, south-west of it.
				Eye = Prize + ac::Vec2(-26, 26);
				Look = Prize + ac::Vec2(2, -8);
				EyeH = 34.0;
				LookH = Ground(Prize);
				break;
			default:  // Along the west rim (west of the north arm), close.
			{
				ac::Vec2 A = North.front(), B = North.back();
				double Lo = 1e9, Hi = -1e9;
				for (const ac::Vec2 Spot : Rims)
				{
					if (Spot.x >= North.front().x) continue;
					if (Spot.y < Lo) Lo = Spot.y, A = Spot;
					if (Spot.y > Hi) Hi = Spot.y, B = Spot;
				}
				Eye = B + ac::normalize(B - A) * 5.0 + ac::Vec2(-2.0, 0);
				Look = A;
				EyeH = Ground(Eye) + 3.2;
				LookH = Ground(A);
				break;
			}
			}
			const FVector EyeW = AcSpace::ToWorld(G(Eye), EyeH);
			const FVector AtW = AcSpace::ToWorld(G(Look), LookH);
			W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
			W4Set = true;
			W4ShotAt = Now;
		}
		if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		if (Now - W4ShotAt < 2.0) return;
		W4Set = false;
		const int32 K = T.Phase < 2305 ? T.Phase - 2302 : T.Phase - 2303;
		static const TCHAR* const Names[] = {TEXT("z1-01-canyon-from-a-rim"), TEXT("z1-02-floor-up-at-a-bend"), TEXT("z1-03-a-tunnel"),
			TEXT("z1-04-prize-at-the-junction"), TEXT("z1-05-the-zone-from-above"), TEXT("z1-06-the-west-rim")};
		static const TCHAR* const Whats[] = {TEXT("The Fork from the west rim: the north arm winding down to the junction"),
			TEXT("on the canyon's floor at the north arm's end, looking back up it past its bend"),
			TEXT("a Brood tunnel's mouth at the end of the east arm"),
			TEXT("the prize field at the junction: the rich ore round the MH well, a group from each tunnel on it"),
			TEXT("the zone from high up: both arms, the junction"),
			TEXT("the west rim over the north arm, close")};
		ShootClean(Names[K], Whats[K]);
		return;
	}
	case 2309:
	{
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		if (Pc && Pilot && Pc->GetViewTarget() != Pilot) Pc->SetViewTarget(Pilot);
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(1, ECVF_SetByCode);
		if (Since() < 1.0) return;
		ToggleWorldMap();
		W4Set = false;
		Advance();
		return;
	}
	case 2310:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map || Since() < 1.5) return;
		if (!W4Set)
		{
			W4Set = true;
			Check(Map->ShowRegion(TEXT("cold-fork"), 1.0), TEXT("z1: the map on Cold Fork"));
			return;
		}
		if (Since() < 2.5) return;
		W4Set = false;
		Shoot(TEXT("z1-07-map-cold-fork"), TEXT("the world map on Cold Fork: the canyon on its satellite picture"));
		return;
	}
	case 2311:
		ToggleWorldMap();
		if (W4Cam.IsValid()) W4Cam->Destroy();
		Check(Shots.Saved + Shots.Skipped >= 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsZ2
	case 2500:
	{
		// Seated at Prospect Landing; Cold Fork's zone from its file, the pilot
		// taken into the gorge south of its prize.
		if (!(U && Mmo->World() && Since() > 2.0)) return;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("server/regions/cold-fork.json"))))
		{
			const ac::region::Parsed P = ac::region::parse(TCHAR_TO_UTF8(*Text));
			if (P.file && !P.file->zones.empty()) Z1File = *P.file;
		}
		Check(Z1File.IsSet(), TEXT("z2: Cold Fork's file has its zone"));
		if (!Z1File) return Finish();
		Check(Mmo->DevClock(TEXT("brood clear\nbrood zone here")), TEXT("z2: the server takes the pilot to The Fork"));
		Advance();
		return;
	}
	case 2501:
	{
		// In Cold Fork, its ground built: the zone reset, an AA tower on the
		// north arm's rim (it sees the arm, and never shoots a Shardling).
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const TOptional<FAcRegionAround> Ground = Mmo->HereGround();
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (!(U && Ground && Ground->Id == TEXT("cold-fork") && Sets >= 4 && Ready == Sets && Since() > 3.0))
		{
			TimedOut(TEXT("z2: in Cold Fork, its ground built"), 120);
			return;
		}
		// The server reads the dev control file a moment later: the next
		// phase's lines wait for these to be read (one file, overwritten).
		if (!W4Set)
		{
			Check(Mmo->DevClock(TEXT("brood clear\ncanyon reset\ncanyon place sentinel 100 19")), TEXT("z2: the zone reset, an AA tower on the west rim"));
			W4Set = true;
			W4ShotAt = Now;
			return;
		}
		if (Now - W4ShotAt < 1.5) return;
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(0, ECVF_SetByCode);
		if (!W4Cam.IsValid())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
			W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			W4Cam->GetCameraComponent()->SetFieldOfView(70.f);
		}
		W4Set = false;
		Advance();
		return;
	}
	case 2502:
	case 2505:
	case 2509:
	case 2511:
	case 2519:
	{
		// The server's dev lines, then a few seconds of the game.
		struct FStep
		{
			int32 Phase;
			const TCHAR* Lines;
			const TCHAR* What;
			double Wait;
		};
		static const FStep Steps[] = {
			{2502, TEXT("canyon send 1"), TEXT("z2: the opening wave sent"), 3.5},
			{2505, TEXT("canyon place gatling 99 13 102.5 22.5 106.5 26.5 132.5 11.5\ncanyon place sentinel 136 20.5 133 24\ncanyon reset\ncanyon send 1"),
				TEXT("z2: Gatling Guns on the rims, AA towers by the east arm, wave 1 again"), 2.2},
			{2509, TEXT("canyon send 4"), TEXT("z2: wave 4 sent, its Gloamwings down the east arm"), 1.8},
			{2511, TEXT("canyon fail"), TEXT("z2: wave 4 broken through"), 1.5},
			{2519, TEXT("canyon hold"), TEXT("z2: the last wave beaten"), 1.5},
		};
		for (const FStep& S2 : Steps)
		{
			if (S2.Phase != T.Phase) continue;
			if (!W4Set)
			{
				if (T.Phase == 2511 && S)
				{
					// The zone's turrets now, before the Matriarch comes.
					Z2Turrets = 0;
					for (const ac::Structure& B : S->structures)
						Z2Turrets += (B.kind == ac::StructureKind::gatling || B.kind == ac::StructureKind::sentinel) && B.hp > 0 ? 1 : 0;
				}
				Check(Mmo->DevClock(S2.Lines), S2.What);
				W4Set = true;
				return;
			}
			if (Since() < S2.Wait) return;
		}
		W4Set = false;
		Advance();
		return;
	}
	case 2507:
	case 2513:
	case 2515:
	case 2517:
	{
		// Waits on the game: wave 1 beaten; the Matriarch in sight; her first
		// turret down; the zone reset.
		if (!S) return;
		const ac::net::Sky::Zone* Z = AcCanyon::Near(*Mmo);
		bool bThere = false;
		const ac::Unit* Queen = nullptr;
		for (const ac::Unit& X : S->units)
			if (X.kind == ac::UnitKind::matriarch && X.hp > 0) Queen = &X;
		int32 Turrets = 0;
		for (const ac::Structure& B : S->structures)
			Turrets += (B.kind == ac::StructureKind::gatling || B.kind == ac::StructureKind::sentinel) && B.hp > 0 ? 1 : 0;
		switch (T.Phase)
		{
		case 2507: bThere = Z && Z->state == ac::net::Sky::Zone::State::running && Z->wave == 2 && !Z->ends; break;
		case 2513: bThere = Queen != nullptr && Since() > 4.0; break;
		case 2515: bThere = Turrets < Z2Turrets; break;
		default: bThere = Z && Z->state == ac::net::Sky::Zone::State::waiting && Turrets == 0; break;
		}
		static const TCHAR* const Waits[] = {TEXT("z2: wave 1 beaten by the rims' guns"), TEXT("z2: the Matriarch out of the tunnel"),
			TEXT("z2: her first turret felled"), TEXT("z2: every zone turret down and the zone reset")};
		const int32 K = T.Phase == 2507 ? 0 : T.Phase == 2513 ? 1 : T.Phase == 2515 ? 2 : 3;
		if (!bThere)
		{
			TimedOut(Waits[K], K == 3 ? 420 : K == 2 ? 200 : 60);
			return;
		}
		Check(true, FString::Printf(TEXT("%s after %.1f s (%d turrets standing)"), Waits[K], Since(), Turrets));
		Advance();
		return;
	}
	case 2503:
	case 2504:
	case 2506:
	case 2508:
	case 2510:
	case 2512:
	case 2514:
	case 2516:
	case 2518:
	case 2520:
	{
		// A free camera in Cold Fork; the HUD's pictures keep it on.
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		if (!Pc || !Terrain || !W4Cam.IsValid() || !Z1File) return;
		const ac::region::Zone& Z = Z1File->zones[0];
		const auto G = [&](const ac::Vec2 R) { const FVector2D V = Cell(R.x, R.y); return ac::Vec2(V.X, V.Y); };
		const auto Ground = [&](const ac::Vec2 Q) { return Terrain->FieldHeight(Q); };
		ac::Vec2 Prize(0, 0);
		for (const int64 I : Z.prizeDeposits) Prize = Prize + Z1File->deposits[size_t(I)].at / double(Z.prizeDeposits.size());
		if (!W4Set)
		{
			// Eye and look on the ground (region cells → ground), and their
			// heights over it.
			ac::Vec2 Eye = G(Prize + ac::Vec2(0, 9.5)), Look = G(Prize);
			double EyeUp = 3.5, LookUp = 0.5;
			switch (T.Phase)
			{
			case 2503:  // Behind the AA tower on the west rim, down into the north arm.
				Eye = G(ac::Vec2(100, 19) + ac::Vec2(-4, 3));
				Look = G(ac::Vec2(106.5, 13));
				EyeUp = 6.0;
				LookUp = 0.0;
				break;
			case 2506:  // Behind the west rim's guns, over the arm.
				Eye = G(ac::Vec2(100, 19) + ac::Vec2(-3, 4));
				Look = G(ac::Vec2(106, 15));
				EyeUp = 4.0;
				LookUp = 0.0;
				break;
			case 2510:  // From the junction's east side up the east arm, at the flyers' height.
				Eye = G(ac::Vec2(124, 29));
				Look = G(ac::Vec2(133, 17));
				EyeUp = 6.0;
				LookUp = 3.5;
				break;
			case 2514:  // The east arm's floor, at its tunnel.
				Eye = G(ac::Vec2(122, 23));
				Look = G(Z.entrances.size() > 1 ? Z.entrances[1].at : Z.entrances[0].at);
				EyeUp = 7.0;
				LookUp = 3.5;
				break;
			case 2516:  // Her, from the prize's side.
			{
				const ac::Unit* Queen = nullptr;
				if (S)
					for (const ac::Unit& X : S->units)
						if (X.kind == ac::UnitKind::matriarch && X.hp > 0) Queen = &X;
				if (Queen)
				{
					const ac::Vec2 Her = Queen->position;
					const ac::Vec2 Away = ac::normalize(G(Prize) - Her + ac::Vec2(0.01, 0));
					Eye = Her + Away * 15 + ac::Vec2(-Away.y, Away.x) * 5;
					Look = Her;
					EyeUp = 7.0;
					LookUp = 3.0;
				}
				break;
			}
			default: break;  // The gorge, the prize ahead: the HUD's pictures.
			}
			const FVector EyeW = AcSpace::ToWorld(Eye, Ground(Eye) + EyeUp);
			const FVector AtW = AcSpace::ToWorld(Look, Ground(Look) + LookUp);
			W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
			W4Set = true;
			W4ShotAt = Now;
		}
		if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		if (Now - W4ShotAt < (T.Phase == 2504 || T.Phase == 2512 || T.Phase == 2520 ? 2.5 : 1.2)) return;
		W4Set = false;
		const ac::net::Sky::Zone* Near = AcCanyon::Near(*Mmo);
		const SAcProgressHud* Hud = nullptr;
		if (const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this)) Hud = H->Widget().Get();
		switch (T.Phase)
		{
		case 2503:
			Check(Near && Near->ends && Near->left > 0, TEXT("z2: the opening wave out"));
			ShootClean(TEXT("z2-01-wave-1-down-the-north-arm"), TEXT("the opening wave of Shardlings coming down the north arm"));
			return;
		case 2504:
			Check(Hud && Hud->Canyon().IsSet(), TEXT("z2: the wave panel shown"));
			if (Hud && Hud->Canyon()) UE_LOG(LogAutocraft, Display, TEXT("mmoshots: z2: panel %s | %s | %s"), *Hud->Canyon()->Name, *Hud->Canyon()->Title, *Hud->Canyon()->Line);
			Shoot(TEXT("z2-02-the-wave-panel"), TEXT("the wave panel: the opening wave on the prize, the ore it takes against the mark"));
			return;
		case 2506:
			ShootClean(TEXT("z2-03-rim-guns-firing"), TEXT("wave 1 again, the west rim's Gatling Guns firing down into the north arm"));
			return;
		case 2508:
			Check(Hud && Hud->ToastTitles().Num() > 0, TEXT("z2: the beaten wave's toasts"));
			if (Hud) UE_LOG(LogAutocraft, Display, TEXT("mmoshots: z2: toasts %s"), *FString::Join(Hud->ToastTitles(), TEXT(" | ")));
			Shoot(TEXT("z2-04-wave-1-beaten"), TEXT("wave 1 beaten: the toast, the pilot's share, the countdown to wave 2"));
			return;
		case 2510:
			ShootClean(TEXT("z2-05-gloamwings-meet-aa-fire"), TEXT("wave 4's Gloamwings over the east arm, the AA towers firing"));
			return;
		case 2512:
			Check(Near && Near->state == ac::net::Sky::Zone::State::failed, TEXT("z2: the zone fallen"));
			Shoot(TEXT("z2-06-the-fork-has-fallen"), TEXT("the wave broken through: THE FORK HAS FALLEN"));
			return;
		case 2514:
			ShootClean(TEXT("z2-07-the-matriarch-arrives"), TEXT("the Matriarch out of the east tunnel"));
			return;
		case 2516:
			ShootClean(TEXT("z2-08-she-fells-the-turrets"), TEXT("the Matriarch felling the zone's turrets"));
			return;
		case 2518:
			Check(Near && Near->state == ac::net::Sky::Zone::State::waiting && Near->wave == 1, TEXT("z2: the zone back at the opening wave"));
			Shoot(TEXT("z2-09-the-reset"), TEXT("the reset: the turrets gone, the opening wave waiting again"));
			return;
		default:
			Check(Near && Near->state == ac::net::Sky::Zone::State::held, TEXT("z2: the canyon held"));
			Shoot(TEXT("z2-10-the-fork-held"), TEXT("THE FORK HELD: no waves for a world day"));
			return;
		}
	}
	case 2521:
	{
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		if (Pc && Pilot && Pc->GetViewTarget() != Pilot) Pc->SetViewTarget(Pilot);
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(1, ECVF_SetByCode);
		if (Since() < 1.0) return;
		ToggleWorldMap();
		W4Set = false;
		Advance();
		return;
	}
	case 2522:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map || Since() < 1.5) return;
		if (!W4Set)
		{
			W4Set = true;
			Check(Map->ShowRegion(TEXT("cold-fork"), 1.0), TEXT("z2: the map on Cold Fork"));
			return;
		}
		if (Since() < 2.5) return;
		W4Set = false;
		Shoot(TEXT("z2-11-map-the-fork"), TEXT("the world map on Cold Fork: The Fork's ring, held"));
		return;
	}
	case 2523:
		ToggleWorldMap();
		Check(OpenJournalScreen() && JournalScreenWidget().IsValid(), TEXT("z2: L opens the Journal"));
		Advance();
		return;
	case 2524:
	{
		const TSharedPtr<SAcJournalScreen> J = JournalScreenWidget();
		if (!J || Since() < 1.5) return;
		// Every canyon of the world (Z3 added the Drowned Cut and the Roost).
		Check(J->GetInfo().Canyons.Num() == 3, FString::Printf(TEXT("z2: the Journal shows the world's three zones (%d)"), J->GetInfo().Canyons.Num()));
		Shoot(TEXT("z2-12-journal"), TEXT("the Journal: The Fork's state in its header"));
		return;
	}
	case 2525:
		CloseJournalScreen();
		if (W4Cam.IsValid()) W4Cam->Destroy();
		Check(Shots.Saved + Shots.Skipped >= 12, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsMapPick
	case 3000:
	case 3001:
	case 3002:
	case 3003:
	case 3004:
	case 3005:
	case 3006:
	case 3007:
	case 3008:
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		switch (T.Phase)
		{
		case 3000:
			if (!(Seated() && Since() > 3.0))
			{
				TimedOut(TEXT("mappick: seated"), 120);
				return;
			}
			// A Landing Pad here only, and Rust Hollow been to.
			Check(Mmo->DevClock(TEXT("ferry pad here\nferry seen")), TEXT("mappick: a Landing Pad here, every region seen"));
			Advance();
			return;
		case 3001:
			if (Since() < 2.0) return;
			Check(OpenWorldMap(), TEXT("mappick: the world map opened"));
			Advance();
			return;
		case 3002:
			if (!Map || Since() < 1.5) return;
			if (!W4Set)
			{
				W4Set = true;
				Check(Map->ShowRegion(TEXT("start"), 1.8), TEXT("mappick: the map round Prospect Landing"));
				// A click on Rust Hollow's own pad.
				Check(!Map->PickPad(TEXT("rust-hollow"), TEXT("landing")), TEXT("mappick: Rust Hollow's own pad refused"));
				return;
			}
			if (Since() < 2.5) return;
			W4Set = false;
			Check(Map->RefusalText().StartsWith(TEXT("Only one Landing Pad in the realm")),
				FString::Printf(TEXT("mappick: the refusal line says so (%s)"), *Map->RefusalText()));
			Shoot(TEXT("mappick-01-only-here"), TEXT("a click on another region's own pad, only one Landing Pad in the realm: the refusal line"));
			return;
		case 3003:
			if (Map) ToggleWorldMap();
			Check(Mmo->DevClock(TEXT("ferry pad rust-hollow")), TEXT("mappick: a Landing Pad in Rust Hollow"));
			Advance();
			return;
		case 3004:
			if (Since() < 2.0) return;
			Check(OpenWorldMap(), TEXT("mappick: the world map again"));
			Advance();
			return;
		case 3005:
			if (!Map || Since() < 1.5) return;
			if (!W4Set)
			{
				W4Set = true;
				Map->ShowRegion(TEXT("rust-hollow"), 1.6);
				Check(!Map->PickPad(TEXT("rust-hollow"), TEXT("landing")), TEXT("mappick: Rust Hollow's own pad refused again"));
				return;
			}
			if (Since() < 2.5) return;
			W4Set = false;
			Check(Map->RefusalText().Contains(TEXT("own pad")), FString::Printf(TEXT("mappick: the own pad's line (%s)"), *Map->RefusalText()));
			Shoot(TEXT("mappick-02-own-pad"), TEXT("Rust Hollow's own pad: the Dropship lands only on a Landing Pad"));
			return;
		case 3006:
			if (!Map) return;
			Check(Map->PickPad(TEXT("rust-hollow"), TEXT("Landing Pad 1")), TEXT("mappick: Rust Hollow's Landing Pad picked"));
			Advance();
			return;
		case 3007:
			if (Since() < 1.5) return;
			Shoot(TEXT("mappick-03-picked"), TEXT("Rust Hollow's Landing Pad picked: FLY"));
			return;
		default:
			if (Map) ToggleWorldMap();
			Check(Shots.Saved >= 3, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
			Finish();
			return;
		}
	}
	// MARK: -AcMmoShotsA2
	case 2900:
	case 2901:
	case 2902:
	case 2903:
	case 2904:
	case 2905:
	case 2906:
	case 2907:
	case 2908:
	case 2909:
	case 2910:
	case 2911:
	case 2912:
	case 2913:
	case 2914:
	case 2915:
	case 2916:
	{
		UAcMusicPlayer* Music = UAcMusicPlayer::Get(this);
		if (!Music) return;
		// In the timeline since the step began (`A2From`), or ever.
		auto HeardSince = [&](const TCHAR* Kind, const FString& Name, const int32 From) {
			for (int32 I = From; I < Music->Cues().Num(); ++I)
				if (Music->Cues()[I].Kind == Kind && Music->Cues()[I].Name.StartsWith(Name)) return true;
			return false;
		};
		auto Heard = [&](const TCHAR* Kind, const FString& Name) { return HeardSince(Kind, Name, A2From); };
		// The step's dev lines, then what it waits for in the timeline.
		auto Stage = [&](const TCHAR* Lines, const TCHAR* What, TFunctionRef<bool()> Done, const double Limit) {
			if (!W4Set)
			{
				Check(Mmo->DevClock(Lines), What);
				W4Set = true;
				A2From = Music->Cues().Num();
				return;
			}
			if (!Done())
			{
				TimedOut(What, Limit);
				return;
			}
			W4Set = false;
			Check(true, FString::Printf(TEXT("a2: %s after %.1f s"), What, Since()));
			Advance();
		};
		switch (T.Phase)
		{
		case 2900:
			if (!(Seated() && Since() > 3.0))
			{
				TimedOut(TEXT("a2: seated"), 120);
				return;
			}
			Check(Mmo->DevClock(TEXT("brood clear\nbrood zone here the-fork")), TEXT("a2: to The Fork"));
			Advance();
			return;
		case 2901:
		{
			const TOptional<FAcRegionAround> Ground = Mmo->HereGround();
			if (!(U && Ground && Ground->Id == TEXT("cold-fork") && Since() > 3.0))
			{
				TimedOut(TEXT("a2: in Cold Fork"), 120);
				return;
			}
			Advance();
			return;
		}
		case 2902:
			// A wave: its layer rises, the radio ducks, KSTR calls it.
			Stage(TEXT("brood clear\ncanyon reset\ncanyon turrets 2 1 0\ncanyon send 3"), TEXT("a2: wave 3 at The Fork, its layer up"),
				[&] { return Music->SiegeWave() > 0.5 && Heard(TEXT("radio"), TEXT("canyon-the-fork-wave")) && Music->SiegeRadio() < 0.07; }, 30);
			return;
		case 2903:
			if (Since() < 4.0) return;
			Advance();
			return;
		case 2904:
			Stage(TEXT("canyon beat"), TEXT("a2: wave 3 beaten, KSTR says so"), [&] { return Heard(TEXT("radio"), TEXT("canyon-the-fork-beaten")); }, 30);
			return;
		case 2905:
			// A wave broken through: the Matriarch's layer, "has fallen".
			Stage(TEXT("canyon send 5\ncanyon fail"), TEXT("a2: The Fork fallen, the Matriarch's layer up"),
				[&] { return Music->SiegeMatriarch() > 0.9 && Heard(TEXT("radio"), TEXT("canyon-the-fork-fallen")); }, 40);
			return;
		case 2906:
			if (Since() < 3.0) return;
			Advance();
			return;
		case 2907:
			// Held: both layers out, the radio back to full.
			Stage(TEXT("canyon hold"), TEXT("a2: The Fork held, the layers out and the radio back"),
				[&] { return Music->SiegeWave() == 0 && Music->SiegeMatriarch() == 0 && Music->SiegeRadio() == 1.0 && Heard(TEXT("radio"), TEXT("canyon-the-fork-held")); },
				40);
			return;
		case 2908:
			// A goal: the next one from KSTR, read.
			Stage(TEXT("goal level 10"), TEXT("a2: a new goal, read by KSTR"), [&] {
				for (int32 I = A2From; I < Music->Cues().Num(); ++I)
					if (Music->Cues()[I].Kind == TEXT("radio") && Music->Cues()[I].Name.StartsWith(TEXT("goal-")) && Music->Cues()[I].Gain > 0) return true;
				return false;
			}, 30);
			return;
		case 2909:
			// The Dropship: pads here and at Prospect Landing, then fly.
			Stage(TEXT("ferry pad here\nferry pad start\nferry mh 200"), TEXT("a2: Landing Pads here and at Prospect Landing"),
				[&] { return Since() > 2.0; }, 10);
			return;
		case 2910:
			if (Since() < 1.0) return;
			Check(OpenWorldMap(), TEXT("a2: the world map opened"));
			Advance();
			return;
		case 2911:
		{
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			if (!Map || Since() < 1.0) return;
			Check(Map->PickPad(TEXT("start"), TEXT("Landing Pad 1")), TEXT("a2: Prospect Landing's pad picked"));
			Shots.VisitWas = Mmo->Visit();
			A2From = Music->Cues().Num();
			Map->Fly();
			Advance();
			return;
		}
		case 2912:
			if (!Heard(TEXT("flight"), TEXT("flroar")))
			{
				TimedOut(TEXT("a2: lifted off"), 30);
				return;
			}
			Advance();
			return;
		case 2913:
			if (!(Mmo->Visit() > Shots.VisitWas && Seated() && Since() > 3.0 && Heard(TEXT("flight"), TEXT("fltouch"))))
			{
				TimedOut(TEXT("a2: landed"), 90);
				return;
			}
			Advance();
			return;
		case 2914:
			Shoot(TEXT("a2-01-landed"), TEXT("landed at Prospect Landing after the siege's timeline"));
			return;
		default:
		{
			// The timeline, every cue in order.
			int32 Layers = 0, Lines = 0, Flight = 0;
			for (const UAcMusicPlayer::FCue& C : Music->Cues())
			{
				UE_LOG(LogAutocraft, Display, TEXT("mmoshots: a2: cue %7.2f s  %-6s %-44s gain %.3f"), C.At, *C.Kind, *C.Name, C.Gain);
				Layers += C.Kind == TEXT("layer") ? 1 : 0;
				Lines += C.Kind == TEXT("radio") ? 1 : 0;
				Flight += C.Kind == TEXT("flight") ? 1 : 0;
			}
			Check(HeardSince(TEXT("layer"), TEXT("siege_wave"), 0) && HeardSince(TEXT("layer"), TEXT("siege_matriarch"), 0), TEXT("a2: both layers in the timeline"));
			Check(HeardSince(TEXT("flight"), TEXT("flroar"), 0) && HeardSince(TEXT("flight"), TEXT("flcruise on"), 0) && HeardSince(TEXT("flight"), TEXT("fltouch"), 0),
				TEXT("a2: the Dropship's lift, cruise and touch"));
			Check(Lines >= 5, FString::Printf(TEXT("a2: %d KSTR lines, %d layer cues, %d flight cues"), Lines, Layers, Flight));
			Finish();
			return;
		}
		}
	}
	// MARK: -AcMmoShotsZ3
	case 2800:
	case 2801:
	case 2802:
	case 2803:
	case 2804:
	case 2805:
	case 2806:
	case 2807:
	case 2808:
	case 2809:
	case 2810:
	case 2811:
	case 2812:
	{
		// The two canyons' files (their zones in region cells).
		static TOptional<ac::region::RegionFile> Brine, Dust;
		auto Load = [](const TCHAR* Name, TOptional<ac::region::RegionFile>& Into) {
			if (Into) return;
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("server/regions"), Name)))
			{
				const ac::region::Parsed P = ac::region::parse(TCHAR_TO_UTF8(*Text));
				if (P.file && !P.file->zones.empty()) Into = *P.file;
			}
		};
		Load(TEXT("brine-pans.json"), Brine);
		Load(TEXT("dust-terraces.json"), Dust);
		if (!Brine || !Dust)
		{
			Check(false, TEXT("z3: both canyons' files read"));
			return Finish();
		}
		const bool bRoost = T.Phase >= 2806 && T.Phase <= 2810;
		const ac::region::RegionFile& F = bRoost ? *Dust : *Brine;
		const ac::region::Zone& Z = F.zones[0];
		const auto G = [&](const ac::Vec2 R) { const FVector2D V = Cell(R.x, R.y); return ac::Vec2(V.X, V.Y); };
		ac::Vec2 Prize(0, 0);
		for (const int64 I : Z.prizeDeposits) Prize = Prize + F.deposits[size_t(I)].at / double(Z.prizeDeposits.size());
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const SAcProgressHud* Hud = nullptr;
		if (const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this)) Hud = H->Widget().Get();
		auto Frame = [&](const ac::Vec2 EyeCell, const double EyeUp, const ac::Vec2 LookCell, const double LookUp) {
			if (!Pc || !Terrain) return;
			if (!W4Cam.IsValid())
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
				W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
				W4Cam->GetCameraComponent()->SetFieldOfView(62.f);
			}
			const ac::Vec2 Eye = G(EyeCell), Look = G(LookCell);
			const FVector EyeW = AcSpace::ToWorld(Eye, Terrain->FieldHeight(Eye) + EyeUp);
			const FVector AtW = AcSpace::ToWorld(Look, Terrain->FieldHeight(Look) + LookUp);
			W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
			if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		};
		auto InRegion = [&](const TCHAR* Id) {
			const TOptional<FAcRegionAround> Ground = Mmo->HereGround();
			const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
			return U && Ground && Ground->Id == Id && Sets >= 4 && Ready == Sets;
		};
		auto Bugs = [&](const bool bFlyers) {
			int32 N = 0;
			if (S)
				for (const ac::Unit& X : S->units) N += X.bug && X.hp > 0 && (X.kind == ac::UnitKind::gloamwing) == bFlyers ? 1 : 0;
			return N;
		};
		if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(0, ECVF_SetByCode);
		switch (T.Phase)
		{
		case 2800:
			if (!(Seated() && Since() > 3.0))
			{
				TimedOut(TEXT("z3: seated"), 120);
				return;
			}
			Check(Mmo->DevClock(TEXT("brood clear\nbrood zone here the-drowned-cut")), TEXT("z3: the server takes the pilot to The Drowned Cut"));
			Advance();
			return;
		case 2801:
			if (!(InRegion(TEXT("brine-pans")) && Since() > 3.0))
			{
				TimedOut(TEXT("z3: in Brine Pans, its ground built"), 120);
				return;
			}
			if (!W4Set)
			{
				Check(Mmo->DevClock(TEXT("brood clear\ncanyon reset\ncanyon turrets 2 1 0")), TEXT("z3: The Drowned Cut reset, turrets on its rims"));
				W4Set = true;
				W4ShotAt = Now;
				return;
			}
			if (Now - W4ShotAt < 2.0) return;
			W4Set = false;
			Frame(Prize + ac::Vec2(-6, 34), 42, Prize + ac::Vec2(-8, -2), 0);
			Advance();
			return;
		case 2802:
		{
			Frame(Prize + ac::Vec2(-6, 34), 42, Prize + ac::Vec2(-8, -2), 0);
			if (Since() < 1.5) return;
			const AAcBrine* B = AAcBrine::Find(GetWorld());
			Check(B && B->Sheets() == 1, FString::Printf(TEXT("z3: the brine drawn (%d sheets)"), B ? B->Sheets() : 0));
			ShootClean(TEXT("z3-01-drowned-cut-from-above"), TEXT("The Drowned Cut from above: the Salt and Gull arms, the brine over the basin"));
			return;
		}
		case 2803:
		{
			// Wave 6: Shardlings down both arms, into the brine.
			if (!W4Set)
			{
				Check(Mmo->DevClock(TEXT("canyon send 6")), TEXT("z3: wave 6 down both arms"));
				W4Set = true;
				return;
			}
			// A walker in the brine.
			const ac::Unit* Wader = nullptr;
			std::vector<ac::Vec2> Flood;
			for (const ac::Vec2 Q : Z.flood) Flood.push_back(G(Q));
			if (S)
				for (const ac::Unit& X : S->units)
				{
					if (!X.bug || X.hp <= 0 || X.kind == ac::UnitKind::gloamwing) continue;
					bool In = false;
					for (size_t I = 0, J = Flood.size() - 1; I < Flood.size(); J = I++)
						if ((Flood[I].y > X.position.y) != (Flood[J].y > X.position.y)
							&& X.position.x < (Flood[J].x - Flood[I].x) * (X.position.y - Flood[I].y) / (Flood[J].y - Flood[I].y) + Flood[I].x)
							In = !In;
					if (In && X.slowUntil.value_or(-1) > S->time) Wader = &X;
				}
			if (!Wader)
			{
				TimedOut(TEXT("z3: a Shardling wading the brine, slowed"), 120);
				return;
			}
			W4Set = false;
			const ac::Vec2 At = Wader->position;
			const ac::Vec2 Back = ac::normalize(G(Prize) - At + ac::Vec2(0.01, 0));
			{
				// Low over the water, the wader ahead, toward its arm.
				if (Pc && Terrain)
				{
					if (!W4Cam.IsValid()) Frame(Prize, 30, Prize, 0);
					const ac::Vec2 Eye = At + Back * 9 + ac::Vec2(-Back.y, Back.x) * 3;
					const FVector EyeW = AcSpace::ToWorld(Eye, Terrain->FieldHeight(At) + 3.5);
					const FVector AtW = AcSpace::ToWorld(At, Terrain->FieldHeight(At) + 0.5);
					W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
					if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
				}
			}
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: z3: a wader at %.1f,%.1f slowed to %.2f"), At.x, At.y, Wader->slowedTo.value_or(1));
			W4ShotAt = Now;
			Advance();
			return;
		}
		case 2804:
			if (Now - W4ShotAt < 0.3) return;
			ShootClean(TEXT("z3-02-waders-in-the-brine"), TEXT("a wave's Shardlings wading the brine, slowed"));
			return;
		case 2805:
			Check(AcCanyon::Near(*Mmo) && AcCanyon::Near(*Mmo)->left > 0, TEXT("z3: The Drowned Cut's wave out"));
			Shoot(TEXT("z3-03-drowned-cut-wave"), TEXT("the same with the HUD: The Drowned Cut's wave panel"));
			return;
		case 2806:
			if (!W4Set)
			{
				Check(Mmo->DevClock(TEXT("canyon reset\nbrood clear\nbrood zone here the-roost")), TEXT("z3: the server takes the pilot to The Roost"));
				W4Set = true;
				return;
			}
			if (!(InRegion(TEXT("dust-terraces")) && Since() > 3.0))
			{
				TimedOut(TEXT("z3: in Dust Terraces, its ground built"), 120);
				return;
			}
			if (Now - W4ShotAt < 0) return;
			W4Set = false;
			Check(Mmo->DevClock(TEXT("brood clear\ncanyon reset\ncanyon turrets 1 2 0")), TEXT("z3: The Roost reset, turrets on its rims"));
			Advance();
			return;
		case 2807:
			Frame(Prize + ac::Vec2(10, 30), 40, Prize + ac::Vec2(-2, -1), 0);
			if (Since() < 2.5) return;
			ShootClean(TEXT("z3-04-the-roost-from-above"), TEXT("The Roost from above: the Ochre and Dust tunnels, the roost's spire over the basin"));
			return;
		case 2808:
		{
			// Wave 4: Shardlings down the Dust arm, six Gloamwings off the roost.
			if (!W4Set)
			{
				Check(Mmo->DevClock(TEXT("canyon send 4")), TEXT("z3: wave 4: Gloamwings off the roost"));
				W4Set = true;
				W4ShotAt = Now;
				return;
			}
			ac::Vec2 Roost(0, 0);
			for (const ac::region::Zone::Entrance& E : Z.entrances)
				if (E.roost) Roost = E.at;
			// From the basin's far side, up at the roost's spire.
			Frame(Prize + ac::Vec2(9, 3), 3.0, Roost, 2.0);
			if (!(Bugs(true) >= 3 && Now - W4ShotAt > 1.6))
			{
				TimedOut(TEXT("z3: Gloamwings off the roost"), 60);
				return;
			}
			W4Set = false;
			ShootClean(TEXT("z3-05-gloamwings-off-the-roost"), TEXT("the roost's Gloamwings lifting off its ledge toward the prize"));
			return;
		}
		case 2809:
			Check(AcCanyon::Near(*Mmo) && AcCanyon::Near(*Mmo)->left > 0, TEXT("z3: The Roost's wave out"));
			Shoot(TEXT("z3-06-the-roost-wave"), TEXT("the same with the HUD: The Roost's wave panel"));
			return;
		case 2810:
		{
			if (Pc && Pilot && Pc->GetViewTarget() != Pilot) Pc->SetViewTarget(Pilot);
			if (Since() < 1.0) return;
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			if (!Map)
			{
				ToggleWorldMap();
				return;
			}
			if (!W4Set)
			{
				W4Set = true;
				W4ShotAt = Now;
				Check(Map->ShowRegion(TEXT("brine-pans"), 1.0), TEXT("z3: the map on Brine Pans"));
				return;
			}
			if (Now - W4ShotAt < 2.0) return;
			W4Set = false;
			Shoot(TEXT("z3-07-map-drowned-cut"), TEXT("the world map on Brine Pans: The Drowned Cut's ring"));
			return;
		}
		case 2811:
		{
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			if (!Map) return;
			if (!W4Set)
			{
				W4Set = true;
				W4ShotAt = Now;
				Check(Map->ShowRegion(TEXT("dust-terraces"), 1.0), TEXT("z3: the map on Dust Terraces"));
				return;
			}
			if (Now - W4ShotAt < 2.0) return;
			W4Set = false;
			Shoot(TEXT("z3-08-map-the-roost"), TEXT("the world map on Dust Terraces: The Roost's ring"));
			return;
		}
		default:
			ToggleWorldMap();
			if (IConsoleVariable* Veil = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PilotVeil"))) Veil->Set(1, ECVF_SetByCode);
			if (W4Cam.IsValid()) W4Cam->Destroy();
			Check(Shots.Saved + Shots.Skipped >= 8, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
			Finish();
			return;
		}
	}
	// MARK: -AcMmoShotsP1
	case 2600:
	case 2601:
	case 2602:
	case 2603:
	case 2604:
	case 2605:
	case 2606:
	case 2607:
	case 2608:
	case 2609:
	case 2610:
	case 2611:
	case 2612:
	case 2613:
	{
		// The frontier pilots now: the sky's list, the roster.
		const std::optional<ac::net::Sky>& Sky = Mmo ? Mmo->Sky() : std::optional<ac::net::Sky>();
		const ac::net::ProgressMessage* Pm = Mmo ? Mmo->Progress() : nullptr;
		// The one of that habit the dev line brought: in the pilot's region,
		// live, nearest it (a world has two miners and two builders; others
		// land meanwhile).
		auto Of = [&](const ac::net::FrontierHabit Habit) -> const ac::net::Sky::Frontier* {
			if (!Sky || !Pm) return nullptr;
			const ac::net::Sky::Frontier* Best = nullptr;
			double Near = 1e18;
			for (const ac::net::ProgressMessage::FrontierPilot& X : Pm->frontier)
				if (X.habit == Habit)
					for (const ac::net::Sky::Frontier& F : Sky->frontier)
					{
						if (F.id != X.id) continue;
						double D = 1e12;
						if (S && U && F.unit != 0)
							for (const ac::Unit& V : S->units)
								if (V.id == F.unit) D = ac::distance(V.position, U->position);
						if (!Best || D < Near)
						{
							Best = &F;
							Near = D;
						}
					}
			return Best;
		};
		auto UnitOf = [&](const ac::net::Sky::Frontier* F) -> const ac::Unit* {
			if (!F || !S || F->unit == 0) return nullptr;
			for (const ac::Unit& X : S->units)
				if (X.id == F->unit) return &X;
			return nullptr;
		};
		const SAcProgressHud* Hud = nullptr;
		if (const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this)) Hud = H->Widget().Get();
		auto Toasted = [&](const TCHAR* Prefix) {
			if (!Hud) return false;
			for (const FString& X : Hud->ToastTitles())
				if (X.StartsWith(Prefix)) return true;
			return false;
		};
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		// The free camera at `Eye`, looking at `Look` (ground points), up.
		auto Frame = [&](const ac::Vec2 Eye, const double EyeUp, const ac::Vec2 Look, const double LookUp) {
			if (!Pc || !Terrain) return;
			if (!W4Cam.IsValid())
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
				W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
				W4Cam->GetCameraComponent()->SetFieldOfView(62.f);
			}
			const FVector EyeW = AcSpace::ToWorld(Eye, Terrain->FieldHeight(Eye) + EyeUp);
			const FVector AtW = AcSpace::ToWorld(Look, Terrain->FieldHeight(Look) + LookUp);
			W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
			if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		};
		auto Back = [&] {
			if (Pc && Pilot && Pc->GetViewTarget() != Pilot) Pc->SetViewTarget(Pilot);
		};
		switch (T.Phase)
		{
		case 2600:
			// Seated at the dev rig's Citadel, the ground built: a miner beside.
			if (!(Seated() && Terrain && Terrain->NumSets() > 0 && Terrain->NumSetsComplete() == Terrain->NumSets() && Since() > 3.0))
			{
				TimedOut(TEXT("p1: seated, the ground built"), 120);
				return;
			}
			Check(Mmo->DevClock(TEXT("brood clear\nfrontier here miner 6 poor")), TEXT("p1: a frontier miner beside the pilot"));
			P1Miner = 0;
			Advance();
			return;
		case 2601:
		{
			// The miner at its drill: the one staged (pinned by its id once
			// seen beside the pilot; others land meanwhile).
			if (P1Miner == 0)
				if (const ac::net::Sky::Frontier* F = Of(ac::net::FrontierHabit::miner); F && UnitOf(F) && U
					&& ac::distance(UnitOf(F)->position, U->position) < 14)
					P1Miner = F->id;
			const ac::net::Sky::Frontier* Pinned = nullptr;
			if (Sky)
				for (const ac::net::Sky::Frontier& F : Sky->frontier)
					if (F.id == P1Miner) Pinned = &F;
			const ac::Unit* M1 = UnitOf(Pinned);
			if (!(M1 && M1->task == ac::Unit::Task::mining && Since() > 2.0))
			{
				TimedOut(TEXT("p1: the frontier miner drilling"), 120);
				return;
			}
			if (!W4Set)
			{
				const ac::Vec2 Away = U ? ac::normalize(U->position - M1->position + ac::Vec2(0.01, 0)) : ac::Vec2(1, 0);
				Frame(M1->position + Away * 9 + ac::Vec2(-Away.y, Away.x) * 3, 4.0, M1->position, 1.0);
				W4Set = true;
				W4ShotAt = Now;
				return;
			}
			if (Now - W4ShotAt < 1.5) return;
			W4Set = false;
			Check(Pinned && Pinned->doing == ac::net::FrontierDoing::mining, TEXT("p1: told as mining"));
			Shoot(TEXT("p1-01-frontier-miner-drilling"), TEXT("a frontier miner drilling near the pilot, its tag over it"));
			return;
		}
		case 2602:
			Back();
			Check(Mmo->DevClock(TEXT("frontier here builder -6\nfrontier build habDome")), TEXT("p1: a builder puts up a Hab Dome"));
			Advance();
			return;
		case 2603:
		{
			// Its Hab Dome finished: the deed's toast.
			// The camera on the Hab Dome from the start (a cut lands its tags
			// a frame late); the picture a second after the deed is first
			// seen (the toast has faded in).
			const ac::Structure* Dome = nullptr;
			const ac::Unit* B = UnitOf(Of(ac::net::FrontierHabit::builder));
			if (S && B)
				for (const ac::Structure& X : S->structures)
					if (X.kind == ac::StructureKind::habDome && (!Dome || ac::distance(X.position, B->position) < ac::distance(Dome->position, B->position))) Dome = &X;
			if (Dome && U)
			{
				const ac::Vec2 Away = ac::normalize(U->position - Dome->position + ac::Vec2(0.01, 0));
				Frame(Dome->position + Away * 14 + ac::Vec2(-Away.y, Away.x) * 4, 6.0, Dome->position, 1.5);
			}
			if (!Toasted(TEXT("ON THE FRONTIER")))
			{
				W4ShotAt = Now;
				TimedOut(TEXT("p1: the builder's deed told"), 60);
				return;
			}
			if (Now - W4ShotAt < 1.0) return;
			Check(Dome && Dome->complete(), TEXT("p1: the Hab Dome stands"));
			if (Hud) UE_LOG(LogAutocraft, Display, TEXT("mmoshots: p1: toasts %s"), *FString::Join(Hud->ToastTitles(), TEXT(" | ")));
			Shoot(TEXT("p1-02-builder-hab-dome-deed"), TEXT("a frontier builder's Hab Dome finished, the deed's toast"));
			return;
		}
		case 2604:
			Back();
			if (Since() < 4.0) return;  // the deed's toast fades first
			Check(Mmo->DevClock(TEXT("frontier radio raid")), TEXT("p1: a raid warning on the radio"));
			Advance();
			return;
		case 2605:
			if (!(Toasted(TEXT("KSTR  ·")) && Since() > 1.0))
			{
				TimedOut(TEXT("p1: the radio line told"), 60);
				return;
			}
			Shoot(TEXT("p1-03-radio-line"), TEXT("a KSTR radio line from a frontier pilot"));
			return;
		case 2606:
			if (Since() < 1.0) return;
			ToggleWorldMap();
			W4Set = false;
			Advance();
			return;
		case 2607:
		{
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			if (!Map || Since() < 1.5) return;
			if (!W4Set)
			{
				W4Set = true;
				Check(Map->ShowRegion(TEXT("start"), 1.6), TEXT("p1: the map round Prospect Landing"));
				return;
			}
			if (Since() < 2.5) return;
			W4Set = false;
			int32 Dots = 0;
			for (const ac::net::atlas::Item& It : Map->GetInfo().Items) Dots += It.kind == ac::net::atlas::Kind::frontier ? 1 : 0;
			Check(Dots >= 2, FString::Printf(TEXT("p1: the frontier pilots on the map (%d)"), Dots));
			Shoot(TEXT("p1-04-map-frontier-markers"), TEXT("the world map: the frontier pilots' dots in their paints"));
			return;
		}
		case 2608:
		{
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			if (!Map) return;
			const ac::net::Sky::Frontier* F = Of(ac::net::FrontierHabit::miner);
			const uint32 Id = F ? F->id : 0;
			const bool bHover = Map->HoverWhere([&](const ac::net::atlas::Item& It) {
				return It.kind == ac::net::atlas::Kind::frontier && (Id == 0 || It.ref == (F ? F->unit : 0));
			});
			Check(bHover, TEXT("p1: a frontier pilot hovered"));
			if (Since() < 1.0) return;
			Shoot(TEXT("p1-05-map-tooltip"), TEXT("a frontier pilot's tooltip: callsign, level, habit, what it does"));
			return;
		}
		case 2609:
			ToggleWorldMap();
			Check(Mmo->DevClock(TEXT("brood clear\nbrood zone here")), TEXT("p1: the server takes the pilot to The Fork"));
			Advance();
			return;
		case 2610:
		{
			// In Cold Fork, its ground built: a defender beside, wave 1 sent.
			const TOptional<FAcRegionAround> Ground = Mmo->HereGround();
			const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
			if (!(U && Ground && Ground->Id == TEXT("cold-fork") && Sets >= 4 && Ready == Sets && Since() > 3.0))
			{
				TimedOut(TEXT("p1: in Cold Fork, its ground built"), 120);
				return;
			}
			if (!W4Set)
			{
				Check(Mmo->DevClock(TEXT("brood clear\ncanyon reset\nfrontier here defender 4\ncanyon send 1")),
					TEXT("p1: a frontier defender beside the pilot, the opening wave sent"));
				W4Set = true;
				return;
			}
			W4Set = false;
			Advance();
			return;
		}
		case 2611:
		{
			// The defender at the wave's bugs.
			const ac::net::Sky::Frontier* F = Of(ac::net::FrontierHabit::defender);
			const ac::Unit* D = UnitOf(F);
			const ac::Unit* Bug = nullptr;
			if (S && D)
				for (const ac::Unit& X : S->units)
					if (X.bug && X.hp > 0 && (!Bug || ac::distance(X.position, D->position) < ac::distance(Bug->position, D->position))) Bug = &X;
			if (!(F && F->doing == ac::net::FrontierDoing::fighting && D && Bug && ac::distance(Bug->position, D->position) < 6
					&& (!U || ac::distance(U->position, D->position) < 16)))
			{
				TimedOut(TEXT("p1: the frontier defender fighting the wave"), 150);
				return;
			}
			if (!W4Set)
			{
				// Over the pilot's shoulder: the pilot, the defender beside it, the bug.
				const ac::Vec2 Mid = (D->position + Bug->position) * 0.5;
				const ac::Vec2 From = U ? U->position : D->position;
				const ac::Vec2 Behind = ac::normalize(From - Mid + ac::Vec2(0.01, 0));
				Frame(From + Behind * 6 + ac::Vec2(-Behind.y, Behind.x) * 2, 4.0, Mid, 0.8);
				W4Set = true;
				W4ShotAt = Now;
				return;
			}
			if (Now - W4ShotAt < 0.6) return;
			W4Set = false;
			ShootClean(TEXT("p1-06-defender-at-the-fork"), TEXT("at The Fork: a frontier defender fighting the wave beside the pilot"));
			return;
		}
		case 2612:
			Shoot(TEXT("p1-07-defender-with-the-hud"), TEXT("the same with the HUD: its tag, the wave panel"));
			return;
		default:
			Back();
			if (W4Cam.IsValid()) W4Cam->Destroy();
			Check(Shots.Saved + Shots.Skipped >= 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
			Finish();
			return;
		}
	}
	// MARK: -AcMmoShotsS3
	case 2700:
	case 2703:
	case 2705:
	case 2707:
	case 2711:
	case 2713:
	{
		// 2700: noon, the pilot taken to Slag Pockets 55 cells from the nest.
		if (T.Phase == 2700)
		{
			if (!(S && U && Mmo->Progress() && Since() > 3.0))
			{
				TimedOut(TEXT("the dev rig"));
				return;
			}
			Check(Mmo->DevClock(TEXT("hour 12\nmatriarch here 55")), TEXT("s3: noon, the server takes the pilot to the Matriarch's nest"));
			Advance();
			return;
		}
		// The others: the frame and GPU times over S s, facing the bugs.
		const ac::Unit* B = S && U ? NearestBug(*S, U->position, ac::UnitKind::matriarch) : nullptr;
		if (!B && S && U)
			for (const ac::Unit& X : S->units)
				if (X.bug && X.hp > 0 && (!B || ac::distance(X.position, U->position) < ac::distance(B->position, U->position))) B = &X;
		if (B && U) Stop(Face(B->position));
		double PerfFor = 8;
		FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsS3Perf="), PerfFor);
		if (Since() > 1.0)
		{
			if (S3FrameAt > 0)
			{
				S3Frames.Add((Now - S3FrameAt) * 1000.0);
				S3Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
			}
			S3FrameAt = Now;
		}
		if (Since() < 1.0 + PerfFor) return;
		int32 Bugs = 0;
		if (S)
			for (const ac::Unit& X : S->units)
				if (X.bug && X.hp > 0) Bugs++;
		auto Pct = [](TArray<double> V, const double P) { V.Sort(); return V.Num() ? V[FMath::Clamp(int32(P * V.Num()), 0, V.Num() - 1)] : 0.0; };
		double Sum = 0;
		for (const double F : S3Frames) Sum += F;
		const TCHAR* Scene = T.Phase == 2703 ? TEXT("nest, noon, daylight")
			: T.Phase == 2705                ? TEXT("nest, noon, before S3")
			: T.Phase == 2707                ? TEXT("nest, midnight")
			: T.Phase == 2711                ? TEXT("fork, noon, daylight")
											 : TEXT("fork, noon, before S3");
		UE_LOG(LogAutocraft, Log, TEXT("s3 perf: %s: %d bugs told, %d units, %d frames over %.0f s: frame mean %.2f p50 %.2f p95 %.2f p99 %.2f ms, gpu p50 %.2f p95 %.2f ms"),
			Scene, Bugs, S ? int32(S->units.size()) : 0, S3Frames.Num(), PerfFor, S3Frames.Num() ? Sum / S3Frames.Num() : 0.0, Pct(S3Frames, 0.5), Pct(S3Frames, 0.95),
			Pct(S3Frames, 0.99), Pct(S3Gpu, 0.5), Pct(S3Gpu, 0.95));
		S3Frames.Reset();
		S3Gpu.Reset();
		S3FrameAt = -1;
		Advance();
		return;
	}
	case 2701:
	{
		// In Slag Pockets, her and the ground round built: the horde held still.
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (Here == TEXT("slag-pockets") && U && S && NearestBug(*S, U->position, ac::UnitKind::matriarch) && Sets > 0 && Ready == Sets && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("matriarch calm")), TEXT("s3: the horde held still"));
			Advance();
			return;
		}
		TimedOut(TEXT("s3: in Slag Pockets, the Matriarch sent, the ground built"), 60);
		return;
	}
	case 2702:
	case 2704:
	case 2706:
	case 2710:
	case 2712:
	{
		// A scene: its dev lines, a few seconds, its checks and picture.
		const TCHAR* Lines = T.Phase == 2704 ? TEXT("daylight off")
			: T.Phase == 2706                ? TEXT("daylight on\nhour 0")
			: T.Phase == 2712                ? TEXT("daylight off")
											 : nullptr;
		if (Lines && !S3Set)
		{
			Check(Mmo->DevClock(Lines), FString::Printf(TEXT("s3: %s"), Lines));
			S3Set = true;
			return;
		}
		const ac::Unit* Q = S && U ? NearestBug(*S, U->position, ac::UnitKind::matriarch) : nullptr;
		const ac::Unit* B = Q;
		if (!B && S && U)
			for (const ac::Unit& X : S->units)
				if (X.bug && X.hp > 0 && (!B || ac::distance(X.position, U->position) < ac::distance(B->position, U->position))) B = &X;
		if (B && U) Stop(Face(B->position));
		if (Since() < (T.Phase == 2710 ? 9.0 : 3.0)) return;
		S3Set = false;
		int32 Horde = 0;
		double Farthest = 0;
		if (S && U)
			for (const ac::Unit& X : S->units)
				if (X.bug && X.hp > 0 && X.kind != ac::UnitKind::matriarch)
				{
					Horde++;
					Farthest = FMath::Max(Farthest, ac::distance(X.position, U->position));
				}
		// The share of the region's ground the mirror's fog shows seen.
		double Seen = 0;
		if (M && !M->simulation().sights.empty())
		{
			const std::vector<uint8_t>& C = M->simulation().sights[0].seen;
			int64 N = 0;
			for (const uint8_t X : C) N += X ? 1 : 0;
			Seen = C.empty() ? 0 : double(N) / double(C.size());
		}
		switch (T.Phase)
		{
		case 2702:
			Check(Horde >= 10 && Seen > 0.99, FString::Printf(TEXT("s3: by day the nest's horde told from afar (%d bugs, the farthest %.0f cells; %.0f%% of the ground seen)"), Horde, Farthest, Seen * 100));
			Shoot(TEXT("s3-01-nest-noon"), TEXT("Slag Pockets at noon, 55 cells from the nest: her horde told"));
			break;
		case 2704:
			Check(Horde <= 2 && Seen < 0.2, FString::Printf(TEXT("s3: before S3 the horde out of sight (%d bugs; %.0f%% seen)"), Horde, Seen * 100));
			Shoot(TEXT("s3-02-nest-before"), TEXT("the same with the sight before S3"));
			break;
		case 2706:
			Check(Horde <= 2 && Seen < 0.2, FString::Printf(TEXT("s3: at midnight the horde out of sight, as before (%d bugs; %.0f%% seen)"), Horde, Seen * 100));
			Shoot(TEXT("s3-03-nest-night"), TEXT("the same at midnight: half sight and the headlamp"));
			break;
		case 2710:
			Check(Horde >= 15 && Seen > 0.99, FString::Printf(TEXT("s3: by day wave 10 told across The Fork (%d bugs, the farthest %.0f cells; %.0f%% seen)"), Horde, Farthest, Seen * 100));
			Shoot(TEXT("s3-04-fork-noon"), TEXT("Cold Fork at noon, wave 10 out: all of it told"));
			break;
		default:
			Shoot(TEXT("s3-05-fork-before"), TEXT("the same with the sight before S3"));
			break;
		}
		return;
	}
	case 2708:
		Check(Mmo->DevClock(TEXT("daylight on\nhour 12\nferry go cold-fork")), TEXT("s3: noon again, the pilot taken to Cold Fork"));
		Advance();
		return;
	case 2709:
	{
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (Here == TEXT("cold-fork") && U && Sets > 0 && Ready == Sets && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("brood clear\ncanyon reset\ncanyon send 10")), TEXT("s3: wave 10 sent down The Fork"));
			Advance();
			return;
		}
		TimedOut(TEXT("s3: in Cold Fork, its ground built"), 90);
		return;
	}
	case 2714:
		Check(Mmo->DevClock(TEXT("daylight on")), TEXT("s3: daylight on again"));
		Check(Shots.Saved + Shots.Skipped >= 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsBench
	case 4000:
	{
		// Seated at home, its ground built: noon.
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (Seated() && S && Mmo->Progress() && CitadelOf(*S) && Sets > 0 && Ready == Sets && Since() > 3.0)
		{
			// -AcMmoBenchHour=H: the first scene at another hour (a dawn clip).
			double Hour = 12;
			FParse::Value(FCommandLine::Get(), TEXT("AcMmoBenchHour="), Hour);
			Check(Mmo->DevClock(FString::Printf(TEXT("hour %g"), Hour)), FString::Printf(TEXT("bench: hour %g at home"), Hour));
			Advance();
			return;
		}
		TimedOut(*FString::Printf(TEXT("bench: seated at home by the Citadel (%s), the ground built (%d of %d sets)"),
			S && CitadelOf(*S) ? TEXT("there") : TEXT("none"), Ready, Sets), 120);
		return;
	}
	case 4003:
		Check(Mmo->DevClock(TEXT("hour 12\nmatriarch here 55")), TEXT("bench: noon, the server takes the pilot to the Matriarch's nest"));
		ArriveMeter.Begin();
		bArriving = true;
		Advance();
		return;
	case 4004:
	{
		if (bArriving) ArriveMeter.Frame();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (Here == TEXT("slag-pockets") && U && S && NearestBug(*S, U->position, ac::UnitKind::matriarch) && Sets > 0 && Ready == Sets && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("matriarch calm")), TEXT("bench: the horde held still"));
			Advance();
			return;
		}
		TimedOut(TEXT("bench: in Slag Pockets, the Matriarch sent, the ground built"), 60);
		return;
	}
	case 4006:
		Check(Mmo->DevClock(TEXT("hour 12\nferry go cold-fork")), TEXT("bench: the pilot taken to Cold Fork"));
		ArriveMeter.Begin();
		bArriving = true;
		Advance();
		return;
	case 4007:
	{
		if (bArriving) ArriveMeter.Frame();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (Here == TEXT("cold-fork") && U && Sets > 0 && Ready == Sets && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("brood clear\ncanyon reset\ncanyon send 10")), TEXT("bench: wave 10 sent down The Fork"));
			Advance();
			return;
		}
		TimedOut(TEXT("bench: in Cold Fork, its ground built"), 90);
		return;
	}
	case 4001:
	case 4002:
	case 4005:
	case 4008:
	{
		// A scene: its dev lines, settled, timed, its stats read, its picture.
		const TCHAR* Name = T.Phase == 4001 ? TEXT("mmo-home-noon")
			: T.Phase == 4002                ? TEXT("mmo-home-night")
			: T.Phase == 4005                ? TEXT("mmo-nest-noon")
											 : TEXT("mmo-fork-wave");
		const TCHAR* Lines = T.Phase == 4002 ? TEXT("hour 0") : nullptr;
		const double Settle = T.Phase == 4008 ? 9.0 : 3.0;
		double Seconds = 10;
		FParse::Value(FCommandLine::Get(), TEXT("AcMmoBenchSeconds="), Seconds);
		const bool bGpu = FParse::Param(FCommandLine::Get(), TEXT("AcBenchGpu"));
		// Facing the Citadel at home, the nearest bug (her first) out there.
		if (S && U)
		{
			const ac::Unit* B = T.Phase >= 4005 ? NearestBug(*S, U->position, ac::UnitKind::matriarch) : nullptr;
			if (!B && T.Phase >= 4005)
				for (const ac::Unit& X : S->units)
					if (X.bug && X.hp > 0 && (!B || ac::distance(X.position, U->position) < ac::distance(B->position, U->position))) B = &X;
			const ac::Structure* C = T.Phase < 4005 ? CitadelOf(*S) : nullptr;
			if (B) Stop(Face(B->position));
			else if (C) Stop(Face(C->position));
		}
		switch (BenchStep)
		{
		case 0:
			if (Lines) Check(Mmo->DevClock(Lines), FString::Printf(TEXT("bench: %s"), Lines));
			BenchStep = 1;
			T.PhaseAt = Now;
			return;
		case 1:
			if (bArriving) ArriveMeter.Frame();
			if (Since() < Settle) return;
			if (bArriving)
			{
				// The arrival ends where the scene's timing begins.
				ArriveMeter.End();
				bArriving = false;
				const FString Arrive = T.Phase == 4005 ? TEXT("mmo-arrive-nest") : TEXT("mmo-arrive-fork");
				UE_LOG(LogAutocraft, Display, TEXT("bench-result %s"), *ArriveMeter.Json(Arrive, this, [&](FJsonObject& J) {
					J.SetStringField(TEXT("region"), Here);
					J.SetNumberField(TEXT("seconds"), ArriveMeter.Seconds());
				}));
			}
			BenchMeter.Begin();
			BenchStep = 2;
			return;
		case 2:
			BenchMeter.Frame();
			if (Since() < Settle + Seconds) return;
			BenchMeter.End();
			FAcBenchMeter::ShowStats(GetWorld(), true, bGpu);
			// With -AcBenchGpu, one frame's pass tree to the log too (never the profiler window).
			if (bGpu) FAcBenchMeter::ProfileGpu(GetWorld());
			// -AcBenchMemReport: where the memory is (Saved/Profiling/MemReports, the -UserDir's in a package).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcBenchMemReport")))
				if (APlayerController* MemPC = GetWorld()->GetFirstPlayerController())
				{
					MemPC->ConsoleCommand(TEXT("memreport -full"));
					// Lumen's surface cache: its pages used (to the log).
					MemPC->ConsoleCommand(TEXT("r.LumenScene.DumpStats 1"));
				}
			BenchStep = 3;
			BenchAt = Now;
			return;
		case 3:
			// The RHI counts draws only while its stats show: read for a while.
			BenchMeter.ReadStats(bGpu);
			if (Now - BenchAt < 2.0) return;
			FAcBenchMeter::ShowStats(GetWorld(), false, bGpu);
			BenchStep = 4;
			BenchAt = Now;
			return;
		default:
			if (Now - BenchAt < 0.5) return;
			UE_LOG(LogAutocraft, Display, TEXT("bench-result %s"), *BenchMeter.Json(Name, this, [&](FJsonObject& J) {
				J.SetStringField(TEXT("region"), Here);
				int32 Bugs = 0;
				if (S)
					for (const ac::Unit& X : S->units) Bugs += X.bug && X.hp > 0 ? 1 : 0;
				J.SetNumberField(TEXT("bugs"), Bugs);
				if (const AAcDaylight* D = AAcDaylight::Find(GetWorld())) J.SetNumberField(TEXT("sunElevation"), D->GetSunElevation());
			}));
			BenchStep = 0;
			Shoot(Name, FString::Printf(TEXT("the MMO bench's %s"), Name));
			return;
		}
	}
	case 4009:
		Check(Shots.Saved + Shots.Skipped >= 4, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsZ4
	case 3100:
	{
		// Seated; Cold Fork's zone from its file; the pilot taken into The Fork.
		if (!(U && Mmo->World() && Since() > 2.0)) return;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("server/regions/cold-fork.json"))))
		{
			const ac::region::Parsed P = ac::region::parse(TCHAR_TO_UTF8(*Text));
			if (P.file && !P.file->zones.empty()) Z1File = *P.file;
		}
		Check(Z1File.IsSet(), TEXT("z4: Cold Fork's file has its zone"));
		if (!Z1File) return Finish();
		Check(Mmo->DevClock(TEXT("brood clear\nbrood zone here")), TEXT("z4: the server takes the pilot to The Fork"));
		Advance();
		return;
	}
	case 3101:
	{
		// In Cold Fork, its ground built: a block of guns packed on the floor
		// south of the prize, as close as the footprints let them (the server
		// refuses what doesn't fit: ore, cliffs), then wave 3.
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const TOptional<FAcRegionAround> Ground = Mmo->HereGround();
		const int32 Sets = Terrain ? Terrain->NumSets() : 0, Ready = Terrain ? Terrain->NumSetsComplete() : 0;
		if (!(U && Ground && Ground->Id == TEXT("cold-fork") && Sets >= 4 && Ready == Sets && Since() > 3.0))
		{
			TimedOut(TEXT("z4: in Cold Fork, its ground built"), 120);
			return;
		}
		// The guns in the mirror: the server has read the line (the dev
		// control file is one file, overwritten by the next line sent).
		int32 Placed = 0;
		if (S)
			for (const ac::Structure& B : S->structures)
				if (B.kind == ac::StructureKind::gatling && B.hp > 0) Placed++;
		if (!W4Set || (Placed == 0 && Now - W4ShotAt > 5.0))
		{
			const ac::region::Zone& Z = Z1File->zones[0];
			ac::Vec2 Prize(0, 0);
			for (const int64 I : Z.prizeDeposits) Prize = Prize + Z1File->deposits[size_t(I)].at / double(Z.prizeDeposits.size());
			FString Line = TEXT("canyon reset\ncanyon place gatling");
			for (int32 Y = 0; Y < 3; ++Y)
				for (int32 X = -2; X <= 2; ++X) Line += FString::Printf(TEXT(" %.1f %.1f"), Prize.x + 2.0 * X, Prize.y + 6.0 + 2.0 * Y);
			Line += FString::Printf(TEXT("\ncanyon place sentinel %.1f %.1f %.1f %.1f"), Prize.x - 6.0, Prize.y + 8.0, Prize.x + 6.0, Prize.y + 8.0);
			Check(Mmo->DevClock(Line), TEXT("z4: a block of Gatling Guns packed on the floor by the prize, two AA towers"));
			W4Set = true;
			W4ShotAt = Now;
			return;
		}
		if (Placed == 0 || Now - W4ShotAt < 2.0)
		{
			TimedOut(TEXT("z4: the packed guns placed"), 30);
			return;
		}
		Check(Mmo->DevClock(TEXT("canyon send 3")), TEXT("z4: wave 3 sent at it"));
		W4Set = false;
		Advance();
		return;
	}
	case 3102:
	{
		APlayerController* Pc = GetWorld()->GetFirstPlayerController();
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		if (!Pc || !Terrain || !Z1File) return;
		const ac::region::Zone& Z = Z1File->zones[0];
		ac::Vec2 Prize(0, 0);
		for (const int64 I : Z.prizeDeposits) Prize = Prize + Z1File->deposits[size_t(I)].at / double(Z.prizeDeposits.size());
		const auto G = [&](const ac::Vec2 R) { const FVector2D V = Cell(R.x, R.y); return ac::Vec2(V.X, V.Y); };
		if (!W4Cam.IsValid())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
			W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
			W4Cam->GetCameraComponent()->SetFieldOfView(60.f);
		}
		const ac::Vec2 Block = Prize + ac::Vec2(0, 8);
		const ac::Vec2 Eye = Block + ac::Vec2(-7, 9);
		const FVector EyeW = AcSpace::ToWorld(G(Eye), Terrain->FieldHeight(G(Eye)) + 7.0);
		const FVector AtW = AcSpace::ToWorld(G(Block), Terrain->FieldHeight(G(Block)));
		W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
		if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
		if (Since() < 7.0) return;
		int32 Guns = 0;
		if (S)
			for (const ac::Structure& B : S->structures)
				if ((B.kind == ac::StructureKind::gatling || B.kind == ac::StructureKind::sentinel) && B.hp > 0
					&& ac::distance(B.position, G(Block)) < 10)
					Guns++;
		// The floor takes six side by side by the prize (the rest refused:
		// ore, the walls).
		Check(Guns >= 6, FString::Printf(TEXT("z4: the packed guns stand on the floor (%d)"), Guns));
		ShootClean(TEXT("z4-01-packed-defence"), TEXT("a tightly packed defence on The Fork's floor by the prize, wave 3 coming at it"));
		return;
	}
	case 3103:
		if (W4Cam.IsValid()) W4Cam->Destroy();
		Check(Shots.Saved + Shots.Skipped >= 1, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsR2
	case 3200:
		// The dev rig's home: the realm wallet filled, the Citadel screen open
		// on its REALM tab, the pilot at the Citadel.
		if (S && U && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("defence wallet")), TEXT("r2: the server fills the realm wallet"));
			Check(AtRealmCitadel(), TEXT("r2: the pilot stands at a Citadel"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("r2: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->SetTab(SAcCitadelScreen::ETab::Realm);
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel"));
		return;
	case 3201:
	{
		// The projects' cards filled, the Beacon affordable: key 7 starts it.
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (W && P && P->realmWallet.ore >= 3000 && W->GetInfo().Projects.Num() == ac::RealmProject::count && Since() > 1.0)
		{
			Check(W->ProjectWhyNot(0).IsEmpty(), TEXT("r2: the Beacon can be started here"));
			DKept = Mmo->CitadelKept();
			DRealmOre = P->realmWallet.ore;
			Check(W->Key(EKeys::Seven), TEXT("r2: key 7 starts the Beacon"));
			Advance();
			return;
		}
		TimedOut(TEXT("the REALM tab's projects"), 15);
		return;
	}
	case 3202:
	{
		// Kept: the realm wallet paid, the card runs; the screen closed at
		// once for the toast while it is fresh.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		if (P && W && Mmo->CitadelKept() > DKept && Mmo->ProjectOn(ac::RealmProject::Kind::beacon) && W->GetInfo().Projects.Num() > 0
			&& W->GetInfo().Projects[0].bRunning)
		{
			const ac::RealmProject::Price Price = ac::RealmProject::price(ac::RealmProject::Kind::beacon, 0);
			Check(P->realmWallet.ore == DRealmOre - Price.ore,
				FString::Printf(TEXT("r2: the realm wallet paid: %lld -> %lld ore"), (long long)DRealmOre, (long long)P->realmWallet.ore));
			Check(!W->KeptText().IsEmpty(), TEXT("r2: the foot says what was started"));
			CloseCitadelScreen();
			Advance();
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("r2: start: %s"), *Mmo->CitadelRefusal()));
			Advance();
			return;
		}
		TimedOut(TEXT("the Beacon started"), 10);
		return;
	}
	case 3203:
	{
		// The toast and the HUD's realm strip with the bar.
		const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this);
		const TSharedPtr<SAcProgressHud> Hud = H ? H->Widget() : nullptr;
		bool bToast = false;
		if (Hud)
			for (const FString& X : Hud->ToastTitles()) bToast = bToast || X.StartsWith(TEXT("THE REALM LIT THE BEACON"));
		if (bToast && Since() > 0.6)
		{
			Shoot(TEXT("r2-01-toast-and-bar"), TEXT("the realm-wide toast \"THE REALM LIT THE BEACON\" and the gold bar under the HUD's realm wallet"));
			return;
		}
		TimedOut(TEXT("r2: the Beacon's toast"), 10);
		return;
	}
	case 3204:
		// The REALM tab again, the Beacon running.
		Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("r2: H opens the Citadel screen again"));
		if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->SetTab(SAcCitadelScreen::ETab::Realm);
		Advance();
		return;
	case 3205:
		if (Since() < 1.5) return;
		Shoot(TEXT("r2-02-realm-tab"), TEXT("the REALM tab: the Beacon running, who started it, its time left; the other projects' prices"));
		return;
	case 3206:
	{
		CloseCitadelScreen();
		// The flight menu's fare with the Beacon: half (rounded up).
		const std::optional<int64> Fare = Mmo->FlightPrice(TEXT("cold-fork"));
		int64 Steps = -1;
		if (const ac::net::WorldMessage* Wm = Mmo->World(); Wm && Mmo->Here())
			for (const ac::net::WorldMessage::Region& R : Wm->regions)
				if (Utf(R.id) == TEXT("cold-fork")) Steps = ac::Flight::steps(Mmo->Here()->column, Mmo->Here()->row, R.column, R.row);
		const int64 Whole = Steps >= 0 ? ac::Flight::hydrogen(Steps) : -1;
		Check(Fare.has_value() && Steps >= 0 && (*Fare == (Whole + 1) / 2 || *Fare == 0),
			FString::Printf(TEXT("r2: a flight to Cold Fork costs %lld MH with the Beacon (%lld without)"), (long long)Fare.value_or(-1), (long long)Whole));
		Check(Shots.Saved + Shots.Skipped >= 2, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	}
	// MARK: -AcMmoShotsR3
	case 3300:
		// The dev rig's home: a Citadel blueprint bought at its Citadel, then
		// Cold Fork.
		if (S && U && CitadelOf(*S) && Mmo->Progress() && Seated() && Since() > 3.0)
		{
			FString Why;
			Check(AtRealmCitadel(), TEXT("r3: the pilot stands at the home Citadel"));
			Check(Mmo->BuyBlueprint(ac::StructureKind::citadel, &Why), FString::Printf(TEXT("r3: a Citadel blueprint bought at home (%s)"), *Why));
			Check(Mmo->DevClock(TEXT("ferry go cold-fork")), TEXT("r3: the server takes the pilot to Cold Fork"));
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel"));
		return;
	case 3301:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const bool bCopy = P && P->blueprints.count(ac::StructureKind::citadel) && P->blueprints.at(ac::StructureKind::citadel) > 0;
		if (Here == TEXT("cold-fork") && Seated() && bCopy && U && M && Since() > 2.0)
		{
			Check(!CitadelOf(*S), TEXT("r3: no Citadel in Cold Fork yet"));
			// Off the pad's plate: 12 cells away from it, onto open ground.
			ac::Vec2 Pad = U->position;
			double Best = 1e9;
			for (const ac::region::Place& Q : ac::net::padsOnGround(*Mmo->Here(), M->simulation().map->bounds))
				if (ac::distance(Q.at, U->position) < Best)
				{
					Best = ac::distance(Q.at, U->position);
					Pad = Q.at;
				}
			const ac::Vec2 To = U->position + ac::normalize(U->position - Pad + ac::Vec2(0, 0.01)) * 12.0;
			Route({FVector2D(To.x, To.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("in Cold Fork with a Citadel blueprint"), 30);
		return;
	}
	case 3302:
	{
		const int32 R = Drive();
		if (R < 0) return Stalled(TEXT("r3: off the pad"));
		if (R == 0) return;
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::citadel) + 1;
		Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("r3: B opened the build menu"));
		Check(Number > 0 && Build && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
			&& *Build->Placing() == ac::StructureKind::citadel, TEXT("r3: a Citadel held out in Cold Fork (the region has none)"));
		Advance();
		return;
	}
	case 3303:
		if (Since() > 1.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Spot(ac::StructureKind::citadel).IsSet(), TEXT("r3: the ghost has a spot"));
			Shoot(TEXT("r3-01-ghost-cold-fork"), TEXT("the second Citadel's ghost placed in Cold Fork"));
		}
		return;
	case 3304:
		if (UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this)) Check(Build->Key(TEXT("place")), TEXT("r3: placed (the Build request)"));
		Advance();
		return;
	case 3305:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr; C && Built(*C) >= 0.5)
		{
			Shoot(TEXT("r3-02-rising"), FString::Printf(TEXT("the second Citadel rising in Cold Fork, %.0f%%"), Built(*C) * 100));
			return;
		}
		for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("r3: built, not refused: %s"), *Why));
		TimedOut(TEXT("the Cold Fork Citadel half up"), WeldLimit);
		return;
	case 3306:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr; C && C->complete() && Since() > 2.0)
		{
			// From a free camera: the Citadel and its four corner turrets.
			APlayerController* Pc = GetWorld()->GetFirstPlayerController();
			const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
			if (Pc && Terrain)
			{
				if (!W4Cam.IsValid())
				{
					FActorSpawnParameters Params;
					Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					W4Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
					W4Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
					W4Cam->GetCameraComponent()->SetFieldOfView(62.f);
				}
				const ac::Vec2 Out = U ? ac::normalize(U->position - C->position + ac::Vec2(0.01, 0)) : ac::Vec2(1, 0);
				const ac::Vec2 Eye = C->position + Out * 13.0 + ac::Vec2(-Out.y, Out.x) * 6.0;
				const FVector EyeW = AcSpace::ToWorld(Eye, Terrain->FieldHeight(Eye) + 7.0);
				const FVector AtW = AcSpace::ToWorld(C->position, Terrain->FieldHeight(C->position) + 1.2);
				W4Cam->SetActorLocationAndRotation(EyeW, (AtW - EyeW).Rotation());
				if (Pc->GetViewTarget() != W4Cam.Get()) Pc->SetViewTarget(W4Cam.Get());
			}
			if (Since() < 3.0) return;
			// (The corner guns' state is the server's; the picture shows them.)
			Check(C->complete(), TEXT("r3: the Cold Fork Citadel finished"));
			ShootClean(TEXT("r3-03-citadel-with-turrets"), TEXT("the second Citadel finished in Cold Fork, its four corner turrets"));
			return;
		}
		TimedOut(TEXT("the Cold Fork Citadel finished"), WeldLimit);
		return;
	case 3307:
		if (APlayerController* Pc = GetWorld()->GetFirstPlayerController(); Pc && Pilot && Pc->GetViewTarget() != Pilot) Pc->SetViewTarget(Pilot);
		if (W4Cam.IsValid()) W4Cam->Destroy();
		Advance();
		return;
	case 3308:
		// Straight at it until it stands where it unloads.
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr; C && U && Pilot)
		{
			if (!AtRealmCitadel())
			{
				Pilot->SetTestInput(FVector2D(0, -1), Face(C->position));
				TimedOut(TEXT("r3: at the Cold Fork Citadel"), 20);
				return;
			}
			Stop(Pilot->Yaw());
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("r3: H opens the Citadel screen in Cold Fork"));
			Check(CitadelScreenWidget().IsValid() && CitadelScreenWidget()->GetInfo().ShopWhyNot.IsEmpty(), TEXT("r3: its shop sells here"));
			Advance();
		}
		return;
	case 3309:
		if (Since() > 1.5) Shoot(TEXT("r3-04-citadel-screen-cold-fork"), TEXT("the Citadel screen at the Cold Fork Citadel: the realm's one wallet"));
		return;
	case 3310:
		{
			CloseCitadelScreen();
			const FString Why = AcProgressHud::BlueprintRefusal(Mmo, ac::StructureKind::citadel);
			Check(Why == TEXT("This region has its Citadel: one a region"), FString::Printf(TEXT("r3: the build menu refuses a second here (%s)"), *Why));
			Check(Shots.Saved + Shots.Skipped >= 4, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
			Finish();
			return;
		}
	// MARK: -AcMmoShotsX2
	case 3400:
		// The dev rig's home: hire a mercenary at its Citadel (as H2's).
		if (S && CitadelOf(*S) && Mmo->Progress() && !Mmo->Progress()->mercFields.empty() && Since() > 3.0)
		{
			Check(FAcMmoClient::RunningServer(Mmo->SavePath).has_value(), TEXT("x2: the world server runs detached"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("x2: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget())
			{
				for (int32 K = 0; K < SAcCitadelScreen::TabCount && W->Tab() != SAcCitadelScreen::ETab::Mercs; ++K) W->Key(EKeys::Tab);
			}
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel and the mercenaries' fields"));
		return;
	case 3401:
		if (Since() < 1.0) return;
		if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->Hire();
		Advance();
		return;
	case 3402:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->mercs.size() == 1 && P->mercs[0].unit > 0 && Since() > 2.0)
		{
			Check(true, FString::Printf(TEXT("x2: %s hired"), *Utf(P->mercs[0].name)));
			CloseCitadelScreen();
			Advance();
			return;
		}
		TimedOut(TEXT("the mercenary hired"), 15);
		return;
	}
	case 3403:
		// QUIT TO HOME: the server runs on, the pilot off line.
		if (Since() < 3.0) return;
		StopMmo();
		OpenMenu(EAcMenuMode::Home);
		Advance();
		return;
	case 3404:
		if (Since() < 2.0) return;
		Check(FAcMmoClient::RunningServer().has_value(), TEXT("x2: the world server runs on at home"));
		Check(FAcMmoClient::DevClock(TEXT("catchup 10800")), TEXT("x2: the world caught up on three hours"));
		Advance();
		return;
	case 3405:
	{
		// The summary file, read again until the server wrote it.
		if (FMath::Fmod(Since(), 1.0) < 0.05) OpenMenu(EAcMenuMode::Home);
		const TSharedPtr<SAcMenu> W = MenuWidget();
		if (W && W->AwayRan().StartsWith(TEXT("Your world ran 3 h")) && W->AwayWhat().Contains(TEXT("delivered")) && Since() > 2.0)
		{
			X2Ran = W->AwayRan();
			X2What = W->AwayWhat();
			Check(true, FString::Printf(TEXT("x2: home: \"%s\" \"%s\""), *X2Ran, *X2What));
			Shoot(TEXT("x2-01-home"), TEXT("home: the away lines under ENTER THE REACH"));
			return;
		}
		TimedOut(TEXT("the away lines on the home screen"), 30);
		return;
	}
	case 3406:
		if (const UAcMusicPlayer* Music = UAcMusicPlayer::Get(this)) X2Cues = Music->Cues().Num();
		OnMenuPick(EAcMenuAction::Mmo);
		Advance();
		return;
	case 3407:
	{
		const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
		const double Age = Hud && Hud->AwayReportAt() > 0 ? FPlatformTime::Seconds() - Hud->AwayReportAt() : -1;
		if (Seated() && Hud && Hud->Away() && Age >= 0.6)
		{
			const ac::net::AwayReport& R = *Hud->Away();
			bool bMerc = false;
			for (const ac::net::AwayRow& Row : R.rows) bMerc = bMerc || (Row.kind == ac::net::AwayKind::merc && Row.value > 0);
			Check(bMerc && R.ran >= 10800, FString::Printf(TEXT("x2: the report: %d rows, %s, the mercenary's deliveries in it"), int32(R.rows.size()),
				*Utf(ac::net::awayRan(R.ran))));
			Check(Utf(ac::net::awaySummary(R)[1]) == X2What,
				FString::Printf(TEXT("x2: the toast's report is the home screen's (\"%s\")"), *Utf(ac::net::awaySummary(R)[1])));
			bool bHeard = false;
			if (const UAcMusicPlayer* Music = UAcMusicPlayer::Get(this))
				for (int32 I = X2Cues; I < Music->Cues().Num(); ++I)
					bHeard = bHeard || (Music->Cues()[I].Kind == TEXT("radio") && Music->Cues()[I].Name == Utf(ac::net::awayVoice(R)));
			Check(bHeard, FString::Printf(TEXT("x2: KSTR's welcome read (%s)"), *Utf(ac::net::awayVoice(R))));
			Check(Age < 3.8, FString::Printf(TEXT("x2: the toast still up (%.1f s old)"), Age));
			Shoot(TEXT("x2-02-toast"), TEXT("in again: WHILE YOU WERE AWAY"));
			return;
		}
		TimedOut(TEXT("the away report on the return"), 40);
		return;
	}
	case 3408:
		Check(OpenJournalScreen() && JournalScreenWidget().IsValid(), TEXT("x2: L opens the Journal"));
		Advance();
		return;
	case 3409:
	{
		const TSharedPtr<SAcJournalScreen> W = JournalScreenWidget();
		if (!W || Since() < 1.2) return;
		Check(W->AwayPicked() && W->GetInfo().bAway && !W->GetInfo().AwayLines.IsEmpty(),
			FString::Printf(TEXT("x2: the Journal opens on WHILE YOU WERE AWAY (%d rows)"), W->GetInfo().AwayLines.Num()));
		Shoot(TEXT("x2-03-journal"), TEXT("the Journal: WHILE YOU WERE AWAY, every row with when"));
		return;
	}
	case 3410:
	{
		CloseJournalScreen();
		const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
		Check(Hud && !Hud->AwayUnread(), TEXT("x2: read once shown"));
		// Home, and the detached server stopped (STOP).
		StopMmo();
		OpenMenu(EAcMenuMode::Home);
		Advance();
		return;
	}
	case 3411:
		if (Since() < 2.0) return;
		OnMenuPick(EAcMenuAction::StopServer);
		Check(!FAcMmoClient::RunningServer().has_value(), TEXT("x2: STOP saved and stopped the world server"));
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsS4
	case 3500:
		// The dev rig's home, the chain done: three frontier pilots beside
		// the pilot, a mercenary hired at its Citadel (as X2's).
		if (S && CitadelOf(*S) && Mmo->Progress() && !Mmo->Progress()->mercFields.empty() && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("brood clear\nfrontier here miner 6 poor\nfrontier here builder -6\nfrontier here defender 4")),
				TEXT("s4: three frontier pilots beside the pilot"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("s4: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget())
			{
				for (int32 K = 0; K < SAcCitadelScreen::TabCount && W->Tab() != SAcCitadelScreen::ETab::Mercs; ++K) W->Key(EKeys::Tab);
			}
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel and the mercenaries' fields"));
		return;
	case 3501:
		if (Since() < 1.0) return;
		if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->Hire();
		Advance();
		return;
	case 3502:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->mercs.size() == 1 && P->mercs[0].unit > 0 && Since() > 2.0)
		{
			CloseCitadelScreen();
			// Two world hours of the frontier pilots' and the mercenary's
			// work, booked as on a world with nobody on line.
			Check(Mmo->DevClock(TEXT("idle 7200")), TEXT("s4: the world idles two hours"));
			Advance();
			return;
		}
		TimedOut(TEXT("the mercenary hired"), 15);
		return;
	}
	case 3503:
	{
		// The standings told: three frontier pilots or more with ore.
		const ac::net::StandingsMessage* St = Mmo->Standings();
		int32 Hauled = 0;
		if (St)
			for (const ac::net::WeekStanding& R : St->rows) Hauled += R.frontier && R.ore > 0 ? 1 : 0;
		if (St && St->you && Hauled >= 2 && Since() > 3.0)
		{
			for (const ac::net::WeekStanding& R : St->rows)
				UE_LOG(LogAutocraft, Log, TEXT("s4: %lld %s%s: %lld ore, %lld bugs, %lld waves, %lld built"), (long long)R.rank, *Utf(R.callsign),
					R.frontier ? TEXT(" (frontier)") : TEXT(""), (long long)R.ore, (long long)R.bugs, (long long)R.waves, (long long)R.built);
			Check(true, FString::Printf(TEXT("s4: %d pilots in the standings, %d frontier pilots hauled"), int32(St->rows.size()), Hauled));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("s4: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->SetTab(SAcCitadelScreen::ETab::Realm);
			Advance();
			return;
		}
		TimedOut(TEXT("s4: the standings with the frontier pilots' work"), 60);
		return;
	}
	case 3504:
	{
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		if (!W || Since() < 1.5) return;
		const FAcCitadelScreenInfo& I = W->GetInfo();
		bool bYou = false, bFrontier = false;
		for (const FAcStandingLine& X : I.Standings)
		{
			bYou = bYou || X.bYou;
			bFrontier = bFrontier || X.bFrontier;
		}
		Check(I.bStandingsKnown && bYou && bFrontier && !I.WeekLeft.IsEmpty(),
			FString::Printf(TEXT("s4: the REALM tab's standings: %d rows, the pilot's own, frontier pilots, %s"), I.Standings.Num(), *I.WeekLeft));
		Shoot(TEXT("s4-01-realm-standings"), TEXT("the REALM tab: the week's standings, frontier pilots and the pilot's own row, the time left"));
		return;
	}
	case 3505:
		CloseCitadelScreen();
		if (Since() < 1.0) return;
		ToggleWorldMap();
		W4Set = false;
		Advance();
		return;
	case 3506:
	{
		// A frontier pilot's dot with its rank line, hovered.
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map || Since() < 1.5) return;
		if (!W4Set)
		{
			W4Set = true;
			Check(Map->ShowRegion(TEXT("start"), 1.6), TEXT("s4: the map round Prospect Landing"));
			return;
		}
		if (Since() < 2.5) return;
		auto Ranked = [](const ac::net::atlas::Item& It) {
			if (It.kind != ac::net::atlas::Kind::frontier) return false;
			for (const std::string& L : It.lines)
				if (L.find("this week") != std::string::npos && L.find(" ore ") != std::string::npos) return true;
			return false;
		};
		const bool bHover = Map->HoverWhere(Ranked);
		if (Since() < 3.5) return;
		Check(bHover, TEXT("s4: a frontier pilot with its rank hovered"));
		W4Set = false;
		Shoot(TEXT("s4-02-map-tooltip"), TEXT("a frontier pilot's map tooltip with its rank and its ore this week"));
		return;
	}
	case 3507:
		// The map closed once, then the week ends.
		if (!W4Set)
		{
			W4Set = true;
			ToggleWorldMap();
			return;
		}
		if (Since() < 1.5) return;
		W4Set = false;
		Check(!WorldMapWidget().IsValid(), TEXT("s4: the map closed"));
		Check(Mmo->DevClock(TEXT("week end")), TEXT("s4: the week ends now"));
		Advance();
		return;
	case 3508:
	{
		const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this);
		const TSharedPtr<SAcProgressHud> Hud = H ? H->Widget() : nullptr;
		bool bToast = false;
		if (Hud)
			for (const FString& X : Hud->ToastTitles()) bToast = bToast || X.StartsWith(TEXT("THE WEEK IS COUNTED"));
		const ac::net::StandingsMessage* St = Mmo->Standings();
		if (bToast && St && St->rows.size() <= 1 && Since() > 2.2)
		{
			Check(true, TEXT("s4: the counts start again"));
			Shoot(TEXT("s4-03-week-toast"), TEXT("the week's end: THE WEEK IS COUNTED, its top hauler and the ore"));
			return;
		}
		TimedOut(TEXT("s4: the week's toast"), 15);
		return;
	}
	case 3509:
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsN4 (mmohunt)
	case 3600:
		// The dev rig's home, the chain done: a frontier defender beside the
		// pilot; the MERCS tab.
		if (S && CitadelOf(*S) && Mmo->Progress() && !Mmo->Progress()->mercFields.empty() && Mmo->Hunt() && Since() > 3.0)
		{
			Check(Mmo->DevClock(HourGiven() ? TEXT("brood clear\nhunt open\nfrontier here defender 6")
											: TEXT("hour 12\nbrood clear\nhunt open\nfrontier here defender 6")),
				TEXT("n4: a frontier defender beside the pilot"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("n4: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->SetTab(SAcCitadelScreen::ETab::Mercs);
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel, the mercenaries' fields, the hunt told"));
		return;
	case 3601:
		if (Since() < 1.0) return;
		if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->Hire(true);
		Advance();
		return;
	case 3602:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->mercs.size() == 1 && Since() > 1.5)
		{
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->Hire(true);
			Advance();
			return;
		}
		TimedOut(TEXT("n4: the first guard hired"), 15);
		return;
	}
	case 3603:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->mercs.size() == 2 && Since() > 1.5)
		{
			Check(P->mercs[0].guard && P->mercs[1].guard, TEXT("n4: two guards hired (HIRE A GUARD)"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->SetTab(SAcCitadelScreen::ETab::Realm);
			Advance();
			return;
		}
		TimedOut(TEXT("n4: the second guard hired"), 15);
		return;
	}
	case 3604:
	{
		const ac::net::HuntMessage* H = Mmo->Hunt();
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		if (H && W && H->refusal.empty() && W->HuntWhyNot().IsEmpty() && Since() > 1.0)
		{
			Check(H->state == ac::net::HuntMessage::State::none, TEXT("n4: no hunt on before the call"));
			W->CallHunt();
			Advance();
			return;
		}
		TimedOut(TEXT("n4: THE HUNT open to call"), 15);
		return;
	}
	case 3605:
	{
		const ac::net::HuntMessage* H = Mmo->Hunt();
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		if (H && W && H->state == ac::net::HuntMessage::State::gathering && H->party.size() >= 4 && Since() > 2.5)
		{
			const FAcCitadelScreenInfo& I = W->GetInfo();
			Check(I.bHuntOn && I.HuntParty.Num() >= 4 && I.HuntState == TEXT("GATHERING"),
				FString::Printf(TEXT("n4: the REALM tab's hunt: %s, %d in the party (the caller, a defender, two guards)"), *I.HuntState, I.HuntParty.Num()));
			Check(!W->HuntWhyNot().IsEmpty(), FString::Printf(TEXT("n4: a second call refused: %s"), *W->HuntWhyNot()));
			Check(!W->KeptText().IsEmpty(), FString::Printf(TEXT("n4: the call kept: %s"), *W->KeptText()));
			Shoot(TEXT("n4-01-realm-hunt"), TEXT("the REALM tab: THE HUNT called, gathering, its party"));
			return;
		}
		TimedOut(TEXT("n4: the hunt called, its party told"), 15);
		return;
	}
	case 3606:
		CloseCitadelScreen();
		if (Since() < 1.0) return;
		ToggleWorldMap();
		W4Set = false;
		Advance();
		return;
	case 3607:
	{
		// The ring on the map, two of the party there (the defender, a guard).
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		const ac::net::HuntMessage* H = Mmo->Hunt();
		if (!Map || !H) return;
		if (!W4Set)
		{
			W4Set = true;
			Check(Map->ShowRegion(TEXT("slag-pockets"), 1.4), TEXT("n4: the map round Slag Pockets"));
			return;
		}
		int32 Guards = 0;
		for (const ac::net::HuntHunter& X : H->party) Guards += X.kind == ac::net::HuntHunter::Kind::merc && X.there ? 1 : 0;
		if (ac::net::huntThere(*H) >= 2 && Guards >= 1 && Since() > 3.0)
		{
			for (const ac::net::HuntHunter& X : H->party)
				UE_LOG(LogAutocraft, Log, TEXT("n4: %s %s: %s"), *Utf(X.name), X.kind == ac::net::HuntHunter::Kind::merc ? TEXT("guard") : TEXT("hunter"),
					X.down ? TEXT("down") : X.there ? TEXT("at the ring") : TEXT("on the way"));
			Check(true, FString::Printf(TEXT("n4: %lld of the party at the ring %.0f s after the call, by the rules"), (long long)ac::net::huntThere(*H),
				Mmo->Sky() ? Mmo->Sky()->time - H->calledAt : -1.0));
			W4Set = false;
			Shoot(TEXT("n4-02-map-ring"), TEXT("the world map: the ring round her nest, the party on the way and there"));
			return;
		}
		TimedOut(TEXT("n4: a defender and a guard at the ring"), 150);
		return;
	}
	case 3608:
		// The map closed once; the pilot taken to her nest's region, on the
		// ring's side, outside it.
		if (!W4Set)
		{
			W4Set = true;
			ToggleWorldMap();
			return;
		}
		if (Since() < 1.5) return;
		W4Set = false;
		Check(Mmo->DevClock(TEXT("matriarch here 44")), TEXT("n4: the pilot taken to her nest, outside the ring"));
		Advance();
		return;
	case 3609:
		if (Here == TEXT("slag-pockets") && U && S && NearestBug(*S, U->position, ac::UnitKind::matriarch))
		{
			Check(Mmo->DevClock(TEXT("matriarch calm")), TEXT("n4: the horde held still for the pictures"));
			Advance();
			return;
		}
		TimedOut(TEXT("n4: in Slag Pockets, the Matriarch sent"), 40);
		return;
	case 3610:
	{
		const ac::Unit* Q = NearestBug(*S, U->position, ac::UnitKind::matriarch);
		if (Q) Stop(Face(Q->position));
		const ac::net::HuntMessage* H = Mmo->Hunt();
		if (Q && H && ac::net::huntThere(*H) >= 2 && Since() > 5.0)
		{
			Check(H->state == ac::net::HuntMessage::State::gathering, TEXT("n4: still gathering: the caller outside the ring"));
			ShootClean(TEXT("n4-03-ring"), TEXT("the party gathered at the ring round her nest"));
			return;
		}
		TimedOut(TEXT("n4: the party at the ring in sight"), 30);
		return;
	}
	case 3611:
		// The gathering's two minutes jumped; the pilot inside the ring.
		Check(Mmo->DevClock(TEXT("hunt gathered\nmatriarch here 24")), TEXT("n4: the gathering over, the caller inside the ring"));
		Advance();
		return;
	case 3612:
	{
		const ac::Unit* Q = S && U ? NearestBug(*S, U->position, ac::UnitKind::matriarch) : nullptr;
		if (Q) Stop(Face(Q->position));
		const ac::net::HuntMessage* H = Mmo->Hunt();
		const UAcProgressHudSubsystem* Hs = UAcProgressHudSubsystem::Get(this);
		const TSharedPtr<SAcProgressHud> Hud = Hs ? Hs->Widget() : nullptr;
		bool bToast = false;
		if (Hud)
			for (const FString& X : Hud->ToastTitles()) bToast = bToast || X == TEXT("THE HUNT GOES IN");
		if (H && H->state == ac::net::HuntMessage::State::assault && bToast && Since() > 3.0)
		{
			Check(true, TEXT("n4: the assault: THE HUNT GOES IN"));
			Shoot(TEXT("n4-04-charge"), TEXT("the charge: THE HUNT GOES IN, the party in"));
			return;
		}
		TimedOut(TEXT("n4: the charge"), 20);
		return;
	}
	case 3613:
		// Her fight cut short: hurt by the pilot, then down by its hit.
		Check(Mmo->DevClock(TEXT("matriarch hurt 0.6\nmatriarch down")), TEXT("n4: her fight cut short"));
		Advance();
		return;
	case 3614:
	{
		const ac::net::HuntMessage* H = Mmo->Hunt();
		const UAcProgressHudSubsystem* Hs = UAcProgressHudSubsystem::Get(this);
		const TSharedPtr<SAcProgressHud> Hud = Hs ? Hs->Widget() : nullptr;
		bool bFall = false, bShare = false;
		if (Hud)
			for (const FString& X : Hud->ToastTitles())
			{
				bFall = bFall || X == TEXT("THE HUNT BROUGHT HER DOWN");
				bShare = bShare || X == TEXT("YOUR SHARE OF THE MATRIARCH");
			}
		if (H && H->state == ac::net::HuntMessage::State::won && bFall && Since() > 2.2)
		{
			Check(bShare, TEXT("n4: her pay: YOUR SHARE OF THE MATRIARCH"));
			const ac::net::StandingsMessage* St = Mmo->Standings();
			const bool bCounted = St && St->you && St->rows[size_t(*St->you)].bugs >= 1;
			Check(bCounted, TEXT("n4: she counts as a bug felled to her hardest hitter"));
			Check(!H->refusal.empty(), FString::Printf(TEXT("n4: the day's call spent: %s"), UTF8_TO_TCHAR(H->refusal.c_str())));
			Shoot(TEXT("n4-05-fall-toast"), TEXT("her fall: THE HUNT BROUGHT HER DOWN, the share"));
			return;
		}
		TimedOut(TEXT("n4: her fall's toast"), 20);
		return;
	}
	case 3615:
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsL1
	case 2100:
		// The first ten minutes' last step open: a point spent ends it.
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 6 && Since() > 2.0)
		{
			Check(!Mmo->Progress()->goal.has_value(), TEXT("l1: no goal while the first ten minutes run"));
			Mmo->Spend(ac::Skill::quickDrill);
			Advance();
			return;
		}
		TimedOut(TEXT("l1: the first ten minutes' step 6"));
		return;
	case 2101:
	{
		// The chain over: KSTR's first goal, its toast, the NEXT line.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->goal && !AcProgressHud::FirstStep(*Mmo) && Since() > 2.5)
		{
			Check(P->goal->key == "derrick", FString::Printf(TEXT("l1: the first goal after the chain is the Derrick's (%s)"), UTF8_TO_TCHAR(P->goal->key.c_str())));
			Check(P->goal->marked, TEXT("l1: the goal's marker on the nearest well"));
			Shoot(TEXT("l1-01-next-after-o1"), TEXT("after the first ten minutes: KSTR's toast and the NEXT line under the contracts"));
			return;
		}
		TimedOut(TEXT("l1: the first goal"), 20);
		return;
	}
	case 2102:
		Check(OpenJournalScreen() && JournalScreenWidget().IsValid(), TEXT("l1: L opens the Journal"));
		Advance();
		return;
	case 2103:
		if (Since() > 1.0)
		{
			const TSharedPtr<SAcJournalScreen> J = JournalScreenWidget();
			Check(J && J->GetInfo().bNext && !J->GetInfo().NextWhy.IsEmpty(), TEXT("l1: the Journal's NEXT section, why it matters"));
			Shoot(TEXT("l1-02-journal-next"), TEXT("the Journal (L): the NEXT section on top, the goal and why it matters"));
		}
		return;
	case 2104:
		CloseJournalScreen();
		// The Derrick passed (dev): spending the points is next.
		Check(Mmo->DevClock(TEXT("goal pass derrick")), TEXT("l1: the dev line passes the Derrick's goal"));
		Advance();
		return;
	case 2105:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->goal && P->goal->key == "skills" && Since() > 3.0)
		{
			Shoot(TEXT("l1-03-goal-skills"), TEXT("the next goal: spend your skill points, the count in its line"));
			return;
		}
		TimedOut(TEXT("l1: the skill points' goal"), 20);
		return;
	}
	case 2106:
	{
		// Every point spent, one a second: the goal done, the next one in.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->goal && P->goal->key == "mission")
		{
			L1DoneAt = Now;
			Advance();
			return;
		}
		if (P && P->points > 0 && Now - P5AskedAt > 1.0)
		{
			Mmo->Spend(ac::Skill::quickDrill);
			P5AskedAt = Now;
		}
		TimedOut(TEXT("l1: the points spent, the goal done"), 20);
		return;
	}
	case 2107:
		if (Now - L1DoneAt > 1.2)
		{
			Shoot(TEXT("l1-04-goal-done"), TEXT("the goal done: its toast and pay, the next goal on the NEXT line"));
		}
		return;
	case 2108:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && !Hud->ContractsExpanded()) Hud->ToggleContracts();
		if (Since() > 1.0) Shoot(TEXT("l1-05-panel-whole"), TEXT("T: the whole panel, the next goal with the radio's line"));
		return;
	case 2109:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && Hud->ContractsExpanded()) Hud->ToggleContracts();
		Check(Mmo->DevClock(TEXT("goal level 10")), TEXT("l1: the dev line makes the pilot level 10"));
		Advance();
		return;
	case 2110:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->goal && P->goal->key == "active" && Since() > 3.0)
		{
			Check(P->level >= 10, FString::Printf(TEXT("l1: level %lld"), (long long)P->level));
			Shoot(TEXT("l1-06-level-10"), TEXT("a level-10 pilot's goal: its first active on Q"));
			return;
		}
		TimedOut(TEXT("l1: the level-10 goal"), 20);
		return;
	}
	case 2111:
		if (!JournalScreenWidget().IsValid()) Check(OpenJournalScreen(), TEXT("l1: L opens the Journal again"));
		if (Since() > 1.0) Shoot(TEXT("l1-07-level-10-journal"), TEXT("the Journal's NEXT section for the level-10 goal"));
		return;
	case 2112:
		CloseJournalScreen();
		Check(Shots.Saved + Shots.Skipped == 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsU1
	case 2200:
		// The dev rig's home, at a Citadel: ten levels at once.
		if (S && U && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0)
		{
			Check(AtRealmCitadel(), TEXT("u1: the pilot stands at a Citadel"));
			Check(Mmo->DevClock(TEXT("goal level 35")), TEXT("u1: the dev line makes the pilot level 35"));
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel"));
		return;
	case 2201:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->level >= 35 && Since() > 1.2)
		{
			const UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this);
			Check(L && L->BannerShowing(), TEXT("u1: the level-up banner is up"));
			Shoot(TEXT("u1-01-level-up"), TEXT("the level-up banner and KSTR's NEXT toast at once"));
			return;
		}
		TimedOut(TEXT("u1: level 35"), 20);
		return;
	}
	case 2202:
		// The panel as it stands most of the time.
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && Hud->ContractsExpanded()) Hud->ToggleContracts();
		if (Since() > 6.0) Shoot(TEXT("u1-02-panel-compact"), TEXT("the contract panel compact: titles, the NEXT line"));
		return;
	case 2203:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && !Hud->ContractsExpanded()) Hud->ToggleContracts();
		if (Since() > 1.0) Shoot(TEXT("u1-03-panel-whole"), TEXT("T: the contract panel whole"));
		return;
	case 2204:
	{
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			if (Hud->ContractsExpanded()) Hud->ToggleContracts();
			// Three loads home, a contract, a mission and a goal done at once.
			for (int32 I = 0; I < 3; ++I) U1Gain(24, 3);
			using K = ac::net::ProgressMessage::NewsKind;
			const ac::net::ProgressMessage* P = Mmo->Progress();
			ac::net::ProgressMessage::News N;
			if (P && !P->contracts.empty())
			{
				N.kind = K::contract;
				N.id = std::to_string(P->contracts[0].id);
				N.xp = P->contracts[0].xp;
				N.value = P->contracts[0].hydrogen;
				N.by = P->contracts[0].title;
				Hud->PreviewNews(N);
			}
			N = {};
			N.kind = K::mission;
			N.id = "9001";
			N.xp = 1200;
			N.value = 400;
			N.by = "Salvage the Ore Hauler";
			Hud->PreviewNews(N);
			N = {};
			N.kind = K::goal;
			N.id = "spacedock";
			N.xp = 600;
			N.value = 40;
			N.by = "A Dock for Ships";
			Hud->PreviewNews(N);
		}
		Advance();
		return;
	}
	case 2205:
		if (Since() > 0.8) Shoot(TEXT("u1-04-busy"), TEXT("a busy moment: three unloads, a contract, a mission and a goal done"));
		return;
	case 2206:
	case 2207:
	case 2208:
	case 2209:
	case 2210:
	{
		// Each screen over a banner and toasts: it opens clean on top.
		static const TCHAR* const Names[] = {TEXT("k"), TEXT("j"), TEXT("h"), TEXT("l"), TEXT("m")};
		static const TCHAR* const Says[] = {TEXT("K: the skill screen"), TEXT("J: the rig screen"), TEXT("H: the Citadel screen"),
			TEXT("L: the Journal"), TEXT("M: the world map")};
		const int32 I = T.Phase - 2206;
		if (U1Staged != T.Phase)
		{
			U1Staged = T.Phase;
			T.PhaseAt = Now;
			CloseSkillScreen();
			CloseRigScreen();
			CloseCitadelScreen();
			CloseJournalScreen();
			CloseWorldMap();
			if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this))
				L->ShowBanner(TEXT("BROOD SWARM"), TEXT("heading for the Citadel  ·  12 m NE"), FString());
			if (const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && Hud->Widget())
				Hud->Widget()->Toast(TEXT("SURVEY CHIP"), TEXT("A site marked in Prospect Landing"), FAcHudStyle::Cyan(), FPlatformTime::Seconds());
			const bool bOpen = I == 0 ? OpenSkillScreen() : I == 1 ? OpenRigScreen() : I == 2 ? OpenCitadelScreen() : I == 3 ? OpenJournalScreen()
																																	  : OpenWorldMap();
			Check(bOpen, FString::Printf(TEXT("u1: %s opens"), Says[I]));
			return;
		}
		if (Since() > 1.2) Shoot(*FString::Printf(TEXT("u1-%02d-screen-%s"), 5 + I, Names[I]), FString::Printf(TEXT("%s over a banner and toasts"), Says[I]));
		return;
	}
	case 2211:
		CloseWorldMap();
		Check(Mmo->DevClock(TEXT("brood clear\ndefence citadel")), TEXT("u1: the server stages a raid on the Citadel"));
		Advance();
		return;
	case 2212:
	{
		const std::optional<ac::net::Sky>& Sky = Mmo->Sky();
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr) Stop(Face(C->position));
		if (Sky && !Sky->raids.empty() && Since() > 2.0)
		{
			if (const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && Hud->Widget())
			{
				Hud->Widget()->Toast(TEXT("KSTR: STARFALL IN 60"), TEXT("Prospect Landing  ·  bearing 040°, 320 m"), FAcHudStyle::Amber(),
					FPlatformTime::Seconds());
			}
			U1Gain(24, 3);
			Advance();
			return;
		}
		TimedOut(TEXT("the raid told"), 15);
		return;
	}
	case 2213:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr) Stop(Face(C->position));
		if (Since() > 0.8) Shoot(TEXT("u1-10-raid"), TEXT("a raid on the Citadel: its warning, toasts beside it"));
		return;
	case 2214:
	{
		// A gatling gun ahead: U opens its card.
		if (U1Staged != T.Phase)
		{
			U1Staged = T.Phase;
			T.PhaseAt = Now;
			Check(Mmo->DevClock(TEXT("brood clear\ndefence gatling")), TEXT("u1: the server stages a gatling gun ahead"));
			return;
		}
		const ac::Structure* Gun = S && U ? NearestGatling(*S, U->position) : nullptr;
		if (Gun && Since() > 1.0 && !NearTurret())
		{
			// Up to it: within the card's reach.
			const ac::Vec2 Back = ac::normalize(U->position - Gun->position + ac::Vec2(0.01, 0));
			const ac::Vec2 To = Gun->position + Back * (ac::Rules::radius(Gun->kind) + 1.5);
			if (Shots.Route.Num() != 1 || FVector2D(To.x, To.y) != Shots.Route[0]) Route({FVector2D(To.x, To.y)});
			Drive();
		}
		else if (Gun && Since() > 1.0 && OpenTurretCard())
		{
			Stop(Face(Gun->position));
			if (Since() > 4.0) Shoot(TEXT("u1-11-screen-u"), TEXT("U: the gatling gun's card"));
			return;
		}
		TimedOut(TEXT("u1: the gun's card"), 30);
		return;
	}
	case 2215:
		CloseTurretCard();
		Check(Mmo->DevClock(TEXT("brood clear\nmatriarch here 55")), TEXT("u1: the server takes the pilot to the Matriarch's nest"));
		U1RoarAt = -1;
		Advance();
		return;
	case 2216:
		if (Here == TEXT("slag-pockets") && U && S && NearestBug(*S, U->position, ac::UnitKind::matriarch) && Since() > 2.0)
		{
			Check(Mmo->DevClock(TEXT("matriarch calm\nmatriarch near 20 -90")), TEXT("u1: the pilot walks up to her, her horde held"));
			Advance();
			return;
		}
		TimedOut(TEXT("in Slag Pockets, the Matriarch sent"), 40);
		return;
	case 2217:
	{
		const ac::Unit* Q = S && U ? NearestBug(*S, U->position, ac::UnitKind::matriarch) : nullptr;
		if (Q) Stop(Face(Q->position));
		if (Q && Q->bug && Q->bug->state == ac::Bug::State::roar && U1RoarAt < 0) U1RoarAt = Now;
		if (U1RoarAt > 0 && Now - U1RoarAt > 0.9)
		{
			const TSharedPtr<SAcProgressHud> W = UAcProgressHudSubsystem::Get(this) ? UAcProgressHudSubsystem::Get(this)->Widget() : nullptr;
			Check(W && W->Boss().IsSet(), TEXT("u1: the boss bar near her"));
			Shoot(TEXT("u1-12-boss"), TEXT("the Matriarch's roar: the boss bar under the compass, her plate clear"));
			return;
		}
		TimedOut(TEXT("her roar"), 20);
		return;
	}
	case 2218:
		Check(Mmo->DevClock(TEXT("matriarch down")), TEXT("u1: the Matriarch is felled"));
		Advance();
		return;
	case 2219:
	{
		const ac::Unit* Q = nullptr;
		if (S)
			for (const ac::Unit& X : S->units)
				if (X.kind == ac::UnitKind::matriarch && X.bug) Q = &X;
		if (Q && U) Stop(Face(Q->position));
		if (Q && Q->bug && Q->bug->state == ac::Bug::State::dead && Since() > 1.6)
		{
			Shoot(TEXT("u1-13-down"), TEXT("the Matriarch down: her banner in the slot, the bar felled"));
			return;
		}
		TimedOut(TEXT("her body"), 15);
		return;
	}
	case 2220:
		Mmo->DevClock(TEXT("matriarch back"));
		Check(Shots.Saved + Shots.Skipped == 13, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsK2
	case 1200:
		// The dev rig's home: the Prospector where it unloads at a Citadel.
		if (S && CitadelOf(*S) && Mmo->Progress() && Mmo->Progress()->board.size() == 6 && Since() > 3.0)
		{
			Check(AtRealmCitadel(), TEXT("k2: the pilot stands at a Citadel"));
			// A new pilot's board is drawn from its key's dice: an easy survey
			// or hold here only by chance (Q2). The server draws it again.
			Check(Mmo->DevClock(TEXT("missions here")), TEXT("k2: the server draws the board with an easy survey here"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("k2: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget())
			{
				W->Key(EKeys::Tab);
				W->Key(EKeys::Tab);
				Check(W->Tab() == SAcCitadelScreen::ETab::Missions, TEXT("k2: Tab twice shows the MISSIONS"));
			}
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel and the board"));
		return;
	case 1201:
	{
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!W || !P || Since() < 1.0) return;
		const FAcCitadelScreenInfo& I = W->GetInfo();
		int32 Tiers[3] = {0, 0, 0};
		for (const FAcMissionLine& X : I.Offers) Tiers[FMath::Clamp(X.Tier, 0, 2)] += 1;
		Check(I.Offers.Num() == 6 && Tiers[0] == 2 && Tiers[1] == 2 && Tiers[2] == 2,
			FString::Printf(TEXT("k2: the board offers %d: %d easy, %d hard, %d perilous"), I.Offers.Num(), Tiers[0], Tiers[1], Tiers[2]));
		Check(I.MissionsWhyNot.IsEmpty(), TEXT("k2: the board signs up here"));
		// The offer to take: an easy survey here (the server's `missions here`
		// puts one on), else an easy hold, else any survey or hold here; the
		// nearest first.
		K2Index = -1;
		double Best = 1e9;
		for (int32 Pass = 0; Pass < 3 && K2Index < 0; ++Pass)
			for (int32 X = 0; X < int32(P->board.size()); ++X)
			{
				const ac::net::CitadelMission& O = P->board[size_t(X)];
				const bool bSurvey = O.kind == ac::net::CitadelMission::Kind::survey;
				const bool bPoint = bSurvey || O.kind == ac::net::CitadelMission::Kind::hold;
				const bool bEasy = O.tier == ac::net::CitadelMission::Tier::easy;
				if (!bPoint || (Pass < 2 && !bEasy) || (Pass == 0 && !bSurvey) || Utf(O.region) != Here) continue;
				const double D = ac::distance(O.at, U->position);
				if (D < Best)
				{
					Best = D;
					K2Index = X;
				}
			}
		Check(K2Index >= 0, TEXT("k2: a survey or hold offer here to take"));
		if (K2Index < 0)
		{
			Finish();
			return;
		}
		K2Id = P->board[size_t(K2Index)].id;
		K2Key = Utf(P->board[size_t(K2Index)].key);
		K2Kept = Mmo->CitadelKept();
		Shoot(TEXT("k2-01-missions"), TEXT("the MISSIONS tab: the board, two offers a tier"));
		return;
	}
	case 1202:
	{
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
		Check(W && W->Key(Digits[K2Index]), FString::Printf(TEXT("k2: key %d signs up for %s"), K2Index + 1, *K2Key));
		Advance();
		return;
	}
	case 1203:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const ac::net::CitadelMission* Taken = nullptr;
		if (P)
			for (const ac::net::CitadelMission& X : P->journal)
				if (X.id == K2Id) Taken = &X;
		if (P && Taken && Mmo->CitadelKept() > K2Kept && NarrationKey == K2Key && Since() > 1.0)
		{
			Check(Taken->state == ac::net::CitadelMission::State::taken, TEXT("k2: in the Journal, taken"));
			Check(CitadelScreenWidget() && CitadelScreenWidget()->KeptText().StartsWith(TEXT("Signed up")), TEXT("k2: the foot says signed up"));
			Check(NarrationKey == K2Key, FString::Printf(TEXT("k2: the narration %s played on signing up"), *K2Key));
			// A1: the radio under it, at about -24 dB.
			if (const UAcMusicPlayer* Music = UAcMusicPlayer::Get(this))
				Check(Music->DuckGain() < 0.07, FString::Printf(TEXT("a1: the radio ducks under the narration (gain %.3f)"), Music->DuckGain()));
			Shoot(TEXT("k2-02-signed-up"), TEXT("signed up: the foot, the board one offer short"));
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("k2: sign up: %s"), *Mmo->CitadelRefusal()));
			Finish();
			return;
		}
		TimedOut(TEXT("the sign-up kept and narrated"), 10);
		return;
	}
	case 1204:
		CloseCitadelScreen();
		NarrationKey.Reset();
		K2DoneAt = -1;
		Check(OpenJournalScreen() && JournalScreenWidget().IsValid(), TEXT("k2: L opens the Journal"));
		Advance();
		return;
	case 1205:
	{
		const TSharedPtr<SAcJournalScreen> J = JournalScreenWidget();
		if (!J || Since() < 1.0) return;
		const FAcJournalInfo& I = J->GetInfo();
		Check(I.Entries.Num() == 1 && I.Entries[0].Id == K2Id, FString::Printf(TEXT("k2: the Journal holds the mission (%d)"), I.Entries.Num()));
		if (I.Entries.Num() == 1)
		{
			const FAcJournalEntry& E = I.Entries[0];
			Check(E.bMarked && !E.Briefing.IsEmpty() && !E.Instructions.IsEmpty() && !E.Reward.IsEmpty(),
				TEXT("k2: the entry has its briefing, instructions, reward and place"));
			Check(E.Narration == K2Key && E.Seconds > 5, FString::Printf(TEXT("k2: its narration %s, %.1f s"), *E.Narration, E.Seconds));
		}
		Check(J->Key(EKeys::P) && NarrationKey == K2Key, TEXT("k2: P plays the briefing again"));
		// A1: a second narration keeps it down (no bump between them).
		if (const UAcMusicPlayer* Music = UAcMusicPlayer::Get(this))
			Check(Music->DuckGain() < 0.07, FString::Printf(TEXT("a1: the radio stays down for the replay (gain %.3f)"), Music->DuckGain()));
		Shoot(TEXT("k2-03-journal"), TEXT("the Journal: briefing, what to do, the map, PLAY"));
		return;
	}
	case 1206:
	{
		CloseJournalScreen();
		// A1: the replay plays out, then the radio comes back to full within a second.
		if (const UAcMusicPlayer* Music = UAcMusicPlayer::Get(this))
		{
			if (!NarrationKey.IsEmpty() || Music->DuckGain() < 0.999)
			{
				TimedOut(TEXT("a1: the narration over and the radio back"), 30);
				return;
			}
			Check(true, FString::Printf(TEXT("a1: the narration over, the radio back at full (gain %.3f)"), Music->DuckGain()));
		}
		const ac::net::CitadelMission* Taken = nullptr;
		for (const ac::net::CitadelMission& X : Mmo->Progress()->journal)
			if (X.id == K2Id) Taken = &X;
		if (!Taken)
		{
			Check(false, TEXT("k2: the mission in the Journal"));
			Finish();
			return;
		}
		K2Target = Taken->at;
		K2Unsticks = 0;
		K2BackUntil = -1;
		// (The way there is found along the walking grid: the next phase.)
		Shots.Route.Reset();
		Shots.RouteAt = 0;
		Advance();
		return;
	}
	case 1207:
	{
		// Toward its place until it is in view, then a look at the markers.
		if (U && ac::distance(U->position, K2Target) < 16)
		{
			Stop(Face(K2Target));
			Advance();
			return;
		}
		if (K2Unstick()) return;
		if (Drive() == 1)
		{
			// Along the walking grid, as into the ring.
			TArray<FVector2D> Way;
			if (const ac::Simulation& Ms = M->simulation(); Ms.nav && U)
			{
				const auto P = Ms.nav->path(U->position, K2Target, 12.0);
				if (P)
					for (const ac::Vec2 Q : *P) Way.Add(FVector2D(Q.x, Q.y));
			}
			if (Way.IsEmpty()) Way.Add(FVector2D(K2Target.x, K2Target.y));
			Drivable(Way);
			Shots.Route = Way;
			Shots.RouteAt = 0;
			Shots.StallAt = U ? FVector2D(U->position.x, U->position.y) : FVector2D::ZeroVector;
			Shots.StallSince = Now;
		}
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the mission's place"));
		return;
	}
	case 1208:
		if (Since() > 1.5)
		{
			Check(AcMissions::Goals(*Mmo, U->position).Num() == 1, TEXT("k2: the mission's marker on the compass"));
			Shoot(TEXT("k2-04-marker-field"), TEXT("near the mission's place: its marker on the compass and the minimap"));
		}
		return;
	case 1209:
	{
		// Into the ring; a hold: the world's clock under the ringshadow band.
		const ac::net::CitadelMission* Taken = nullptr;
		for (const ac::net::CitadelMission& X : Mmo->Progress()->journal)
			if (X.id == K2Id) Taken = &X;
		if (!Taken)
		{
			TimedOut(TEXT("k2: the mission in the Journal"), 20);
			return;
		}
		// Done already (Q2: a survey's ring reached the marker picture's spot,
		// and its scan ran out there): on to its picture.
		if (Taken->state == ac::net::CitadelMission::State::done)
		{
			Advance();
			return;
		}
		if (U && ac::distance(U->position, K2Target) < FMath::Max(1.0, Taken->radius * 0.4))
		{
			Stop(Face(K2Target));
			if (Taken->kind == ac::net::CitadelMission::Kind::hold && Mmo->Sky() && M->simulation().map)
			{
				const ac::GroundRect& B = M->simulation().map->bounds;
				const ac::Ringshadow::Sweep Sw = ac::Ringshadow::next(Mmo->Sky()->time);
				double Under = -1;
				for (double At = Sw.start; At + 30 < Sw.end && Under < 0; At += 0.5)
					if (ac::Ringshadow::cover(At, K2Target, B) >= 1 && ac::Ringshadow::cover(At + 30, K2Target, B) >= 1) Under = At;
				Check(Under > 0 && Mmo->DevClock(FString::Printf(TEXT("clock %.1f"), Under)), TEXT("k2: the clock moved under the band"));
			}
			Advance();
			return;
		}
		if (K2Unstick()) return;
		if (Drive() == 1)
		{
			// Along the walking grid (a ridge or a wall may stand on the
			// straight line) to free ground inside the ring (its centre may be
			// a deposit, solid); straight at the centre only with no way.
			// The spot: the centre, else the nearest ground the Prospector
			// stands on well inside the ring (Q2: a centre in a deposit left
			// the route ending short of it).
			const double Inside = FMath::Max(1.0, Taken->radius * 0.4) - 0.3;
			ac::Vec2 Goal = K2Target;
			if (const ac::Simulation& Ms = M->simulation(); U && !Ms.pilotCanStand(*U, K2Target))
			{
				double Near = 1e9;
				for (double R = 0.5; R <= Inside; R += 0.25)
					for (int32 K = 0; K < 24; ++K)
					{
						const double A = K * 2.0 * PI / 24;
						const ac::Vec2 C = K2Target + ac::Vec2(std::cos(A), std::sin(A)) * R;
						if (Ms.pilotCanStand(*U, C) && ac::distance(C, U->position) < Near)
						{
							Near = ac::distance(C, U->position);
							Goal = C;
						}
					}
			}
			TArray<FVector2D> Way;
			if (const ac::Simulation& Ms = M->simulation(); Ms.nav && U)
				if (const auto P = Ms.nav->path(U->position, Goal, 0.5))
					for (const ac::Vec2 Q : *P) Way.Add(FVector2D(Q.x, Q.y));
			Way.Add(FVector2D(Goal.x, Goal.y));
			Drivable(Way);
			Shots.Route = Way;
			Shots.RouteAt = 0;
			Shots.StallAt = U ? FVector2D(U->position.x, U->position.y) : FVector2D::ZeroVector;
			Shots.StallSince = Now;
		}
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("into the mission's ring"));
		// A route that ends short of the ring's middle is planned again each
		// time and never stalls (Q2: a run waited 15 min): a time limit.
		TimedOut(TEXT("into the mission's ring"), 60);
		return;
	}
	case 1210:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const ac::net::CitadelMission* Taken = nullptr;
		if (P)
			for (const ac::net::CitadelMission& X : P->journal)
				if (X.id == K2Id) Taken = &X;
		if (Taken && Taken->state == ac::net::CitadelMission::State::done && K2DoneAt < 0) K2DoneAt = Now;
		if (Taken && Taken->state == ac::net::CitadelMission::State::done && Now - K2DoneAt > 1.2)
		{
			Check(true, FString::Printf(TEXT("k2: the mission done: %lld / %lld"), (long long)Taken->have, (long long)Taken->need));
			Shoot(TEXT("k2-05-mission-done"), TEXT("the mission done in its ring: the MISSION DONE toast"));
			return;
		}
		TimedOut(TEXT("the mission done"), 45);
		return;
	}
	case 1211:
		// A1: the narration over, the radio back at full.
		if (const UAcMusicPlayer* Music = UAcMusicPlayer::Get(this); Music && NarrationKey.IsEmpty())
			Check(Music->DuckGain() > 0.999, FString::Printf(TEXT("a1: the radio back at full after the narration (gain %.3f)"), Music->DuckGain()));
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsN2
	case 1300:
		// The dev rig's home; out from the Citadel's walls onto open ground.
		if (S && U && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0)
		{
			// Out from the Citadel, then 10 cells short of the deposit nearest
			// there: the lingering group's site in view, the pilot outside it.
			const ac::Vec2 C = CitadelOf(*S)->position;
			const ac::Vec2 Out = ac::normalize(U->position - C + ac::Vec2(0.01, 0));
			const ac::Vec2 To = C + Out * 13.0;
			const ac::OreDeposit* D = nullptr;
			for (const ac::OreDeposit& X : S->patches)
				if (X.remaining > 0 && ac::distance(X.position, C) > 16 && (!D || ac::distance(X.position, To) < ac::distance(D->position, To))) D = &X;
			const ac::Vec2 Near = D ? D->position + ac::normalize(To - D->position + ac::Vec2(0.01, 0)) * 10.0 : To;
			Route({FVector2D(To.x, To.y), FVector2D(Near.x, Near.y)});
			// Noon, unless -AcMmoHour says (Q2: on the world clock, a night
			// run's sight was half, and lingering bugs fell out of it).
			if (!HourGiven()) Check(Mmo->DevClock(TEXT("hour 12")), TEXT("n2: noon"));
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's home"));
		return;
	case 1301:
	{
		// Walk out, then stage the Shardlings lingering at a deposit in view.
		if (!N2Staged)
		{
			const int32 D = Drive();
			if (D < 0) { Stalled(TEXT("n2: out from the Citadel")); return; }
			if (D == 0) return;
			Check(Mmo->DevClock(TEXT("brood linger")), TEXT("n2: the server stages Shardlings at a deposit in view"));
			N2Staged = true;
			T.PhaseAt = Now;
			return;
		}
		const ac::Unit* B = NearestBug(*S, U->position, ac::UnitKind::shardling);
		if (B) Stop(Face(B->position));
		if (B && Since() > 5.0)
		{
			int32 Calm = 0;
			for (const ac::Unit& X : S->units)
				if (X.kind == ac::UnitKind::shardling && X.bug && (X.bug->state == ac::Bug::State::idle || X.bug->state == ac::Bug::State::move)) Calm++;
			Check(Calm >= 3, FString::Printf(TEXT("n2: %d Shardlings lingering, %.1f cells off"), Calm, ac::distance(B->position, U->position)));
			Shoot(TEXT("n2-01-linger"), TEXT("Shardlings lingering at a deposit"));
			return;
		}
		TimedOut(TEXT("the Shardlings in sight"), 20);
		return;
	}
	case 1302:
		// One control file a tick: the clear and the next scene together.
		Check(Mmo->DevClock(TEXT("brood clear\nbrood attack")), TEXT("n2: the server stages Shardlings on the pilot"));
		N2Hp = U->hp;
		Advance();
		return;
	case 1303:
	{
		// Turned half away: biting at its side, they show beside it from the chase eye.
		const ac::Unit* B = NearestBug(*S, U->position, ac::UnitKind::shardling);
		if (B && Since() < 0.5) Stop(Face(B->position) + 1.0);
		int32 On = 0;
		for (const ac::Unit& X : S->units)
			if (X.bug && X.hp > 0 && X.target == U->id) On++;
		if (B && On >= 2 && U->hp < N2Hp - 1 && Since() > 1.5)
		{
			Check(true, FString::Printf(TEXT("n2: %d Shardlings bite the Prospector: %.0f -> %.0f hp"), On, N2Hp, U->hp));
			Shoot(TEXT("n2-02-attack"), TEXT("Shardlings attacking the Prospector"));
			return;
		}
		TimedOut(TEXT("the Shardlings biting"), 20);
		return;
	}
	case 1304:
		// One control file a tick: the clear and the next scene together.
		Check(Mmo->DevClock(TEXT("brood clear\nbrood dive")), TEXT("n2: the server stages Gloamwings on the pilot"));
		Advance();
		return;
	case 1305:
	{
		const ac::Unit* G = nullptr;
		for (const ac::Unit& X : S->units)
			if (X.kind == ac::UnitKind::gloamwing && X.bug && X.bug->state == ac::Bug::State::dive) G = &X;
		if (const ac::Unit* B = G ? G : NearestBug(*S, U->position, ac::UnitKind::gloamwing)) Stop(Face(B->position));
		if (G && G->bug->height <= 1.0 && Since() > 1.0)
		{
			Check(G->bug->height < 1.5, FString::Printf(TEXT("n2: a Gloamwing dives to %.1f cells"), G->bug->height));
			Shoot(TEXT("n2-03-dive"), TEXT("a Gloamwing diving at the Prospector"));
			return;
		}
		TimedOut(TEXT("a Gloamwing's dive"), 25);
		return;
	}
	case 1306:
		// One control file a tick: the clear and the next scene together.
		Check(Mmo->DevClock(TEXT("brood clear\nbrood lodeback")), TEXT("n2: the server stages a full Lodeback leaving"));
		Advance();
		return;
	case 1307:
	{
		const ac::Unit* L = NearestBug(*S, U->position, ac::UnitKind::lodeback);
		if (L) Stop(Face(L->position));
		if (L && Since() > 2.0)
		{
			Check(L->bug->sac > ac::Brood::sacFull * 0.9 && L->bug->state == ac::Bug::State::move,
				FString::Printf(TEXT("n2: a Lodeback leaving with %.0f ore in its sac"), L->bug->sac));
			Shoot(TEXT("n2-04-lodeback"), TEXT("a Lodeback with a full sac, leaving"));
			return;
		}
		TimedOut(TEXT("the Lodeback in sight"), 20);
		return;
	}
	case 1308:
		// One control file a tick: the clear and the next scene together.
		Check(Mmo->DevClock(TEXT("brood clear\nbrood raid")), TEXT("n2: the server stages a raid on a Hab Dome"));
		N2Dome = -1;
		Advance();
		return;
	case 1309:
	{
		const std::optional<ac::net::Sky>& Sky = Mmo->Sky();
		if (Sky && !Sky->raids.empty() && Since() > 0.5)
		{
			N2Dome = Sky->raids.front().building;
			Check(Sky->raids.front().kind == ac::StructureKind::habDome, TEXT("n2: the raid is told: a Hab Dome"));
			Advance();
			return;
		}
		TimedOut(TEXT("the raid told"), 15);
		return;
	}
	case 1310:
	{
		const ac::Structure* D = nullptr;
		for (const ac::Structure& X : S->structures)
			if (X.id == N2Dome) D = &X;
		// The dome left of the middle: the KSTR toast, still up, is in the middle.
		if (D) Stop(Face(D->position) - 0.45);
		if (D && D->hp < ac::Rules::hp(ac::StructureKind::habDome) - 8 && Since() > 0.8)
		{
			Check(true, FString::Printf(TEXT("n2: the raided Hab Dome at %.0f of %.0f hp"), D->hp, ac::Rules::hp(ac::StructureKind::habDome)));
			Shoot(TEXT("n2-05-raid"), TEXT("a raid on a Hab Dome, its health falling"));
			return;
		}
		TimedOut(TEXT("the Hab Dome bitten"), 40);
		return;
	}
	case 1311:
		Mmo->DevClock(TEXT("brood clear"));
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsD
	case 1600:
		// The dev rig's home: the realm wallet filled, turret blueprints given,
		// the Citadel screen's DEFENCE tab open.
		if (S && U && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("defence wallet\ndefence blueprints")), TEXT("d: the server fills the realm wallet and gives turret blueprints"));
			Check(AtRealmCitadel(), TEXT("d: the pilot stands at a Citadel"));
			Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("d: H opens the Citadel screen"));
			if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget())
			{
				for (int32 K = 0; K < SAcCitadelScreen::TabCount && W->Tab() != SAcCitadelScreen::ETab::Defence; ++K) W->Key(EKeys::Tab);
				Check(W->Tab() == SAcCitadelScreen::ETab::Defence, TEXT("d: Tab reaches the DEFENCE"));
			}
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel"));
		return;
	case 1601:
	{
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (W && P && P->realmWallet.ore >= 3000 && W->GetInfo().Defence.Num() == 3 && Since() > 1.0)
		{
			Check(W->GetInfo().DefenceWhyNot.IsEmpty(), TEXT("d: the turrets can be upgraded here"));
			DKept = Mmo->CitadelKept();
			DRealmOre = P->realmWallet.ore;
			Check(W->Key(EKeys::One), TEXT("d: key 1 upgrades the turrets' fire rate"));
			Advance();
			return;
		}
		TimedOut(TEXT("the DEFENCE tab filled"), 15);
		return;
	}
	case 1602:
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && Mmo->CitadelKept() > DKept && P->citadelMarks.rate == 1 && Since() > 1.0)
		{
			const ac::ShopPrice Price = ac::Defence::citadelPrice(ac::DefenceStat::rate, 1);
			Check(P->realmWallet.ore == DRealmOre - Price.ore,
				FString::Printf(TEXT("d: the realm wallet paid: %lld -> %lld ore"), (long long)DRealmOre, (long long)P->realmWallet.ore));
			Check(CitadelScreenWidget() && !CitadelScreenWidget()->KeptText().IsEmpty(), TEXT("d: the foot says what was upgraded"));
			Shoot(TEXT("d-02-defence"), TEXT("the DEFENCE tab after a fire-rate mark from the realm wallet"));
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("d: upgrade: %s"), *Mmo->CitadelRefusal()));
			Advance();
			return;
		}
		TimedOut(TEXT("the fire-rate mark kept"), 10);
		return;
	}
	case 1603:
	{
		CloseCitadelScreen();
		// Out 13 cells from the Citadel: it fills the view, the raid comes past.
		const ac::Vec2 C = CitadelOf(*S)->position;
		const ac::Vec2 Out = ac::normalize(U->position - C + ac::Vec2(0.01, 0));
		const ac::Vec2 To = C + Out * 13.0;
		Route({FVector2D(To.x, To.y)});
		DStaged = false;
		Advance();
		return;
	}
	case 1604:
	{
		if (!DStaged)
		{
			const int32 D = Drive();
			if (D < 0) { Stalled(TEXT("d: out from the Citadel")); return; }
			if (D == 0) return;
			Check(Mmo->DevClock(TEXT("defence citadel")), TEXT("d: the server stages a raid on the Citadel"));
			DStaged = true;
			T.PhaseAt = Now;
			return;
		}
		// The Citadel right of the middle: the raid's KSTR toast is in the middle.
		const ac::Structure* C = CitadelOf(*S);
		if (C) Stop(Face(C->position) - 0.35);
		int32 Hurt = 0, Near = 0;
		for (const ac::Unit& X : S->units)
			if (X.bug)
			{
				Hurt += X.hp < ac::Rules::hp(X.kind) ? 1 : 0;
				Near += C && X.hp > 0 && ac::distance(X.position, C->position) < 12 ? 1 : 0;
			}
		if (C && C->marks && Hurt >= 2 && Near >= 3 && Since() > 2.0)
		{
			Check(true, FString::Printf(TEXT("d: the corner turrets fire: %d bugs hit, %d close"), Hurt, Near));
			Shoot(TEXT("d-01-citadel"), TEXT("the Citadel's corner turrets firing at a raid"));
			return;
		}
		TimedOut(TEXT("the Citadel firing at the raid"), 30);
		return;
	}
	case 1605:
	{
		// Out past the corner turrets' reach (34 cells), facing away: the
		// turrets placed ahead fight alone.
		Mmo->DevClock(TEXT("brood clear"));
		const ac::Vec2 C = CitadelOf(*S)->position;
		const ac::Vec2 Out = ac::normalize(U->position - C + ac::Vec2(0.01, 0));
		const ac::Vec2 To = C + Out * 34.0;
		Route({FVector2D(To.x, To.y)});
		DStaged = false;
		Advance();
		return;
	}
	case 1606:
	{
		if (!DStaged)
		{
			// A stall short of the spot is far enough out: the scene goes on there.
			const int32 D = Drive();
			if (D == 0) return;
			if (D < 0) Stop(U->heading);
			Check(Mmo->DevClock(TEXT("defence aa")), TEXT("d: the server stages an AA tower and Gloamwings"));
			DStaged = true;
			T.PhaseAt = Now;
			return;
		}
		const ac::Structure* A = nullptr;
		for (const ac::Structure& X : S->structures)
			if (X.kind == ac::StructureKind::sentinel && (!A || ac::distance(X.position, U->position) < ac::distance(A->position, U->position))) A = &X;
		const ac::Unit* G = NearestBug(*S, A ? A->position : U->position, ac::UnitKind::gloamwing);
		// The tower and its flyers both in view: a little toward the flyers.
		if (A) Stop(Face(G ? A->position * 0.7 + G->position * 0.3 : A->position));
		int32 Hit = 0;
		for (const ac::Unit& X : S->units)
			if (X.kind == ac::UnitKind::gloamwing && X.bug && X.hp < ac::Rules::hp(X.kind)) Hit++;
		if (A && Hit >= 1 && Since() > 1.5)
		{
			Check(true, FString::Printf(TEXT("d: the AA tower hits %d Gloamwings"), Hit));
			Shoot(TEXT("d-03-aa"), TEXT("an AA tower downing a Gloamwing"));
			return;
		}
		TimedOut(TEXT("the AA tower firing"), 30);
		return;
	}
	case 1607:
		Check(Mmo->DevClock(TEXT("brood clear\ndefence gatling")), TEXT("d: the server stages a gatling gun and Shardlings"));
		Advance();
		return;
	case 1608:
	{
		const ac::Structure* Gun = nullptr;
		for (const ac::Structure& X : S->structures)
			if (X.kind == ac::StructureKind::gatling && (!Gun || ac::distance(X.position, U->position) < ac::distance(Gun->position, U->position))) Gun = &X;
		const ac::Unit* B = NearestBug(*S, Gun ? Gun->position : U->position, ac::UnitKind::shardling);
		// The gun left of the middle: the raid's KSTR toast is in the middle.
		if (Gun) Stop(Face(B ? (Gun->position * 2.0 + B->position) / 3.0 : Gun->position) + 0.4);
		int32 Hit = 0;
		for (const ac::Unit& X : S->units)
			if (X.kind == ac::UnitKind::shardling && X.bug && X.hp < ac::Rules::hp(X.kind)) Hit++;
		int32 Close = 0, Alive = 0;
		for (const ac::Unit& X : S->units)
			if (X.kind == ac::UnitKind::shardling && X.bug && X.hp > 0)
			{
				Alive++;
				Close += Gun && ac::distance(X.position, Gun->position) < 7 ? 1 : 0;
			}
		// The dead leave the mirror at once: firing is a target and the pack
		// in reach, some of it hurt or already gone (six were set on it).
		if (Gun && Gun->target && Close >= 2 && (Hit >= 1 || Alive < 6) && Since() > 0.8)
		{
			Check(true, FString::Printf(TEXT("d: the gatling gun fires: %d Shardlings in reach, %d hurt, %d left"), Close, Hit, Alive));
			Shoot(TEXT("d-04-gatling"), TEXT("a gatling gun firing amalgam rounds at Shardlings"));
			return;
		}
		TimedOut(TEXT("the gatling gun firing"), 30);
		return;
	}
	case 1609:
	{
		Mmo->DevClock(TEXT("brood clear"));
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("d: B opens the build menu"));
		Advance();
		return;
	}
	case 1610:
		if (Since() > 1.0)
		{
			Check(AcProgressHud::Copies(Mmo, ac::StructureKind::gatling) > 0 && AcProgressHud::Copies(Mmo, ac::StructureKind::sentinel) > 0,
				TEXT("d: the build menu holds gatling gun and AA tower copies"));
			Check(AcPilotText::BuildKinds(true).Contains(ac::StructureKind::gatling), TEXT("d: the gatling gun is in the build menu"));
			Shoot(TEXT("d-05-build-menu"), TEXT("the build menu with the gatling gun and the AA tower"));
		}
		return;
	case 1611:
		if (UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this)) Build->Key(TEXT("esc"));
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsD3
	case 1800:
		// The dev rig's home: the pilot's wallet filled; out past the corner
		// turrets' reach (34 cells), where a gun fights alone.
		if (S && U && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0)
		{
			Check(Mmo->DevClock(TEXT("defence purse")), TEXT("d3: the server fills the pilot's wallet"));
			const ac::Vec2 C = CitadelOf(*S)->position;
			const ac::Vec2 Out = ac::normalize(U->position - C + ac::Vec2(0.01, 0));
			const ac::Vec2 To = C + Out * 34.0;
			Route({FVector2D(To.x, To.y)});
			DStaged = false;
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig's Citadel"));
		return;
	case 1801:
	{
		if (!DStaged)
		{
			const int32 D = Drive();
			if (D == 0) return;
			if (D < 0) Stop(U->heading);
			Check(Mmo->DevClock(TEXT("brood clear\ndefence air")), TEXT("d3: the server stages a gatling gun and Gloamwings beyond its air reach"));
			DStaged = true;
			T.PhaseAt = Now;
			return;
		}
		const ac::Structure* Gun = NearestGatling(*S, U->position);
		const ac::Unit* G = Gun ? NearestBug(*S, Gun->position, ac::UnitKind::gloamwing) : nullptr;
		if (Gun) Stop(Face(G ? Gun->position * 0.75 + G->position * 0.25 : Gun->position));
		int32 Hit = 0;
		for (const ac::Unit& X : S->units)
			if (X.kind == ac::UnitKind::gloamwing && X.bug && X.hp < ac::Rules::hp(X.kind)) Hit++;
		if (Gun && G && Since() > 3.0)
		{
			Check(!Gun->marks.has_value(), TEXT("d3: the gun starts bare"));
			FString Gaps;
			for (const ac::Unit& X : S->units)
				if (X.kind == ac::UnitKind::gloamwing && X.bug)
					Gaps += FString::Printf(TEXT(" %.2f/%d/%.0f"), ac::distance(X.position, Gun->position) - ac::Rules::radius(X.kind) - ac::Rules::radius(Gun->kind),
						int32(X.bug->state), X.hp);
			Check(Hit == 0, FString::Printf(TEXT("d3: the bare gun can't reach the Gloamwings (%d hit; gap/state/hp:%s)"), Hit, *Gaps));
			Shoot(TEXT("d3-01-before"), TEXT("a bare gatling gun: its chevrons dark, Gloamwings beyond its air reach"));
			return;
		}
		TimedOut(TEXT("the gatling gun and its Gloamwings"), 30);
		return;
	}
	case 1802:
		Check(NearTurret().has_value(), TEXT("d3: a turret is near"));
		Check(OpenTurretCard() && TurretCardWidget().IsValid(), TEXT("d3: U opens the turret card"));
		D3Step = 0;
		D3Kept = Mmo->TurretKept();
		Advance();
		return;
	case 1803:
	{
		// Air reach to its last mark, a mark of fire rate, two of damage, one of range.
		static const FKey Presses[] = {EKeys::Four, EKeys::Four, EKeys::Four, EKeys::One, EKeys::Two, EKeys::Two, EKeys::Three};
		const TSharedPtr<SAcTurretCard> W = TurretCardWidget();
		if (!W)
		{
			Check(false, TEXT("d3: the turret card stays open"));
			Advance();
			return;
		}
		if (!Mmo->TurretRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("d3: upgrade: %s"), *Mmo->TurretRefusal()));
			Advance();
			return;
		}
		if (D3Step == 0 && Since() > 1.0)
		{
			Check(W->GetInfo().Lines.Num() == 4, FString::Printf(TEXT("d3: the card lists four stats (%d)"), W->GetInfo().Lines.Num()));
			Check(W->GetInfo().WhyNot.IsEmpty(), TEXT("d3: the gun can be upgraded from here"));
		}
		if (D3Step < int32(UE_ARRAY_COUNT(Presses)) && Mmo->TurretKept() == D3Kept + D3Step && Since() > 1.0 + 0.4 * D3Step)
		{
			W->Key(Presses[D3Step]);
			D3Step++;
			return;
		}
		if (D3Step == int32(UE_ARRAY_COUNT(Presses)) && Mmo->TurretKept() == D3Kept + D3Step && Since() > 1.0 + 0.4 * D3Step + 1.0)
		{
			const ac::Structure* Gun = NearestGatling(*S, U->position);
			const ac::TurretMarks Want{.rate = 1, .damage = 2, .range = 1, .air = 3};
			Check(Gun && Gun->marks == Want, TEXT("d3: the gun carries its marks (rate 1, damage 2, range 1, air 3)"));
			Check(W->GetInfo().Lines.Num() == 4 && W->GetInfo().Lines[3].Mark == 3, TEXT("d3: the card shows air reach at its last mark"));
			Check(!W->KeptText().IsEmpty(), TEXT("d3: the foot says what was upgraded"));
			Shoot(TEXT("d3-02-card"), TEXT("the turret card after marks from the pilot's wallet"));
			return;
		}
		TimedOut(TEXT("the marks kept"), 30);
		return;
	}
	case 1804:
	{
		// From where the bare gun was shot, the toasts gone: its chevrons lit.
		CloseTurretCard();
		const ac::Structure* Gun = NearestGatling(*S, U->position);
		if (Gun) Stop(Face(Gun->position) + 0.2);
		if (Gun && Since() > 7.0)
		{
			Check(Gun->marks && Gun->marks->total() == 7, TEXT("d3: the gun's seven marks lit on its base"));
			Shoot(TEXT("d3-03-after"), TEXT("the gatling gun with its marks: the chevrons lit on its base"));
			return;
		}
		TimedOut(TEXT("the marked gun"), 15);
		return;
	}
	case 1805:
		Advance();
		return;
	case 1806:
		// Fresh Gloamwings at 5 to 5.8 cells: the first ones were shot down
		// while the card was open.
		Check(Mmo->DevClock(TEXT("brood clear\ndefence flyers")), TEXT("d3: the server sends Gloamwings at the gun's new air reach"));
		Advance();
		return;
	case 1807:
	{
		const ac::Structure* Gun = NearestGatling(*S, U->position);
		const ac::Unit* G = Gun ? NearestBug(*S, Gun->position, ac::UnitKind::gloamwing) : nullptr;
		if (Gun) Stop(Face(G ? (Gun->position + G->position) * 0.5 : Gun->position));
		// A Gloamwing hurt while it hovers beyond the bare reach: the new air reach.
		const ac::Unit* Far = nullptr;
		for (const ac::Unit& X : S->units)
		{
			if (!Gun || X.kind != ac::UnitKind::gloamwing || !X.bug || X.hp <= 0 || X.hp >= ac::Rules::hp(X.kind)) continue;
			const double Gap = ac::distance(X.position, Gun->position) - ac::Rules::radius(X.kind) - ac::Rules::radius(Gun->kind);
			if (Gap > ac::UnitStats::gatling.airRange) Far = &X;
		}
		if (Since() > 1.0 && int32(Since()) != D3Logged)
		{
			D3Logged = int32(Since());
			FString Gaps;
			for (const ac::Unit& X : S->units)
				if (Gun && X.kind == ac::UnitKind::gloamwing && X.bug)
					Gaps += FString::Printf(TEXT(" %.2f/%d/%.0f"), ac::distance(X.position, Gun->position) - ac::Rules::radius(X.kind) - ac::Rules::radius(Gun->kind),
						int32(X.bug->state), X.hp);
			UE_LOG(LogAutocraft, Log, TEXT("mmoshots: d3: gloamwings gap/state/hp:%s"), *Gaps);
		}
		if (Gun && Far && Gun->target && Since() > 0.3)
		{
			const double Gap = ac::distance(Far->position, Gun->position) - ac::Rules::radius(Far->kind) - ac::Rules::radius(Gun->kind);
			Check(true, FString::Printf(TEXT("d3: the marked gun hits a Gloamwing %.2f cells from its edge (bare reach %.1f)"), Gap,
				ac::UnitStats::gatling.airRange));
			Shoot(TEXT("d3-04-air-reach"), TEXT("the marked gatling gun hitting a Gloamwing at its new air reach"));
			return;
		}
		TimedOut(TEXT("the marked gun hitting a Gloamwing"), 30);
		return;
	}
	case 1808:
		Mmo->DevClock(TEXT("brood clear"));
		Advance();
		return;
	case 1809:
		Check(Shots.Saved + Shots.Skipped == 4, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsN3
	case 1900:
		// Taken to Slag Pockets, 55 cells from the nest.
		if (S && U && Mmo->Progress() && Since() > 3.0)
		{
			// Noon, unless -AcMmoHour says (Q2: on the world clock, a night
			// run's sight was half, and her horde out of it).
			Check(Mmo->DevClock(HourGiven() ? TEXT("matriarch here 55") : TEXT("hour 12\nmatriarch here 55")),
				TEXT("n3: the server takes the pilot to the Matriarch's nest"));
			Advance();
			return;
		}
		TimedOut(TEXT("the dev rig"));
		return;
	case 1901:
		if (Here == TEXT("slag-pockets") && U && S && NearestBug(*S, U->position, ac::UnitKind::matriarch))
		{
			// Her horde held still and her spit held, for the pictures.
			Check(Mmo->DevClock(TEXT("matriarch calm")), TEXT("n3: the horde held still"));
			Advance();
			return;
		}
		TimedOut(TEXT("in Slag Pockets, the Matriarch sent"), 40);
		return;
	case 1902:
	{
		const ac::Unit* Q = NearestBug(*S, U->position, ac::UnitKind::matriarch);
		if (Q) Stop(Face(Q->position));
		if (Q && Since() > 6.0)
		{
			const double Far = ac::distance(U->position, Q->position);
			Check(Far > 40.0, FString::Printf(TEXT("n3: she is sent from afar (%.0f cells off)"), Far));
			const TSharedPtr<SAcProgressHud> W = UAcProgressHudSubsystem::Get(this) ? UAcProgressHudSubsystem::Get(this)->Widget() : nullptr;
			Check(W && !W->Boss().IsSet(), TEXT("n3: no boss bar from afar"));
			// The nest's mound drawn where the map puts it (the region's
			// ground set, W4b: built after the pilot left the first region).
			{
				const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
				TOptional<ac::Vec2> Nest;
				for (const ac::Doodad& D : Sim->Map().doodads)
					if (D.kind == ac::Doodad::Kind::broodNest) Nest = D.position;
				const TOptional<FVector> Drawn = Terrain ? Terrain->DrawnAt(ac::Doodad::Kind::broodNest) : TOptional<FVector>();
				const double Off = Nest && Drawn ? FVector::Dist2D(*Drawn, AcSpace::ToWorld(*Nest)) / AcSpace::CmPerCell : -1.0;
				Check(Off >= 0 && Off < 0.5, FString::Printf(TEXT("n3: the nest's mound drawn at the nest (%.2f cells off)"), Off));
			}
			Shoot(TEXT("n3-01-nest-afar"), TEXT("the Matriarch's nest from afar: her crown over its rim"));
			return;
		}
		TimedOut(TEXT("the nest from afar"), 20);
		return;
	}
	case 1903:
		// Within 25 cells: she roars at the pilot on first sight.
		Check(Mmo->DevClock(TEXT("matriarch near 20 -90")), TEXT("n3: the pilot walks up to her"));
		N3RoarAt = -1;
		Advance();
		return;
	case 1904:
	{
		const ac::Unit* Q = S ? NearestBug(*S, U ? U->position : ac::Vec2(), ac::UnitKind::matriarch) : nullptr;
		if (Q && U) Stop(Face(Q->position));
		// The roar's age on the server's clock: the picture 0.9 s into it, or
		// at once when the mirror shows it later than that (it lasts 2 s).
		const std::optional<ac::net::Sky>& Heard = Mmo->Sky();
		const double Age = Heard && !Heard->roars.empty() ? Heard->time - Heard->roars.back().at : -1;
		if (Q && Q->bug && Q->bug->state == ac::Bug::State::roar && N3RoarAt < 0)
		{
			N3RoarAt = Now;
			UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmoshots: n3: her roar first seen %.2f s into it"), Age);
		}
		const bool bStillRoaring = Q && Q->bug && Q->bug->state == ac::Bug::State::roar;
		if (N3RoarAt > 0 && (Now - N3RoarAt > 0.9 || (Age >= 0.9 && bStillRoaring)))
		{
			Check(Mmo->Sky() && !Mmo->Sky()->roars.empty(), TEXT("n3: the roar is told on the wire"));
			int32 Horde = 0;
			for (const ac::Unit& X : S->units)
				if (X.bug && X.kind != ac::UnitKind::matriarch && X.hp > 0) Horde++;
			Check(Horde >= 1, FString::Printf(TEXT("n3: her horde round her (%d bugs in view)"), Horde));
			const TSharedPtr<SAcProgressHud> W = UAcProgressHudSubsystem::Get(this) ? UAcProgressHudSubsystem::Get(this)->Widget() : nullptr;
			Check(W && W->Boss().IsSet() && W->Boss()->bRoaring, TEXT("n3: the boss bar near her, flaring with the roar"));
			Shoot(TEXT("n3-02-roar"), TEXT("the Matriarch roars on first sight, her horde round her"));
			return;
		}
		TimedOut(TEXT("her roar"), 20);
		return;
	}
	case 1905:
	{
		// -AcMmoShotsN3Perf=S: the frame and GPU times over S s first, 10 cells from her, her horde held still in sight.
		double PerfFor = 0;
		FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsN3Perf="), PerfFor);
		if (PerfFor > 0 && N3FrameAt != -2)
		{
			if (N3FrameAt < 0 && N3Frames.Num() == 0 && Since() < 0.5)
			{
				if (N3FrameAt == -1) Check(Mmo->DevClock(TEXT("matriarch near 10 -90\nmatriarch calm")), TEXT("n3: the pilot amid the held horde, for the frame times"));
				N3FrameAt = -3;
			}
			const ac::Unit* Q = S && U ? NearestBug(*S, U->position, ac::UnitKind::matriarch) : nullptr;
			if (Q) Stop(Face(Q->position));
			if (Since() > 1.0)
			{
				if (N3FrameAt > 0)
				{
					N3Frames.Add((Now - N3FrameAt) * 1000.0);
					N3Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
				}
				N3FrameAt = Now;
			}
			if (Since() < 1.0 + PerfFor) return;
			int32 Bugs = 0;
			if (S)
				for (const ac::Unit& X : S->units)
					if (X.bug && X.hp > 0) Bugs++;
			auto Pct = [](TArray<double> V, const double P) { V.Sort(); return V.Num() ? V[FMath::Clamp(int32(P * V.Num()), 0, V.Num() - 1)] : 0.0; };
			double Sum = 0;
			for (const double F : N3Frames) Sum += F;
			UE_LOG(LogAutocraft, Log, TEXT("n3 perf: %d bugs sent to the pilot, %d frames over %.0f s: frame mean %.2f p50 %.2f p95 %.2f ms, gpu p50 %.2f p95 %.2f ms"),
				Bugs, N3Frames.Num(), PerfFor, N3Frames.Num() ? Sum / N3Frames.Num() : 0.0, Pct(N3Frames, 0.5), Pct(N3Frames, 0.95), Pct(N3Gpu, 0.5), Pct(N3Gpu, 0.95));
			N3FrameAt = -2;
			T.PhaseAt = Now;
			return;
		}
		// In front of her: her slam.
		Check(Mmo->DevClock(TEXT("matriarch near 6.2 -90\nmatriarch slam")), TEXT("n3: the pilot in front of her, her slam"));
		N3SlamAt = -1;
		N3Strikes = -1;
		Advance();
		return;
	}
	case 1906:
	{
		const ac::Unit* Q = S ? NearestBug(*S, U ? U->position : ac::Vec2(), ac::UnitKind::matriarch) : nullptr;
		if (Q && U) Stop(Face(Q->position));
		if (Q && Q->bug)
		{
			if (N3Strikes < 0) N3Strikes = Q->bug->strikes;
			if (Q->bug->strikes != N3Strikes && N3SlamAt < 0 && ac::Matriarch::slammed(Q->bug->sac)) N3SlamAt = Now;
			N3Strikes = Q->bug->strikes;
		}
		if (N3SlamAt > 0 && Now - N3SlamAt > ac::Matriarch::slamWindup + 0.06)
		{
			Check(Here == TEXT("slag-pockets"), TEXT("n3: the pilot still at the nest"));
			Shoot(TEXT("n3-03-slam"), TEXT("the Matriarch's slam crashing down in front of the pilot"));
			return;
		}
		TimedOut(TEXT("her slam"), 20);
		return;
	}
	case 1907:
		// Back off; she is wounded: the boss bar shows her life.
		Check(Mmo->DevClock(TEXT("matriarch near 22 -90\nmatriarch calm\nmatriarch hurt 0.45")), TEXT("n3: the pilot backs off, she is wounded"));
		Advance();
		return;
	case 1908:
	{
		const ac::Unit* Q = S ? NearestBug(*S, U ? U->position : ac::Vec2(), ac::UnitKind::matriarch) : nullptr;
		if (Q && U) Stop(Face(Q->position));
		const TSharedPtr<SAcProgressHud> W = UAcProgressHudSubsystem::Get(this) ? UAcProgressHudSubsystem::Get(this)->Widget() : nullptr;
		if (W && W->Boss() && W->Boss()->Hp < 0.6 * W->Boss()->Max && Since() > 2.5)
		{
			Check(W->Boss()->Hp > 0.5 * W->Boss()->Max, FString::Printf(TEXT("n3: the boss bar shows her at %.0f of %.0f"), W->Boss()->Hp, W->Boss()->Max));
			Shoot(TEXT("n3-04-boss-bar"), TEXT("the boss bar: the Matriarch wounded"));
			return;
		}
		TimedOut(TEXT("the boss bar"), 20);
		return;
	}
	case 1909:
		// Felled: her body sinks, the world hears it.
		Check(Mmo->DevClock(TEXT("matriarch down")), TEXT("n3: the Matriarch is felled"));
		Advance();
		return;
	case 1910:
	{
		// Her body (the live-bug finder passes bodies over).
		const ac::Unit* Q = nullptr;
		if (S)
			for (const ac::Unit& X : S->units)
				if (X.kind == ac::UnitKind::matriarch && X.bug) Q = &X;
		if (Q && U) Stop(Face(Q->position));
		if (Q && Q->bug && Q->bug->state == ac::Bug::State::dead && Since() > 2.2)
		{
			Shoot(TEXT("n3-05-down"), TEXT("the Matriarch down: her body sinking, the world told"));
			return;
		}
		TimedOut(TEXT("her body"), 15);
		return;
	}
	case 1911:
		Mmo->DevClock(TEXT("matriarch back"));
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	default:
		Check(Shots.Saved + Shots.Skipped == 22, FString::Printf(TEXT("%d pictures (%d skipped) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	}
}
