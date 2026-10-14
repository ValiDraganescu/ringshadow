# /// script
# requires-python = ">=3.11"
# ///
"""Encodes a hidden run's raw recording (-AcShotRaw=FILE, AcShot.h) to an MP4.

A run through the build train's queue cannot hand the game a pipe, so the
recorder writes its BGRA frames to a plain file in the run's folder; this
encodes it (H.264, 30 fps) and writes a contact sheet. For an MMO clip
(-AcMmoShotsClip, AcMmoShots.cpp) the run had no fixed time step (the
server keeps the wall clock), so the frames came at the render's own rate:
the rate is the frames over the wall seconds between the log's
`shot: recording starts` and `shot: recorded N raw frames`, and the clip is
brought to 30 fps from it. It also prints where each moment
(`mmoshots: clip: NAME at world T s`) falls in the clip.

    uv run unreal/Tools/showreel/raw2mp4.py unreal/server/build/v3/runs/starfall [--fps 30] [--keep]

--fps: the run had -UseFixedTimeStep -FPS=N (each frame 1/N s of game
time). --keep keeps the raw file (deleted by default: about 8 MB a frame).
"""
import argparse
import re
import shutil
import subprocess
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
STAMP = re.compile(r"^\[(\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d:\d{3})\]")


def tool(name: str) -> str:
    local = REPO / "video/.tools/ffmpeg-bin/bin" / name
    if local.exists():
        return str(local)
    if found := shutil.which(name):
        return found
    out = subprocess.run(["nix", "--extra-experimental-features", "nix-command flakes", "build", "nixpkgs#ffmpeg.bin",
                          "--no-link", "--print-out-paths"], capture_output=True, text=True, check=True).stdout.strip()
    return f"{out}/bin/{name}"


def when(line: str) -> datetime | None:
    m = STAMP.match(line)
    return datetime.strptime(m.group(1), "%Y.%m.%d-%H.%M.%S:%f") if m else None


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", type=Path, help="the run's folder: run.log and one .raw")
    ap.add_argument("--fps", type=float, help="the run's fixed rate (-UseFixedTimeStep -FPS=N)")
    ap.add_argument("--size", default="1920x1080")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    raws = sorted(a.run.glob("*.raw"))
    if len(raws) != 1:
        raise SystemExit(f"want one .raw in {a.run}, found {len(raws)}")
    raw = raws[0]
    w, h = (int(v) for v in a.size.split("x"))
    frames = raw.stat().st_size // (w * h * 4)
    log = (a.run / "run.log").read_text(errors="replace").splitlines()
    start_wall = end_wall = None
    start_world = None
    moments = []
    for line in log:
        if "shot: recording starts at world" in line:
            start_wall = when(line)
            start_world = float(re.search(r"world ([\d.]+) s", line).group(1))
        elif re.search(r"shot: recorded \d+ raw frames", line):
            end_wall = when(line)
        elif m := re.search(r"mmoshots: clip: (\S+) at world ([\d.]+) s", line):
            moments.append((m.group(1), float(m.group(2))))
    rate = a.fps
    if not rate:
        if not (start_wall and end_wall):
            raise SystemExit("the log has no recording start and end; give --fps")
        rate = frames / (end_wall - start_wall).total_seconds()
    seconds = frames / rate
    print(f"{raw.name}: {frames} frames at {rate:.2f} fps: {seconds:.2f} s")
    for name, t in moments:
        print(f"  {name}: {t - start_world:7.2f} s" if start_world is not None else f"  {name}")
    out = raw.with_suffix(".mp4")
    subprocess.run([tool("ffmpeg"), "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "bgra", "-s", f"{w}x{h}",
                    "-framerate", f"{rate:.4f}", "-i", str(raw), "-vf", "fps=30", "-c:v", "libx264", "-crf", "14",
                    "-preset", "slow", "-pix_fmt", "yuv420p", str(out)], check=True)
    step = max(1, int(seconds * 30 / 8))
    subprocess.run([tool("ffmpeg"), "-v", "error", "-y", "-i", str(out), "-vf",
                    f"select='not(mod(n\\,{step}))',scale=640:-1,tile=3x3", "-frames:v", "1",
                    str(out.with_name(f"{out.stem}-contact.png"))], check=True)
    (out.with_name(f"{out.stem}-moments.txt")).write_text(
        f"{frames} frames at {rate:.2f} fps, {seconds:.2f} s\n"
        + "".join(f"{n} {t - start_world:.2f}\n" for n, t in moments if start_world is not None))
    if not a.keep:
        raw.unlink()
    print(out)


if __name__ == "__main__":
    main()
