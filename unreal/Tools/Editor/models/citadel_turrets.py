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
from ac_modelkit import Box, Cyl, Model, bar, box, cyl, m_apply, sphere, load_catalog, save_catalog, sk_point  # noqa: E402

POSE = "AcPoseBuildings.cpp:PoseCitadel"
CORNER = 620.0  # Defence::cornerInset 6.2: on the bartizans, the trunnion outside the walls
TOP = 900.0     # citadel.py PYLON_TOP (Defence::mountHeight 9.0)
ARC_HALF = 135.0  # Defence::gunArcHalf (C2: each gun covers both its walls to the foot)
PITCH_MIN = -70.0  # Defence::gunPitchMin (C2: the walls' foot)
PITCH_MAX = 50.0   # Defence::gunPitchMax
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
    # C2, the pintle (the user's minigun reference): pitched down to -70 degrees the barrels sweep the space before
    # and under the trunnion, so the yaw part is a slim bearing on the bracket's post, a low body behind the trunnion,
    # two yoke arms beside the housing and the ammunition box beside the body (self_clear() checks it over the pitch
    # range); the feed enters through the left arm's journal
    m.mesh(t, "hullDark", [cyl(5 * S, 4 * S, at=at(0, 0, 0), base=True, sides=14)])
    m.mesh(t, "chrome", [cyl(5.8 * S, 1.2 * S, at=at(0, 0, 1.4), base=True, sides=14)], solid=False)
    m.mesh(t, "hullMid", [box((19 * S, 27 * S, 8 * S), at=at(-12.5, 0, 4), base=True)])  # the body: x -22..-3
    m.mesh(t, "hullDark", [box((24 * S, 33 * S, 3 * S), at=at(-8, 0, 3), base=True)] +   # the yoke's foot plate, behind the barrels' sweep
           [box((12 * S, 3 * S, 24 * S), at=at(8, s * 13.3, 4), base=True) for s in (-1, 1)])  # its arms, 3 cm off the housing
    m.mesh(t, "chrome", [cyl(3.4 * S, 1.2 * S, at=at(8, s * 14.8 + (0 if s > 0 else -1.2), 22), axis="y", base=True,
                             sides=12) for s in (-1, 1)], solid=False)  # the trunnion's journals
    m.mesh(t, "team", [box((15 * S, 1.2 * S, 4 * S), at=at(-12.5, -14.1, 6), base=True)], solid=False)
    m.mesh(t, "hullMid", [box((18 * S, 9 * S, 12 * S), at=at(-12, 19, 4), base=True)])  # the ammunition box
    m.mesh(t, "hazard", [box((16 * S, 1.2 * S, 3 * S), at=at(-12, 24.1, 11), base=True)], solid=False)
    m.mesh(t, "hullDark", [bar(at(-6, 20, 16), at(4, 17, 26), 4 * S, 1.5 * S), bar(at(4, 17, 26), at(8, 15.4, 22), 4 * S, 1.5 * S)],
           solid=False)  # the feed chute into the left journal
    axis = at(12, 0, 22)
    cr = m.part("turretCradles_%d" % k, parent=t, at=at(8, 0, 22), hidden=True,
                driver=d("corner turret %d's guns: pitch about the trunnion (C1: the gunner's pitch)" % k,
                         ("rotation", "visibility")))
    # on the cradle: the stubby rotor housing (a rear cone, two ribs, a chrome band at its front)
    m.mesh(cr, "gunmetal", [cyl(9 * S, 12 * S, at=at(4, 0, 22), axis="x", base=True, sides=14),
                            cyl(6 * S, 4 * S, at=at(0, 0, 22), axis="x", base=True, r1=9 * S, sides=14)])
    m.mesh(cr, "hullDark", [cyl(9.6 * S, 1.2 * S, at=at(xr, 0, 22), axis="x", base=True, sides=14) for xr in (6.5, 9.5)],
           solid=False)
    m.mesh(cr, "chrome", [cyl(9.6 * S, 1.2 * S, at=at(12.6, 0, 22), axis="x", base=True, sides=14)], solid=False)
    b = m.part("turretBarrels_%d" % k, parent=cr, at=axis, hidden=True,
               driver=d("corner turret %d's barrels: spin about their axis while it fires" % k, ("rotation", "visibility")))
    # six long barrels round a spindle, a mid and a front clamp (they spin together)
    bores = [(math.cos(a) * 4.6, math.sin(a) * 4.6) for a in (j * math.pi / 3 for j in range(6))]
    tip_x = 76.2  # the muzzle (tip_x + 4) 1.3 cells along from the trunnion: Defence::muzzleAlong, where the rounds leave
    m.mesh(b, "gunmetal", [cyl(1.6 * S, (tip_x - 16) * S, at=at(16, by, 22 + bz), axis="x", base=True, sides=8)
                           for by, bz in bores])
    m.mesh(b, "hullDark", [cyl(1.9 * S, 54 * S, at=at(16, 0, 22), axis="x", base=True, sides=8)] +
           [cyl(6.4 * S, 1.6 * S, at=at(xc, 0, 22), axis="x", base=True, sides=14) for xc in (36, 56)])
    tips = m.part("turretTips_%d" % k, parent=b, at=at(tip_x, 0, 22), hidden=True,
                  driver=d("corner turret %d's hot barrel ends: brighter while it fires" % k, ("emission", "visibility")))
    m.mesh(tips, "gatlingHeat", [cyl(1.6 * S, 2 * S, at=at(tip_x, by, 22 + bz), axis="x", base=True, sides=8)
                                 for by, bz in bores], solid=False)  # butted on the barrels' ends
    # the lit flash hider collar near the muzzle (on the cradle: steady while the barrels spin inside it)
    m.mesh(cr, "muzzleRing", [cyl(7 * S, 2.5 * S, at=at(68.7, 0, 22), axis="x", base=True, sides=16)], solid=False)
    m.part("turretMuzzles_%d" % k, parent=cr, at=at(tip_x + 4, 0, 22))
    f = m.part("turretFlashes_%d" % k, parent=cr, at=at(tip_x + 8, 0, 22), hidden=True,
               driver=d("corner turret %d's muzzle flash: a frame per round" % k, ("visibility",)))
    m.mesh(f, "glowYellow", sphere(8 * S, at=at(tip_x + 8, 0, 22), squash=(1.6, 1, 1), steps=8), solid=False)
    cradles.append((k, (x, y), cr.at, [pr for part in (cr, b, tips) for prims in part.meshes.values() for pr in prims],
                    [pr for prims in t.meshes.values() for pr in prims if not getattr(pr, "decor", False)]))


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
    for k, (tx, ty), (cx, cy, cz), prims, _ in cradles:
        pts = [p for pr in prims for p in dense(pr)]
        # only the pieces the barrels can reach: those within their longest reach of the turret's foot
        reach = max(math.dist((p[0], p[1], p[2]), (tx, ty, cz)) for p in pts) / 100.0 + margin
        foot = sk_point((tx, ty, cz))
        near = [pc for pc in pieces if piece_dist(pc, foot) < reach]
        rest = math.degrees(math.atan2(ty, tx))  # Defence::cornerAngle(k)
        combos = [(math.radians(p_), math.radians(rest - ARC_HALF + y_)) for p_ in PITCHES for y_ in YAWS]
        for pitch, yaw in combos:
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
                        worst = (gap, (k, round(math.degrees(yaw) - rest, 1), round(math.degrees(pitch)), round(wx, 1),
                                       round(wy, 1), round(qz, 1)))
                    if gap < margin:
                        raise AssertionError("citadel_turrets: turret %d's barrels hit the hull at %.0f degrees from its "
                                             "corner, pitch %.0f: (%.1f, %.1f, %.1f) cm" % (
                                                 k, math.degrees(yaw) - rest, math.degrees(pitch), wx, wy, qz))
    print("citadel_turrets: arc check passed: the barrels keep %.1f cm off the hull at worst (turret, yaw off the "
          "corner, pitch, x, y, z): %s" % (worst[0] * 100.0, worst[1]))


def self_clear():
    """The pitching parts (cradle, barrels, tips) keep CLEAR cm off the gun's own yaw part's solids at every pitch (the
    yaw turns both together: the pitch alone matters)."""
    from ac_modelkit import _inside
    worst = (1e9, None)
    for k, (tx, ty), (cx, cy, cz), prims, own in cradles:
        pts = [p for pr in prims for p in dense(pr)]
        for p_ in range(int(PITCH_MIN), int(PITCH_MAX) + 1, 2):
            pitch = math.radians(p_)
            for (px, py, pz) in pts:
                dx, dz = px - cx, pz - cz
                q = (cx + dx * math.cos(pitch) - dz * math.sin(pitch), py, cz + dx * math.sin(pitch) + dz * math.cos(pitch))
                for solid in own:
                    if _inside(solid, q, -CLEAR):
                        raise AssertionError("citadel_turrets: turret %d's gun hits its own mount at pitch %d: (%.1f, %.1f, "
                                             "%.1f) cm" % (k, p_, q[0], q[1], q[2]))
    print("citadel_turrets: the gun keeps %.0f cm off its own mount at every pitch from %d to %d" % (CLEAR, PITCH_MIN,
                                                                                                     PITCH_MAX))


# the arc: every 10 degrees of pitch over the range, every 5 degrees of yaw over the arc and its ends
PITCHES = list(range(int(PITCH_MIN), int(PITCH_MAX) + 1, 10))
YAWS = sorted(set(list(range(0, int(2 * ARC_HALF) + 1, 5)) + [int(2 * ARC_HALF)]))
self_clear()
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
