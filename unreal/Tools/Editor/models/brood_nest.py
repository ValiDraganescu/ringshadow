"""The Matriarch's nest (docs/creatures.md, "The Matriarch"; N3): a hive mound of chitin plates, broken opal ore and
dark rock round a shallow bowl where she stands, clusters of glowing egg pods on its flanks and a broken crown of curved
spines on its rim, readable from far off. A landmark doodad: a region file's `nest` puts it down
(`Doodad::Kind::broodNest`, walked over: no solid feet in `NavGrid::footprint`), drawn by AAcDoodads (`doodad_<kind>`).

    uv run unreal/Tools/hidden_run.py --out <dir> --python unreal/Tools/Editor/models/brood_nest.py

Writes /Game/Models/doodad_broodNest/SM_body__<material> and the catalog entry `doodad_broodNest` (category doodad: no
team, no pose; one part, `body`). The concept is art/models/broodNest/concepts/c1-mound.jpg.

UE cm, Z = 0 the ground at the nest's point; a cell is 100 cm. Her footprint is 3.5 cells: the bowl is 4 across inside.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ac_modelkit import Model, bar, box, cone, sphere  # noqa: E402
from brood_kit import materials, painted  # noqa: E402

DRIVER = "AcDoodads.cpp:AAcDoodads::Rebuild"


def ring(n, r, phase=0.0):
    """n angles (degrees) and their unit directions round the centre."""
    out = []
    for k in range(n):
        a = 360.0 * k / n + phase
        out.append((a, math.cos(math.radians(a)), math.sin(math.radians(a))))
    return out


def nest():
    m = Model("doodad_broodNest", category="doodad", notes=[
        "The Matriarch's nest (N1b, painted): a ring mound 12.5 cells across and 2.5 high of overlapping chitin plates in "
        "three tiers and dark rock, a bowl 8 across inside where she stands, ore veins glowing violet and teal in the "
        "seams between the tiers, opal ore crystals jutting from it, five clusters of veined egg pods lit from inside on "
        "its flanks, and seven curved horn spines up to 5 cells tall round its rim (a broken crown), tipped teal."])
    materials(m, "queen", strength=1.6)
    egg, _glass, opal = painted(m, "queen")
    m.material("rockDark", parent="hull", color=(0.055, 0.048, 0.050), roughness=0.92)
    m.material("resin", parent="hull", color=(0.10, 0.075, 0.085), metallic=0.1, roughness=0.55)
    # The opal crystals glow a little of their own (they read at dusk); the ore veins in the seams light the mound.
    m.material("opalLit", parent="broodOpal", color=(0.12, 0.10, 0.16), metallic=0.35, roughness=0.2,
               emissive=(0.05, 0.02, 0.10), params={"OpalGain": 1.8, "PatternScale": 0.03})
    m.material("oreVein", parent="emissive", color=(0.22, 0.04, 0.45), emissive=(0.55, 0.10, 1.10))
    m.material("oreVeinTeal", parent="emissive", color=(0.02, 0.32, 0.26), emissive=(0.05, 0.85, 0.65))
    # The mound's plates a shade lighter than a bug's chitin, so its tiers read beside her dark body.
    m.material("plate", parent="hull", color=(0.085, 0.072, 0.082), metallic=0.2, roughness=0.55)
    m.material("hornTip", parent="emissive", color=(0.02, 0.4, 0.32), emissive=(0.04, 0.9, 0.7))
    b = m.part("body", driver=dict(what="the nest itself, standing still", channels=[], functions=[DRIVER]))
    chitin, plates, mid, rock, resin, crystals, pods, veins, teal, tips = [], [], [], [], [], [], [], [], [], []

    # The mound: a high earth-and-resin ring under everything.
    for a, c, s in ring(18, 0):
        resin.append(sphere(210, at=(c * 480, s * 480, 0), squash=(1.2, 0.9, 0.85), rot=(0, a, 0), steps=8))
    # Three tiers of big plates leaning out from the bowl, each staggered over the one below; between the tiers a
    # seam of glowing ore (violet, every third run teal).
    tiers = ((570, 45, 250, 150, 58, 20), (490, 130, 220, 140, 42, 20), (425, 205, 190, 115, 26, 20))
    for tier, (r, z, ln, ht, lean, n) in enumerate(tiers):
        for k, (a, c, s) in enumerate(ring(n, r, phase=9.0 * tier)):
            p = box((60, ln, ht), at=(c * r, s * r, z), rot=(lean, a + 90, 0))
            (mid if (k + tier) % 4 == 0 else plates).append(p)
        if tier < 2:
            rs, zs = r - 30 - 15 * tier, z + ht * 0.55
            for k, (a, c, s) in enumerate(ring(n, rs, phase=9.0 * tier + 9)):
                (teal if k % 3 == 0 else veins).append(box((22, ln * 0.75, 16), at=(c * rs, s * rs, zs), rot=(lean - 10, a + 90, 0)))
    # The rim: a lip of plates round the bowl, a glowing seam under it, and glowing seams down the bowl's inner wall
    # (the bowl lit from within).
    for k, (a, c, s) in enumerate(ring(22, 405, phase=4)):
        chitin.append(box((55, 140, 45), at=(c * 405, s * 405, 265), rot=(-15, a + 90, 0)))
        if k % 2 == 0:
            veins.append(box((10, 110, 9), at=(c * 378, s * 378, 245), rot=(-15, a + 90, 0)))
        if k % 3 == 1:
            (teal if k % 2 else veins).append(box((10, 14, 150), at=(c * 372, s * 372, 140), rot=(-12, a + 90, 0)))
    # Dark rock fused into the foot of the mound.
    for k, (a, c, s) in enumerate(ring(13, 640, phase=17)):
        rock.append(sphere(80 + 30 * (k % 3), at=(c * 640, s * 640, 10), squash=(1.3, 1.0, 0.75), rot=(0, a, 0), steps=6))
    # Opal ore crystals jutting out of the plates, in twos and threes.
    for k, (a, c, s) in enumerate(ring(15, 560, phase=7)):
        for j in range(2 + k % 2):
            h = 90 + 40 * ((k + j) % 3)
            aj = math.radians(a + (j - 1) * 5)
            r = 560 - 40 * j
            crystals.append(box((34, 34, h), at=(math.cos(aj) * r, math.sin(aj) * r, 70 + 50 * (k % 3) + 20 * j),
                                rot=(25 + 12 * ((k + j) % 3), a + 15 * (j - 1), 0), base=True))
    # The crown: seven curved horn spines round the rim, each three tapering bars bending inward, a teal tip.
    for k, (a, c, s) in enumerate(ring(7, 420, phase=-8)):
        tall = 1.0 if k % 3 == 0 else 0.75 if k % 3 == 1 else 0.55
        r0, z0 = 420, 255
        pts = []
        for j in range(4):
            t = j / 3.0
            r = r0 + 80 * math.sin(math.pi * t * 0.8) - 130 * t * t
            pts.append((c * r, s * r, z0 + 400 * tall * t))
        for j in range(3):
            w = 80 * (1 - 0.3 * j)
            chitin.append(bar(pts[j], pts[j + 1], w, shape="cyl", taper=0.7))
        tips.append(cone(20, 1, 70 * tall, at=pts[3], rot=(35, a + 180, 0), base=True))
    # Egg pods: five clusters on the flanks, four or five each, lit from inside (accents: the mound shows between).
    for q, (a, c, s) in enumerate(ring(5, 610, phase=30)):
        for j in range(5 if q % 2 == 0 else 4):
            u = (j - 2.0) * 0.1
            ca, sa = math.cos(math.radians(a) + u), math.sin(math.radians(a) + u)
            r = 610 + 35 * (j % 2)
            z = 55 + 60 * (j % 2)
            pods.append(sphere(48 + 8 * (j % 2), at=(ca * r, sa * r, z), squash=(1.0, 0.85, 1.25), rot=(0, a, 0), steps=14))
        # The cluster's stalk into the mound.
        resin.append(sphere(95, at=(c * 530, s * 530, 120), squash=(1.0, 1.5, 0.9), rot=(0, a, 0), steps=6))
    # The bowl's floor: dark, trampled.
    rock.append(sphere(400, at=(0, 0, -30), squash=(1.0, 1.0, 0.09), steps=16))

    m.mesh(b, "resin", resin, solid=False)
    m.mesh(b, "chitin", chitin)
    m.mesh(b, "plate", plates)
    m.mesh(b, "chitinMid", mid, solid=False)
    m.mesh(b, "rockDark", rock, solid=False)
    m.mesh(b, "opalLit", crystals, solid=False)
    m.mesh(b, egg, pods, solid=False)
    m.mesh(b, "oreVein", veins, solid=False)
    m.mesh(b, "oreVeinTeal", teal, solid=False)
    m.mesh(b, "hornTip", tips, solid=False)
    return m


nest().write()
