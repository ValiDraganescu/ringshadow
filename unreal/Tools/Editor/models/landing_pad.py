"""Landing Pad: where the Dropship sets down (F2, docs/mmorpg-m8.md). A low steel deck of 3 by 3 cells the Prospector
walks onto: hazard-striped edges, a landing ring and a centre cross in the team's colour, four corner posts with the
building's yellow working lamps, a short ramp lip on each side, and the red beacon on one post. Blockout (pipeline
steps 1, 6, 7).

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/landing_pad.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/landingPad/SM_<part>__<material> and the catalog entry `landingPad_blue`.

UE cm, a building: its front (+Y) toward the game camera, Z up, the deck on Z = 0 and 18 cm high (a walkable deck,
`Rules::walkable`), 300 cm a side. The pose (`PoseLandingPad`) drives `lamps_0..3` (steady yellow, alternating while
the Dropship is over the pad) and `beacon`.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, sphere  # noqa: E402

POSE = "AcPoseBuildings2.cpp:PoseLandingPad"

m = Model("landingPad", category="building", coplanar_check=True, notes=[
    "F2's Landing Pad (docs/mmorpg-m8.md). Pose: PoseLandingPad: lamps_0..3 the working light (steady, alternating "
    "while the Dropship is over it), beacon.",
    "Under construction the deck rises out of the ground like every building. height 0.18"])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "workGlow", "glowRed"):
    m.material(name)


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


# --- the deck: a dark sub-frame, the steel plate, sloped lips all round so the Prospector walks on ------------------
m.mesh(m.root, "hullDark", [box((300, 300, 8), base=True)])
m.mesh(m.root, "hullMid", [box((284, 284, 10), at=(0, 0, 8), base=True)])
lips = [box((300, 14, 6), at=(0, s * 157, 0), base=True) for s in (-1, 1)] + \
       [box((14, 300, 6), at=(s * 157, 0, 0), base=True) for s in (-1, 1)]
m.mesh(m.root, "hullLight", lips)
# deck plates: a grid of seams (those along x cut at those along y: crossing seams share their tops)
SEAMS = (-71, 0, 71)
cuts = [-142] + [c for a in SEAMS for c in (a - 1, a + 1)] + [142]
m.mesh(m.root, "hullDark", [box((cuts[2 * i + 1] - cuts[2 * i], 2, 1), at=((cuts[2 * i] + cuts[2 * i + 1]) / 2, y, 18), base=True)
                            for y in SEAMS for i in range(4)] +
       [box((2, 284, 1), at=(x, 0, 18), base=True) for x in SEAMS], solid=False)
# hazard bands along the edges
m.mesh(m.root, "hazard", [box((250, 10, 3), at=(0, s * 134, 18), base=True) for s in (-1, 1)] +
       [box((10, 250, 3), at=(s * 134, 0, 18), base=True) for s in (-1, 1)], solid=False)  # 2 cm over the seams
# the landing ring and the centre cross, the team's colour
ring = []
for k in range(24):
    a0 = k * 2 * math.pi / 24
    # (20 long, a gap between them: whole segments overlapped at their corners and shared their tops)
    ring.append(box((20, 8, 1.5), at=(math.cos(a0) * 92, math.sin(a0) * 92, 18.5), rot=(0, math.degrees(a0) + 90, 0), base=True))
# the cross: one bar whole, the other in two halves butted against it
m.mesh(m.root, "team", ring + [box((90, 14, 1.5), at=(0, 0, 18.5), base=True)] +
       [box((14, 38, 1.5), at=(0, s * 26, 18.5), base=True) for s in (-1, 1)], solid=False)
m.mesh(m.root, "chrome", [cyl(10, 4, at=(0, 0, 18.5), base=True, sides=16)], solid=False)  # 2.5 cm over the cross

# --- the corner posts and their lamps ------------------------------------------------------------------------------
posts = []
for sx in (-1, 1):
    for sy in (-1, 1):
        posts.append(box((16, 16, 34), at=(sx * 138, sy * 138, 8), base=True))
m.mesh(m.root, "hullLight", posts)
m.mesh(m.root, "hullDark", [box((20, 20, 4), at=(sx * 138, sy * 138, 42), base=True) for sx in (-1, 1) for sy in (-1, 1)])
# lamps numbered screen left to right, front pair first (front: +Y)
corners = [(-1, 1), (1, 1), (-1, -1), (1, -1)]
for k, (sx, sy) in enumerate(corners):
    lamp = m.part(f"lamps_{k}", at=(sx * 138, sy * 138, 46), driver=d(f"corner lamp {k}: the working light"))
    m.mesh(lamp, "workGlow", [box((12, 12, 8), at=(sx * 138, sy * 138, 46), base=True)], solid=False)
beacon = m.part("beacon", at=(-138, -138, 56), driver=d("beacon blink (0.25 s of every 1.2 s once built)", ("visibility",)))
m.mesh(beacon, "glowRed", sphere(5, at=(-138, -138, 58), steps=8), solid=False)

m.write()
