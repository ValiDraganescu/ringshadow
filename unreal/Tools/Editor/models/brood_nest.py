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
from brood_kit import materials  # noqa: E402

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
        "The Matriarch's nest: a ring mound 12 cells across and 1.6 high of overlapping chitin plates and dark rock, "
        "a shallow bowl 8 across inside where she stands, opal ore crystals jutting from it, four clusters of glowing "
        "violet egg pods on its flanks, and seven curved spines up to 5 cells tall round its rim (a broken crown)."])
    materials(m, "queen", strength=1.6)
    m.material("rockDark", parent="hull", color=(0.055, 0.048, 0.050), roughness=0.92)
    m.material("resin", parent="hull", color=(0.10, 0.075, 0.085), metallic=0.1, roughness=0.55)
    m.material("opal", parent="hull", color=(0.52, 0.46, 0.66), metallic=0.65, roughness=0.15)
    # The egg pods: violet shells lit softly from inside (a full glowBody blooms them into flat blobs).
    m.material("eggPod", parent="emissive", color=(0.42, 0.16, 0.62), emissive=(0.09, 0.02, 0.16))
    b = m.part("body", driver=dict(what="the nest itself, standing still", channels=[], functions=[DRIVER]))
    chitin, mid, rock, resin, opal, pods, seams = [], [], [], [], [], [], []

    # The mound: a low earth-and-resin ring under everything.
    for a, c, s in ring(16, 0):
        resin.append(sphere(170, at=(c * 470, s * 470, 0), squash=(1.2, 0.9, 0.5), rot=(0, a, 0), steps=8))
    # Two tiers of big plates leaning out from the bowl, the upper one staggered.
    for tier, (r, z, ln, ht, lean, n) in enumerate(((520, 40, 230, 120, 58, 18), (430, 105, 190, 110, 40, 18))):
        for k, (a, c, s) in enumerate(ring(n, r, phase=10.0 * tier)):
            p = box((60, ln, ht), at=(c * r, s * r, z), rot=(lean, a + 90, 0))
            (mid if (k + tier) % 4 == 0 else chitin).append(p)
    # The rim: a lip of plates round the bowl, a glowing seam under it.
    for k, (a, c, s) in enumerate(ring(20, 400, phase=4)):
        chitin.append(box((50, 140, 40), at=(c * 400, s * 400, 160), rot=(-15, a + 90, 0)))
        if k % 2 == 0:
            seams.append(box((10, 110, 8), at=(c * 372, s * 372, 140), rot=(-15, a + 90, 0)))
    # Dark rock fused into the foot of the mound.
    for k, (a, c, s) in enumerate(ring(11, 600, phase=17)):
        rock.append(sphere(70 + 25 * (k % 3), at=(c * 600, s * 600, 10), squash=(1.3, 1.0, 0.7), rot=(0, a, 0), steps=6))
    # Opal ore crystals jutting out of the plates.
    for k, (a, c, s) in enumerate(ring(13, 540, phase=7)):
        h = 70 + 30 * (k % 3)
        opal.append(box((26, 26, h), at=(c * 540, s * 540, 60 + 15 * (k % 2)), rot=(25 + 10 * (k % 3), a, 0), base=True))
    # The crown: seven curved spines round the rim, each three tapering bars bending inward.
    for k, (a, c, s) in enumerate(ring(7, 420, phase=-8)):
        tall = 1.0 if k % 3 == 0 else 0.75 if k % 3 == 1 else 0.55
        r0, z0 = 420, 150
        pts = []
        for j in range(4):
            t = j / 3.0
            r = r0 + 70 * math.sin(math.pi * t * 0.8) - 120 * t * t
            pts.append((c * r, s * r, z0 + 360 * tall * t))
        for j in range(3):
            w = 70 * (1 - 0.3 * j)
            chitin.append(bar(pts[j], pts[j + 1], w, shape="cyl", taper=0.7))
        chitin.append(cone(18, 1, 60 * tall, at=pts[3], rot=(35, a + 180, 0), base=True))
    # Egg pods: four clusters on the flanks, five or six each, lit from inside.
    for q, (a, c, s) in enumerate(ring(4, 560, phase=30)):
        for j in range(6 if q % 2 == 0 else 5):
            u = (j - 2.5) * 0.17
            ca, sa = math.cos(math.radians(a) + u), math.sin(math.radians(a) + u)
            r = 560 + 35 * (j % 2)
            z = 55 + 55 * (j % 3)
            pods.append(sphere(48 + 8 * (j % 2), at=(ca * r, sa * r, z), squash=(1.0, 0.85, 1.25), rot=(0, a, 0), steps=10))
        # The cluster's stalk into the mound.
        resin.append(sphere(70, at=(c * 500, s * 500, 90), squash=(1.0, 1.4, 0.8), rot=(0, a, 0), steps=6))
    # The bowl's floor: dark, trampled.
    rock.append(sphere(400, at=(0, 0, -30), squash=(1.0, 1.0, 0.09), steps=16))

    m.mesh(b, "resin", resin, solid=False)
    m.mesh(b, "chitin", chitin)
    m.mesh(b, "chitinMid", mid, solid=False)
    m.mesh(b, "rockDark", rock, solid=False)
    m.mesh(b, "opal", opal, solid=False)
    m.mesh(b, "eggPod", pods, solid=False)
    m.mesh(b, "glowDim_queen", seams, solid=False)
    return m


nest().write()
