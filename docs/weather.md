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

Two or three times a world day a meteor streaks across the sky under the
gas giant and lands somewhere in the world. It leaves a crater with a rich
deposit, first come, first served (docs/mmorpg-m7.md E2). It runs in the
MMORPG mode only. The server owns it, the same for every pilot; the client
only draws it and tells the pilot.

The schedule, the numbers and the streak's path are `ac::Starfall`
(`unreal/Source/AutocraftCore/Public/Starfall.h`). The server's part is
`WorldStarfall.cpp` (`unreal/server/world/`), tested in
`server/tests/StarfallTests.cpp`.

| What | Value |
|---|---|
| When | 2 or 3 a world day, by dice seeded with the world's id: one in each equal share of the day, in its middle 70%. The same world falls at the same times on every server and after every restart |
| Where | picked when it is told, a minute before the impact. Any region, weighted toward the pilots: each region counts 1, each pilot in it 8, each pilot next door 3. Then a spot on open ground: flat (no cliff, no rock under the crater's 4 cells), joined to the region's pads, at least 12 cells from the map's edge, clear of pads (by 11 cells past the crater), sites (8), ore (6.5), wells (7.5), buildings, units and other craters |
| The warning | 60 s before, to every pilot: the KSTR radio's toast, "STARFALL IN 60", with the region, the bearing and the distance from the pilot. Again at the impact ("STARFALL DOWN") in the pilot's region and the ones around it |
| The deposit | at the impact a deposit appears in the crater with 3 × the region's median deposit's ore (at least 3,000). Ore drilled from it pays ×1.5 XP: the extra half when the load is drilled (to the pilot whose Prospector stands at the deposit), the plain XP when it is unloaded. It never grows back: emptied, it is gone |
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
- **The impact.** A flash of light that swells and fades in 1.4 s, a ring of
  dust thrown 30 cells out, and a column of smoke lit from below. All of it
  settles in 9 s.
- **The crater.** A decal (`M_StarfallCrater`) 21 cells across: a scorched
  pit with glowing seams while it cools (10 minutes), a pale rim of thrown
  rock and rays of dust. It fades out over the last fifth of its day.
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
  metre), and its roll off the hills (`sfecho`) 1.6 s after it. They are
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
measurable cost. The clouds are traced at quarter resolution and rebuilt
over frames (`r.VolumetricRenderTarget.Mode=0`, the engine's default). The
engine moves each ray's start every frame, and mode 1 (half resolution,
no rebuild) showed that as pixels popping on bright edges: 56 per 10,000
cloud pixels a frame against 8 (hidden 30 fps clips). In the pilot bench
the cloud pass is 1.3 ms cloudy and 1.0 in a storm.

The look (`Tools/Editor/make_cloud_material.py`, `AcDaylight.cpp`): the
shape noise 2.5 times per tile and only its densest part let through
between clouds, so a cloud is billows on billows, not one cotton slab; a
short edge into full density; erosion only on thin edges; less sky light
at a cloud's base; and the high sun's light on the clouds ×2.5
(`ac.Daylight.CloudSun`, the light's cloud scattering only, the sky
unchanged), which brings out sunlit tops. A low sun gets ×0.6 (dusk went
neon), the moon ×1, and under gloom both go to ×0.4. A rain or storm
deck keeps the wide shape and soft edge: small dense lumps let the sun
through in white patches. Past 6 km the fine billows and the erosion fade out (gone by
20 km): from the cockpit the far clouds near the horizon were billows
smaller than a traced pixel, which only shimmered (pops 2.4 per 10,000
cloud pixels a frame, 0 with the fade). A storm's clouds go dark blue-grey (albedo
0.30, 0.34, 0.375) and keep a third of the sky light at their base, as
Epic's sample storm clouds do.

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
  weather's cover.
