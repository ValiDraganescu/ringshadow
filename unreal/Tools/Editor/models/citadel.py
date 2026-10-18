"""The Citadel (C1, docs/citadel-interior.md): the realm's keep, 12 by 12 cells, with a hall the Prospector walks into.
Replaces the SceneKit export. A fortress block: walls a cell thick and 7 high round a hall 10 by 10 by 6, the roof
slab over it, a doorway 3 by 3 on the front (+Y) closed by the `ramp` (the door standing up, a ramp once it falls
open), the roof deck at 7 with the four corner pylons (the turrets on them come from citadel_turrets.py) and the old
keep's tower, dish and beacon on its centre. Inside: the floor, eight yellow working lamps on the walls, a gun
station console under each corner gun.

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/citadel.py

then citadel_turrets.py and turret_marks.py are not needed again unless the turrets change; citadel_turrets.py
splices its parts into the entries this script writes, so run it after this one, every time.

Writes /Game/Models/citadel/SM_<part>__<material> and the catalog entries `citadel_blue` and `citadel_red` (the same
meshes), and /Game/Models/scaffold_citadel/... with `scaffold_citadel_blue` and `scaffold_citadel_red`.

UE cm, a building: front +Y (the sim's +y, toward the game camera), Z up, the ground at Z = 0, the footprint
+-600 (Rules::radius 6.0). The slabs are those of Rules::footprint (docs/citadel-interior.md section 2); cladding,
lamps and trim stand at most 8 cm proud of them. Parts the pose drives (AcPoseBuildings.cpp PoseCitadel):
`progressLamps_0..3` (the tower's windows), `queueLamps_0..4` (the tower's ring), `doorSegments_0..5` (the strip on
the lintel), `dish`, `beacon`, `light_2` (the door light), `emitter_1` (steam), the glow groups `ventGlow`,
`markerGlow`, `floodGlow`, and C1's `ramp`, `doorPanels_0` (outside) / `_1` (inside), `gunStations_k` with
`gunScreens_k`, `hallLamps_0..7`.
"""
import copy
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, bar, box, cyl, sphere, load_catalog, save_catalog, merge_entry, log  # noqa: E402

POSE = "AcPoseBuildings.cpp:PoseCitadel"
H = 600.0          # the footprint's half side
IN = 500.0         # the hall's half side (walls 100 thick)
DECK = 700.0       # the roof deck
CEIL = 600.0       # the hall's ceiling
DOOR = 150.0       # the doorway's half width
DOOR_TOP = 300.0
SINK = 100.0       # walls and floor reach this far below the ground, to hide a slope
FLOOR = 5.0        # the hall floor's top
CORNER = 550.0     # the pylons (Defence::cornerInset 5.5)
PYLON_TOP = 730.0  # citadel_turrets.py TOP
STATION = 400.0    # the gunner's point (Defence::gunStation)


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


def slab(x0, x1, y0, y1, z0, z1):
    """An axis-aligned box from its two corners."""
    return box((x1 - x0, y1 - y0, z1 - z0), at=((x0 + x1) / 2, (y0 + y1) / 2, z0), base=True)


def on_face(axis, sign, along, z, size, out=H, depth=6.0):
    """A plate on an outer face (axis 'x' or 'y', sign +-1), centred `along` the face at height z (its bottom),
    size (width along the face, height), standing `depth` proud of `out`."""
    w, h = size
    c = out * sign + sign * depth / 2
    if axis == "y":
        return box((w, depth, h), at=(along, c, z), base=True)
    return box((depth, w, h), at=(c, along, z), base=True)


m = Model("citadel", category="building", notes=[
    "C1's Citadel (docs/citadel-interior.md): 12 by 12 cells, walls 1 thick and 7 high, the hall 10 by 10 by 6 inside, "
    "the doorway 3 by 3 on the front closed by the ramp, the roof deck at 7 with the corner pylons, the tower and beacon to 9.5.",
    "Pose: PoseCitadel (production lamps, door strip, dish, glow groups, the ramp, door panels, gun stations, hall "
    "lamps, the corner turrets of citadel_turrets.py).",
    "Under construction the body rises out of the ground like every building, inside scaffold_citadel. height 9.0"])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "glow_2399ff", "glowRed", "doorGlow",
             "ventGlow", "floodGlow", "markerGlow"):
    m.material(name)
# C1's new lamps: the hall's yellow working strips (lit enough to light the hall under Lumen) and the door panels'
# amber. The base colour is the hue (the unlit master adds it: a white base washes a lamp out).
m.material("hallGlow", parent="emissive", color=(0.6, 0.42, 0.12), emissive=(18.0, 12.0, 3.4))
m.material("panelGlow", parent="emissive", color=(0.5, 0.25, 0.03), emissive=(2.4, 1.1, 0.12))
root = m.root

# --- the shell: Rules::footprint's slabs, sunk SINK into the ground --------------------------------------------
m.mesh(root, "hullMid", [
    slab(-H, -DOOR, IN, H, -SINK, DECK),       # front wall, left of the door
    slab(DOOR, H, IN, H, -SINK, DECK),         # front wall, right of the door
    slab(-DOOR, DOOR, IN, H, DOOR_TOP, DECK),  # the lintel
    slab(-H, H, -H, -IN, -SINK, DECK),         # back wall
    slab(-H, -IN, -IN, IN, -SINK, DECK),       # left wall
    slab(IN, H, -IN, IN, -SINK, DECK),         # right wall
    slab(-IN, IN, -IN, IN, CEIL, DECK),        # the roof slab
])
# the hall floor and the doorway's threshold
m.mesh(root, "hullDark", [slab(-IN, IN, -IN, IN, -SINK, FLOOR), slab(-DOOR, DOOR, IN, H, -SINK, FLOOR)])

# --- outside: corner buttresses, light cladding panels, the team band, the coping ------------------------------
def corner(sx, sy, inner, proud, z0, z1):
    """A block in the corner (sx, sy): from `inner` cm inside the footprint's edge to `proud` cm outside it."""
    xs, ys = sorted((sx * (H - inner), sx * (H + proud))), sorted((sy * (H - inner), sy * (H + proud)))
    return slab(xs[0], xs[1], ys[0], ys[1], z0, z1)


m.mesh(root, "hullDark", [corner(sx, sy, 136, 4, -SINK, DECK - 30) for sx in (-1, 1) for sy in (-1, 1)])
m.mesh(root, "hullDark", [corner(sx, sy, 150, 8, -20, 40) for sx in (-1, 1) for sy in (-1, 1)], solid=False)  # feet
panels = []
for axis in ("x", "y"):
    for sign in (-1, 1):
        front = axis == "y" and sign > 0
        spans = [(-505, -335), (-325, -185), (185, 325), (335, 505)] if front else \
            [(-505, -335), (-325, -165), (-155, -5), (5, 155), (165, 325), (335, 505)]
        for a0, a1 in spans:
            for z0, z1 in ((30, 290), (300, 560)):
                panels.append(on_face(axis, sign, (a0 + a1) / 2, z0, (a1 - a0, z1 - z0), depth=5))
m.mesh(root, "hullLight", panels, solid=False)
band = []
for axis in ("x", "y"):
    for sign in (-1, 1):
        band.append(on_face(axis, sign, 0, 590, (1060, 40), depth=7))
m.mesh(root, "team", band, solid=False)
# the coping round the deck's edge (low: the corner guns' barrels sweep over it at -20 degrees)
m.mesh(root, "hullDark", [slab(-H - 6, H + 6, min(sy * (H - 14), sy * (H + 6)), max(sy * (H - 14), sy * (H + 6)),
                               DECK - 30, DECK + 6) for sy in (-1, 1)] +
       [slab(min(sx * (H - 14), sx * (H + 6)), max(sx * (H - 14), sx * (H + 6)), -H, H, DECK - 30, DECK + 6)
        for sx in (-1, 1)], solid=False)

# --- the front: the doorway's hazard frame, the door strip on the lintel, floodlights, markers, door panels ----
m.mesh(root, "hazard", [slab(sx * DOOR - (30 if sx < 0 else 0), sx * DOOR + (30 if sx > 0 else 0), H, H + 5, 0, DOOR_TOP + 30)
                        for sx in (-1, 1)] + [slab(-DOOR - 30, DOOR + 30, H, H + 5, DOOR_TOP, DOOR_TOP + 30)], solid=False)
# the doorway's reveal lined dark, hazard strips on its jambs inside
m.mesh(root, "hullDark", [slab(sx * DOOR - (4 if sx > 0 else 0), sx * DOOR + (4 if sx < 0 else 0), IN, H, 0, DOOR_TOP)
                          for sx in (-1, 1)] + [slab(-DOOR, DOOR, IN, H, DOOR_TOP - 4, DOOR_TOP)], solid=False)
m.mesh(root, "hazard", [slab(sx * DOOR - 30 * (sx < 0), sx * DOOR + 30 * (sx > 0), IN - 5, IN, 0, DOOR_TOP + 30)
                        for sx in (-1, 1)], solid=False)
SEG_W = 42.0
for i in range(6):
    x = -125.0 + 50.0 * i
    seg = m.part("doorSegments_%d" % i, at=(x, H + 3, 352),
                 driver=d("the door strip over the doorway: chases toward the door while training", ("emission", "visibility")))
    m.mesh(seg, "doorGlow", [box((SEG_W, 5, 18), at=(x, H + 2.5, 343), base=True)], solid=False)
m.mesh(root, "hullDark", [box((300, 8, 34), at=(0, H + 4, 335), base=True)], solid=False)  # the strip's backing
# two floodlights on the lintel, over the strip, looking down on the apron
for sx in (-1, 1):
    m.mesh(root, "hullDark", [box((70, 16, 30), at=(sx * 95, H + 8, 400), base=True)], solid=False)
    m.mesh(root, "floodGlow", [box((60, 3, 16), at=(sx * 95, H + 16.5, 404), base=True)], solid=False)
m.mesh(root, "markerGlow", [box((12, 4, 12), at=(sx * 230, H + 2, 300), base=True) for sx in (-1, 1)] +
       [box((10, 10, 10), at=(sx * (H + 6), sy * (H + 6), DECK - 22), base=True) for sx in (-1, 1) for sy in (-1, 1)],
       solid=False)
# the door panels: a dark frame each side of the doorway (right of it), the lit face the pose drives
PANEL_X, PANEL_Z = 225.0, 120.0
for k, (y, s) in enumerate(((H, 1), (IN, -1))):
    m.mesh(root, "hullDark", [box((44, 6, 56), at=(PANEL_X, y + s * 3, PANEL_Z - 28), base=True)], solid=False)
    p = m.part("doorPanels_%d" % k, at=(PANEL_X, y + s * 7, PANEL_Z),
               driver=d("the door panel %s: lit while a pilot is in reach of the door" % ("outside" if k == 0 else "inside")))
    m.mesh(p, "panelGlow", [box((32, 2, 40), at=(PANEL_X, y + s * 7, PANEL_Z - 20), base=True)], solid=False)
m.part("light_2", at=(0, H + 100, 250), handles=["doorLight"],
       light={"attenuationEnd": "6.0", "color": "SIMD4<Float>(1.0, 0.6600805, 0.19007224, 1.0)", "handles": "doorLight",
              "intensity": "0.0", "type": "omni"},
       driver=d("the door light: on while a unit trains", ("light",)))

# --- the ramp: the door standing in the doorway, hinged at its outer bottom edge --------------------------------
ramp = m.part("ramp", at=(0, H, 0), driver=d(
    "the door: turns about its hinge (the doorway's outer bottom edge, local X) by door.open x 90 degrees, its top "
    "outward (+Y), to lie on the ground as a ramp", ("rotation",)))
m.mesh(ramp, "hullLight", [slab(-DOOR + 4, DOOR - 4, H - 30, H, 0, DOOR_TOP - 4)])
m.mesh(ramp, "hullDark", [slab(-DOOR + 4, DOOR - 4, H - 34, H - 30, 20 + 46 * j, 30 + 46 * j) for j in range(6)] +
       [slab(-DOOR + 20 + 65 * j, -DOOR + 30 + 65 * j, H, H + 3, 10, DOOR_TOP - 20) for j in range(5)], solid=False)
m.mesh(ramp, "hazard", [slab(-DOOR + 4, DOOR - 4, H, H + 2, 0, 18), slab(-DOOR + 4, DOOR - 4, H, H + 2, DOOR_TOP - 22, DOOR_TOP - 4)],
       solid=False)

# --- the sides and back: vents (ventGlow) -----------------------------------------------------------------------
vents_dark, vents_glow = [], []
for axis, sign, spots in (("x", 1, (-300, 0, 300)), ("x", -1, (-300, 0, 300)), ("y", -1, (-250, 250))):
    for a in spots:
        vents_dark.append(on_face(axis, sign, a, 160, (130, 70), depth=8))
        for j in range(3):
            vents_glow.append(on_face(axis, sign, a, 172 + 18 * j, (110, 8), depth=10))
m.mesh(root, "hullDark", vents_dark, solid=False)
m.mesh(root, "ventGlow", vents_glow, solid=False)

# --- inside: baseboards, ceiling beams, floor markings, the eight hall lamps -----------------------------------
m.mesh(root, "hullDark", [slab(-IN, -DOOR, IN - 4, IN, FLOOR, 60), slab(DOOR, IN, IN - 4, IN, FLOOR, 60),
                          slab(-IN, IN, -IN, -IN + 4, FLOOR, 60), slab(-IN, -IN + 4, -IN, IN, FLOOR, 60),
                          slab(IN - 4, IN, -IN, IN, FLOOR, 60)], solid=False)
m.mesh(root, "hullDark", [slab(-IN, IN, y - 20, y + 20, CEIL - 45, CEIL) for y in (-250, 0, 250)] +
       [slab(x - 15, x + 15, -IN, IN, CEIL - 30, CEIL) for x in (-250, 250)], solid=False)
m.mesh(root, "hazard", [slab(sx * (DOOR - 10) - 6, sx * (DOOR - 10) + 6, 0, IN, FLOOR, FLOOR + 1) for sx in (-1, 1)],
       solid=False)  # the walk in from the door
m.mesh(root, "team", [cyl(130, 1, at=(0, -60, FLOOR), base=True, sides=32)], solid=False)
m.mesh(root, "hullDark", [cyl(105, 1.5, at=(0, -60, FLOOR), base=True, sides=32)], solid=False)
HALL_LAMPS = [((-330, IN), "y", -1), ((330, IN), "y", -1), ((IN, 250), "x", -1), ((IN, -250), "x", -1),
              ((250, -IN), "y", 1), ((-250, -IN), "y", 1), ((-IN, -250), "x", 1), ((-IN, 250), "x", 1)]
LAMP_Z = 430.0
for k, ((x, y), axis, inward) in enumerate(HALL_LAMPS):
    if axis == "y":
        housing = box((172, 16, 40), at=(x, y + inward * 8, LAMP_Z - 20), base=True)
        strip = box((160, 4, 26), at=(x, y + inward * 18, LAMP_Z - 13), base=True)
    else:
        housing = box((16, 172, 40), at=(x + inward * 8, y, LAMP_Z - 20), base=True)
        strip = box((4, 160, 26), at=(x + inward * 18, y, LAMP_Z - 13), base=True)
    m.mesh(root, "hullDark", [housing], solid=False)
    lamp = m.part("hallLamps_%d" % k, at=strip.at, driver=d("hall lamp %d: the yellow working light, steady once built" % k))
    m.mesh(lamp, "hallGlow", [strip], solid=False)

# --- the gun stations: a console under each corner gun, its screen toward the gunner's point ------------------
for k, (sx, sy) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
    diag = (sx / math.sqrt(2), sy / math.sqrt(2))
    c = (sx * STATION + diag[0] * 60, sy * STATION + diag[1] * 60)
    yaw = math.degrees(math.atan2(-sy, -sx))  # local +X toward the gunner (the hall's middle)
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def L(lx, ly, lz):
        return (c[0] + lx * cy_ - ly * sy_, c[1] + lx * sy_ + ly * cy_, lz)

    st = m.part("gunStations_%d" % k, at=(c[0], c[1], FLOOR),
                driver=d("gun station %d's console (corner %d's gun): still" % (k, k), ()))
    m.mesh(st, "hullDark", [box((44, 80, 92), at=L(-6, 0, FLOOR), rot=(0, yaw, 0), base=True)])
    m.mesh(st, "hullMid", [box((62, 96, 10), at=L(4, 0, FLOOR + 92), rot=(-8, yaw, 0), base=True),
                           box((10, 90, 64), at=L(-22, 0, FLOOR + 100), rot=(14, yaw, 0), base=True)])
    m.mesh(st, "team", [box((2, 60, 10), at=L(16.5, 0, FLOOR + 60), rot=(0, yaw, 0), base=True)], solid=False)
    # the conduit up to the gun over the corner
    m.mesh(st, "hullDark", [cyl(7, CEIL - FLOOR - 120, at=L(-24, 0, FLOOR + 120), base=True, sides=10)], solid=False)
    m.mesh(st, "chrome", [cyl(9, 4, at=L(-24, 0, z), base=True, sides=10) for z in (200, 320, 440, 560)], solid=False)
    m.mesh(st, "hazard", [cyl(70, 1, at=(sx * STATION, sy * STATION, FLOOR), base=True, sides=24)], solid=False)
    m.mesh(st, "hullDark", [cyl(60, 1.5, at=(sx * STATION, sy * STATION, FLOOR), base=True, sides=24)], solid=False)
    sc_at = L(-16, 0, FLOOR + 132)
    scr = m.part("gunScreens_%d" % k, parent=st, at=sc_at,
                 driver=d("gun station %d's screen: blue, lit while the gun is manned" % k))
    m.mesh(scr, "glow_2399ff", [box((2, 76, 46), at=L(-16, 0, FLOOR + 109), rot=(14, yaw, 0), base=True)], solid=False)

# --- the roof deck: plating, the pylons, the tower, the dish and the steam stack --------------------------------
m.mesh(root, "hullMid", [slab(-H + 14, H - 14, -H + 14, H - 14, DECK, DECK + 3)])
m.mesh(root, "hullDark", [slab(-H + 14, H - 14, a - 3, a + 3, DECK + 3, DECK + 4) for a in (-300, 0, 300)] +
       [slab(a - 3, a + 3, -H + 14, H - 14, DECK + 3, DECK + 4) for a in (-300, 0, 300)], solid=False)  # seams
for sx in (-1, 1):
    for sy in (-1, 1):
        x, y = sx * CORNER, sy * CORNER
        m.mesh(root, "hullDark", [cyl(56, PYLON_TOP - DECK - 6, at=(x, y, DECK + 3), base=True, sides=16)])
        m.mesh(root, "chrome", [cyl(58, 5, at=(x, y, PYLON_TOP - 5), base=True, sides=16)], solid=False)
        m.mesh(root, "hazard", [cyl(57, 7, at=(x, y, DECK + 8), base=True, sides=16)], solid=False)

# the tower (the old keep's): an octagon with the progress windows, a chrome ring, a drum with the queue ring,
# a dome and the beacon's mast
TR = 240.0                 # the octagon's corner radius
TRI = TR * math.cos(math.pi / 8)
T0, T1 = DECK + 3, 800.0
m.mesh(root, "hullMid", [cyl(TR, T1 - T0, at=(0, 0, T0), base=True, sides=8, rot=(0, 22.5, 0))])
m.mesh(root, "hullLight", [cyl(TR + 4, 30, at=(0, 0, T0), base=True, sides=8, rot=(0, 22.5, 0))], solid=False)
m.mesh(root, "team", [cyl(TR + 3, 12, at=(0, 0, T1 - 22), base=True, sides=8, rot=(0, 22.5, 0))], solid=False)
m.mesh(root, "chrome", [cyl(TR + 8, 8, at=(0, 0, T1), base=True, sides=8, rot=(0, 22.5, 0))])
DRUM = 150.0
m.mesh(root, "hullLight", [cyl(DRUM, 50, at=(0, 0, T1 + 8), base=True, sides=32)])
m.mesh(root, "glow_2399ff", [cyl(DRUM + 1, 18, at=(0, 0, T1 + 20), base=True, sides=32)], solid=False)  # dark glass
m.mesh(root, "hullMid", [sphere(DRUM, at=(0, 0, T1 + 58), squash=(1, 1, 0.35), steps=24)])
m.mesh(root, "chrome", [cyl(DRUM + 3, 5, at=(0, 0, T1 + 56), base=True, sides=32)], solid=False)
m.mesh(root, "chrome", [cyl(4, 36, at=(0, 0, T1 + 104), base=True, sides=8)], solid=False)
beacon = m.part("beacon", at=(0, 0, T1 + 146), driver=d("beacon blink (0.25 s of every 1.2 s once built)", ("visibility",)))
m.mesh(beacon, "glowRed", sphere(9, at=(0, 0, T1 + 146), steps=10), solid=False)
# progress windows on the octagon's faces toward the camera, screen left to right (-X first)
WIN_Z = 750.0
for k, (ang, off) in enumerate(((135, 0), (90, -55), (90, 55), (45, 0))):
    a = math.radians(ang)
    n = (math.cos(a), math.sin(a))
    px = n[0] * (TRI + 2) + (off if ang == 90 else 0)
    py = n[1] * (TRI + 2)
    lamp = m.part("progressLamps_%d" % k, at=(px, py, WIN_Z), rot=(0, ang - 90, 0),
                  driver=d("training progress: lamp k on when k < progress x 4, the next blinking"))
    m.mesh(lamp, "glow_2399ff", [box((70, 4, 34), at=(px, py, WIN_Z - 17), rot=(0, ang - 90, 0), base=True)], solid=False)
# the queue ring: five segments round the drum's front, screen left to right
for k, ang in enumerate((161, 125.5, 90, 54.5, 19)):
    a = math.radians(ang)
    px, py = math.cos(a) * (DRUM + 3), math.sin(a) * (DRUM + 3)
    q = m.part("queueLamps_%d" % k, at=(px, py, T1 + 29), rot=(0, ang - 90, 0),
               driver=d("the build queue: one segment lit per queued item"))
    m.mesh(q, "glow_2399ff", [box((46, 4, 20), at=(px, py, T1 + 19), rot=(0, ang - 90, 0), base=True)], solid=False)
# the dish on its mast, back right; the pose turns it about the mast
DISH_AT = (330.0, -300.0, 800.0)
m.mesh(root, "hullDark", [cyl(30, 20, at=(DISH_AT[0], DISH_AT[1], DECK + 3), base=True, sides=12)])
m.mesh(root, "chrome", [cyl(9, DISH_AT[2] - DECK - 20, at=(DISH_AT[0], DISH_AT[1], DECK + 20), base=True, sides=10)])
dish = m.part("dish", at=DISH_AT, driver=d("the dish: turns slowly once built", ("rotation",)))
m.mesh(dish, "chrome", [cyl(12, 26, at=DISH_AT, rot=(30, 0, 0), base=True, r1=80, sides=20, axis="x"),
                        cyl(3, 70, at=DISH_AT, rot=(30, 0, 0), base=True, sides=6, axis="x")], solid=False)
m.mesh(dish, "hullDark", [box((20, 24, 24), at=(DISH_AT[0] - 10, DISH_AT[1], DISH_AT[2] - 12), base=True)], solid=False)
# the steam stack, back left: emitter_1 at its mouth
STACK = (-330.0, -300.0)
m.mesh(root, "hullDark", [cyl(34, 90, at=(STACK[0], STACK[1], DECK + 3), base=True, sides=16)])
m.mesh(root, "ventGlow", [cyl(35, 8, at=(STACK[0], STACK[1], DECK + 60), base=True, sides=16)], solid=False)
m.mesh(root, "chrome", [cyl(37, 6, at=(STACK[0], STACK[1], DECK + 90), base=True, sides=16)], solid=False)
m.part("emitter_1", at=(STACK[0], STACK[1], DECK + 100), particles={"handles": "steam"},
       driver=d("steam from the stack: more while training", ("particles",)))

entry = m.write()


# --- the scaffold: poles round the walls, rails, braces ------------------------------------------------------------
s = Model("scaffold_citadel", category="construction", notes=[
    "Construction scaffold for the citadel (C1), radius 6.3, height 7.6; shown until the building is complete."])
for name in ("chrome", "hazard"):
    s.material(name)
R = H + 25.0
TOPZ = 760.0
poles, rails, bands = [], [], []
ticks = [-R + k * (2 * R / 6) for k in range(7)]
pts = sorted({(round(x, 3), round(sy * R, 3)) for x in ticks for sy in (-1, 1)} |
             {(round(sx * R, 3), round(y, 3)) for y in ticks for sx in (-1, 1)})
for x, y in pts:
    poles.append(cyl(6, TOPZ, at=(x, y, 0), base=True, sides=6))
    bands.append(cyl(9, 30, at=(x, y, 0), base=True, sides=6))
for z in (150.0, 300.0, 450.0, 600.0, TOPZ - 6):
    for sy in (-1, 1):
        rails.append(box((2 * R + 12, 8, 8), at=(0, sy * R, z), base=True))
    for sx in (-1, 1):
        rails.append(box((8, 2 * R + 12, 8), at=(sx * R, 0, z), base=True))
braces = []
for i in range(6):
    a, b = ticks[i], ticks[i + 1]
    for lo, hi in ((0.0, 300.0), (300.0, 600.0)):
        for sy in (-1, 1):
            if sy > 0 and abs((a + b) / 2) < DOOR + 60:
                continue  # leave the doorway clear
            braces.append(bar((a, sy * R, lo), (b, sy * R, hi), 6, shape="cyl", sides=6))
        for sx in (-1, 1):
            braces.append(bar((sx * R, a, lo), (sx * R, b, hi), 6, shape="cyl", sides=6))
s.mesh(s.root, "chrome", poles + rails + braces)
s.mesh(s.root, "hazard", bands + [box((2 * R, 10, 10), at=(0, sy * R, TOPZ), base=True) for sy in (-1, 1)], solid=False)
scaffold = s.write()


# --- the red entries: the same meshes, team 1 ---------------------------------------------------------------------
def red_of(blue):
    base = blue["base"]
    b, r = base + "_blue", base + "_red"
    red = copy.deepcopy(blue)
    red["name"], red["team"] = r, "red"
    for p in red["parts"]:
        if p["name"] == b + "_root":
            p["name"] = r + "_root"
        if p.get("parent") == b + "_root":
            p["parent"] = r + "_root"
        p["path"] = "/" + r + p["path"][len("/" + b):]
        for x in p["meshes"]:
            x["prim"] = x["prim"].replace(b + "_root__", r + "_root__")
    return red


doc = load_catalog()
for blue in (entry, scaffold):
    log("catalog: %s %s" % (merge_entry(doc, red_of(blue)), blue["base"] + "_red"))
save_catalog(doc)
