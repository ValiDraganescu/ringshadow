"""The Citadel's four corner turrets (D1, docs/mmorpg-m8.md): rapid rotary close-in guns, one on each of the Citadel
model's four corner pylons, that track and shred incoming bugs, ground and air, with long tracer streams. They come
with every Citadel of a world (built with it, no blueprint): the core fires them (`Defence::cornerGun`, four guns per
armed Citadel) and the pose shows them only on an armed Citadel (`Structure::marks`), so the strategy game's
Citadels look as they always did. Small cousins of the gatling gun (gatling.py): a turret ring, a squat housing and a
six-barrel cluster with white-hot tips.

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/citadel_turrets.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/citadelguns/SM_<part>__<material>, then adds the parts to the `citadel_blue` and `citadel_red`
catalog entries (parented to the Citadel's root, hidden at rest) and leaves no entry of its own. Run again, it
replaces the parts it added.

UE cm in the Citadel's model frame (front +Y, Z up). Turret `k` stands on the pylon at corner k of the core's order:
0 (+x, +y), 1 (-x, +y), 2 (-x, -y), 3 (+x, -y), at the pylons' top. C1 (docs/citadel-interior.md): the Citadel is
12 by 12 cells, the mounts at (+-550, +-550) on the corner towers, their top at 900 (citadel.py PYLON_TOP). Parts
per turret: `turrets_k` (yaw: -aim), `turretCradles_k` (C1: the pitch, about the trunnion across the barrels),
`turretBarrels_k` (spins about its axis while firing), `turretTips_k` (heat glow), `turretMuzzles_k` (where the
rounds leave), `turretFlashes_k` (a frame per round). The muzzles, flashes and the muzzle ring ride on the cradle.

The guns are manned (C1 section 5): each points only within Defence::gunArcHalf (110 degrees) of its corner's
outward angle, pitch Defence::gunPitchMin (-20) to +50. The script asserts that the barrels clear the Citadel's
solids (the catalog's ray pieces of citadel_blue, written by citadel.py: run that first) at every yaw of the arc at
the lowest pitch, and refuses to write otherwise.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Box, Cyl, Model, box, cyl, m_apply, sphere, load_catalog, save_catalog, sk_point  # noqa: E402

POSE = "AcPoseBuildings.cpp:PoseCitadel"
CORNER = 550.0  # Defence::cornerInset 5.5
TOP = 900.0     # citadel.py PYLON_TOP (Defence::mountHeight 9.0)
ARC_HALF = 110.0  # Defence::gunArcHalf
PITCH_MIN = -20.0  # Defence::gunPitchMin
CLEAR = 3.0     # cm the barrels keep off every solid
PREFIXES = ("turrets_", "turretCradles_", "turretBarrels_", "turretTips_", "turretMuzzles_", "turretFlashes_")

m = Model("citadelguns", category="building", coplanar_check=True)
for name in ("hullLight", "hullMid", "hullDark", "team", "chrome", "glowYellow"):
    m.material(name)
# N1b: the base colour the hue (a white base washed the heat to white); a steady lit ring round each muzzle.
m.material("gatlingHeat", parent="emissive", color=(0.55, 0.24, 0.05), emissive=(2.6, 1.0, 0.22))
m.material("muzzleRing", parent="emissive", color=(0.5, 0.18, 0.02), emissive=(1.9, 0.62, 0.08))
m.material("gunmetal", parent="hull", color=(0.035, 0.036, 0.04), metallic=0.85, roughness=0.32)
# N1b: the turrets 1.8 times their D1 size, so they read on the pylons from the usual camera.
S = 1.8


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


cradles = []  # (k, turret pivot, cradle pivot, [prims that pitch])
for k, (sx, sy) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
    x, y = sx * CORNER, sy * CORNER

    def at(dx, dy, dz):
        """A point dx, dy, dz cm (at D1's size) from the turret's foot, scaled."""
        return (x + dx * S, y + dy * S, TOP + dz * S)

    t = m.part("turrets_%d" % k, at=(x, y, TOP), hidden=True,
               driver=d("corner turret %d: yaw toward its target (-aim); shown only on an armed Citadel" % k,
                        ("rotation", "visibility")))
    m.mesh(t, "hullDark", [cyl(26 * S, 8 * S, at=at(0, 0, 0), base=True, sides=20)])
    # (the bands and rings stand 2 cm off the faces they would share: the coplanar check of ac_modelkit)
    m.mesh(t, "chrome", [cyl(27 * S, 2 * S, at=at(0, 0, 4.8), base=True, sides=20)], solid=False)
    # C1: the housing's back and the cheeks low (top at 24 and 27 of D1's 28 and 34), so the gun's eye
    # (Defence::gunEye, 1.2 over the mount, 0.4 behind it) sees the barrels and the muzzle ring over them
    m.mesh(t, "hullMid", [box((34 * S, 30 * S, 16 * S), at=at(-2, 0, 8), base=True)])
    for s in (-1, 1):
        m.mesh(t, "hullLight", [box((28 * S, 6 * S, 19 * S), at=at(6, s * 17, 8), base=True)])
    m.mesh(t, "team", [box((20 * S, 2 * S, 6 * S), at=at(-2, 15.5, 16), base=True)], solid=False)
    axis = at(12, 0, 22)
    cr = m.part("turretCradles_%d" % k, parent=t, at=at(8, 0, 22), hidden=True,
                driver=d("corner turret %d's guns: pitch about the trunnion (C1: the gunner's pitch)" % k,
                         ("rotation", "visibility")))
    b = m.part("turretBarrels_%d" % k, parent=cr, at=axis, hidden=True,
               driver=d("corner turret %d's barrels: spin about their axis while it fires" % k, ("rotation", "visibility")))
    m.mesh(b, "gunmetal", [cyl(10 * S, 18 * S, at=axis, axis="x", base=True, sides=14)])
    m.mesh(b, "hullDark", [cyl(12 * S, 2 * S, at=at(14 + 3.5 * j, 0, 22), axis="x", base=True, sides=14) for j in range(4)],
           solid=False)  # the ribbed jacket
    m.mesh(b, "chrome", [cyl(11 * S, 2.5 * S, at=at(13.2, 0, 22), axis="x", base=True, sides=14)], solid=False)
    bores = [(math.cos(a) * 5.5, math.sin(a) * 5.5) for a in (j * math.pi / 3 for j in range(6))]
    m.mesh(b, "gunmetal", [cyl(2.4 * S, 30 * S, at=at(28, by, 22 + bz), axis="x", base=True, sides=8) for by, bz in bores])
    tip_x = 12 + 46
    tips = m.part("turretTips_%d" % k, parent=b, at=at(tip_x, 0, 22), hidden=True,
                  driver=d("corner turret %d's hot barrel ends: brighter while it fires" % k, ("emission", "visibility")))
    m.mesh(tips, "gatlingHeat", [cyl(2.6 * S, 3 * S, at=at(tip_x, by, 22 + bz), axis="x", base=True, sides=8)
                                 for by, bz in bores], solid=False)
    # the lit muzzle ring round the cluster's end (on the turret: steady while the barrels spin inside it)
    m.mesh(cr, "muzzleRing", [cyl(9.5 * S, 2 * S, at=at(tip_x - 6, 0, 22), axis="x", base=True, sides=16)], solid=False)
    m.part("turretMuzzles_%d" % k, parent=cr, at=at(tip_x + 4, 0, 22))
    f = m.part("turretFlashes_%d" % k, parent=cr, at=at(tip_x + 8, 0, 22), hidden=True,
               driver=d("corner turret %d's muzzle flash: a frame per round" % k, ("visibility",)))
    m.mesh(f, "glowYellow", sphere(8 * S, at=at(tip_x + 8, 0, 22), squash=(1.6, 1, 1), steps=8), solid=False)
    cradles.append((k, (x, y), cr.at, [pr for part in (cr, b, tips) for prims in part.meshes.values() for pr in prims]))


# --- the arc check: the barrels clear the hull at every yaw of the arc, at the lowest pitch --------------------------
def m4_mul(a, b):
    """Row-vector 4x4 matrices (SceneKit's, flat 16): a then b."""
    return [sum(a[i * 4 + k] * b[k * 4 + j] for k in range(4)) for i in range(4) for j in range(4)]


def m4_inv_rigid(mx):
    R = [[mx[i * 4 + j] for j in range(3)] for i in range(3)]
    t = mx[12:15]
    Rt = [[R[j][i] for j in range(3)] for i in range(3)]
    ti = [-sum(t[k] * Rt[k][j] for k in range(3)) for j in range(3)]
    return [Rt[0][0], Rt[0][1], Rt[0][2], 0, Rt[1][0], Rt[1][1], Rt[1][2], 0, Rt[2][0], Rt[2][1], Rt[2][2], 0,
            ti[0], ti[1], ti[2], 1]


def hull_pieces():
    """Every solid of the Citadel but the turrets, as (inverse world matrix, lo, hi) in SceneKit cells."""
    doc = load_catalog()
    model = next(mm for mm in doc["models"] if mm["name"] == "citadel_blue")
    parts = {p["name"]: p for p in model["parts"]}
    world = {}

    def w(p):
        if p["name"] not in world:
            mx = p["localTransform"]
            world[p["name"]] = m4_mul(mx, w(parts[p["parent"]])) if p.get("parent") else mx
        return world[p["name"]]
    out = []
    for p in model["parts"]:
        if p["name"].startswith(PREFIXES):
            continue
        for r in p.get("rays", []):
            out.append((m4_inv_rigid(m4_mul(r["transform"], w(p))), r["lo"], r["hi"], r.get("round"), r.get("radius")))
    if not out:
        raise RuntimeError("citadel_turrets: citadel_blue has no ray pieces (run citadel.py first)")
    return out


def dense(prim):
    """A prim's points, its sides sampled every few cm (a barrel over an edge must not slip between two corners)."""
    pts = prim.points()
    if isinstance(prim, Cyl):  # 16 points on each end, paired
        return [tuple(a + (b - a) * j / 8 for a, b in zip(pts[2 * i], pts[2 * i + 1])) for i in range(16) for j in range(9)]
    if isinstance(prim, Box):  # a 5 by 5 by 3 lattice in the box's own frame
        hx, hy = prim.size[0] / 2, prim.size[1] / 2
        z0, z1 = prim._zrange()
        return [m_apply((u, v, w), prim.R, prim.at) for u in (-hx, -hx / 2, 0, hx / 2, hx)
                for v in (-hy, -hy / 2, 0, hy / 2, hy) for w in (z0, (z0 + z1) / 2, z1)]
    return pts


def piece_dist(piece, p):
    """How far point p (SceneKit cells) lies outside a piece's box, at most (0 inside)."""
    inv, lo, hi = piece[0], piece[1], piece[2]
    l = [p[0] * inv[0] + p[1] * inv[4] + p[2] * inv[8] + inv[12], p[0] * inv[1] + p[1] * inv[5] + p[2] * inv[9] + inv[13],
         p[0] * inv[2] + p[1] * inv[6] + p[2] * inv[10] + inv[14]]
    return math.sqrt(sum(max(lo[i] - l[i], 0.0, l[i] - hi[i]) ** 2 for i in range(3)))


def check_arc(pieces):
    worst = (1e9, None)
    margin = CLEAR / 100.0
    for k, (tx, ty), (cx, cy, cz), prims in cradles:
        pts = [p for pr in prims for p in dense(pr)]
        # only the pieces the barrels can reach: those within their longest reach of the turret's foot
        reach = max(math.dist((p[0], p[1], p[2]), (tx, ty, cz)) for p in pts) / 100.0 + margin
        foot = sk_point((tx, ty, cz))
        near = [pc for pc in pieces if piece_dist(pc, foot) < reach]
        rest = math.degrees(math.atan2(ty, tx))  # Defence::cornerAngle(k)
        pitch = math.radians(PITCH_MIN)
        for step in range(int(2 * ARC_HALF) + 1):
            yaw = math.radians(rest - ARC_HALF + step)
            for (px, py, pz) in pts:
                # pitch about the trunnion (the cradle's pivot, across the barrels: model Y at rest), then yaw about
                # the turret's foot
                dx, dz = px - cx, pz - cz
                qx, qz = cx + dx * math.cos(pitch) - dz * math.sin(pitch), cz + dx * math.sin(pitch) + dz * math.cos(pitch)
                qy = py
                ox, oy = qx - tx, qy - ty
                wx = tx + ox * math.cos(yaw) - oy * math.sin(yaw)
                wy = ty + ox * math.sin(yaw) + oy * math.cos(yaw)
                s = sk_point((wx, wy, qz))
                for inv, lo, hi, round_, radius in near:
                    l = [s[0] * inv[0] + s[1] * inv[4] + s[2] * inv[8] + inv[12],
                         s[0] * inv[1] + s[1] * inv[5] + s[2] * inv[9] + inv[13],
                         s[0] * inv[2] + s[1] * inv[6] + s[2] * inv[10] + inv[14]]
                    if round_ == "cylinder":  # its axis is the piece's local y
                        gap = max(math.hypot(l[0], l[2]) - radius, lo[1] - l[1], l[1] - hi[1])
                    elif round_ == "sphere":
                        gap = math.sqrt(l[0] ** 2 + l[1] ** 2 + l[2] ** 2) - radius
                    else:
                        gap = max(max(lo[i] - l[i], l[i] - hi[i]) for i in range(3))
                    if gap < worst[0]:
                        worst = (gap, (k, round(math.degrees(yaw) - rest, 1), round(wx, 1), round(wy, 1), round(qz, 1)))
                    if gap < margin:
                        raise AssertionError("citadel_turrets: turret %d's barrels hit the hull at %.0f degrees from its "
                                             "corner, pitch %.0f: (%.1f, %.1f, %.1f) cm" % (
                                                 k, math.degrees(yaw) - rest, PITCH_MIN, wx, wy, qz))
    print("citadel_turrets: arc check passed: the barrels keep %.1f cm off the hull at worst (turret, yaw off the "
          "corner, x, y, z): %s" % (worst[0] * 100.0, worst[1]))


check_arc(hull_pieces())

entry = m.write(catalog=False)

# Splice the turrets into the Citadel's entries.
doc = load_catalog()
added = [p for p in entry["parts"] if p["name"].startswith(PREFIXES)]
for model in doc["models"]:
    if model["name"] not in ("citadel_blue", "citadel_red"):
        continue
    root = model["parts"][0]["name"]
    model["parts"] = [p for p in model["parts"] if not p["name"].startswith(PREFIXES)]
    for p in added:
        q = dict(p)
        q["parent"] = root if p.get("parent") == entry["parts"][0]["name"] else p["parent"]
        q["path"] = p["path"].replace("/" + entry["name"], "/" + model["name"], 1)
        model["parts"].append(q)
    print("citadel_turrets: %d parts into %s" % (len(added), model["name"]))
save_catalog(doc)
