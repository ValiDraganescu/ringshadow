# Weather

The frontier world has weather: sun, broken cloud, rain and storms. It
runs on the game clock with the day and night (`AAcDaylight`), changes
slowly and always through the states between (sun never turns into a
storm in one step), and is drawn and heard, not simulated: units, vision
and the economy don't feel it.

## The four states

| State | Cloud cover | Clouds | Rain | Light on the ground | Extras |
|---|---|---|---|---|---|
| Clear | 0.35 | flat fair-weather cumulus, white | none | full sun, crisp shadows | – |
| Cloudy | 0.70 | broken cumulus, some towers | none | sun with cloud shadows passing; the sky light a little greyer | – |
| Rain | 0.96 | a grey layer, thick, low | steady (0.5) | dim, flat; the ground wet, puddles on flat ground | rain bed |
| Storm | 0.99 | dark towers over the layer | heavy (1.0) | dark; haze closes in | lightning and thunder, heavy rain bed, gusts |

The values are targets: every quantity eases from one state's value to the
next over the change (45 s of game time), and some follow on their own
clocks: rain starts only once the cloud has built (the second half of a
change) and stops first when it clears; the ground gets wet over ~30 s of
rain (sooner in a downpour) and dries over 1.5-3 min after it, slower
under heavy cloud.

## The schedule

`ac::Weather` (`unreal/Source/AutocraftCore/Public/Weather.h`, engine-free,
tested in `core-tests/tests/WeatherTests.cpp`) is a pure function of the
game time and a seed: the same seed and time always give the same weather,
whatever was asked before (the integrated parts, the wind's offset and the
wet ground, are stepped a whole second at a time from 0 with checkpoints
every minute). Spells follow each other as a walk between neighbours: Spells follow each

- Clear → Cloudy.
- Cloudy → Clear or Rain (even odds).
- Rain → Cloudy (55 %) or Storm (45 %).
- Storm → Rain.

Spell lengths (game seconds, one day is 600): Clear 240-480, Cloudy
150-300, Rain 120-240, Storm 90-180. A game starts Clear, for at least
the first 5 minutes. Over a 30-minute game that is usually one or two wet
spells, sometimes a storm.

The seed is the game's id (a hash of the UUID its save keeps, `Session::game`), so
a loaded game goes on with the same weather, and a new game gets its own.
`-AcWeatherSeed=N` fixes it.

## What it drives

`AAcWeather` (`unreal/Source/Autocraft/AcWeather.h`, spawned after the
daylight) owns the model, the rain and the lightning's light; the rest is
set each frame by `AAcDaylight::Update` from the same `ac::WeatherSample`:

- **Clouds** (`M_AcCloud`): half the tracing samples in a storm (a deck
  full of cloud has nothing to skip; its grey hides the grain), cover
  (past 0.85 a flat layer fills the
  gaps: rain cloud), density (×1.8 in a storm), albedo (white → storm
  grey), the cloud type (flat fair-weather cumulus when clear, towers in
  a storm), the wind (15 m/s clear to 26 m/s in a storm; the offset is
  the integral of the speed, so a change of wind never jumps the clouds).
- **Cloud shadows** (`M_AcCloudShadow`): the same clouds, so they follow.
- **Sun, moon, sky light**: the ground's sun and moon dimmed by the
  gloom (to 30-35 % in a storm, and their highlights to 10 %: no sun
  disc to mirror under a rain deck), the fill to 70 %, the sky light to 80 %
  and greyer, the atmosphere's sun to 45 % (a dark sky behind the
  clouds), the dusk glow muted under full cover.
- **Haze**: the height fog up to 3.6 times as thick and greyer in a
  storm, the pilot's view (`Tones.Reach`) closing in to under half.
- **Rain** (`AAcWeather`, `M_AcRain` from
  `Tools/Editor/make_weather_materials.py`): 20,000 streaks in a 40 × 40 ×
  30 m box ahead of the camera, one procedural mesh written once and one
  draw call; the vertex shader moves them with the fall (7 m/s, faster in
  a storm) and a quarter of the wind, wraps them in the box (fixed in the
  world, never swimming with the camera) and opens each into a streak a
  pixel or two wide facing the camera. How many show follows the rain
  rate. Lit by the sky (dim at night, greyer under cloud), white in a
  flash.
- **Wet ground** (`M_Terrain`'s `Wetness` and `RainRate`,
  `Tools/Editor/make_terrain_material.py`): darker and glossier ground,
  steep rock and the border less; puddles spread over flat ground as it
  soaks, nearly mirror-smooth with flattened normals, so they hold the
  sky; rings where drops land, more of them in heavier rain.
- **Lightning** (only in the storm's heaviest part, about one strike every
  7 s): each strike a double flicker (stroke, dip, return stroke, glow),
  brighter when close: a shadowless directional light from its side of
  the sky, the cloud above the strike lit from inside (`Glow` round
  `Bolt`: a bright core in a soft halo 2.5 km across, the rest of the deck
  dark), the sky light up to
  1.8 times, the rain white; and the bolt itself in the cockpit and the
  sky view (the RTS camera never sees the horizon): a jagged channel from
  the cloud base to the ground at the strike's distance and side, two or
  three branches, seeded from the strike, a white core in a blue glow
  (`M_AcBolt`), fading in out of the cloud. Strikes come from 0.6 to 9 km
  away; the thunder arrives distance / 343 m/s later. The flash, the
  bolt and the clouds' glow share one flicker (`ac::Weather::flicker`).
- **Wet hulls** (`MPC_AcWorld.Wetness`, read by `M_Hull`): buildings and
  units darker and glossier as the ground soaks, their top faces most.
- **Rain on the glass** (`M_AcLensRain`, a post-process): driving a unit
  in first person in the rain, drops bead up on the glass, refract the
  view, the big ones slide down; more of them in heavier rain.
- **Sound** (`Tools/AudioGen/weather.manifest.json`, ElevenLabs sound
  effects, in `unreal/Resources/Sounds/sfx/`): `rain` and `downpour` beds
  (seamless loops, two layers each like the wind), crossfaded on the rain
  rate; the wind up to 1.7 times in a storm; `thunder` (3 rolls) for a far
  strike, `crack` (2) within 2 km, quieter with distance. The rules are
  `FAcAudioRules::Weather`/`Thunder`, tested in `Autocraft.Audio.Rules.Weather`.
  The rain beds were remade on 2026-10-12: the first takes were a clipped
  1-16 kHz hiss, 13 dB over the wind, that sounded like static. The new
  ones are darker prompts, low-passed at 7 kHz (`Tools/AudioGen/soften.py`;
  the takes are `Tools/AudioGen/raw/rain_1.mp3`, `downpour_1.mp3`): light
  rain about 2 dB under the wind, the downpour about 4 dB over it.

## Ringshadow sweep

Twice a world day the gas giant's rings throw a band of shadow across the
land: the ringshadow, the game's signature event (docs/mmorpg-m7.md E1). It
runs in the MMORPG mode only, on the world's clock (`ac::WorldClock`), the
same in every region at once.

The schedule and the band are `ac::Ringshadow`
(`unreal/Source/AutocraftCore/Public/Ringshadow.h`, tested in
`core-tests/tests/RingshadowTests.cpp`), a pure function of the world time
and a ground point. The server pays the bonus from it and the client draws
the band from it, so the two agree with nothing new on the wire.

| What | Value |
|---|---|
| When | at 11:30 and 16:00 world time, both in daylight. A new world (it starts at 8:00) sees the first one about 8 minutes in (485 s), the second at 1,108 s; then every 40-minute day |
| How long | 120 s, from the band's leading edge 25 cells short of the region to its trailing edge 25 cells past it |
| Which way | toward bearing 100° (east, a little south), out of the giant's side of the sky (it hangs at 280°) |
| Width | 110 cells across, each edge fading over 16 cells. On a medium region (144 × 96) it moves about 2.7 cells a second, slower than a walking Prospector (3.9), and a point is in the shadow about 35 s, 29 of them at full dark |
| Reward | an ore load drilled at a deposit at least half in the shadow carries ×1.5 ore (25 → 38), so its unload pays ×1.5 ore and ×1.5 XP. MH is not affected |
| Warning | 60 s before, the KSTR radio's toast: "KSTR: RINGSHADOW IN 60 · Hold your ground · ore drilled under the band pays half again" |

The server (`WorldProgress.cpp`, the moment a load is drilled) raises the
unit's cargo when the deposit is under the band, tested in
`server/tests/RingshadowTests.cpp`. Ore drilled in the sun before or after
the sweep is the plain amount.

On screen (`AAcRingshadow`, `unreal/Source/Autocraft/AcRingshadow.cpp`):

- **The band on the land.** `M_Ringshadow`
  (`Tools/Editor/make_ringshadow_material.py`), a post-process pass, darkens
  the ground in the band to 0.14 of its light (about 0.4 of its brightness
  on screen) and cools it to the sky's blue. Its edges sharpen with
  distance (half the 16-cell soft edge close by, 2 cells 120 cells off,
  about the same half-cover lines as the gameplay's), its first cells are
  darker still, the land just ahead of it is warmer, and a line of dust
  along the edge catches the sun: the wall reads coming over the far land
  long before it arrives. The band goes to `MPC_AcWorld` (`SweepDirX`, `SweepDirY`,
  `SweepLead`, `SweepTrail`, `SweepSoft`) every frame.
- **The sun.** With the camera in the band the sun dims to 0.3
  (`AAcDaylight::RingshadowSun`, which also dims the sky's sun). The pass
  makes up for that outside the band, so the band reads the same from
  inside as from outside.
- **The sky.** The ring's arc, 12° wide, runs along the great circle
  through the giant and the sun. At 11:30 the sun stands 17° above the
  giant's limb, so the arc climbs from the giant across it; the giant never
  passes in front of the sun during a sweep. The arc is drawn on the sky
  dome (`M_AcSky`, `Tools/Editor/make_sky_material.py`; `MPC_AcWorld`'s
  `RingOn`, `RingOff`, `RingHalf`), so clouds pass in front of it. It uses
  the giant's own ring profile, with its gaps and ringlets, and it is
  translucent: dense bands let a fifth of the light through, thin ones
  scatter it forward. The sun shows through dimmed and banded, with a
  bloom, and the arc's edges are fine bright lines near the sun. Its offset
  from the sun follows the camera's place in the band, so the arc's edge
  crosses the sun as the band's edge crosses the camera.
- **God rays.** As the arc's edge crosses the sun, the post-process pass
  smears the bright sky toward the sun: the sun's disc, the glowing gaps
  and the lit cloud edges. Only sky pixels feed the rays, so the ring's
  bands, the clouds and the land cut them into shafts. They are weaker
  deeper in the band.
- **The ore.** Stardust Ore veins under the band flare brighter than at
  night (M_Opal's night glow, ×1.8, and more glints).
- **The air.** A cold, slightly grey tint while the camera is in the band.
- **Sound** (`Tools/AudioGen/moments.manifest.json`, ElevenLabs sound
  effects, in `unreal/Resources/Sounds/sfx/`; `FAcAudioRules::Ringshadow`,
  tested in `Autocraft.Audio.Rules.Moments`). From the 60 s warning a low
  drone (`rsdrone`) comes in, quiet at first, and swells as the band's edge
  comes over the last 90 cells (about 30 s). As the cover at the ears passes
  a half, a pressure-drop whoosh plays (`rspass`), and under the band the
  drone gives way to a cold, airy bed (`rsbed`) that follows the cover. As
  the cover drops under a half again, a warm release plays (`rsrelease`).
  Within 14 cells of a deposit that flares (cover at least 0.5), a faint
  crystalline shimmer (`shimmer`) plays there.

A voice line for the radio can come later. To start a sweep now, use
`--ringshadow S` on the server, which moves the world's clock so a sweep
starts S seconds from now (the hour moves with it unless `--hour` holds it).
On the game, `-AcMmoRingshadow=S` passes it on. The picture run is
`-AcMmoShots=DIR -AcMmoShotsRingshadow -AcMmoRingshadow=70`.

## Starfall

About once a world day a meteor streaks across the sky under the gas giant
and lands somewhere in the world. Its blast destroys what it falls on and
hurts what stands round it; it leaves a crater with a deposit, first come,
first served (docs/mmorpg-m7.md E2; m10.md E1: since the regions hold one or
two veins, a fallen star is worth the chase and frightening to stand near).
It runs in the MMORPG mode only. The server owns it, the same for every
pilot, and does the blast's damage; the client draws it and tells the
pilot.

The schedule, the numbers and the streak's path are `ac::Starfall`
(`unreal/Source/AutocraftCore/Public/Starfall.h`). The server's part is
`WorldStarfall.cpp` (`unreal/server/world/`), tested in
`server/tests/StarfallTests.cpp`.

| What | Value |
|---|---|
| When | one a world day realm-wide (E1; 2 or 3 before), by dice seeded with the world's id, in the middle 70% of the day. The same world falls at the same times on every server and after every restart |
| Where | picked when it is told, a minute before the impact. Any region, weighted toward the pilots: each region counts 1, each pilot in it 8, each pilot next door 3. Then a spot on open ground: flat (no cliff, no rock under the crater's 4 cells), joined to the region's pads, at least 12 cells from the map's edge, clear of pads (by 11 cells past the crater), sites (8), ore (6.5), wells (7.5) and other craters. Buildings and units are in its way, not spared (E1). Never where its blast would reach the home's ground: the start region's Citadels and the 25 cells round them (with no Citadel yet, the start pad); elsewhere nothing is spared, in any region |
| The blast | E1: a quarter of the region's diameter across (its longer side's): 18 cells round the impact in a medium region (`Starfall::blastRadius`). Within 40% of that (7.2 cells) everything is destroyed, buildings and units alike; from there the damage falls off to nothing at its edge, by each thing's nearest edge (`Starfall::blastShare`: a Hab Dome 1.5 cells inside the edge loses a seventh of its hp). The server does it at the impact (`WorldMap::blast`), not when a save is loaded |
| The warning | 60 s before, to every pilot: the KSTR radio's toast, "STARFALL IN 60", with the region, the bearing and the distance from the pilot, and how far its blast flattens ("get clear"). Again at the impact ("STARFALL DOWN") in the pilot's region and the ones around it. A pilot has the minute to run |
| The deposit | at the impact a deposit appears in the crater with 100 to 1,000 ore by the fall's dice (E1; 3 × the region's median before). Ore drilled from it pays ×1.5 XP: the extra half when the load is drilled (to the pilot whose Prospector stands at the deposit), the plain XP when it is unloaded. It never grows back: emptied, it is gone |
| The crater | marks the ground for a world day (40 minutes), hot for the first few minutes. The fall is dropped once its crater has faded and its deposit is empty; a deposit with ore left outlives its crater |
| Saved | told and landed falls (`starfalls`, schema 6): the place, the impact time and bearing, the deposit's id and the ore left. A restart puts the deposits back after the region file's own |

On the wire, the world's sky (`net::Sky::starfalls`, protocol 9) carries
every fall told: its id, region (an index in the World message's regions),
impact point on that region's ground, impact time and bearing, and its
deposit's index once landed.

On screen (`AAcStarfall`, `unreal/Source/Autocraft/AcStarfall.cpp`; the
materials from `Tools/Editor/make_starfall_materials.py`):

- **The streak.** For its last 10 s the head comes in at 70 cells a second,
  16° under level, out of the giant's side of the sky (bearing 280° ± 40°).
  It has a white-hot head with an orange halo that flickers and flares as it
  burns away, sheds sparks, and carries a light that sweeps the land below.
  Behind it a glowing trail stretches along the way it came: orange where
  the head just passed, cooling to a faint pale train that fades over
  4.5 s, long enough to cross a good part of the sky. It leaves a smoke
  trail that glows near the head, then lingers for 30 s, spreading and
  drifting on the wind. It is drawn from the regions next door too, placed
  by the world grid.
- **The streak (E1).** Its head 11 cells across and growing as it comes,
  a 36-cell halo and a long tail, so it is seen from anywhere in the region.
- **The impact (E1).** A fireball that swells over the blast's ground and
  fades in 2.2 s, its light reaching 14 blast radii (across the region); the
  screen flares white for 1.1 s, 0.4 near the impact and 0.18 from the
  region's far side (`StartCameraFade`). A shockwave: a ring of hot, bright
  air racing out to 1.6 × the blast's radius over 2.6 s, a ring of dust
  behind it. Rocks thrown out of the crater glowing, falling back over the
  blast's ground. A dust ring out to 1.5 × the blast's radius and a column
  of smoke 50 cells high, lit from below. All of it settles in 12 s.
- **The quake (E1).** The ground heaves under the camera within four blast
  radii (72 cells), hardest near, for 3.6 s (`UAcEffects::Quake`): the
  pilot's view a few degrees and a jolt, the top-down view at any height.
- **The crater.** A decal (`M_StarfallCrater`) 21 cells across: a scorched
  pit with glowing seams while it cools (10 minutes), a pale rim of thrown
  rock and rays of dust. It fades out over the last fifth of its day. Under
  it the blast's ground, scorched faintly, the blast's width (E1, the same
  decal).
- **The deposit.** Drawn 1.3 × a plain one's size in `M_OpalHot`: a dark,
  matte cooling crust cracked by molten veins, gold at their hearts and
  orange to deep red at their edges, that breathe, with sparks in the
  cracks. Their glow is kept low: bright orange past the tonemapper's
  shoulder reads white. A warm light glows with it, stronger at night, when
  the veins burn 3.5 × brighter and embers rise off it. By day only a few
  small embers rise; the air over it shimmers instead (`M_StarfallShimmer`,
  a 2D-offset refraction on soft columns), most while it is hot.
  `AAcResources` leaves it out of the plain ore (`AAcResources::Hot`).
- `ac.StarfallLightGain` scales the lights.
- **Sound** (`Tools/AudioGen/moments.manifest.json`; `FAcAudioRules::Starfalls`,
  tested in `Autocraft.Audio.Rules.Moments`). The streak (`sfstreak`) is a
  whistle that rises to a roar over its last 6.8 s and stops dead at the
  impact. The take is a hit that dies away, played backwards
  (`Tools/AudioGen/rise.py`, the raw take in `Tools/AudioGen/raw/`). The
  boom (`sfboom`, two takes) arrives at the speed of sound (a cell is a
  metre), and its roll off the hills (`sfecho`) 1.6 s after it. E1: close
  by the blast itself with the boom (`sfimpact`, the air torn apart and the
  debris thrown out, heard to 180 cells) and the ground shaking after it
  (`sfrumble`, a 9 s quake that calms, felt to 120 cells), both made with
  ElevenLabs sound effects (`moments.manifest.json`). They are
  heard across the region and the next: from 500 cells off at 0.3 of their
  gain, from the impact's side whatever the distance. A hot crater in the
  pilot's region hums and crackles (`sfcrater`), placed at the crater,
  cooling over its day.

To drop one now near the pilot, use `--starfall S` on the server: the
first pilot that drives gets a fall 40–58 cells away (ahead of its
Prospector first, coming in from the side), S seconds after it comes, told at once. `--clock-hour H`
moves the world's clock on to hour H and lets it run. On the game,
`-AcMmoStarfall=S` and `-AcMmoClockHour=H` pass them on. The picture run is
`-AcMmoShots=DIR -AcMmoShotsStarfall` (AcMmoShots.cpp, `sf-01` to `sf-09`):
it adds `-AcMmoStarfall=10 -AcMmoClockHour=17` unless given others, so the
fall comes 10 s after the pilot drives and the streak is up at once. For
the night it moves the world's clock on to 19:00 at once (the server's
`--dev-control` file, shot runs only), so the run takes under a minute,
not four. `-AcMmoShotsOnly=sf-02,sf-08` shoots only those moments; with
none of `sf-02` to `sf-05` asked for, the clock jumps past the impact.

E1's set, `-AcMmoShots=DIR -AcMmoShotsE1` (`e1-01` to `e1-06`, about a
minute): the fall 30 s after the pilot drives, 17:00, clear. The pilot walks
to 30 cells of the impact (its blast reaches 18); 7 s before it the server
puts the realm's buildings and Prospectors in its way (the dev line
`starfall victims`: a Hab Dome at its heart, a Gatling Gun and a Sentinel
inside, a Hab Dome near the edge, Prospectors from the heart to the edge).
The pictures: the world map with the start's two veins and its well, what
stands in its way, the streak, the impact, the shockwave, and the crater
with what it destroyed and what it hurt.

## Cost

The budget is under 1 ms a frame at 5120×1378 on the M4 Max for
everything above, in the worst case (a storm): the rain is one draw call
of thin quads, the wet ground a few instructions in the terrain's own
shader, the lightning a light and a few parameters, the cloud shadows a
light function. `bench.py --gpu` measures it (see the GAME-LAYER.md row
"Weather").

A view ray's 96 samples are spread over 2 km
(`r.VolumetricCloud.DistanceToSampleMaxCount=2`; the default 15 made a
step ~156 m, and the cloud edges a salt-and-pepper fuzz), at no
measurable cost. The clouds are traced at half resolution with no rebuild
over frames (`r.VolumetricRenderTarget.Mode=1`, `DefaultEngine.ini`, since
docs/mmorpg-m6.md V2: the default mode 0's quarter resolution left every
cloud edge stair-stepped in the pilot's view). The cloud pass's cost and
the look are in "Clouds (2026-10-18)" below.

## Clouds (2026-10-18)

The user's verdict on the clouds before this pass: far behind the clouds of
the engine's own sample level, the one the editor opens on (the project sets
no startup map, so it is `/Engine/Maps/Templates/OpenWorld`: the engine's
`m_SimpleVolumetricCloud` on a 5-15 km layer under a 6 lux atmosphere sun,
the same strength as our `SkySun`). The target: cumulus with sunlit tops and
dark bases, soft eroded edges with no grain or popping, a silver lining
against the sun, depth to the horizon, a storm that reads as a storm, at no
more than today's cloud pass plus 0.3 ms.

### Side by side

`ac.Daylight.CloudReference engine` swaps the OpenWorld template's clouds
(material, layer, the component's defaults, no lift on the sun) into our
sky, so both are drawn under the same sun, atmosphere, camera and exposure;
`ac.Daylight.CloudReference /Path/To.Material[;bottom;height]` takes any
other cloud material. `-AcCloudShots=DIR` (`AcCloudShots.h`) renders every
pairing of hour, weather, view and look in one hidden run, about 2 s a
picture at 5120×1378, in the RTS game or (`-AcMmo -AcMmoNewWorld
-AcMmoHour=H -AcMmoWeather=W -AcCloudShotsDelay=60`: the world's sky
arrives with its snapshots, so an MMO run waits) in an MMO region; with
`-AcCloudShotsPan` it records a turning camera instead. A look can also set
console variables for its picture only, which is how every value below was
chosen: `ac.Daylight.CloudParam "CloudE=12:45;CloudM=:0.4"` sets the
material's vector parameters over the frame's own.

The Volumetrics plugin's sample clouds (`-EnablePlugins=Volumetrics`,
hidden runs only) were tried as a second bar; `CF_Midday` and `ED_Storm`
draw nothing under our setup (their maps drive them from Blueprints), so
they are not a reference.

### Why the engine's looked better (round 0)

- **Density.** Ours had an extinction of 0.15 per metre, the engine's about
  0.008-0.04. Every view step through ours was opaque: hard cut-out edges,
  blue-grey speckle where a step landed inside a cloud and the next outside,
  grey opaque clouds against the sun with no light through them, and dark
  brown clouds at dusk where the engine's glow orange.
- **Edges.** The density reached full within 30 % of the cloud's own range,
  so a cloud was a solid body with a knife edge.
- **Shape language.** Small vertical puffs evenly scattered (popcorn); the
  engine's are broad, flattened, wind-stretched bodies with a few fragments
  round them.
- **Horizon.** A fade carried over from the Swift sky removed every cloud
  below ~17° of elevation, so there was no depth to the horizon.
- **Storm.** One flat featureless dark sheet.
- **Light.** The high sun's light on the clouds was lifted ×2.5 to make the
  dense clouds' tops stand out; thin clouds do not need it.
- **Two pops in time.** The shape noise repeated 2.5 times per tile and its
  wind offset wrapped every 4 km, so every ~4.6 minutes (at 15 m/s) the
  shape jumped half a tile; and the clouds' lean downwind took its direction
  from the wrapped offset, which flipped when one axis wrapped.

### What changed

`Tools/Editor/make_cloud_material.py` (the graph), `bake_clouds.py` (the
weather map), `AcDaylight.cpp`:

- **Density 0.04 per metre** (`CloudDensity`), a soft rim (density grows
  over the cloud's whole range, `CloudD.x`; at 0.7 a cloud toward the sun
  still read as a cut-out) and denser cores (×2, `CloudE.z`).
- **The body** (`body()`, shared by the clouds and their shadows): the
  weather map is read along the wind and stretched 2× along it
  (`CloudW`, `CloudStretch`), so clouds come in streets and broad bodies;
  its cover is now a few big Perlin bodies with Perlin-Worley clumps round
  their edges (`bake_clouds.py`), its alpha says where each cloud's base
  sits (±255 m, `CloudF.y`); the shape noise repeats a whole number of
  times per tile (2 near, 1 far and in a deck), and the lean follows the
  wind's direction.
- **Fair weather sits higher.** The layer stays 1.6-5 km (a rain deck's
  base, where the bolts come from, to the storm's tops), but clouds with sky
  between them sit 0.9 km up in it (`CloudFine.z`), from 2.5 km: at 1.6 km
  the pilot's sky felt like a low ceiling.
- **Detail.** The edge erosion has a second octave four times finer within
  4 km (`CloudFine.xy`): crisp billows on the masses overhead.
- **Light.** The engine's sample phase (g 0.8 with a 0.17 lobe) and
  multiple scattering (0.67/0.25/0.18) side by side against a sharper
  forward lobe and more, more occluded multiple scattering (g 0.85 / 0.25,
  blend 0.5; 0.8/0.4/0.4, `CloudP`, `CloudM`): the latter gives the brighter
  rim against the sun and the darker bases, and is the default. Occlusion
  0.5 and 35 % of the sky light at a cloud's base (was 60 %) give the
  undersides the grey that made the engine's noon clouds read airy (0.6
  and 30 % turned 17:30's clouds stormy grey; a sun lift of 1.0, the
  engine's multiple scattering or density 0.03 barely changed noon). The
  sun's lift on the clouds ×1.3 (`ac.Daylight.CloudSun`, was 2.5).
- **Horizon.** Far fair-weather cloud thins out from 8 to 30 km
  (`CloudE.xy`; at 12-45 km single puffs survived as a row of white
  cloudlets on the horizon) and takes the horizon's air (`Haze`, rising as
  the sky dome's does): the pilot's horizon tone at half its light
  (`CloudHazeGain`). The dome's own haze tone was brighter than the horizon
  reads and stacked the far clouds into a white wall. Past 8 km the fine
  billows and the erosion fade out (gone by 25 km): smaller than a traced
  pixel, they only shimmer.
- **Rain and storm.** A deck has no holes: at least the shape noise itself,
  thick where the weather map's fine clumps are and thin between (darker
  masses, lighter patches). With the coverage cut alone its rifts were holes
  to the sky, and seen edge on they lined up into a clear band along the
  horizon. Its base is ragged by the same clumps (±200 m), it stays whole to
  the tracing's end, the ground's bounce on it goes with the gloom (the
  material's ground contribution assumes a sunlit ground, and lit the far
  deck's underside as a white strip), and over the gas giant it thins only a
  quarter as much (a clear hole in a storm read as a fault). Under a deck
  the edge erosion bites a third as hard and the fine octave is off. Storm clouds
  are dark blue-grey (albedo 0.30, 0.34, 0.375) and keep a third of the sky
  light at their base, as the engine's sample storm clouds do.
- **The murk.** Under gloom the first-person haze tone (`Tones.Haze`, which
  the sky dome's horizon and the pilot's veil over the far ground take) goes
  darker and greyer and rises to 32°: the far land is lost in the storm's
  grey. With the fair sky's haze the veil painted the far ridges under the
  deck clear-sky blue, a bright strip that read as the deck ending.

The shots, the engine above ours: `art/sky/clouds/before` (round 0) and
`art/sky/clouds/final` (MMO region, noon, 17:30 and 18:18, clear, cloudy,
rain and storm).

### The views and what is left

- The world map (M) shows region pictures baked offline (`AcMapBake`,
  clouds, fog and atmosphere off, clear weather): no clouds and no cost
  there. The MMO's top-down view (Esc) sees the clouds as their shadows: a
  region is 144 m and a cloud several km, so a shadow passes over it as a
  whole-map dimming. With the cover held at 0.95 the ground is 7 % darker
  with the shadows than without, and a forced full shadow is 27 % (the
  ground keeps its sky light and fill); a storm dims the sun ×0.3 through
  the gloom besides.
- The storm's far seam: toward a low sun, a band of grey-blue far deck and
  the sun's glow through the haze over the far ridges stay under the
  ceiling. A self-shadow reach of 12 km (`ac.Daylight.CloudShadowKm`, 4)
  and a third of the erosion did not change it; it reads as the light at
  the edge of a storm, and the 4 km reach is kept (12 costs more).
- The rain deck's thin patches show a fine cellular grain at 1:1 at 5K:
  cells of one quarter-resolution traced pixel (6-8 px), mode 0's
  reconstruction of a busy, half-transparent patch. Full self-shadow steps
  and 128 view samples keep it; `r.VolumetricRenderTarget.UpsamplingMode 3`
  removes it but doubles the pan pops (5.2 vs 2.8 at 30 fps, 3.1 vs 0.7 at
  60) and mode 1 turns it into speckle, so it stays.

### Cost and time

The clouds are traced at quarter resolution and rebuilt over frames
(`r.VolumetricRenderTarget.Mode=0`), and a ray steps three times as far
through air the height profile marks empty
(`r.VolumetricCloud.StepSizeOnZeroConservativeDensity=3`), both in
`DefaultEngine.ini`.

The `VolumetricCloud` pass, ms a frame (mean of 2 rounds, `bench.py
--gpu`; the pilot shot looks at the sky, the MMO home view barely does and
its harness holds the weather clear):

| | clear | cloudy | storm | load |
|---|---|---|---|---|
| pilot 5120×1378@2, before (d28e66e) | 1.45 | 2.14 | 1.96 | 9-13 |
| pilot, half resolution (mode 1), the new look | 1.53-1.56 | 2.68-3.08 | 0.74-1.17 | 8-31 |
| pilot, mode 0 | 1.05 | 1.97 | 0.61 | 9-13 |
| pilot, mode 0 + step 3 (final) | 0.68 | 1.83 | 0.58 | 7-10 |
| pilot, with the grey bases and the deck's lighter erosion | | 1.83 | 0.48 | 11-13 |
| MMO home noon 5120×1378@2, before (bcae75c) | 0.54 | | | 10-19 |
| MMO home noon, final | 0.25 | | | 7 |

Priced on the way (pilot, cloudy): the fine erosion octave 0.08 ms; 72
view samples -0.64 ms but 2.5× the pops in mode 1; 128 samples no change
in mode 0; self-shadow steps ×0.6 -0.33 ms but more pops; the engine's
`EmptySpaceSkipping` no gain.

**Time.** `-AcCloudShotsPan=3 -AcCloudShotsSeconds=3` at a fixed 30 and
60 fps (MMO, 17:30, cloudy, toward the sun) and `flicker.py`-style counting:
a pop is a pixel more than 10/255 off both neighbouring frames in the same
direction, per 10,000 cloud pixels a frame, on the sky (the top quarter of
the frame; lower, the mesa ridgelines' own edges dominate). Each MMO world
is new, so its clouds differ from run to run (identical settings varied
2×); only the engine and ours in the same run compare:

| | pan 30 fps | pan 60 fps | still 30 fps | still 60 fps |
|---|---|---|---|---|
| ours, mode 1 | 7.1 | | | |
| ours, mode 0 | 1.9-3.6 | 0.8-0.9 | 0.01 | 0.01 |
| the engine's clouds | 0.95-1.3 | 0.36 | 0.00 | 0.00 |

Still, the clouds are as steady as the engine's (no flicker, no popping as
they drift); under a turning camera ours resample about twice as many
pixels per frame, their sharper billows (the engine's are soft blobs),
about 0.02-0.03 % of the cloud a frame. The reprojection's box constraint
(`r.VolumetricRenderTarget.ReprojectionBoxConstraint 1`) made the still
camera worse. The bar was 1.5× the engine's pan rate; it was not chased
further because what is left is detail, not instability (the still camera
is as steady as the engine's), and the only lever left is less edge detail,
which is the look this pass was for. Re-open it only with a measure that
tells resampled detail from flicker.

## Console and flags

- `ac.Weather` (-1 the schedule; 0 Clear, 1 Cloudy, 2 Rain, 3 Storm hold
  a state; fractions blend, so 2.5 is between rain and storm);
  `-AcWeather=clear|cloudy|rain|storm` for shots.
- `-AcWeatherSeed=N`; `-AcWeatherT=S` samples the weather at S seconds
  (with the seed and a held storm, a still can land on a strike);
  `ac.Weather.Rain` (scale on the rain streaks),
  `ac.Weather.Wet` (wet ground), `ac.Weather.Lightning` (0/1).
- The clouds' own: `ac.Daylight.Clouds`, `ac.Daylight.CloudShadows`,
  `ac.Daylight.CloudDensity`; `ac.Daylight.CloudCover` overrides the
  weather's cover; `ac.Daylight.CloudBottom` and `CloudHeight` the layer
  (km); `ac.Daylight.CloudSun` the sun's lift on them;
  `ac.Daylight.CloudReference` another cloud setup side by side and
  `ac.Daylight.CloudParam` the material's look parameters ("Clouds
  (2026-10-18)"); `-AcCloudShots=DIR` (`AcCloudShots.h`) the pictures.
