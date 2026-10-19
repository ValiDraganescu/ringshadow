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
from ac_modelkit import Model, box, cyl, sphere, bar, loft, ring_box, v_add, v_sub  # noqa: E402
from brood_kit import materials, painted, flecks, scatter  # noqa: E402

POSE = "AcPoseBrood.cpp:PoseMatriarch"

m = Model("matriarch", category="unit", coplanar_check=True, notes=[
    "The Brood's giant queen (docs/creatures.md), never leaves her nest. Pose: PoseMatriarch (N3): slow breath (body "
    "and egg sac swell 3%), turning to her target, forelimb slam, spit lunge, the roar (rears 1 cell, crown fans open, "
    "jaw gapes), death (sinks, crown folds, glow dies). No team colour: violet glow fixed.",
    "Parts: body, abdomen, head, jaw, crown, forelimb_0/1, leg_0..5, pods."])
GLOW, DIM = materials(m, "queen")
EGG, GLASS, OPAL = painted(m, "queen")
# N1b: the crown's lamps saturated (the full-strength violet bloomed pink-white): a violet core strip, teal tips.
m.material("glowTip", parent="emissive", color=(0.02, 0.5, 0.4), emissive=(0.04, 1.15, 0.85))
m.material("crownGlow", parent="emissive", color=(0.25, 0.04, 0.5), emissive=(0.42, 0.06, 0.85))


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


# --- body: a low segmented trunk on the legs, rising into a raised torso ------------------------------------------------
body = m.part("body", at=(0, 0, 220), driver=d("body: slow breath, turns to its target, rears up 1 cell on the roar, sinks on death", ("position", "rotation", "scale")))
TRUNK = [(190, 90, 70), (120, 140, 95), (30, 160, 105), (-60, 150, 100), (-140, 120, 85), (-190, 80, 60)]
m.mesh(body, "chitin", loft([ring_box(x, w, h, 0, 220, 16, 2.6) for x, w, h in TRUNK]))
# (the belly's end rings 2 cm inside the body's: their caps shared a plane)
m.mesh(body, "chitinUnder", loft([ring_box(x - (2 if i == 0 else 0) + (2 if i == len(TRUNK) - 1 else 0), w * 0.85, h * 0.5, 0,
                                           220 - h * 0.55, 12, 2.2) for i, (x, w, h) in enumerate(TRUNK)]), solid=False)
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
m.mesh(body, OPAL, flecks(scatter(0, 0, 230, 180, 150, 100, 40, seed=7), r=9), solid=False)  # opal ore crusted on her plates

# --- the egg sac: huge, glowing, banded ---------------------------------------------------------------------------------
abdomen = m.part("abdomen", parent=body, at=(-190, 0, 240), driver=d("egg sac: swells with her breath, pulses brighter on the roar", ("scale", "emission")))
m.mesh(abdomen, EGG, sphere(200, at=(-370, 0, 250), squash=(1.2, 0.95, 0.95), steps=28))
m.mesh(abdomen, "chitin", [cyl(r, 14, at=(-370 + dx, 0, 250), axis="x") for dx, r in ((-170, 116), (-75, 186), (35, 180), (125, 118))], solid=False)
# the plated underside the sac rests on (the veins are the egg material's own)
m.mesh(abdomen, "chitinMid", [box((90, 70, 30), at=(-370 + 110 * math.cos(a), 150 * math.sin(a), 95 - 2 * (k % 2)), rot=(0, math.degrees(a), 0))
                              for k, a in enumerate([k * 2 * math.pi / 10 for k in range(10)])], solid=False)  # (neighbours 2 cm apart)

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
m.mesh(jaw, DIM, box((30, 70, 16), at=(440, 0, 568)), solid=False)  # (2 cm over the jaw's top)                                   # the glowing throat

# --- the crown: a fan of glowing spines behind the head ------------------------------------------------------------
crown = m.part("crown", parent=head, at=(300, 0, 660), driver=d("crown: breathes with the body's glow; fans open (spreads and scales out) on the roar, folds on death", ("rotation", "scale", "emission")))
m.mesh(crown, "chitin", sphere(1, at=(300, 0, 660), squash=(50, 110, 60)))
N = 13
spines = []
for k in range(N):
    a = math.radians(-78 + 156 * k / (N - 1))       # the fan's angle from straight up, toward +Y
    ln = 300 - 70 * abs(a) / math.radians(78)
    base = (300, math.sin(a) * 60, 660 + math.cos(a) * 40)
    tip = (300 - ln * 0.45, math.sin(a) * (60 + ln * 0.85), 660 + math.cos(a) * (40 + ln * 0.85))
    mid = tuple(base[i] + (tip[i] - base[i]) * 0.7 for i in range(3))
    m.mesh(crown, "chitin", bar(base, mid, 26, 16, taper=0.6, side=(1, 0, 0)))
    m.mesh(crown, "crownGlow", bar(tuple(base[i] + (tip[i] - base[i]) * 0.1 for i in range(3)), mid, 12, 18, taper=0.6, side=(1, 0, 0)), solid=False)
    m.mesh(crown, "glowTip", bar(mid, tip, 14, 10, taper=0.1, side=(1, 0, 0)))
    spines.append((base, mid))
# the fan's violet membrane between the spines, lit through, to 60% up
for (b0, m0), (b1, m1) in zip(spines, spines[1:]):
    lo0, lo1 = [tuple(b[i] + (mm[i] - b[i]) * 0.12 for i in range(3)) for b, mm in ((b0, m0), (b1, m1))]
    hi0, hi1 = [tuple(b[i] + (mm[i] - b[i]) * 0.8 for i in range(3)) for b, mm in ((b0, m0), (b1, m1))]
    t = (3, 0, 0)
    m.mesh(crown, GLASS, loft([[v_sub(lo0, t), v_sub(lo1, t), v_add(lo1, t), v_add(lo0, t)],
                               [v_sub(hi0, t), v_sub(hi1, t), v_add(hi1, t), v_add(hi0, t)]]), solid=False)

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
    # N1b: big lilac pods on her shoulders and flanks, as in the concept (they read from across the nest)
    for j, (x, z) in enumerate(((170, 360), (210, 425), (145, 440), (235, 350), (195, 500), (125, 380), (255, 460),
                                (230, 545), (165, 530))):
        r = 40 + (j % 3) * 8
        pp.append(sphere(r, at=(x, s * (150 + (j % 2) * 26 + r * 0.3), z), squash=(0.85, 0.8, 1.2), rot=(0, 0, s * 12), steps=14))
m.mesh(pods, EGG, pp)
m.mesh(pods, "chitinMid", [sphere(1, at=(190, s * 120, 410), squash=(90, 30, 100)) for s in (-1, 1)], solid=False)  # the sockets they hang from

m.write()
