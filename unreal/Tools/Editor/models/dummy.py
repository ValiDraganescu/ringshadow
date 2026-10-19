"""Target Dummy (C2, docs/guns-and-practice.md sections 2 and 3): a practice target a pilot builds for 10 ore and shoots.
A hazard-striped post on a square steel base, a hinge at its top carrying a plate with a Shardling's silhouette and
three ring marks; the plate falls back on its hinge while the dummy is down and stands again. Blockout (pipeline
steps 1, 6, 7).

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/dummy.py

Writes /Game/Models/dummy/SM_<part>__<material> and the catalog entry `dummy_blue`.

UE cm, a building: its front (+Y) toward the game camera, Z up, the base on Z = 0, a 1 by 1 footprint (Rules radius
0.5: +-50), 1.8 cells high at the plate's top. Parts the pose drives (c2-game): `plate` (turns back about its hinge,
local X at the plate's bottom edge, when the dummy is down: top toward -Y), `rings_0..2` (the ring marks, inner to
outer, lit by hits), `beacon` (the red blink of a built building).
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, prism, sphere  # noqa: E402

POSE = "the target dummy's pose (c2-game)"
HINGE = 100.0      # the hinge's height: the post's top, the plate's bottom edge
PLATE_Y = 9.0      # the plate's front face
CENTRE = 140.0     # the silhouette's and the rings' middle

m = Model("dummy", category="building", coplanar_check=True, notes=[
    "C2's Target Dummy (docs/guns-and-practice.md section 3). Pose: the plate falls back on its hinge while the dummy "
    "is down, rings_0..2 light with hits, beacon.",
    "Under construction the body rises out of the ground like every building. height 1.8"])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "glowRed"):
    m.material(name)
# the ring marks: an ember lamp, its hue the base colour (the unlit master adds it)
m.material("dummyRing", parent="emissive", color=(0.5, 0.12, 0.02), emissive=(2.4, 0.6, 0.08))


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


# --- the base: a steel square, hazard bands on its top edges (butted at the corners), a plinth for the post ----------
m.mesh(m.root, "hullDark", [box((90, 90, 10), base=True)])
m.mesh(m.root, "hazard", [box((90, 8, 1.5), at=(0, s * 41, 10), base=True) for s in (-1, 1)] +
       [box((8, 74, 1.5), at=(s * 41, 0, 10), base=True) for s in (-1, 1)], solid=False)
m.mesh(m.root, "hullMid", [box((40, 40, 8), at=(0, 0, 10), base=True)])
m.mesh(m.root, "team", [box((40, 2, 4), at=(0, 21, 12), base=True)], solid=False)
# --- the post, hazard striped, a bracket at its top and the hinge pin before it --------------------------------------
m.mesh(m.root, "hazard", [box((12, 12, HINGE - 18 - 8), at=(0, 0, 18), base=True)])
m.mesh(m.root, "hullDark", [box((18, 14, 8), at=(0, 0, HINGE - 8), base=True)])  # the pin stands 2 cm proud of it
m.mesh(m.root, "chrome", [cyl(2.5, 34, at=(-17, PLATE_Y - 2.5, HINGE), axis="x", base=True, sides=10)])
# the beacon's mast at the base's back right corner
m.mesh(m.root, "chrome", [cyl(1.5, 52, at=(36, -36, 10), base=True, sides=8)], solid=False)
beacon = m.part("beacon", at=(36, -36, 66), driver=d("beacon blink (0.25 s of every 1.2 s once built)", ("visibility",)))
m.mesh(beacon, "glowRed", sphere(3.5, at=(36, -36, 66), steps=8), solid=False)

# --- the plate on its hinge: steel, a Shardling's silhouette, three dashed ring marks ------------------------------
plate = m.part("plate", at=(0, PLATE_Y - 2.5, HINGE), driver=d(
    "the plate: turns back about its hinge (local X, its bottom edge) by down x 80 degrees, its top toward -Y, while "
    "the dummy is down", ("rotation",)))
m.mesh(plate, "hullLight", [box((72, 3, 78), at=(0, PLATE_Y - 1.5, HINGE + 2), base=True)])
m.mesh(plate, "hullDark", [box((76, 3, 4), at=(0, PLATE_Y - 1.5, HINGE + 80), base=True)], solid=False)  # its top lip
# a Shardling seen from the front: a crystal body, shards up, legs splayed (x, z from the middle)
SHARDLING = [(0, 34), (-6, 18), (-14, 28), (-16, 12), (-26, 16), (-22, 2), (-30, -10), (-24, -12), (-18, -4),
             (-14, -22), (-9, -22), (-8, -8), (0, -14), (8, -8), (9, -22), (14, -22), (18, -4), (24, -12), (30, -10),
             (22, 2), (26, 16), (16, 12), (14, 28), (6, 18)]
m.mesh(plate, "hullDark", [prism(SHARDLING, 2, "xz", at=(0, PLATE_Y + 1, CENTRE))], solid=False)
# the rings 2 cm over the silhouette, dashed (whole rings of boxes overlap at their corners: shared faces)
for k, r in enumerate((9.0, 18.0, 27.0)):
    n = 8 + 6 * k
    w = 2.4
    seg = 2 * (r - w / 2) * math.tan(math.pi / n) * 0.8
    ring = m.part("rings_%d" % k, parent=plate, at=(0, PLATE_Y + 3.75, CENTRE),
                  driver=d("ring mark %d (inner to outer): lit by hits" % k))
    # segment j at angle a round the ring (x = r sin a, z = r cos a), its long side along the ring (pitch -a)
    m.mesh(ring, "dummyRing", [box((seg, 1.5, w), at=(math.sin(a) * r, PLATE_Y + 3.75, CENTRE + math.cos(a) * r),
                                   rot=(-math.degrees(a), 0, 0))
                               for a in (2 * math.pi * j / n for j in range(n))], solid=False)

m.write()
