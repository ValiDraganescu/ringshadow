"""The Mule: the hauler a Prospector buys at the Foundry and drives standing on its front perch (C4,
docs/haul-and-ferry.md sections 2 and 4; concept H2, art/models/cart/concepts/cart-hauler-2.jpg). Six big knobbly
wheels, a riveted gunmetal and deep blue body, hazard stripes, an open ore hopper at the back, two silver MH tanks
with gauges at the flanks, two levers on a railed perch, a headlamp bar, an orange beacon, and a load screen on each
flank (room left of 1000, ore, MH).

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/mule.py

Writes /Game/Models/mule/SM_<part>__<material> and the catalog entry `mule_blue`. The screen's picture comes from
mule_screen.py, its live numbers from the readout material (make_readout_material.py).

UE cm, +X forward, +Y right, Z up, the wheels on Z = 0; ac::Mule's numbers (core Mule.h): 300 long (+-150), 160 wide
(+-80), the body's top 140, the perch deck 35 up (the driver, a Prospector 147 tall, stands on it: its top at 182),
the perch point (+100, 0), the hopper's centre (-55, 0) and its rim 130 up. The driver is the rider unit itself, drawn
by the game on `perch` (its feet on the deck, its gripper and drill on the levers' knobs).

Parts the pose moves (AcPoseVehicles.cpp:PoseMule): `steers_0/1` (the front pair's steering, left and right) with
`wheels_0/1` under them, `wheels_2..5` (the rear tandem: 2 and 3 the middle axle, 4 and 5 the back one; even left);
`levers_0/1` (left and right, pushed with the drive and the turn); `heap` (the ore in the hopper, grown with the load
from its floor: hidden while empty); `mhLevel_0/1` (the MH in each tank's sight glass, grown from its foot) and
`needles_0/1` (the tanks' dials); `beacon` (the orange beacon, turning while it drives) and `headlamps`; the screens'
readouts `roomDigits_k`, `roomBar_k`, `oreDigits_k`, `mhDigits_k` (k 0 left, 1 right; the part's emission is the number:
make_readout_material.py) and `screens` (the screens' fixed face, its emission its brightness). `perch` is the socket
the driver stands on; `exhaust` the stack's top (smoke); `dust_0/1` behind the back wheels (the dust trail).
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
from ac_modelkit import Model, box, cyl, panel, sphere, prism, bar  # noqa: E402
import mule_screen  # noqa: E402

POSE = "AcPoseVehicles.cpp:PoseMule"

# --- the numbers the core shares (ac::Mule) and the layout ------------------------------------------------------------
DECK = 35.0          # the perch deck's top: the driver's feet
DRIVER_X = 74.0      # where the driver stands (its pivot), its levers' knobs before it
WHEEL_R = 34.0       # the tyres' radius over the lugs
TYRE_R = 31.0
TYRE_W = 22.0
WHEEL_Y = 66.0       # the wheels' middle plane (the tyres' outer face at 77, the hubs' 78.2)
AXLES = (90.0, -40.0, -110.0)  # front, middle, back
HOP_X0, HOP_X1 = -146.0, 20.0  # the hopper's outside, back and front
HOP_Y = 70.0         # its side walls' outer face
HOP_Z0, HOP_RIM = 72.0, 130.0
FLOOR = HOP_Z0 + 4.0  # the hopper's floor, the heap's foot
TANK_X, TANK_Y, TANK_R = 38.0, 62.0, 14.0
TANK_Z0, TANK_Z1 = 42.0, 124.0
LEVER_X, LEVER_Y = 118.0, 50.0
SCREEN_X, SCREEN_Z = -84.0, 93.0  # the screen's middle on the hopper's flank
SCREEN_W, SCREEN_H = 60.0, 36.0

m = Model("mule", category="unit", coplanar_check=True, notes=[
    "C4's Mule (docs/haul-and-ferry.md): the hauler a Prospector drives standing on the perch. Pose: PoseMule "
    "(wheels roll and the front pair steers, the levers move with the drive, the heap grows with the load, the MH "
    "gauges, the screens' readouts, the beacon turns while it drives).",
    "Parts: steers_0/1 > wheels_0/1, wheels_2..5, levers_0/1, heap (hidden while empty), mhLevel_0/1, needles_0/1, "
    "beacon, headlamps, screens, roomDigits_0/1, roomBar_0/1, oreDigits_0/1, mhDigits_0/1, perch, exhaust, dust_0/1."])
for name in ("gunmetal", "hullDark", "hullMid", "team", "hazard", "chrome", "tyre", "glowOrange", "glowRed",
             "floodGlow", "oreRock"):
    m.material(name)
# the screens: the fixed face (mule_screen.png) lit, and the readouts on the readout master
m.material("muleScreen", parent="emissive", color=(0, 0, 0), emissive=(1.0, 1.0, 1.0),
           emissive_texture="unreal/Resources/Textures/mule_screen.png")
READOUTS = {
    "roomDigits": dict(Mode=0, Cells=4, OnColor=(0.55, 1.6, 2.2), OffColor=(0.012, 0.03, 0.04)),
    "roomBar": dict(Mode=1, Cells=10, OnColor=(0.35, 1.8, 0.55), OffColor=(0.01, 0.035, 0.015)),
    "oreDigits": dict(Mode=0, Cells=4, OnColor=(2.2, 1.15, 0.25), OffColor=(0.04, 0.025, 0.01)),
    "mhDigits": dict(Mode=0, Cells=4, OnColor=(0.45, 1.5, 2.4), OffColor=(0.012, 0.03, 0.045)),
}
for name, params in READOUTS.items():
    m.material("readout_" + name, parent="readout", color=(0, 0, 0), params=params)
# Stardust Ore's crystals in the heap, violet and lit from inside (the concept's), dim enough to keep their hue
m.material("muleCrystal", parent="emissive", color=(0.3, 0.08, 0.62), emissive=(0.5, 0.13, 1.05))
# the mercury of a tank's sight glass, cold and silver (the Derrick's MH look)
m.material("muleMercury", parent="emissive", color=(0.55, 0.62, 0.7), emissive=(0.5, 0.75, 1.1))


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


def sides():
    return ((0, -1.0), (1, 1.0))  # (k, the side's sign): 0 left (-Y), 1 right


# --- the chassis: a spine between the wheels, axles, bumpers ---------------------------------------------------------
m.mesh(m.root, "hullDark", [box((176, 80, 20), at=(-58, 0, 26), base=True),  # under the hopper
                            box((96, 80, 12), at=(78, 0, 18), base=True)])   # under the perch deck
for x in AXLES:
    if x < 0:  # (the front pair's stubs only: an axle across would stand up through the perch deck)
        m.mesh(m.root, "hullMid", [cyl(5, 2 * (WHEEL_Y - TYRE_W / 2) - 2, at=(x, 0, WHEEL_R), axis="y", sides=10)], solid=False)
    else:
        m.mesh(m.root, "hullMid", [cyl(5, 18, at=(x, s * (WHEEL_Y - TYRE_W / 2 - 9), WHEEL_R), axis="y", sides=10)
                                   for _, s in sides()], solid=False)
# the back bumper, hazard striped, and the tail lamps over it
m.mesh(m.root, "hazard", [box((5, 140, 16), at=(-147.5, 0, 24), base=True)])
m.mesh(m.root, "glowRed", [box((2, 12, 5), at=(-147, s * 58, 45), base=True) for _, s in sides()], solid=False)

# --- the wheels: knobbly tyres on dark rims, chrome hubs --------------------------------------------------------------
LUGS = 18


def wheel(part, x, s):
    y = s * WHEEL_Y
    m.mesh(part, "tyre", cyl(TYRE_R, TYRE_W, at=(x, y, WHEEL_R), axis="y", sides=20))
    # each lug turned to the tyre's tangent: built along the tread, pitched by its angle round the axle
    lugs = [box((5, 11, 3.6), at=(x + math.sin(a) * (TYRE_R + 1.3), y + (4.5 if j % 2 else -4.5),
                                  WHEEL_R + math.cos(a) * (TYRE_R + 1.3)), rot=(-math.degrees(a), 0, 0))
            for j, a in ((j, 2 * math.pi * (j + 0.5) / LUGS) for j in range(LUGS))]
    m.mesh(part, "tyre", lugs, solid=False)
    m.mesh(part, "hullDark", cyl(19, TYRE_W + 1.4, at=(x, y, WHEEL_R), axis="y", sides=16), solid=False)
    m.mesh(part, "chrome", cyl(7.5, TYRE_W + 2.8, at=(x, y, WHEEL_R), axis="y", sides=10), solid=False)
    # five wheel nuts on the outer face
    m.mesh(part, "hullMid", [cyl(1.6, 2.2, at=(x + math.sin(b) * 12.5, y + s * (TYRE_W / 2 + 1.0), WHEEL_R + math.cos(b) * 12.5),
                                 axis="y", sides=6) for b in (2 * math.pi * i / 5 for i in range(5))], solid=False)


for k, s in sides():
    st = m.part("steers_%d" % k, at=(AXLES[0], s * WHEEL_Y, WHEEL_R),
                driver=d("the front %s wheel's steering (yaw with the turn)" % ("left", "right")[k]))
    w = m.part("wheels_%d" % k, parent=st, at=(AXLES[0], s * WHEEL_Y, WHEEL_R),
               driver=d("front %s wheel: rolls with the distance driven" % ("left", "right")[k]))
    wheel(w, AXLES[0], s)
for i, (x, s) in enumerate(((AXLES[1], -1.0), (AXLES[1], 1.0), (AXLES[2], -1.0), (AXLES[2], 1.0))):
    w = m.part("wheels_%d" % (i + 2), at=(x, s * WHEEL_Y, WHEEL_R),
               driver=d("rear wheel (%s axle, %s): rolls with the distance driven" % (("middle", "back")[i // 2],
                                                                                    ("left", "right")[i % 2])))
    wheel(w, x, s)

# --- the fenders over the wheels: dark plates, hazard skirts at their ends ---------------------------------------------
for _, s in sides():
    y0 = s * (WHEEL_Y + TYRE_W / 2 + 0.8)  # the outer edge, 0.8 past the tyres
    yc = (y0 + s * 54) / 2
    yw = abs(y0) - 54
    # the rear fender runs under the hopper, the front one over the front wheels
    m.mesh(m.root, "gunmetal", [box((150, yw, 4), at=(-73, yc, 68), base=True), box((80, yw, 4), at=(90, yc, 70), base=True)])
    # hazard skirts slanting down at the fenders' ends
    for x0, x1, z in ((-144, -147, 68), (2, 6, 68), (50, 46, 70), (130, 134, 70)):
        m.mesh(m.root, "hazard", [prism([(x0, z), (x1, z), (x1 + (x1 - x0) * 0.5, z - 16), (x0 + (x1 - x0) * 0.2, z - 16)],
                                        yw - 2, "xz", at=(0, yc, 0))], solid=False)
    # a hazard band along the front fender's top edge, past the rail posts
    m.mesh(m.root, "hazard", [box((76, 7, 1.2), at=(90, s * 75.5, 74), base=True)], solid=False)

# --- the hopper: deep blue riveted walls on a gunmetal frame, open at the top -----------------------------------------
HL = HOP_X1 - HOP_X0
HC = (HOP_X0 + HOP_X1) / 2
m.mesh(m.root, "gunmetal", [box((HL, 2 * HOP_Y, 4), at=(HC, 0, HOP_Z0), base=True)])  # the floor
for _, s in sides():
    m.mesh(m.root, "team", [box((HL - 8, 4, HOP_RIM - HOP_Z0 - 4), at=(HC, s * (HOP_Y - 2), HOP_Z0 + 4), base=True)])
for x in (HOP_X0 + 2, HOP_X1 - 2):
    m.mesh(m.root, "team", [box((4, 2 * HOP_Y, HOP_RIM - HOP_Z0 - 4), at=(x, 0, HOP_Z0 + 4), base=True)])
# the rim: a gunmetal rail along the top, its corners butted (the sides between the ends)
m.mesh(m.root, "gunmetal", [box((6, 2 * HOP_Y + 4, 5), at=(x, 0, HOP_RIM), base=True) for x in (HOP_X0 + 2, HOP_X1 - 2)] +
       [box((HL - 12, 6, 5), at=(HC, s * (HOP_Y - 1), HOP_RIM), base=True) for _, s in sides()])
# the flared lip along each flank's top (the concept's hopper), a gunmetal edge on it
LIP = 116.0
for _, s in sides():
    m.mesh(m.root, "team", [bar((HC, s * (HOP_Y + 1.5), LIP), (HC, s * (HOP_Y + 6.5), HOP_RIM), 3, HL - 12, side=(1, 0, 0))])
    m.mesh(m.root, "gunmetal", [cyl(2.0, HL - 10, at=(HC, s * (HOP_Y + 7.0), HOP_RIM + 0.8), axis="x", sides=8)], solid=False)
# ribs down the flanks (up to the lip) and the ends, 2 proud, with rivets
RIBS = (-136.0, -118.0, -50.0, -20.0, 10.0)
for _, s in sides():
    m.mesh(m.root, "gunmetal", [box((5, 2, LIP - HOP_Z0 - 4), at=(x, s * (HOP_Y + 1), HOP_Z0 + 4), base=True) for x in RIBS])
    m.mesh(m.root, "chrome", [sphere(1.1, at=(x, s * (HOP_Y + 2.2), z), steps=5) for x in RIBS for z in (82, 96, 110)], solid=False)
    # a riveted seam along the bottom of each flank
    m.mesh(m.root, "chrome", [sphere(1.0, at=(x, s * (HOP_Y + 0.6), 77), steps=5)
                              for x in range(int(HOP_X0) + 12, int(HOP_X1) - 4, 12)], solid=False)
for x, sx in ((HOP_X0, -1.0), (HOP_X1, 1.0)):
    m.mesh(m.root, "gunmetal", [box((2, 5, HOP_RIM - HOP_Z0 - 8), at=(x + sx * 1.0, y, HOP_Z0 + 4), base=True)
                                for y in (-42.0, 0.0, 42.0)])

# --- the screens on the hopper's flanks: a dark bezel, the picture, the readouts in its wells -------------------------
m.mesh(m.root, "hullDark", [box((SCREEN_W + 8, 3.0, SCREEN_H + 8), at=(SCREEN_X, s * (HOP_Y + 1.5), SCREEN_Z)) for _, s in sides()])
FACE = HOP_Y + 3.0  # the bezel's face; the picture 0.8 thick on it, the readouts 0.7 proud of the picture
screens = m.part("screens", at=(SCREEN_X, 0, SCREEN_Z),
                 driver=d("the screens' fixed face: emission its brightness (dim while parked, lit while driven or near)",
                          ("emission",)))
for k, s in sides():
    m.mesh(screens, "muleScreen", panel((0.8, SCREEN_W, SCREEN_H), at=(SCREEN_X, s * (FACE + 0.4), SCREEN_Z), rot=(0, s * 90, 0)),
           solid=False)


def well_at(s, w):
    """A well's middle and size: the picture's u runs toward the viewer's right (the Mule's front on its right flank,
    its back on the left), v down."""
    u0, v0, u1, v1 = w
    u, v = (u0 + u1) / 2 - 0.5, (v0 + v1) / 2 - 0.5
    return (SCREEN_X + s * u * SCREEN_W, SCREEN_Z - v * SCREEN_H), ((u1 - u0) * SCREEN_W - 1.0, (v1 - v0) * SCREEN_H - 1.0)


for name, well in (("roomDigits", "room"), ("roomBar", "bar"), ("oreDigits", "ore"), ("mhDigits", "mh")):
    for k, s in sides():
        (x, z), (w, h) = well_at(s, mule_screen.WELLS[well])
        p = m.part("%s_%d" % (name, k), at=(x, s * FACE, z), driver=d(
            "%s readout, %s flank: the part's emission is the number shown (make_readout_material.py)" %
            (name, ("left", "right")[k]), ("emission",)))
        m.mesh(p, "readout_" + name, panel((0.6, w, h), at=(x, s * (FACE + 0.8 + 0.7 + 0.3), z), rot=(0, s * 90, 0)), solid=False)

# --- the heap: Stardust Ore in the hopper, dark rock and violet crystals, grown from the floor with the load ----------
heap = m.part("heap", at=(HC, 0, FLOOR), hidden=True, driver=d(
    "the ore in the hopper: shown with any load, scaled up from the floor (Z by the load's share of 1000, X and Y a little)",
    ("visibility", "scale")))
IN_X, IN_Y = HL / 2 - 6, HOP_Y - 6
BED = 40.0  # the bed's top over the floor at a full load, the mound and the crystals over it
m.mesh(heap, "oreRock", [box((2 * IN_X - 2, 2 * IN_Y - 2, BED), at=(HC, 0, FLOOR), base=True)])
rnd = random.Random(4)


def mound(x, y):
    """The heap's top over the floor at (x, y) from the hopper's middle: a dome over the bed."""
    return BED + 24 * max(0.0, 1 - (x / IN_X) ** 2) * max(0.0, 1 - (y / IN_Y) ** 2)


rocks = []
for i in range(26):
    x, y = rnd.uniform(-0.8, 0.8) * IN_X, rnd.uniform(-0.75, 0.75) * IN_Y
    r = rnd.uniform(12, 22)
    # each turned its own way (two alike rocks shared buried faces: the mesh check reads them)
    rocks.append(sphere(r, at=(HC + x, y, FLOOR + mound(x, y) - r * 0.55), squash=(1.4, 1.2, 0.6), steps=8,
                        rot=(rnd.uniform(-6, 6), rnd.uniform(0, 360), rnd.uniform(-6, 6))))
m.mesh(heap, "oreRock", rocks)
# the crystals in clusters, the tallest in the middle (the concept's violet spray)
for cx, cy, n, tall in ((-10, 0, 6, 46), (-50, -24, 4, 32), (36, 20, 4, 34), (56, -26, 3, 24), (-62, 30, 3, 24), (6, -40, 3, 22)):
    for j in range(n):
        x, y = cx + rnd.uniform(-12, 12), cy + rnd.uniform(-10, 10)
        h = tall * rnd.uniform(0.55, 1.0)
        r = rnd.uniform(4.5, 9.5) * (h / 40) ** 0.5
        foot = (HC + x, y, FLOOR + mound(x, y) - 6)
        tip = (HC + x + rnd.uniform(-0.35, 0.35) * h, y + rnd.uniform(-0.35, 0.35) * h, FLOOR + mound(x, y) - 6 + h)
        m.mesh(heap, "muleCrystal", [bar(foot, tip, r * 2, shape="cyl", taper=0.2, sides=6)], solid=False)

# --- the perch: a tread deck between the front wheels, railed, two levers at its front -------------------------------
m.mesh(m.root, "gunmetal", [box((90, 106, 5), at=(80, 0, DECK - 5), base=True)])
m.mesh(m.root, "hullMid", [box((3, 90, 1.2), at=(x, 0, DECK), base=True) for x in range(42, 112, 9)], solid=False)  # tread bars
perch = m.part("perch", at=(DRIVER_X, 0, DECK), driver=dict(
    what="the driver's place: the rider (a Prospector) stands here, its feet on the deck, facing +X",
    channels=["position"], functions=[POSE]))
# the console before the levers: a dark box with a small dial and two lamps
m.mesh(m.root, "hullDark", [box((12, 74, 26), at=(124, 0, DECK), base=True)])
m.mesh(m.root, "team", [box((12.4, 60, 3), at=(124, 0, DECK + 26), base=True)], solid=False)
m.mesh(m.root, "glowOrange", [box((1.2, 5, 3), at=(130.6, y, DECK + 18)) for y in (-22.0, 22.0)], solid=False)
# the rail: chrome posts on the fenders and the deck's front, a top rail and a mid rail on each side, one across
POSTS = [(46, s * 60) for _, s in sides()] + [(128, s * 60) for _, s in sides()]
m.mesh(m.root, "chrome", [cyl(2.2, 104 - 74, at=(x, y, 74), base=True, sides=8) for x, y in POSTS])
m.mesh(m.root, "chrome", [bar((46, s * 60, 104), (128, s * 60, 104), 3.6, shape="cyl", sides=8) for _, s in sides()] +
       [bar((46, s * 60, 89), (128, s * 60, 89), 3.0, shape="cyl", sides=8) for _, s in sides()] +
       [bar((128, -60, 104), (128, 60, 104), 3.6, shape="cyl", sides=8)])
for k, s in sides():
    lv = m.part("levers_%d" % k, at=(LEVER_X, s * LEVER_Y, DECK + 14),
                driver=d("the %s lever: pitched forward with that side's drive (forward speed and the turn)" % ("left", "right")[k]))
    m.mesh(lv, "hullDark", [box((10, 8, 14), at=(LEVER_X, s * LEVER_Y, DECK), base=True)])
    m.mesh(lv, "chrome", [bar((LEVER_X, s * LEVER_Y, DECK + 14), (LEVER_X - 6, s * LEVER_Y, DECK + 60), 3.2, shape="cyl", sides=8)])
    m.mesh(lv, "glowRed", [sphere(3.4, at=(LEVER_X - 6.5, s * LEVER_Y, DECK + 63), steps=8)], solid=False)

# --- the nose: a dark block with a grille, the headlamp bar, the hazard bumper -----------------------------------------
m.mesh(m.root, "hullDark", [box((14, 124, 34), at=(137, 0, 26), base=True)])
m.mesh(m.root, "team", [box((14.4, 126, 6), at=(137, 0, 60), base=True)], solid=False)
m.mesh(m.root, "hullMid", [box((1.5, 34, 2.2), at=(144.4, 0, z)) for z in (36, 40.5, 45, 49.5, 54)], solid=False)  # grille
m.mesh(m.root, "hazard", [box((5, 150, 12), at=(146.5, 0, 16), base=True)])
headlamps = m.part("headlamps", at=(145, 0, 48), driver=d("headlamps: lit while driven, dim parked", ("emission",)))
m.mesh(headlamps, "chrome", [cyl(6.2, 2.4, at=(144, y, 48), axis="x", base=True, sides=12) for y in (-48, -32, 32, 48)], solid=False)
m.mesh(headlamps, "floodGlow", [cyl(5.0, 1.0, at=(146.4, y, 48), axis="x", base=True, sides=12) for y in (-48, -32, 32, 48)])
m.mesh(headlamps, "glowOrange", [box((1.2, 7, 3), at=(144.6, y, 32)) for y in (-58, 58)], solid=False)  # indicators

# --- the MH tanks: silver cylinders at the hopper's front corners, a sight glass and a dial on each -------------------
for k, s in sides():
    ty = s * TANK_Y
    m.mesh(m.root, "chrome", [cyl(TANK_R, TANK_Z1 - TANK_Z0, at=(TANK_X, ty, TANK_Z0), base=True, sides=18),
                              sphere(TANK_R, at=(TANK_X, ty, TANK_Z1), squash=(1, 1, 0.55), steps=10),
                              sphere(TANK_R, at=(TANK_X, ty, TANK_Z0), squash=(1, 1, 0.45), steps=10)])
    m.mesh(m.root, "hullDark", [cyl(TANK_R + 1.2, 4, at=(TANK_X, ty, z), base=True, sides=18) for z in (56, 104)], solid=False)  # straps
    # the sight glass: the mercury between two dark caps, outward and back of the tank, a part of its own
    a = math.radians(40)
    gx, gy = TANK_X - math.sin(a) * (TANK_R + 3.2), s * (TANK_Y + math.cos(a) * (TANK_R + 3.2))
    m.mesh(m.root, "hullDark", [cyl(3.4, 4, at=(gx, gy, 54), base=True, sides=8), cyl(3.4, 4, at=(gx, gy, 112), base=True, sides=8)])
    lv = m.part("mhLevel_%d" % k, at=(gx, gy, 58), driver=d(
        "the MH in the %s tank's sight glass: scaled up from its foot by the MH's share of 1000" % ("left", "right")[k],
        ("scale",)))
    m.mesh(lv, "muleMercury", [cyl(1.8, 53, at=(gx, gy, 58.4), base=True, sides=8)], solid=False)
    # the dial on the tank's outer side, its needle a part
    m.mesh(m.root, "hullDark", [cyl(7.2, 2.0, at=(TANK_X, s * (TANK_Y + TANK_R - 1.0), 88), axis="y", base=True, rot=(0, 0, 0) if s > 0 else (0, 0, 180), sides=14)], solid=False)
    m.mesh(m.root, "chrome", [cyl(5.8, 1.2, at=(TANK_X, s * (TANK_Y + TANK_R + 0.8), 88), axis="y",
                                                       rot=(0, 0, 0) if s > 0 else (0, 0, 180), base=True, sides=14)], solid=False)
    nd = m.part("needles_%d" % k, at=(TANK_X, s * (TANK_Y + TANK_R + 2.6), 88), driver=d(
        "the %s tank's dial needle: turned with the MH held" % ("left", "right")[k]))
    m.mesh(nd, "glowRed", [box((1.2, 0.6, 5.0), at=(TANK_X, s * (TANK_Y + TANK_R + 2.6), 88), base=True, rot=(-50, 0, 0))], solid=False)

# --- the stack and the beacon at the hopper's front corners -------------------------------------------------------------
m.mesh(m.root, "hullDark", [cyl(4, 34, at=(HOP_X1 - 8, 64, HOP_RIM - 4), base=True, sides=10)])
m.mesh(m.root, "hullMid", [cyl(4.8, 4, at=(HOP_X1 - 8, 64, HOP_RIM + 30), base=True, sides=10)], solid=False)
m.part("exhaust", at=(HOP_X1 - 8, 64, HOP_RIM + 34), driver=dict(what="the stack's top: exhaust smoke while it drives",
                                                                  channels=["particles"], functions=[POSE]))
m.mesh(m.root, "hullDark", [cyl(2.0, 30, at=(HOP_X1 - 8, -64, HOP_RIM + 5), base=True, sides=8),
                            cyl(6.5, 3, at=(HOP_X1 - 8, -64, HOP_RIM + 35), base=True, sides=12)])
beacon = m.part("beacon", at=(HOP_X1 - 8, -64, HOP_RIM + 38), driver=d(
    "the orange beacon: turns and pulses while it drives, steady dim parked", ("rotation", "emission")))
m.mesh(beacon, "glowOrange", [cyl(5.0, 8, at=(HOP_X1 - 8, -64, HOP_RIM + 38), base=True, sides=12),
                              sphere(5.0, at=(HOP_X1 - 8, -64, HOP_RIM + 46), squash=(1, 1, 0.5), steps=8)])
m.mesh(beacon, "hullDark", [box((10.6, 2.4, 6), at=(HOP_X1 - 8, -64, HOP_RIM + 39), base=True)], solid=False)  # its shutter

# the dust trail's sockets, behind the back wheels on the ground
for k, s in sides():
    m.part("dust_%d" % k, at=(AXLES[2] - 30, s * WHEEL_Y, 4), driver=dict(
        what="dust thrown up behind the %s back wheel while it drives" % ("left", "right")[k],
        channels=["particles"], functions=[POSE]))

m.write()
