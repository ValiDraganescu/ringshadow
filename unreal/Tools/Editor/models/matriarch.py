"""The Matriarch: the Brood's giant queen (docs/creatures.md; concept art/models/matriarch/concepts/matriarch-c1-crowned.jpg).
A towering segmented body raised high on six massive legs, two great hooked forelimbs, a head with a cluster of violet
eyes and heavy mandibles, a fan crown of glowing violet spines tipped teal behind it, clusters of glowing egg pods on
her flanks and a huge glowing egg sac behind. Readable from across a region: 9 cells tall, the crown lit.

    uv run unreal/Tools/hidden_run.py --out unreal/server/build/n1/model --python unreal/Tools/Editor/models/matriarch.py

Writes /Game/Models/matriarch/SM_<part>__<material> and the catalog entry `matriarch_blue`.

UE cm, +X forward, +Y right, Z up, the feet on Z = 0; about 1,000 long with the egg sac, 800 wide over the legs, 900
high at the crown. Legs 0..2 on the left (-Y) front to back, 3..5 on the right. Parts the pose moves: body, abdomen
(the egg sac), head, jaw, crown, forelimb_0/1, leg_0..5, pods.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ac_modelkit import Model, box, cyl, sphere, bar, loft, ring_box  # noqa: E402
from brood_kit import materials, flecks, scatter  # noqa: E402

POSE = "AcPoseBrood.cpp:PoseMatriarch"

m = Model("matriarch", category="unit", notes=[
    "The Brood's giant queen (docs/creatures.md), never leaves her nest. Pose: PoseMatriarch (N3): slow breath (body "
    "and egg sac swell 3%), turning to her target, forelimb slam, spit lunge, the roar (rears 1 cell, crown fans open, "
    "jaw gapes), death (sinks, crown folds, glow dies). No team colour: violet glow fixed.",
    "Parts: body, abdomen, head, jaw, crown, forelimb_0/1, leg_0..5, pods."])
GLOW, DIM = materials(m, "queen")
m.material("glowTip", parent="emissive", color=(1, 1, 1), emissive=(0.15, 1.6, 1.3))


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


# --- body: a low segmented trunk on the legs, rising into a raised torso ------------------------------------------------
body = m.part("body", at=(0, 0, 220), driver=d("body: slow breath, turns to its target, rears up 1 cell on the roar, sinks on death", ("position", "rotation", "scale")))
TRUNK = [(190, 90, 70), (120, 140, 95), (30, 160, 105), (-60, 150, 100), (-140, 120, 85), (-190, 80, 60)]
m.mesh(body, "chitin", loft([ring_box(x, w, h, 0, 220, 16, 2.6) for x, w, h in TRUNK]))
m.mesh(body, "chitinUnder", loft([ring_box(x, w * 0.85, h * 0.5, 0, 220 - h * 0.55, 12, 2.2) for x, w, h in TRUNK]), solid=False)
# the raised torso: segments stacked up and forward, each a little narrower
TORSO = [(200, 300, 120, 95), (235, 390, 112, 88), (262, 470, 102, 80), (280, 545, 90, 72)]  # x, z, half width, half depth
for i, (x, z, w, dp) in enumerate(TORSO):
    m.mesh(body, "chitin", sphere(1, at=(x, 0, z), rot=(0, -20, 0), squash=(dp, w, 88)))
    m.mesh(body, "chitinMid", sphere(1, at=(x - 30, 0, z + 18), rot=(0, -20, 0), squash=(dp * 0.7, w * 1.02, 22)), solid=False)  # plate lips
    m.mesh(body, DIM, [box((dp * 1.2, 4, 6), at=(x, s * (w - 4), z - 30), rot=(0, -20, 0)) for s in (-1, 1)], solid=False)    # glow seams
# glowing seams and ribs along the trunk
for x, w, h in TRUNK[1:-1]:
    m.mesh(body, "chitinMid", box((14, w * 2.04, h * 1.6), at=(x + 40, 0, 236)), solid=False)
    m.mesh(body, DIM, [box((20, 5, 8), at=(x + 20, s * (w - 2), 200)) for s in (-1, 1)], solid=False)
m.mesh(body, "chitin", [bar((x, 0, 220 + h - 6), (x - 30, 0, 220 + h + 50), 22, 18, taper=0.15, side=(0, 1, 0))
                        for x, w, h in TRUNK[1:5]], solid=False)                          # dorsal spikes
m.mesh(body, "oreFleck", flecks(scatter(0, 0, 230, 180, 150, 100, 40, seed=7), r=6), solid=False)

# --- the egg sac: huge, glowing, banded ---------------------------------------------------------------------------------
abdomen = m.part("abdomen", parent=body, at=(-190, 0, 240), driver=d("egg sac: swells with her breath, pulses brighter on the roar", ("scale", "emission")))
m.mesh(abdomen, "glowBody_queen", sphere(200, at=(-370, 0, 250), squash=(1.2, 0.95, 0.95), steps=20))
m.mesh(abdomen, "chitin", [cyl(r, 14, at=(-370 + dx, 0, 250), axis="x") for dx, r in ((-170, 116), (-75, 186), (35, 180), (125, 118))], solid=False)
m.mesh(abdomen, "glowTip", [sphere(1, at=(-370 + 175 * math.cos(a) * 1.15, 175 * math.sin(a) * 0.9, 275 + 100 * math.sin(3 * a)), squash=(9, 9, 9), steps=6)
                            for a in [k * 2 * math.pi / 12 for k in range(12)]], solid=False)  # teal veins' nodes

# --- head, eyes, jaw -----------------------------------------------------------------------------------------------------
head = m.part("head", parent=body, at=(300, 0, 600), driver=d("head: turns to its target, lunges on the spit, lifts on the roar"))
m.mesh(head, "chitin", sphere(1, at=(345, 0, 615), squash=(85, 78, 62)))
m.mesh(head, "chitinMid", box((90, 150, 22), at=(360, 0, 668), rot=(0, -14, 0)), solid=False)          # brow ridge
m.mesh(head, GLOW, [sphere(r, at=(418, y, z), steps=8) for y, z, r in
                    ((-26, 640, 13), (26, 640, 13), (-50, 622, 9), (50, 622, 9), (-14, 666, 8), (14, 666, 8), (0, 618, 7))])
jaw = m.part("jaw", parent=head, at=(380, 0, 575), driver=d("jaw: gapes on the roar and the spit"))
m.mesh(jaw, "chitinMid", box((80, 110, 28), at=(410, 0, 560), rot=(0, 12, 0)))
for s in (-1, 1):
    m.mesh(jaw, "chitin", [bar((420, s * 46, 560), (470, s * 66, 520), 26, 22, taper=0.6), bar((470, s * 66, 520), (500, s * 26, 480), 20, 16, taper=0.15)])
m.mesh(jaw, DIM, box((30, 70, 12), at=(440, 0, 568)), solid=False)                                   # the glowing throat

# --- the crown: a fan of glowing spines behind the head ------------------------------------------------------------
crown = m.part("crown", parent=head, at=(300, 0, 660), driver=d("crown: breathes with the body's glow; fans open (spreads and scales out) on the roar, folds on death", ("rotation", "scale", "emission")))
m.mesh(crown, "chitin", sphere(1, at=(300, 0, 660), squash=(50, 110, 60)))
N = 13
for k in range(N):
    a = math.radians(-78 + 156 * k / (N - 1))       # the fan's angle from straight up, toward +Y
    ln = 300 - 70 * abs(a) / math.radians(78)
    base = (300, math.sin(a) * 60, 660 + math.cos(a) * 40)
    tip = (300 - ln * 0.45, math.sin(a) * (60 + ln * 0.85), 660 + math.cos(a) * (40 + ln * 0.85))
    mid = tuple(base[i] + (tip[i] - base[i]) * 0.7 for i in range(3))
    m.mesh(crown, "chitin", bar(base, mid, 26, 16, taper=0.6, side=(1, 0, 0)))
    m.mesh(crown, GLOW, bar(tuple(base[i] + (tip[i] - base[i]) * 0.1 for i in range(3)), mid, 12, 18, taper=0.6, side=(1, 0, 0)), solid=False)
    m.mesh(crown, "glowTip", bar(mid, tip, 12, 8, taper=0.1, side=(1, 0, 0)))

# --- two great forelimbs, hooked ---------------------------------------------------------------------------------------
for i, s in enumerate((-1, 1)):
    sh = (250, s * 105, 470)
    el = (380, s * 230, 400)
    wr = (470, s * 200, 150)
    f = m.part("forelimb_%d" % i, parent=body, at=sh, driver=d("forelimb: rise over 0.8 s and crash down on the slam; braced on the roar"))
    m.mesh(f, "chitin", [bar(sh, el, 62, 54), sphere(42, at=el, steps=10), bar(el, wr, 52, 46, taper=0.8)])
    m.mesh(f, "chitinMid", [bar(wr, (530, s * 160, 0), 40, 34, taper=0.2), bar((wr[0] - 10, wr[1], wr[2] + 20), (440, s * 150, 60), 22, 18, taper=0.15)])
    m.mesh(f, DIM, [box((40, 6, 8), at=(315, s * 168, 452)), box((40, 6, 8), at=(425, s * 215, 270))], solid=False)

# --- six massive legs ----------------------------------------------------------------------------------------------------
for k in range(6):
    s = -1 if k < 3 else 1
    x = (110, -10, -130)[k % 3]
    lean = (60, 0, -60)[k % 3]
    hip = (x, s * 130, 200)
    knee = (x + lean * 0.5, s * 300, 340)
    foot = (x + lean, s * 400, 0)
    leg = m.part("leg_%d" % k, parent=body, at=hip, driver=d("leg: a slow shifting step when she turns (0, 2, 4 against 1, 3, 5)"))
    m.mesh(leg, "chitin", [bar(hip, knee, 52, 46), sphere(34, at=knee, steps=10)])
    m.mesh(leg, "chitinMid", bar(knee, foot, 44, 38, taper=0.3))
    m.mesh(leg, DIM, box((28, 6, 8), at=(knee[0], knee[1], knee[2] + 34)), solid=False)

# --- egg pods: clusters on her flanks ------------------------------------------------------------------------------------
pods = m.part("pods", parent=body, at=(200, 0, 380), driver=d("egg pods: pulse with the breath; flare when Shardlings hatch on the roar", ("emission", "scale")))
pp = []
for s in (-1, 1):
    for j, (x, z) in enumerate(((170, 360), (210, 420), (150, 430), (230, 350), (190, 480), (130, 380), (250, 450))):
        pp.append(sphere(30 + (j % 3) * 6, at=(x, s * (130 + (j % 2) * 18), z), squash=(0.8, 0.8, 1.15), steps=10))
m.mesh(pods, "glowBody_queen", pp)
m.mesh(pods, "chitinMid", [sphere(1, at=(190, s * 120, 410), squash=(90, 30, 100)) for s in (-1, 1)], solid=False)  # the sockets they hang from

m.write()
