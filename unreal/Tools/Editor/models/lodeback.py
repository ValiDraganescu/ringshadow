"""Lodeback: the Brood's ore eater (docs/creatures.md; concept art/models/lodeback/concepts/lodeback-c1-hermit.jpg).
A crab-like body on four thick jointed legs, two short digging claws by its broad jaw, a wide crusted carapace, and on
its back a glassy sac where the swallowed ore glows gold and opal; the sac swells and brightens as it fills.

    uv run unreal/Tools/hidden_run.py --out unreal/server/build/n1/model --python unreal/Tools/Editor/models/lodeback.py

Writes /Game/Models/lodeback/SM_<part>__<material> and the catalog entry `lodeback_blue`.

UE cm, +X forward, +Y right, Z up, the feet on Z = 0; 220 long with the claws, 230 wide over the feet, 130 high at the
carapace, 185 at a full sac (a small car). Legs 0, 1 on the left (-Y) front and back, 2, 3 on the right. The sac is
authored full; the pose scales it from its base (0.55 empty to 1.0 full) with the sac fill. Parts the pose moves: body,
head, jaw, claw_0/1, leg_0..3, sac.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ac_modelkit import Model, box, sphere, bar, loft, ring_box  # noqa: E402
from brood_kit import materials, painted, flecks, scatter  # noqa: E402

POSE = "AcPoseBrood.cpp:PoseLodeback"

m = Model("lodeback", category="unit", coplanar_check=True, notes=[
    "The Brood's ore eater, car-sized (docs/creatures.md). Pose: PoseLodeback (N2): slow four-leg walk, the claws and "
    "jaw digging while it eats, the sac scaled 0.55..1.0 and lit by its fill, a bite when cornered, the sac bursting on "
    "death. No team colour: gold glow fixed.",
    "Parts: body, head, jaw, claw_0/1, leg_0..3, sac."])
GLOW, DIM = materials(m, "gold")
EGG, GLASS, OPAL = painted(m, "gold")
# N1b: the sac a gold-lit glass where the swallowed ore shows through: opal ore shapes round a gold glow inside.
m.material("sacGlass", parent="broodGlass", blend="translucent", double_sided=True, color=(0.30, 0.18, 0.05),
           roughness=0.05, emissive=(0.34, 0.17, 0.02), opacity=0.12,
           params={"VeinColor": (0.55, 0.30, 0.06), "PatternScale": 0.06})
m.material("oreInside", parent="broodOpal", color=(0.05, 0.04, 0.035), metallic=0.5, roughness=0.15,
           emissive=(0.12, 0.06, 0.008), params={"OpalGain": 2.6, "PatternScale": 0.2})
m.material("oreNodule", parent="hull", color=(0.30, 0.34, 0.40), metallic=0.6, roughness=0.15)
m.material("opalTeal", parent="emissive", color=(1, 1, 1), emissive=(0.1, 1.4, 1.1))
m.material("opalViolet", parent="emissive", color=(1, 1, 1), emissive=(1.1, 0.2, 1.5))


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


Z = 72.0
# --- body: a broad, flat carapace, crusted --------------------------------------------------------------------------------
body = m.part("body", at=(0, 0, Z), driver=d("body: heavy sway of the walk, sinks on death", ("position", "rotation")))
SEC = [(70, 40, 18), (52, 66, 28), (24, 80, 32), (-10, 82, 32), (-44, 70, 27), (-66, 44, 18), (-74, 18, 9)]  # x, half width, half height
m.mesh(body, "chitin", loft([ring_box(x, w, h, 0, Z, 16, 2.4) for x, w, h in SEC]))
# (the belly's front ring 2 cm behind the body's: their front caps shared a plane)
m.mesh(body, "chitinUnder", loft([ring_box(x - (2 if i == 0 else 0), w * 0.85, h * 0.5, 0, Z - h * 0.55, 12, 2.2)
                                  for i, (x, w, h) in enumerate(SEC[:-1])]), solid=False)
# the carapace's crust: raised plates and lumps, and gold seams between them
for x, y, sx, sy in ((40, -34, 34, 30), (40, 34, 34, 30), (6, -48, 40, 30), (6, 48, 40, 30), (-30, -40, 36, 30), (-30, 40, 36, 30)):
    m.mesh(body, "chitinMid", sphere(1, at=(x, y, Z + 22), squash=(sx * 0.5, sy * 0.5, 9)), solid=False)
m.mesh(body, DIM, [box((120, 3, 3), at=(-2, s * 70, Z - 4)) for s in (-1, 1)] + [box((3, 110, 3), at=(56, 0, Z + 14))], solid=False)
m.mesh(body, "oreFleck", flecks(scatter(0, 0, Z + 6, 70, 74, 28, 30, seed=3), r=2.4), solid=False)

# --- head, eyes, jaw ---------------------------------------------------------------------------------------------------
head = m.part("head", parent=body, at=(66, 0, Z - 8), driver=d("head: digs into the deposit while it eats, thrusts on a bite"))
m.mesh(head, "chitin", sphere(30, at=(80, 0, Z - 10), squash=(0.75, 1.15, 0.6)))
m.mesh(head, GLOW, [sphere(5.5, at=(98, s * 14, Z - 2), steps=8) for s in (-1, 1)])               # two gold eyes on short stalks
m.mesh(head, "chitinMid", [bar((92, s * 12, Z - 8), (98, s * 14, Z - 3), 5, shape="cyl") for s in (-1, 1)], solid=False)
jaw = m.part("jaw", parent=head, at=(92, 0, Z - 22), driver=d("jaw: grinds while it eats, snaps on a bite"))
m.mesh(jaw, "chitinMid", box((26, 46, 9), at=(102, 0, Z - 25), rot=(0, 10, 0)))                    # the broad flat jaw
m.mesh(jaw, "oreNodule", [box((4, 5, 6), at=(114, y, Z - 22)) for y in (-16, -8, 0, 8, 16)], solid=False)  # grinding teeth
m.mesh(head, DIM, box((8, 34, 5), at=(100, 0, Z - 16)), solid=False)                                # the glowing maw

# --- two short digging claws ---------------------------------------------------------------------------------------------
for i, s in enumerate((-1, 1)):
    sh = (70, s * 34, Z - 12)
    el = (100, s * 52, Z - 30)
    wr = (118, s * 34, 18)
    c = m.part("claw_%d" % i, parent=body, at=sh, driver=d("claw: scrapes ore into the jaw while it eats; pinches on a bite"))
    m.mesh(c, "chitin", [bar(sh, el, 16, 14), sphere(10, at=el, steps=8), bar(el, wr, 15, 13, taper=0.85)])
    m.mesh(c, "chitinMid", [bar(wr, (wr[0] + 22, wr[1] - s * 6, 6), 13, 10, taper=0.25),
                            bar((wr[0] - 4, wr[1] + s * 8, wr[2]), (wr[0] + 14, wr[1] + s * 2, 4), 9, 8, taper=0.25)])
    m.mesh(c, DIM, sphere(3, at=(el[0], el[1], el[2] + 9), steps=5), solid=False)

# --- four thick crab legs ----------------------------------------------------------------------------------------------
for k in range(4):
    s = -1 if k < 2 else 1
    x = (24, -36)[k % 2]
    lean = (14, -18)[k % 2]
    hip = (x, s * 64, Z - 6)
    knee = (x + lean * 0.5, s * 102, 104)
    foot = (x + lean, s * 116, 0)
    leg = m.part("leg_%d" % k, parent=body, at=hip, driver=d("leg: slow walk, two legs at a time (0, 3 against 1, 2)"))
    m.mesh(leg, "chitin", [bar(hip, knee, 20, 17), sphere(12, at=knee, steps=8)])
    m.mesh(leg, "chitinMid", bar(knee, foot, 17, 15, taper=0.35))
    m.mesh(leg, DIM, box((10, 3, 3), at=(knee[0], knee[1], knee[2] + 11)), solid=False)

# --- the sac: glassy, gold-lit, ore nodules showing through ---------------------------------------------------------
SB = (-22, 0, Z + 26)  # its base on the back: the pose scales it from here
sac = m.part("sac", parent=body, at=SB, driver=d("sac: scale 0.55 empty to 1.0 full with the sac fill, its glow with it; bursts (scale to 0) on death", ("scale", "emission")))
m.mesh(sac, "sacGlass", sphere(58, at=(-26, 0, Z + 72), squash=(1.15, 1.0, 0.8), steps=20))
m.mesh(sac, EGG, sphere(26, at=(-26, 0, Z + 66), squash=(1.2, 1.0, 0.8), steps=12), solid=False)   # the gold glow at its heart
# the swallowed ore inside the glass: angular opal chunks round the glow
nod = []
for i in range(14):
    a = 2 * math.pi * i / 14 + 0.4
    e = -0.2 + 0.75 * ((i * 5) % 7) / 7
    nod.append((-26 + 44 * math.cos(e) * math.cos(a) * 1.1, 38 * math.cos(e) * math.sin(a), Z + 70 + 28 * math.sin(e)))
m.mesh(sac, "oreInside", [box((14 + (i % 3) * 5, 11 + (i % 2) * 5, 16 + (i % 4) * 4), at=p, rot=(20 * (i % 5), 37 * i, 15 * (i % 3)))
                          for i, p in enumerate(nod)], solid=False)
m.mesh(sac, "opalTeal", [sphere(3.5, at=p, steps=5) for p in nod[::3]], solid=False)
m.mesh(sac, "opalViolet", [sphere(3.0, at=(p[0], p[1], p[2] + 8), steps=5) for p in nod[1::3]], solid=False)
m.mesh(sac, "chitinMid", [sphere(1, at=(-26 + 10 * s, 0, Z + 28), squash=(70, 56, 10)) for s in (0,)], solid=False)  # the rim it sits in

m.write()
