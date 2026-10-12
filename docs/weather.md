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
