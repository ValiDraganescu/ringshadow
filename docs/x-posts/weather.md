---
status: posted
date: 2026-10-12
angle: world
video: video/renders/weather-x.mp4
posted: https://x.com/escu__valentin/status/2109637295884116183
xdraft: xd_0f8ce56e
---

## Post

Ringshadow is set on Ashfall Reach, a mining frontier under a gas giant, and now it has weather. Clear sky, broken cloud, rain and storms roll in on their own over the day and the night.

The clip is a storm passing over, sped up 16 times, from the seat of a Prospector I'm driving: rain darkens the sand and fills puddles, drops run down the cockpit glass, then the sky clears to cumulus. Hulls go glossy in the wet, and lightning lights the cloud above each strike.

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

docs/weather.md is on main since 95beeee (2026-10-12), so the link in post 4 works.

Videos, one per post (all 1920x1080, H.264 with sound, under 10 MB for X):
- Post 1: video/renders/weather-x.mp4 (26.1 s, 9.5 MB). Sim `weather-storm-clears` in .claude/skills/ringshadow-video/sims.json: a Prospector driven in first person at 16:30, seed 42's schedule from 2185 s at 16x (`-AcWeatherFrom=2185 -AcWeatherSpeed=16`, AcWeather.h): the storm's tail, the rain easing, cumulus by ~2435 s, then the pull-out to the gas giant. At night and at dusk the storm was too dark to read. Cut: `uv run .claude/skills/ringshadow-video/scripts/giantcut.py video/clips/weather-storm-clears.mp4 --down 14.65,19.0 --end 21.0 --title "Storms roll in and pass." --caption "Time-lapse, 16x: a storm passes over" --caption-at 0.5,7 --music rising_pressure_1 --music-at 20 --out video/renders/weather.mp4 --x`. The flashes in it are faint by day; the lightning has its own clip.
- Post 2: video/renders/weather-lightning-x.mp4 (16.0 s, 9.2 MB). Sim `weather-lightning`: the sky view at 23:00 facing yaw 21, a held storm, weather time from 48 s, so the close strike (2.6 km) lands at ~3.4 s and its thunder ~7.6 s later. Cut: `giantcut.py video/clips/weather-lightning.mp4 --end 14.0 --title "Lightning in the cloud." --caption "A strike 2.6 km out lights its cloud" --caption-at 2.5,8 --music-ends --music night_shift_1 --music-at 10 --out video/renders/weather-lightning.mp4 --x`.
- Post 3: video/renders/weather-flicker-x.mp4 (5.3 s, 2.2 MB, silent). The comparison clips of 2026-10-12 (sky view at 14:00, seed 42, 50 % render scale, the 2 km sample spread; mode 1 against mode 0), the same cloud edge at 3x side by side, composed with Pillow and played twice. Labels: the measured pops, 56 and 8 per 10,000 cloud pixels.

Facts, all from docs/weather.md and the 2026-10-12 session:
- Post 1: four states (clear, cloudy, rain, storm) on a seeded schedule tied to the game clock, so day and night carry on under it; wet ground and puddles in M_Terrain; wet hulls; rain on the glass only while driving in first person; thunder after distance / 343 m/s. Cost: bench.py --gpu at 5120x1378 put the weather at +0.7 to +1.0 ms GPU over clear (fight 13.5 to 14.5 ms, pilot 16.2 to 17.0); the cloud pass later dropped to 1.3 ms cloudy in the cockpit.
- Post 2: the uniform cloud `Glow` had to sit at 0.08 (4 and 0.6 whited out the frame through the real-time sky capture). The developer brought a PDF on the UE 5.5 storm clouds; the engine's m_SimpleVolumetricCloud lights patches from a lightning volume texture. Ours: `BOLT_HLSL` in Tools/Editor/make_cloud_material.py, a core and a halo round `Bolt` (strike position, radius 2.5 km), glow 0.2 at full flash.
- Post 3: recordings with -AcShotRecord at -UseFixedTimeStep -FPS=30, scored by counting pixels whose luminance jumps more than 25/255 between frames, per 10,000 cloud pixels. Fixes: r.VolumetricCloud.DistanceToSampleMaxCount=2 (from 15), r.VolumetricRenderTarget.Mode=0 (from 1), the material's distance level of detail (6 to 20 km). "Grok's research" is the ask_grok answer that named DistanceToSampleMaxCount.
