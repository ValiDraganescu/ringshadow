"""Z-fighting check of every model in the catalog, at the mesh level (the pipeline skill's step after any model change;
MODELLING.md "No shared faces"):

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/check_models.py
    AC_CHECK_MODELS=gatling_blue,citadel_blue uv run ...   (only these models; default: every model but the _red copies)

For every model of ModelCatalog.json (a `_red` entry draws the `_blue` meshes, so it is left out) it reads each part's
static meshes (Geometry Script, the source model), places them in the model frame at rest (the catalog's part
transforms) and finds:
- coplanar pairs: triangles of two meshes (two parts, one part's two materials, or one mesh's two solids) facing the
  same way in one plane (normals within 0.5 degrees, planes within 0.5 cm) and overlapping by more than 1 cm2. An overlap whose
  front is covered by triangles facing the other way in the same plane (two plates' backs pressed on a wall) is
  left out: nobody sees it;
- duplicates: a mesh whose every triangle lies on a triangle of another mesh (the same part twice).
Hidden parts count too (an armed Citadel's turrets), marked "hidden" in the report.

Writes art/models/zfight-report.md (the models with hits, worst first: part A, part B, triangles, area) and logs a
line per model. Raises (the run fails) when any building or construction model has a hit, so it stays clean; the
other categories are reported only, until they are fixed.
"""
import collections
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ac_modelkit as K  # noqa: E402

try:
    import unreal
except ImportError:
    unreal = None

COS_TOL = math.cos(math.radians(0.5))
D_TOL = 0.5
MIN_AREA = 1.0
STRICT = ("building", "construction")
REPORT = os.path.join(K.REPO, "art", "models", "zfight-report.md")


def log(s):
    if unreal:
        unreal.log("[check_models] " + s)
    else:
        print("[check_models] " + s)


# --- geometry (plain Python) ------------------------------------------------------------------------------------


def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _area2(p):
    return 0.5 * sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p)))


def _ccw(p):
    return p if _area2(p) >= 0 else p[::-1]


def _clip(poly, a, b):
    out = []
    side = lambda p: (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
    for i in range(len(poly)):
        p, q = poly[i], poly[(i + 1) % len(poly)]
        sp, sq = side(p), side(q)
        if sp >= 0:
            out.append(p)
        if (sp >= 0) != (sq >= 0):
            t = sp / (sp - sq)
            out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


def _inside_tri(p, t, eps=1e-6):
    (ax, ay), (bx, by), (cx, cy) = t
    d1 = (bx - ax) * (p[1] - ay) - (by - ay) * (p[0] - ax)
    d2 = (cx - bx) * (p[1] - by) - (cy - by) * (p[0] - bx)
    d3 = (ax - cx) * (p[1] - cy) - (ay - cy) * (p[0] - cx)
    return (d1 >= -eps and d2 >= -eps and d3 >= -eps) or (d1 <= eps and d2 <= eps and d3 <= eps)


class Tri:
    __slots__ = ("mesh", "n", "d", "pts", "flat", "lo", "hi")


def triangles(mesh_id, pts, tris):
    """Model-frame triangles of one mesh: outward normal (the kit's winding: (b - a) x (c - a) points in)."""
    out = []
    for i, j, k in tris:
        a, b, c = pts[i], pts[j], pts[k]
        cr = _cross(_sub(b, a), _sub(c, a))
        ln = math.sqrt(_dot(cr, cr))
        if ln < 1e-6:
            continue
        t = Tri()
        t.mesh, t.n, t.pts = mesh_id, (-cr[0] / ln, -cr[1] / ln, -cr[2] / ln), (a, b, c)
        t.d = _dot(t.n, a)
        out.append(t)
    return out


def _key(n):
    return (round(n[0] * 50), round(n[1] * 50), round(n[2] * 50))


def _basis(n):
    a = (1.0, 0.0, 0.0) if abs(n[0]) < 0.9 else (0.0, 1.0, 0.0)
    u = _cross(n, a)
    lu = math.sqrt(_dot(u, u))
    u = (u[0] / lu, u[1] / lu, u[2] / lu)
    return u, _cross(n, u)


def check(tris):
    """Returns ({(meshA, meshB): [count, area]}, {meshA: covered triangle count by another mesh, per other mesh})."""
    bins = collections.defaultdict(list)
    for t in tris:
        bins[_key(t.n)].append(t)
    pairs = collections.defaultdict(lambda: [0, 0.0])
    same = collections.defaultdict(int)  # (mesh, other): triangles of `mesh` lying on a triangle of `other`
    for key, group in bins.items():
        n = group[0].n
        u, v = _basis(n)
        for t in group:
            t.flat = _ccw([(_dot(p, u), _dot(p, v)) for p in t.pts])
            t.lo = (min(p[0] for p in t.flat), min(p[1] for p in t.flat))
            t.hi = (max(p[0] for p in t.flat), max(p[1] for p in t.flat))
        opp = bins.get((-key[0], -key[1], -key[2]), [])
        # the opposite-facing triangles in this basis (their planes at -d)
        oflat = [(-t.d, [(_dot(p, u), _dot(p, v)) for p in t.pts]) for t in opp]
        oflat.sort(key=lambda x: x[0])
        group.sort(key=lambda t: t.d)
        for x in range(len(group)):
            a = group[x]
            for y in range(x + 1, len(group)):
                b = group[y]
                if b.d - a.d > D_TOL:
                    break
                if _dot(a.n, b.n) < COS_TOL:
                    continue
                if b.lo[0] >= a.hi[0] or a.lo[0] >= b.hi[0] or b.lo[1] >= a.hi[1] or a.lo[1] >= b.hi[1]:
                    continue
                poly = a.flat
                for k in range(3):
                    poly = _clip(poly, b.flat[k], b.flat[(k + 1) % 3])
                    if len(poly) < 3:
                        break
                area = abs(_area2(poly)) if len(poly) >= 3 else 0.0
                if area <= 0.01:
                    continue
                ta, tb = abs(_area2(a.flat)), abs(_area2(b.flat))
                if a.mesh != b.mesh and area >= ta * 0.999:
                    same[(a.mesh, b.mesh)] += 1
                if a.mesh != b.mesh and area >= tb * 0.999:
                    same[(b.mesh, a.mesh)] += 1
                # covered from the front by a triangle facing the other way in this plane?
                cu = sum(p[0] for p in poly) / len(poly)
                cv = sum(p[1] for p in poly) / len(poly)
                probes = [(cu, cv)] + [(p[0] + math.copysign(min(0.3, abs(cu - p[0])), cu - p[0]),
                                        p[1] + math.copysign(min(0.3, abs(cv - p[1])), cv - p[1])) for p in poly]
                near = [f for dd, f in oflat if abs(dd - a.d) <= D_TOL]
                if near and all(any(_inside_tri(q, f) for f in near) for q in probes):
                    continue
                k2 = (a.mesh, b.mesh) if a.mesh < b.mesh else (b.mesh, a.mesh)
                pairs[k2][0] += 1
                pairs[k2][1] += area
    return pairs, same


def selftest():
    def quad(mesh, z, x0, x1, y0, y1, up=True):
        a, b, c, d = (x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)
        # outward +z: (b - a) x (c - a) must point down (into the solid)
        tris = [(a, c, b), (a, d, c)] if up else [(a, b, c), (a, c, d)]
        out = []
        for p, q, r in tris:
            out += triangles(mesh, [p, q, r], [(0, 1, 2)])
        return out
    t = quad("A", 0, 0, 10, 0, 10) + quad("B", 0.2, 5, 15, 0, 10)
    pairs, _ = check(t)
    assert pairs[("A", "B")][0] >= 1 and abs(pairs[("A", "B")][1] - 50) < 1, pairs
    t = quad("A", 0, 0, 10, 0, 10) + quad("B", 2, 5, 15, 0, 10)
    assert not check(t)[0]
    assert check(quad("A", 0, 0, 10, 0, 10) + quad("A", 0, 5, 15, 0, 10))[0][("A", "A")][0] >= 1  # within one mesh
    assert not check(quad("A", 0, 0, 10, 0, 10) + quad("A", 0, 10, 20, 0, 10))[0]                # side by side
    # two backs (facing down) pressed on a wall's top (facing up)
    t = quad("A", 0, 0, 10, 0, 10, up=False) + quad("B", 0, 0, 10, 0, 10, up=False) + quad("W", 0, -5, 20, -5, 20)
    assert not check(t)[0], check(t)[0]
    _, same = check(quad("A", 0, 0, 10, 0, 10) + quad("B", 0, 0, 10, 0, 10))
    assert same[("A", "B")] == 2 and same[("B", "A")] == 2, same
    log("selftest: the six cases hold")


# --- the catalog, in the editor ------------------------------------------------------------------------------------


def mesh_tris(path):
    sm = unreal.load_asset(path)
    if sm is None:
        return None
    dm = unreal.DynamicMesh()
    opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
    res = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(sm, dm, opts, lod)
    dm = res[0] if isinstance(res, tuple) else dm
    _, pos, _ = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, True)
    pts = [(p.x, p.y, p.z) for p in unreal.GeometryScript_List.convert_vector_list_to_array(pos)]
    _, tri, _ = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, True)
    return pts, [(t.x, t.y, t.z) for t in unreal.GeometryScript_List.convert_triangle_list_to_array(tri)]


def m4_mul(a, b):
    return [sum(a[i * 4 + k] * b[k * 4 + j] for k in range(4)) for i in range(4) for j in range(4)]


def model_tris(model, cache):
    parts = {p["name"]: p for p in model["parts"]}
    world = {}

    def w(p):
        if p["name"] not in world:
            mx = p["localTransform"]
            world[p["name"]] = m4_mul(mx, w(parts[p["parent"]])) if p.get("parent") else mx
        return world[p["name"]]

    def hidden(p):
        return p.get("hidden") or (p.get("parent") and hidden(parts[p["parent"]]))
    out, hid = [], set()
    for p in model["parts"]:
        ue = K.sk_to_ue(w(p))
        R = [ue[0:3], ue[4:7], ue[8:11]]
        t = ue[12:15]
        for mesh in p.get("meshes", []):
            path = mesh["mesh"].split(".")[0]
            if path not in cache:
                cache[path] = mesh_tris(path)
            got = cache[path]
            if got is None:
                log("%s: no mesh %s" % (model["name"], path))
                continue
            pts, tris = got
            placed = [(x * R[0][0] + y * R[1][0] + z * R[2][0] + t[0], x * R[0][1] + y * R[1][1] + z * R[2][1] + t[1],
                       x * R[0][2] + y * R[1][2] + z * R[2][2] + t[2]) for x, y, z in pts]
            mid = "%s/%s" % (p["name"], mesh["material"])
            if hidden(p):
                hid.add(mid)
            out += triangles(mid, placed, tris)
    return out, hid


def main():
    selftest()
    doc = K.load_catalog()
    only = [s for s in os.environ.get("AC_CHECK_MODELS", "").split(",") if s]
    models = [m for m in doc["models"] if (m["name"] in only if only else not m["name"].endswith("_red"))]
    cache = {}
    rows, strict_hits = [], []
    for m in models:
        tris, hid = model_tris(m, cache)
        pairs, same = check(tris)
        sizes = collections.Counter(t.mesh for t in tris)
        dups = sorted({(a, b) for (a, b), c in same.items() if c >= sizes[a] > 0 and a != b})
        area = sum(v[1] for v in pairs.values())
        log("%s (%s): %d triangles, %d coplanar pairs of meshes, %.0f cm2, %d duplicate meshes" % (
            m["name"], m["category"], len(tris), len(pairs), area, len(dups)))
        if pairs or dups:
            rows.append((m, pairs, dups, hid, area))
            if m["category"] in STRICT:
                strict_hits.append(m["name"])
    rows.sort(key=lambda r: (r[0]["category"] not in STRICT, -r[4]))
    lines = ["# Z-fighting report",
             "",
             "Written by `unreal/Tools/Editor/check_models.py` (run it with `hidden_run.py --python` after any model "
             "change). For each model: pairs of meshes (part/material) with triangles facing the same way in one plane "
             "(normals within 0.5 degrees, planes within 0.5 cm) that overlap where someone can see them (A = B: two solids "
             "in one mesh), and meshes "
             "that are another mesh's copy. The `_red` entries draw the `_blue` meshes and are left out. Fix by the 2 cm "
             "rule (MODELLING.md, \"No shared faces\").",
             "",
             "Checked %d models; %d with hits." % (len(models), len(rows)), ""]
    for m, pairs, dups, hid, area in rows:
        lines += ["## %s (%s)" % (m["name"], m["category"]), "",
                  "| part/material A | part/material B | triangles | area cm2 |", "|---|---|---|---|"]
        for (a, b), (cnt, ar) in sorted(pairs.items(), key=lambda kv: -kv[1][1]):
            mark = lambda s: s + (" (hidden)" if s in hid else "")
            lines.append("| %s | %s | %d | %.0f |" % (mark(a), mark(b), cnt, ar))
        for a, b in dups:
            lines.append("| %s | %s | duplicate: every triangle of A lies on B | |" % (a, b))
        lines.append("")
    with open(REPORT, "w") as f:
        f.write("\n".join(lines))
    log("report: %s (%d models with hits)" % (REPORT, len(rows)))
    if strict_hits and not only:
        raise RuntimeError("check_models: coplanar faces in %s (see %s)" % (", ".join(strict_hits), REPORT))


if __name__ == "__main__":
    if unreal is None:
        selftest()
    else:
        main()
