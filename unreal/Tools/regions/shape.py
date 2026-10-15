# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "scikit-image", "scipy", "shapely"]
# ///
"""The First Reach's region shapes (W4b, docs/regions.md "border").

Draws each region's playable land as a natural shape well inside its cell:
deep bays, rock spurs, necks where two bays face each other, now and then an
island of high rock, the corners mostly rock, and a valley narrowing into
each passage (the openings stay as they are). Everything that must stay
reachable (pads, spawn points, sites, wells, the nest, the landmarks, the
ramps' ends) keeps land round it; a deposit the walls take moves in onto the
land near where it was; plants and debris on the rock go.

It rewrites each region file's `border` (outline, islands), its deposits and
its doodads, then formats and validates it with `autocraft-region`. The same
seed gives the same shape; `--seed` tries another for one region.

  uv run unreal/Tools/regions/shape.py [REGION ...] [--seed REGION=N] [--dry]
"""
import json, math, random, re, subprocess, sys
from pathlib import Path

import numpy as np
from scipy import ndimage
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import nearest_points, unary_union
from skimage import measure
from skimage.graph import MCP_Geometric

ROOT = Path(__file__).resolve().parents[3]
REG = ROOT / "unreal/server/regions"
TOOL = ROOT / "unreal/server/build/autocraft-region"
GRID = [["ashen-shelf", "cold-fork", "glass-flats"],
        ["rust-hollow", "start", "tern-ridge"],
        ["dust-terraces", "brine-pans", "slag-pockets"]]
W, D = 144.0, 96.0
STEP = 0.5          # the field's sample, cells
PAD = 6.0           # sampled past each edge, cells
EDGES = ["north", "east", "south", "west"]
# Land kept round what must stay reachable, cells past its own size.
KEEP = {"pad": 12, "spawn": 7, "site": 10, "well": 9, "nest": 14, "landmark": 8, "ramp": 5}
LANDMARKS = {"sunkHauler", "derrickRuin", "tower", "arch", "relayDish", "crateStack", "crate"}
LOOSE = {"plant", "bush", "deadTree", "debris"}
# Islands of rock in these (W4b: "an island of high rock in some").
ISLANDS = {"rust-hollow", "brine-pans", "dust-terraces"}


def noise(rng, xs, zs, spacing):
    """Smooth value noise in [-1, 1] on the grid of xs × zs."""
    nx = int((xs[-1] - xs[0]) / spacing) + 3
    nz = int((zs[-1] - zs[0]) / spacing) + 3
    lat = rng.uniform(-1, 1, size=(nz, nx))
    u = (xs - xs[0]) / spacing
    v = (zs - zs[0]) / spacing
    i, j = np.floor(u).astype(int), np.floor(v).astype(int)
    fu, fv = u - i, v - j
    su, sv = fu * fu * (3 - 2 * fu), fv * fv * (3 - 2 * fv)
    a = lat[np.ix_(j, i)] * (1 - su)[None, :] + lat[np.ix_(j, i + 1)] * su[None, :]
    b = lat[np.ix_(j + 1, i)] * (1 - su)[None, :] + lat[np.ix_(j + 1, i + 1)] * su[None, :]
    return a * (1 - sv)[:, None] + b * sv[:, None]


def capsule(X, Z, a, b):
    """Distance to segment ab and where along it (0..1)."""
    ax, az = a
    bx, bz = b
    ex, ez = bx - ax, bz - az
    t = np.clip(((X - ax) * ex + (Z - az) * ez) / max(ex * ex + ez * ez, 1e-9), 0, 1)
    return np.hypot(X - ax - ex * t, Z - az - ez * t), t


def inward(edge):
    return {"north": (0, 1), "south": (0, -1), "west": (1, 0), "east": (-1, 0)}[edge]


def edge_point(edge, along, depth):
    return {"north": (along, depth), "south": (along, D - depth), "west": (depth, along), "east": (W - depth, along)}[edge]


def keeps(f):
    """(point, radius) the land must hold."""
    out = []
    out += [(p["at"], KEEP["pad"]) for p in f.get("pads", [])]
    out += [(p["at"], KEEP["spawn"]) for p in f.get("spawns", [])]
    out += [(s["at"], KEEP["site"]) for s in f.get("sites", [])]
    out += [(w["at"], KEEP["well"]) for w in f.get("wells", [])]
    if "nest" in f:
        out.append((f["nest"]["at"], KEEP["nest"]))
    out += [(d["at"], KEEP["landmark"]) for d in f.get("doodads", []) if d["kind"] in LANDMARKS]
    for h in f.get("height", []):
        if h.get("op") == "ramp":
            # The whole ramp, and its landing past the top.
            (ax, az), (bx, bz) = h["from"], h["to"]
            l = math.dist((ax, az), (bx, bz)) or 1
            for k in range(int(l / 2) + 5):
                t = k * 2 / l
                out.append(((ax + (bx - ax) * t, az + (bz - az) * t), h.get("width", 4) / 2 + KEEP["ramp"]))
    return out


def steep(f):
    """Where a deposit must not go: the height edits' cliffs and ramps."""
    shapes = []
    for h in f.get("height", []):
        if h.get("op") in ("ridge",):
            shapes.append(LineString(h["line"]).buffer(h.get("width", 3) / 2 + 2.5))
        elif h.get("op") == "ramp":
            shapes.append(LineString([h["from"], h["to"]]).buffer(h.get("width", 4) / 2 + 2.5))
        elif "area" in h:
            a = h["area"]
            poly = Point(a["circle"][0], a["circle"][1]).buffer(a["circle"][2]) if isinstance(a, dict) else Polygon(a)
            shapes.append(poly.exterior.buffer(2.5 + h.get("slope", 0)))
    return unary_union(shapes) if shapes else Polygon()


def shape(rid, f, seed):
    rng = random.Random(f"{rid}|w4b|{seed}")
    nrng = np.random.default_rng(rng.randrange(1 << 30))
    xs = np.arange(-PAD, W + PAD + 1e-9, STEP)
    zs = np.arange(-PAD, D + PAD + 1e-9, STEP)
    X0, Z0 = np.meshgrid(xs, zs)
    # The shapes drawn on a warped sheet: no side of a bay or a spur runs
    # straight. Passages and what is kept are drawn on the true one.
    X = X0 + noise(nrng, xs, zs, 13) * 4.5
    Z = Z0 + noise(nrng, xs, zs, 13) * 4.5
    de = np.minimum(np.minimum(X, W - X), np.minimum(Z, D - Z))
    de0 = np.minimum(np.minimum(X0, W - X0), np.minimum(Z0, D - Z0))
    opens = [(e, o["span"][0], o["span"][1]) for e in EDGES for o in f.get("openings", {}).get(e, [])]

    # The cell inset, meandering: wide swings, then bends, then a rough edge.
    inset = rng.uniform(17, 21)
    n = (noise(nrng, xs, zs, 40) * 1.4 + noise(nrng, xs, zs, 17) * 0.8 + noise(nrng, xs, zs, 9) * 0.55
         + noise(nrng, xs, zs, 4) * 0.15)
    S = np.clip((de - inset) / 8, -3, 1.6) + n
    # The corners: rock, mostly.
    for cx, cz in ((0, 0), (W, 0), (W, D), (0, D)):
        dc = np.hypot(X - cx, Z - cz)
        S -= rng.choice([0.4, 1.0, 1.4, 1.8]) * np.clip(1 - dc / 46, 0, 1) * 2

    def clear_of_passages(edge, along, room):
        return all(not (e == edge and lo - room < along < hi + room) for e, lo, hi in opens)

    def place_on_edge(room):
        for _ in range(200):
            edge = rng.choice(EDGES)
            length = W if edge in ("north", "south") else D
            along = rng.uniform(0.15 * length, 0.85 * length)
            if clear_of_passages(edge, along, room):
                return edge, along, length
        return None

    # Bays: 2–4, one deep; sometimes two facing each other (a neck).
    cuts = []
    for k in range(rng.randint(2, 4)):
        got = place_on_edge(16)
        if not got:
            continue
        edge, along, _ = got
        across = D if edge in ("north", "south") else W
        frac = rng.uniform(0.22, 0.32) if k == 0 else rng.uniform(0.12, 0.24)
        cuts.append((edge, along, frac * across, rng.uniform(9, 16), 3.2))
    if rng.random() < 0.6:
        edge = rng.choice(["north", "west"])
        length = W if edge == "north" else D
        across = D if edge == "north" else W
        for _ in range(60):
            along = rng.uniform(0.25 * length, 0.75 * length)
            opp = {"north": "south", "west": "east"}[edge]
            if clear_of_passages(edge, along, 16) and clear_of_passages(opp, along, 16):
                cuts.append((edge, along, across * rng.uniform(0.2, 0.28), rng.uniform(8, 12), 3.0))
                cuts.append((opp, along + rng.uniform(-6, 6), across * rng.uniform(0.2, 0.28), rng.uniform(8, 12), 3.0))
                break
    # Rock spurs: narrow and long, 1–2.
    for _ in range(rng.randint(1, 2)):
        got = place_on_edge(12)
        if not got:
            continue
        edge, along, _ = got
        across = D if edge in ("north", "south") else W
        cuts.append((edge, along, across * rng.uniform(0.25, 0.36), rng.uniform(3.5, 5), 2.6))
    for edge, along, depth, half, strength in cuts:
        a = edge_point(edge, along, -2)
        # The far end leans a little, so a bay is never a plain slot.
        ix, iz = inward(edge)
        lean = rng.uniform(-0.35, 0.35) * depth
        b = edge_point(edge, along, depth)
        b = (b[0] + lean * abs(iz), b[1] + lean * abs(ix))
        d, t = capsule(X, Z, a, b)
        width = half * (1.0 - 0.35 * t) + noise(nrng, xs, zs, 9) * half * 0.25
        S -= strength * np.clip(1 - d / np.maximum(width, 1.0), 0, 1) * 1.6
    # An island of high rock.
    island_at = None
    if rid in ISLANDS:
        ks = keeps(f)
        for _ in range(400):
            p = (rng.uniform(30, W - 30), rng.uniform(26, D - 26))
            if all(math.dist(p, q) > r + 9 for q, r in ks):
                island_at = p
                break
        if island_at:
            r = rng.uniform(5, 7.5)
            d = np.hypot(X - island_at[0], Z - island_at[1]) + noise(nrng, xs, zs, 5) * r * 0.35
            S = np.minimum(S, (d - r) / 3)

    # Off the edges but at the passages: rock (a soft limit, so the land
    # seldom runs along it).
    S = np.minimum(S, (de0 - 8) / 1.5)
    # What must stay reachable keeps land round it, up to the edge's band.
    # Its rim wanders (never a ring of circles).
    wander = noise(nrng, xs, zs, 8) * 3.5
    reach = (de0 - 7 + noise(nrng, xs, zs, 5) * 2.2) / 1.5
    for (px, pz), r in keeps(f):
        S = np.maximum(S, np.minimum((r + wander - np.hypot(X0 - px, Z0 - pz)) / 3, reach))
    # Each passage: a valley narrowing into the gap, out past the edge.
    pad = f["pads"][0]["at"] if f.get("pads") else (W / 2, D / 2)
    for edge, lo, hi in opens:
        mid, half = (lo + hi) / 2, (hi - lo) / 2
        p0 = edge_point(edge, mid, -PAD)
        p1 = edge_point(edge, mid, 6)
        ix, iz = inward(edge)
        bend = rng.uniform(-8, 8)
        p2 = (p1[0] + (pad[0] - p1[0]) * 0.45 + bend * abs(iz), p1[1] + (pad[1] - p1[1]) * 0.45 + bend * abs(ix))
        d01, _ = capsule(X0, Z0, p0, p1)
        d12, t12 = capsule(X0, Z0, p1, p2)
        depth = np.where(d01 <= d12, np.clip(de0, -PAD, 6), 6 + t12 * math.dist(p1, p2))
        dist = np.minimum(d01, d12)
        # A canyon mouth at the gap, opening into a valley: its sides wander.
        width = (half + 2.6 + np.clip((depth - 5) / 20, 0, 1) * 9
                 + noise(nrng, xs, zs, 6) * np.clip((depth - 4) / 6, 0, 1) * 3.5)
        S = np.maximum(S, (width - dist) / 3)
    S = np.where(de0 < -PAD + 1, np.minimum(S, -1), S)
    return xs, zs, S, island_at


def ground(rid, orig):
    """The bare ground, a cell at a time (no walls): walkable or not."""
    tmp = ROOT / f"unreal/server/build/w4b-shape-{rid}.json"
    # No walls, and none of what the shapes move or keep land round (each
    # stands in a unit's way itself).
    bare = {k: v for k, v in orig.items() if k not in ("border", "deposits", "wells")}
    tmp.write_text(json.dumps(bare))
    rows = run([TOOL, "levels", tmp, "--bare"]).stdout.split()
    tmp.unlink()
    return np.array([[c.isdigit() for c in row] for row in rows])


def walkable(xs, zs, cells):
    """The cells' walkable ground on the field's samples (past the edges:
    the passages' own, so walkable)."""
    i = np.clip(np.floor(xs).astype(int), 0, cells.shape[1] - 1)
    j = np.clip(np.floor(zs).astype(int), 0, cells.shape[0] - 1)
    w = cells[np.ix_(j, i)]
    out = (xs < 0)[None, :] | (xs >= W)[None, :] | (zs < 0)[:, None] | (zs >= D)[:, None]
    return w | out


def joined(S, walk, at, clear=1):
    """The walkable land joined to the point `at` (sample index) through
    gaps wider than twice `clear` (half-cell samples)."""
    open_ = ndimage.binary_erosion((S > 0) & walk, structure=np.ones((3, 3)), iterations=clear)
    lab, _ = ndimage.label(open_, structure=np.ones((3, 3)))
    k = lab[at]
    if k == 0:
        near = ndimage.distance_transform_edt(lab == 0, return_indices=True)[1]
        k = lab[near[0][at], near[1][at]]
    return lab == k


def join(f, xs, zs, S, walk):
    """Carve a valley from anything kept that the land leaves cut off (on
    walkable ground) to the pad's land, by the walkable way round."""
    X0, Z0 = np.meshgrid(xs, zs)
    de0 = np.minimum(np.minimum(X0, W - X0), np.minimum(Z0, D - Z0))
    pad = f["pads"][0]["at"]
    targets = [p for p, _ in keeps(f)]
    # A vault is meant to be reached by a Cliff hop alone.
    vaults = [tuple(t["at"]) for t in f.get("sites", []) if t.get("kind") == "vault"]
    targets = [p for p in targets if tuple(p) not in vaults]
    index = lambda p: (int(round((p[1] + PAD) / STEP)), int(round((p[0] + PAD) / STEP)))
    # What is kept stands in the way itself: its own spot counts as ground.
    X0_, Z0_ = X0, Z0
    for p in targets:
        walk = walk | (np.hypot(X0_ - p[0], Z0_ - p[1]) < 3.5)
    for _ in range(8):
        main = joined(S, walk, index(pad))
        reach = ndimage.binary_dilation(main, structure=np.ones((3, 3)), iterations=4)
        cut = [p for p in targets if walk[index(p)] and not reach[index(p)]]
        if not cut:
            return S
        # The wide way round: dear by a cliff or a rock.
        clear = ndimage.distance_transform_edt(walk) * STEP
        costs = np.where(walk, np.where(S > 0, 1.0, 4.0) + 30.0 * (clear < 2.5), np.inf)
        costs = np.where(de0 < 6, np.where(S > 0, costs, np.inf), costs)
        costs = np.where(np.isfinite(costs), costs, 1e9)
        mcp = MCP_Geometric(costs)
        mcp.find_costs(list(zip(*np.nonzero(main))))
        path = np.zeros_like(main)
        for p in cut:
            for r, c in mcp.traceback(index(p)):
                path[r, c] = True
        dist = ndimage.distance_transform_edt(~path) * STEP
        S = np.maximum(S, (4.5 + noise(np.random.default_rng(len(cut)), xs, zs, 7) * 1.5 - dist) / 3)
    return S


def land_of(xs, zs, S, pad):
    """The outline and islands round the pad's land (region cells)."""
    land = S > 0
    lab, _ = ndimage.label(land)
    pj, pi = int((pad[1] + PAD) / STEP), int((pad[0] + PAD) / STEP)
    main = lab == lab[pj, pi]
    # Holes: rock wholly inside the land. Small ones, or ones close to the
    # outline (a slot too narrow to walk), become land.
    holes, nh = ndimage.label(~main)
    border_labels = set(np.unique(np.concatenate([holes[0], holes[-1], holes[:, 0], holes[:, -1]])))
    outer = np.isin(holes, list(border_labels))
    gap = ndimage.distance_transform_edt(~outer) * STEP
    F = np.where(main, S, np.minimum(S, -0.5))
    for k in range(1, nh + 1):
        if k in border_labels:
            continue
        m = holes == k
        if m.sum() * STEP * STEP < 45 or gap[m].min() < 8:
            F = np.where(m, np.maximum(F, 0.5), F)
    contours = measure.find_contours(F, 0.0)
    loops = []
    for c in contours:
        pts = [(float(xs[0] + col * STEP), float(zs[0] + row * STEP)) for row, col in c]
        if len(pts) > 8 and math.dist(pts[0], pts[-1]) < 1e-6:
            loops.append(pts[:-1])
    loops.sort(key=lambda l: -Polygon(l).area)
    return loops[0], loops[1:]


def simplify(pts, tol):
    poly = Polygon(pts).simplify(tol, preserve_topology=True)
    return [list(p) for p in poly.exterior.coords[:-1]]


def clamp_outline(pts):
    out = []
    for x, z in pts:
        q = [round(min(max(x, 0.0), W), 1), round(min(max(z, 0.0), D), 1)]
        if not out or q != out[-1]:
            out.append(q)
    if out[0] == out[-1]:
        out.pop()
    return out


def longest_run(outline, show=False):
    """The longest straight stretch of wall (not along the cell's edges)."""
    pts = simplify(outline, 1.0)
    best = 0.0
    if show:
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            if math.dist(a, b) > 12:
                print(f"    straight: {a} → {b}, {math.dist(a, b):.1f} cells")
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        on_edge = (a[0] in (0, W) and a[0] == b[0]) or (a[1] in (0, D) and a[1] == b[1])
        if not on_edge:
            best = max(best, math.dist(a, b))
    return best


def settle(f, orig, outline, islands, bad, reach):
    """Deposits onto the land near where they were (clear of the spots the
    validator found wrong), plants and debris off the rock."""
    land = Polygon(outline, holes=islands)
    inner = land.buffer(-4.5)
    others = [(w["at"], 4.0) for w in f.get("wells", [])] + [(s_["at"], 7.0) for s_ in f.get("sites", [])]
    others += [(p["at"], 8.0) for p in f.get("pads", [])] + [(d["at"], 5.0) for d in f.get("doodads", []) if d["kind"] in LANDMARKS]
    if "nest" in f:
        others.append((f["nest"]["at"], 10.0))
    cliffs = steep(orig)
    src = orig.get("deposits", [])
    # Veins: deposits within 5 cells of each other, chained.
    vein = list(range(len(src)))
    def root(i):
        while vein[i] != i:
            i = vein[i]
        return i
    for i in range(len(src)):
        for j in range(i):
            if math.dist(src[i]["at"], src[j]["at"]) < 5:
                vein[root(i)] = root(j)
    veins = {}
    for i in range(len(src)):
        veins.setdefault(root(i), []).append(i)
    placed = {}

    def ok(i, q, mine):
        return (inner.contains(Point(q)) and reach(q) and not cliffs.contains(Point(q))
                and all(math.dist(q, o) > 2.6 for j, o in placed.items() if j not in mine)
                and all(math.dist(q, o) > r for o, r in others) and all(math.dist(q, b) > 2.5 for b in bad.get(i, [])))

    def stays(i, q):
        # Where it was, on the land and joined: it stays (the validator had
        # it right there before).
        return (inner.contains(Point(q)) and reach(q) and all(math.dist(q, b) > 2.5 for b in bad.get(i, []))
                and all(math.dist(q, o) > 2.6 for o in placed.values()))

    moved = 0
    for members in veins.values():
        at = {i: tuple(src[i]["at"]) for i in members}
        if all(stays(i, at[i]) for i in members):
            placed.update(at)
            continue
        # The whole vein moved by one step in onto the land, its shape kept;
        # failing that, each deposit on its own.
        shift = None
        # Mostly on the land still: only the strays move.
        strays = sum(not stays(i, at[i]) for i in members)
        for r in (np.arange(1.0, 30.0, 1.0) if strays * 2 > len(members) else []):
            for k in range(int(6 * r)):
                a_ = 2 * math.pi * k / int(6 * r)
                dx, dz = r * math.cos(a_), r * math.sin(a_)
                if all(ok(i, (at[i][0] + dx, at[i][1] + dz), members) for i in members):
                    shift = (dx, dz)
                    break
            if shift:
                break
        for i in members:
            q = at[i]
            if shift:
                q = (round(q[0] + shift[0], 1), round(q[1] + shift[1], 1))
            elif not stays(i, q):
                for r in np.arange(1.0, 40.0, 1.0):
                    hit = None
                    for k in range(int(6 * r)):
                        a_ = 2 * math.pi * k / int(6 * r)
                        c = (round(q[0] + r * math.cos(a_), 1), round(q[1] + r * math.sin(a_), 1))
                        if ok(i, c, ()):
                            hit = c
                            break
                    if hit:
                        q = hit
                        break
            if q != at[i]:
                moved += 1
            placed[i] = q
    deposits = []
    for i, d0 in enumerate(src):
        d = dict(d0)
        if tuple(d["at"]) != placed[i]:
            d["at"] = [placed[i][0], placed[i][1]]
        deposits.append(d)
    f["deposits"] = deposits
    keep, dropped = [], 0
    for d in orig.get("doodads", []):
        if d["kind"] in LOOSE and not land.buffer(-1).contains(Point(d["at"])):
            dropped += 1
            continue
        keep.append(d)
    f["doodads"] = keep
    return moved, dropped


def run(cmd):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True)


PROBLEM = re.compile(r": (\w+)\[(\d+)\][^:]*? at \(([-\d.]+), ([-\d.]+)\)")


def attempt(rid, orig, seed, path, cells):
    """One shape, its deposits settled against the validator; its problems."""
    f = json.loads(json.dumps(orig))
    xs, zs, S, _ = shape(rid, f, seed)
    walk = walkable(xs, zs, cells)
    S = join(f, xs, zs, S, walk)
    for p, _ in keeps(f):
        walk = walk | (np.hypot(*np.meshgrid(xs - p[0], zs - p[1])) < 3.5)
    outline, islands = land_of(xs, zs, S, f["pads"][0]["at"])
    # Deposits go only where the pad's land walks to.
    at = lambda p: (int(round((p[1] + PAD) / STEP)), int(round((p[0] + PAD) / STEP)))
    main = joined(S, walk, at(f["pads"][0]["at"]))
    reach = lambda q: bool(main[at(q)])
    outline = clamp_outline(simplify(outline, 0.3))
    islands = [[[round(x, 1), round(z, 1)] for x, z in simplify(i, 0.3)] for i in islands]
    f["border"] = {"outline": outline, **({"islands": islands} if islands else {}), "level": orig.get("border", {}).get("level", 4)}
    bad = {}
    for _ in range(16):
        moved, dropped = settle(f, orig, outline, islands, bad, reach)
        path.write_text(json.dumps(f, indent=2) + "\n")
        run([TOOL, "format", path])
        lines = [l for l in run([TOOL, "validate", path]).stdout.splitlines()
                 if ": " in l and not l.endswith(": ok") and "(vault) is reached" not in l]
        rest = []
        for l in lines:
            m = PROBLEM.search(l)
            if m and m.group(1) == "deposits":
                bad.setdefault(int(m.group(2)), []).append((float(m.group(3)), float(m.group(4))))
                # "on top of deposits[j]": move that one too.
                for j in re.findall(r"on top of deposits\[(\d+)\] at \(([-\d.]+), ([-\d.]+)\)", l):
                    bad.setdefault(int(j[0]), []).append((float(j[1]), float(j[2])))
            else:
                rest.append(l)
        if not lines:
            break
        if rest:
            break
    stats = dict(points=len(outline), islands=len(islands), land=Polygon(outline, holes=islands).area / (W * D),
                 run=longest_run(outline, "--show" in sys.argv), moved=moved, dropped=dropped)
    return lines, stats


def main():
    """Each region: the seed given, or of the first `--tries` (40) the one
    that validates with the shortest straight run of wall."""
    seeds, tries = {}, 40
    argv = sys.argv[1:]
    for i, a in enumerate(argv):
        if a == "--seed":
            rid, n = argv[i + 1].split("=")
            seeds[rid] = int(n)
        if a == "--tries":
            tries = int(argv[i + 1])
    ids = [a for a in argv if not a.startswith("--") and "=" not in a and not a.isdigit()] or [r for row in GRID for r in row]
    for rid in ids:
        path = REG / f"{rid}.json"
        text = path.read_text()
        orig = json.loads(text)
        cells = ground(rid, orig)
        best = None
        for seed in ([seeds[rid]] if rid in seeds else range(tries)):
            problems, stats = attempt(rid, orig, seed, path, cells)
            # An island region's shape keeps its island when one can.
            score = stats["run"] + (40 if rid in ISLANDS and not stats["islands"] else 0)
            if (not problems or rid in seeds) and (best is None or score < best[2]):
                best = (seed, stats, score)
            if "--verbose" in argv:
                print(f"  {rid} seed {seed}: {len(problems)} problems, {stats['islands']} islands, longest straight wall {stats['run']:.1f}"
                      + ("" if not problems else "  | " + " | ".join(p.split(": ", 1)[-1] for p in problems[:3])))
        if best is None:
            path.write_text(text)
            print(f"{rid}: no seed validates; left as it was")
            continue
        problems, stats = attempt(rid, orig, best[0], path, cells)
        print(f"{rid} seed {best[0]}: {len(problems)} problems, {stats['points']} points, {stats['islands']} islands, "
              f"land {stats['land']:.0%} of the cell, longest straight wall {stats['run']:.1f} cells, "
              f"{stats['moved']} deposits moved in, {stats['dropped']} loose doodads off the rock")


if __name__ == "__main__":
    main()
