#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""Two game clients on one world server, hidden, for the W5 checks
(docs/mmorpg-play.md "Two pilots"; the flow step `mmotwo`, AcMmoTwo.cpp).

Run it under the one hidden-run slot, as a raw run:

    R="uv run unreal/Tools/hidden_run.py"
    $R --out unreal/server/build/w5/two1 --timeout 40 --raw -- \\
        uv run unreal/Tools/two_clients.py --out unreal/server/build/w5/two1 --headless
    $R --out unreal/server/build/w5/shots1 --timeout 40 --raw -- \\
        uv run unreal/Tools/two_clients.py --out unreal/server/build/w5/shots1 --shots unreal/server/build/w5

Game A starts the world server and names it in OUT/join.txt
(-AcMmoShare); game B joins it (-AcMmoJoin) through the lag simulator
(150 ms, 20 ms jitter, 5% loss). Each has its own save folder and pilot key
(OUT/A/save, OUT/B/save). They keep in step through OUT/sync. B leaves mid-
wave (its game ends); this script starts it again (-AcMmoTwoBack, the same
save folder and key) and it comes back. The logs: OUT/A/run.log,
OUT/B/run.log, OUT/B2/run.log. Exit 0 when all three passed.

The hidden flags and the command line are hidden_run.py's own (`command`);
this script never takes the slot itself (its caller holds it).
"""

from __future__ import annotations

import argparse
import importlib.util
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("hidden_run", HERE / "hidden_run.py")
hr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(hr)


def launch(root: Path, name: str, flags: list[str], headless: bool, res: str, level: str) -> subprocess.Popen:
    out = root / name
    out.mkdir(parents=True, exist_ok=True)
    a = argparse.Namespace(raw=False, python=None, tests=None, app=None, headless=headless, res=res, level=level)
    cmd = hr.command(a, out, flags)
    hr.say(f"{name}: starting ({out / 'run.log'})")
    so = (out / "run.out").open("w")
    return subprocess.Popen(cmd, stdout=so, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, cwd=hr.REPO)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", required=True, help="the run's folder (new: its sync marks must not be there yet)")
    p.add_argument("--headless", action="store_true", help="-nullrhi: the flows, no pictures")
    p.add_argument("--shots", help="the pictures' folder (-AcMmoShots=DIR -AcMmoShotsW5)")
    p.add_argument("--res", default="1600x1000")
    p.add_argument("--level", default="/Game/Maps/Battlefield")
    p.add_argument("--minutes", type=float, default=35, help="the whole run's limit")
    a = p.parse_args()
    root = Path(a.out).expanduser()
    root = (root if root.is_absolute() else Path.cwd() / root).resolve()
    sync = root / "sync"
    if sync.exists() and any(sync.iterdir()):
        sys.exit(f"{sync} has marks from an earlier run: use a new --out")
    sync.mkdir(parents=True, exist_ok=True)
    common = ["-AcFlowTest=mmotwo", f"-AcMmoTwoSync={sync}"]
    if a.shots:
        shots = Path(a.shots).expanduser()
        shots = (shots if shots.is_absolute() else Path.cwd() / shots).resolve()
        common += [f"-AcMmoShots={shots}", "-AcMmoShotsW5"]
    share = root / "join.txt"
    flags_a = common + ["-AcMmoTwo=A", f"-AcMmoShare={share}"]
    flags_b = common + ["-AcMmoTwo=B", f"-AcMmoJoin={share}", "-AcMmoLag=150,20,5"]
    headless = a.headless and not a.shots
    deadline = time.time() + a.minutes * 60

    def wait(proc: subprocess.Popen, name: str, done: Path | None = None) -> int:
        # `done`: the game's last mark. The engine's teardown can hang after
        # the flow ends; 20 s past the mark it is killed and counts as ended.
        done_at = None
        while proc.poll() is None:
            if done is not None and done_at is None and done.exists():
                done_at = time.time()
            if done_at is not None and time.time() - done_at > 20:
                hr.say(f"{name}: still closing 20 s after {done.name}, killed")
                proc.kill()
                proc.wait()
                return 0
            if time.time() > deadline:
                hr.say(f"{name}: past {a.minutes:g} min, killed")
                proc.kill()
                proc.wait()
                return -9
            time.sleep(1)
        hr.say(f"{name}: exit {proc.returncode}")
        return proc.returncode

    game_a = launch(root, "A", flags_a, headless, a.res, a.level)
    game_b = launch(root, "B", flags_b, headless, a.res, a.level)
    codes = {"B": wait(game_b, "B", sync / "B.quit")}
    # B left mid-wave on purpose: its game again, its own save and key.
    if (sync / "B.quit").exists():
        game_b2 = launch(root, "B2", flags_b + ["-AcMmoTwoBack", f"-AcSaveDir={root / 'B' / 'save'}"], headless, a.res, a.level)
        codes["B2"] = wait(game_b2, "B2")
    else:
        hr.say("B never left mid-wave (no B.quit mark): no second B")
        if game_a.poll() is None and not (sync / "A.failed").exists():
            (sync / "B.failed").write_text("")
    codes["A"] = wait(game_a, "A")

    failed = False
    for name in ("A", "B", "B2"):
        log = root / name / "run.log"
        if name not in codes:
            print(f"{name}: not run")
            failed = True
            continue
        bad, show = hr.read_log(log, False)
        bad = bad or codes[name] not in (0, None)
        failed |= bad
        print(f"{name}: {'FAILED' if bad else 'OK'} (exit {codes[name]}), {log}")
        for line in show:
            print("  " + line)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
