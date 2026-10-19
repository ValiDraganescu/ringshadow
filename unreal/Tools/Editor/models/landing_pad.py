"""Landing Pad: where the Dropship sets down (F2, docs/mmorpg-m8.md). A low steel deck the Prospector walks onto and the
walk-in Dropship sits on: hazard-striped edges, a landing ring and a centre cross in the team's colour, four corner posts
with the building's yellow working lamps, a short ramp lip on each side, and the red beacon on one post. Blockout
(pipeline steps 1, 6, 7).

C5 (docs/building-sizes.md, 2026-10-19): the pad's size follows the Dropship (`Rules::radius(landingPad)`, 11 by 11
cells with today's ship), so it is made at 11 by 11 (it was 3 by 3) and the game stretches it to its footprint
(`AcPose::BuildingScale`). The deck, its plates, the ring and the cross stretch with it; what stands at a Prospector's
scale is parts of its own the pose keeps at its size (`AcBuildingPose::KeepSize`): `post_0..3` at the deck's corners
(each post, its lamp `lamps_k` and on the back left one the beacon) and `edge_0..3` (each side's lip and hazard band,
pivot on the deck's edge: they stretch along the side only).

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/landing_pad.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/landingPad/SM_<part>__<material> and the catalog entry `landingPad_blue`.

UE cm, a building: its front (+Y) toward the game camera, Z up, the deck on Z = 0 and 18 cm high (a walkable deck,
`Rules::walkable`), `SIDE` cm a side. The pose (`PoseLandingPad`) drives `lamps_0..3` (steady yellow, alternating while
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
SIDE = 1100         # C5: 11 cells (Rules::radius(landingPad) 5.5 in the MMO with today's Dropship)
HALF = SIDE / 2
PLATE = SIDE - 16   # the plate inset 8 cm all round
m.mesh(m.root, "hullDark", [box((SIDE, SIDE, 10), at=(0, 0, -2), base=True)])  # 2 under the ground (the call post stands on 0)
m.mesh(m.root, "hullMid", [box((PLATE, PLATE, 10), at=(0, 0, 8), base=True)])
# deck plates: a grid of seams about a cell apart (those along x cut at those along y: crossing seams share their tops)
SEAMS = tuple(range(-400, 401, 100))
cuts = [-PLATE / 2] + [c for a in SEAMS for c in (a - 1, a + 1)] + [PLATE / 2]
m.mesh(m.root, "hullDark", [box((cuts[2 * i + 1] - cuts[2 * i], 2, 1), at=((cuts[2 * i] + cuts[2 * i + 1]) / 2, y, 18), base=True)
                            for y in SEAMS for i in range(len(SEAMS) + 1)] +
       [box((2, PLATE, 1), at=(x, 0, 18), base=True) for x in SEAMS], solid=False)
# the landing ring (round the ship's legs) and the centre cross, the team's colour
RING = 350
ring = []
for k in range(48):
    a0 = k * 2 * math.pi / 48
    # (34 long, a gap between them: whole segments overlapped at their corners and shared their tops)
    ring.append(box((34, 14, 1.5), at=(math.cos(a0) * RING, math.sin(a0) * RING, 18.5), rot=(0, math.degrees(a0) + 90, 0), base=True))
# the cross: one bar whole, the other in two halves butted against it
m.mesh(m.root, "team", ring + [box((320, 30, 1.5), at=(0, 0, 18.5), base=True)] +
       [box((30, 145, 1.5), at=(0, s * 87.5, 18.5), base=True) for s in (-1, 1)], solid=False)
m.mesh(m.root, "chrome", [cyl(14, 4, at=(0, 0, 18.5), base=True, sides=16)], solid=False)  # 2.5 cm over the cross

# --- the edges: each side's sloped lip and its hazard band, a Prospector's scale across (KeepSize across the side) ---
# edge_0 front (+Y), edge_1 back, edge_2 left (-X), edge_3 right; the pivot on the deck's edge.
for k, (ax, sg) in enumerate((("y", 1), ("y", -1), ("x", -1), ("x", 1))):
    pivot = (0, sg * HALF, 0) if ax == "y" else (sg * HALF, 0, 0)
    edge = m.part(f"edge_{k}", at=pivot, driver=d(f"edge {k}: kept a Prospector's size across the side", ("scale",)))
    if ax == "y":
        lip = box((SIDE, 14, 6), at=(0, sg * (HALF + 7), 0), base=True)
        band = box((SIDE - 100, 10, 3), at=(0, sg * (HALF - 16), 18), base=True)
    else:
        lip = box((14, SIDE, 6), at=(sg * (HALF + 7), 0, 0), base=True)
        band = box((10, SIDE - 100, 3), at=(sg * (HALF - 16), 0, 18), base=True)
    m.mesh(edge, "hullLight", [lip])
    m.mesh(edge, "hazard", [band], solid=False)  # 2 cm over the seams

# --- the corner posts and their lamps, a Prospector's scale 12 cm in from the deck's corners (KeepSize) -------------
# lamps numbered screen left to right, front pair first (front: +Y); the beacon on the back left post.
corners = [(-1, 1), (1, 1), (-1, -1), (1, -1)]
for k, (sx, sy) in enumerate(corners):
    px, py = sx * (HALF - 12), sy * (HALF - 12)
    post = m.part(f"post_{k}", at=(sx * HALF, sy * HALF, 0), driver=d(f"corner post {k}: kept a Prospector's size", ("scale",)))
    m.mesh(post, "hullLight", [box((16, 16, 34), at=(px, py, 8), base=True)])
    m.mesh(post, "hullDark", [box((20, 20, 4), at=(px, py, 42), base=True)])
    lamp = m.part(f"lamps_{k}", parent=post, at=(px, py, 46), driver=d(f"corner lamp {k}: the working light"))
    m.mesh(lamp, "workGlow", [box((12, 12, 8), at=(px, py, 46), base=True)], solid=False)
    if (sx, sy) == (-1, -1):
        beacon = m.part("beacon", parent=post, at=(px, py, 56), driver=d("beacon blink (0.25 s of every 1.2 s once built)", ("visibility",)))
        m.mesh(beacon, "glowRed", sphere(5, at=(px, py, 58), steps=8), solid=False)

m.write()
