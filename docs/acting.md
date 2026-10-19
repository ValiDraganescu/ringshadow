# Acting: the E key (C5)

The user, playing on 2026-10-19: "the actionable things like buttons should
only work if I am really close and the cursor points directly at the button;
right now the game is confused and does not know if I want to ride the Mule or
open the door." Before C5 every act of the MMORPG mode found its own target by
distance (the nearest door, gun station, Mule perch, Foundry, pad within some
reach), in a fixed order, so a door could take E at a gun station and a Mule's
perch could take it at a door. C5 puts them all behind one rule and one picker.

## The rule

In the MMORPG mode, on foot or driving, first or third person:

1. **E acts only on what the crosshair is on.** The crosshair's ray is the
   camera's line through the reticle (third person too: the chase camera's
   line, from just past the driven unit). It must enter the thing's **act
   box** before the world stops it (a wall, the ground, a unit: 0.4 cells of
   slack, so a panel on a wall still counts).
2. **And only within reach.** The driven body's edge must be within
   `ac::Act::reach` (1.5 cells) of that box, across and up. The body is the
   unit's walk circle (as a square), a Mule's box along its heading; its
   height a Prospector's 1.5, a Mule's deck and driver.
3. **One thing at a time:** the box the ray enters first. If it is out of
   reach, nothing (a farther box does not take over).
4. **No box under the crosshair:** a building of the realm the crosshair is on,
   within reach of the point hit, takes the demolish hold (`HOLD E · DEMOLISH
   …`, C3). Else nothing: no prompt, and E does nothing. **Driving a Mule**,
   nothing aimed means "E · CLIMB OFF".
5. **The prompt names exactly that thing**, or the reason it would be refused
   now (the core's refusal, in the blocked tone; the key sends nothing).
6. **Only E.** The click and Space keep the core's acts (fire, strike, drill,
   field pump, weld a scaffold, repair, enter a Derrick, place a held
   building); they no longer open doors, take guns, climb on Mules, buy, call
   or press the ship's buttons. E no longer drills or repairs. At a gun every
   key fires.

## The act boxes

Each is an `ac::Block` (a box turned about the vertical, with its heights) in
`AutocraftCore/Public/Act.h`, built from the things' own numbers, so they follow
the buildings' and the ship's sizes:

| Act | Prompt | Box |
|---|---|---|
| Citadel door | "E · OPEN" / "E · CLOSE" | its panels beside the doorway, outside and in (`doorPanel`, right of the jamb), and the door itself while it stands (`doorLeaf`; an open door lies flat: not a target) |
| Gun station | "E · TAKE GUN 1 SE" | the station's console in its corner (`console`) |
| Mule | "E · CLIMB ON" / "E · FILL THE MULE" | its perch (the rail and levers at its front) and its hopper (`perch`, `hopper`) |
| Foundry | "E · BUY MULE · 600 ORE · 150 MH" | the call post's button beside its door (`foundryPost`) |
| Landing Pad | "E · CALL THE DROPSHIP" | the call post's button off its front-right corner (`padPost`) |
| Dropship | "E · OPEN THE RAMP" / "E · CLOSE THE RAMP" / "E · TAKE OFF" | the ramp post's head (its button and the lamps over it) and the console's take-off button, in the ship's frame (`rampPostLocal`, `takeOffLocal`), in its bay or riding it |
| Demolish hold | "HOLD E · DEMOLISH …" | no box: the building the crosshair hits |
| Climb off | "E · CLIMB OFF" | driving a Mule, nothing else aimed |

Driving a Mule only the doors and the ship's buttons are targets; on foot
(a Prospector not aboard, at a gun or in a Derrick) all of them.

**The call posts** (`unreal/Tools/Editor/models/call_posts.py`): a short
hazard-striped post with a big green button on its head, facing the front.
The Landing Pad and the Foundry had nothing to aim at; now they have a
button like the Dropship's ramp post. The script adds the parts `callPost` and
`callButton` to the `landingPad_*` and `foundry_*` catalog entries (run it
after `landing_pad.py`, which rewrites the pad's entry). The pose
(`AcBuildingPose::CallPost`) moves the post to the core's point
(`Act::padPostAt`, `foundryPostAt`: relative to the building's radius) and
keeps its size when the building is drawn stretched; the button glows,
brighter while the crosshair is on it. The post is solid to a pilot's walk
(`Simulation::pilotCanStand`, `pilotContact`). A landed Dropship must keep
clear of the pad's post at any heading.

## The server

The server keeps its own checks, on the same boxes seen from above, a little
looser: `Act::serverReach` (2.0) from the body's edge, so an act the client
allows is never refused for its distance (`act_clientReachIsNeverRefusedForDistance`
in the core tests walks a grid of positions round every box). They replace
`Rules::doorReach`, `Defence::stationReach`, `Mule::reach` and
`ShuttleBay::reach`: `Simulation::doorRefusal` (the door or a panel), `man`
(the console), `muleBoardRefusal` (the perch), `muleFillRefusal` (the
hopper), `rampRefusal` and `takeOffRefusal` (the buttons). New: `buyMule`
refuses "Too far from the Foundry's post", and `callShuttle` also takes a call
from a realm pad's post (beside its old "within the Pad link's reach of the
centre"). The demolish check is C3's (`demolishReach` with its slack).

## The game

- `AcActPick` (`AcActPick.h`): `Targets` gathers the boxes near the driven
  unit (world cm, the ground from the terrain, a ship's drawn frame);
  `Pick` takes the crosshair's ray and the world's first hit and returns the
  box in reach; `Act` makes the act and its refusal. Plain C++ and Unreal
  math: `Autocraft.Pilot.ActPick` tests it with no world.
- The pawn works it out every frame after the crosshair (`UpdateAct`,
  `CurrentAct`), lights the door panel on the pilot's side while E is on the
  door, and the call post's button. `Press(bDown, bKeyE)` sends the act on E
  and nothing else; the demolish hold starts from it too.
- The cockpit's prompt (`AcPilotText`, `FAcPilotInputs::Act`) shows E's act
  before the click's prompt; the demolish line keeps its own place.
- Tests and flows aim like a player: `AAcPilotPawn::TestLookAtAct(kind, id,
  index)` turns the look onto a box's middle. The flows `mmopractice`,
  `mmocitadel`, `mmomule`, `mmoferry`, `mmoferrymule`, `mmomuledrive` and
  `mmomuleshots` walk to a spot in reach, aim, check the prompt, then press E.
