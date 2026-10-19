"""The walk-in Dropship (C4, grown in C5; docs/haul-and-ferry.md): the Dropship the Prospector walks into, closes,
steers to its windshield and launches by hand. Today's Dropship's look (art/models/zfight/dropship/after-3q.jpg: a grey
hull, deep blue panels in the team's colour, two nacelles with octagonal intakes, twin fins, the white cross, orange
lamps) on a long shaped hull (concept art/models/dropship/concepts/shuttle-exterior-2.jpg: a long fuselage, the cockpit
in the nose, the ramp under the tail), round a bay 4 wide, 12 long and 2.6 tall: a tapered nose under a swept
three-pane windshield, a curved roof that rises from the brow and settles toward the tail over the ramp, chines along
the flanks, the nacelles high on swept pylons with the fins on them. Inside, the bay of concept B2
(art/models/dropship/concepts/shuttle-bay-2.jpg: octagonal ribs, the orange-lit ramp post with its hazard plate by the
opening, cabin lamps, tie-down rails, hazard-striped deck edges) and the windshield of W2 (shuttle-window-2.jpg: three
panes in a riveted frame, blue-and-white striped pillars, two wipers) over the console with the green take-off button.
The strategy game's Dropship (`dropship_blue`) is left as it is.

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/ferry.py

Writes /Game/Models/ferry/SM_<part>__<material> and the catalog entry `ferry_blue`.

UE cm, a unit: the nose +X, Y across (the sim's y, cells x 100), Z up, the ground under the landed ship at Z = 0, the
origin the ship's position (the sim's). The bay (the sim's slabs, `ac::ShuttleBay`, docs/haul-and-ferry.md section 3):
the deck's top at DECK, clear from BAY_X0 (the rear opening) to COCK_X (the console's front), +-BAY_W across (the ribs'
faces), DECK to CEIL_IN high; the opening and the ramp +-RAMP_W across.
Parts the pose drives (AcPoseFerry.cpp): `ramp` (hinged at the opening's bottom edge), `rampButton`, `rampLamps_0`
(orange: open or moving) and `rampLamps_1` (green: shut), `takeoffButton` (lit while the ship can take off),
`consoleScreens_0..1`, `cabinLamps_0..7` (with their lights), `wipers_0..1` (turning in the glass's plane),
`fans_0..1`, `engineGlow_0..1`, `beacon_0..1`, the glow group `navGlow`.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, bar, box, cyl, loft, m_apply, prism, ring_box, rot3, sphere, log  # noqa: E402

POSE = "AcPoseFerry.cpp:PoseFerry"

# --- the numbers the sim shares (cells x 100) ------------------------------------------------------------------------
DECK = 60.0        # the deck's top
CEIL_IN = 320.0    # the clear ceiling (the ribs' undersides; the panels between them at CEIL)
CEIL = 328.0
ROOF0 = 345.0      # the ceiling slab's top
BAY_W = 200.0      # the clear half width (the ribs' faces)
WALL_IN = 206.0    # the wall's own inner face, between the ribs
WALL_OUT = 230.0
SKIN = 245.0       # the flanks' outer face
CHINE = (262.0, 100.0)   # the chine's edge (y, z): the hull's widest
RAMP_W = 160.0     # the opening's and the ramp's half width
BAY_X0 = -680.0    # the rear opening (the ramp's hinge line)
TAIL_X = -705.0    # the tail's jambs' back (the sim's hullRear)
COCK_X = 660.0     # the console's front: the walkable floor ends here
NOSE_X = 692.0     # the nose block's back (behind the console)
TIP_X = 930.0      # the nose's tip (the sim's hullFront)

# The windshield: the bottom line at GLASS_B (x, z), the top at GLASS_T; the centre pane +-PANE, the side panes from
# +-SIDE0 sweeping back to the flanks (+-SIDE_OUT, SIDE_BACK further back).
GLASS_B = (820.0, 130.0)   # low, so the ground ahead shows through it
GLASS_T = (675.0, 315.0)
PANE = 95.0
SIDE0 = 107.0
SIDE_OUT = 236.0
SIDE_BACK = 124.0  # how far back the side panes' outer edge stands: a swept nose
TILT = math.degrees(math.atan2(GLASS_B[0] - GLASS_T[0], GLASS_T[1] - GLASS_B[1]))  # back from upright, ~38 degrees
SIDE_TOP_X = GLASS_T[0] - SIDE_BACK   # where the side panes' top outer corner stands: the roof's front


TUMBLE = 6.0       # the flanks lean in this much from the strake up to the roof's spring


def roof_top(x):
    """The roof's crest at x: up from the brow, highest amidships, settling toward the tail over the ramp."""
    pts = [(-800.0, 400.0), (-600.0, 432.0), (-300.0, 446.0), (150.0, 446.0), (420.0, 425.0), (SIDE_TOP_X, 395.0)]
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        if x <= x1:
            f = max(0.0, min(1.0, (x - x0) / (x1 - x0)))
            return z0 + (z1 - z0) * (f * f * (3 - 2 * f))
    return pts[-1][1]


def spring(x):
    """Where the rounded roof springs from the flanks at x: low amidships (a rounded upper hull), up to the ceiling
    slab's top over the cockpit, where the side panes meet it."""
    f = along(x, 380.0, SIDE_TOP_X)
    return lerp(300.0, ROOF0 + 2, f * f * (3 - 2 * f))


def flank_y(z, x=0.0):
    """The flank's outer face across at height z: leaning in from SKIN at the strake's top to SKIN - TUMBLE at the
    spring."""
    return SKIN - TUMBLE * max(0.0, min(1.0, (z - 140.0) / (spring(x) - 140.0)))


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


def strip(stations, ring):
    """Lofts along x between neighbouring `stations`, `ring(x)` the section there (a convex ring of (y, z) points,
    the same count at every station): one small loft per gap, so each stays convex for the loft's winding."""
    out = []
    for x0, x1 in zip(stations, stations[1:]):
        out.append(loft([[(x0, y, z) for y, z in ring(x0)], [(x1, y, z) for y, z in ring(x1)]]))
    return out


def lerp(a, b, f):
    return a + (b - a) * f


def along(x, x0, x1):
    return max(0.0, min(1.0, (x - x0) / (x1 - x0)))


# The glass's frame: G(u, v, w) is u across (Y), v up the glass from the bottom line, w out along its normal.
_GL = math.hypot(GLASS_B[0] - GLASS_T[0], GLASS_T[1] - GLASS_B[1])
_V = ((GLASS_T[0] - GLASS_B[0]) / _GL, 0.0, (GLASS_T[1] - GLASS_B[1]) / _GL)
_N = (_V[2], 0.0, -_V[0])


def G(u, v, w=0.0):
    return (GLASS_B[0] + v * _V[0] + w * _N[0], u, GLASS_B[1] + v * _V[2] + w * _N[2])


GLASS_H = _GL      # the centre pane's height along the glass


def side_pane(s, inset=0.0):
    """The right (s = 1) or left side pane's corners: inner edge at +-SIDE0 on the glass lines, outer edge on the
    flank's plane SIDE_BACK further back."""
    b0, t0 = (GLASS_B[0], s * SIDE0, GLASS_B[1]), (GLASS_T[0], s * SIDE0, GLASS_T[1])
    b1 = (GLASS_B[0] - SIDE_BACK, s * SIDE_OUT, GLASS_B[1])
    t1 = (GLASS_T[0] - SIDE_BACK, s * SIDE_OUT, GLASS_T[1])
    return [b0, b1, t1, t0]


def pane_line_z(x):
    """The height of the side panes' outer edge at x (from its top corner down to its bottom corner)."""
    f = along(x, SIDE_TOP_X, GLASS_B[0] - SIDE_BACK)
    return lerp(GLASS_T[1], GLASS_B[1], f)


m = Model("ferry", category="unit", coplanar_check=True, notes=[
    "C5's walk-in Dropship (docs/haul-and-ferry.md): a long shaped hull round a bay 4 wide, 12 long, 2.6 tall (clear), "
    "the rear ramp hinged at the deck's back edge, the cockpit with the console and the three-pane windshield in the "
    "tapered nose, the nacelles on swept pylons with the fins on them.",
    "Pose: PoseFerry (the ramp, the ramp post's button and lamps, the take-off button, the cabin lamps, the wipers, the "
    "engines). The bay's slabs are the sim's (ac::ShuttleBay); this model's walls, ceiling and ramp follow them."])
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

# === the shell inside: the sim's slabs ================================================================================
m.mesh(root, "hullDark", [slab(TAIL_X + 3, NOSE_X, -WALL_OUT, WALL_OUT, 35, DECK)])          # the floor slab (the deck)
m.mesh(root, "hullMid", [slab(TAIL_X + 3, GLASS_T[0] - 50, -WALL_OUT, WALL_OUT, CEIL, ROOF0)])  # the ceiling slab
WALL_PTS = [(BAY_X0 - 2, DECK), (NOSE_X, DECK), (NOSE_X, GLASS_B[1]), (GLASS_B[0] - SIDE_BACK + 2, GLASS_B[1]),
            (SIDE_TOP_X - 2, GLASS_T[1]), (SIDE_TOP_X - 2, CEIL), (BAY_X0 - 2, CEIL)]
for s in (1, -1):
    m.mesh(root, "hullMid", [prism(WALL_PTS, WALL_OUT - WALL_IN, plane="xz", at=(0, s * (WALL_IN + WALL_OUT) / 2, 0))])
# the nose block under the windshield, behind the console
NOSE_PTS = [(NOSE_X, -WALL_OUT), (GLASS_B[0] - SIDE_BACK + 2, -WALL_OUT), (GLASS_B[0] + 2, -SIDE0),
            (GLASS_B[0] + 2, SIDE0), (GLASS_B[0] - SIDE_BACK + 2, WALL_OUT), (NOSE_X, WALL_OUT)]
m.mesh(root, "hullMid", [prism(NOSE_PTS, GLASS_B[1] - 2 - 35, plane="xy", at=(0, 0, (GLASS_B[1] - 2 + 35) / 2))])
# the brow over the windshield, from the top line up into the roof
BROW = [(SIDE_TOP_X - 4, -SKIN), (GLASS_T[0] + 2, -SIDE0), (GLASS_T[0] + 2, SIDE0), (SIDE_TOP_X - 4, SKIN),
        (GLASS_T[0] - 140, SKIN), (GLASS_T[0] - 140, -SKIN)]
m.mesh(root, "hullMid", [prism(BROW, ROOF0 + 4 - GLASS_T[1] - 4, plane="xy", at=(0, 0, (ROOF0 + 4 + GLASS_T[1] + 4) / 2))])

# === the outside: the fuselage ========================================================================================
# The roof: a rounded crest over the ceiling slab, from the tail's hood (over the ramp) to the side panes' top.


ARC_P = 2.4   # the roof's section: a superellipse's power (2 an ellipse; higher, squarer)


def roof_ring(x):
    # the tail's hood narrows and drops over the ramp, so its end is small
    f = along(x, -800.0, -690.0)
    top, base, half = roof_top(x), spring(x) - 2 + 30 * (1 - f), (SKIN - TUMBLE) * lerp(0.72, 1.0, f ** 0.6)
    pts = []
    for k in range(ARC):
        a = math.pi * k / (ARC - 1)
        c, sn = math.cos(a), math.sin(a)
        pts.append((half * math.copysign(abs(c) ** (2 / ARC_P), c), base + (top - base) * abs(sn) ** (2 / ARC_P)))
    return pts


ARC = 13
ROOF_X = [-800.0, -740.0, -680.0, -600.0, -500.0, -400.0, -300.0, 150.0, 300.0, 380.0, 440.0, 500.0, SIDE_TOP_X]
m.mesh(root, "hullLight", strip(ROOF_X, roof_ring))


# its front: from the side panes' top down onto the brow over the centre pane
def cap_ring(x):
    f = along(x, SIDE_TOP_X, GLASS_T[0] + 2)
    top = lerp(roof_top(SIDE_TOP_X), ROOF0 + 6, f ** 0.7)
    half = lerp(SKIN - TUMBLE, SIDE0, f)
    pts = []
    for k in range(ARC):
        a = math.pi * k / (ARC - 1)
        c, sn = math.cos(a), math.sin(a)
        pts.append((half * math.copysign(abs(c) ** (2 / ARC_P), c), ROOF0 - 2 + (top - ROOF0 + 2) * abs(sn) ** (2 / ARC_P)))
    return pts


m.mesh(root, "hullLight", strip([SIDE_TOP_X + k * (GLASS_T[0] + 2 - SIDE_TOP_X) / 4 for k in range(5)], cap_ring))


# The flanks: an upright panel from the chine strake up into the roof, its top following the side panes' outer edge
# in the cockpit; the strake below it with the chine's sharp edge, narrowing along the nose to the tip.
def flank_ring(s):
    def ring(x):
        top = spring(x) + 2 if x <= SIDE_TOP_X else pane_line_z(x) - 2
        return [(s * (WALL_OUT - 2), 140.0), (s * SKIN, 140.0), (s * flank_y(top, x), top), (s * (WALL_OUT - 2), top)]
    return ring


FLANK_X = [TAIL_X, -400.0, 0.0, 380.0, 440.0, 500.0, SIDE_TOP_X - 0.5]
m.mesh(root, "hullMid", both(lambda s: strip(FLANK_X, flank_ring(s)) +
                              strip([SIDE_TOP_X + 0.5, (SIDE_TOP_X + GLASS_B[0] - SIDE_BACK) / 2, GLASS_B[0] - SIDE_BACK],
                                    flank_ring(s))))


def strake_half(x):
    """The strake's outer y (the chine) and the hull's half width at its foot at x: full back from the side panes'
    bottom corner, then in along the nose to the tip."""
    f = along(x, GLASS_B[0] - SIDE_BACK, TIP_X - 20)
    return lerp(CHINE[0], 60.0, f ** 0.8), lerp(WALL_OUT + 2, 50.0, f ** 0.8)


def strake_rings(s):
    """The strake's lower facet (dark, from the belly out to the chine) and its upper one (grey, from the chine in to
    the flank)."""
    def section(x):
        chine, foot = strake_half(x)
        f = along(x, GLASS_B[0] - SIDE_BACK, TIP_X - 20)
        return chine, foot, lerp(CHINE[1], 82.0, f), lerp(142.0, 112.0, f), 26.0 + 30 * f, f

    def lower(x):
        chine, foot, cz, top, bottom, f = section(x)
        return [(s * (foot - 2), bottom), (s * (foot + 4), bottom), (s * chine, cz), (s * (foot - 2), cz)]

    def upper(x):
        chine, foot, cz, top, bottom, f = section(x)
        return [(s * (foot - 2), cz), (s * chine, cz), (s * (foot + 14 - 2 * f), top), (s * (foot - 2), top)]
    return lower, upper


STRAKE_X = [TAIL_X, -400.0, 0.0, 400.0, GLASS_B[0] - SIDE_BACK, (GLASS_B[0] - SIDE_BACK + GLASS_B[0]) / 2, GLASS_B[0] - 20,
            880.0, TIP_X - 20]
m.mesh(root, "hullDark", both(lambda s: strip(STRAKE_X, strake_rings(s)[0])))
m.mesh(root, "hullMid", both(lambda s: strip(STRAKE_X, strake_rings(s)[1])))
# the belly under the deck slab, from the tail to the nose
m.mesh(root, "hullDark", strip([TAIL_X, 0.0, GLASS_B[0] - SIDE_BACK],
                               lambda x: [(-WALL_OUT, 35.0), (-WALL_OUT, 28.0), (-200.0, 18.0), (200.0, 18.0),
                                          (WALL_OUT, 28.0), (WALL_OUT, 35.0)]))
# the nose: a rounded loft forward of the nose block, under the windshield's bottom line, the team's blue cap on its tip
rings = [ring_box(GLASS_B[0] - 4, 112, 50, cz=76, n=16), ring_box(866, 96, 48, cz=76, n=16),
         ring_box(902, 66, 38, cz=76, n=16), ring_box(TIP_X - 14, 34, 22, cz=76, n=16)]
m.mesh(root, "hullLight", [loft(rings)])
m.mesh(root, "team", [sphere(36, at=(TIP_X - 14, 0, 76), squash=(0.36, 1.0, 0.64), steps=16)])
m.mesh(root, "hazard", [slab(GLASS_B[0] - 30, 880, -60, 60, 22, 27)], solid=False)  # the chin's hazard plate

# the sides' paint on the leaning flanks: grey fore and aft, the team's blue in two big panels with the white cross, a
# hazard band low down


def on_flank(s, x0, x1, z0, z1, off):
    """A plate on the flank from x0 to x1 and z0 to z1 (its outer face `off` cm proud of the flank)."""
    return quad([(x0, s * (flank_y(z0) + off), z0), (x1, s * (flank_y(z0) + off), z0),
                 (x1, s * (flank_y(z1) + off), z1), (x0, s * (flank_y(z1) + off), z1)], 1.0)


side_grey = []
for x0, x1 in ((-690, -580), (-260, -160), (310, 370)):
    side_grey += both(lambda s, x0=x0, x1=x1: [on_flank(s, x0, x1, 152, 290, 1.0)])
m.mesh(root, "hullLight", side_grey, solid=False)
m.mesh(root, "team", both(lambda s: [on_flank(s, -570, -270, 152, 290, 1.0), on_flank(s, -150, 300, 152, 290, 1.0)]),
       solid=False)
CROSS = (75.0, 221.0)
m.mesh(root, "crossWhite", both(lambda s: [
    on_flank(s, CROSS[0] - 15, CROSS[0] + 15, CROSS[1] - 56, CROSS[1] + 56, 2.5),
    on_flank(s, CROSS[0] - 56, CROSS[0] - 15, CROSS[1] - 15, CROSS[1] + 15, 2.5),
    on_flank(s, CROSS[0] + 15, CROSS[0] + 56, CROSS[1] - 15, CROSS[1] + 15, 2.5)]), solid=False)
m.mesh(root, "hazard", both(lambda s: [on_flank(s, TAIL_X + 6, 560, 142, 149, 1.5)]), solid=False)
# portholes along the flanks, dark rims round cyan glass
PORTS = (-470.0, -380.0, 220.0, 260.0)
m.mesh(root, "hullDark", both(lambda s: [cyl(16, 6, at=(x, s * (flank_y(255) + 1), 255), axis="y", sides=12) for x in PORTS]),
       solid=False)
m.mesh(root, "wallGlow", both(lambda s: [cyl(11, 6, at=(x, s * (flank_y(255) + 2.5), 255), axis="y", sides=12) for x in PORTS]),
       solid=False)
# the white cross on the roof, on a team square over the crest
m.mesh(root, "team", [slab(-60, 120, -56, 56, 446.5, 448.5)], solid=False)
m.mesh(root, "crossWhite", [slab(16, 44, -44, 44, 448.5, 450.5), slab(-14, 16, -14, 14, 448.5, 450.5),
                            slab(44, 74, -14, 14, 448.5, 450.5)], solid=False)
# roof vents and a dorsal spine aft
m.mesh(root, "hullDark", [slab(-420, -200, -26, 26, 440, 458), slab(190, 270, -50, -24, 438, 450),
                          slab(190, 270, 24, 50, 438, 450)])

# --- the windshield's frame, pillars and glass ------------------------------------------------------------------------
# the sill and the header along the glass lines, outside
sill = [bar(G(-SIDE0, -5, 5), G(SIDE0, -5, 5), 12, 16, side=_N),
        bar(G(-SIDE0, GLASS_H + 5, 5), G(SIDE0, GLASS_H + 5, 5), 12, 16, side=_N)]
for s in (1, -1):
    b0, b1, t1, t0 = side_pane(s)
    sill += [bar((b0[0], b0[1], b0[2] - 5), (b1[0], b1[1], b1[2] - 5), 12, 14),
             bar((t0[0], t0[1], t0[2] + 5), (t1[0], t1[1], t1[2] + 5), 12, 14)]
m.mesh(root, "hullDark", sill)
# the pillars: blue and white bands (the team's colour, W2's stripes), riveted
pillars_team, pillars_white = [], []
for s in (1, -1):
    for (bx, by), (tx, ty) in (((GLASS_B[0], s * (PANE + SIDE0) / 2), (GLASS_T[0], s * (PANE + SIDE0) / 2)),
                               ((GLASS_B[0] - SIDE_BACK, s * (SIDE_OUT + 5)), (GLASS_T[0] - SIDE_BACK, s * (SIDE_OUT + 5)))):
        a, b = (bx, by, GLASS_B[1] - 7), (tx, ty, GLASS_T[1] + 7)
        for k in range(8):
            f0, f1 = k / 8.0, (k + 1) / 8.0
            pa = tuple(a[i] + (b[i] - a[i]) * f0 for i in range(3))
            pb = tuple(a[i] + (b[i] - a[i]) * f1 for i in range(3))
            (pillars_team if k % 2 == 0 else pillars_white).append(bar(pa, pb, 18, 16, side=(0, 1, 0)))
m.mesh(root, "team", pillars_team)
m.mesh(root, "crossWhite", pillars_white)
# the rivets on the pillars and the sill, inside and out
rivets = []
for s in (1, -1):
    for k in range(9):
        f = (k + 0.5) / 9.0
        for w in (9.0, -9.0):
            rivets.append(sphere(1.8, at=G(s * (PANE + SIDE0) / 2 + 11 * s, 6 + f * (GLASS_H - 12), w), steps=6))
for k in range(15):
    rivets.append(sphere(1.8, at=G(-91 + 13 * k, -5, -7), steps=6))
m.mesh(root, "chrome", rivets, solid=False)
# the glass: the three panes (the rain on them is AcWindshield's post-process, in the glass's own frame)
glass = m.part("glass", driver=d("the windshield's three panes, faintly tinted; the rain and the wipe are drawn on them "
                                  "by AcWindshield", ("material",)))
panes = [quad([G(-PANE - 6, -2), G(PANE + 6, -2), G(PANE + 6, GLASS_H + 2), G(-PANE - 6, GLASS_H + 2)], 1.2)]
for s in (1, -1):
    panes.append(quad(side_pane(s), 1.2))
m.mesh(glass, "windshield", panes, solid=False)

# --- the wipers: two blades on the centre pane, parked lying left along the sill, turning in the glass's plane -------
WIPER_U = (-45.0, 55.0)   # their pivots across the centre pane
WIPER_L = 120.0           # the blade's reach from the pivot
for k, u in enumerate(WIPER_U):
    piv = G(u, 4, 5)
    w = m.part("wipers_%d" % k, at=piv, rot=(TILT, 0, 0), driver=d(
        "wiper %d: turns about the glass's normal (its local X) with the sweep of ac::Wipers, parked lying left" % k,
        ("rotation",)))
    m.mesh(w, "hullDark", [cyl(5, 6, at=G(u, 4, 2), axis="x", rot=(TILT, 0, 0), base=False, sides=10),
                           bar(G(u, 4, 6), G(u, WIPER_L * 0.55, 7), 5, 2.6, side=_N)], solid=False)
    # the blade's arm, and the blade with its rubber, along the glass
    m.mesh(w, "chrome", [bar(G(u, WIPER_L * 0.55, 7), G(u, WIPER_L, 6), 3, 2.2, side=_N)], solid=False)
    m.mesh(w, "hullDark", [bar(G(u, WIPER_L * 0.18, 4.5), G(u, WIPER_L, 4.5), 1.4, 2.8, side=_N)], solid=False)

# --- the nacelles: octagonal pods high on swept pylons at the shoulders, chrome intakes, the team's cowling, the jets
NAC = (-480.0, 355.0, 330.0)   # the axis's middle (x, |y|, z)
NAC_R, NAC_L = 85.0, 440.0
nac_front = NAC[0] + NAC_L / 2
nac_back = NAC[0] - NAC_L / 2
for k, s in enumerate((1, -1)):
    y = s * NAC[1]
    m.mesh(root, "hullMid", [cyl(NAC_R, NAC_L, at=(NAC[0], y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "chrome", [cyl(NAC_R + 9, 28, at=(nac_front - 8, y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "hullDark", [cyl(NAC_R - 8, 4, at=(nac_front + 8, y, NAC[2]), axis="x", sides=8)])
    # the cowling on the top and outer face, in the team's colour
    m.mesh(root, "team", [slab(nac_back + 34, nac_front - 44, y - 46, y + 46, NAC[2] + NAC_R - 8, NAC[2] + NAC_R + 12),
                          slab(nac_back + 34, nac_front - 44, *sorted((y + s * (NAC_R - 8), y + s * (NAC_R + 12))),
                               NAC[2] - 46, NAC[2] + 46)])
    m.mesh(root, "hullDark", [slab(nac_back + 60, nac_front - 70, *sorted((y + s * (NAC_R + 12), y + s * (NAC_R + 16))),
                                   NAC[2] - 10, NAC[2] + 10)], solid=False)  # a rail on the cowling's flank
    # the pylon to the shoulder, swept back
    PYL = [(-650.0, s * (SKIN - 12)), (-300.0, s * (SKIN - 12)), (-360.0, s * (NAC[1] - NAC_R + 14)),
           (-640.0, s * (NAC[1] - NAC_R + 14))]
    m.mesh(root, "hullMid", [prism(PYL if s > 0 else PYL[::-1], 36, plane="xy", at=(0, 0, NAC[2] - 6))])
    # the jet: open like the intake, a chrome octagonal rim round the dark nozzle, vanes across it, a small warm glow
    # deep in its middle (brighter with the thrust)
    m.mesh(root, "chrome", [cyl(NAC_R + 8, 24, at=(nac_back + 7, y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "hullDark", [cyl(NAC_R - 8, 4, at=(nac_back - 4, y, NAC[2]), axis="x", sides=8)])
    m.mesh(root, "hullDark", [box((2, 9, (NAC_R - 14) * 2), at=(nac_back - 12.5 - 3 * j, y, NAC[2]), rot=(0, 0, 60 * j))
                              for j in range(3)], solid=False)
    jet = m.part("engineGlow_%d" % k, at=(nac_back - 9, y, NAC[2]), handles=["engineGlow"],
                 driver=d("engine %d's jet: a faint glow deep in the nozzle, brighter with the thrust" % k))
    m.mesh(jet, "glow_ff6b14", [cyl(25, 1.2, at=(nac_back - 9, y, NAC[2]), axis="x", sides=8)], solid=False)
    m.part("emitter_%d" % k, parent=jet, at=(nac_back - 40, y, NAC[2]), particles={"handles": "jet"})
    # the fan in the intake
    fan = m.part("fans_%d" % k, at=(nac_front + 11, y, NAC[2]), driver=d("intake fan %d: spins with the thrust" % k,
                                                                         ("rotation",)))
    m.mesh(fan, "chrome", [cyl(14, 8, at=(nac_front + 11, y, NAC[2]), axis="x", sides=10)], solid=False)
    m.mesh(fan, "hullDark", [box((2, 11, NAC_R * 1.6), at=(nac_front + 12 + 3 * j, y, NAC[2]), rot=(0, 0, 30 + 60 * j))
                             for j in range(3)], solid=False)

# --- the fins: twin fins on the nacelles' backs, swept, canted out, the team's panels, red beacons on their tips ------
FIN = [(0, 0), (240, 0), (130, 230), (40, 236)]   # (x from the fin's root's back, z up from the nacelle's top)
FIN_CANT = 14.0
for k, s in enumerate((1, -1)):
    root_at = (nac_back - 60, s * NAC[1], NAC[2] + NAC_R - 8)
    # canted outward: of the two rolls, the one that takes the tip away from the ship's middle
    roll = max((FIN_CANT, -FIN_CANT), key=lambda r: s * m_apply((0, 0, 100), rot3(0, 0, r))[1])
    m.mesh(root, "hullLight", [prism(FIN, 14, plane="xz", at=root_at, rot=(0, 0, roll))])
    m.mesh(root, "team", [prism([(36, 40), (190, 40), (110, 190), (50, 196)], 18, plane="xz", at=root_at, rot=(0, 0, roll))],
           solid=False)
    tip_local = m_apply((85, 0, 240), rot3(0, 0, roll))
    tip = m.part("beacon_%d" % k, at=tuple(root_at[i] + tip_local[i] for i in range(3)),
                 handles=["beacon"], driver=d("fin %d's beacon: blinks" % k))
    m.mesh(tip, "glowRed", [sphere(8, at=tip.at, steps=8)], solid=False)

# --- the landing gear: four blue fairings on the strakes (today's Dropship's boots), feet on the ground, an orange lamp
# on each fairing's toe -----------------------------------------------------------------------------------------------
nav = m.part("navGlow", handles=["navGlow"], driver=d("the orange nav lamps: steady, brighter in flight"))
POD_Y, POD_T = 238.0, 30.0     # the fairings' middle across and their thickness
PODS = {1: [(380, 4), (500, 4), (520, 60), (500, 128), (390, 128)],          # front: the toe forward
        -1: [(-280, 4), (-400, 4), (-420, 60), (-400, 128), (-290, 128)]}   # back
for fb, pts in PODS.items():
    lx = (pts[0][0] + pts[1][0]) / 2
    toe = pts[2][0]
    for s in (1, -1):
        m.mesh(root, "team", [prism(pts, POD_T, plane="xz", at=(0, s * POD_Y, 0))])
        m.mesh(root, "chrome", [cyl(30, 3, at=(lx, s * POD_Y, 0), base=True, sides=12)])
        m.mesh(nav, "glow_ff6b14", [box((8, 12, 10), at=(toe - fb * 4, s * POD_Y, 54), base=True)], solid=False)
# lamps on the nacelles' noses and the tail's corners
m.mesh(nav, "glow_ff6b14", both(lambda s: [box((8, 10, 10), at=(nac_front - 8, s * (NAC[1] + NAC_R + 16), NAC[2] + 30), base=True),
                                           box((8, 10, 10), at=(TAIL_X - 6, s * (SKIN - 10), 360), base=True)]), solid=False)

# === the rear: the opening's frame and the ramp =======================================================================
JAMB = (TAIL_X - 3, BAY_X0)
# the jambs either side of the opening, from the ramp's edge out to the flanks, and the header over it
m.mesh(root, "hullMid", both(lambda s: [slab(JAMB[0], JAMB[1], *sorted((s * (RAMP_W + 2), s * (SKIN - 3))), 26, CEIL_IN + 2)]))
m.mesh(root, "hullMid", [slab(JAMB[0], JAMB[1] - 2, -SKIN + 3, SKIN - 3, CEIL_IN + 2, ROOF0 + 1)])
m.mesh(root, "hazard", both(lambda s: [slab(JAMB[0] - 3, JAMB[0], *sorted((s * (RAMP_W + 4), s * (RAMP_W + 26))), 34, CEIL_IN)]) +
       [slab(JAMB[0] - 3, JAMB[0], -RAMP_W - 4, RAMP_W + 4, CEIL_IN + 6, CEIL_IN + 26)], solid=False)
m.mesh(root, "hazard", [slab(BAY_X0 + 2, BAY_X0 + 24, -RAMP_W + 2, RAMP_W - 2, DECK, DECK + 1)], solid=False)  # the sill
# the tail's hood over the ramp, under the roof's end: a dark underside
m.mesh(root, "hullDark", [slab(-770, TAIL_X - 3, -SKIN + 30, SKIN - 30, ROOF0 - 6, ROOF0 + 6)])

# the ramp: standing in the opening (closed) at rest, hinged at its inner bottom edge; open it lies down to the ground
RAMP_T = 20.0
ramp = m.part("ramp", at=(BAY_X0, 0, DECK), driver=d(
    "the ramp: turns about its hinge (the opening's bottom edge, the ship's Y axis), the top outward and down, "
    "from 0 (shut, standing) to RampOpenDeg (lying on the ground)", ("rotation",)))
m.mesh(ramp, "hullLight", [slab(BAY_X0 - RAMP_T, BAY_X0, -RAMP_W + 2, RAMP_W - 2, DECK + 2, CEIL_IN - 2)])
m.mesh(ramp, "hullDark", [slab(BAY_X0, BAY_X0 + 2, -RAMP_W + 30, RAMP_W - 30, DECK + 16 + 24 * j, DECK + 25 + 24 * j)
                          for j in range(10)], solid=False)  # treads on the inner face (the top, open)
m.mesh(ramp, "hazard", both(lambda s: [slab(BAY_X0, BAY_X0 + 2, *sorted((s * (RAMP_W - 24), s * (RAMP_W - 6))), DECK + 6,
                                            CEIL_IN - 8)]), solid=False)
m.mesh(ramp, "team", [slab(BAY_X0 - RAMP_T - 2, BAY_X0 - RAMP_T, -110, 110, DECK + 50, CEIL_IN - 50)], solid=False)
m.mesh(ramp, "crossWhite", [slab(BAY_X0 - RAMP_T - 4, BAY_X0 - RAMP_T - 2, -14, 14, DECK + 80, CEIL_IN - 80),
                            slab(BAY_X0 - RAMP_T - 4, BAY_X0 - RAMP_T - 2, -56, -14, 176, 204),
                            slab(BAY_X0 - RAMP_T - 4, BAY_X0 - RAMP_T - 2, 14, 56, 176, 204)], solid=False)

# === the bay: octagonal ribs, wall panels and boxes, cabin lamps, tie-down rails, hazard deck edges ====================
RIBS = tuple(-620.0 + 100.0 * k for k in range(12)) + (590.0,)
CH = 30.0  # the ribs' corner chamfer
ribs = []
for x in RIBS:
    ribs += both(lambda s, x=x: [slab(x - 7, x + 7, *sorted((s * BAY_W, s * WALL_IN)), DECK, CEIL_IN - CH),
                                 bar((x, s * BAY_W, CEIL_IN - CH), (x, s * (BAY_W - CH), CEIL_IN), 14, 10, side=(1, 0, 0))])
    ribs.append(slab(x - 7, x + 7, -BAY_W + CH, BAY_W - CH, CEIL_IN, CEIL))
m.mesh(root, "hullDark", ribs)
# wall boxes with cyan lamps (B2), and the pipe runs along the upper walls
boxes, glows = [], []
for s in (1, -1):
    for x in (-570.0, -370.0, -170.0, 30.0, 230.0, 430.0):
        if s < 0 and x < -500:
            continue  # the ramp post's bay
        boxes.append(slab(x - 24, x + 24, *sorted((s * (WALL_IN - 1), s * (WALL_IN - 6))), 150, 216))
        glows.append(slab(x + 8, x + 16, *sorted((s * (WALL_IN - 6), s * (WALL_IN - 7))), 192, 208))
m.mesh(root, "hullMid", boxes, solid=False)
m.mesh(root, "wallGlow", glows, solid=False)
m.mesh(root, "chrome", both(lambda s: [cyl(3, 1180, at=(-60, s * (WALL_IN - 3.5), z), axis="x", sides=8) for z in (276.0, 290.0)]),
       solid=False)
# the deck: tie-down rails (flush, under 3 cm) and the hazard-striped edges
m.mesh(root, "chrome", [slab(BAY_X0 + 30, COCK_X - 30, y - 2, y + 2, DECK, DECK + 1.5) for y in (-130.0, -50.0, 50.0, 130.0)],
       solid=False)
m.mesh(root, "hazard", both(lambda s: [slab(BAY_X0 + 30, COCK_X - 8, *sorted((s * 180, s * (BAY_W - 4))), DECK, DECK + 1)]),
       solid=False)
# the cabin lamps between the ribs (warm, B2), each with its light
for k, x in enumerate((-570.0, -410.0, -250.0, -90.0, 70.0, 230.0, 390.0, 540.0)):
    lamp = m.part("cabinLamps_%d" % k, at=(x, 0, CEIL - 6), handles=["cabinLamp"],
                  light={"attenuationEnd": "4.0", "color": "SIMD4<Float>(1.0, 0.62, 0.3, 1.0)", "handles": "cabinLamp",
                         "intensity": "0.0", "type": "omni"},
                  driver=d("cabin lamp %d: steady warm light in the bay" % k, ("emission", "light")))
    m.mesh(lamp, "hullDark", [slab(x - 30, x + 30, -14, 14, CEIL - 4, CEIL)], solid=False)
    m.mesh(lamp, "cabinGlow", [slab(x - 22, x + 22, -8, 8, CEIL - 5, CEIL - 4)], solid=False)

# --- the ramp post: beside the jamb on the right of the opening (looking in), its lamps on top, the button on its face
POST = (BAY_X0 + 24, -(BAY_W - 12))   # its face's middle (x, y): 12 cm into the clear bay
post_face = POST[1]
m.mesh(root, "hullDark", [slab(POST[0] - 16, POST[0] + 16, -WALL_IN - 4, post_face, DECK, DECK + 128)])
m.mesh(root, "hazard", [slab(POST[0] - 14, POST[0] + 14, post_face, post_face + 2, DECK + 8, DECK + 56)], solid=False)
m.mesh(root, "hullMid", [slab(POST[0] - 12, POST[0] + 12, post_face, post_face + 4, DECK + 50, DECK + 76)], solid=False)  # the button's plate
btn = m.part("rampButton", at=(POST[0], post_face + 4, DECK + 63), driver=d(
    "the ramp button: pushed in 1.5 cm (along +Y) for 0.25 s on each press", ("position",)))
m.mesh(btn, "hazard", [cyl(7, 4, at=(POST[0], post_face + 4, DECK + 63), axis="y", base=True, sides=12)], solid=False)
m.mesh(root, "hullMid", [slab(POST[0] - 14, POST[0] + 14, post_face - 2, post_face + 26, DECK + 128, DECK + 134)])  # the lamp's cap
lamp_open = m.part("rampLamps_0", at=(POST[0], post_face + 14, DECK + 112), handles=["rampLamp"], driver=d(
    "the ramp post's orange lamp: lit while the ramp is open, flashing while it moves"))
m.mesh(lamp_open, "rampOpenGlow", [cyl(11, 14, at=(POST[0], post_face + 13, DECK + 112), axis="y", sides=12, r1=10)],
       solid=False)
m.mesh(root, "chrome", [cyl(12.5, 3, at=(POST[0], post_face + 2, DECK + 112), axis="y", base=True, sides=12)], solid=False)
lamp_shut = m.part("rampLamps_1", at=(POST[0], post_face + 2, DECK + 142), handles=["rampLamp"], driver=d(
    "the ramp post's green lamp: lit while the ramp is shut"))
m.mesh(root, "hullDark", [slab(POST[0] - 7, POST[0] + 7, post_face - 2, post_face + 8, DECK + 134, DECK + 150)])
m.mesh(lamp_shut, "rampShutGlow", [slab(POST[0] - 5, POST[0] + 5, post_face + 8, post_face + 10, DECK + 136, DECK + 148)],
       solid=False)

# --- the console under the glass: three panels, the big green take-off button, screens ----------------------------------
CON = (COCK_X, NOSE_X + 6)    # its front and back (x)
CON_TOP = DECK + 46
CON_W = 160.0
m.mesh(root, "hullMid", [slab(CON[0], CON[1], -CON_W, CON_W, DECK, CON_TOP)])
m.mesh(root, "hullDark", [slab(CON[0] - 2, CON[0], -CON_W + 16, CON_W - 16, DECK + 10, CON_TOP - 30)], solid=False)  # the knee panel
# the sloped desk: up toward the glass
DESK = [(CON[0] - 2, CON_TOP), (CON[1], CON_TOP), (CON[1], CON_TOP + 16), (CON[0] - 2, CON_TOP + 4)]
m.mesh(root, "hullDark", [prism(DESK, CON_W * 2, plane="xz", at=(0, 0, 0))])
DESK_ANG = math.degrees(math.atan2(12, CON[1] - CON[0] + 2))


def desk(x, y, h=0.0):
    """A point on the desk's top at x (along it) and y, h over it."""
    f = (x - (CON[0] - 2)) / (CON[1] - CON[0] + 2)
    z = CON_TOP + 4 + 12 * f
    return (x + h * -math.sin(math.radians(DESK_ANG)), y, z + h * math.cos(math.radians(DESK_ANG)))


panels = []
for y0, y1 in ((-150, -40), (-30, 30), (40, 150)):
    c = desk((CON[0] + CON[1]) / 2, (y0 + y1) / 2, 3)
    panels.append(box((CON[1] - CON[0] - 6, y1 - y0, 6), at=c, rot=(DESK_ANG, 0, 0)))
m.mesh(root, "hullMid", panels)
# switches and dials on the side panels
knobs = []
for y in (-140, -126, -112, -98, -84, -70, -56, 58, 72, 86, 100, 114, 128, 142):
    for x in (CON[0] + 8, CON[0] + 22):
        knobs.append(cyl(2.6, 4, at=desk(x, y, 6), axis="z", rot=(DESK_ANG, 0, 0), base=True, sides=8))
m.mesh(root, "chrome", knobs, solid=False)
for k, y in enumerate((-95.0, 95.0)):
    scr = m.part("consoleScreens_%d" % k, at=desk(CON[1] - 6, y, 6), driver=d(
        "console screen %d: lit while someone is aboard; brighter in flight" % k))
    c = desk(CON[1] - 6, y, 15)
    sx = (math.cos(math.radians(-40)), 0.0, math.sin(math.radians(-40)))  # the housing's local X: its face is -X
    m.mesh(root, "hullDark", [box((4, 46, 28), at=c, rot=(-40, 0, 0))])
    m.mesh(scr, "screenGlow", [box((1, 40, 22), at=tuple(c[i] - 4 * sx[i] for i in range(3)), rot=(-40, 0, 0))], solid=False)
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
log("ferry: tilt %.1f deg, glass %.0f cm high, the take-off button at %s, the ramp button at %s" % (
    TILT, GLASS_H, tuple(round(c) for c in desk(BX, 0, 14)), (POST[0], post_face + 4, DECK + 63)))
