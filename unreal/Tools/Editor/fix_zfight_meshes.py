"""Z-fighting fixes for models with no modelling script (the SceneKit imports: Bastion, Sentinel, Garrison, Foundry,
Spacedock, Lab, Derrick and their scaffolds), done on their static meshes in the editor:

    AC_FIX_MODELS=bastion_blue,scaffold_bastion_blue uv run unreal/Tools/hidden_run.py --out DIR \
        --python unreal/Tools/Editor/fix_zfight_meshes.py

For each model it finds what check_models.py finds (triangles of two meshes, or of one mesh's two solids, facing the
same way in one plane and overlapping where someone can see them) and, for each such pair, moves the smaller of the
two solids' face in that plane 2 cm outward along its normal, with every vertex of that solid at the same positions,
so the solid grows 2 cm (a band then stands 2 cm proud of the hull it lay flush with; nothing that showed is hidden).
The faces of a skin (a plate, a band, a visor round a dome: all its moved faces facing one way) move as an offset
surface: each vertex along its moved faces' normals together, so every one of them ends 2 cm out. A solid flush with
another on opposite faces (a joint: a toe plate in a sole) would swell that way, so it shrinks 1 cm a side about its
middle instead.
A solid is a connected piece of one mesh (its vertices welded by position). It saves the meshes it changed and checks
again, up to three rounds, and logs what it moved. The model scripts' models are fixed in their scripts instead.
"""
import collections
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ac_modelkit as K  # noqa: E402
import check_models as C  # noqa: E402

OUT = 2.0  # cm
INSET = 1.0  # cm: a solid flush with another on opposite faces (a joint) shrinks this much on every side instead


def log(s):
    unreal.log("[fix_zfight] " + s)


def load(model):
    """{mesh id: dict(path, R, t, pts (part frame), tris, placed (model frame))} and the model's Tris."""
    parts = {p["name"]: p for p in model["parts"]}
    world = {}

    def w(p):
        if p["name"] not in world:
            mx = p["localTransform"]
            world[p["name"]] = C.m4_mul(mx, w(parts[p["parent"]])) if p.get("parent") else mx
        return world[p["name"]]
    meshes, tris = {}, []
    for p in model["parts"]:
        ue = K.sk_to_ue(w(p))
        R, t = (ue[0:3], ue[4:7], ue[8:11]), ue[12:15]
        for mesh in p.get("meshes", []):
            path = mesh["mesh"].split(".")[0]
            got = C.mesh_tris(path)
            if got is None:
                continue
            pts, tr = got
            placed = [tuple(x * R[0][i] + y * R[1][i] + z * R[2][i] + t[i] for i in range(3)) for x, y, z in pts]
            mid = "%s/%s" % (p["name"], mesh["material"])
            meshes[mid] = dict(path=path, R=R, t=t, pts=pts, tris=tr, placed=placed)
            tris += C.triangles(mid, placed, tr)
    return meshes, tris


def solids(mesh):
    """Each vertex's solid: connected pieces of the mesh, vertices welded by position (0.01 cm)."""
    key = [tuple(round(c, 2) for c in p) for p in mesh["placed"]]
    parent = list(range(len(key)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb
    first = {}
    for v, k in enumerate(key):
        if k in first:
            union(v, first[k])
        else:
            first[k] = v
    for i, j, k in mesh["tris"]:
        union(i, j)
        union(j, k)
    return [find(v) for v in range(len(key))], key


def flat_patches(tris, info):
    """id(Tri) -> (patch id, patch area, the patch's vertices with every copy of their positions): a patch is the
    triangles of one solid in one plane joined through shared positions."""
    groups = collections.defaultdict(list)
    for t in tris:
        groups[(t.mesh, info[t.mesh][0][t.vids[0]], round(t.n[0], 2), round(t.n[1], 2), round(t.n[2], 2), round(t.d))].append(t)
    out = {}
    pid = 0
    for (mid, root, *_), group in groups.items():
        key = info[mid][1]
        by_pos = collections.defaultdict(list)
        for t in group:
            for v in t.vids:
                by_pos[key[v]].append(t)
        seen = set()
        for t in group:
            if id(t) in seen:
                continue
            pid += 1
            stack, members = [t], []
            seen.add(id(t))
            while stack:
                u = stack.pop()
                members.append(u)
                for v in u.vids:
                    for w in by_pos[key[v]]:
                        if id(w) not in seen:
                            seen.add(id(w))
                            stack.append(w)
            positions = {key[v] for u in members for v in u.vids}
            verts = [v for v in range(len(key)) if key[v] in positions and info[mid][0][v] == root]
            total = sum(area(u) for u in members)
            for u in members:
                out[id(u)] = (pid, total, verts)
    return out


def area(t):
    a, b, c = t.pts
    cr = C._cross(C._sub(b, a), C._sub(c, a))
    return 0.5 * math.sqrt(C._dot(cr, cr))


def fix_model(model):
    moved_total = []
    for rnd in range(3):
        meshes, tris = load(model)
        found = []
        C.check(tris, collect=found)
        if not found:
            break
        info = {mid: solids(m) for mid, m in meshes.items()}
        size = collections.Counter()
        for t in tris:
            size[(t.mesh, info[t.mesh][0][t.vids[0]])] += area(t)
        # for each pair, the smaller solid's plane moves out: {(mesh, solid): {(plane key): normal}}
        moves, patch_moves = {}, collections.defaultdict(dict)
        patches = None
        for a, b, _ in found:
            sa, sb = (a.mesh, info[a.mesh][0][a.vids[0]]), (b.mesh, info[b.mesh][0][b.vids[0]])
            if sa == sb:
                # one solid's own two faces (a fold): move the smaller flat patch of the two
                patches = patches or flat_patches(tris, info)
                pa, pb = patches[id(a)], patches[id(b)]
                if pa[0] == pb[0]:
                    continue
                pick = a if pa[1] <= pb[1] else b
                for v in (patches[id(pick)][2]):
                    prev = patch_moves[pick.mesh].get(v, (0.0, 0.0, 0.0))
                    patch_moves[pick.mesh][v] = tuple(prev[i] + pick.n[i] * OUT for i in range(3)) if v not in \
                        patch_moves[pick.mesh] else prev
                continue
            pick = a if size[sa] <= size[sb] else b
            s = sa if pick is a else sb
            moves.setdefault(s, {})[(round(pick.n[0], 2), round(pick.n[1], 2), round(pick.n[2], 2), round(pick.d))] = (pick.n, pick.d)
        changed = collections.defaultdict(dict)  # mesh -> {vertex: displacement (model frame)}
        for mid, disp in patch_moves.items():
            changed[mid].update(disp)
            moved_total.append("%s: a flat patch of %d vertices" % (mid, len(disp)))
        for (mid, root), planes in moves.items():
            m = meshes[mid]
            roots, key = info[mid]
            normals = [n for n, _ in planes.values()]
            if any(C._dot(a, b) < -0.5 for a in normals for b in normals):
                # a joint (a piece flush with another on opposite sides: a toe plate in a sole): growing it there would
                # swell it, so the smaller piece shrinks about its middle, every side INSET in
                vs = [v for v in range(len(key)) if roots[v] == root]
                lo = [min(m["placed"][v][i] for v in vs) for i in range(3)]
                hi = [max(m["placed"][v][i] for v in vs) for i in range(3)]
                mid3 = [(a + b) / 2 for a, b in zip(lo, hi)]
                half = [(b - a) / 2 for a, b in zip(lo, hi)]
                for v in vs:
                    changed[mid][v] = tuple((mid3[i] - m["placed"][v][i]) * (INSET / half[i] if half[i] > 2 * INSET else 0.0)
                                            for i in range(3))
                moved_total.append("%s solid %d: shrunk %g cm a side (a joint, %d planes)" % (mid, root, INSET, len(planes)))
                continue
            # a skin (a plate, a band, a visor round a dome: its faces all one way): an offset surface, each vertex along
            # its moved faces' normals together, far enough that every one of those faces stands OUT out
            on = collections.defaultdict(list)  # position -> the normals of its moved planes
            for n, d in planes.values():
                for v in range(len(key)):
                    if roots[v] == root and abs(C._dot(n, m["placed"][v]) - d) < C.D_TOL and n not in on[key[v]]:
                        on[key[v]].append(n)
            for v in range(len(key)):
                ns = on.get(key[v]) if roots[v] == root else None
                if not ns:
                    continue
                sm = [sum(n[i] for n in ns) for i in range(3)]
                ln = math.sqrt(sum(c * c for c in sm))
                dirn = [c / ln for c in sm]
                mag = OUT / max(0.25, min(C._dot(n, dirn) for n in ns))
                prev = changed[mid].get(v, (0.0, 0.0, 0.0))
                changed[mid][v] = tuple(prev[i] + dirn[i] * mag for i in range(3))
            moved_total.append("%s solid %d: %d plane(s)" % (mid, root, len(planes)))
        for mid, disp in changed.items():
            write(meshes[mid], disp)
        log("%s, round %d: %d overlapping triangle pairs, moved %d solid planes in %d meshes" % (
            model["name"], rnd + 1, len(found), sum(len(p) for p in moves.values()), len(changed)))
    meshes, tris = load(model)
    pairs, _ = C.check(tris)
    log("%s: %d coplanar pairs of meshes left; moved: %s" % (model["name"], len(pairs), "; ".join(moved_total) or "nothing"))
    return pairs


def write(mesh, disp):
    """Move the vertices by their model-frame displacements and save the static mesh."""
    sm = unreal.load_asset(mesh["path"])
    dm = unreal.DynamicMesh()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
    res = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(sm, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), lod)
    dm = res[0] if isinstance(res, tuple) else dm
    R = mesh["R"]
    for v, dmod in disp.items():
        # model frame = part frame . R (rows orthonormal): the part-frame displacement is dmod . R^T
        dp = tuple(sum(dmod[j] * R[i][j] for j in range(3)) for i in range(3))
        x, y, z = mesh["pts"][v]
        unreal.GeometryScript_MeshEdits.set_vertex_position(dm, v, unreal.Vector(x + dp[0], y + dp[1], z + dp[2]), True)
    opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    opts.set_editor_property("enable_recompute_normals", False)
    opts.set_editor_property("enable_recompute_tangents", True)
    opts.set_editor_property("replace_materials", False)
    _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm, sm, opts, unreal.GeometryScriptMeshWriteLOD())
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("cannot update " + mesh["path"])
    unreal.EditorAssetLibrary.set_metadata_tag(sm, "AcZFight", "moved %d vertices 2 cm out (fix_zfight_meshes.py)" % len(disp))
    unreal.EditorAssetLibrary.save_loaded_asset(sm)


def main():
    names = [s for s in os.environ.get("AC_FIX_MODELS", "").split(",") if s]
    if not names:
        raise RuntimeError("fix_zfight_meshes: name the models in AC_FIX_MODELS")
    doc = K.load_catalog()
    by = {m["name"]: m for m in doc["models"]}
    for name in names:
        fix_model(by[name])


main()
