#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""The idle soak (Q3, docs/mmorpg.md "Soak"): the world server alone, no
client on line, as it runs on with the game closed.

    uv run unreal/Tools/soak.py run --out unreal/server/build/q3/soak --minutes 120
    uv run unreal/Tools/soak.py catchup --out unreal/server/build/q3/catchup

`run` makes a save (build/soak-save: a pilot's rig, two mining mercenaries
and a guard, the Beacon under way, The Fork held, the pilot off line with
its away ledger, the frontier pilots landed), serves it with
build/autocraft-server at low priority (nice 10) and writes OUT/soak.csv
every `--every` seconds: resident memory, CPU time, the save and its WAL,
the log, and the server's idle stats line (the idle calls' mean and max,
the world's clock, the ledgers' rows, the deposits' ore). OUT/table.md is
the table for the docs.

`table` pairs OUT/soak.csv with the log's stats lines again and rewrites
OUT/table.md.

`catchup` stops nothing it did not start: it makes a save, serves it once
(a minute), stops it, then starts it again as if it had been down 30 h
(`--dev-down`) and checks what the start did: the clock moved 24 h at
most, the start's time, the ledger bounded, the week told once.

Everything under OUT, never the player's save. Run from the repo root.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import signal
import sqlite3
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SERVER_DIR = ROOT / "unreal" / "server"
SERVER = SERVER_DIR / "build" / "autocraft-server"
SOAK_SAVE = SERVER_DIR / "build" / "soak-save"
USER_SAVES = Path.home() / "Library" / "Application Support" / "Autocraft"

IDLE_STATS = re.compile(
    r"idle stats: (\d+) calls, mean ([\d.]+) ms max ([\d.]+) ms, clock (\d+) s, ledger (\d+) rows, "
    r"deposits (-?\d+) ore, radio (\d+) lines"
)
FIELDS = [
    "minute", "rss_kb", "cpu_s", "save_bytes", "wal_bytes", "log_bytes", "idle_calls", "idle_mean_ms",
    "idle_max_ms", "world_clock_s", "ledger_rows", "deposits_ore", "radio_lines",
]


def build() -> None:
    subprocess.run(["make", "-C", str(SERVER_DIR), "build/autocraft-server", "build/soak-save"], check=True,
                   stdout=subprocess.DEVNULL)


def fresh(out: Path) -> Path:
    out = out.resolve()
    if USER_SAVES in out.parents:
        sys.exit(f"soak: {out} is under the player's saves")
    out.mkdir(parents=True, exist_ok=True)
    save = out / "world.sqlite"
    if save.exists():
        sys.exit(f"soak: {save} exists; give a new --out")
    return save


def make_save(save: Path) -> str:
    r = subprocess.run([str(SOAK_SAVE), str(save)], cwd=SERVER_DIR, check=True, capture_output=True, text=True)
    return r.stdout.strip()


def start(save: Path, log: Path, extra: list[str]) -> tuple[subprocess.Popen, float]:
    """The server on `save`, niced; returns it and the seconds to its PORT line."""
    t0 = time.monotonic()
    p = subprocess.Popen(
        ["nice", "-n", "10", str(SERVER), "--world", "worlds/first.json", "--save", str(save), "--log", str(log), *extra],
        cwd=SERVER_DIR, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
    )
    assert p.stdin and p.stdout
    p.stdin.write("soak\n")
    p.stdin.flush()
    line = p.stdout.readline().strip()
    if not line.startswith("PORT"):
        p.kill()
        sys.exit(f"soak: the server did not start: {line}")
    return p, time.monotonic() - t0


def stop(p: subprocess.Popen) -> None:
    p.send_signal(signal.SIGTERM)
    p.wait(timeout=30)


def ps(pid: int) -> tuple[int, float]:
    """Resident KB and CPU seconds."""
    r = subprocess.run(["ps", "-o", "rss=,time=", "-p", str(pid)], capture_output=True, text=True)
    rss, cpu = r.stdout.split()
    parts = [float(x) for x in cpu.replace("-", ":").split(":")]
    secs = 0.0
    for x in parts:
        secs = secs * 60 + x
    return int(rss), secs


def size(path: Path) -> int:
    return path.stat().st_size if path.exists() else 0


def idle_stats(log: Path) -> list[list[str]]:
    """The server's idle stats lines, in order."""
    return [list(m.groups()) for line in log.read_text(errors="replace").splitlines() if (m := IDLE_STATS.search(line))]


def nth_idle_stats(log: Path, k: int) -> list[str]:
    """The k-th idle stats line (1 the first), waited for up to 30 s: the
    server's lines drift a little later than the sampler's clock."""
    if k < 1:
        return [""] * 7
    for _ in range(60):
        lines = idle_stats(log)
        if len(lines) >= k:
            return lines[k - 1]
        time.sleep(0.5)
    return [""] * 7


def run(args: argparse.Namespace) -> None:
    build()
    out = Path(args.out)
    save = fresh(out)
    out = save.parent
    print(make_save(save), flush=True)
    log = out / "server.log"
    p, took = start(save, log, ["--idle-stats", str(args.every)])
    print(f"soak: server {p.pid} up in {took:.2f} s, {args.minutes} min, a row every {args.every} s -> {out / 'soak.csv'}",
          flush=True)
    rows = []
    began = time.monotonic()
    try:
        with open(out / "soak.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(FIELDS)
            k = 0
            while True:
                # A few seconds after the server's own stats line.
                due = began + k * args.every + (3 if k else 1)
                time.sleep(max(0.0, due - time.monotonic()))
                if p.poll() is not None:
                    sys.exit(f"soak: the server ended ({p.returncode})")
                stats = nth_idle_stats(log, k)
                rss, cpu = ps(p.pid)
                row = [f"{(time.monotonic() - began) / 60:.1f}", rss, f"{cpu:.2f}", size(save),
                       size(Path(str(save) + "-wal")), size(log), *stats]
                w.writerow(row)
                f.flush()
                rows.append(row)
                print(", ".join(f"{n}={v}" for n, v in zip(FIELDS, row)), flush=True)
                k += 1
                if k * args.every > args.minutes * 60:
                    break
    finally:
        stop(p)
    table(out, rows)


def table(out: Path, rows: list[list]) -> None:
    lines = ["| min | RSS MB | CPU s | save KB | WAL KB | log KB | idle mean ms | idle max ms | ledger rows | deposits ore |",
             "|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in rows:
        lines.append(f"| {float(r[0]):.0f} | {int(r[1]) / 1024:.1f} | {float(r[2]):.1f} | {int(r[3]) / 1024:.0f} | "
                     f"{int(r[4]) / 1024:.0f} | {int(r[5]) / 1024:.1f} | {r[7] or '-'} | {r[8] or '-'} | {r[10] or '-'} | {r[11] or '-'} |")
    (out / "table.md").write_text("\n".join(lines) + "\n")
    print((out / "table.md").read_text())


def ledger(save: Path) -> list[dict]:
    db = sqlite3.connect(f"file:{save}?mode=ro", uri=True)
    rows = []
    for (away,) in db.execute("SELECT away FROM pilots"):
        if away:
            rows += json.loads(away).get("rows", [])
    db.close()
    return rows


def catchup(args: argparse.Namespace) -> None:
    build()
    save = fresh(Path(args.out))
    out = save.parent
    print(make_save(save), flush=True)
    log = out / "server.log"
    # A minute served, then stopped: the save as a closed game leaves it.
    p, _ = start(save, log, ["--idle-stats", "30"])
    time.sleep(60)
    stop(p)
    before = ledger(save)
    down = 30 * 3600
    p, took = start(save, log, ["--dev-down", str(down), "--idle-stats", "30"])
    time.sleep(2)
    stop(p)
    after = ledger(save)
    text = log.read_text()
    moves = re.findall(r"catch-up: down (\d+) s, the world moved on (\d+) s \(cap (\d+) s\)", text)
    moved = int(moves[-1][1]) if moves else 0
    cap = int(moves[-1][2]) if moves else 0
    weeks = [r for r in after if r["kind"] == 11]
    mercs = {r["who"]: r["value"] for r in after if r["kind"] == 0}
    mercs_before = {r["who"]: r["value"] for r in before if r["kind"] == 0}
    frontier_new = [r for r in after if r["kind"] == 1 and r not in before]
    checks = {
        f"moved {moved} s <= cap {cap} s": 0 < moved <= cap == 24 * 3600,
        f"start {took:.2f} s < 1 s": took < 1.0,
        f"ledger {len(before)} -> {len(after)} rows (<= 120)": len(after) <= 120,
        f"weeks told {len(weeks)} (once)": len(weeks) == 1,
        f"frontier deeds in the gap: {len(frontier_new)}": len(frontier_new) == 0,
    }
    for who, v in mercs.items():
        print(f"catch-up: {who} delivered {v - mercs_before.get(who, 0)} ore in the gap")
    for what, ok in checks.items():
        print(("ok    " if ok else "FAIL  ") + what)
    (out / "catchup.json").write_text(json.dumps({"moved": moved, "cap": cap, "start_s": took, "before": before,
                                                   "after": after, "checks": checks}, indent=1))
    sys.exit(0 if all(checks.values()) else 1)


def retable(args: argparse.Namespace) -> None:
    """OUT/soak.csv's idle columns again from the log's stats lines, the
    k-th row with the k-th line (a run sampled before its line was out),
    and OUT/table.md."""
    out = Path(args.out)
    stats = idle_stats(out / "server.log")
    with open(out / "soak.csv") as f:
        rows = list(csv.reader(f))[1:]
    for k, row in enumerate(rows):
        row[6:] = stats[k - 1] if 1 <= k <= len(stats) else [""] * 7
    with open(out / "soak.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(FIELDS)
        w.writerows(rows)
    table(out, rows)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--out", required=True)
    r.add_argument("--minutes", type=float, default=120)
    r.add_argument("--every", type=float, default=300)
    c = sub.add_parser("catchup")
    c.add_argument("--out", required=True)
    t = sub.add_parser("table")
    t.add_argument("--out", required=True)
    args = ap.parse_args()
    {"run": run, "catchup": catchup, "table": retable}[args.cmd](args)


if __name__ == "__main__":
    main()
