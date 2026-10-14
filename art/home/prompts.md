# The home screen's key art (docs/mmorpg-m7.md H1)

The home screen's art: a lone Prospector small on a ridge at dusk, the
ringed giant overhead, the ring's shadow band across the plain, Stardust Ore
glinting below. Made with Grok Imagine (`grok_image`) on 2026-10-14, four
rounds of four. The variants are in `variants/`; the user's review page is
https://claude.ai/artifact/8gUrLVMuQ7ChH1NqVXhFiN.

- `key-art.jpg`: the pick, `variants/d-giant-2.jpg` (1280×720).
- `unreal/Content-src/launcher/01-prospector-dusk.jpg`: the pick at
  2560×1440 (Lanczos, a light unsharp mask), the only image the home screen
  shows (`FAcLauncherArt`; one image drifts in and out over a minute).
- `rts-launcher/`: the strategy game's twelve old images (war, Rangers, a
  dogfight), taken off the home screen. Nothing else reads them.

What worked:
- The Prospector stays the Prospector only with its model sheet
  (`art/models/prospector/painted.jpg`) as the reference. Without it every
  image draws a different robot (round 2).
- An edit keeps the reference's shape: the 3:2 sheet gave 3:2 images. Round
  3 put two views of the sheet on a 1280×720 canvas for 16:9 images.
- The menu panel covers the left third, so the Prospector has to be asked
  for in the right third and the left third kept calm.
- Asking for a bigger giant on an edit (round 4) changed the dusk, not the
  giant's size.

## Round 1: `variants/a-ridge-ref*.jpg` (reference: the model sheet)

Epic painted key art for a frontier mining game, wide cinematic landscape. A
single small squat mining walker robot (the one in the reference: round
deep-blue riveted bell hull, glowing cyan visor slit, warm-white headlamp on
its crown, gunmetal arms, a spiral drill on its right arm, hazard-yellow
stripes) stands alone on a rocky ridge in the lower left third, seen from
behind and slightly to the side, tiny against the world, its headlamp beam
cutting through dust. Dusk on a barren rust-and-ochre frontier world. A
colossal ringed gas giant fills most of the sky, banded in cream, amber and
dusty rose, its thin translucent rings sweeping across the sky in a wide
arc. The rings cast a long dark shadow band across the plains below: a sharp
line between warm sunset light and cold blue shadow running across the land.
In the valley below, veins of crystal ore glint with iridescent opal
colours. Thin dust clouds, first stars. Lonely, quiet, awe, hope. Gritty
painterly sci-fi concept art, rich warm-cold contrast, volumetric light.
Leave the left third of the sky calm and darker for a title. No text, no
letters, no logos, no soldiers, no weapons, no explosions.

## Round 2: `variants/b-ridge*.jpg` (no reference, 16:9)

Epic painted key art for a frontier mining game, wide cinematic 16:9
landscape. Dusk on a barren rust and ochre frontier world. A colossal ringed
gas giant rises over the horizon and fills most of the sky, softly banded in
cream, amber and dusty rose, its rings a thin translucent arc sweeping high
across the sky. The rings throw a long straight band of cold blue shadow
across the warm sunlit plains, a clear edge between gold light and shadow.
On a rocky ridge in the lower left stands one small squat mining walker: a
round deep-blue riveted bell-shaped hull with a glowing cyan visor slit
across its front, a bright warm headlamp on top, stubby gunmetal legs, one
arm ending in a spiral drill. It is tiny against the landscape, a lone
prospector, seen from behind looking out over the valley. Below, cracked
ground with veins and clusters of crystal ore glinting iridescent opal blue
and violet. Far away, a few small lights of a lonely outpost. Thin dust in
the air, the first stars. Mood: lonely, vast, quiet wonder. Painterly gritty
sci-fi concept art, volumetric dusk light, rich warm-cold contrast. Calm
darker sky area on the left for a title. No text, no letters, no logos, no
soldiers, no weapons, no explosions, no spaceships.

## Round 3: `variants/c-right*.jpg` (reference: the sheet's back and three-quarter views on a 1280×720 canvas)

Replace this whole picture with an epic painted key art landscape; the two
robot views are only a reference for the robot's design. Wide cinematic
scene at dusk on a barren rust and ochre frontier world. One small squat
mining walker exactly like the reference robot (round deep-blue riveted bell
hull, glowing cyan visor slit, warm-white headlamp on its crown with a beam
cutting through dust, gunmetal arms, spiral drill on its right arm, back
canisters and hoses, hazard-yellow striped boots) stands alone on a rocky
ridge in the RIGHT third of the frame, lower right, seen from behind
three-quarter, looking out over a vast valley. A colossal ringed gas giant
fills the upper middle and right of the sky, softly banded cream, amber and
dusty rose, its thin translucent rings sweeping across the sky. The rings
cast a long straight band of cold blue shadow across the warm sunlit valley
floor, a clear edge between gold light and blue shadow. Below the ridge,
cracked ground with clusters of crystal ore glinting iridescent opal blue
and violet. Far off, a few tiny lights of a lonely outpost. The LEFT third
of the frame is calm: dark dusk sky and soft distant plain, nothing
important there. Lonely, vast, quiet wonder. Painterly gritty sci-fi concept
art, volumetric dusk light. No text, no letters, no logos, no soldiers, no
weapons, no explosions, no spaceships.

## Round 4: `variants/d-giant*.jpg` (reference: `variants/c-right-2.jpg`)

Keep this exact composition, the robot, the ridge, the valley, the ore
crystals and the outpost light, but make the ringed gas giant colossal: it
rises from behind the horizon and fills most of the upper sky from the
middle to the right edge, its rings a huge translucent arc sweeping across
the whole sky up to the left. Deepen the dusk: a warm orange sunset glow low
on the horizon at the left, the sunlit side of the giant glowing warm, the
first stars in the dark blue upper left sky. Keep the long straight band of
cold blue ring-shadow lying across the warm sunlit plain, and make the ore
crystals glint brighter. Painterly gritty sci-fi concept art. No text, no
letters, no signature, no logos.
