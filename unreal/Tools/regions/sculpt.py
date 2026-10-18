# /// script
# requires-python = ">=3.11"
# dependencies = ["shapely"]
# ///
"""World sculpting (docs/world-sculpting.md): opens a region up.

S1, a low rim: the land grows out over the old walls' ground (where the
bare ground is level and walkable), the border drops to `--level` with a
`--slope` of broken rock, and every edge shared with a neighbour opens wide
wherever both sides' ground is level 0, in BOTH files (their outlines go out
to the edge round each new span), so the two read as one plain.

S2, free-standing rock: `--rocks` islands of kind butte, mesa or spire at
their own level, clear of everything that must stay reachable.

It rewrites the files, then formats and validates each with
`autocraft-region`. The same seed gives the same rock.

  uv run unreal/Tools/regions/sculpt.py REGION [--grow 8] [--level 1] [--slope 5]
      [--rocks butte:4,butte:3,mesa:2,spire:5,spire:4] [--seed N] [--no-widen] [--dry]
"""
import argparse, json, math, random, subprocess, sys
from pathlib import Path

from shapely.geometry import Point, Polygon, box
from shapely.ops import unary_union

ROOT = Path(__file__).resolve().parents[3]
REG = ROOT / "unreal/server/regions"
TOOL = ROOT / "unreal/server/build/autocraft-region"
GRID = [["ashen-shelf", "cold-fork", "glass-flats"],
        ["rust-hollow", "start", "tern-ridge"],
        ["dust-terraces", "brine-pans", "slag-pockets"]]
W, D = 144.0, 96.0
EDGES = ["north", "east", "south", "west"]
OPPOSITE = {"north": "south", "south": "north", "east": "west", "west": "east"}
STEP = {"north": (-1, 0), "south": (1, 0), "west": (0, -1), "east": (0, 1)}
# The walls' faces stand this far past each end of a span (RegionEdges::gapMargin).
MARGIN = 1.5
# Clear round what must stay reachable, cells past a rock's own size; a pad's
# ring is the Citadel's guns' field of fire (a butte there shields a raid).
KEEP = {"pads": 30, "spawns": 8, "sites": 9, "deposits": 7, "wells": 8}
LANDMARKS = {"sunkHauler", "derrickRuin", "tower", "arch", "relayDish", "crateStack", "crate", "broodTunnel"}


def run(*cmd):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True)


def bare(rid):
    """The region's bare ground a cell at a time (`levels --bare`)."""
    out = run(TOOL, "levels", REG / f"{rid}.json", "--bare").stdout.splitlines()
    return [l for l in out if l and set(l) <= set("0123456789#o")]


def neighbour(rid, edge):
    for r, row in enumerate(GRID):
        if rid in row:
            c = row.index(rid)
            dr, dc = STEP[edge]
            if 0 <= r + dr < 3 and 0 <= c + dc < 3:
                return GRID[r + dr][c + dc]
    return None


def strip(g, edge, lo=5, hi=14):
    """Each cell along `edge`: the bare ground from `lo` to `hi` cells in."""
    H, Wd = len(g), len(g[0])
    if edge == "north": return ["".join(g[z][i] for z in range(lo, hi)) for i in range(Wd)]
    if edge == "south": return ["".join(g[H - 1 - z][i] for z in range(lo, hi)) for i in range(Wd)]
    if edge == "west": return ["".join(g[i][z] for z in range(lo, hi)) for i in range(H)]
    return ["".join(g[i][Wd - 1 - z] for z in range(lo, hi)) for i in range(H)]


def level_runs(a, b, least=10, trim=2.5, corner=10):
    """Spans along a shared edge where both sides' bare ground is level 0."""
    good = [set(x) == {"0"} and set(y) == {"0"} for x, y in zip(a, b)]
    runs, i = [], 0
    while i < len(good):
        if not good[i]: i += 1; continue
        j = i
        while j < len(good) and good[j]: j += 1
        lo, hi = max(i + trim, corner), min(j - trim, len(good) - corner)
        if hi - lo >= least: runs.append([lo, hi])
        i = j
    return runs


def merge(spans):
    spans = sorted([list(s) for s in spans])
    out = []
    for s in spans:
        # Gaps whose faces would touch merge into one.
        if out and s[0] <= out[-1][1] + 2 * MARGIN + 7: out[-1][1] = max(out[-1][1], s[1])
        else: out.append(s)
    return out


def strip_box(edge, span, depth):
    """Land from the edge (a cell past it) `depth` cells in, over the span and its faces."""
    a, b = span[0] - MARGIN - 0.5, span[1] + MARGIN + 0.5
    if edge == "north": return box(a, -1, b, depth)
    if edge == "south": return box(a, D - depth, b, D + 1)
    if edge == "west": return box(-1, a, depth, b)
    return box(W - depth, a, W + 1, b)


def walkable_poly(g):
    """The bare level-0 ground as one shape (cells)."""
    cells = [box(i, z, i + 1, z + 1) for z, row in enumerate(g) for i, ch in enumerate(row) if ch == "0"]
    return unary_union(cells)


def clamp(pts):
    return [[round(min(max(x, 0.0), W), 1), round(min(max(z, 0.0), D), 1)] for x, z in pts]


def outline_of(poly):
    if poly.geom_type != "Polygon":
        poly = max(poly.geoms, key=lambda p: p.area)
    poly = poly.simplify(0.35)
    pts = list(poly.exterior.coords)[:-1]
    return clamp(pts)


def widen(rid, files, dry):
    """S1: each shared edge open wide where both sides are level (both files)."""
    mine = bare(rid)
    for edge in EDGES:
        nb = neighbour(rid, edge)
        if not nb: continue
        far = OPPOSITE[edge]
        runs = level_runs(strip(mine, edge), strip(bare(nb), far))
        have = [o["span"] for o in files[rid].get("openings", {}).get(edge, [])]
        spans = merge(have + runs)
        print(f"{rid}.{edge} | {nb}.{far}: {have} -> {spans}")
        for r, e in ((rid, edge), (nb, far)):
            f = files[r]
            f.setdefault("openings", {})[e] = [{"span": [round(s[0], 1), round(s[1], 1)], "level": 0} for s in spans]
            land = Polygon(f["border"]["outline"])
            add = []
            for s in spans:
                # As deep as it takes to meet the land well (the span's
                # ground is level 0 on both sides: `level_runs`).
                for depth in range(6, 40, 2):
                    piece = strip_box(e, s, depth)
                    if piece.intersection(land).area > 4 * (s[1] - s[0]): break
                add.append(strip_box(e, s, depth + 2))
            f["border"]["outline"] = outline_of(unary_union([land] + add).buffer(1.2).buffer(-1.2))


def grow(rid, f, cells):
    """S1: the land over the old walls' level, walkable ground, `cells` out."""
    land = Polygon(f["border"]["outline"])
    walk = walkable_poly(bare(rid))
    grown = land.buffer(cells, join_style="round").intersection(walk.buffer(-1.5).buffer(1.5)).intersection(
        box(3, 3, W - 3, D - 3))
    # The edges' own strips (the openings) stay as they were.
    keep = unary_union([land.intersection(box(-1, -1, W + 1, 3.5)), land.intersection(box(-1, D - 3.5, W + 1, D + 1)),
                        land.intersection(box(-1, -1, 3.5, D + 1)), land.intersection(box(W - 3.5, -1, W + 1, D + 1))])
    shape = unary_union([land, grown, keep]).buffer(2.5).buffer(-2.5)
    f["border"]["outline"] = outline_of(shape)


def rocks(rid, f, spec, seed):
    """S2: free-standing islands, clear of what must stay reachable."""
    rng = random.Random(f"{rid}:{seed}")
    land = Polygon(f["border"]["outline"])
    old = [Polygon(i if isinstance(i, list) else i["outline"]) for i in f["border"].get("islands", [])]
    g = bare(rid)
    walk = walkable_poly(g)
    keep = []
    for k, r in KEEP.items():
        for p in f.get(k, []): keep.append((p["at"], r))
    for d in f.get("doodads", []):
        if d["kind"] in LANDMARKS: keep.append((d["at"], 8))
    for z in f.get("zones", []):
        for e in z.get("entrances", []): keep.append((e["at"], 10))
    lanes = []
    for edge, spans in f.get("openings", {}).items():
        for s in spans:
            for t in range(int(s["span"][0]), int(s["span"][1]) + 1, 3):
                lanes.append({"north": (t, 0), "south": (t, D), "west": (0, t), "east": (W, t)}[edge])
    ramps = [e for e in f.get("height", []) if e.get("op") == "ramp"]
    islands, placed = [], []
    for item in spec:
        kind, level = item.split(":")
        level = int(level)
        r = {"butte": (3.0, 5.0), "mesa": (4.5, 7.5), "spire": (1.7, 2.6)}[kind]
        for _ in range(6000):
            rad = rng.uniform(*r)
            x, z = rng.uniform(10, W - 10), rng.uniform(10, D - 10)
            c = Point(x, z)
            if not land.buffer(-(rad + 6)).contains(c): continue
            if not walk.buffer(-(rad + 2)).contains(c): continue
            if any(o.distance(c) < rad + 6 for o in old): continue
            if any(math.dist((x, z), q) < rad + m for q, m in keep): continue
            if any(math.dist((x, z), q) < rad + 18 for q in lanes): continue
            if any(Point(e["from"]).distance(c) < rad + 8 or Point(e["to"]).distance(c) < rad + 8 for e in ramps): continue
            if any(math.dist((x, z), p) < rad + q + 12 for p, q in placed): continue
            break
        else:
            print(f"  no room for a {kind}")
            continue
        placed.append(((x, z), rad))
        n = 7 if kind == "spire" else 11
        pts, stretch, turn = [], rng.uniform(0.75, 1.0), rng.uniform(0, math.pi)
        for i in range(n):
            a = 2 * math.pi * i / n + rng.uniform(-0.18, 0.18)
            rr = rad * rng.uniform(0.78, 1.18)
            u, v = rr * math.cos(a), rr * stretch * math.sin(a)
            pts.append([round(x + u * math.cos(turn) - v * math.sin(turn), 1), round(z + u * math.sin(turn) + v * math.cos(turn), 1)])
        islands.append({"outline": pts, "level": level, "kind": kind})
        print(f"  {kind} level {level} at ({x:.0f}, {z:.0f}) r {rad:.1f}")
    f["border"]["islands"] = f["border"].get("islands", []) + islands


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("region")
    ap.add_argument("--grow", type=float, default=8)
    ap.add_argument("--level", type=int, default=1)
    ap.add_argument("--slope", type=float, default=5)
    ap.add_argument("--rocks", default="butte:4,butte:3,mesa:2,spire:5,spire:4")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--no-widen", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    rid = a.region
    files = {rid: json.loads((REG / f"{rid}.json").read_text())}
    for e in EDGES:
        nb = neighbour(rid, e)
        if nb: files[nb] = json.loads((REG / f"{nb}.json").read_text())
    if not a.no_widen: widen(rid, files, a.dry)
    f = files[rid]
    grow(rid, f, a.grow)
    f["border"]["level"] = a.level
    if a.slope > 0: f["border"]["slope"] = a.slope
    rocks(rid, f, [s for s in a.rocks.split(",") if s], a.seed)
    changed = files if not a.no_widen else {rid: f}
    if a.dry:
        print(json.dumps(f["border"], indent=1)[:2000]); return
    bad = 0
    for r, body in changed.items():
        path = REG / f"{r}.json"
        path.write_text(json.dumps(body, indent=2) + "\n")
        run(TOOL, "format", path)
        v = run(TOOL, "validate", path)
        print(v.stdout.strip() or v.stderr.strip())
        bad += v.returncode != 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
