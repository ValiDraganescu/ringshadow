# Performance

The standing goal since 2026-10-18: make the client and the server faster,
a measured step at a time. Each round names the bench, the numbers before
and after (run in turns on the same Mac, with its load), and what stays
equal. The client's rounds are under "Client", the server's under "Server".

## Client

The game as a player runs it in the MMORPG world (`unreal/Source/Autocraft`,
the core as the client runs it, the renderer settings). What the player
sees stays the same, and the core stays bit for bit. Only the MMO counts
(the RTS mode is legacy).

**Where players stand** (2026-10-18, after round 4; the cooked
Development build players get, beside the editor binary the rounds
measure; MMO bench, 2 rounds each in turns, load 6-17, ms and MB):

| scene | 1920×1080 GPU p50, cooked / editor | 5120×1378 GPU p50, cooked / editor | game thread p50, cooked / editor | memory, cooked / editor |
|---|---:|---:|---:|---:|
| home noon | 9.9 / 10.1 | 13.9 / 14.1 | 1.8 / 2.0 | 7,675 / 10,226 |
| home midnight | 10.5 / 10.7 | 14.7 / 14.9 | 1.7 / 2.0 | 7,707 / 10,346 |
| nest at noon | 9.0 / 9.2 | 12.7 / 12.7 | 1.7 / 2.4 | 7,736 / 10,411 |
| Fork, wave 10 | 9.4 / 9.4 | 13.0 / 13.0 | 2.1 / 2.1 | 7,836 / 10,512 |

After round 6, the cooked game at 1920×1080 holds 7.5-7.8 GB (124-387 MB
less) and its GPU frame is 0.15-0.25 ms shorter (round 6's table).

The frame is the GPU's in both builds and at both sizes: the render
thread waits on it, and the game thread takes about a fifth of it. The
cooked game's GPU frame is 0-0.3 ms shorter (the editor's own sky pass,
0.1-0.15 ms, is gone). The rounds' editor numbers stand for players' to
within that. The footprint (`task_info`, the GPU's memory included on the
M4 Max's unified memory) is 7.7-8.8 GB cooked.

**The MMO bench** is the standard: `uv run unreal/Tools/bench/bench.py
--mmo [--rounds N] [--gpu] [--variant NAME=FLAGS ...]`. Each run is one
world client through `hidden_run.py` (its own slot, so A/B variants take
turns run by run) on highlands-small's world with clear weather, running
the `-AcMmoShotsBench` set (AcMmoShots.cpp). It has four scenes, each
settled, timed `--seconds` (10) on the wall clock and then pictured:

- `mmo-home-noon`: Prospect Landing at noon, facing the pilot's Citadel.
- `mmo-home-night`: the same at midnight, with the lamps and the headlamp.
- `mmo-nest-noon`: Slag Pockets at noon, 55 cells from the nest, her horde in sight.
- `mmo-fork-wave`: Cold Fork at noon, wave 10 down The Fork, facing the nearest bug.

`FAcBenchMeter` (AcBenchMeter.h) times the scenes. It reports the wall
frame (p50, p95, p99, max, frames over 33 ms), the game, render and RHI
threads, the GPU frame and the driver's GPU busy time, the game's tick and
its parts, and memory. With `--gpu` it adds `stat gpu`'s passes, read
after the timed frames. `report.md` has the table and the passes. On Metal
a pass's time can include earlier GPU work: trust an A/B toggle over a
single pass. `ac.HudHide 1` hides the whole HUD, to measure what it costs.
The numbers come from the editor binary (`UnrealEditor -game`); a cooked
game skips the editor's own passes (`SkyAtmosphereEditor`, 0.1 ms) and its
checks.

### Round 1: the bench, and the ISM materials kept (2026-10-18)

**Where the frame goes.** The MMO client is GPU-bound in every scene. At
1920×1080 the game thread takes 2.3-3.4 ms of an 11-16 ms frame, and the
render thread waits on the GPU. The GPU passes at home at noon (`--gpu`,
ms a frame): shadow depths 3.7, deferred lighting 3.5, Lumen screen probes
2.0, post 1.5, TSR 1.2, lights 0.9, clouds 0.6. At midnight the shadow
depths take 5.0 ms. The HUD costs no measurable GPU time: with `ac.HudHide
1` the GPU frame and the 1.8 ms "Slate UI" pass stay the same (Metal puts
earlier work under that marker). The HUD costs about 0.3 ms of the game
thread. The console's warp (`ac.ConsoleWarp 0`) changes nothing measurable.

The game thread (xctrace, nest at noon, 18 s) is 22 % busy. Of that, the
game's tick and its frame stages are 26 %, Slate 18 % and the engine's
end-of-frame updates 15 %.
In those updates every instanced mesh with moved instances works out its
materials' property descriptor again from each material's relevance
(`GetUsedMaterialPropertyDesc` -> `GetUsedMaterials`), 5.4 % of the game
thread. On the RTS bench's late8 (1,450 units) it is 5.3 % of a
game-thread-bound frame.

**The change.** `UAcInstancedMesh` (the renderer's batches, the life bars,
the ore, the effect pools, the particles) overrides
`BuildComponentInstanceData`. It does the engine's work line for line but
keeps the descriptor. It works it out again when the mesh, the override
materials or the shader platform change, and every
`ac.RenderMaterialDescFrames` (64) updates in case a shader finished
compiling. 0 is the engine's path, every update, for A/B. The override
copies the engine's body as UE 5.8 has it: after an engine update, compare
it with `UInstancedStaticMeshComponent::BuildComponentInstanceData` again
(0 is the way out meanwhile). Conservative
bounds are never on for these components, so the engine's other branch
never runs.

| MMO bench, 2 rounds in turns, load 7.6-7.7 | game p50 | game p95 | RHI p50 | frame p50 |
|---|---:|---:|---:|---:|
| home noon: engine / kept | 3.5 / 3.0 | 3.9 / 3.3 | 2.0 / 1.7 | 13.9 / 13.6 |
| home midnight | 4.1 / 3.3 | 4.5 / 3.7 | 2.3 / 1.8 | 16.7 / 15.7 |
| nest at noon | 2.6 / 2.4 | 3.0 / 2.5 | 1.7 / 1.5 | 11.3 / 11.1 |
| Fork, wave 10 | 2.9 / 2.6 | 3.1 / 2.8 | 1.8 / 1.6 | 11.7 / 11.7 |

The game thread's p95 drops by 10-18 %. The frame stays where the GPU puts
it. The pictures are the same: the descriptor is the one the engine would
work out.

**Before the MMO-only rule**, the RTS late8 bench (1920×1080, 1,450
units, load 6.5-7) was game-thread-bound: GPU p50 13.2 ms, game p95 22.6
ms. Its game thread was the sim step (37 %: `Vision::sight`/`stamp` 12 %,
the server worker's), the renderer's sync (15 %) and the engine's instance
updates (18 %).

**Next.** The biggest GPU cost is the virtual shadow maps. The shadow
lights turn a little every frame with the world's clock, and any turn
throws away the whole shadow cache. With the sun held still, shadow depths
drop from 3.65 to 1.31 ms at home and from 2.81 to 0.95 ms at the nest, so
the GPU frame at home goes from 13.1 to 10.8 ms. `r.Shadow.Virtual.Cache=0`
measures the same as today. Round 2 turns the shadow lights in small steps.

### Round 2: the shadow lights turned in steps (2026-10-18)

**The cost.** The virtual shadow maps keep a light's pages from frame to
frame only while the light holds still: the engine keys its cache on the
light's exact rotation (`VirtualShadowMapCacheManager`, UE 5.8). AcDaylight
set the sun's rotation and the ringlight's (the "Moon", which casts the
night's shadows) every frame as the world's clock turns (a 2,400 s day,
about 0.15° a second), so every page was drawn again every frame.
`r.Shadow.Virtual.Cache=0` measured the same as the game did.

**The change.** `TurnShadowLight` (AcDaylight.cpp) turns a shadow-casting
light only once its true direction is a step away from where it points. A
step is at most `ac.Daylight.ShadowStepDeg` (0.5°), and at most what moves
the shadow tip of an object h metres tall by 0.1 m. That tip moves
h / sin²(e) per radian of elevation e, so the step is 0.1 / h · sin²(e)
radians. For the sun h is 20 m (`ac.Daylight.ShadowStepMetres`, a
Citadel's mast), which gives 0.18° at the bench's noon (53° up), 0.034° at
20° and 0.015° at 13°. For the ringlight h is 5 m
(`ac.Daylight.ShadowStepMetresMoon`), because its shadows are faint
(0.75 × the fade). The sky's own pair (SkySun, SkyMoon: the disk, the
atmosphere, the clouds' light) stays continuous. `ac.Daylight.ShadowStepDeg
0` (or a light's metres at 0) turns the lights every frame, as before.

| MMO bench, round 2 of 2 in turns, load 6-9, ms | GPU p50 | GPU p95 | shadow depths | the frame after a step |
|---|---:|---:|---:|---:|
| home noon: every frame / stepped | 13.2 / 10.8 | 13.5 / 11.2 | 3.63 / 1.32 | 13.2 / 14.5 |
| home midnight | 15.3 / 11.6 | 16.0 / 12.1 | 5.00 / 1.37 | 15.3 / 17.6 |
| nest at noon | 10.8 / 9.1 | 11.2 / 9.5 | 2.65 / 1.12 | 10.9 / 11.5 |
| Fork, wave 10 | 11.4 / 9.5 | 11.7 / 10.2 | 2.77 / 0.96 | 11.4 / 12.2 |

The average and the p95 GPU frame drop by about 20 % (16-24 %). One frame
per step costs more than today's frame. The engine draws the step's frame
uncached, at today's cost, and the next frame refills every page as it
goes back to caching, which costs 0.6-2.4 ms more than today's frame. The
meter's `gpuAroundStep` shows this as the mean GPU time at each offset
from a step; the GPU timer lags four frames. No engine setting skips that
refill. There are about 5 steps in 10 s at noon. At midnight there were
17 steps in 10 s while the ringlight had the sun's 20 m bound, and its
17.7 ms frame missed one 60 Hz vsync per step at 1920×1080 on this GPU.
With the ringlight's 5 m bound there are 4 steps in 10 s: the bump is still
17.6 ms, but about every 2.5 s. No frame from the game thread shows GPU
slack to time a step into: the sky light captures one face every frame and
Lumen updates every frame, so steps go when they fall due.

**The look.** Hidden clips at 1280×720 of the bench's home scene
(`-AcMmoShotsBench -AcMmoBenchHour=H -AcMmoShotsClip`, raw frames through
FFmpeg), stepped and continuous, at dawn (hour 6.7, sun about 13° up) and at
midnight, 15 s each. For each frame they count the changed pixels among
the clip's static ones (pixels that change in under 2 % of frames). A step
that shows moves edges in static places all at once. With a 5 m bound for
the sun, the Citadel's long dawn shadow jumped about a pixel at each step
(16 isolated rises against 4 in the continuous clip, the largest 6,140
pixels). That is why the sun has the 20 m bound. With it the stepped dawn
clip equals the continuous one: 5 isolated rises against 4, and a largest
spike of 2,682 against 2,627 pixels, made by a walking unit. The midnight
clip's spikes are all units and headlamps switching on, with no shadow
edges. compare.py cannot grade this, because its references come from the
Swift game, which left the repo.

**Not a lever.** `r.Shaders.BoundsChecking` and `ZeroInitialise`, which
the engine's Mac device profile turns on for the editor and for a cooked
game alike, change nothing in Metal SM6 code. The shader converter reads
the flags and never uses them (MetalCompileShaderMSC.cpp, UE 5.8), so they
only change the shader cache's key.

**Next.** Deferred lighting (3.5 ms at home noon) and the Lumen screen
probes (2.0 ms) are now the largest passes. A `ProfileGPU` tree per scene
in the MMO bench will show what is inside them.

### Round 3: parts sent only when they move; arrivals (2026-10-18)

**Inside the GPU frame.** With `--gpu` the MMO bench also logs one
`ProfileGPU` tree per scene. At home at noon after round 2 (11.6 ms):

- Lumen screen probes 2.1 ms (the radiance cache 0.8, the filter 1.2)
- TSR 1.1, base pass 0.9, Lumen scene lighting 0.6
- shadow depths 1.4
- the sun's projection 0.75 (distance-field shadows 0.37, the virtual
  shadow map 0.36)
- the global distance field's update of movable objects 0.28, at home
  only

Those are quality settings, so they stay.

**The change.** When an object moved, `UAcWorldRenderer::WriteObject` sent
every part of it again. For a building whose lamp or barrel moves (the
Citadel), that meant its whole hull every frame. The engine then
invalidated each re-sent instance's virtual shadow pages and redrew them.
Now a part is sent only when its transform differs bit for bit from what
its instance holds (`ac.RenderSendChangedOnly`, 1; 0 sends every part, as
before). An object coming back into view is still written whole. The
instances end up with the same transforms either way.

| MMO bench, 2 rounds in turns, load 7-10, every part / changed only | GPU p50 | shadow depths | game p50 |
|---|---:|---:|---:|
| home noon | 10.8 / 10.4 | 1.35 / 0.98 | 2.4 / 2.1 |
| home midnight | 11.6 / 11.2 | 1.31 / 0.90 | 2.4 / 2.1 |
| nest at noon | 9.1 / 9.1 | 1.11 / 1.04 | 2.0 / 2.0 |
| Fork, wave 10 | 9.4 / 9.6 | 0.84 / 1.01 | 2.2 / 2.2 |

The change saves about 0.4 ms of GPU (3.5 %) where a Citadel is in view
and nothing elsewhere; the GPU p95 is equal or lower in every run. The
global distance field's update stays at 0.17 ms at home, so something
there really moves.

**Arrivals.** The bench gains two scenes, `mmo-arrive-nest` and
`mmo-arrive-fork`. Each one times the frames from the server's teleport to
the start of the next scene's timing. An Insights trace
(`-trace=cpu,gpu,frame,bookmark`, exported with UnrealInsightsBG)
explains the Fork's 650-690 ms frame. On the region change the client
built Cold Fork's ground (357 ms) and baked its minimap (218 ms) on the
game thread, because that region had not been built ahead. Only the dev
teleport (`ferry go`) arrives like that. The player's paths come in
built ahead:

- **Dropship.** It makes its target ahead.
  `-AcMmoShotsF2 -AcMmoShotsF2CallAt=5`: worst frame from the call to
  the landing 17.5 ms, p99 13.0, 1,681 frames.
- **Walking.** It enters only neighbours, which are built ahead.
  `-AcMmoShotsW4`: 13.0 ms at most around the change, against a 9.9 ms
  median.
- **Respawn.** One path could still come in unbuilt: a respawn at a home
  two regions from the fall. In this 3×3 world the start pad is at the
  centre, so it can't happen today, but Citadels can stand in any
  region. `FAcMmoClient::PrepareAround` now treats the pending respawn's
  region (`ProgressMessage::respawn`) as a target, the way it treats the
  Dropship's, and builds it ahead (map, mirror, ground, minimap) while
  the respawn screen counts down. No shot set stages a fall that far
  from home, so that path is not measured yet.

The earlier "many 300-500 ms stalls" on arrival came from the measuring:
xctrace attached, and the `stat gpu` / ProfileGPU windows. Neither runs in
the timed frames.

**Next.** The game thread (2.0-2.4 ms; Slate about a fifth of it, the
console's warp into a render target, the HUD's text measuring) and the
global distance field's movers at home.

### Round 4: the long tail on the game thread (2026-10-18)

**Where it stands.** At 1920×1080 the MMO client is GPU-bound in every
scene. The game thread takes 1.9-2.3 ms of a 9-11 ms frame, so no game
thread change moves the frame. The GPU's own passes are quality settings
(round 3), so the wins above 2 % of a frame that leave the picture alone
are taken. These are the long tail, each with its number. A profile of
the home scene (xctrace, 15 s): Slate 20 % of the game thread (the
console's warp into its render target 7.7 %, the HUD's labels 5.7 %),
the frame stages 24 %, the engine's instance updates 7 %, and actor
iterators 3.1 %, entered from about fifteen places a frame.

**The changes.**

- `AcFindActor<T>` (AcFind.h). Each one-of-a-kind actor's `Find`
  (AAcTerrain, AAcFog, AAcWeather, the pilot pawn and 11 more) walked
  the level's actor list on every call. Now the actor found is kept while
  it is alive and in that world (`ac.FindKeep`, 1). Off the game thread,
  or with the cvar at 0, the walk is as before. AAcDaylight's `Find` is
  left as it was, because that file belongs to the clouds' owner.
- `FAcHudText::Paint` keeps a label's measures per text and font
  (`ac.HudKeepMeasures`, 1). A kerned label placed each letter at the
  measured width of the text before it, so a label of n letters measured
  n prefixes on every paint, twice with its shadow. The measures are
  filled as they are asked for, so a text that changes every frame costs
  what it did. The same numbers reach the same draw calls.

| MMO bench, 2 rounds in turns, load 6-15, game thread p50 ms | both off | both on |
|---|---:|---:|
| home noon | 2.22 / 2.14 | 2.02 / 2.09 |
| home midnight | 2.10 / 2.07 | 2.00 / 2.06 |
| nest at noon | 2.14 / 1.97 | 1.93 / 1.92 |
| Fork, wave 10 | 2.26 / 2.15 | 2.29 / 2.12 |

It is lower in 7 of 8 pairs, by 0.05-0.2 ms (2-9 %). The tick's p50
(the game's own frame stages) is 0.51-0.52 ms against 0.53-0.55.

**Next.** Measure where players' pixels are: a Retina window (5120×1378
and 2880×1800 at the auto screen percentage), where the GPU share grows;
and the cooked Development build next to the editor binary, for the
render thread's per-pass overhead (RDG names and checks) that a cooked
game drops.

### Round 5: where players' pixels are, and the cooked game (2026-10-18)

**Retina sizes.** The MMO bench at 2880×1800 and 5120×1378, both at a
backing scale of 2 (`--size WxH@2` now passes `-AcHudScale`). One round,
load 7-13, GPU p50 in ms:

| scene | 1920×1080 | 2880×1800 | 5120×1378 |
|---|---:|---:|---:|
| home noon | 10.1 | 12.2 | 14.1 |
| home midnight | 10.7 | 12.9 | 15.0 |
| nest at noon | 9.2 | 11.0 | 12.8 |
| Fork, wave 10 | 9.4 | 11.7 | 13.1 |

The passes that grow with the pixels, from 1920×1080 to 5120×1378 at home
at noon:

- deferred lighting 3.5 → 4.8 ms
- TSR 1.1 → 2.1
- the rest of post 0.3 → 0.8
- Lumen screen probes 2.0 → 2.7
- base pass 0.9 → 1.4

All of these are quality settings. At 5K the client still holds 66-78 fps
on the M4 Max.

**The cooked game.** `build_release.sh mac --config Development --ad-hoc`
(the cook was warm, 2.5 min). `bench.py --mmo --app APP` runs the same
scenes in the package through `hidden_run.py --app`. The table at the top
of this section is the result. The cooked game drops the editor's sky pass
and about 0.2-0.3 ms of game thread, and holds 2.5 GB less memory. Its RHI
thread is 0.3-0.5 ms busier (1.8-2.1 against 1.4-1.6 ms), with the frame
still the GPU's. The other numbers match.

**Also.** AcFindActor keeps one actor per world (PIE beside the editor's
world was a re-walk on every alternating call).

**Next.** Memory: 7.7 GB in the cooked client is a lot for a Mac with 8
or 16 GB. A `memreport -full` of the cooked game per scene will show
where it goes (the texture pool, the virtual shadow map pool, Lumen,
Nanite's streaming pool, our ISM buffers, the world's ground sets kept
ahead) before any change.

### Round 6: memory, two pools sized to the game (2026-10-18)

**Where it goes.** `bench.py --mmo -AcBenchMemReport` runs `memreport -full`
after each scene's timed frames. In a package it lands in the run's
`user/Saved/Profiling/MemReports`. It also logs `r.LumenScene.DumpStats`.
The cooked client at home at noon, 1920×1080, holds 7.76 GB:

- the CPU allocator: 0.86 GB
- Metal heaps: 5.18 GB, of them 1.26 GB unused. The heaps are 512 MB
  blocks (`METALHEAP_DEFAULT_BLOCKSIZE`, a constant), and
  `rhi.Metal.HeapBufferBytesToCompact` is read but never used in UE 5.8,
  so that slack cannot be tuned without changing the engine.
- the render target pool, 1.9 GB, listed by name
  (`r.DumpRenderTargetPoolMemory`):
  - the virtual shadow maps' physical page pool, 512 MB (4,096 pages × 2:
    static and dynamic since round 2 made the cache work) and its HZB, 85 MB
  - Lumen's surface cache at 4096² (albedo, normal, depth, emissive,
    direct, final, radiosity), about 600 MB
  - the radiance cache's atlases, about 170 MB
  - the global distance field, about 95 MB
  - TSR, about 80 MB
- the texture streaming pool: a 1,000 MB budget, 244 MB used

LLM (`-llm`) leaves the GPU's memory untagged on Metal, and the RHI's
per-resource dump is empty there. The render target pool's own list is
the one that names the owners.

**What the scenes use.** The virtual shadow maps' stats
(`r.Shadow.Virtual.Stats.Visible 1`, drawn into the bench's pictures):
459-535 pages requested at home, 636 at the nest and in the Fork at
5120×1378, of 4,096. Lumen's surface cache: 39 of 1,024 pages at home (3 %)
at both sizes, 20 at the nest, no resolution dropped.

**The change** (`DefaultEngine.ini` `[SystemSettings]`, where the project
already sets its quality; the game has no quality menu):

- `r.Shadow.Virtual.MaxPhysicalPages=2048`, the engine's "high" setting.
  The busiest scene's 636 pages are 31 % of it, so the pool's dynamic
  resolution never starts.
- `r.LumenScene.SurfaceCache.AtlasSize=2048` (256 pages): 6.5 times the 39
  the busiest scene uses. A card's resolution comes from its texel density
  and drops only when the atlas is full.

| MMO bench, editor, 2 rounds in turns, MB | 1920×1080, before / after | 5120×1378, before / after |
|---|---:|---:|
| home noon | 10,543 / 9,898 | 11,320 / 10,136 |
| home midnight | 10,846 / 9,958 | 11,137 / 10,251 |
| nest at noon | 10,743 / 9,879 | 11,404 / 10,393 |
| Fork, wave 10 | 10,794 / 10,080 | 11,605 / 10,356 |

| the cooked game, 2 rounds in turns against round 5's package, 1920×1080, load 8-9 | memory, before / after (MB) | GPU p50, before / after (ms) |
|---|---:|---:|
| home noon | 8,051 / 7,664 | 9.84 / 9.59 |
| home midnight | 7,948 / 7,824 | 10.36 / 10.12 |
| nest at noon | 7,977 / 7,725 | 9.00 / 8.81 |
| Fork, wave 10 | 7,922 / 7,547 | 9.23 / 9.08 |

The cooked game gives back less than the editor: 124-387 MB against
645-888. The pools' memory is freed, but the Metal heaps it sat in are
512 MB blocks that stay while anything else lives in them (the 1.26 GB
of slack above).

**When the pins become wrong.** The shadow pool's dynamic resolution
starts at 85 % of its pages
(`r.Shadow.Virtual.DynamicRes.MaxPagePoolLoadFactor`), so at about 1,740
requested pages; today's busiest scene asks for 636. The bench's
pictures with `-dpcvars=r.Shadow.Virtual.Stats.Visible=1` show
"Requested" and a "Global Resolution Bias" above 0 once it does. The
surface cache drops resolution once its 256 pages are full: "Dropped res
levels" above 0 in `r.LumenScene.DumpStats`. `bench.py --mmo` prints the
cache's pages and that warning from any `-AcBenchMemReport` run. Re-run
both checks when a scene grows (more buildings, more lights at night).

The GPU frame is 0.24-0.41 ms shorter too (less to clear and keep). The
pictures' difference across the change (the mean |RGB| % over the 3-D view
at a quarter scale) is the same as between two runs of the same build: at
home at noon 0.65 against 0.69, at midnight 0.70 against 0.73, at the nest
0.49 against 0.55, and at 5K 0.34-0.68 against 0.38-0.97. The Fork's wave
moves between runs, 3.6-5.3 either way.

**Next.** The radiance cache's atlases (~170 MB) and the Metal heaps'
slack, which only a smaller set of big allocations can shrink.

### Round 7: hitches in steady play, and the first minute (2026-10-18)

No code this round: what a player feels between the frame times.

**Steady play is smooth.** The cooked game, 60 s a scene (an Insights
trace on, `-trace=cpu,frame,bookmark`), 1920×1080:

- nest at noon: 0 of 6,437 frames over 33 ms, the worst 18 ms
- the Fork with wave 10 down: 1 of 6,127 (34 ms)
- home at midnight: 2 of 5,285 (34-35 ms)

**Startup.** The cooked game is seated 2.3-3.2 s after its process
starts (10 runs; K3 left it at 3.0-4.2 s). The editor binary takes
13-16 s.

**The first minute, not settled.** One traced cooked run had a storm 17-40
s into the home scene: 329 frames over 33 ms and 217 over 50, the worst
688 ms. In the trace:

- The game thread waits at `FFrameEndSync` on the render thread.
- The render thread waits on engine tasks: visibility 438 ms, an RHI fence
  444 ms.
- The engine's two foreground and two background workers spend those
  seconds in background tasks with no trace scopes (5.5 s of 12) and in
  oversubscription waits.
- The game's own long jobs run on their own pool (AcWork, K3), not there.

Across all of this round's 77 bench runs, 15-60 s after seating, most have
1-2 stalls: the bench's own moments (the stats windows, the memory report,
the teleport to the nest). Five have a storm of 7-33. Four of those five
ran while the Mac's load jumped (to 29-36: other workers' builds), and an
xctrace run of the same scene, without the trace, had none. A storm is
not shown to be the game's. If one turns up on a quiet machine, the next
step is a sampling profile of the cooked game's workers in that minute
(xctrace on the background workers).

**Where the client stands** on the M4 Max: the GPU sets the frame. What is
left on it is quality settings (deferred lighting, Lumen's screen probes,
TSR, post). The game thread is a fifth of the frame and its rest is long
tail (the console's warp 0.17 ms, the pilot's follow 0.08 ms, the rays 0.05
ms a frame). Memory's rest is the Metal heaps' slack, the engine's. The
next big win for players is on smaller Macs. A base M-series GPU has a
quarter to a fifth of this one's cores, so the same frame takes 40-55 ms
there: a quality level per Mac (device profiles by GPU), which the game
does not have.

## Server

The MMORPG server (`unreal/server`), its endpoint (`unreal/Source/AutocraftNet`)
and the core as the server steps it. Results stay equal: the world's
deeds, the ledgers, the save, the wire bytes and the simulation's hashes.
The benches are `make -C unreal/server bench` (-O3); the M4 Max is shared,
so every number carries the load average it ran under.

Where it stands after round 15 (the first number before round 1, or
before the round that first measured it):

| | before | now |
|---|---:|---:|
| the server's start (The First, cold) | ~850 ms, 5.5 s CPU | ~300 ms, 2.7 s CPU |
| the first step after a pilot joins | 12-25 ms | under 1 ms |
| the world's step, one pilot (`worldbench`) | 0.030 ms | 0.005 ms |
| the nest's step (`worldbench ... nest`) | 0.054 ms | 0.040 ms |
| a look by day, a pilot's region (`lookbench`) | 63 µs | 2.2 µs |
| building and writing a pilot's snapshot (`snapbench`, start, noon) | 3 + 44-70 µs | 1.8 + 6.0 µs |
| 16 pilots in one region, a tick (`crowdbench`) | 230 µs | ~100 µs |
| a path search on the nest's grid (`pathbench`) | 10.5 µs | 9.0 µs |
| the idle call, no pilot on line (`soak-save` days) | 0.073-0.120 ms | 0.007-0.009 ms |

Every change kept the results: the wire's bytes, the save, and the
simulation's state hashes in the benches are the same before and after.
Round 3 found and fixed three reads of freed or dead memory in the nest's
code (the only change of behaviour, where it read garbage). What is left in
every busy scene is the Brood's path searches (half the nest's step; their
order cannot change) and the look's clearing of its arrays.

### Round 1: the first board's walking grids (2026-10-18)

`worldbench 30` (The First, one pilot driving, 1,800 ticks) showed a max
step of 12.6 ms against a mean of 0.026 ms, always on tick 1. The new
"slowest steps" line in the bench named it, and timing the world's step
phases found `missions()`: a pilot's first mission board asks which sites
it can reach, and `region::siteReach` built a terrain field, a walking grid
and a game state for every region with sites, inside the 60 Hz tick. It
was cached afterwards, so it hit the first pilot after each server start
(and the first contract board too), a stall for every pilot in the world.

The world's load already works out the same reach while it checks each
region (`region::validate` walks the same grid with the same `Reach`). It
now hands it back (`validate(file, built, reach)`), the load keeps it on
`WorldRegion::siteReach`, and `WorldMap` starts with it. The lazy path
stays for a world put together without a load. The load does no more work
than before.

| `worldbench 30`, 4 runs each in turns | load | mean ms | p95 ms | max ms |
|---|---:|---:|---:|---:|
| before | 11-12 | 0.042-0.051 | 0.154-0.192 | 20.6-24.8 |
| after | 11-12 | 0.030-0.034 | 0.153-0.173 | 0.31-0.75 |

At a load of 8 the before binary's max was 12.3-12.7 ms. The mean drops
by the one tick it no longer spends. The core's `RegionFile` gained the
overload; the simulation is untouched (core-tests 521/521, the goldens
equal). Test: `sites_reach_from_the_load_is_site_reach` (the load's reach
equals `region::siteReach` in every region of The First).

### Round 2: the walking grids at the start (2026-10-18)

Two new benches: `savebench` (the soak's world with its pilot on line,
saved every 5 s of game time as the server saves) and `loadbench` (the
world's start: `loadWorld`, then `WorldMap`). The save costs 0.5 ms every
5 s (0.23 ms building the save, 0.28 ms writing about 20 rows), not worth a
round yet. The start was: Time Profiler on `savebench` put 79% of the
process's CPU in `NavGrid::buildGround`, the walking grid sampled from the
terrain (16 threads, kept per process in `NavGrid::cache`), and 45% of
everything in one function, `landOutside`: the signed distance to a
region's outline (130-175 edges), asked some 20 times a cell (the slope
test's heights each walk the region's walls) over 55,000 cells a region.

All in `TerrainField`, all bit for bit:

- `PolygonRuns`: the outline's and islands' edges in runs of 8, each with a
  box. The nearest edge: the nearest box first, then only the boxes not
  further than the nearest edge so far (the minimum does not hang on the
  order, and a margin covers the rounding); the inside: only the runs whose
  boxes span the point's row, found by 32 bands across the polygon. Each
  edge goes through the same two helpers as `signedDistance` itself.
- `openLand(p)`: inside the outline, out of every island and off every
  edge's band, `wall(p).coverage` is exactly 0: known from the sign alone.
  `walled`, `materials` and the grid's wall test ask it first.
- `wallNear(p)`: `walled`, `materials` and the grid read the distance only
  through smoothsteps that stand at 0 on the land and at 1 past 11.7 cells
  (the deepest terrace's shelf plus a cliff's width), so they take it
  capped at `wallReach()` (12.7): exact below, the runs past it not
  measured. `wall(p)` stays exact for everyone else.
- Noise not sampled where its weight is exactly +0: a zero product leaves
  any sum but -0 as it is (`plusNoise`): the crag on flat ground, a height
  edit's crag inside it (coverage 1), the wall's faces at coverage 0 or 1,
  its ridge and roughness where `away` is 0; and the plateau wobble's two
  octaves only once a plateau is within reach.

Checked: the ground and cliff layers of all 10 region files, and `height`,
`wall`, `materials`, `borderHeight` and `landOutside` on a quarter-cell grid
over each region and 4 cells past it, hashed by their bits: the same
before and after. Test `RegionFile_outlineRunsMeasureAsThePolygon` holds
the runs, `landOutside`, `wallNear` and `openLand` to the plain answers on a
half-cell grid in every region file. core-tests (the goldens), the server
and net suites pass.

| 10 region files' grids, 3 runs each in turns | load | wall ms | CPU ms (all threads) |
|---|---:|---:|---:|
| before | 7-10 | 501-515 | 5,435-5,592 |
| after | 7-10 | 264-270 | 2,695-2,718 |

| `loadbench 3`, 4 runs each in turns, load 8.4-8.5 | first `loadWorld` | its CPU | first `WorldMap` | warm `loadWorld` |
|---|---:|---:|---:|---:|
| before | 564-603 ms | 5.2-5.5 s | 261-263 ms | 68.5-69.3 ms |
| after | 311-323 ms | 2.59-2.63 s | 130-132 ms | 58.6-59.3 ms |

The server's start (The First, the grids cold) goes from about 850 ms to
450 ms, and its CPU from 5.5 s to 2.7 s; the client's local server and its
terrain sampling read the same code. Tried and left out: `Noise::hash`'s
division by 2^32-1 built from bits (h/(2^32-1) is the binary fraction
0.hhhh..., so its correctly rounded double is the pattern's top 53 bits,
rounded once, never a tie): equal on all 2^32 inputs, and slower than the
M4's divider (2.57 against 1.58 ns for four). What is left of the start:
`Noise::fbm` (29% of it), the runs' distance and inside (23%), and
`WorldMap`'s single-threaded 130 ms (each region's `Vision` samples the
terrain).

### Round 3: the fog's look, and three dangling references (2026-10-18)

A world region looks every 0.1 s (`Simulation::look`): what each side sees.
By day (S3) a pilot's daylight is one circle over the whole region, about
55,000 cells, and Time Profiler on `savebench` put `Vision::stamp` at half
of the world's step. The new `lookbench` times the look alone in three
regions of The First, at noon and at midnight.

In the core's `Vision`, all bit for bit:

- `stamp` fills each row's span of the circle at once. A row's cells in
  the circle are one unbroken run (a cell's `dx` grows with `x`, rounding
  keeps the order, and the test grows with `|dx|`); its ends come from the
  half chord, set right by the same test, so the cells between need no
  test. A cone (a headlamp) keeps the cell-by-cell loop.
- The explored bits of a span are gathered in a register, a word at a
  time, and written once (they were a read-modify-write of one word, cell
  after cell, chained through memory), with restrict pointers.
- In the air or the daylight (every level): `near` and `seen` by `memset`,
  and the "not seen yet" bits 8 cells at a time: `seen` holds 0 or 1, so
  `(~w & 0x0101010101010101) * 0x0102040810204080 >> 56` packs the eight
  flipped low bits into a byte (byte k to bit k; checked on all 256
  patterns).
- `look` fills the last look's `Sight` arrays again (`Vision::sightInto`)
  instead of allocating two region-sized arrays a side each time, and
  moves the sides' `Intel` in and out instead of copying it.

Tests: `vision_stampSpansAsEveryCell` (2,400 circles of every size, in and
off the map, on every level and in the air, stamped over each other: the
same bytes as the cell-by-cell loop), `vision_sightIntoAUsedSightIsAFreshOne`;
the vision goldens and every core test pass.

| `lookbench 2000`, 2 runs each in turns, load 4-6 | before µs | after µs |
|---|---:|---:|
| start, noon (one pilot) | 62.3-65.0 | 9.4-9.5 |
| start, midnight | 7.7 | 4.9 |
| nest, noon (29 bugs, the Matriarch) | 124.4-125.2 | 25.9-26.0 |
| nest, midnight | 18.9-19.3 | 15.1-15.2 |
| Cold Fork, noon | 62.6-64.3 | 9.4 |

| `daybench 20`, the whole server tick by day, 3 runs each in turns, load 6 | before ms | after ms |
|---|---:|---:|
| nest, noon, daylight | 0.07 (p99 0.18-0.21) | 0.05 (p99 0.15-0.16) |
| canyon wave, noon, daylight | 0.06 (p99 0.21-0.22) | 0.04 (p99 0.09) |

**The dangling references.** Proving the look equal, the nest scene came out
with 29 bugs or 33 from run to run of one binary, and only of the new one.
An AddressSanitizer build found the cause in the base code: `nestThink`
held the Matriarch as a pointer into the region's units, then the horde's
spawns and a hatchling added units and the vector moved, so her site and
tether went to freed memory and her foe was read from a stale copy;
`roar` read her position after the hatchlings it adds; and the pilots near
her were pointers to loop-local copies (`GameState::unit` returns one).
What the step did hung on whether the freed block had been used again, so
any change in allocation could flip it. Fixed in their own commit: only
the steps that read freed or dead memory change. The same sweep found
reads of dead memory in three tests (a null unit, a temporary's array, a
temporary simulation's grid and a temporary copy's guns), fixed in the
tests.

`make -C unreal/server test-asan` (not the default) builds the server's
tests with AddressSanitizer and UBSan in `build/asan/` and stops at the
first finding: a few minutes to build, half a minute to run. The server,
core and net suites all pass under both now.

### Round 4: the fog on the wire (2026-10-18)

Time Profiler on `daybench`'s server tick by day put `SnapshotWriter::write`
at 31% of it, most in its own body. The new `snapbench` times a pilot's
snapshot apart from its writing, 20 a second with every part acknowledged,
and hashes every byte written. The writing was 40-55 µs a snapshot even at
midnight with nothing near: with the fog left out it was 3.5-15 µs. Timed
inside, nearly all of the fog was building its chunks (37 µs a snapshot:
the 27 chunks of 2,048 cells a region, 0.67 ns a cell), not comparing them
with their baselines (2 µs) nor writing the ones that changed (0.1 µs).

`fogChunk` (AutocraftNet's `NetSnapshot.cpp`) built a cell's byte with two
branches and a division each. Where `near` and `explored` cover the chunk
(the server's own fog, always) it now takes eight cells to a word: a byte's
bits folded into its lowest bit say "not 0" for `seen` and `near`, and a
256-entry table spreads the explored word's byte into bit 2 of each cell.
Short arrays and a ragged tail keep the cell-by-cell loop.

| `snapbench 20`, 3 runs each in turns, load 31-36 | before µs | after µs |
|---|---:|---:|
| start, noon, writing | 48.0-69.9 | 17.1-20.5 |
| start, midnight | 43.5-53.0 | 13.4-14.0 |
| nest, noon | 53.4-56.0 | 24.6-25.2 |
| nest, midnight | 39.4-49.4 | 13.9-27.7 |

The wire is the same: every scene's hash of all the bytes written is
equal before and after (`fd5e5104ff08f662`, `467fab1c80dd1394`,
`35325d9463465925`, `d4d978dd55fbf064`). Test
`protocol_fog_chunks_of_any_bytes_and_sizes_round_trip` (net-tests): fogs of
1 to 4,613 cells with any byte values, short `near` and `explored` arrays,
whole and as deltas, come back as the fog quantised. A pilot's snapshots
cost the server 0.4 ms a second instead of 0.9.

### Round 5: a crowd's fog, kept (2026-10-18)

The new `crowdbench` puts 1, 4 and 16 pilots in the start region at noon,
each driving, and builds and writes every pilot's snapshot 20 times a
second: with 16 the writing was the cost (29 µs a pilot a snapshot, the
world's step 46 µs). The fog is still most of a write, and it changes only
when the region looks (every 0.1 s, every other snapshot) and where it
looks; each writer built all 27 chunks every time and threw away the ones
equal to their baselines.

`SnapshotWriter` keeps each fog chunk it built with what it was built
from (the three arrays' sizes and their stretches over the chunk): the
same inputs give the same bits, so a chunk is built again only when its
cells changed, and a baseline that is the very chunk kept needs no
comparing. Test `protocol_fog_chunks_of_any_bytes_and_sizes_round_trip`
adds a fog of another size through the same writer.

| 2 runs each in turns, load 3 | before µs | after µs |
|---|---:|---:|
| `snapbench`: start, noon, writing | 18.6-20.2 | 11.7-11.8 |
| start, midnight | 14.0-15.3 | 7.8-8.0 |
| nest, noon | 27.3-28.3 | 19.5-20.3 |
| nest, midnight | 15.1-15.9 | 8.0-8.1 |
| `crowdbench`: 1 pilot, the whole tick | 15.7 | 11.2-13.1 |
| 16 pilots, a pilot's writing | 29.1-29.3 | 21.5-22.0 |
| 16 pilots, the whole tick | 230-231 | 188-193 |

The wire is the same (the four scenes' hashes as in round 4).

Measured, not changed:

- `pathbench` (new): 20,000 searches on the nest's grid, 5 to 40 cells,
  10.6 µs and 228 cells closed a search, every path hashed. The nest's
  bugs make about 100 searches a second (541 cells each), half its step.
  Keeping each cell's heuristic for the search (it was worked out again at
  every push) gave the same paths and no time back: the cost is the
  neighbour visits and the 4-ary heap, already unrolled and kept between
  searches.
- `travel` copied a walking unit's whole route every step; editing it in
  place kept `worldbench`'s state hash and moved nothing measurable in the
  nest (0.054 against 0.055 ms), so it stays as it was.
- The save: `savebench 300` (it now prints its slowest writes with the
  WAL's size) has the WAL growing about 55 KB a save for some 22 rows, and
  SQLite's checkpoint at 4 MB (every 70 saves, 6 minutes with pilots on
  line) makes that save 3-6 ms instead of 0.23. Inside one 16.7 ms tick of
  about 0.05 ms that is no hitch; left for now.

`worldbench` takes a region (`worldbench 30 worlds/first.json nest`) and
prints a hash of every unit's position, heading and hp at the end, the
check that a core change leaves the world's simulation bit for bit.

### Round 6: the writer's items, no allocation for the unchanged (2026-10-18)

Counted in `crowdbench`, a snapshot has about 60 items (the fog's chunks,
the sites, deposits and wells, the players, the units near), and about 42
of them come out the same as their baselines. Each one was written to a new
`shared_ptr<ItemBits>` (three allocations) only to be found equal and
thrown away, and the bit writer went a byte at a time with a `push_back`
for each.

- `canonInto` writes an item's bits into a scratch the writer keeps; the
  scratch is compared with the baseline, and the shared copy is made only
  for an item that is written.
- `BitWriter::writeBits` ORs a value's bytes in one pass after one
  resize (the bytes past the end are always zero: only these writes add
  to them); `BitReader::readBits` gathers them the same way; `appendBits`
  copies 56 bits at a time, reading only the bytes they lie in. Test
  `bitStream_wordWritesAreTheBitsOneByOne`: values of every width at every
  bit offset, and stretches appended anywhere, the same bytes as a writer
  of one bit at a time, read back the same.

| 2 runs each in turns, load 7 | before µs | after µs |
|---|---:|---:|
| `snapbench`: start, noon, writing | 9.9 | 7.2-7.8 |
| start, midnight | 6.7-6.8 | 5.6-5.7 |
| nest, noon | 17.0-17.2 | 12.4-12.7 |
| nest, midnight | 6.8-6.9 | 6.1 |
| `crowdbench`: 16 pilots, a pilot's writing | 18.2-18.7 | 14.8 |
| 16 pilots, the whole tick | 160-166 | 144 |

The wire is the same (the four scenes' hashes as before). Since round 3 a
pilot's snapshot writing went from 44-70 µs to 6-13 µs, and 16 pilots in
one region from 230 µs a tick to 144.

### Round 7: a crowd's daylight, stamped once (2026-10-18)

Time Profiler on `crowdbench`'s step with 16 pilots: the look was 43% of
it, `Vision::stamp` alone 31%. By day every driver gets a daylight circle
over the whole region (W5), and the test that skips a circle inside
another one drawn never fires for circles round different units, so all
16 were stamped over every cell, on top of the units' own circles.

`Vision::sightInto` stops stamping once a circle on every level has taken
in every cell: the grid is then all seen, near and explored, and nothing
left can change it. `Vision::coversGrid` says whether a circle does,
exactly: `stamp`'s test (the same sums) passing at the four corner cells,
since it grows with `|dx|` and `|dz|` and those are largest at the
corners. Test `vision_aCircleOverTheWholeGridLeavesNothingToStamp`:
`coversGrid` agrees with what `stamp` marks for circles a hair inside and
outside the farthest corner, from centres in and off the map, and after a
covering circle no stamp, on any level or in a cone, changes a byte.
`crowdbench` now hashes every byte written and every unit at the end.

| `crowdbench 10`, in turns, load 7.5 | before µs | after µs |
|---|---:|---:|
| 4 pilots, the step | 13.2 (p99 54.5) | 9.8 (p99 34.3) |
| 16 pilots, the step | 41.5 (p99 173) | 22.8 (p99 53) |
| 16 pilots, the whole tick | 152.8 | 135.8 |

The wire and state hashes are equal for 1, 4 and 16 pilots, and
`worldbench`'s state hash (the start and the nest) too.

Left: with several drivers, `boost` (read for each unit's speed every step
and its sight every look) swaps the other driver's whole context in and
back for each of its units (`swapDriver`: the `Pilot` copied three times,
`PilotProgress` and the side's record moved three times), about 2.7 µs a
tick with 16 drivers. Reading the driver's fields in place would need
`boost`, `addSkills`, `skilled` and `burst` to take the context; not done.

### Round 8: the empty regions (2026-10-18)

Every region steps every tick (P5), pilot or none. Stepped alone, an empty
region of The First cost 0.42-0.45 µs a step; Time Profiler put a third of
it in `NavGrid::setDynamic`, which built its key (the buildings, the
deposits with ore, the doodads and wells put down) into a new vector every
step only to find it equal to the last. The key is now read against the
last one as it goes and written out only when it changed: the same answer,
no allocation. (Half of the rest is the look zeroing its arrays; other
code may write into them between looks, so they stay zeroed.)

| in turns, load 11 | before | after |
|---|---:|---:|
| an empty region's step (8 regions, 20,000 steps) | 0.415-0.445 µs | 0.261-0.271 µs |
| `worldbench 30` (one pilot, all 9 regions): mean | 0.007 ms | 0.005-0.006 ms |
| p50 | 0.003 ms | 0.001 ms |

The state hashes of `worldbench` (start, nest) and the wire and state
hashes of `crowdbench` (1, 4, 16 pilots) are equal. After round 1 this
world's step was 0.030-0.034 ms: the rounds since took it to 0.005.

### Round 9: the start's fog tiers on every core (2026-10-18)

After round 2 the world's start was `loadWorld` (about 300 ms, the walking
grids already spread over the cores) and `WorldMap` (126-133 ms on one
core): each region's `Vision` samples the terrain's height at every cell's
centre for its tiers (a cell's level), 55,000 cells a region. Each cell is
its own, as in `NavGrid::buildGround`, which already gives each thread its
own rows; `Vision` now does the same.

| `loadbench 3`, 3 runs each in turns, load 10 | before | after |
|---|---:|---:|
| first `WorldMap`, wall | 125.2-133.4 ms | 20.9-21.7 ms |
| its CPU (all threads) | 125-133 ms | 156-157 ms |
| first `loadWorld`, wall | 271-279 ms | 281-326 ms (the same work; load) |

The tiers are the same (the vision goldens, every core test; `worldbench`
and `crowdbench` hashes equal). The server's start is now about 300 ms of
which 280 are the walking grids' ground.

Tried and left out: two exact changes to the path search, the goal test's
root replaced by the largest square whose root is still within reach, and
the search's per-cell state cut from 12 to 8 bytes (where a cell came
from kept apart). The same paths (`pathbench`'s hash), and no time back
beyond the noise (10.5-12.9 µs against 11.0-13.0 at load 26).

The Makefile rebuilt `build/bench/SoakSave.o` on no header change (its
dependency file was not included), so since round 1 changed `WorldRegion`
a stale one made `soak-save` crash; it is included now.

The short soak after this round (`soak.py run --minutes 6 --every 60`, a
fresh save in `unreal/server/build/soak-r9`, load about 10): resident
memory flat at 21.3 MB, CPU 2.8 s at the start and 0.03 s a minute after
it, the WAL 62 KB a minute, the idle call 0.18-0.35 ms mean and 0.74-3.7 ms
at most (Q3's soak: 0.06-0.55 and up to 4.5).

### Round 10: the idle call's build spots (2026-10-18)

With no pilot on line the world runs on in 0.5 s calls (`WorldMap::idle`).
`soak-save SAVE 3` idles the soak's world three world days so: the call
took 0.073-0.120 ms, and Time Profiler put 98% of it in the frontier
pilots' thinking, `frontierSpot` trying up to 112 spots round an anchor
(the same spots every think) and `Simulation::placementRefusal` reading
the terrain's height at up to a hundred points under each footprint.

The terrain never changes in a simulation, so the heights' spread under a
footprint is a function of the spot and the size alone: `placementRefusal`
reads the walking grid's cells first (in the same order, so the same
first refusal) and keeps each spot's spread by the bits of the spot and
the radius (`Simulation::heightSpreads`, emptied past 65,536 spots).

| `soak-save SAVE 3`, the same world | before | after |
|---|---:|---:|
| idle call mean, world day 1 | 0.078 ms | 0.008 ms |
| day 2 | 0.120 ms | 0.009 ms |
| day 3 | 0.073 ms | 0.007 ms |

The saves both write after the three days are the same, table by table
(`sqlite3 .dump`, all but the wall clock's `saved_at`); `worldbench` and
`crowdbench` hashes equal. Test `frontier_placement_spread_kept_is_the_footprint`:
in every region of The First, at 1,350 spots, the answer kept, the answer
of a copy with nothing kept and the footprint read cell by cell as before
agree.

### Round 11: a driver's bank read in place; the WAL (2026-10-18)

With several humans driving in one region (W5), the other drivers'
contexts are swapped in for each of their reads (`Simulation::asDriver`,
`swapDriver`: the `Pilot` copied, `PilotProgress` and the side's record
moved three times each). Time Profiler on `crowdbench` with 16 pilots:
`swapDriver` 10% of the step, its largest caller `Region::bank`, which the
world's step asks for every pilot every tick to read two numbers. A
driver's bank is the side's ore and MH only while it is swapped in, and
`Driver::ore` and `hydrogen` otherwise: `Simulation::bankFor` and
`setBankFor` read and set them in place, under the same condition the swap
trades them on. `Region::bank`, `setBank` and `keepBank` use them.

| `crowdbench 30`, 16 pilots, 4 runs each in turns, load 19-20 | before µs | after µs |
|---|---:|---:|
| the step | 19.9-21.0 | 17.9-19.4 |
| the whole tick | 126-134 | 125-135 |

The wire and state hashes are equal (1, 4, 16 pilots). The other swaps are
real context switches (each driver's leveling, skills and piloted step use
its whole context); making them cheaper needs a member-wise swap of
`PilotProgress` that would go wrong the day a field is added: left.

The save's WAL: about 55 KB a save in `savebench` for some 22 changed rows
is SQLite's page granularity. The rows are small (the frontier's state
4 KB, the pilot's row about 6 KB, 79 deposits, a few units), and a save
touches some 14 pages of 4 KB across the tables and their B-trees. Writing
fewer would mean another layout of the file: left.

### Round 12: the search's heap picks its child by pairs (2026-10-18)

The Brood's path searches are half of every busy MMO step (the nest, a
raid, a canyon wave). Their order cannot change: among equal costs the
cell reached first wins (`came`), so another queue would give other paths
of the same length. The 4-ary heap (`Heap4`) stays, with the same picks:
`popCell` chose the first least of a node's four children with a chain of
three dependent compares, mispredicted half the time; with all four there
it now takes the first least of each pair, then of the two winners, the
earlier on a tie: the same child, every time.

| 3-4 runs each in turns, load 8 | before | after |
|---|---:|---:|
| `pathbench` (20,000 searches on the nest's grid), a search | 10.35-10.50 µs (p99 181-183) | 8.96-9.05 µs (p99 154-155) |
| `worldbench 60 worlds/first.json nest`, the step | 0.046-0.047 ms (p95 0.134-0.138) | 0.040-0.041 ms (p95 0.116-0.118) |

The same paths (`pathbench`'s hash), and every state and wire hash of
`worldbench`, `crowdbench` and `snapbench` the same.

Measured, not changed: the snapshot's build per pilot (3-5 µs) is 70% the
copy of the region's fog into it (two 55 KB arrays and a large allocation
a snapshot); sharing them would change `net::Snapshot`'s type, which the
client reads too.

### Round 13: a side's fog shared between its snapshots (2026-10-18)

The manager's go-ahead, with its conditions: a snapshot never changes once
made, the wire stays the same, and the client changes only as far as the
type forces it (it forced nothing: the client reads the fog through
`NetMirror`, whose `s.fog->seen` reads the same).

`net::Snapshot::fog` is a `shared_ptr<const FogView>`. A region makes its
side's fog once a look (`Simulation::lookId`, a number no other look in the
process had; only a look changes the sights and what the side knows) as a
fresh copy, and every snapshot of that side's pilots until the next look
shares it: a snapshot held across a look keeps the fog it had. The writer
holds the fog it last built its chunks from; given that very fog again
(every other snapshot, a look being every 0.1 s and a snapshot every
0.05 s), it takes the chunks as built without comparing them. Test
`snapshot_fog_held_across_a_look_keeps_its_fog`: two snapshots between
looks share one fog; one held across the next look reads its fog byte for
byte; the next carries the fog the look left.

| 3 runs each in turns, load 19 | before µs | after µs |
|---|---:|---:|
| `crowdbench`: 1 pilot, snapshot / writing / tick | 3.2 / 7.8-8.0 / 8.5-8.8 | 1.7-1.9 / 6.4-6.6 / 7.4-7.7 |
| 4 pilots | 3.6 / 9.1-9.2 / 24.3-24.5 | 1.2 / 7.6-7.7 / 18.9-19.4 |
| 16 pilots | 4.7-4.8 / 15.0-15.1 / 122.6-124.2 | 2.1 / 13.6-13.7 / 101.5-102.5 |
| `snapbench`: start, noon, snapshot / writing | 3.0-3.2 / 7.1-7.5 | 1.7-1.8 / 6.0 |
| nest, noon | 6.4-6.6 / 12.1-12.5 | 5.4-5.6 / 11.4-11.6 |

The wire is the same (`snapbench`'s four hashes, `crowdbench`'s three), and
the simulation too (`worldbench`'s and `crowdbench`'s state hashes).

Why the look's number is enough (checked by grep over the core, the net
code, the server and the game): on the server only `Simulation::look()`
writes a simulation's `sights` and its sides' intel (`explored`, `sites`).
`Vision::sightInto` and `stamp` run only inside it (or on a fresh `Sight`
that `Vision::sight` returns); a `GameState` with intel enters a server
region's simulation only through its constructor, which looks (and
`newGame` builds a new one); nothing in `unreal/server` calls `lookNow`.
A shot or a hit, a Dropship landing, a crossing and a pilot's blueprint
sites change units and progress that the next look reads, not the sights.
The client's `NetMirror` writes the sights and intel of its own mirror
simulation from snapshots, never a server region's.

### Round 14: the headlamp's wedge without the root behind it (2026-10-18)

At night a driven Prospector's headlamp adds a wedge 22.5° either side of
its heading (`Vision::stamp` with a cone). Each cell in its circle took a
square root for the test `along < edge * sqrt(d²)`, `along` being the cell's
distance ahead of the lamp. With a wedge narrower than a half turn `edge`
is not negative, so wherever `along` is negative (behind the lamp) the test
already holds: those cells skip the root, the same answer.

| `lookbench 4000`, midnight, 3 runs each in turns, load 21-22 | before µs | after µs |
|---|---:|---:|
| the start region (one pilot) | 5.1-5.2 | 4.4-4.6 |
| the nest | 16.0-16.5 | 15.0-15.2 |
| Cold Fork | 5.1-5.3 | 4.3-4.4 |

`vision_stampSpansAsEveryCell` now stamps wedges too, narrow and wider
than a half turn (where `edge` is negative and the shortcut does not
apply), against the cell-by-cell loop: the same bytes. Every hash the same.

### Round 15: daylight over the whole region written at once (2026-10-18)

By day a pilot's daylight takes in its whole region, and it is the widest
viewer of its side: the look cleared both arrays, stamped every cell with
1, and gathered 55,000 "not seen yet" bits, every one set since the arrays
had just been cleared. When the widest viewer is on every level and covers
the grid (`coversGrid`, round 7), `Vision::sightInto` now writes that result
at once: both arrays all 1, every explored bit of the grid's cells set (the
bits past the last cell as they were), and nothing more is stamped (round
7). Test `vision_daylightOverTheWholeGridIsWrittenAtOnce`: into a used
Sight, over explored bits already set and stray ones past the last cell,
the same as stamping that circle on cleared arrays, and the same ids seen.

| 3 runs each in turns, load 10-11 | before µs | after µs |
|---|---:|---:|
| `lookbench`, noon: the start region | 9.7 | 2.2-2.3 |
| the nest | 26.6-26.8 | 18.5-19.2 |
| Cold Fork | 9.4-9.5 | 2.0-2.1 |
| `crowdbench 20`, the step: 1 pilot | 4.9-5.0 | 3.8 |
| 4 pilots | 7.5-7.8 | 6.1-6.2 |
| 16 pilots | 18.1-18.4 | 16.7-18.0 |

Every hash the same. A pilot's region looks by day in 2.2 µs: 63 µs before
round 3.

### Round 16: the save's statements prepared once (2026-10-18)

`WorldStore::write` prepared an SQLite statement for every row it wrote
(some 20 a save, every 5 s with a pilot on line) and finalized it after.
The store now keeps each statement prepared by its text
(`WorldStore::cached`, finalized before the file closes); a `Stmt` that
borrows one resets it and clears its values first, and resets it again
when done. The same SQL with the same values: the same rows.

| `savebench 100`, 3 runs each in turns, load 15 | before ms | after ms |
|---|---:|---:|
| the store's write, p50 | 0.147-0.186 | 0.109-0.111 |
| its mean | 0.197-0.227 | 0.142-0.178 |
| the whole save, mean | 0.378-0.421 | 0.324-0.362 |

The files both write are the same (`sqlite3 .dump`, byte for byte:
savebench's clock is its own, so `saved_at` too). A small win: 0.06 ms
every 5 s.

### Round 17: the noise's four octaves side by side (2026-10-18)

`Noise::fbm` (a third of the start, and every height the client samples)
took its octaves in a loop with a count known only at run time, so each
octave's value waited on the last. For the four every caller asks, it now
works the four values out side by side and adds them in the same order,
with the same factors (2.03 multiplied up as the loop does): the same
noise to the bit.

| `loadbench 2`, 3 runs each in turns, load 9 | before | after |
|---|---:|---:|
| first `loadWorld`, CPU | 3.16-3.39 s | 2.82-3.03 s |
| its wall | 364-398 ms | 344-353 ms |
| first `WorldMap`, CPU | 177-179 ms | 154-170 ms |

The terrain hashes of all 10 region files (heights, walls, materials,
border heights, the walking grids) are the same, the terrain goldens pass,
and every bench hash is equal.

### Round 18: the regions with a pilot on threads, stage 1, off by default (2026-10-18)

`WorldMap::step` steps every region, one after another, then does the
world's own work. `WorldParams::stepThreads` (0, the default: as before;
-1: half the cores, at most 8; more: that many; with 0 the
`AUTOCRAFT_STEP_THREADS` environment variable sets it) makes a pool of
threads at the start (`StepPool`, the caller among them, no more threads
than regions) and steps regions with a pilot on it; the regions without
one, and the world's work after the steps, stay on the caller in today's
order. A dispatch wakes as many workers as there are jobs past the
caller's; the caller steps jobs too (the one that took longest first) and
returns only when every worker that took a place is through. Between
dispatches the workers sleep on a condition: with no pilot on line nothing
is dispatched or woken, and with the default nothing is timed either.

The gate. The pool times one dispatch when it is made: one worker woken
after 2 ms asleep and heard back from, on two empty jobs, the mean of 16
(the slow wakes count; 40 ms at the start): `wakeUs`, 8-12 µs on this Mac at load 11. A
region goes to the pool only when its last two steps each took longer than
that, and only when two or more do; otherwise every region steps on the
caller, as off. Two steps, not one: a region's step is bursty (the start
region 5-7 µs most ticks and 55-65 µs every sixth, the nest 5-130 µs), and
the last step alone sent the light tick after each burst to the pool.
`stepAlways` (`AUTOCRAFT_STEP_ALWAYS=1`) sends every region with a pilot,
for the tests.

The break-even, measured with every region with a pilot sent (the start
and the nest, 1,200 ticks, load 9): the region steps take 29.4 µs on one
thread (5.2 + 24.2), and 40.9 µs on two. A dispatch costs its wake, the
woken core runs its region 1.4 times slower (5.2 -> 7.4 µs: its caches
and clock cold after 16 ms asleep), and the caller's own region runs 15%
slower beside it (24.2 -> 27.9 µs). So moving a region pays only when the
smaller of the two takes well over the wake, every tick: two regions each
steadily above 10-15 µs. The First has none such today (one heavy region,
the nest, and bursts), so the gate keeps nearly every tick on the caller.

Audit, of everything a region's step reaches that is not its own: the
walking grid's ground cache and search scratch pool, and the fog tiers'
cache, are behind mutexes (filled when a region is made, not in a step);
the search counters and `Simulation::lookId` are atomics (a look's number
is compared only within its own region); every other static in the core is
a constant set up once (thread-safe in C++); the world's regions run no
AI. A region's step reads and writes its own simulation and seats alone.

Checked: `spreadbench` (new: pilots in 1, 2 and 4 regions, the nest and
Cold Fork's canyon wave among them, and 8 on each of two) gives the same
four state hashes off as on 2, 4 and 8 threads with every step sent, 21
runs, none different; the server suite passes, and passes under
ThreadSanitizer on 4 threads with every step sent (`make -C unreal/server
test-tsan`, new: 360 tests, no race); spreadbench under ThreadSanitizer
the same. Every other bench hash is unchanged (the default is off).

| `spreadbench 20`, 3 runs each in turns, load 11-12 | off | on (`-1`, gated) | every step sent |
|---|---:|---:|---:|
| 1 region with a pilot | 3.6-3.9 µs | 4.0 µs (0 sent) | 3.8-3.9 µs |
| 2 (the start and the nest) | 41.8-42.2 µs | 41.5-41.9 µs (0 of 1,380 sent) | 47.1-47.4 µs |
| 4 (with the canyon wave) | 58.0-59.0 µs | 59.5-62.6 µs (4-122 of 1,440) | 59.6-60.1 µs (p95 134-138 against 165-169) |
| 2 with 8 pilots each | 35.8-37.3 µs | 36.3-37.4 µs (2-10 of 1,380) | 39.7-40.0 µs |

So with the gate "on" is "off" within the load's noise, and sending every
step costs 3-6 µs a tick where the regions are light. It pays when the
world has several crowded regions at once.

Decided (2026-10-18, with the manager): the default stays off until a world
has several crowded regions at once (after round 19 the nest's step is a
third of what it was, so on The First the gate keeps nearly every tick on
one thread). The switch, the day it is needed: `WorldParams::stepThreads =
-1` (half the cores, at most 8), or `AUTOCRAFT_STEP_THREADS=-1` in the
server's environment; then `make -C unreal/server test-tsan` and
`spreadbench` (off, on, and `AUTOCRAFT_STEP_ALWAYS=1`) before it ships
(docs/mmorpg.md, "The world runs on").

### Round 19: the walking grid keeps its searches (2026-10-18)

With round 18's spread of pilots, the world's own work after the region
steps is 3.5% of a tick; the region steps are the rest, and half of them
is the bugs' path searches (`NavGrid::pathSearch` and its heap, 46%). In
`spreadbench 20` those are about 5,400 searches, most of 12 cells or
fewer, but 2,395 close 500 to 1,640 cells each, and those 2,395 ask only
52 different questions: the same start cell, target, stop distance and
grid, asked again every few steps (bugs across a wall from their target,
and units stepped back off their line, ask the same way again).

The grid now keeps what each long search found (`NavGrid::searchMemo`):
its cells, start to end, or that there is no way, by the question to the
bit (start cell, target, stop distance, jumps, scan order, `fastSearch`),
for searches that closed at least 64 cells. Asked again, the search is
skipped and the straightening runs on the kept cells from where the unit
stands now: the same path to the bit.

Why the kept cells are the search's. The A* (`pathSearch` up to the
straightening) reads its question and, of the grid, only `passable` (one
byte a cell), `width`, `height`, `origin` and `cell` (set once, when the
grid is made, before the first memo). It reads no unit (units are not on
the grid: `dynamic`, and so `passable`, holds buildings, live deposits,
wells and placed doodads only), no reservation, no random number and no
cost but the fixed step lengths; its ties fall by the heap's order of the
same pushes; its working arrays are reset per search (`Scratch::begin`:
a new stamp, both heaps cleared); its counters are only written. The scan
order is in the question (`east(from)`), and so is `fastSearch` (the
heuristic and the heap). `passable` is written only by `indexPassable`,
which `setDynamic` runs whenever its key or anything it builds from
changes, and which makes a new, empty memo; a copy of the grid shares the
memo only while its `passable` is the one the memo was filled on. The
straightening, which also reads `ground`, `dynamic`, `cliffs` and where the
unit stands, is not kept: it runs every time.

The bound: at most 256 searches and 65,536 cells (512 KB) a grid; when
either would be passed the memo is emptied and starts again (with 600
different long questions in the test, it peaks at 256 searches and 44,705
cells). A mutex guards it, for `PathAhead`'s workers and the world's step
threads.

| 3 runs each in turns, load 14-15 | before | after |
|---|---:|---:|
| `worldbench 30` from the nest: the world's step, mean | 0.046 ms | 0.015-0.016 ms |
| its p50 / p95 | 0.037 / 0.121-0.125 ms | 0.008 / 0.049-0.052 ms |
| `spreadbench 20`, 2 regions (the start and the nest) | 41.4-42.4 µs | 16.0-16.6 µs |
| 4 regions (with the canyon wave) | 58.0-59.6 µs | 31.4-33.1 µs |
| 2 regions with 8 pilots each | 36.0-37.5 µs | 27.5-28.4 µs |
| `pathbench` (20,000 different questions) | 9.1-9.3 µs | 9.1-9.4 µs |
| `crowdbench 10`, 16 pilots at the start, tick | 102-106 µs | 97-103 µs |

Every hash is the same: worldbench's states (nest 1239e1800ff8ae42, start
c7644119037bfb4c), spreadbench's four, crowdbench's wires and states,
pathbench's paths (ac539c8c47941eeb); the core suite, its goldens and the
server and net suites pass. Tests: `searchMemo_aSearchAskedAgainIsTheSearch`
(60 long ways, exact and fast, asked again and from elsewhere in the
start cell, against a grid without the memo; none searched again) and
`searchMemo_aChangeToTheGridStartsItAgain` (deposits mined out: a new
memo, the ways a grid without one finds; a copy taken before keeps the old
memo and the old ways) and `searchMemo_manyQuestionsStayWithinItsBound`
(600 different long questions: the same ways as without the memo, never
past its bound).

It also takes most of the point out of round 18's threads: the nest's
step, the one heavy region, is a third of what it was.

### Round 20: the writer's items on one writer (2026-10-18)

With 16 pilots in the start region a pilot's snapshot writing
(`SnapshotWriter::write`) is most of the server's tick (16 x 13 µs every
third tick, against a step of 17-19 µs). Every item it wrote made a
`BitWriter` of its own, its bytes allocated and freed; and every
`BitWriter::writeBits` grew its bytes with `resize`, a call each time,
for the one or two bytes it adds. Now one writer holds each item in turn
(cleared, its room kept), and `writeBits` pushes the bytes it adds. The
same bytes in the same order.

| 4 runs each in turns, load 11-14 | before | after |
|---|---:|---:|
| `crowdbench 10`, 16 pilots: a pilot's write | 12.9-14.3 µs | 11.3-12.7 µs |
| the tick (step and all the snapshots) | 96-108 µs | 88-99 µs |
| `snapbench 20`, start at noon: write | 6.0-6.4 µs | 5.5-5.8 µs |
| the nest at noon: write | 11.3-12.3 µs | 10.2-11.3 µs |

The wires are the same (crowdbench's three, snapbench's four:
fd5e5104ff08f662, 467fab1c80dd1394, 35325d9463465925, d4d978dd55fbf064);
the net, server and core suites pass. Tried and left out: `writeBits`
inline in the header for up to 56 bits (no difference beyond this).

### Round 21: the stuck and trapped checks read the units in place (2026-10-18)

`Simulation::unstick` (every step) and `Simulation::trapped` (once a
second) copied every unit before looking at it: its waypoints, cargo and
the rest, allocated and freed, a third of `unstick`. They read it in
place now; `unstick` keeps where a stuck unit stood for its event before
it moves it (the copy's position before), and `trapped` changes only the
hit points, which its event does not carry. The same events, the same
state.

| 3 runs each in turns, load 11 | before | after |
|---|---:|---:|
| `spreadbench 20`, 4 regions | 32.9-33.1 µs | 30.4-31.6 µs |
| 2 regions | 15.7-17.1 µs | 15.7-16.2 µs |
| 2 regions with 8 pilots each | 27.8-28.3 µs | 26.9-27.5 µs |
| `worldbench 30` from the nest, mean | 0.015-0.016 ms | 0.015-0.016 ms |

Every hash is the same (spreadbench's four, worldbench's nest
1239e1800ff8ae42, crowdbench's); the core, server and net suites pass.
Core touched (Simulation). The long tail now: a few percent a change.

### Round 22: a ground viewer's cells eight at a time (2026-10-18)

In the nest by day the look is half of the region's step, and most of it
is the Brood's sight: its bugs stand on the ground, so `Vision::stamp`
took their circles cell by cell (seen where the level is not above the
viewer's and not seen yet, its explored bit gathered one at a time),
where the air's and the daylight's go eight at a time. The ground's go
eight at a time now too: the levels' bytes with their sign bit flipped
compare as unsigned bytes, the textbook byte-wise compare (`x >= y` in
each byte's high bit from `(x & ~y) | (~(x ^ y) & ((x | H) - (y & ~H)))`),
and with `seen`'s "not yet" the same multiply gathers the eight explored
bits. Exact for every level (any int8): the cell-by-cell test is the
reference.

`lookbench` now prints a hash of every side's sight (seen, near, the ids
it sees) and explored cells after its last look.

| `lookbench`, 3 runs each in turns, load 15-18 | before | after |
|---|---:|---:|
| the nest at noon (29 units) | 19.1-19.8 µs | 16.8-17.4 µs |
| the nest at midnight | 15.0-15.3 µs | 14.5-14.7 µs |
| the start, Cold Fork | 2.0-4.9 µs | 2.0-4.4 µs |

The sights hashes are the same in all six scenes; worldbench's and
spreadbench's state hashes too. Test `vision_stampComparesLevelsOfEveryByte`
(levels of every byte value, viewers on every level including -128 and
127, against the cell-by-cell stamp). Core touched (Vision).
