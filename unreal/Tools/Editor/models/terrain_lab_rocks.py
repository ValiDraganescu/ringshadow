# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "pillow"]
# ///
"""The terrain lab's rock kit (docs/terrain-lab.md, rounds 3 and 4): cliff-face slabs, talus, spire crowns and the
ground clutter, as plain static meshes for the lab's scenery actor to instance.

    uv run unreal/Tools/hidden_run.py --out <dir> --python unreal/Tools/Editor/models/terrain_lab_rocks.py
    uv run unreal/Tools/Editor/models/terrain_lab_rocks.py --preview art/terrain-lab/rocks.jpg

In the editor it writes /Game/TerrainLab/Rocks/SM_<name> (one material slot, `rock`, or `tuft` for the grass; a
plain engine material in it, the lab sets its own at runtime), no Nanite, no collision, and touches no other asset
and not the model catalog (lab rule 2). Outside the editor it renders the same meshes to a contact sheet (plain
Python, numpy and Pillow; back faces red, so an inverted face or a hole shows).

UE cm, Z up, pivot at the base. A slab's +X points out of the cliff, Y runs along it, its foot's middle at the
origin and its back at x = 0. Everything is seeded per name: a rerun gives the same meshes.

Facts (2026-10-19): the kit's lofts are convex-only (they flip a triangle by the centroid), so the shapes here are
triangulated by hand, wound outward and flipped once on the way out to the kit's rule ((b - a) x (c - a) points
into the solid, the normal out). Normals are smoothed between faces closer than a crease angle: faceted rock, not
flat-shaded noise.
"""
import math
import os
import random
import sys

try:
    import unreal
except ImportError:
    unreal = None

FOLDER = "/Game/TerrainLab/Rocks"
MATERIALS = ("/Engine/BasicShapes/BasicShapeMaterial", "/Engine/EngineMaterials/DefaultMaterial")


# --- small maths -------------------------------------------------------------------------------------------------

def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    n = math.sqrt(dot(a, a))
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-12 else (0.0, 0.0, 1.0)


def rng_for(name):
    return random.Random("ringshadow-terrain-lab-rocks:" + name)


def noise1(rng, knots, smooth=True):
    """A 1D noise on [0, 1] in [-1, 1]: `knots` random values, eased (smooth) or straight (jagged) between them."""
    v = [rng.uniform(-1.0, 1.0) for _ in range(knots + 1)]

    def f(t):
        t = min(max(t, 0.0), 1.0) * knots
        i = min(int(t), knots - 1)
        u = t - i
        if smooth:
            u = u * u * (3 - 2 * u)
        return v[i] * (1 - u) + v[i + 1] * u
    return f


def noise_loop(rng, knots):
    """Like noise1 but closed: f(0) == f(1), for rings."""
    v = [rng.uniform(-1.0, 1.0) for _ in range(knots)]

    def f(t):
        t = (t % 1.0) * knots
        i = int(t) % knots
        u = t - int(t)
        u = u * u * (3 - 2 * u)
        return v[i] * (1 - u) + v[(i + 1) % knots] * u
    return f


def ear_clip(ring):
    """Triangles (index triples, counter-clockwise from above) of a ring's outline seen from above."""
    idx = list(range(len(ring)))
    xy = [(p[0], p[1]) for p in ring]

    def cr(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    out = []
    while len(idx) > 3:
        m = len(idx)
        best = None
        for k in range(m):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % m]
            a, b, c = xy[i0], xy[i1], xy[i2]
            if cr(a, b, c) <= 1e-9:
                continue
            if any(cr(a, b, xy[j]) >= 0 and cr(b, c, xy[j]) >= 0 and cr(c, a, xy[j]) >= 0
                   for j in idx if j not in (i0, i1, i2)):
                continue
            best = k
            break
        if best is None:  # a degenerate outline: take the fattest corner
            best = max(range(m), key=lambda k: cr(xy[idx[k - 1]], xy[idx[k]], xy[idx[(k + 1) % m]]))
        out.append((idx[best - 1], idx[best], idx[(best + 1) % m]))
        idx.pop(best)
    out.append(tuple(idx))
    return out


class Mesh:
    """Triangles wound outward (math cross product out of the solid), plus the crease angle for the normals."""

    def __init__(self, name, slot="rock", crease=32.0):
        self.name, self.slot, self.crease = name, slot, crease
        self.tris = []

    def tri(self, a, b, c):
        n = cross(sub(b, a), sub(c, a))
        if dot(n, n) > 1e-12:
            self.tris.append((tuple(a), tuple(b), tuple(c)))

    def quad(self, a, b, c, d):
        self.tri(a, b, c)
        self.tri(a, c, d)

    def loft(self, rings, cap_bottom=True, cap_top=True):
        """Rings of equal size, each counter-clockwise seen from above, bottom to top; the caps are the end rings
        ear-clipped in plan (a fan from the centre flips on a long thin outline: the slabs' first tops came out
        back-facing). A ring wound the other way is turned round first."""
        n = len(rings[0])
        r = rings[0]
        if sum(r[k][0] * r[(k + 1) % n][1] - r[(k + 1) % n][0] * r[k][1] for k in range(n)) < 0:
            rings = [list(reversed(x)) for x in rings]
        for r0, r1 in zip(rings, rings[1:]):
            for k in range(n):
                self.quad(r0[k], r0[(k + 1) % n], r1[(k + 1) % n], r1[k])
        if cap_bottom:
            r = rings[0]
            for a, b, c in ear_clip(r):
                self.tri(r[a], r[c], r[b])
        if cap_top:
            r = rings[-1]
            for a, b, c in ear_clip(r):
                self.tri(r[a], r[b], r[c])

    def shift(self, d):
        self.tris = [tuple(add(p, d) for p in t) for t in self.tris]

    def bounds(self):
        pts = [p for t in self.tris for p in t]
        return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]

    def normals(self):
        """Per corner: the area-weighted mean of the faces at that point within the crease angle of this face."""
        fn = [cross(sub(b, a), sub(c, a)) for a, b, c in self.tris]
        unit = [norm(n) for n in fn]
        at = {}
        key = lambda p: (round(p[0], 2), round(p[1], 2), round(p[2], 2))  # noqa: E731
        for i, t in enumerate(self.tris):
            for p in t:
                at.setdefault(key(p), []).append(i)
        cos_c = math.cos(math.radians(self.crease))
        out = []
        for i, t in enumerate(self.tris):
            corner = []
            for p in t:
                s = (0.0, 0.0, 0.0)
                for j in at[key(p)]:
                    if dot(unit[j], unit[i]) >= cos_c:
                        s = add(s, fn[j])
                corner.append(norm(s))
            out.append(corner)
        return out


# --- icospheres (talus, pebbles, clinker) ----------------------------------------------------------------------

def icosphere(subdiv):
    t = (1 + math.sqrt(5)) / 2
    v = [norm(p) for p in [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t),
                           (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6),
         (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10),
         (8, 6, 7), (9, 8, 1)]
    for _ in range(subdiv):
        mid = {}

        def m(a, b):
            k = (min(a, b), max(a, b))
            if k not in mid:
                mid[k] = len(v)
                v.append(norm(mul(add(v[a], v[b]), 0.5)))
            return mid[k]
        nf = []
        for a, b, c in f:
            ab, bc, ca = m(a, b), m(b, c), m(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        f = nf
    return v, f


def stone(name, rng, size, subdiv, rough, cuts, squash, sink, crease, pits=0.0):
    """A boulder: an icosphere scaled to `size` (x, y, z full extents), its points pushed in and out by `rough`,
    `cuts` fracture planes that flatten whatever lies past them, `pits` of the points sunk deep (clinker)."""
    v, f = icosphere(subdiv)
    pts = []
    for p in v:
        r = 1.0 + rng.uniform(-rough, rough * 0.6)
        if pits and rng.random() < pits:
            r *= rng.uniform(0.55, 0.75)
        pts.append(mul(p, r))
    for _ in range(cuts):
        n = norm((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.6, 1)))
        d = rng.uniform(0.55, 0.8)
        pts = [sub(p, mul(n, dot(p, n) - d)) if dot(p, n) > d else p for p in pts]
    pts = [(p[0] * size[0] / 2, p[1] * size[1] / 2, p[2] * size[2] / 2 * squash) for p in pts]
    yaw = rng.uniform(0, 2 * math.pi)
    cy, sy = math.cos(yaw), math.sin(yaw)
    pts = [(p[0] * cy - p[1] * sy, p[0] * sy + p[1] * cy, p[2]) for p in pts]
    me = Mesh(name, crease=crease)
    for a, b, c in f:
        # icosphere faces are wound outward already (checked by the preview: no red).
        me.tri(pts[a], pts[b], pts[c])
    lo, hi = me.bounds()
    me.shift((0, 0, -lo[2] - sink * (hi[2] - lo[2])))
    return me


# --- 1. cliff-face slabs --------------------------------------------------------------------------------------

def slab(i):
    name = "slab_%d" % i
    rng = rng_for(name)
    L = rng.uniform(200, 400)
    H = rng.uniform(150, 320)
    D = rng.uniform(40, 90)
    nl = rng.choice((3, 4, 4, 5, 5))
    # Layer thicknesses: weights cubed so one slab has a fat bed and two thin ones, the next three even ones.
    while True:
        w = [rng.uniform(0.12, 1.0) ** rng.uniform(1.0, 2.6) for _ in range(nl)]
        th = [H * x / sum(w) for x in w]
        if min(th) >= 18:
            break
    # Bed boundaries: each dips along the run a little and wavers, never crossing the next.
    dip = rng.uniform(-0.07, 0.07)
    bounds = [lambda y: 0.0]
    z = 0.0
    for j in range(nl - 1):
        z += th[j]
        own_dip = dip + rng.uniform(-0.03, 0.03)
        wav = noise1(rng, rng.randint(2, 4))
        amp = min(th[j], th[j + 1]) * 0.18
        bounds.append(lambda y, z=z, d=own_dip, wav=wav, amp=amp: z + d * y + amp * wav(y / L + 0.5))
    top_n = noise1(rng, rng.randint(5, 9), smooth=False)
    # The broken top bites at most half the top bed (deeper, the bed turned inside out: slab_5's first top).
    top_amp = min(H * rng.uniform(0.08, 0.2), th[-1] * 0.5)
    bounds.append(lambda y: H - top_amp * (0.5 + 0.5 * top_n(y / L + 0.5)))

    me = Mesh(name, crease=34.0)
    KF = 11  # points along the front of a bed
    for j in range(nl):
        zb, zt = bounds[j], bounds[j + 1]
        Dj = D * rng.uniform(0.7, 1.0)
        Lj = L * rng.uniform(0.86, 1.0)
        sy = rng.uniform(-0.5, 0.5) * (L - Lj)
        front = noise1(rng, rng.randint(3, 6))
        y0, y1 = sy - Lj / 2, sy + Lj / 2
        e0, e1 = rng.uniform(8, 30), rng.uniform(8, 30)
        # The bed's outline seen from above, counter-clockwise: back-right corner, the front from +Y to -Y, back-left.
        ys = [y1 - e1 - (Lj - e0 - e1) * k / (KF - 1) for k in range(KF)]
        ys = [ys[0]] + [y + rng.uniform(-0.3, 0.3) * (Lj - e0 - e1) / (KF - 1) for y in ys[1:-1]] + [ys[-1]]
        outline = [(0.0, y1)]
        for k, y in enumerate(ys):
            x = Dj * (1.0 + 0.14 * front(k / (KF - 1)))
            if k in (0, KF - 1):
                x *= rng.uniform(0.55, 0.85)  # the ends turn back toward the cliff
            outline.append((x, y))
        outline.append((0.0, y0))
        n = len(outline)
        rings = [[], [], [], []]
        for k, (x, y) in enumerate(outline):
            back = k in (0, n - 1)
            bb, bt = rng.uniform(3, 10), rng.uniform(3, 14)
            chip = (not back) and rng.random() < 0.18
            if chip:
                bt = rng.uniform(14, min(28, x * 0.45))
            zb_, zt_ = zb(y) - (2.0 if j else 0.0), zt(y)
            bb, bt = min(bb, (zt_ - zb_) * 0.3), min(bt, (zt_ - zb_) * 0.4)
            if back:
                yin_b = y - math.copysign(bb, y - sy)
                yin_t = y - math.copysign(bt, y - sy)
                rings[0].append((0.0, yin_b, zb_))
                rings[1].append((0.0, y, zb_ + bb))
                rings[2].append((0.0, y, zt_ - bt))
                rings[3].append((0.0, yin_t, zt_))
            else:
                rings[0].append((max(x - bb, 1.0), y, zb_))
                rings[1].append((x, y, zb_ + bb))
                rings[2].append((x - (bt * 0.4 if chip else 0.0), y, zt_ - bt))
                rings[3].append((max(x - bt, 1.0), y, zt_ - (bt * 0.3 if chip else 0.0)))
        me.loft(rings)
    return me


# --- 3. spire crowns ------------------------------------------------------------------------------------------

def crown_bed(me, rng, cx, cy, r, zb, zt, n, jag, top_peaks=None, bottom=True, tilt=(0.0, 0.0)):
    """One bed of a crown: an angular ring outline (radius noise and jitter), bevelled top and bottom; the top may
    be broken (`top_peaks`) and slope (`tilt`, cm per cm along x and y)."""
    rn = noise_loop(rng, rng.randint(4, 7))
    sq = rng.uniform(0.75, 1.0)
    rings = [[], [], [], []]
    a0 = rng.uniform(0, 2 * math.pi)
    for k in range(n):
        a = a0 + 2 * math.pi * k / n + rng.uniform(-0.25, 0.25) * 2 * math.pi / n
        rr = r * (1.0 + jag * rn(k / n) + rng.uniform(-jag * 0.6, jag * 0.4))
        ca, sa = math.cos(a), math.sin(a) * sq
        bb, bt = rng.uniform(4, 12), rng.uniform(5, 18)
        zt_ = zt + (top_peaks(k / n) if top_peaks else rng.uniform(-4, 4)) + tilt[0] * ca * rr + tilt[1] * sa * rr
        bb, bt = min(bb, (zt - zb) * 0.25), min(bt, (zt - zb) * 0.3)
        zt_ = max(zt_, zb + bb + bt + 6.0)  # a deep bite or slope never turns the bed inside out
        p = lambda rad, z: (cx + ca * rad, cy + sa * rad, z)  # noqa: E731
        rings[0].append(p(rr - bb, zb))
        rings[1].append(p(rr, zb + bb))
        rings[2].append(p(rr, zt_ - bt))
        rings[3].append(p(max(rr - bt, rr * 0.5), zt_))
    me.loft(rings, cap_bottom=bottom)


def spire(i):
    name = "spire_%d" % i
    rng = rng_for(name)
    W = rng.uniform(200, 500)
    H = rng.uniform(150, 400)
    nl = rng.choice((3, 4, 5))
    while True:
        w = [rng.uniform(0.15, 1.0) ** rng.uniform(1.0, 2.4) for _ in range(nl)]
        th = [H * 0.78 * x / sum(w) for x in w]
        if min(th) >= 20:
            break
    me = Mesh(name, crease=36.0)
    z = 0.0
    cx = cy = 0.0
    r = W / 2
    ta = rng.uniform(0, 2 * math.pi)
    slope = rng.uniform(0.05, 0.2)
    for j in range(nl):
        top = j == nl - 1
        peaks = None
        if top:
            pk = noise_loop(rng, rng.randint(3, 5))
            amp = H * 0.22
            peaks = lambda t, pk=pk, amp=amp: amp * max(pk(t), -0.4)  # noqa: E731
        crown_bed(me, rng, cx, cy, r, z - (2.0 if j else 0.0), z + th[j], 12, rng.uniform(0.14, 0.28), peaks,
                  bottom=(j == 0), tilt=(math.cos(ta) * slope, math.sin(ta) * slope) if top else (0.0, 0.0))
        z += th[j]
        # Each bed sits back or juts out and drifts off the axis: a column's cap, no stack of even plates.
        r *= rng.uniform(0.84, 1.05)
        cx += rng.uniform(-0.06, 0.06) * W
        cy += rng.uniform(-0.06, 0.06) * W
    # One or two broken teeth standing on the top bed.
    for t in range(rng.choice((1, 2))):
        a = rng.uniform(0, 2 * math.pi)
        d = r * rng.uniform(0.2, 0.55)
        tr = r * rng.uniform(0.3, 0.5)
        th_t = H - z if t == 0 else (H - z) * rng.uniform(0.5, 0.8)
        th_t = max(th_t, 30.0)
        pk = noise_loop(rng, 3)
        crown_bed(me, rng, cx + math.cos(a) * d, cy + math.sin(a) * d, tr, z - 10.0, z + th_t, 8,
                  rng.uniform(0.15, 0.3), lambda t, pk=pk, a=th_t * 0.3: a * pk(t), bottom=False)
    return me


# --- 2. talus, 4. clutter -----------------------------------------------------------------------------------------

def talus(i):
    name = "talus_%d" % i
    rng = rng_for(name)
    s = rng.uniform(20, 60)
    size = (s, s * rng.uniform(0.6, 0.95), s * rng.uniform(0.5, 0.85))
    return stone(name, rng, size, 1, 0.14, rng.randint(2, 4), 1.0, 0.1, 30.0)


def pebble(i):
    name = "pebble_%d" % i
    rng = rng_for(name)
    s = rng.uniform(5, 20)
    size = (s, s * rng.uniform(0.6, 0.95), s * rng.uniform(0.4, 0.7))
    return stone(name, rng, size, 1 if s > 11 else 0, 0.1, rng.randint(1, 2), 1.0, 0.08, 55.0)


def clinker(i):
    name = "clinker_%d" % i
    rng = rng_for(name)
    s = rng.uniform(10, 30)
    size = (s, s * rng.uniform(0.7, 1.0), s * rng.uniform(0.55, 0.85))
    return stone(name, rng, size, 1, 0.22, 0, 1.0, 0.12, 48.0, pits=0.3)


def plate(name, rng, r, thick, lift, n, lobes, inner, crease, tilt=0.0):
    """A flat plate (crust, rime, flake): an outline of `n` points round a lobed radius, `thick` cm, its rim raised
    by `lift` cm (upturned edges)."""
    rn = noise_loop(rng, lobes)
    sq = rng.uniform(0.6, 1.0)
    a0 = rng.uniform(0, 2 * math.pi)
    tx, ty = rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt)
    outline = []
    for k in range(n):
        a = a0 + 2 * math.pi * k / n + rng.uniform(-0.3, 0.3) * 2 * math.pi / n
        rr = r * (1.0 + 0.32 * rn(k / n) + rng.uniform(-0.1, 0.1))
        outline.append((math.cos(a) * rr, math.sin(a) * rr * sq))
    zt = lambda x, y, z: z + tx * x + ty * y  # noqa: E731
    rings = [[(x * 0.92, y * 0.92, zt(x, y, 0.0)) for x, y in outline],
             [(x, y, zt(x, y, lift)) for x, y in outline],
             [(x * 0.97, y * 0.97, zt(x, y, lift + thick)) for x, y in outline]]
    if inner:
        rings.append([(x * inner, y * inner, zt(x, y, thick + lift * 0.25)) for x, y in outline])
    me = Mesh(name, crease=crease)
    me.loft(rings)
    lo, _ = me.bounds()
    me.shift((0, 0, -lo[2]))
    return me


def crust(i):
    name = "crust_%d" % i
    rng = rng_for(name)
    r = rng.uniform(30, 80) / 2
    return plate(name, rng, r, rng.uniform(2, 4), rng.uniform(1.5, 4.0), 9, rng.randint(3, 5), 0.72, 40.0)


def rime(i):
    name = "rime_%d" % i
    rng = rng_for(name)
    r = rng.uniform(40, 120) / 2
    return plate(name, rng, r, 1.0, 0.0, 12, rng.randint(4, 7), 0.0, 50.0)


def flake(i):
    name = "flake_%d" % i
    rng = rng_for(name)
    r = rng.uniform(3, 10) / 2
    return plate(name, rng, r, rng.uniform(0.3, 0.6), 0.0, rng.randint(5, 6), 3, 0.0, 30.0, tilt=0.15)


def shard(i):
    """A glass shard: one to three thin angular blades from one foot, leaning apart."""
    name = "shard_%d" % i
    rng = rng_for(name)
    me = Mesh(name, crease=20.0)
    for b in range(rng.choice((1, 2, 3)) if i else 3):
        h = rng.uniform(10, 40) * (1.0 if b == 0 else rng.uniform(0.45, 0.8))
        w = max(rng.uniform(8, 14) * h / 25, 4.0)
        t = rng.uniform(0.6, 1.4)
        yaw = rng.uniform(0, math.pi)
        lean = (rng.uniform(-0.35, 0.35), rng.uniform(-0.35, 0.35))
        cy, sy = math.cos(yaw), math.sin(yaw)

        def ring(z, wk, tk, off):
            # A thin rhomb, counter-clockwise from above: blade edge, face, other edge, face.
            pts = [(wk / 2, 0.0), (wk * 0.1, tk / 2), (-wk / 2, off), (-wk * 0.05, -tk / 2)]
            return [(x * cy - y * sy + lean[0] * z, x * sy + y * cy + lean[1] * z, z) for x, y in pts]
        mid = rng.uniform(0.35, 0.6)
        tip = (rng.uniform(-0.3, 0.3) * w, rng.uniform(-0.3, 0.3) * w)
        rings = [ring(-2.0, w * 0.8, t, 0.0), ring(h * mid, w * rng.uniform(0.8, 1.15), t * 0.8, 0.0),
                 [(tip[0] + lean[0] * h + dx, tip[1] + lean[1] * h + dy, h) for dx, dy in
                  ((0.2, 0), (0, 0.2), (-0.2, 0), (0, -0.2))]]
        me.loft(rings)
    return me


def tuft(i):
    """Dry grass: 5-12 thin blades from one foot, crossing, leaning out and curving; each blade two-sided (its
    back faces are its own, wound the other way) so a one-sided material shows both."""
    name = "tuft_%d" % i
    rng = rng_for(name)
    me = Mesh(name, slot="tuft", crease=10.0)
    nb = rng.randint(5, 12)
    for b in range(nb):
        h = rng.uniform(15, 40)
        w = rng.uniform(0.8, 1.6)
        yaw = 2 * math.pi * b / nb + rng.uniform(-0.6, 0.6)
        out = (math.cos(yaw), math.sin(yaw))
        side = (-out[1], out[0])
        lean = rng.uniform(0.15, 0.55)
        foot = (rng.uniform(-2.5, 2.5), rng.uniform(-2.5, 2.5))
        twist = rng.uniform(-0.5, 0.5)

        def at(f, half):
            # the blade's centre line bends out as it rises (lean grows with height squared)
            c = (foot[0] + out[0] * lean * h * f * f, foot[1] + out[1] * lean * h * f * f, h * f * (1 - 0.2 * lean * f))
            s = (side[0] + out[0] * twist * f, side[1] + out[1] * twist * f)
            return (c[0] + s[0] * half, c[1] + s[1] * half, c[2])
        a, b_, c, d = at(0, -w), at(0, w), at(0.55, w * 0.7), at(0.55, -w * 0.7)
        tip = at(1.0, 0.0)
        a = (a[0], a[1], -1.0)
        b_ = (b_[0], b_[1], -1.0)
        me.quad(a, b_, c, d)
        me.tri(d, c, tip)
        me.quad(a, d, c, b_)
        me.tri(d, tip, c)
    return me


KINDS = [("slab", slab, 8, 700), ("talus", talus, 6, 120), ("spire", spire, 4, 900), ("pebble", pebble, 6, 80),
         ("crust", crust, 4, 80), ("shard", shard, 4, 80), ("clinker", clinker, 4, 80), ("flake", flake, 4, 80),
         ("rime", rime, 4, 80), ("tuft", tuft, 4, 80)]


def all_meshes():
    out = []
    for kind, fn, count, cap in KINDS:
        for i in range(count):
            me = fn(i)
            if len(me.tris) > cap:
                raise RuntimeError("%s: %d triangles, over %d" % (me.name, len(me.tris), cap))
            out.append((kind, me))
    return out


# --- the editor: write the assets ----------------------------------------------------------------------------------

def write_assets():
    EAL = unreal.EditorAssetLibrary
    mi = None
    for p in MATERIALS:
        if EAL.does_asset_exist(p):
            mi = EAL.load_asset(p)
            break
    if not EAL.does_directory_exist(FOLDER):
        EAL.make_directory(FOLDER)
    lines = []
    for kind, me in all_meshes():
        path = FOLDER + "/SM_" + me.name
        verts, norms, uvs, tris = [], [], [], []
        for (a, b, c), (na, nb, nc) in zip(me.tris, me.normals()):
            fnrm = norm(cross(sub(b, a), sub(c, a)))
            ax = max(range(3), key=lambda i: abs(fnrm[i]))
            base = len(verts)
            # Out to the kit's rule: (b - a) x (c - a) into the solid, so the order flips; normals stay outward.
            for p, n in ((a, na), (c, nc), (b, nb)):
                verts.append(unreal.Vector(*p))
                norms.append(unreal.Vector(*n))
                u, v = [p[k] for k in range(3) if k != ax]
                uvs.append(unreal.Vector2D(u * 0.01, v * 0.01))
            tris.append(unreal.IntVector(base, base + 1, base + 2))
        dm = unreal.DynamicMesh()
        dm.reset()
        buf = unreal.GeometryScriptSimpleMeshBuffers()
        buf.set_editor_property("vertices", verts)
        buf.set_editor_property("normals", norms)
        buf.set_editor_property("uv0", uvs)
        buf.set_editor_property("triangles", tris)
        unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dm, buf, 0)
        # Weld the corners that share a point and a normal (the buffers come in one vertex per corner).
        try:
            unreal.GeometryScript_MeshRepair.weld_mesh_edges(dm, unreal.GeometryScriptWeldEdgesOptions())
        except Exception as e:  # a missing weld costs vertices, not looks
            unreal.log_warning("[rocks] weld skipped: %s" % e)
        ntris = unreal.GeometryScript_MeshQueries.get_num_triangle_i_ds(dm)
        sm = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
        if sm is None:
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.set_editor_property("enable_collision", False)
            opts.set_editor_property("enable_nanite", False)
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            sm, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, opts)
            if sm is None or outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
                raise RuntimeError("cannot create " + path)
        copt = unreal.GeometryScriptCopyMeshToAssetOptions()
        copt.set_editor_property("enable_recompute_normals", False)
        copt.set_editor_property("enable_recompute_tangents", True)
        ns = unreal.MeshNaniteSettings()
        ns.set_editor_property("enabled", False)
        copt.set_editor_property("apply_nanite_settings", True)
        copt.set_editor_property("new_nanite_settings", ns)
        copt.set_editor_property("replace_materials", True)
        copt.set_editor_property("new_materials", [mi])
        copt.set_editor_property("new_material_slot_names", [me.slot])
        _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
            dm, sm, copt, unreal.GeometryScriptMeshWriteLOD())
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError("cannot update " + path)
        body = sm.get_editor_property("body_setup")
        if body is not None:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        EAL.set_metadata_tag(sm, "AcMadeBy", "Tools/Editor/models/terrain_lab_rocks.py")
        EAL.save_loaded_asset(sm)
        slots = [str(s.get_editor_property("material_slot_name")) for s in sm.get_editor_property("static_materials")]
        lo, hi = me.bounds()
        line = "%s %d tris, slots %s, size %.0f x %.0f x %.0f cm" % (
            path, ntris, slots, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2])
        lines.append(line)
        unreal.log("[rocks] " + line)
    unreal.log("[rocks] wrote %d meshes" % len(lines))


# --- outside the editor: the contact sheet -------------------------------------------------------------------------

def render(meshes, out, tile=230):
    import numpy as np
    from PIL import Image, ImageDraw

    def view(yaw, pitch):
        yaw, pitch = math.radians(yaw), math.radians(pitch)
        v = np.array([math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)])
        f = -v
        r = np.cross(f, [0, 0, 1.0])
        r /= np.linalg.norm(r)
        u = np.cross(r, f)
        return v, r, u

    light = np.array([0.45, -0.35, 0.82])
    light /= np.linalg.norm(light)

    def draw(img, zbuf, me, ox, oy, scale, cam, centre):
        v, r, u = cam
        T = np.array(me.tris) - centre
        N = np.array(me.normals())
        sx = T @ r * scale + ox
        sy = -(T @ u) * scale + oy
        sz = T @ v
        H, W = zbuf.shape
        for t in range(len(T)):
            x, y, z = sx[t], sy[t], sz[t]
            x0, x1 = max(int(x.min()), 0), min(int(x.max()) + 2, W)
            y0, y1 = max(int(y.min()), 0), min(int(y.max()) + 2, H)
            if x0 >= x1 or y0 >= y1:
                continue
            den = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
            if abs(den) < 1e-9:
                continue
            px, py = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
            w0 = ((y[1] - y[2]) * (px - x[2]) + (x[2] - x[1]) * (py - y[2])) / den
            w1 = ((y[2] - y[0]) * (px - x[2]) + (x[0] - x[2]) * (py - y[2])) / den
            w2 = 1 - w0 - w1
            m = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
            if not m.any():
                continue
            zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
            zb = zbuf[y0:y1, x0:x1]
            m &= zz > zb
            if not m.any():
                continue
            zb[m] = zz[m]
            fn = np.cross(T[t][1] - T[t][0], T[t][2] - T[t][0])
            front = fn @ v > 0
            if not front and me.slot == "tuft":
                continue  # two-sided by its own back faces: cull as the game does
            n = w0[..., None] * N[t][0] + w1[..., None] * N[t][1] + w2[..., None] * N[t][2]
            n /= np.linalg.norm(n, axis=-1, keepdims=True)
            lam = np.clip(n @ light, 0, 1) * 0.75 + 0.25
            col = np.array([185, 170, 150.0]) if front else np.array([230, 30, 30.0])
            if not front:
                lam = np.ones_like(lam)
            img[y0:y1, x0:x1][m] = (lam[..., None] * col)[m]

    cam = view(-35, 28)
    rows = []
    for kind, fn, count, cap in KINDS:
        rows.append((kind, [me for k, me in meshes if k == kind]))
    cols = 8
    cliff_h = 300
    Wpx, Hpx = cols * tile, len(rows) * tile + cliff_h
    img = np.full((Hpx, Wpx, 3), 58.0)
    zbuf = np.full((Hpx, Wpx), -1e9)
    labels = []
    for ri, (kind, ms) in enumerate(rows):
        ext = 0
        for me in ms:
            lo, hi = me.bounds()
            ext = max(ext, max(hi[k] - lo[k] for k in range(3)))
        scale = tile * 0.62 / ext
        for ci, me in enumerate(ms):
            lo, hi = me.bounds()
            c = np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2])
            draw(img, zbuf, me, ci * tile + tile / 2, ri * tile + tile / 2 + 8, scale, cam, c)
            labels.append((ci * tile + 6, ri * tile + 4, "%s  %d tris  %.0f cm" % (
                me.name, len(me.tris), max(hi[k] - lo[k] for k in range(3)))))
    # The cliff: the eight slabs side by side along the run, seen from out in front, to judge the beds' repeat.
    slabs = [me for k, me in meshes if k == "slab"]
    run = sum(me.bounds()[1][1] - me.bounds()[0][1] for me in slabs)
    scale = (Wpx - 40) / run
    y = -run / 2
    cam2 = view(-8, 10)
    oy = len(rows) * tile + cliff_h - 30
    for me in slabs:
        lo, hi = me.bounds()
        ln = hi[1] - lo[1]
        T = Mesh(me.name, crease=me.crease)
        T.tris = me.tris
        # place this slab's middle at its spot on the run (the view's right axis is about +Y)
        shift = np.array([0.0, y + ln / 2 - (lo[1] + hi[1]) / 2, 0.0])
        T.tris = [tuple(tuple(np.array(p) + shift) for p in t) for t in me.tris]
        draw(img, zbuf, T, Wpx / 2, oy, scale, cam2, np.array([0.0, 0.0, 0.0]))
        y += ln
    labels.append((6, len(rows) * tile + 4, "slab_0..7 side by side (front, 10 deg up); red = a back face"))
    im = Image.fromarray(np.clip(img, 0, 255).astype("uint8"))
    d = ImageDraw.Draw(im)
    for x, yy, s in labels:
        d.text((x, yy), s, fill=(235, 235, 235))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    im.save(out, quality=88)
    print("wrote", out)


if __name__ == "__main__" and unreal is None:
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="write a contact sheet (jpg) of every mesh")
    a = ap.parse_args()
    ms = all_meshes()
    for kind, me in ms:
        lo, hi = me.bounds()
        print("%-10s %4d tris  %5.0f x %5.0f x %5.0f cm" % (
            me.name, len(me.tris), hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]))
    if a.preview:
        render(ms, a.preview)
elif unreal is not None:
    write_assets()
