"""Shardling: the Brood's ground swarmer (docs/creatures.md; concept art/models/shardling/concepts/shardling-c1-quill.jpg).
A long, low, segmented body on six splayed legs, a ridge of opal crystal quills on its back glowing teal at their roots,
a wide head with two hooked mandibles. Charcoal chitin with opal flecks; teal light from the plate seams and the eyes.

    uv run unreal/Tools/hidden_run.py --out unreal/server/build/n1/model --python unreal/Tools/Editor/models/shardling.py

Writes /Game/Models/shardling/SM_<part>__<material> and the catalog entry `shardling_blue`.

UE cm, +X forward, +Y right, Z up, the feet on Z = 0; 125 long with the mandibles, 70 high at the quills, 130 wide over
the feet (a big dog). Legs 0..2 on the left (-Y) front to back, 3..5 on the right; each leg is one rigid part pivoting at
its hip. Parts the pose moves (docs/creatures.md, "Models and how to drive them"): body, head, mandible_0/1, leg_0..5.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ac_modelkit import Model, box, sphere, bar, loft, ring_box  # noqa: E402
from brood_kit import materials, painted, flecks, scatter  # noqa: E402

POSE = "AcPoseBrood.cpp:PoseShardling"

m = Model("shardling", category="unit", coplanar_check=True, notes=[
    "The Brood's ground swarmer, dog-sized (docs/creatures.md). Pose: PoseShardling (N2): tripod gait, body bob, "
    "lunge and mandible snap on each attack, flinch, death roll. No team colour: teal glow fixed.",
    "Parts: body, head, mandible_0/1, leg_0..5."])
GLOW, DIM = materials(m, "teal")
_EGG, _GLASS, OPAL = painted(m, "teal")
# N1b: the quills are opal (iridescent, teal-violet-gold with the view), not white cones.
m.material("quill", parent="broodOpal", color=(0.16, 0.15, 0.20), metallic=0.3, roughness=0.15,
           emissive=(0.0, 0.035, 0.03), params={"OpalGain": 3.2, "PatternScale": 0.25})


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


Z = 36.0  # the body axis
# --- body: five segments, plates overlapping back to front --------------------------------------------------------------
body = m.part("body", at=(0, 0, Z), driver=d("body: bob of the gait, lunge forward on a bite, roll on death", ("position", "rotation")))
SEG = [(40, 17, 11), (24, 22, 14), (6, 25, 16), (-14, 24, 15), (-32, 20, 13), (-48, 13, 9), (-58, 6, 5)]  # x, half width, half height
m.mesh(body, "chitin", loft([ring_box(x, w, h, 0, Z + 2, 14, 2.6) for x, w, h in SEG]))
# (the belly's front ring 2 cm behind the body's: their front caps shared a plane)
m.mesh(body, "chitinUnder", loft([ring_box(x - (2 if i == 0 else 0), w * 0.8, h * 0.55, 0, Z - h * 0.45, 12, 2.4)
                                  for i, (x, w, h) in enumerate(SEG[:-1])]), solid=False)
# plate ridges: a lip at the back of each segment, a glow seam under it on the flanks
for x, w, h in SEG[1:-1]:
    m.mesh(body, "chitinMid", box((4, w * 2.05, h * 1.1), at=(x + 6, 0, Z + 4)), solid=False)
    m.mesh(body, DIM, [box((6, 2, 3), at=(x - 1, s * (w - 0.5), Z - 2)) for s in (-1, 1)], solid=False)  # (clear of the lip)
m.mesh(body, GLOW, [box((70, 3, 2.5), at=(-6, s * 17, Z - 9)) for s in (-1, 1)], solid=False)   # the teal fringe along the belly
# the quills: a ridge of opal crystal shards, two rows, leaning back, glowing teal at their roots
QUILLS = [(30, 0, 22, 8), (18, -9, 26, 9), (16, 9, 24, 9), (4, 0, 34, 11), (-6, -11, 28, 9), (-8, 11, 30, 10),
          (-18, 0, 30, 10), (-28, -9, 22, 8), (-30, 8, 24, 8), (-42, 0, 18, 7), (-50, -5, 12, 5), (-50, 6, 12, 5)]
for x, y, h, w in QUILLS:
    root = (x, y, Z + 14)
    tip = (x - h * 0.35, y * 1.6, Z + 14 + h)
    m.mesh(body, "quill", bar(root, tip, w, w * 0.8, taper=0.12, side=(1, 0, 0)))
    m.mesh(body, GLOW, sphere(w * 0.45, at=(x, y, Z + 15), squash=(1.2, 1.2, 0.5), steps=6), solid=False)
m.mesh(body, OPAL, flecks(scatter(-8, 0, Z + 4, 48, 22, 14, 26, seed=2), r=2.4), solid=False)  # opal ore crusted on the plates

# --- head and mandibles ------------------------------------------------------------------------------------------------
head = m.part("head", parent=body, at=(40, 0, Z + 2), driver=d("head: dips and thrusts on a bite, turns to its target"))
m.mesh(head, "chitin", sphere(17, at=(50, 0, Z + 1), squash=(1.0, 1.15, 0.7)))
m.mesh(head, "chitinMid", box((14, 30, 5), at=(54, 0, Z + 10), rot=(0, -12, 0)), solid=False)   # brow plate
m.mesh(head, GLOW, [sphere(4.2, at=(60, s * 10, Z + 5), steps=8) for s in (-1, 1)]
       + [sphere(2.6, at=(63, s * 4.5, Z + 8), steps=6) for s in (-1, 1)])                        # four eyes
m.mesh(head, DIM, box((6, 14, 4), at=(64, 0, Z - 6)), solid=False)                                 # the glowing maw
for i, s in enumerate((-1, 1)):
    root = (60, s * 9, Z - 4)
    mid = (74, s * 20, Z - 7)
    tip = (86, s * 6, Z - 9)
    md = m.part("mandible_%d" % i, parent=head, at=root, driver=d("mandible: twitches at rest, opens wide and snaps shut on a bite"))
    m.mesh(md, "chitinMid", [bar(root, mid, 8, 6, taper=0.8), bar(mid, tip, 6.4, 5, taper=0.25)])
    m.mesh(md, "quill", [bar((70, s * 17, Z - 6), (72, s * 13, Z - 2), 2.6, taper=0.2),
                           bar((78, s * 16, Z - 8), (79, s * 12, Z - 4), 2.4, taper=0.2)], solid=False)  # teeth

# --- six legs, splayed wide ---------------------------------------------------------------------------------------------
for k in range(6):
    s = -1 if k < 3 else 1
    row = k % 3
    x = (24, 2, -24)[row]
    lean = (24, 4, -22)[row]
    hip = (x, s * 18, Z - 2)
    knee = (x + lean * 0.4, s * 46, 56)
    foot = (x + lean, s * 64, 0)
    leg = m.part("leg_%d" % k, parent=body, at=hip, driver=d("leg: swing and lift of the tripod gait (0, 2, 4 against 1, 3, 5)"))
    m.mesh(leg, "chitin", [bar(hip, knee, 9, 8), sphere(5.5, at=knee, steps=8)])
    m.mesh(leg, "chitinMid", bar(knee, foot, 7.5, 6.5, taper=0.3))
    m.mesh(leg, DIM, sphere(2.4, at=(knee[0], knee[1], knee[2] + 4), steps=5), solid=False)        # a glow at each joint

m.write()
