---
status: proposed
date:
angle: world
video: record: one cockpit shot under a sky that turns from broken cumulus to a storm deck: rain darkening the sand, puddles, drops on the glass, a close strike with its bolt and the cloud lit above it; night, so the gas giant shows. Posts 2 and 3 can carry their own clips: a close strike in the sky view (-AcSkyView=21 -AcWeather=storm -AcWeatherSeed=42, the strike at 53.44 s) and the flicker before and after (the clips on https://claude.ai/artifact/DeJV7LodhrH83abULYTH6m)
posted:
xdraft:
---

## Post

Ringshadow is set on Ashfall Reach, a mining frontier under a gas giant, and now it has weather. Clear sky, broken cloud, rain and storms roll in on their own over the day and the night.

In the clip: cumulus over the dunes, then a storm deck. Rain darkens the sand and fills puddles, hulls go glossy, and when you drive a unit (take it over and play it yourself) drops run down the cockpit glass. Lightning lights the cloud above each strike, and the thunder arrives distance / 343 m/s later.

Claude Code built all of it. The whole system costs about 1 ms of GPU time at 5K on an M4 Max.

## Reply

The storms needed lightning that looks right from the cockpit. The first version lit every cloud in the sky at once on each strike, which turned the whole frame white, so it got turned down until a strike barely showed.

Unreal's sample cloud material lights one patch of cloud. Claude rebuilt the idea from the game's own strikes: each one knows its direction and distance, so the cloud right above it glows from inside, a bright core in a halo 2.5 km across, and the rest of the storm stays dark.

---

Claude can't see the game move. So when I told it the clouds flickered, it recorded 30 fps clips of the sky in a hidden copy of the game and counted the pixels that jumped between frames.

It found three causes. Each cloud ray spread its 96 steps over 15 km, about 156 m apart, so cloud edges came out salt-and-pepper; Grok's research turned that one up. The half-resolution mode it had picked never averaged frames: 56 popping pixels per 10,000, against 8 in the engine's default. And far clouds had billows smaller than a pixel. They now smooth out past 6 km, and the horizon went from 2.4 pops to 0.

---

How the weather works, from the cloud material to the thunder delay: github.com/ValiDraganescu/ringshadow/blob/main/docs/weather.md

## Notes

A thread of four: the Post is post 1, and the Reply holds posts 2 to 4, split at the lines with only `---` (x_create_draft gets four posts). Each post is under 700 characters.

Before posting: docs/weather.md is not committed yet, so the link in post 4 would 404 until the weather work is committed and pushed.

Facts, all from docs/weather.md and the 2026-10-12 session:
- Post 1: four states (clear, cloudy, rain, storm) on a seeded schedule tied to the game clock, so day and night carry on under it; wet ground and puddles in M_Terrain; wet hulls; rain on the glass only while driving in first person; thunder after distance / 343 m/s. Cost: bench.py --gpu at 5120x1378 put the weather at +0.7 to +1.0 ms GPU over clear (fight 13.5 to 14.5 ms, pilot 16.2 to 17.0); the cloud pass later dropped to 1.3 ms cloudy in the cockpit.
- Post 2: the uniform cloud `Glow` had to sit at 0.08 (4 and 0.6 whited out the frame through the real-time sky capture). The developer brought a PDF on the UE 5.5 storm clouds; the engine's m_SimpleVolumetricCloud lights patches from a lightning volume texture. Ours: `BOLT_HLSL` in Tools/Editor/make_cloud_material.py, a core and a halo round `Bolt` (strike position, radius 2.5 km), glow 0.2 at full flash.
- Post 3: recordings with -AcShotRecord at -UseFixedTimeStep -FPS=30, scored by counting pixels whose luminance jumps more than 25/255 between frames, per 10,000 cloud pixels. Fixes: r.VolumetricCloud.DistanceToSampleMaxCount=2 (from 15), r.VolumetricRenderTarget.Mode=0 (from 1), the material's distance level of detail (6 to 20 km). "Grok's research" is the ask_grok answer that named DistanceToSampleMaxCount.
