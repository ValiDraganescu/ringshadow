"""Gloamwing: the Brood's flyer (docs/creatures.md; concept art/models/gloamwing/concepts/gloamwing-c1-lantern.jpg).
A small armoured thorax, a head with big violet eyes, four long translucent violet wings, long trailing barbed legs,
and a swollen glassy abdomen hanging under it like a lantern lit from inside. From the high camera: a violet light.

    uv run unreal/Tools/hidden_run.py --out unreal/server/build/n1/model --python unreal/Tools/Editor/models/gloamwing.py

Writes /Game/Models/gloamwing/SM_<part>__<material> and the catalog entry `gloamwing_blue`.

UE cm, +X forward, +Y right, Z up. Authored perched: the thorax 80 up, the legs hanging to the ground; the pose lifts
`thorax` to its flying height (2.5 cells) and dives it. 180 wingspan, 120 long. Wings 0 and 1 are the left pair (front,
back), 2 and 3 the right; legs 0, 1 left, 2, 3 right. Parts the pose moves: thorax, head, abdomen, wing_0..3, leg_0..3.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ac_modelkit import Model, box, cyl, sphere, bar, prism  # noqa: E402
from brood_kit import materials, painted, flecks, scatter  # noqa: E402

POSE = "AcPoseBrood.cpp:PoseGloamwing"

m = Model("gloamwing", category="unit", notes=[
    "The Brood's flyer, person-sized (docs/creatures.md). Pose: PoseGloamwing (N2): thorax lifted to its hover height "
    "and bobbing, wings flapping (two pairs out of phase), abdomen swaying, nose-down on the move, the dive and spit, "
    "falling on death. No team colour: violet glow fixed.",
    "Parts: thorax, head, abdomen, wing_0..3, leg_0..3."])
GLOW, DIM = materials(m, "violet")
EGG, _GLASS, OPAL = painted(m, "violet")
# N1b: the wings a violet membrane lit through, veined, clearer at the middle (make_brood_materials, broodGlass).
m.material("gloamWing", parent="broodGlass", blend="translucent", double_sided=True, color=(0.26, 0.10, 0.40),
           roughness=0.1, emissive=(0.10, 0.015, 0.22), opacity=0.25,
           params={"VeinColor": (0.26, 0.08, 0.55), "PatternScale": 0.12})


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


Z = 80.0
# --- thorax: the armoured middle, the root of everything --------------------------------------------------------------
thorax = m.part("thorax", at=(0, 0, Z), driver=d("thorax: lifted to the hover height, bobs, pitches nose-down on the move and in the dive", ("position", "rotation")))
m.mesh(thorax, "chitin", sphere(20, at=(2, 0, Z), squash=(1.25, 0.9, 0.85)))
m.mesh(thorax, "chitinMid", [box((26, 22, 6), at=(4, 0, Z + 15), rot=(0, 6, 0)), box((14, 26, 5), at=(-12, 0, Z + 12), rot=(0, -10, 0))])
m.mesh(thorax, "chitin", [bar((-8, s * 10, Z + 14), (-16, s * 16, Z + 24), 5, taper=0.2) for s in (-1, 1)], solid=False)  # shoulder spikes
m.mesh(thorax, OPAL, flecks(scatter(2, 0, Z + 4, 22, 16, 15, 12, seed=5), r=2.0), solid=False)

head = m.part("head", parent=thorax, at=(22, 0, Z + 2), driver=d("head: turns to its target, jerks back on the spit"))
m.mesh(head, "chitin", sphere(12, at=(30, 0, Z + 1), squash=(1.0, 1.05, 0.9)))
m.mesh(head, GLOW, [sphere(6.0, at=(33, s * 8.5, Z + 5), squash=(0.9, 0.7, 1.0), steps=10) for s in (-1, 1)])  # big eyes
m.mesh(head, "chitinMid", [bar((38, s * 4, Z - 4), (46, s * 2, Z - 12), 3.0, taper=0.2) for s in (-1, 1)])     # mouth hooks
m.mesh(head, DIM, sphere(3.0, at=(41, 0, Z - 4), steps=6), solid=False)                                          # the acid in its mouth
m.mesh(head, "chitin", [bar((32, s * 5, Z + 9), (44, s * 12, Z + 22), 1.6, shape="cyl", taper=0.4) for s in (-1, 1)], solid=False)  # antennae

# --- the lantern: a swollen glassy abdomen, lit from inside, hanging below and behind --------------------------------
abdomen = m.part("abdomen", parent=thorax, at=(-14, 0, Z - 8), driver=d("abdomen: sways behind the flight, flares bright on the spit", ("rotation", "emission")))
m.mesh(abdomen, EGG, sphere(24, at=(-34, 0, Z - 34), rot=(0, 35, 0), squash=(0.85, 0.85, 1.25), steps=16))  # the lantern
m.mesh(abdomen, "chitin", [cyl(r, 3.2, at=(-34 + 13 * t, 0, Z - 34 + 20 * t), axis="z", rot=(0, 35, 0))
                           for t, r in ((-1.25, 13), (-0.6, 22), (0.0, 25), (0.6, 22), (1.15, 15))], solid=False)  # chitin bands over the glow
m.mesh(abdomen, "chitinMid", sphere(9, at=(-18, 0, Z - 10), squash=(1.2, 1, 1)))                                 # waist
m.mesh(abdomen, "chitin", sphere(7, at=(-50, 0, Z - 58), steps=8))                                                 # tip cap

# --- four wings: long blades, translucent violet, glowing veins ----------------------------------------------------
WINGS = [(-1, 6, 92, 22, 14), (-1, -8, 76, 18, -8), (1, 6, 92, 22, 14), (1, -8, 76, 18, -8)]  # side, x, length, chord, sweep
for i, (s, x, ln, ch, sweep) in enumerate(WINGS):
    root = (x, s * 9, Z + 16)
    w = m.part("wing_%d" % i, parent=thorax, at=root, driver=d("wing: flaps fast (about 18 Hz), the pairs out of phase; folds a little in the dive"))
    blade = [(0, 0), (ch * 0.5, s * ln * 0.25), (ch * 0.35 - sweep * 0.5, s * ln * 0.8), (-sweep, s * ln), (-ch * 0.6 - sweep, s * ln * 0.75), (-ch * 0.55, s * ln * 0.2)]
    m.mesh(w, "gloamWing", prism([(px, py) for px, py in blade], 1.2, plane="xy", at=root, rot=(0, 0, -s * 18)), solid=False)
    vein = [(0, 0), (ch * 0.1 - sweep * 0.5, s * ln * 0.95)]
    tip = (root[0] + vein[1][0], root[1] + vein[1][1] * 0.95, root[2] + ln * 0.95 * 0.31)
    m.mesh(w, DIM, bar(root, tip, 1.6, 1.0), solid=False)
    m.mesh(w, "chitin", bar(root, (root[0] + ch * 0.3, root[1] + s * ln * 0.3, root[2] + ln * 0.3 * 0.31), 3.0, 2.0), solid=False)  # leading edge

# --- legs: long, trailing, barbed ---------------------------------------------------------------------------------------
LEGS = [(-1, 10, 26), (-1, -6, 16), (1, 10, 26), (1, -6, 16)]  # side, x of the hip, reach forward
for i, (s, x, fwd) in enumerate(LEGS):
    hip = (x, s * 9, Z - 12)
    knee = (x + fwd * 0.5, s * 22, Z - 34)
    foot = (x - 10, s * 18, 4)
    leg = m.part("leg_%d" % i, parent=thorax, at=hip, driver=d("leg: dangles and trails behind in flight, reaches forward in the dive"))
    m.mesh(leg, "chitin", [bar(hip, knee, 4.2, 3.6), sphere(3.0, at=knee, steps=6)])
    m.mesh(leg, "chitinMid", bar(knee, foot, 3.4, 2.8, taper=0.3))
    m.mesh(leg, "chitin", [bar(((knee[0] + foot[0]) * 0.5, (knee[1] + foot[1]) * 0.5, (knee[2] + foot[2]) * 0.5),
                                ((knee[0] + foot[0]) * 0.5 + 6, (knee[1] + foot[1]) * 0.5 + s * 4, (knee[2] + foot[2]) * 0.5 + 3), 2.0, taper=0.1)], solid=False)  # barb

m.write()
