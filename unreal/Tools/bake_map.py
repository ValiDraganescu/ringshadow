# /// script
# requires-python = ">=3.11"
# ///
"""Bakes the world map's satellite pictures (M2; docs/regions.md, "The
satellite map"): run it after a region file changes.

    uv run unreal/Tools/bake_map.py [--only start,cold-fork] [--px 8] [--hour 10.5]

1. A hidden picture run of the game (AcMapBake.h): every region of
   unreal/server/worlds/first.json built as a client builds it, lit at the
   held hour, photographed straight down; unreal/Content-src/map/<id>.png.
2. A hidden editor run (Tools/Editor/import_map.py): the pictures into
   /Game/UI/Map/T_AcMap_<id>.

Both through hidden_run.py (one run at a time on the Mac). Build first
(unreal/Tools/build.sh). The runs' logs: unreal/server/build/map-bake/.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UNREAL = os.path.dirname(HERE)
REPO = os.path.dirname(UNREAL)


def run(args: list[str]) -> int:
    print("bake_map:", " ".join(args), flush=True)
    return subprocess.call(args, cwd=REPO)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--only", help="region ids, comma separated (default: all)")
    p.add_argument("--px", type=int, default=8, help="pixels a cell")
    p.add_argument("--hour", type=float, default=10.5, help="the hour of the light")
    p.add_argument("--world", default="unreal/server/worlds/first.json")
    p.add_argument("--no-import", action="store_true", help="the pictures only")
    a = p.parse_args()
    hidden = ["uv", "run", "unreal/Tools/hidden_run.py"]
    out = "unreal/server/build/map-bake"
    flags = ["-AcMap=highlands-small", "-AcMapBake=" + os.path.join(REPO, "unreal/Content-src/map"), f"-AcMapBakePx={a.px}",
             f"-AcMapBakeHour={a.hour}", "-AcMapBakeWorld=" + os.path.join(REPO, a.world), "-AcNoSave", "-nosound"]
    if a.only:
        flags.append("-AcMapBakeOnly=" + a.only)
    code = run(hidden + ["--out", out + "/pictures", "--"] + flags)
    if code != 0:
        sys.exit(code)
    if a.no_import:
        return
    sys.exit(run(hidden + ["--out", out + "/import", "--python", "unreal/Tools/Editor/import_map.py"]))


if __name__ == "__main__":
    main()
