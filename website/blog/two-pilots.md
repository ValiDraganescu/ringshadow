# Two pilots on one world

> Two humans on one Ringshadow world: the core that drove one pilot per region now drives several, and two game clients on one server, one under 150 ms of lag and 5% loss, checked each other's Prospector, buildings, flights, tax and wave pay from both sides.

Source: https://ringshadow.com/blog/two-pilots

15 October 2026 · M10 · The long game

The mode is called an MMORPG and had only ever been played by one human at a time. That was the first task on the milestone 10 list, and it came in two merges today: the core simulation learned to drive several pilots in one region, and then two game clients on one local server checked each other, from both sides, with one of them under 150 ms of lag and 5% loss.

![Pilot A's view at the Citadel: the other human's Prospector with the tag PILOT 2B96 over it, standing by the Hab Dome and the Foundry, a corner gun in the foreground](https://ringshadow.com/blog/media/two-pilots/cover.jpg)

*The first picture of one human seeing another in this game: pilot A at the Citadel, pilot B tagged PILOT 2B96.*

## The core

Since the first milestone the core simulation drove exactly one pilot per region. The pilot's context, its progress, its skill effects, its own buildings, its headlamp and its circle of daylight, sat in one place, and a second human in a region where someone else drove could not drive at all. The morning's merge keeps the active driver's context where it was and holds each other human's beside it; the step swaps each in for its own unit's turn, its levelling and its skills, every AI and commander check counts all the driven units, and the vision gives each its own headlamp and daylight. The single-player strategy game has no other drivers, so its golden tests are untouched, and nothing changed on the wire or in the save. Seventeen files, 572 lines in and 119 out.

## The two clients

The afternoon's merge is the check. A new tool runs two games and the world server under the one hidden-run slot, the second game through the lag simulator at 150 ms each way and 5% loss, and takes pictures from both. What each saw: the other's Prospector in the realm's blue with its callsign over it, and the tag hidden when a building is in the way. One pilot starting the Beacon, the other getting the toast and seeing the project running on its own REALM tab with the halved fare. One pilot's Hab Dome rising on the other's screen, the deed toast naming who built it, the map tooltip saying the same. Unloading at the other's Dome, with the tax landing in the realm wallet on both screens. The Dropship seen from the ground: coming down, the other pilot walking aboard, lifting off. The Fork's first wave fought by both and each paid its share by the damage it did, 40 ore to one and 20 to the other. And one pilot leaving during the siege and coming back where it left, with its wallet.

Each pilot keeps its own wallet in a shared region now; a region's bank had been shared by its humans. Two more things were wrong and are fixed. A game closing under lag never sent its goodbye, so the other fought alone for the 10 seconds the server waits before letting a silent pilot go. And a flight between Landing Pads that share a building id, since ids count per region, was refused as "At Landing Pad 1 already". Protocol 34. The suites stand at 505, 99 and 336.

## What a second human needs

A second game with a folder of its own, so it gets a pilot key of its own, and the share file the first game writes: the port, a one-time token and the server's control file. The second game joins with that file. Both are flags on the command line for now, in the play doc. Not yet: two Macs on a network, since the server binds to the local address only; telling who is who, since both humans wear the same blue and only the tag tells them apart; a chat or a party. Those are the long game.

- ![Pilot A's view at the Citadel: the other Prospector with the tag PILOT 2B96 over it, by the Hab Dome and the Foundry](https://ringshadow.com/blog/media/two-pilots/01-a-sees-b.webp) A sees B at the Citadel, tagged PILOT 2B96.
- ![Pilot B's view, with RTT 371 ms and 150 ms lag each way at 5% loss in the corner: the other Prospector tagged PILOT B2B4 by a corner gun](https://ringshadow.com/blog/media/two-pilots/02-b-sees-a.webp) B sees A, through 150 ms each way and 5% loss.
- ![The Citadel from the front with a Hab Dome beside it, no tag in sight: the other pilot stands behind the building](https://ringshadow.com/blog/media/two-pilots/03-behind-citadel.webp) A tag shows only when nothing is in the way: B is behind the Citadel.
- ![Pilot B's screen with the Citadel and the toast THE REALM LIT THE BEACON, Dropship flights half price till tomorrow, started by Pilot B2B4](https://ringshadow.com/blog/media/two-pilots/04-beacon-toast.webp) A starts the Beacon; B gets the toast, naming who started it.
- ![Pilot B's REALM tab: the realm wallet at 1,000 ore and 300 MH, the Beacon card RUNNING with 40 min left, started by Pilot B2B4](https://ringshadow.com/blog/media/two-pilots/05-realm-tab.webp) B's REALM tab: the Beacon running, started by the other pilot.
- ![The Citadel with a Hab Dome's frame rising at its side, seen by pilot B](https://ringshadow.com/blog/media/two-pilots/06-dome-rising.webp) A's Hab Dome rising on B's screen.
- ![Pilot B's screen with the toast PILOT B2B4 BUILT A HAB DOME, in Prospect Landing, every pilot's, and A's green outline on the Citadel](https://ringshadow.com/blog/media/two-pilots/07-deed-toast.webp) The deed toast: PILOT B2B4 BUILT A HAB DOME.
- ![The world map on B's screen with a tooltip on the new Hab Dome: Built by Pilot B2B4, HP 400 / 400](https://ringshadow.com/blog/media/two-pilots/08-map-tooltip.webp) The map's tooltip says who built it.
- ![Pilot B at A's Hab Dome, its wallet reading 6,613 +24 ore and the realm 1,003, the tag PILOT B2B4 in the distance](https://ringshadow.com/blog/media/two-pilots/09-unloaded.webp) B unloads at A's Dome: 24 ore to B, 3 to the realm.
- ![Pilot A's screen, the realm reading 1,003 ore and 300 MH, the tag PILOT 2B96 across the field](https://ringshadow.com/blog/media/two-pilots/10-realm-tax.webp) A sees the realm wallet go up by the same 3 ore.
- ![The Dropship coming down over A's pad on B's screen, its engines lit, the frontier pilot Larkspur under it](https://ringshadow.com/blog/media/two-pilots/11-ship-down.webp) A calls the Dropship; B watches it come down.
- ![The Dropship on the pad on B's screen, A's green outline walking up its ramp](https://ringshadow.com/blog/media/two-pilots/12-boarding.webp) A walks aboard, seen by B.
- ![The Dropship on the pad with its ramp up, the ground crew gone, on B's screen](https://ringshadow.com/blog/media/two-pilots/13-lift.webp) Lift off; the tag is gone while A is aboard.
- ![Pilot A in The Fork's passage with the toasts THE FORK: WAVE 1 BEATEN, KSTR Larkspur's line, and YOUR SHARE: THE FORK WAVE 1 +40 ore +11 MH +53 XP](https://ringshadow.com/blog/media/two-pilots/14-a-share.webp) The Fork's first wave beaten: A's share, 40 ore, 11 MH, 53 XP.
- ![Pilot B in The Fork with the same wave toasts and YOUR SHARE: THE FORK WAVE 1 +20 ore +5 MH +27 XP, the tag PILOT B2B4 up the passage](https://ringshadow.com/blog/media/two-pilots/15-b-share.webp) B's share of the same wave: 20 ore, 5 MH, 27 XP, by the damage done.
- ![Night in The Fork on B's screen, wave 3 of 10 due in 9:20, no other pilot in sight, B's wallet at 6,644 ore and 4,205 MH](https://ringshadow.com/blog/media/two-pilots/16-b-back.webp) B leaves during the siege and comes back where it left, with its wallet.
- ![Night in The Fork on A's screen, the tag PILOT 2B96 back over the other Prospector beside a bug](https://ringshadow.com/blog/media/two-pilots/17-a-sees-b-back.webp) A sees B back.
