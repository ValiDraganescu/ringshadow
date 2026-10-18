#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""Run one hidden editor or game process, one at a time on this Mac.

Every worktree's agents start their hidden runs through this script. It waits
for the machine-wide run slot (two runs at once fight for the GPU: one took
24 min instead of 4), adds the hidden flags, runs at low priority, kills a
run past its timeout, and prints what the log says.

    R="uv run unreal/Tools/hidden_run.py"
    $R --out unreal/server/build/g2 -- -AcMap=highlands-small -AcFlowTest=mmoshots -AcMmoShots={out} -AcMmoShotsG2
    $R --out unreal/server/build/hud --headless -- -AcFlowTest=mmohud,mmocross
    $R --out unreal/server/build/pick --tests Autocraft.Pick
    $R --out unreal/server/build/mat --python unreal/Tools/Editor/make_resource_materials.py
    $R --out unreal/Saved/Shots --raw -- uv run unreal/Tools/shots/compare.py top
    $R --out unreal/server/build/k3/run --app unreal/server/build/k3/Ringshadow.app -- -AcFlowTest=mmoshots ...

Modes: pictures (the default: -game -RenderOffscreen at --res), --headless
(-game -nullrhi: flow steps that need no pictures; cheaper), --tests PREFIX
(automation tests in the editor, -nullrhi), --python SCRIPT (an editor script
as the pythonscript commandlet), --app APP (a packaged Mac game instead of the
editor, in the pictures mode (not --headless: a cooked game asserts under
-nullrhi): it runs a copy of APP beside it,
APP-hidden.app, with LSUIElement set so it gets no Dock icon, signed ad hoc,
made again when APP is newer), --raw (the flags are a whole command, for a
tool that starts the editor itself: it runs as given, under the slot, at low
priority, and only its exit code counts). `{out}` in the flags becomes --out. The run
gets its own -AcSaveDir in --out unless the flags name one (a packaged game
its own config too: -UserDir=OUT/user/), and keeps the MMO
world server attached (-AcMmoServerAttached: it ends with the run) unless the
flags say -AcMmoServerDetached (the one scenario that checks the reattach; it
stops its server itself). A world server left running on the run's folder
is named at the end. Its log is OUT/run.log.

Exit codes: 0 ok, 1 failed (a flow step, a test, a crash, a script error),
2 timeout, 3 the module is missing (build first: unreal/Tools/build.sh).

The slot is an flock on ~/Library/Caches/Autocraft/hidden-run.lock: it frees
itself when its holder dies, so a crashed run never blocks the next.
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UNREAL = REPO / "unreal"
ENGINE = Path("/Users/Shared/Epic Games/UE_5.8")
# AUTOCRAFT_RUN_EDITOR and AUTOCRAFT_RUN_SLOT: a stand-in editor and lock, for tests of this script.
EDITOR_BG = Path(os.environ.get("AUTOCRAFT_RUN_EDITOR")
                 or ENGINE / "Engine/Binaries/Mac/UnrealEditorBG.app/Contents/MacOS/UnrealEditor")
EDITOR_CMD = Path(os.environ.get("AUTOCRAFT_RUN_EDITOR") or ENGINE / "Engine/Binaries/Mac/UnrealEditor-Cmd")
UPROJECT = UNREAL / "Autocraft.uproject"
MODULE = UNREAL / "Binaries/Mac/libUnrealEditor-Autocraft.dylib"
SLOT = Path(os.environ.get("AUTOCRAFT_RUN_SLOT") or Path.home() / "Library/Caches/Autocraft/hidden-run.lock")
HOLDER = SLOT.with_suffix(".holder")
HIDDEN = ["-unattended", "-nosplash", "-nosound",
          "-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False"]


def say(msg: str) -> None:
    print(f"[{datetime.now():%H:%M:%S}] {msg}", flush=True)


def newest(paths) -> float:
    return max((p.stat().st_mtime for p in paths if p.is_file()), default=0.0)


def staleness_warnings() -> list[str]:
    out = []
    if MODULE.exists() and newest((UNREAL / "Source").rglob("*")) > MODULE.stat().st_mtime:
        out.append("unreal/Source is newer than the module: this run has none of those edits "
                   "(build first: unreal/Tools/build.sh)")
    server = UNREAL / "server" / "build" / "autocraft-server"
    if server.exists():
        code = [*(UNREAL / "Source" / "AutocraftCore").rglob("*"), *(UNREAL / "Source" / "AutocraftNet").rglob("*"),
                *(UNREAL / "server" / "world").rglob("*"), *(UNREAL / "server" / "program").rglob("*")]
        if newest(code) > server.stat().st_mtime:
            out.append("the server's code is newer than unreal/server/build/autocraft-server "
                       "(make -C unreal/server first)")
    return out


def hidden_app(app: Path) -> Path:
    """A copy of the packaged APP with LSUIElement set (no Dock icon), its
    signature made again ad hoc; made once, again when APP changes."""
    copy = app.with_name(app.stem + "-hidden.app")
    stamp = copy.with_suffix(".from")
    want = f"{app.resolve()} {(app / 'Contents/Info.plist').stat().st_mtime} {app_binary(app).stat().st_mtime}"
    if stamp.exists() and stamp.read_text() == want:
        return copy
    say(f"making the hidden copy {copy}")
    if copy.exists():
        shutil.rmtree(copy)  # the old hidden copy, by its exact path
    subprocess.run(["ditto", str(app), str(copy)], check=True)
    plist = copy / "Contents/Info.plist"
    if subprocess.run(["/usr/libexec/PlistBuddy", "-c", "Set :LSUIElement true", str(plist)],
                      capture_output=True).returncode != 0:
        subprocess.run(["/usr/libexec/PlistBuddy", "-c", "Add :LSUIElement bool true", str(plist)], check=True)
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(copy)], check=True, capture_output=True)
    stamp.write_text(want)
    return copy


def app_binary(app: Path) -> Path:
    """The game's executable in a packaged .app (it keeps the project's name)."""
    macos = app / "Contents/MacOS"
    names = [p for p in macos.iterdir() if p.is_file() and os.access(p, os.X_OK)]
    for p in names:
        if p.name == "Autocraft":
            return p
    if len(names) == 1:
        return names[0]
    raise SystemExit(f"no game executable in {macos}")


def command(a, out: Path, flags: list[str]) -> list[str]:
    flags = [f.replace("{out}", str(out)) for f in flags]
    if a.raw:
        return flags
    # --sound-offline: an audio device on the offline mixer (nothing reaches the speakers).
    hidden = [("-deterministicaudio" if f == "-nosound" else f) for f in HIDDEN] if a.sound_offline else HIDDEN
    log = f"-abslog={out / 'run.log'}"
    if a.python:
        return [str(EDITOR_CMD), str(UPROJECT), "-run=pythonscript",
                f"-script={Path(a.python).resolve()}", "-nullrhi", *hidden, log, *flags]
    # The MMO world server ends with the run unless the run asks otherwise.
    if not any(f.startswith("-AcMmoServerDetached") for f in flags):
        flags = ["-AcMmoServerAttached", *flags]
    if a.tests:
        return [str(EDITOR_BG), str(UPROJECT), "-nullrhi", *hidden, log,
                f"-ExecCmds=Automation RunTests {a.tests}; Quit",
                "-TestExit=Automation Test Queue Empty", *flags]
    if a.app:
        # A packaged game: no project, no -game; the same hidden flags.
        cmd = [str(app_binary(hidden_app(Path(a.app).resolve()))), a.level]
    else:
        cmd = [str(EDITOR_BG), str(UPROJECT), a.level, "-game"]
    if a.headless:
        cmd += ["-nullrhi"]
    else:
        w, h = a.res.split("x")
        cmd += ["-RenderOffscreen", "-windowed", "-ForceRes", f"-ResX={w}", f"-ResY={h}"]
    cmd += [*hidden, log]
    if not any(f.startswith("-AcSaveDir") for f in flags):
        cmd.append(f"-AcSaveDir={out / 'save'}")
    # A packaged game keeps its config (GameUserSettings.ini) in the user's
    # Library unless told otherwise: the run's own, in OUT/user (U2).
    if a.app and not any(f.lower().startswith("-userdir=") for f in flags):
        cmd.append(f"-UserDir={out / 'user'}/")
    return cmd + flags


def python_errors(lines: list[str]) -> list[str]:
    """The script's own LogPython errors; a traceback wholly inside the engine
    (a stock plugin failing at editor start) is not the script's."""
    blocks, cur = [], []
    for l in lines:
        if "LogPython: Error" in l:
            cur.append(l.split("LogPython:", 1)[1].strip())
        elif cur:
            blocks.append(cur)
            cur = []
    if cur:
        blocks.append(cur)
    mine = [b for b in blocks if not (any(str(ENGINE) in x for x in b)
                                      and not any(str(REPO) in x for x in b))]
    return [x[:200] for b in mine for x in b][:8]


def read_log(path: Path, python: bool) -> tuple[bool, list[str]]:
    """(failed, lines worth showing)"""
    text = path.read_text(errors="replace") if path.exists() else ""
    lines = text.splitlines()
    show, failed = [], False
    if not lines:
        return True, ["the log stayed empty: the editor did not start"]
    if "could not be found. Please ensure that this module exists" in text:
        return True, ["the Autocraft module could not be found: build first (unreal/Tools/build.sh)"]
    m = re.findall(r"flowtest: done, (\d+) failed", text)
    if m:
        show.append(f"flow test done, {m[-1]} failed")
        failed |= int(m[-1]) > 0
        show += ["  " + l.split("flowtest:", 1)[1].strip()[:200] for l in lines
                 if "flowtest:" in l and re.search(r"\bFAIL", l)][:8]
    m = re.findall(r"mapbake: done, (\d+) regions, (\d+) failed", text)
    if m:
        show.append(f"map bake done, {m[-1][0]} regions, {m[-1][1]} failed")
        failed |= int(m[-1][1]) > 0
        show += ["  " + l.split("mapbake:", 1)[1].strip()[:200] for l in lines if "mapbake:" in l and "Error" in l][:8]
    pics = len(re.findall(r"mmoshots: saved ", text))
    if pics:
        show.append(f"{pics} pictures")
    done = [l for l in lines if "Test Completed" in l]
    if done:
        bad = [l.strip()[-200:] for l in done if not re.search(r"Success|Passed", l)]
        show.append(f"{len(done) - len(bad)} tests passed, {len(bad)} failed")
        show += ["  " + b for b in bad[:8]]
        failed |= bool(bad)
    if python:
        errs = python_errors(lines)
        if errs:
            show.append(f"{len(errs)} Python error lines")
            show += ["  " + e for e in errs]
            failed = True
    # A handled ensure doesn't fail the run, but it is a stack walk on the
    # game thread and a bug to name (K3: the packaged game's).
    ensures = [l.split("Ensure condition failed:", 1)[1].strip()[:160] for l in lines if "Ensure condition failed:" in l]
    if ensures:
        show.append(f"{len(ensures)} handled ensures")
        show += ["  " + e for e in ensures[:4]]
    if re.search(r"=== Critical error|Fatal error|Assertion failed", text):
        show.append("CRASHED: read the log")
        failed = True
    return failed, show


# A run whose process died before its log opened is tried this often.
STILLBORN_TRIES = 3


def stillborn(out: Path, binary: str, began: float) -> str | None:
    """Why a run died before writing its log, from the crash report macOS
    wrote for it, or None when the log has lines (an ordinary failure).

    The packaged Development game sometimes dies at launch (K3, 2 of ~35):
    the Cocoa thread allocates in AppKit's launch while the game thread turns
    the low-level memory tracker (LLM) off, unguarded on the Mac
    (FLLMGlobals::ProcessCommandLineInner locks only with
    PLATFORM_HAS_MULTITHREADED_PREMAIN), and FLLMTracker::PopTag finds an
    empty stack. An engine race; Shipping has no LLM."""
    log = out / "run.log"
    if log.exists() and log.stat().st_size > 0:
        return None
    reports = Path.home() / "Library/Logs/DiagnosticReports"
    name = Path(binary).name
    for _ in range(10):  # the report lands a moment after the exit
        found = sorted((p for p in reports.glob(f"{name}-*.ips") if p.stat().st_mtime >= began - 1),
                       key=lambda p: p.stat().st_mtime)
        if found:
            text = found[-1].read_text(errors="replace")
            if "FLLMTracker::PopTag" in text:
                return f"the Development build's LLM start-up race (engine; {found[-1].name})"
            return f"a crash at launch ({found[-1].name})"
        time.sleep(0.5)
    return "no crash report"


def servers_left(out: Path) -> list[str]:
    """World servers still running on the run's folder (a detached one the
    run did not stop)."""
    ps = subprocess.run(["ps", "-axo", "pid=,command="], capture_output=True, text=True).stdout
    return [l.strip()[:200] for l in ps.splitlines()
            if "autocraft-server" in l.split(" --", 1)[0] and str(out) in l]


def take_slot(out: Path):
    SLOT.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(SLOT, os.O_RDWR | os.O_CREAT, 0o644)
    told = False
    while True:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            break
        except BlockingIOError:
            if not told:
                try:
                    h = json.loads(HOLDER.read_text())
                    who = f"{h.get('worktree')} ({h.get('out')}, since {h.get('since')})"
                except (OSError, ValueError):
                    who = "another run"
                say(f"waiting for the run slot: {who}")
                told = True
            time.sleep(3)
    HOLDER.write_text(json.dumps({"pid": os.getpid(), "worktree": REPO.name, "out": str(out),
                                  "since": f"{datetime.now():%H:%M:%S}"}))
    return fd


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", required=True, help="the run's folder; {out} in the flags becomes it")
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--headless", action="store_true", help="-game -nullrhi: no pictures")
    mode.add_argument("--tests", help="automation tests, e.g. Autocraft.Pick")
    mode.add_argument("--python", help="an editor script, e.g. unreal/Tools/Editor/make_input.py")
    mode.add_argument("--raw", action="store_true", help="the flags are a whole command to run under the slot")
    p.add_argument("--app", help="a packaged Mac game (.app) to run instead of the editor")
    p.add_argument("--level", default="/Game/Maps/Battlefield")
    p.add_argument("--res", default="1600x1000")
    p.add_argument("--timeout", type=float, default=15, help="minutes")
    p.add_argument("--sound-offline", action="store_true",
                   help="-deterministicaudio instead of -nosound: an audio device on the offline mixer, silent")
    p.add_argument("flags", nargs=argparse.REMAINDER)
    a = p.parse_args()
    flags = a.flags[1:] if a.flags[:1] == ["--"] else a.flags
    out = Path(a.out).expanduser()
    out = (out if out.is_absolute() else Path.cwd() / out).resolve()
    out.mkdir(parents=True, exist_ok=True)

    if a.app:
        if a.tests or a.python or a.raw or a.headless:
            # -nullrhi: a cooked game may not build mesh render data, and the
            # game builds meshes at run time (StaticMesh.cpp asserts, K3).
            p.error("--app runs the game in the pictures mode only (a cooked game asserts under -nullrhi)")
        if not (Path(a.app) / "Contents/MacOS").is_dir():
            p.error(f"--app: no packaged app at {a.app}")
    elif not MODULE.exists():
        print("the Autocraft module is missing: build first (unreal/Tools/build.sh)")
        sys.exit(3)
    else:
        for w in staleness_warnings():
            say("WARNING: " + w)

    fd = take_slot(out)
    cmd = command(a, out, flags)
    started = time.time()
    say(f"running in {out}")
    timed_out = False
    for attempt in range(1, STILLBORN_TRIES + 1):
        began = time.time()
        with (out / "run.out").open("w") as so:
            child = subprocess.Popen(["nice", "-n", "10", *cmd], stdout=so, stderr=subprocess.STDOUT,
                                     stdin=subprocess.DEVNULL, cwd=REPO, start_new_session=True)
            try:
                child.wait(timeout=a.timeout * 60)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
                timed_out = True
            except KeyboardInterrupt:
                os.killpg(child.pid, signal.SIGKILL)
                raise
        # A run that died before its log opened yields nothing: said why
        # (its crash report) and run again, never a silent empty result.
        why = None if a.raw or timed_out or child.returncode == 0 else stillborn(out, cmd[0], began)
        if why is None:
            break
        say(f"the run died before its log opened (exit {child.returncode}): {why}"
            + (f"; again ({attempt + 1} of {STILLBORN_TRIES})" if attempt < STILLBORN_TRIES else ""))
    fcntl.flock(fd, fcntl.LOCK_UN)
    os.close(fd)

    secs = time.time() - started
    if a.raw:
        failed, show = child.returncode != 0, [f"exit code {child.returncode}; output in {out / 'run.out'}"]
    else:
        failed, show = read_log(out / "run.log", bool(a.python))
    if timed_out:
        show = [f"killed after {a.timeout:g} min"]
    elif child.returncode not in (0, None):
        failed = True
        show.append(f"exit code {child.returncode}")
    status = "TIMEOUT" if timed_out else "FAILED" if failed else "OK"
    say(f"{status}, {secs:.0f} s; log {out / ('run.out' if a.raw else 'run.log')}")
    for line in show:
        print(line)
    for left in servers_left(out):
        print(f"WARNING: a world server still runs on this run's folder: {left}")
    module_missing = any("module could not be found" in l for l in show)
    sys.exit(2 if timed_out else 3 if module_missing else 1 if failed else 0)


if __name__ == "__main__":
    main()
