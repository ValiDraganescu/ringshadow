# Release builds

How Ringshadow is packaged for players: Windows and Linux on a Windows
machine in AWS, made on demand, and the Mac build on this Mac. The script is
`unreal/Tools/release/build_release.sh`. Run it with no arguments for its
commands.

Status, 2026-10-08: the build machine's image is `ringshadow-build-20261008-1342`
(`ami-0fed5dee9c6ddb99a`), warmed on the setup machine: the first full Win64
and Linux build (`138d96b`) took 12 minutes, and a rebuild of `7737714`, which changed
only the build scripts, took 2 on the same machine. The first release from the
image (`a042937`) took 28 minutes from launch to terminate, 23 of them on
the build: that commit made five plugins editor-only, which recompiles the
game for both platforms, and the image's cache is older than it. Refresh the
image ("Refreshing the image") to bring the cache up to date. Nobody has
played those packages yet. "Files outside the cook" lists what the game reads
outside the cook and how it is staged.

## What is built where

| Platform | Built on | Package |
|---|---|---|
| Windows (x64) | Windows EC2 machine, UE 5.8 from the Epic Games Launcher | `Ringshadow-<version>-windows.zip` |
| Linux (x64) | The same machine, cross-compiled with Epic's toolchain | `Ringshadow-<version>-linux.tar.gz` |
| macOS (Apple silicon) | This Mac | `Ringshadow-<version>-mac.zip` |

One Linux build runs on Ubuntu, Omarchy (Arch) and the other mainstream
distributions: Unreal's Linux target carries its own C++ runtime and needs
glibc and a Vulkan driver. Unreal cannot build Windows or Linux on a Mac, and
an x86 VM on Apple silicon is too slow for a cook, so those two need a real
x86 machine.

The packages land in `unreal/Saved/Releases/<version>/` (ignored by git), with
`SHA256SUMS`. Publishing is a GitHub pre-release on the public repo
(`ValiDraganescu/ringshadow`), and only when the developer says to release.
One command does all of it:

```sh
unreal/Tools/release/build_release.sh release v0.1.2 --notes notes.md   # HEAD; or give a commit
```

1. Tags the commit, locally.
2. Builds Windows and Linux in AWS (`build`, log `release-aws.log`) and the
   Mac in the release worktree (`mac`, log `release-mac.log`), at the same time.
3. Copies the Mac zip in beside the other two and writes `SHA256SUMS` over
   the three.
4. Pushes the tag and runs `gh release create --prerelease` with the notes.

If a build fails, nothing is published and the tag stays local. `--on-demand`
goes to the AWS build, and `--region` before `release` too. The notes are
written by hand: what changed, the download table, how to install, the
known problems, like v0.1.1's on GitHub.

The Windows symbols stay in S3. The first release, `v0.1.0` from `a9e3d2e`,
went up on 2026-10-09: Windows and Linux 416 and 419 MB, the Mac 535 MB,
signed and notarized.

The release build tree is the worktree `~/git/dev/autocraft-release`. Other
sessions keep uncommitted work in `~/git/dev/autocraft`, and `mac` packages
the working tree, so the Mac build runs from the worktree: `release` checks
it out at the commit (`AUTOCRAFT_RELEASE_TREE` names another). Its
`unreal/Saved` and `Intermediate` stay between releases, so a release
recompiles only what changed.

**The version.** A release is named by its tag: tag `v0.1.1`, and
`git describe` gives `0.1.1` (the `v` dropped), or `0.1.1-4-g<sha>` four
commits later. It goes into each package's name. The Windows exe's Product
version carries it (UBT's `-BuildVersion`; the File version stays the
engine's 5.8.3). On the Mac it goes into `CFBundleShortVersionString` (`0.1.1`)
and `CFBundleVersion` (`0.1.1`, or `0.1.1.4` past the tag). `--version 0.1.1`
names a build before the tag exists.

## Files outside the cook

A cooked game reads only what is cooked or staged. The game reads some plain
files from the project folder itself; `Autocraft.Build.cs` stages them loose,
at the same paths, for the game targets (`RuntimeDependencies`, `NonUFS`):
- the music, `Resources/Sounds/music/*.mp3` (`AcMusicPlayer.cpp`);
- the launcher art, `Content-src/launcher/*.jpg` (`AcLauncherArt.cpp`);
- the skill icons, `Resources/Textures/Skills/skill_icons.png`
  (`SAcSkillScreen.cpp`, drawn by `Tools/hud/skill_icons.py`);
- `Content/Audio/Sounds.json` and `Content/Models/ModelCatalog.json`;
- the cursors, `Content/UI/Cursors`. macOS loads the TIFFs. Windows and
  Linux load `Cursor_<kind>.png` and `@2x.png` instead
  (`UGameViewportClient::LoadCursorFromPngs`). `Tools/cursors/bake_cursors.sh`
  writes both.

A new file the game reads at run time goes on that list too. `Shatter` is
staged inside the pak by `DirectoriesToAlwaysStageAsUFS` in
`DefaultGame.ini`.

Cooked assets the code loads by path (`/Game/Weather/M_AcRain...`) are found
by no reference the cook follows: each such folder is in
`DirectoriesToAlwaysCook`. `build_release.sh cook-check` (run by `mac`)
fails when a `/Game/<folder>/` named in `unreal/Source` is missing there:
until K3 (2026-10-15) `/Game/Weather` was, and the packaged game had no
rain, no lens drops and no lightning.

The editor's plugins (`ModelContextProtocol`, `AllToolsets`,
`PythonScriptPlugin`, `EditorScriptingUtilities`, `USDImporter`) have
`"TargetAllowList": ["Editor"]` in `Autocraft.uproject`, so a player's game
carries no MCP server and no Python.

The console's art (the dashboard, the cab's frame, the plates) is drawn
the same way on every platform since 2026-10-08: Slate geometry baked per
window size (`AcCabArt.cpp`, `AcArtList.h`, `AcBakedArt.h`). It was Core
Graphics on the Mac and flat colour elsewhere before.

Already fine: the saves (`AcSaves.cpp` uses the platform's settings folder off
the Mac) and the Mac-only code (icon, watchdog, GPU timing), which is behind
`PLATFORM_MAC`. The executable keeps the project's name, `Autocraft`
(AGENTS.md). The folder in each package is called `Ringshadow`.

The cheapest way to find what else is missing is `build_release.sh mac`: a
Mac package built here, played by hand, before any AWS time is spent.

### The server in the Mac package

The MMO mode starts `autocraft-server` (`unreal/server`) as a detached
process that outlives the app (S2, [mmorpg.md](mmorpg.md) "The world runs
on"): quitting Ringshadow leaves the world running until the Mac shuts down
or the home screen's STOP. Its run file sits beside the save
(`~/Library/Application Support/Autocraft/Unreal/mmo/first.run`). An updated
app (another build of the server) asks the running one to save and stop and
starts its own.
It is not part of the cook: `build_release.sh mac` builds it with
`make -C unreal/server` (arm64, the tests' flags, `-O2`, not stripped) and
copies it into the app after UAT, before signing:

```
Ringshadow.app/Contents/Resources/server/
  autocraft-server
  worlds/first.json        names its regions as ../regions/<name>.json
  regions/*.json
```

A packaged Mac game (`FAcMmoClient::BundledServerDir`, cooked data on the
Mac) starts that program with `--world` at the absolute path of
`worlds/first.json` beside it. Editor runs keep `unreal/server/build/autocraft-server`
and `unreal/server/worlds/first.json` in the repo. Without the program the
mode shows a banner: "missing from this package" in a package, "not built,
make -C unreal/server" in the editor, each with the path it looked at. The
pilot key, the world's save and the server log stay in the player's folders
(`~/Library/Application Support/Autocraft/Unreal/mmo`,
`~/Library/Logs/Autocraft/server.log`), never in the app. `sign` signs the
server like every other Mach-O file in the app. Windows and Linux packages
carry no server yet: they need the AWS build machine.

`build_release.sh mac --ad-hoc` signs the app ad hoc, on this Mac only (no
certificate, no timestamp, no notarizing), for a local try. A bundle with
no fresh signature at all does not start: UAT's own carries the App Sandbox
entitlement and the renamed `Info.plist` breaks it ("Signing the Mac build").
2026-10-13, `mac --config Development --ad-hoc` with the cook already warm:
3 minutes, the app 1.0 GB, the zip 617 MB, the server 2.6 MB of it. Its
binary, run hidden with `-AcMmo` (a copy with `LSUIElement` set, so no Dock
icon), started the bundled server and was seated in a Prospector.

### Measuring packaged

A packaged game is measured the way hidden editor runs are (K3,
2026-10-15): build it with `build_release.sh mac --config Development
--ad-hoc` (Shipping compiles the log out, and the numbers are read from
it), copy the app into the ignored `unreal/server/build/k3/`, and run it
with `hidden_run.py --app`:

```sh
uv run unreal/Tools/hidden_run.py --out unreal/server/build/k3/runs/app-call5-1 --res 1728x1084 \
  --app unreal/server/build/k3/Ringshadow.app -- -AcMap=highlands-small -AcFlowTest=mmoshots \
  -AcMmoShots={out} -AcMmoShotsOnly=none -AcMmoShotsF2 -AcMmoHour=12 -AcMmoShotsF2CallAt=5
```

The MMO bench runs the same way on a package:
`uv run unreal/Tools/bench/bench.py --mmo --app unreal/Saved/Releases/V/mac/Ringshadow.app`
([perf.md](perf.md) "Client": its scenes, cooked beside the editor).

`--app` runs `Ringshadow-hidden.app` beside it (`LSUIElement` set, signed
ad hoc, made again when the app changes): no Dock icon, offscreen,
`-nosound`, its own save and its own world server, one run at a time with
every other hidden run. It lists handled ensures, which a packaged game
hits where the editor doesn't. Not `--headless`: a cooked game asserts
under `-nullrhi` when it builds a mesh at run time ("RenderData should not
initialize resources in headless cooked runs"), and this game builds the
ground and more at run time. Every MMO run logs `k3: seated N s after
the process started` (the startup: process start to the first frame the
pilot drives); `-AcMmoShotsF2CallAt=S` calls the Dropship S s after
seating and logs `worst frame since the call`; `-AcMmoShotsS3` logs the
canyon (wave 10 out) and the nest at noon as `s3 perf:`. A trace: add
`-trace=default,gpu,loadtime,task -statnamedevents -tracefile={out}/x.utrace`
and export it with `UnrealInsightsBG` (unreal/Tools/README.md). A traced
packaged run crashes on exit, after its log and the trace are closed (exit
-11, the trace whole): the engine's own check that every `FMemStack` page
is back (`TLockFreeFixedSizeAllocator`'s destructor, at the process's
static teardown) fails with tracing on, and the failed check logs into the
log already torn down. An engine cost of the trace, nothing of the game's.

K3's numbers, three runs each, M4 Max, other worktrees' runs and builds
beside (load 6–12), before and after the fixes below:

| | packaged before | packaged after | editor before | editor after |
|---|---|---|---|---|
| Startup to the first driven frame | 3.5–5.5 s | 3.0–4.2 s | 14.4–23.7 s | 12.9–16.9 s |
| Dropship called 5 s after seating: worst frame | 1547 / 92 / 32 ms | 35 / 37 / 42 ms | 1628 / 78 / 160 ms | 366 / 60 / 38 ms |
| Called 30 s after seating: worst frame | 62 / 31 ms | 30 / 39 / 30 ms | 31 / 92 / 30 ms | 104 / 36 / 48 ms |
| Frames over 250 ms in the 10 s after seating | 2–3 a run, to 1.8 s | none | 2–5 a run, to 1.9 s | 0–3 a run, to 365 ms |

Frame times, p50 / p95 (the first packaged run of each, under load 12,
had a p95 of 17–20 ms, left out; the fixes touch the session's first seconds only, and a
run of each after them matched):

| Scene | packaged 1728×1084 | editor 1728×1084 | packaged 2560×1440 | editor 2560×1440 |
|---|---|---|---|---|
| Canyon, wave 10 out (The Fork, noon) | 12.0–12.3 / 13.1 ms | 12.5–12.6 / 13.5–13.6 ms | 13.0–13.4 / 14.1 ms | 13.2–13.6 / 14.2–14.6 ms |
| The nest at noon (Slag Pockets, 34 bugs) | 11.8–11.9 / 12.9–13.0 ms | 11.8–12.1 / 12.8–13.1 ms | 12.6–13.2 / 13.8–14.2 ms | 12.9–13.3 / 14.2–14.5 ms |

The packaged game draws as fast as the editor's game; it starts four
times sooner. The first call's stall showed in both, and its cause was
not the call: a session's first ~10 s ran at 1–2 s frames, and a call
landing there took one. The trace named three causes, all fixed:
- The game's long CPU jobs (a region made ahead, a ground set's prepare,
  a minimap bake: about 1 s each) ran on the engine's thread pool, which
  is the task graph's two background workers. A 108 ms engine task the
  render thread waited on (`UpdatePrimitive`) sat 860 ms behind them. They
  run on the game's own pool now (`AcWork.h`, three threads).
- The baked console art read its copy back as soon as it could, and Metal
  maps a readback only once the whole GPU is idle
  (`RHIMapStagingSurface`): 668 ms with the session's first frames queued.
  It reads once the GPU is calm (`AcBakedArt.cpp`).
- The world map loaded each region's picture as it first drew it, behind
  everything else the loader read then (1–2 s a picture). They load as
  the world arrives (`AcMmoAround.cpp`).

What only the package showed: `/Game/Weather` was not cooked (no rain;
`cook-check` above); the GameFeatureData rule named a class the package
lacks, an ensure at every start; the meshes the game builds at run time
had no UV densities, an ensure (`AcBuiltMesh.h`). Left as they are: the
engine's volumetric cloud default (`m_SimpleVolumetricCloud_Inst`) is not
cooked and logs a warning once (the sky sets its own material), and two
packaged launches in about 35 died at once with nothing in their log. The
crash reports name an engine race in the Development build: the low-level
memory tracker (LLM) starts on and is turned off once the command line is
read, on the game thread, while the Cocoa thread allocates in AppKit's
launch; the switch takes its lock only where
`PLATFORM_HAS_MULTITHREADED_PREMAIN`, not on the Mac, and
`FLLMTracker::PopTag` finds an empty stack. Shipping has no LLM, so
players never hit it. `hidden_run.py` runs a launch that died before its
log opened again (three tries) and says why, from its crash report.

### The packaged first run (U2)

A packaged first run must need nothing else. The check (2026-10-15) runs
two of `mmosettings`'s steps on a Development package, signed ad hoc, from
the home screen to the field and back. Both runs share one fresh config
folder:

```sh
unreal/Tools/release/build_release.sh mac --version u2 --config Development --ad-hoc
ditto unreal/Saved/Releases/u2/mac/Ringshadow.app unreal/server/build/u2/pkg/Ringshadow.app
U=$PWD/unreal/server/build/u2/pkg-user/
R="uv run unreal/Tools/hidden_run.py"
$R --out unreal/server/build/u2/pkg-run1 --app unreal/server/build/u2/pkg/Ringshadow.app -- -AcMap=highlands-small \
  -UserDir=$U -AcRemember -AcSettingsFirstRun -AcFlowTest=mmosettings -AcMmoShots={out}
$R --out unreal/server/build/u2/pkg-run2 --app unreal/server/build/u2/pkg/Ringshadow.app -- -AcMap=highlands-small \
  -UserDir=$U -AcRemember -AcFlowTest=mmosettingsback
```

1. **The first run** finds no config. It checks a first run's defaults: full
   screen at the display's own resolution, music 80, voice and effects 100,
   the lag off.
2. It opens SETTINGS from the home screen, sets VOICE to 55 and SCREEN to a
   WINDOW at the smallest size offered, and presses OK. It enters the Reach,
   is seated, and checks the view at that size.
3. It opens the pause menu's SETTINGS, flips the lag switch on and off,
   opens the KEY CARD, and quits to home.
4. **The second run** starts the app again on the same config. It reads
   VOICE 55 and the window back from `Saved/Config/Mac/GameUserSettings.ini`,
   enters the field and comes back.

Both ran in 15 and 7 s with no failures and no ensures.

`hidden_run.py --app` now gives every packaged run its own config folder,
`-UserDir=OUT/user/`, unless the flags name one. Without it, a packaged game
reads and writes `~/Library/Application Support/Epic/Autocraft/Saved/Config/`,
the user's own. A hidden run never takes the display: OK resizes its window
offscreen, and a full screen is only logged (`UAcSettings::ApplyScreen`).

## AWS

- **Account** 569854554192, CLI profile `ringshadow` (`aws login --profile
  ringshadow`; a session lasts about 12 hours). The script uses it by default.
  `AUTOCRAFT_AWS_PROFILE` picks another.
- **Region** `eu-north-1` (Stockholm): the bucket, the images and, by
  default, the machines. The organization's policy (SCP
  AdvancedModeRegionRestrictionSecurityControlPolicy, `p-lunneq4k`, edited
  from the management account 687971795322) allows eu-north-1 and, since
  2026-10-09, all of us-east-1 too; us-west-2 only for a few global services,
  every other region not at all. The tester's driver bucket in us-east-1 is
  public and read without credentials ("The Windows tester").
- **us-east-1, the fallback** when Stockholm has no room (spot GPUs there are
  often full): `build_release.sh --region us-east-1 <command>`. Its GPU
  quota is the same 8 vCPU, on-demand and spot (granted 2026-10-09).
  `--region us-east-1 infra` set it up on 2026-10-09: the security group, the
  same key pair (imported from `~/.ssh/ringshadow-build.pem`) and copies of
  the build and tester images (`ami-00786c2ee550bb2b1`,
  `ami-0d99b06a6e3d80f32`). After `image` saves a new build image, run
  `--region us-east-1 infra` again to copy it. The machines there still use
  the Stockholm bucket: every S3 call names its region, and SSM writes its
  output there too.
- **Quotas** (2026-10-08): 32 vCPU of standard instances, on-demand and spot.
  That is one `c7i.8xlarge`, so the setup machine and a build cannot run at
  the same time. GPU (G and VT) instances: 8 vCPU, on-demand and spot
  (granted 2026-10-08), one `g6.2xlarge`.
- **What the script makes** (`infra`; every resource is tagged
  `Project=ringshadow`; the tester's machines and image too):
  - Bucket `ringshadow-builds-569854554192`: private, objects expire after 60
    days.
  - Role and instance profile `ringshadow-build`: SSM, plus read and write on
    the bucket.
  - Security group `ringshadow-build`: no inbound rules. SSM and remote
    desktop both run over SSM's outbound connection, so no port is open.
  - Key pair `ringshadow-build` (`~/.ssh/ringshadow-build.pem`): used only to
    decrypt the Windows Administrator password.

### Costs

Spot prices in `eu-north-1` on 2026-10-08:

| What | Price |
|---|---|
| `c7i.8xlarge` Windows, spot | $1.62 an hour. About $1.47 of that is the Windows licence, which spot does not discount. |
| The same, on-demand | About $2.90 an hour. The setup machine runs on-demand. |
| The 400 GB gp3 disk at 6000 IOPS and 500 MB/s | Cents an hour while a machine runs. |
| The image's snapshot | About $0.05 a GB-month on the used blocks: 111 GB, about $5.60 a month. |
| A release (Windows and Linux) | $0.75 for `a042937`: 28 minutes of a spot machine, a full recompile. A release that recompiles less costs less. |
| A month at 2 to 3 releases a week | At most $7–10 of machine time, $5.60 for the image and under $1 for S3 and the downloads: about $15. |
| The one-time setup | The setup machine ran 3 hours on demand on 2026-10-08, the Windows build fixes included: about $9. |
| `g6.2xlarge` (NVIDIA L4) for playtesting, Windows, spot | $0.47 an hour. |
| The tester image's snapshot | 34 GB, about $1.70 a month. |
| The us-east-1 copies of both images | 111 + 34 GB, about $7.30 a month, and about $2.90 once for the copy across regions. |
| Linux tester (2026-10-09, eu-north-1) | `g5.2xlarge` (A10G) $1.29 an hour on demand, $0.33 spot; `g6.2xlarge` (L4) $0.20 spot. No Windows licence. |

The times come from CloudTrail (`RunInstances`, `TerminateInstances`) and
each build's `build.log`; the snapshot's size from `aws ec2
describe-snapshots` (`FullSnapshotSizeInBytes`).

## One-time setup

1. **`build_release.sh setup`** runs `infra`, then starts an on-demand
   `c7i.8xlarge` from the newest Windows Server 2022 image, with a 400 GB
   disk. Through SSM it runs `setup_windows.ps1`, which installs:
   - Visual Studio 2022 Build Tools: the C++ workload, MSVC 14.44 (17.14) and
     Windows SDK 10.0.22621, the versions in
     `Engine/Config/Windows/Windows_SDK.json`.
   - Git and the AWS CLI.

   The MSVC folder is named `14.44.35207`, inside the 14.44.0–14.44.35210
   range the engine bans. That is fine: Visual Studio services the compiler
   in place (`cl.exe` was 19.44.35229 on 2026-10-08), and UBT reads the
   version from `cl.exe`, not from the folder name.

   It also clones the repo to `C:\build\autocraft`, leaves `C:\build` and the
   engine out of Defender's scans, and puts two installers on the desktop.
   It takes about 10 minutes from the launch. The Visual Studio installer
   keeps working in the background for a few minutes after the script ends.
2. **`build_release.sh rdp`** prints the Administrator password and opens a
   tunnel. Connect Windows App (Mac App Store) to `localhost:13389`. On the
   machine:
   1. Run `EpicGamesLauncherInstaller.msi` and sign in to the launcher.
   2. Install **Unreal Engine 5.8.3**, the same version as on the Mac, at the
      default path (`C:\Program Files\Epic Games\UE_5.8`). In its Options,
      tick the **Linux** target platform. Untick Starter Content, Templates
      and the editor debug symbols, which are large and not needed.
   3. Run `v26_clang-20.1.8-rockylinux8.exe`, the Linux cross toolchain named
      in `Engine/Config/Linux/Linux_SDK.json`. It sets `LINUX_MULTIARCH_ROOT`.
   If **Downloads** ends with no `Engine\Intermediate\Build\Linux` in the
   engine folder, the Linux tick did not take: the Library tab's engine tile,
   ▾ next to its Launch, Options, tick Linux, Apply. Then sign out of the
   launcher, so the image keeps no Epic session.
3. **Warm build:** `build_release.sh build --instance <setup id>`. This is the
   first package. It compiles every shader for both platforms into the
   machine's derived data cache, which the image keeps, so later builds skip
   that work. This is also where packaging problems show up first. The setup
   machine is still there to look at them over remote desktop.
4. **`build_release.sh image`** saves the setup machine as an image
   (`ringshadow-build-<date>`), waits until it is ready (about 15 minutes for
   the 400 GB disk on 2026-10-08), then terminates the machine.

## A release

```sh
git push                                     # the machine clones from GitHub
unreal/Tools/release/build_release.sh build  # HEAD; or a tag, a branch, a commit
unreal/Tools/release/build_release.sh mac    # the Mac package, here
```

`build`:
1. Checks that the commit is on GitHub.
2. Starts a spot `c7i.8xlarge` from the newest image and waits for SSM.
3. Runs `build_windows.ps1`: fetch, check out the commit, then
   `RunUAT BuildCookRun` for Win64 and Linux (Shipping, pak, IoStore,
   compressed, no debug files). The Windows package carries the Visual C++
   runtime next to the game's exe (UAT's app-local prerequisites: the DLLs
   from the Build Tools' `VC\Redist\MSVC\<ver>\x64\Microsoft.VC143.CRT`,
   found with `vswhere`), so a PC without the runtime starts it; the build
   fails if `vcruntime140.dll` is not beside the exe. The runtime's installer
   comes along too, in `Engine\Extras\Redist\en-us\vc_redist.x64.exe`.
4. Zips each package and uploads the zips, a `manifest.txt` and `build.log`
   to `s3://ringshadow-builds-569854554192/builds/<version>/`.
5. Downloads them to `unreal/Saved/Releases/<version>/` and sets the
   executable bits that a tar made on Windows cannot carry.
6. Terminates the machine, on failure too.

Options:
- `--version`: the game's version and the packages' name (default:
  `git describe` of the commit, without the `v`).
- `--platforms Win64` or `--platforms Linux`: build only one.
- `--config Development`: a build with logs and the console.
- `--on-demand`: if spot capacity is short.
- `--keep`: leave the machine running to look at it (`rdp <id>`, then `stop`).

If AWS reclaims a spot machine mid-build, the script stops with "spot
reclaimed?". Run it again.

`status` lists the machines, images and builds. `stop` terminates every
Ringshadow machine.

### Refreshing the image

After an engine update, or when builds get slow because the cache in the
image has fallen behind:
1. `build --keep`.
2. If the engine changed: `rdp <id>` and update it in the launcher.
3. `image <id>`.

Then delete the older image and its snapshot:

```sh
aws ec2 deregister-image --image-id ami-…
aws ec2 delete-snapshot --snapshot-id snap-…
```

`status` lists the images.

## Signing the Mac build

Gatekeeper stops an unsigned download: the player has to right-click and
choose Open. `build_release.sh mac` therefore ends with `sign`, which:
1. Signs every Mach-O file in the `.app`, then the bundle, with the
   **Developer ID Application** certificate in the keychain, hardened
   runtime, a timestamp and no entitlements (the game needs none outside a
   sandbox; Unreal's own `Unrestricted.entitlements` is empty too).
2. Submits the zip to Apple with `xcrun notarytool submit --wait`.
3. Staples the ticket to the `.app` and zips it again, so Gatekeeper passes
   offline, then checks it with `spctl --assess`.

Setup, once per Mac:
- The certificate: Xcode → Settings → Accounts → the team → Manage
  Certificates → + → Developer ID Application (the team's Account Holder).
- The notary login: an app-specific password from account.apple.com, then
  `xcrun notarytool store-credentials ringshadow` (Apple ID, team ID, that
  password). `AUTOCRAFT_NOTARY_PROFILE` names another profile,
  `AUTOCRAFT_SIGN_IDENTITY` another certificate.

Without the certificate the zip is unsigned; without the profile it is
signed but not notarized. `build_release.sh sign APP ZIP` signs an `.app`
that is already packaged.

## Testing the packages

The build machine has no GPU. A package is proven when someone plays it:
- **Windows in AWS:** `build_release.sh test [VERSION]`, below.
- **Linux in AWS:** `build_release.sh test --linux [VERSION]`, below.
- **Linux on real hardware:** a player on Ubuntu or Omarchy, by hand.

### The Windows tester

```sh
unreal/Tools/release/build_release.sh test            # the newest Windows package in the bucket
unreal/Tools/release/build_release.sh test a8a6b03    # or one by name
unreal/Tools/release/build_release.sh dcv             # the remote desktop again, after Ctrl-C
unreal/Tools/release/build_release.sh rdp             # or Windows App (RDP) on localhost:13389; RDP draws with the GPU too
unreal/Tools/release/build_release.sh stop            # done
```

`test` starts a spot `g6.2xlarge` (NVIDIA L4, 24 GB: DirectX 12 and Vulkan;
about $0.47 an hour). Stockholm often has none free: `launch` tries every zone
that offers the type, and the tester falls back to a `g5.2xlarge` (A10G) and
then a `g4dn.2xlarge` (T4), all 8 vCPU and served by the same driver (on
2026-10-08 only the A10G had room, on demand). It unpacks the package to
`C:\Ringshadow` and installs nothing else, as on a player's PC. It prints the
exe's version, whether the machine has the Visual C++ runtime installed, and
which `vcruntime140.dll` the game loads in a short run without a GPU
(`-nullrhi`): the one beside the exe when the app-local runtime works. Then
it puts a shortcut on the desktop and opens a tunnel to
Amazon DCV, a remote desktop that streams what the GPU draws (plain remote
desktop cannot show a 3D game well). Open `https://localhost:18443` in a
browser, accept the machine's own certificate and sign in as Administrator
with the password it prints. The machine switches itself off and is
terminated after 4 hours (`--hours H`), and `stop` ends it sooner.

The first `test` sets the tester up from Windows Server 2022 with
`setup_tester.ps1` (the AWS CLI, NVIDIA's GRID driver, which AWS licenses
for its G instances, and DCV), reboots it and saves it as the image
`ringshadow-tester-<date>`. Later ones start from that image.

The driver comes from AWS's public bucket `ec2-windows-nvidia-drivers` in
us-east-1, over plain HTTPS without credentials: a signed read would meet the
organization's region policy (see AWS above).

### The Linux tester

```sh
unreal/Tools/release/build_release.sh test --linux             # the newest Linux package
unreal/Tools/release/build_release.sh test --linux 0.1.1 --on-demand
unreal/Tools/release/build_release.sh --region us-east-1 test --linux
```

It starts the same GPU types from AWS's Ubuntu 24.04 image with the NVIDIA
driver (`base-oss-nvidia-driver-gpu-ubuntu-24.04`, found through its SSM
parameter in each region). That image has the driver, the driver's X module
and Vulkan, but no X server. `setup_linux_tester.sh` adds one on every start
(about 5 minutes; no image is saved):
- Xorg on the GPU with a 1920x1080 virtual screen and no monitor
  (`nvidia-xconfig --allow-empty-initial-configuration`).
- Xfce, logged in as `ubuntu` by LightDM.
- Amazon DCV sharing that screen as its console session.

`stage` unpacks the package to `/home/ubuntu/Ringshadow` and puts a launcher
on the desktop. Then it runs the game once, silent:
- After a minute the game must still be running and on the GPU.
- The script takes a picture of the screen and saves it to
  `unreal/Saved/Releases/<version>/linux-home.png`.
- It quits the game and opens the same DCV tunnel. Sign in as `ubuntu` with
  the password it prints.

The Shipping build writes no log, so the picture is the proof that the game
reached the home screen.

The first run, 0.1.1 on 2026-10-09, went to us-east-1 because Stockholm had
no spot GPU free. On a spot `g6.2xlarge` (NVIDIA L4, driver 595.91), the
setup took 1.5 minutes and staging 1.5 minutes. The game was still up and on
the GPU after a minute, at the home screen, the first time the Linux build had
run anywhere. The window's title still reads Autocraft, the project's name inside.
The Windows tester ran the same day: the exe's Product version read 0.1.1,
and the game loaded `vcruntime140.dll` from its own folder. That machine has
the runtime installed, but the copy beside the exe comes first in Windows's
search order.
