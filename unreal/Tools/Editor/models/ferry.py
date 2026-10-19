"""The walk-in Dropship (C4, docs/haul-and-ferry.md): the Dropship the Prospector walks into, closes, steers to its
windshield and launches by hand. Today's Dropship's look (art/models/zfight/dropship/after-3q.jpg: a grey hull, deep
blue panels in the team's colour, two nacelles with octagonal intakes, twin fins, the white cross, orange lamps), grown
round a bay 2 wide, 4 long and 2 tall, with a rear ramp that hinges down, the bay of concept B2
(art/models/dropship/concepts/shuttle-bay-2.jpg: octagonal ribs, the orange-lit ramp post with its hazard plate by the
opening, cabin lamps, tie-down rails, hazard-striped deck edges) and the windshield of W2 (shuttle-window-2.jpg: three
panes in a riveted frame, blue-and-white striped pillars, two wipers) over the console with the green take-off button.
The strategy game's Dropship (`dropship_blue`) is left as it is.

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/ferry.py

Writes /Game/Models/ferry/SM_<part>__<material> and the catalog entry `ferry_blue`.

UE cm, a unit: the nose +X, Y the ship's right, Z up, the ground under the landed ship at Z = 0, the origin the ship's
position (the sim's). The bay (the sim's slabs, docs/haul-and-ferry.md section 4): the deck's top at DECK, clear from
BAY_X0 (the rear opening) to COCK_X (the console's front), +-BAY_W across (the ribs' faces), DECK to CEIL_IN high.
Parts the pose drives (AcPoseFerry.cpp): `ramp` (hinged at the opening's bottom edge), `rampButton`, `rampLamps_0`
(orange: open or moving) and `rampLamps_1` (green: shut), `takeoffButton` (lit while the ship can take off),
`consoleScreens_0..1`, `cabinLamps_0..3` (with their lights), `wipers_0..1` (turning in the glass's plane),
`fans_0..1`, `engineGlow_0..1`, `beacon_0..1`, the glow group `navGlow`.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, bar, box, cyl, loft, prism, ring_box, sphere, log  # noqa: E402

POSE = "AcPoseFerry.cpp:PoseFerry"

# --- the numbers the sim shares (cells x 100) ------------------------------------------------------------------------
DECK = 50.0        # the deck's top
CEIL_IN = 250.0    # the clear ceiling (the ribs' undersides; the panels between them at CEIL)
CEIL = 258.0
ROOF0 = 283.0      # the ceiling slab's top
BAY_W = 100.0      # the clear half width (the ribs' faces)
WALL_IN = 106.0    # the wall's own inner face, between the ribs
WALL_OUT = 125.0
SKIN = 131.0       # the cladding's outer face
BAY_X0 = -250.0    # the rear opening (the ramp's hinge line)
COCK_X = 236.0     # the console's front: the walkable floor ends here
NOSE_X = 262.0     # the nose block's back (behind the console)

# The windshield: the bottom line at GLASS_B (x, z), the top at GLASS_T; the centre pane +-PANE, the side panes from
# +-SIDE0 sweeping back to the walls.
GLASS_B = (334.0, 120.0)   # low, so the ground ahead shows through it (the manager, 2026-10-19)
GLASS_T = (262.0, 248.0)
PANE = 50.0
SIDE0 = 62.0
SIDE_BACK = 40.0   # how far back the side panes' outer edge stands
TILT = math.degrees(math.atan2(GLASS_B[0] - GLASS_T[0], GLASS_T[1] - GLASS_B[1]))  # back from upright, ~30 degrees


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


def slab(x0, x1, y0, y1, z0, z1):
    """An axis-aligned box from its two corners."""
    return box((x1 - x0, y1 - y0, z1 - z0), at=((x0 + x1) / 2, (y0 + y1) / 2, z0), base=True)


def both(fn):
    """fn(s) for the right (s = 1) and the left (s = -1) side."""
    return [p for s in (1, -1) for p in fn(s)]


def quad(corners, thick):
    """A slab of `thick` on the quad `corners` (four points in order), as a loft of the quad pushed both ways along
    its normal."""
    a, b, c = corners[0], corners[1], corners[2]
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - b[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    ln = math.sqrt(sum(x * x for x in n))
    n = [x / ln * thick / 2 for x in n]
    return loft([[tuple(p[i] - n[i] for i in range(3)) for p in corners],
                 [tuple(p[i] + n[i] for i in range(3)) for p in corners]])


# The glass's frame: G(u, v, w) is u across (Y), v up the glass from the bottom line, w out along its normal.
_GL = math.hypot(GLASS_B[0] - GLASS_T[0], GLASS_T[1] - GLASS_B[1])
_V = ((GLASS_T[0] - GLASS_B[0]) / _GL, 0.0, (GLASS_T[1] - GLASS_B[1]) / _GL)
_N = (_V[2], 0.0, -_V[0])


def G(u, v, w=0.0):
    return (GLASS_B[0] + v * _V[0] + w * _N[0], u, GLASS_B[1] + v * _V[2] + w * _N[2])


GLASS_H = _GL      # the centre pane's height along the glass


def side_pane(s, inset=0.0):
    """The right (s = 1) or left side pane's corners: inner edge at +-SIDE0 on the glass lines, outer edge on the wall
    plane SIDE_BACK further back."""
    b0, t0 = (GLASS_B[0], s * SIDE0, GLASS_B[1]), (GLASS_T[0], s * SIDE0, GLASS_T[1])
    b1 = (GLASS_B[0] - SIDE_BACK, s * (WALL_OUT - 9), GLASS_B[1])
    t1 = (GLASS_T[0] - SIDE_BACK, s * (WALL_OUT - 9), GLASS_T[1])
    return [b0, b1, t1, t0]


m = Model("ferry", category="unit", coplanar_check=True, notes=[
    "C4's walk-in Dropship (docs/haul-and-ferry.md): today's Dropship grown round a bay 2 wide, 4 long, 2 tall (clear), "
    "the rear ramp hinged at the deck's back edge, the cockpit with the console and the three-pane windshield ahead.",
    "Pose: PoseFerry (the ramp, the ramp post's button and lamps, the take-off button, the cabin lamps, the wipers, the "
    "engines). The bay's slabs are the sim's (c4-ship-sim); this model's walls, ceiling and ramp follow them."])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "glowRed", "glow_ff6b14"):
    m.material(name)
m.material("crossWhite", parent="hull", color=(0.82, 0.82, 0.8), roughness=0.55)
# Lamps: the base colour is the hue (the unlit master adds it).
m.material("cabinGlow", parent="emissive", color=(0.6, 0.36, 0.12), emissive=(16.0, 8.5, 2.6))
m.material("wallGlow", parent="emissive", color=(0.05, 0.4, 0.45), emissive=(0.3, 3.2, 3.6))
m.material("rampOpenGlow", parent="emissive", color=(0.6, 0.22, 0.02), emissive=(9.0, 3.0, 0.3))
m.material("rampShutGlow", parent="emissive", color=(0.05, 0.5, 0.12), emissive=(0.4, 6.0, 0.9))
m.material("takeoffGlow", parent="emissive", color=(0.08, 0.55, 0.15), emissive=(0.3, 3.0, 0.5))
m.material("screenGlow", parent="emissive", color=(0.03, 0.12, 0.2), emissive=(0.1, 0.6, 1.0))
m.material("windshield", parent="hull", color=(0.55, 0.66, 0.74), roughness=0.04, blend="translucent", opacity=0.1,
           double_sided=True)
root = m.root

# === the shell: the sim's slabs ======================================================================================
m.mesh(root, "hullDark", [slab(BAY_X0, NOSE_X, -WALL_OUT, WALL_OUT, 25, DECK)])            # the floor slab (the deck)
m.mesh(root, "hullMid", [slab(BAY_X0, 232, -WALL_OUT, WALL_OUT, CEIL, ROOF0)])             # the ceiling slab
WALL_PTS = [(BAY_X0, DECK), (NOSE_X, DECK), (NOSE_X, GLASS_B[1]), (GLASS_B[0] - SIDE_BACK + 2, GLASS_B[1]),
            (GLASS_T[0] - SIDE_BACK - 2, CEIL), (BAY_X0, CEIL)]
for s in (1, -1):
    m.mesh(root, "hullMid", [prism(WALL_PTS, WALL_OUT - WALL_IN, plane="xz", at=(0, s * (WALL_IN + WALL_OUT) / 2, 0))])
# the nose block under the windshield, behind the console
NOSE_PTS = [(NOSE_X, -WALL_OUT), (GLASS_B[0] - SIDE_BACK + 2, -WALL_OUT), (GLASS_B[0] + 2, -SIDE0),
            (GLASS_B[0] + 2, SIDE0), (GLASS_B[0] - SIDE_BACK + 2, WALL_OUT), (NOSE_X, WALL_OUT)]
m.mesh(root, "hullMid", [prism(NOSE_PTS, GLASS_B[1] - 2 - 25, plane="xy", at=(0, 0, (GLASS_B[1] - 2 + 25) / 2))])
# the brow over the windshield, from the top line to the ceiling slab
BROW = [(GLASS_T[0] - SIDE_BACK - 4, -WALL_OUT), (GLASS_T[0] + 2, -SIDE0), (GLASS_T[0] + 2, SIDE0),
        (GLASS_T[0] - SIDE_BACK - 4, WALL_OUT), (226, WALL_OUT), (226, -WALL_OUT)]
m.mesh(root, "hullMid", [prism(BROW, ROOF0 + 1 - GLASS_T[1] - 4, plane="xy", at=(0, 0, (ROOF0 + 1 + GLASS_T[1] + 4) / 2))])

# === the outside =====================================================================================================
# the roof: an octagon's top half over the ceiling slab, the chamfers down to the sides
ROOF = [(-SKIN, ROOF0 + 2), (SKIN, ROOF0 + 2), (SKIN, ROOF0 + 16), (95, 326), (-95, 326), (-SKIN, ROOF0 + 16)]
m.mesh(root, "hullLight", [prism(ROOF, 222 - BAY_X0 + 6, plane="yz", at=((222 + BAY_X0 - 6) / 2, 0, 0))])
# its front: a sloped cap down onto the brow
m.mesh(root, "hullLight", [prism([(219, ROOF0 + 3), (219, 326), (GLASS_T[0] + 4, ROOF0 + 6), (GLASS_T[0] + 4, ROOF0 + 3)],
                                 186, plane="xz", at=(0, 0, 0))])
# the sides: grey panels fore and aft, the team's blue mid-ship with the white cross, the hazard band low down
side_panels = []
for x0, x1 in ((-246, -170), (-166, -90), (-86, -64), (164, 214)):
    side_panels += both(lambda s, x0=x0, x1=x1: [slab(x0, x1, *sorted((s * (WALL_OUT + 2), s * SKIN)), 54, 274)])
m.mesh(root, "hullLight", side_panels, solid=False)
m.mesh(root, "team", both(lambda s: [slab(-60, 160, *sorted((s * (WALL_OUT + 2), s * SKIN)), 54, 274)]), solid=False)
CROSS = (90.0, 150.0)   # ahead of the nacelles
m.mesh(root, "crossWhite", both(lambda s: [
    slab(CROSS[0] - 12, CROSS[0] + 12, *sorted((s * SKIN, s * (SKIN + 2))), CROSS[1] - 48, CROSS[1] + 48),
    slab(CROSS[0] - 48, CROSS[0] - 12, *sorted((s * SKIN, s * (SKIN + 2))), CROSS[1] - 12, CROSS[1] + 12),
    slab(CROSS[0] + 12, CROSS[0] + 48, *sorted((s * SKIN, s * (SKIN + 2))), CROSS[1] - 12, CROSS[1] + 12)]), solid=False)
m.mesh(root, "hazard", both(lambda s: [slab(BAY_X0 - 4, 300, *sorted((s * (WALL_OUT + 2), s * (SKIN + 3))), 27, 50)]),
       solid=False)
# the belly
m.mesh(root, "hullDark", [slab(BAY_X0 + 10, 300, -110, 110, 18, 25)])
# the white cross on the roof, on a team square
m.mesh(root, "team", [slab(-30, 90, -50, 50, 326, 328)], solid=False)
m.mesh(root, "crossWhite", [slab(16, 44, -38, 38, 328, 330), slab(-8, 16, -12, 12, 328, 330), slab(44, 68, -12, 12, 328, 330)],
       solid=False)
# roof vents and a dorsal spine aft, between the fins
m.mesh(root, "hullDark", [slab(-200, -120, -26, 26, 326, 340), slab(110, 170, -60, -30, 326, 332), slab(110, 170, 30, 60, 326, 332)])

# the nose: a rounded loft forward of the nose block, the team's blue cap on its front (the old Dropship's oval)
rings = [ring_box(GLASS_B[0] - 4, 118, 46, cz=70, n=16), ring_box(362, 100, 42, cz=68, n=16),
         ring_box(390, 70, 32, cz=66, n=16), ring_box(404, 40, 18, cz=66, n=16)]
m.mesh(root, "hullLight", [loft(rings)])
m.mesh(root, "team", [sphere(40, at=(404, 0, 66), squash=(0.3, 1.05, 0.5), steps=16)])
m.mesh(root, "hazard", [slab(330, 372, -70, 70, 24, 30)], solid=False)  # the chin's hazard plate

# --- the windshield's frame, pillars and glass ------------------------------------------------------------------------
# the sill and the header along the glass lines, outside
sill = [bar(G(-SIDE0, -4, 4), G(SIDE0, -4, 4), 10, 14, side=_N),
        bar(G(-SIDE0, GLASS_H + 4, 4), G(SIDE0, GLASS_H + 4, 4), 10, 14, side=_N)]
for s in (1, -1):
    b0, b1, t1, t0 = side_pane(s)
    sill += [bar((b0[0], b0[1], b0[2] - 4), (b1[0], b1[1], b1[2] - 4), 10, 12),
             bar((t0[0], t0[1], t0[2] + 4), (t1[0], t1[1], t1[2] + 4), 10, 12)]
m.mesh(root, "hullDark", sill)
# the pillars: blue and white bands (the team's colour, W2's stripes), riveted
pillars_team, pillars_white = [], []
for s in (1, -1):
    for (bx, by), (tx, ty) in (((GLASS_B[0], s * (PANE + SIDE0) / 2), (GLASS_T[0], s * (PANE + SIDE0) / 2)),
                               ((GLASS_B[0] - SIDE_BACK, s * (WALL_OUT - 4)), (GLASS_T[0] - SIDE_BACK, s * (WALL_OUT - 4)))):
        a, b = (bx, by, GLASS_B[1] - 6), (tx, ty, GLASS_T[1] + 6)
        for k in range(6):
            f0, f1 = k / 6.0, (k + 1) / 6.0
            pa = tuple(a[i] + (b[i] - a[i]) * f0 for i in range(3))
            pb = tuple(a[i] + (b[i] - a[i]) * f1 for i in range(3))
            (pillars_team if k % 2 == 0 else pillars_white).append(bar(pa, pb, 15, 14, side=(0, 1, 0)))
m.mesh(root, "team", pillars_team)
m.mesh(root, "crossWhite", pillars_white)
# the rivets on the pillars and the sill, inside and out
rivets = []
for s in (1, -1):
    for k in range(7):
        f = (k + 0.5) / 7.0
        for w in (8.0, -8.0):
            rivets.append(sphere(1.6, at=G(s * (PANE + SIDE0) / 2 + 9 * s, 6 + f * (GLASS_H - 12), w), steps=6))
for k in range(9):
    rivets.append(sphere(1.6, at=G(-48 + 12 * k, -4, -6), steps=6))
m.mesh(root, "chrome", rivets, solid=False)
# the glass: the three panes (the rain on them is AcWindshield's post-process, in the glass's own frame)
glass = m.part("glass", driver=d("the windshield's three panes, faintly tinted; the rain and the wipe are drawn on them "
                                  "by AcWindshield", ("material",)))
panes = [quad([G(-PANE - 6, -2), G(PANE + 6, -2), G(PANE + 6, GLASS_H + 2), G(-PANE - 6, GLASS_H + 2)], 1.2)]
for s in (1, -1):
    panes.append(quad(side_pane(s), 1.2))
m.mesh(glass, "windshield", panes, solid=False)

# --- the wipers: two blades on the centre pane, parked lying left along the sill, turning in the glass's plane -------
WIPER_U = (-24.0, 30.0)   # their pivots across the centre pane
WIPER_L = 70.0            # the blade's reach from the pivot
for k, u in enumerate(WIPER_U):
    piv = G(u, 3, 5)
    w = m.part("wipers_%d" % k, at=piv, rot=(TILT, 0, 0), driver=d(
        "wiper %d: turns about the glass's normal (its local X) with the sweep of ac::Wipers, parked lying left" % k,
        ("rotation",)))
    m.mesh(w, "hullDark", [cyl(4, 6, at=G(u, 3, 2), axis="x", rot=(TILT, 0, 0), base=False, sides=10),
                           bar(G(u, 3, 6), G(u, WIPER_L * 0.55, 7), 4, 2.4, side=_N)], solid=False)
    # the blade's arm, and the blade with its rubber, along the glass
    m.mesh(w, "chrome", [bar(G(u, WIPER_L * 0.55, 7), G(u, WIPER_L, 6), 2.6, 2, side=_N)], solid=False)
    m.mesh(w, "hullDark", [bar(G(u, WIPER_L * 0.18, 4.5), G(u, WIPER_L, 4.5), 1.2, 2.6, side=_N)], solid=False)

# --- the nacelles: octagonal pods with chrome intakes, the team's cowling, the jets aft ------------------------------
NAC = (-122.0, 198.0, 240.0)   # the axis's middle (x, |y|, z): high on the back, as on today's Dropship
NAC_R, NAC_L = 60.0, 216.0
nac_front = NAC[0] + NAC_L / 2
nac_back = NAC[0] - NAC_L / 2
for k, s in enumerate((1, -1)):
    y = s * NAC[1]
    m.mesh(root, "hullMid", [cyl(NAC_R, NAC_L, at=(NAC[0], y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "chrome", [cyl(NAC_R + 7, 22, at=(nac_front - 6, y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "hullDark", [cyl(NAC_R - 6, 4, at=(nac_front + 6, y, NAC[2]), axis="x", sides=8)])
    # the cowling on the top and outer face, in the team's colour
    m.mesh(root, "team", [slab(nac_back + 24, nac_front - 30, y - 34, y + 34, NAC[2] + NAC_R - 6, NAC[2] + NAC_R + 10),
                          slab(nac_back + 24, nac_front - 30, *sorted((y + s * (NAC_R - 6), y + s * (NAC_R + 10))),
                               NAC[2] - 34, NAC[2] + 34)])
    m.mesh(root, "hullDark", [slab(nac_back + 40, nac_front - 46, *sorted((y + s * (NAC_R + 10), y + s * (NAC_R + 13))),
                                   NAC[2] - 8, NAC[2] + 8)], solid=False)  # a rail on the cowling's flank
    # the pylon to the hull
    m.mesh(root, "hullMid", [slab(nac_back + 20, nac_front - 40, *sorted((s * (SKIN - 8), s * (NAC[1] - NAC_R + 10))), NAC[2] - 30, NAC[2] + 16)])
    # the jet: open like the intake, a chrome octagonal rim round the dark nozzle, vanes across it, a small warm glow
    # deep in its middle (brighter with the thrust)
    m.mesh(root, "chrome", [cyl(NAC_R + 6, 18, at=(nac_back + 5, y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "hullDark", [cyl(NAC_R - 6, 4, at=(nac_back - 4, y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "hullDark", [box((2, 7, (NAC_R - 10) * 2), at=(nac_back - 10.5 - 2.5 * j, y, NAC[2]), rot=(0, 0, 60 * j))
                              for j in range(3)], solid=False)
    jet = m.part("engineGlow_%d" % k, at=(nac_back - 7.5, y, NAC[2]), handles=["engineGlow"],
                 driver=d("engine %d's jet: a faint glow deep in the nozzle, brighter with the thrust" % k))
    m.mesh(jet, "glow_ff6b14", [cyl(18, 1.2, at=(nac_back - 7.5, y, NAC[2]), axis="x", sides=8)], solid=False)
    m.part("emitter_%d" % k, parent=jet, at=(nac_back - 30, y, NAC[2]), particles={"handles": "jet"})
    # the fan in the intake
    fan = m.part("fans_%d" % k, at=(nac_front + 9, y, NAC[2]), driver=d("intake fan %d: spins with the thrust" % k,
                                                                        ("rotation",)))
    m.mesh(fan, "chrome", [cyl(10, 6, at=(nac_front + 9, y, NAC[2]), axis="x", sides=10)], solid=False)
    m.mesh(fan, "hullDark", [box((2, 8, NAC_R * 1.6), at=(nac_front + 9.5 + 2.5 * j, y, NAC[2]), rot=(0, 0, 30 + 60 * j))
                             for j in range(3)], solid=False)

# --- the fins: twin fins aft on the roof, canted out, the team's panels, red beacons on their tips --------------------
FIN = [(0, 0), (130, 0), (78, 124), (16, 134)]
for k, s in enumerate((1, -1)):
    m.mesh(root, "hullLight", [prism([(x - 246, z + 318) for x, z in FIN], 10, plane="xz", at=(0, s * 56, 0),
                                     rot=(0, 0, -s * 16))])
    m.mesh(root, "team", [prism([(x - 246, z + 318) for x, z in ((24, 30), (104, 30), (70, 104), (30, 110))], 14,
                                plane="xz", at=(0, s * 56, 0), rot=(0, 0, -s * 16))], solid=False)
    tip = m.part("beacon_%d" % k, at=(-226, s * (56 + 128 * math.sin(math.radians(16))), 318 + 128 * math.cos(math.radians(16))),
                 handles=["beacon"], driver=d("fin %d's beacon: blinks" % k))
    m.mesh(tip, "glowRed", [sphere(6, at=tip.at, steps=8)], solid=False)

# --- the four leg pods (today's Dropship's blue boots at its corners), struts and pads under them, an orange lamp on
# each pod's toe --------------------------------------------------------------------------------------------------------
nav = m.part("navGlow", handles=["navGlow"], driver=d("the orange nav lamps: steady, brighter in flight"))
POD_Y, POD_T = 152.0, 38.0     # the pods' middle across and their thickness
PODS = {1: [(200, 12), (288, 12), (304, 64), (284, 156), (206, 156)],          # front: the toe forward
        -1: [(-150, 12), (-238, 12), (-252, 64), (-232, 156), (-156, 156)]}   # back: under the nacelles
for fb, pts in PODS.items():
    lx = (pts[0][0] + pts[1][0]) / 2
    toe = pts[2][0]
    for s in (1, -1):
        m.mesh(root, "team", [prism(pts, POD_T, plane="xz", at=(0, s * POD_Y, 0))])
        m.mesh(root, "hullDark", [slab(lx - 34, lx + 34, *sorted((s * (POD_Y - POD_T / 2 - 4), s * (SKIN - 10))), 70, 130)])
        m.mesh(root, "hullDark", [slab(lx - 9, lx + 9, s * POD_Y - 9, s * POD_Y + 9, 3, 14)])
        m.mesh(root, "chrome", [cyl(20, 3, at=(lx, s * POD_Y, 0), base=True, sides=12)])
        m.mesh(nav, "glow_ff6b14", [box((7, 10, 8), at=(toe - fb * 3, s * POD_Y, 60), base=True)], solid=False)
# lamps on the nacelles' noses and the hull's corners
m.mesh(nav, "glow_ff6b14", both(lambda s: [box((6, 8, 8), at=(nac_front - 6, s * (NAC[1] + NAC_R + 12), NAC[2] + 24), base=True),
                                           box((6, 8, 8), at=(BAY_X0 + 2, s * (SKIN + 6), 300), base=True)]), solid=False)

# === the rear: the opening's frame and the ramp =======================================================================
JAMB = (-272.0, -244.0)
m.mesh(root, "hullMid", both(lambda s: [slab(JAMB[0], JAMB[1], *sorted((s * (BAY_W + 2), s * (SKIN + 2))), 22, CEIL_IN + 2)]))
m.mesh(root, "hullMid", [slab(JAMB[0], JAMB[1] - 2, -SKIN - 2, SKIN + 2, CEIL_IN + 2, 322)])  # the header over it
m.mesh(root, "hazard", both(lambda s: [slab(JAMB[0] - 3, JAMB[0], *sorted((s * (BAY_W + 4), s * (BAY_W + 22))), 30, CEIL_IN)]) +
       [slab(JAMB[0] - 3, JAMB[0], -BAY_W - 4, BAY_W + 4, CEIL_IN + 6, CEIL_IN + 22)], solid=False)
m.mesh(root, "hazard", [slab(BAY_X0 + 2, BAY_X0 + 20, -BAY_W + 2, BAY_W - 2, DECK, DECK + 1)], solid=False)  # the sill

# the ramp: standing in the opening (closed) at rest, hinged at its inner bottom edge; open it lies down to the ground
RAMP_T = 20.0
ramp = m.part("ramp", at=(BAY_X0, 0, DECK), driver=d(
    "the ramp: turns about its hinge (the opening's bottom edge, the ship's Y axis), the top outward and down, "
    "from 0 (shut, standing) to RampOpenDeg (lying on the ground)", ("rotation",)))
m.mesh(ramp, "hullLight", [slab(BAY_X0 - RAMP_T, BAY_X0, -BAY_W + 2, BAY_W - 2, DECK + 2, CEIL_IN - 2)])
m.mesh(ramp, "hullDark", [slab(BAY_X0, BAY_X0 + 2, -70, 70, DECK + 14 + 22 * j, DECK + 22 + 22 * j) for j in range(8)],
       solid=False)  # treads on the inner face (the top, open)
m.mesh(ramp, "hazard", both(lambda s: [slab(BAY_X0, BAY_X0 + 2, *sorted((s * 78, s * 94)), DECK + 6, CEIL_IN - 8)]),
       solid=False)
m.mesh(ramp, "team", [slab(BAY_X0 - RAMP_T - 2, BAY_X0 - RAMP_T, -60, 60, DECK + 40, CEIL_IN - 40)], solid=False)
m.mesh(ramp, "crossWhite", [slab(BAY_X0 - RAMP_T - 4, BAY_X0 - RAMP_T - 2, -10, 10, DECK + 60, CEIL_IN - 60),
                            slab(BAY_X0 - RAMP_T - 4, BAY_X0 - RAMP_T - 2, -40, -10, 140, 160),
                            slab(BAY_X0 - RAMP_T - 4, BAY_X0 - RAMP_T - 2, 10, 40, 140, 160)], solid=False)

# === the bay: octagonal ribs, wall panels and boxes, cabin lamps, tie-down rails, hazard deck edges ====================
RIBS = (-200.0, -120.0, -40.0, 40.0, 120.0, 196.0)
CH = 22.0  # the ribs' corner chamfer
ribs = []
for x in RIBS:
    ribs += both(lambda s, x=x: [slab(x - 6, x + 6, *sorted((s * BAY_W, s * WALL_IN)), DECK, CEIL_IN - CH),
                                 bar((x, s * BAY_W, CEIL_IN - CH), (x, s * (BAY_W - CH), CEIL_IN), 12, 8, side=(1, 0, 0))])
    ribs.append(slab(x - 6, x + 6, -BAY_W + CH, BAY_W - CH, CEIL_IN, CEIL))
m.mesh(root, "hullDark", ribs)
# wall boxes with cyan lamps (B2), and the pipe runs along the upper walls
boxes, glows = [], []
for s in (1, -1):
    for x in (-160.0, 0.0, 160.0):
        if s < 0 and x < -100:
            continue  # the ramp post's bay
        boxes.append(slab(x - 18, x + 18, *sorted((s * (WALL_IN - 1), s * (WALL_IN - 5))), 120, 170))
        glows.append(slab(x + 6, x + 12, *sorted((s * (WALL_IN - 5), s * (WALL_IN - 6))), 150, 164))
m.mesh(root, "hullMid", boxes, solid=False)
m.mesh(root, "wallGlow", glows, solid=False)
m.mesh(root, "chrome", both(lambda s: [cyl(2.5, 440, at=(-30, s * (WALL_IN - 3), z), axis="x", sides=8) for z in (212.0, 224.0)]),
       solid=False)
# the deck: tie-down rails (flush, under 3 cm) and the hazard-striped edges
m.mesh(root, "chrome", [slab(BAY_X0 + 24, COCK_X - 20, y - 2, y + 2, DECK, DECK + 1.5) for y in (-56.0, -20.0, 20.0, 56.0)],
       solid=False)
m.mesh(root, "hazard", both(lambda s: [slab(BAY_X0 + 24, COCK_X - 6, *sorted((s * 86, s * (BAY_W - 4))), DECK, DECK + 1)]),
       solid=False)
# the cabin lamps between the ribs (warm, B2), each with its light
for k, x in enumerate((-160.0, -80.0, 0.0, 80.0)):
    lamp = m.part("cabinLamps_%d" % k, at=(x, 0, CEIL - 6), handles=["cabinLamp"],
                  light={"attenuationEnd": "3.0", "color": "SIMD4<Float>(1.0, 0.62, 0.3, 1.0)", "handles": "cabinLamp",
                         "intensity": "0.0", "type": "omni"},
                  driver=d("cabin lamp %d: steady warm light in the bay" % k, ("emission", "light")))
    m.mesh(lamp, "hullDark", [slab(x - 22, x + 22, -12, 12, CEIL - 4, CEIL)], solid=False)
    m.mesh(lamp, "cabinGlow", [slab(x - 16, x + 16, -7, 7, CEIL - 5, CEIL - 4)], solid=False)

# --- the ramp post: in its niche left of the opening (looking in), its lamps on top, the button on its face ----------
POST = (-226.0, -(BAY_W - 12))   # its face's middle (x, y): 12 cm into the clear bay
post_face = POST[1]
m.mesh(root, "hullDark", [slab(POST[0] - 16, POST[0] + 16, -WALL_IN - 4, post_face, DECK, 156)])
m.mesh(root, "hazard", [slab(POST[0] - 14, POST[0] + 14, post_face, post_face + 2, DECK + 8, DECK + 56)], solid=False)
m.mesh(root, "hullMid", [slab(POST[0] - 12, POST[0] + 12, post_face, post_face + 4, 100, 126)], solid=False)  # the button's plate
btn = m.part("rampButton", at=(POST[0], post_face + 4, 113), driver=d(
    "the ramp button: pushed in 1.5 cm (along +Y) for 0.25 s on each press", ("position",)))
m.mesh(btn, "hazard", [cyl(7, 4, at=(POST[0], post_face + 4, 113), axis="y", base=True, sides=12)], solid=False)
m.mesh(root, "hullMid", [slab(POST[0] - 14, POST[0] + 14, post_face - 2, post_face + 26, 156, 162)])  # the lamp's cap
lamp_open = m.part("rampLamps_0", at=(POST[0], post_face + 14, 140), handles=["rampLamp"], driver=d(
    "the ramp post's orange lamp: lit while the ramp is open, flashing while it moves"))
m.mesh(lamp_open, "rampOpenGlow", [cyl(11, 14, at=(POST[0], post_face + 13, 140), axis="y", sides=12, r1=10)],
       solid=False)
m.mesh(root, "chrome", [cyl(12.5, 3, at=(POST[0], post_face + 2, 140), axis="y", base=True, sides=12)], solid=False)
lamp_shut = m.part("rampLamps_1", at=(POST[0], post_face + 2, 170), handles=["rampLamp"], driver=d(
    "the ramp post's green lamp: lit while the ramp is shut"))
m.mesh(root, "hullDark", [slab(POST[0] - 7, POST[0] + 7, post_face - 2, post_face + 8, 162, 178)])
m.mesh(lamp_shut, "rampShutGlow", [slab(POST[0] - 5, POST[0] + 5, post_face + 8, post_face + 10, 164, 176)], solid=False)

# --- the console under the glass: three panels, the big green take-off button, screens ----------------------------------
CON = (COCK_X, NOSE_X + 6)    # its front and back (x)
CON_TOP = 96.0
m.mesh(root, "hullMid", [slab(CON[0], CON[1], -96, 96, DECK, CON_TOP)])
m.mesh(root, "hullDark", [slab(CON[0] - 2, CON[0], -80, 80, DECK + 10, CON_TOP - 30)], solid=False)  # the knee panel
# the sloped desk: up toward the glass
DESK = [(CON[0] - 2, CON_TOP), (CON[1], CON_TOP), (CON[1], CON_TOP + 16), (CON[0] - 2, CON_TOP + 4)]
m.mesh(root, "hullDark", [prism(DESK, 192, plane="xz", at=(0, 0, 0))])
DESK_ANG = math.degrees(math.atan2(12, CON[1] - CON[0] + 2))


def desk(x, y, h=0.0):
    """A point on the desk's top at x (along it) and y, h over it."""
    f = (x - (CON[0] - 2)) / (CON[1] - CON[0] + 2)
    z = CON_TOP + 4 + 12 * f
    return (x + h * -math.sin(math.radians(DESK_ANG)), y, z + h * math.cos(math.radians(DESK_ANG)))


panels = []
for y0, y1 in ((-86, -34), (-26, 26), (34, 86)):
    c = desk((CON[0] + CON[1]) / 2, (y0 + y1) / 2, 3)
    panels.append(box((CON[1] - CON[0] - 6, y1 - y0, 6), at=c, rot=(DESK_ANG, 0, 0)))
m.mesh(root, "hullMid", panels)
# switches and dials on the side panels
knobs = []
for y in (-76, -64, -52, -42, 44, 54, 66, 78):
    for x in (CON[0] + 8, CON[0] + 20):
        knobs.append(cyl(2.6, 4, at=desk(x, y, 6), axis="z", rot=(DESK_ANG, 0, 0), base=True, sides=8))
m.mesh(root, "chrome", knobs, solid=False)
for k, y in enumerate((-60.0, 60.0)):
    scr = m.part("consoleScreens_%d" % k, at=desk(CON[1] - 6, y, 6), driver=d(
        "console screen %d: lit while someone is aboard; brighter in flight" % k))
    c = desk(CON[1] - 6, y, 13)
    sx = (math.cos(math.radians(-40)), 0.0, math.sin(math.radians(-40)))  # the housing's local X: its face is -X
    m.mesh(root, "hullDark", [box((4, 30, 20), at=c, rot=(-40, 0, 0))])
    m.mesh(scr, "screenGlow", [box((1, 25, 15), at=tuple(c[i] - 4 * sx[i] for i in range(3)), rot=(-40, 0, 0))], solid=False)
# the take-off button: a green dome in a dark bezel on a chrome housing, a hazard collar round it, in the centre panel
BX = (CON[0] + CON[1]) / 2
BTN = desk(BX, 0, 6)
m.material("domeGreen", parent="hull", color=(0.05, 0.42, 0.1), roughness=0.18, metallic=0.0)
m.mesh(root, "hazard", [cyl(12, 2, at=BTN, axis="z", rot=(DESK_ANG, 0, 0), base=True, sides=16)], solid=False)
m.mesh(root, "chrome", [cyl(9.5, 4, at=desk(BX, 0, 8), axis="z", rot=(DESK_ANG, 0, 0), base=True, sides=16)], solid=False)
m.mesh(root, "hullDark", [cyl(8.2, 2, at=desk(BX, 0, 12), axis="z", rot=(DESK_ANG, 0, 0), base=True, sides=16, r1=7.6)],
       solid=False)
tob = m.part("takeoffButton", at=desk(BX, 0, 14), rot=(DESK_ANG, 0, 0), driver=d(
    "the take-off button: its core lit green while the ship can take off (landed, the ramp shut), dim otherwise; "
    "pushed in 2 cm along its axis for 0.25 s on each press", ("emission", "position")))
# the dome (lit, glossy) with the lamp's core glowing through its top: the light falls off toward its rim
m.mesh(tob, "domeGreen", [sphere(6.6, at=desk(BX, 0, 14), rot=(DESK_ANG, 0, 0), squash=(1, 1, 0.62), steps=14)], solid=False)
m.mesh(tob, "takeoffGlow", [sphere(4.2, at=desk(BX, 0, 15.6), rot=(DESK_ANG, 0, 0), squash=(1, 1, 0.55), steps=12)],
       solid=False)

m.write()
log("ferry: tilt %.1f deg, glass %.0f cm high, the take-off button at %s" % (TILT, GLASS_H, tuple(round(c) for c in BTN)))
