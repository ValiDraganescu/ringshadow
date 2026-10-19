"""A canyon zone's marks (Z1, docs/regions.md "zones"): the Brood's tunnel mouth at each entrance, and the survey
marker on each build spot. Doodads the region build puts down (`Doodad::Kind::broodTunnel`, `buildSpot`), drawn by
AAcDoodads (`doodad_<kind>`); both walked over or through (no solid feet in `NavGrid::footprint`).

    uv run unreal/Tools/hidden_run.py --out <dir> --python unreal/Tools/Editor/models/canyon.py

Writes /Game/Models/doodad_broodTunnel/ and /Game/Models/doodad_buildSpot/ (SM_body__<material>) and their catalog
entries (category doodad: no team, no pose; one part, `body`).

UE cm, Z = 0 the ground at the doodad's point, +X the way it faces (a tunnel: out of its wall, into the canyon; the
region file's `facing`). A cell is 100 cm.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ac_modelkit import Model, box, cyl, sphere, bar  # noqa: E402
from brood_kit import materials  # noqa: E402

DRIVER = "AcDoodads.cpp:AAcDoodads::Rebuild"


def body(m):
    return m.part("body", driver=dict(what="the mark itself, standing still", channels=[], functions=[DRIVER]))


def arc(n, rx, rz, a0=8.0, a1=172.0):
    """n points round a half ellipse over the ground in the YZ plane (angles from +Y over the top to -Y)."""
    out = []
    for k in range(n):
        a = math.radians(a0 + (a1 - a0) * k / max(1, n - 1))
        out.append((a, math.cos(a) * rx, math.sin(a) * rz))
    return out


# --- the Brood's tunnel: a black hollow in the canyon wall, framed by rock and chitin, a faint glow at its throat ------
def tunnel():
    m = Model("doodad_broodTunnel", category="doodad", coplanar_check=True, notes=[
        "A Brood tunnel's mouth at the foot of a canyon wall (Z1): a hollow 5 cells across and 3.6 high going black "
        "into the rock, its frame of dark boulders crusted with chitin plates and resin, opal flecks in the crust, a "
        "faint violet glow in its throat and in the seams round its rim, the rubble the bugs dug out spilled in front. "
        "The bugs of the zone's paths come out of it; it faces +X, into the canyon."])
    materials(m, "violet", strength=1.2)
    m.material("void", parent="hull", color=(0.004, 0.003, 0.006), roughness=1.0)
    m.material("rockDark", parent="hull", color=(0.055, 0.05, 0.052), roughness=0.92)
    m.material("resin", parent="hull", color=(0.10, 0.075, 0.085), metallic=0.1, roughness=0.55)
    m.material("throat", parent="emissive", color=(0.35, 0.06, 0.55), emissive=(0.22, 0.04, 0.42))
    b = body(m)
    void, frame, crust, rubble, resin, glow, dim = [], [], [], [], [], [], []
    # The hollow: nested black half-domes going back into the wall, each a little smaller and deeper, so the mouth
    # reads as a tunnel from the canyon floor and from the rim.
    for k, (depth, rx, rz) in enumerate(((-40, 250, 360), (-150, 215, 315), (-280, 175, 260), (-420, 130, 205))):
        void.append(sphere(1, at=(depth, 0, 0), squash=(70 + 20 * k, rx, rz), steps=16))
    # The throat's glow, deep in: a dim violet disc behind the last dome's opening, and a few glowing egg-like nubs
    # on its floor.
    glow.append(sphere(1, at=(-455, 0, 70), squash=(18, 110, 150), steps=12))
    for k, (y, z) in enumerate(((-80, 20), (60, 25), (10, 15), (-30, 45))):
        dim.append(sphere(22 + 6 * (k % 2), at=(-300 - 25 * k, y, z), squash=(1, 1, 0.8), steps=8))
    # The frame: boulders round the arch of the mouth, bigger at its feet.
    for k, (a, y, z) in enumerate(arc(13, 290, 395)):
        size = 100 if k in (0, 12) else 72 + 18 * (k % 3)
        frame.append(sphere(size, at=(10, y, z), squash=(1.1, 0.9, 0.85), rot=(0, 0, math.degrees(a)), steps=10))
    # A second ring of boulders further out, sunk back into the wall, so the frame reads as part of it.
    for k, (a, y, z) in enumerate(arc(9, 400, 500, 12, 168)):
        frame.append(sphere(85 + 20 * (k % 2), at=(-40, y, z), squash=(1.2, 1.0, 0.8), rot=(0, 0, math.degrees(a)), steps=10))
    # Chitin plates crusted over the frame, leaning out, and resin between them.
    for k, (a, y, z) in enumerate(arc(9, 300, 410, 18, 162)):
        crust.append(box((40, 120, 70), at=(60, y * 1.02, z * 1.02), rot=(20, 0, 90 - math.degrees(a))))
        resin.append(sphere(55, at=(30, y * 0.96, z * 0.96), squash=(0.7, 1.2, 0.8), steps=6))
    # Glowing seams under the plates round the rim (dim: a glint at dusk, nothing at noon).
    for k, (a, y, z) in enumerate(arc(7, 262, 365, 25, 155)):
        dim.append(box((10, 70, 10), at=(55, y, z), rot=(0, 0, 90 - math.degrees(a))))
    # The spill: rubble dug out and pushed down the canyon's floor in front, no higher than a bug.
    for k in range(14):
        a = (k * 0.6180339) % 1.0
        y = (a - 0.5) * 520
        x = 120 + ((k * 0.4142135) % 1.0) * 260
        rubble.append(sphere(18 + 14 * ((k * 7) % 3), at=(x, y, 0), squash=(1.2, 1.0, 0.6), steps=6))
    m.mesh(b, "void", void, solid=False)
    m.mesh(b, "rockDark", frame + rubble, solid=False)
    m.mesh(b, "chitin", crust, solid=False)
    m.mesh(b, "resin", resin, solid=False)
    m.mesh(b, "throat", glow, solid=False)
    m.mesh(b, "glowDim_violet", dim, solid=False)
    return m


# --- the build spot: a survey marker where a turret goes ---------------------------------------------------------------
def build_spot():
    m = Model("doodad_buildSpot", category="doodad", coplanar_check=True, notes=[
        "A build spot on a canyon's rim (Z1): a turret's 2 by 2 cells of levelled gravel in a painted amber ring, four "
        "survey stakes at its corners with lit amber caps, so pilots see where a turret overlooks the Brood's paths, "
        "from the rim, from the canyon floor and on the map."])
    m.material("gravel", parent="hull", color=(0.20, 0.19, 0.17), roughness=0.97)
    m.material("paintAmber", parent="hull", color=(0.62, 0.36, 0.05), roughness=0.6)
    m.material("stake", parent="hull", color=(0.12, 0.11, 0.10), metallic=0.4, roughness=0.6)
    m.material("capAmber", parent="emissive", color=(1.0, 0.55, 0.12), emissive=(1.2, 0.6, 0.12))
    b = body(m)
    ring = []
    for k in range(24):
        a = 2 * math.pi * k / 24
        # (26 long on a 29.3 cm pitch: longer segments overlapped and shared their tops)
        ring.append(box((26, 14, 4), at=(math.cos(a) * 112, math.sin(a) * 112, 6), rot=(0, math.degrees(a) + 90, 0), base=True))
    ticks = [box((44, 10, 4), at=(x, y, 6), rot=(0, r, 0), base=True)
             for x, y, r in ((80, 0, 0), (-80, 0, 0), (0, 80, 90), (0, -80, 90))]
    stakes, caps = [], []
    for sx, sy in ((1, 1), (1, -1), (-1, -1), (-1, 1)):
        x, y = sx * 105, sy * 105
        stakes.append(cyl(5, 62, at=(x, y, 0), base=True, sides=8))
        caps.append(cyl(8, 10, at=(x, y, 62), base=True, sides=8))
    m.mesh(b, "gravel", [cyl(120, 6, at=(0, 0, 0), base=True, sides=32)], solid=False)
    m.mesh(b, "paintAmber", ring + ticks, solid=False)
    m.mesh(b, "stake", stakes, solid=False)
    m.mesh(b, "capAmber", caps, solid=False)
    return m


tunnel().write()
build_spot().write()
