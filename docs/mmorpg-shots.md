# MMORPG moments in pictures (milestone 4, Q1)

Every moment of the MMORPG test mode, shot from one hidden run and looked at
as a first-time player would see it. This replaces the user's hand check
([mmorpg-m4.md](mmorpg-m4.md)). Q2 fixes the problems listed at the end.

## The run

The harness is `-AcMmoShots=DIR` (`unreal/Source/Autocraft/AcMmoShots.cpp`, the
flow step `mmoshots`). It starts a new world on the local server and plays it
through the real pilot pawn, build menu, level cards and world map. At each
moment it saves a 1600×1000 PNG with the HUD on and logs a line
(`mmoshots: saved NN-moment.png (…; at x,y in region, N ore)`). It keeps its
own pilot key and world in `DIR/mmo` and the window game's sessions in
`DIR/save`, so it never touches the player's saves.

```sh
make -C unreal/server
D="$PWD/unreal/server/build/shots/run3"; mkdir -p "$D"
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditorBG.app/Contents/MacOS/UnrealEditor" \
  "$PWD/unreal/Autocraft.uproject" /Game/Maps/Battlefield -game -RenderOffscreen -windowed -ForceRes \
  -ResX=1600 -ResY=1000 -unattended -nosplash -nosound \
  "-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False" -abslog="$D/run.log" \
  -AcMap=highlands-small -AcMmoShots="$D" >/dev/null 2>&1
grep -E "mmoshots|flowtest" "$D/run.log"     # done when: flowtest: done, 0 failed
```

Since 2026-10-14 (S1) the sets wait on the game, not on the world's clock.
A shot run passes the server a `--dev-control` file (`DIR/mmo/dev-control.txt`),
and `FAcMmoClient::DevClock` moves the world's clock with it at once:
`clock T`, `hour H` (on to the next H) or `ahead S` (`Server::devControl`);
`catchup S` catches the world up as a restart after S s down (X2), `idle S`
idles it S s as with nobody on line (S4: the frontier pilots' and the
mercenaries' work booked), `week end|left S|list` ends the realm week, sets
its time left or lists the standings (S4).
The game never passes the file, so a normal game cannot reach it. The
Starfall set jumps to 19:00 for its night pictures and takes about 40 s,
not 4 minutes. `-AcMmoShotsOnly=sf-02,sf-08` (names, numbers, or a set's
prefix and a number) shoots only those moments. The others are logged as
passed over and count as skipped in the set's total. The Starfall set
also skips their waits: with no fall moment asked for, the clock jumps
past the impact; with no night moment asked for, the run ends after the
day. Hidden runs go through `unreal/Tools/hidden_run.py`, which holds the
Mac's one run slot.

The pictures were taken on 2026-10-13 in run 3: 17 pictures, 0 failed, about
3 minutes, machine load about 13. Git ignores the folder
(`unreal/server/build/`), so the links below only work on this machine. The
build had the Q3 and Q4 sessions' uncommitted work in it at the time: the
"1 UNITS" label, the world map's arrow-key text and the flight. Run 2 used the
same code at a load of 69 (1–5 fps) and is quoted where it showed something
more.

Daylight held through the whole run, so night was not checked.

## Moments

**01 Spawn on the pad, 3rd person:**
[01-spawn-pad-third.png](../unreal/server/build/shots/run3/01-spawn-pad-third.png)
The pad reads as a pad: a round, striped plate under the Prospector. The
dashboard reads well (PROSPECTOR LV1, 0/60 XP, cargo empty, 45/45). It faces
east, away from the ore and the wells, so there is nothing to do in view. The
perf panel covers the top right.

**02 The same, 1st person:**
[02-spawn-pad-first.png](../unreal/server/build/shots/run3/02-spawn-pad-first.png)
Clean. The cockpit arms frame the pad, and VIEW 1ST/3RD shows which view is
on. The tip reads "click enter a Derrick", but there is no Derrick anywhere.

**03 Top-down over the start region (Esc):**
[03-topdown-start.png](../unreal/server/build/shots/run3/03-topdown-start.png)
"Click your Prospector to drive it" is right. But the camera starts so close
that it shows the pad, an orange ring and dark fog. The ore, the wells and
the cliffs are out of view, so you get no sense of the region. The middle of
the dashboard is a big empty panel.

**04 The world map (M):**
[04-world-map.png](../unreal/server/build/shots/run3/04-world-map.png)
Clear: a 3×3 grid, YOU ARE HERE, the pad highlighted, a key line, and
CLOSE/FLY. Each tile is only a name, a style and "landing", with no picture
of the region.

**05 The build menu (B):**
[05-build-menu.png](../unreal/server/build/shots/run3/05-build-menu.png)
Eight icons with numbers and prices, and no names. A new player cannot tell
which one is the Citadel. It offers Garrison, Foundry, Spacedock and Bastion,
which do nothing in a Prospector-only world. It also offers Foundry and
Spacedock at 150/100 when the pilot has 0 MH. "ORE 15" on the compass sits
half under the region box.

**06 The Citadel's ghost near the ore:**
[06-citadel-ghost.png](../unreal/server/build/shots/run3/06-citadel-ghost.png)
Good: a big green hologram between the Prospector and the ore field, with
"Click build Citadel · Esc cancel". Nothing shows the price or the wallet
after the build (400 → 0).

**07 The level-up cards:**
[07-level-up-cards.png](../unreal/server/build/shots/run3/07-level-up-cards.png)
The weld itself levels the Prospector, with no dev XP needed: level 2 at 15%
of the Citadel. The cards are readable (Quick drill / Light frame, Z/X). The
banner "PROSPECTOR LEVEL 2 / NEW PICK ON THE DASHBOARD" is a faint ghost over
the sky and hard to read, and it says "dashboard" while the cards are over
the view. The cards cover the welding Prospector.

**08 A Citadel going up (40%):**
[08-citadel-going-up.png](../unreal/server/build/shots/run3/08-citadel-going-up.png)
Good: the scaffold, sparks, the Citadel half up, "Building", and the XP bar
filling (160/180). The third-person camera sits inside the scaffold frame,
and the cards hide the Prospector.

**09 The Citadel finished:**
[09-citadel-finished.png](../unreal/server/build/shots/run3/09-citadel-finished.png)
The Citadel stands. Nothing says it is finished, and the dashboard only goes
back to "Ready". The Prospector is LV 4 with "3 PICKS WAITING" under a header
reading "LEVEL 2 · PICK ONE", which is confusing next to the LV 4 badge.

**10 Drilling ore:**
[10-drilling-ore.png](../unreal/server/build/shots/run3/10-drilling-ore.png)
The feedback is good: "DRILLING 23%" with a ring at the crosshair, "Let go or
walk away to stop", and a progress bar. You cannot see the patch being
drilled: it is under the cards, in front of the Prospector.

**11 Unloading, before:**
[11-unload-before.png](../unreal/server/build/shots/run3/11-unload-before.png)
Clear: "Carrying a load home", "Walk into the Citadel to unload", CARGO
25 ore, wallet 0. The compass's "CITADEL 2" sits under the region box.

**12 Unloading, after:**
[12-unload-after.png](../unreal/server/build/shots/run3/12-unload-after.png)
The wallet reads 25 and cargo is empty: right. The only feedback is a small,
faint "+25" on the Citadel's tower, and the wallet number does not flash. The
camera is pressed against the Citadel, so its wall fills half the view.

**13 The world map on the pad, Cold Fork picked:**
[13-world-map-pick.png](../unreal/server/build/shots/run3/13-world-map-pick.png)
Clear: "At pad landing: FLY to pad landing in Cold Fork", with FLY
highlighted.

**14 The flight, mid-flight:**
[14-flight-mid.png](../unreal/server/build/shots/run3/14-flight-mid.png)
The Prospector hangs in the air over open ground with its shadow far below.
It does not read as a lift-off from the pad. The view drops to a raised
camera, the dashboard is empty, and the controls card says "Click your
Prospector to drive it", which you cannot do in flight. The counter reads
"0 UNITS". "Flying to Cold Fork…" is small.

**15 Landed at Cold Fork's pad:**
[15-landing-cold-fork.png](../unreal/server/build/shots/run3/15-landing-cold-fork.png)
Good: on the pad on the plateau, ore arcs and wells around, and the box reads
"Cold Fork · The Ridge". Nothing on screen says "you arrived". In run 2 the
landing frame came at 1 fps, with the camera still high and the dashboard
empty.

**16 Edge crossing, just after arrival in the start region:**
[16-edge-crossing.png](../unreal/server/build/shots/run3/16-edge-crossing.png)
Reads well: you come in from the north edge facing south, with the Citadel,
the pad and the ore ahead. Only the small box's text changes, with no region
title. The RTT read 895 ms in run 2 and 35 ms here.

**17 After leaving and resuming:**
[17-resumed.png](../unreal/server/build/shots/run3/17-resumed.png)
The same spot (0.14 cells off), wallet 25, LV 4 and the 3 picks kept: right.
The view turned from 195° (south) to 098° (east), so you look at the
off-region dune wall and not where you were looking. In run 2 the resumed
frame showed only a wall of sand, with no Prospector in view.

## Problems

1. **Annoying.** The level-up cards stay over the middle of the view while a
   pick waits. After the first Citadel that is always (3 waiting), and they
   cover the Prospector, the scaffold and the ore being drilled (07–17).
   *Owner: the cards' layer and place, `AcLeveling.cpp` and `SAcLevelUp.cpp`.*
2. **Annoying.** One hand-built Citadel takes the Prospector from level 1 to 4
   in 35 s (400 XP from the weld), and the header "LEVEL 2 · PICK ONE" next to
   the "LV 4" badge confuses. *Owner: the builder's XP in
   `AutocraftCore/Private/Simulation.cpp:695` (Leveling balance), and the
   cards' header in `SAcLevelUp.cpp`.*
3. **Cosmetic.** The level banner is a faint ghost over the sky, hard to read,
   and says "NEW PICK ON THE DASHBOARD" while the cards are over the view.
   *Owner: `SAcLevelBanner` (`AcLeveling.cpp` / `SAcLevelUp.cpp`).*
4. **Annoying.** The region box (name · style · RTT) sits on the middle of the
   compass strip. It hides the heading under the crosshair and half covers
   compass marks ("ORE 15", "CITADEL 2") in every driving shot. *Owner: where
   `SAcMmoBanner` is placed (`AcGameFlow.cpp` `TickMmo`, `SAcMmoBanner.cpp`),
   against the pilot compass (`AcPilotOverlay`).*
5. **Annoying.** The flight does not read as one. Mid-flight the Prospector
   hangs over open ground with its shadow far off, and the camera jumps to a
   raised view. The dashboard is empty, the controls card says "Click your
   Prospector to drive it" and the counter reads "0 UNITS". *Owner: Q3's
   flight (`AcPilotPawn.cpp`, `AcPilotFlight.h`); the controls card and the
   unit count in the HUD (`SAcRoot`, `SAcScoreboard`).*
6. **Annoying.** The top-down view (Esc) starts zoomed onto the pad, so the
   region (ore, wells, cliffs) is out of view, and the dashboard's middle is
   an empty panel. *Owner: the RTS view on a hand back (`AcPilotPawn.cpp`
   eject target, `AcRtsPawn.cpp`).*
7. **Annoying.** The build menu shows icons and prices with no names. It
   offers Garrison, Foundry, Spacedock and Bastion, which have no use in a
   Prospector-only world, and MH-priced buildings while the pilot has 0 MH.
   *Owner: `AcPilotBuild.cpp` (the menu), `ac::PilotBuild::kinds`
   (`AutocraftCore/Public/Pilot.h`), the server's `Region::build`.*
8. **Annoying.** Arriving in a region (an edge crossing or a landing) shows no
   region title. Only the small box's text changes. *Owner:
   `SAcMmoBanner.cpp`.*
9. **Annoying, seen once.** In run 2, under heavy load (client at 1–5 fps),
   the Prospector was drawn at -1.0,-45.0 after the edge crossing but resumed
   at -4.3,-36.8, 9 cells away. The server's unit had gone on walking after
   the crossing. Run 3 (normal load) kept it within 0.14 cells. *Guess: the
   walk input carried over the crossing (`arrive(…, lastInput)`,
   `unreal/server/world/WorldMap.cpp:461`), with the client's stop input after
   the RegionChange dropped or late, or the prediction not reset
   (`AcMmo.cpp` `OnRegionChange`). Repro: `TestReadEvery` slow frames at the
   crossing.*
10. **Cosmetic.** The perf panel (FPS, GPU, CPU) is on by default on a fresh
    profile and covers the top right of every shot. *Owner:
    `UAcSettings::bShowPerf = true` (`AcSettings.h:54`), `ac.PerfPanel`
    default 1 (`AcPerf.cpp:27`).*
11. **Cosmetic.** Unloading gives almost no feedback: a small, faint "+25"
    over the Citadel, and no flash on the wallet. *Owner: the Deposited event's
    effect (`AcEffects`), `SAcResourceBar`.*
12. **Cosmetic.** The third-person camera does not back off buildings. It sits
    inside the scaffold while welding and against the Citadel's wall while
    unloading. *Owner: the chase camera, `AcPilotChase.h`.*
13. **Cosmetic.** On resume the view's heading is not the one left (195° →
    098°), so the pilot looks at the dune wall at the region's edge. *Owner:
    the seat on resume in `AcPilotPawn.cpp` (`SyncMmo`).*
14. **Cosmetic.** A new pilot spawns facing east, away from the ore and the
    wells. *Owner: the start seat's facing on the server (`WorldMap.cpp`
    start pad), or the region file's spawn.*
15. **Cosmetic.** The tip line rotates tips that do not fit the moment ("click
    enter a Derrick" with no Derrick, music keys while building). *Owner: the
    console's tip list (`AcPilotOverlay.cpp` / `AcPilotText`).*
16. **Cosmetic.** "1 UNITS" should read "1 UNIT". *Owner: the Q4 HUD work
    (`SAcScoreboard`).*
17. **Cosmetic.** The RTT on 127.0.0.1 reads about 30 ms at normal load and
    895–1132 ms under load. It counts frame time and the snapshot cadence,
    and looks like lag to a player. *Owner: the RTT in `AcGameFlow.cpp`
    `TickMmo`, `net::Client` round trip.*
18. **Cosmetic (design).** The first Citadel costs the whole 400-ore start
    wallet, and a trip brings in 25. The pilot can build nothing else for a
    long while, and nothing on screen says how to earn more besides the tip
    line. *Owner: the start wallet (`unreal/server/world/WorldMap.h`) and the
    prices (`Rules.cpp`).*
