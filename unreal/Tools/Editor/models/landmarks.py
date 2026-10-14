"""Landmarks (docs/regions.md, "Landmarks"; docs/mmorpg-m7.md W3): four doodads that make a place, placed by a region
file's `doodads` list and drawn by AAcDoodads like any doodad (`doodad_<kind>`).

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/landmarks.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/doodad_<kind>/SM_<part>__<material> and the catalog entries `doodad_arch`, `doodad_sunkHauler`,
`doodad_derrickRuin`, `doodad_relayDish` (category doodad: no team, no pose; one part, `body`). The concepts are
art/models/landmarks/concepts/c1-*.jpg.

UE cm, Z = 0 the ground at the doodad's point, +X the doodad's own x (its `rotation` turns it). The solid feet must
match `NavGrid::footprint` (AutocraftCore NavGrid.cpp): a cell is 100 cm.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, sphere, bar, m_apply, m_mul, rot3  # noqa: E402

DRIVER = "AcDoodads.cpp:AAcDoodads::Rebuild"


def body(m):
    return m.part("body", driver=dict(what="the landmark itself, standing still", channels=[], functions=[DRIVER]))


def turned(prims, pitch=0.0, yaw=0.0, roll=0.0, shift=(0.0, 0.0, 0.0)):
    """The prims turned as one about the model's origin (pitch, yaw, roll degrees), then moved by `shift`."""
    Rt = rot3(pitch, yaw, roll)
    for p in prims:
        p.R = m_mul(p.R, Rt)
        p.at = m_apply(p.at, Rt, shift)
    return prims


# --- arch: a natural sandstone arch, two thick legs, a lane under it along Y -----------------------------------------
def arch():
    m = Model("doodad_arch", category="doodad", notes=[
        "A natural arch of banded sandstone: two thick legs 5.2 cells apart (feet at x = +-2.6, NavGrid::footprint), a "
        "wind-carved span about 5 cells up, strata in three tones, fallen blocks at its feet. A Prospector drives "
        "under it along its own y."])
    m.material("sandstone", parent="hull", color=(0.34, 0.21, 0.13), roughness=0.92)
    m.material("sandstoneRed", parent="hull", color=(0.27, 0.11, 0.075), roughness=0.94)
    m.material("sandstonePale", parent="hull", color=(0.45, 0.33, 0.22), roughness=0.9)
    tones = ("sandstone", "sandstoneRed", "sandstonePale", "sandstone", "sandstoneRed")
    rnd = random.Random(7)
    b = body(m)
    by = {t: [] for t in set(tones)}
    # The legs: stacked slabs, a little off each other, wider at the foot.
    for s in (-1, 1):
        z = 0.0
        for k in range(7):
            h = 38 + rnd.uniform(-6, 8)
            w = 172 - 7 * k + rnd.uniform(-8, 8)
            d = 214 - 5 * k + rnd.uniform(-10, 6)
            by[tones[k % 5]].append(box((w, d, h + 2), at=(s * 262 + rnd.uniform(-7, 7) - s * 4 * k, rnd.uniform(-6, 6), z),
                                        rot=(0, rnd.uniform(-4, 4), rnd.uniform(-2, 2)), base=True))
            z += h
    # The span: slabs along a flattened half ellipse from leg to leg, thick at the shoulders.
    cx, cz, rx, rz = 0.0, 280.0, 262.0, 170.0
    n = 11
    pts = [(cx - rx * math.cos(math.pi * k / n), 0.0, cz + rz * math.sin(math.pi * k / n)) for k in range(n + 1)]
    for k in range(n):
        a, c = pts[k], pts[k + 1]
        mid = abs(k + 0.5 - n / 2) / (n / 2)       # 0 at the crown, 1 at the shoulders
        w = 120 + 80 * mid
        by[tones[(k + 2) % 5]].append(bar((a[0], 0, a[2] - 10), (c[0], 0, c[2] - 10), w, 200 + rnd.uniform(-12, 12)))
    # A flat cap over the crown, the weathered top, and a ledge.
    by["sandstonePale"].append(box((420, 196, 56), at=(10, 4, 452), rot=(0, 2, 0)))
    by["sandstone"].append(box((300, 180, 40), at=(-30, -6, 492), rot=(0, -3, 1)))
    by["sandstoneRed"].append(box((200, 120, 26), at=(-180, 60, 300), rot=(0, 0, 4)))
    for t, prims in by.items():
        m.mesh(b, t, prims)
    # Fallen blocks at the feet, outside the lane.
    m.mesh(b, "sandstone", [box((90, 70, 54), at=(-180, 150, 0), rot=(10, 30, 0), base=True),
                            box((60, 50, 40), at=(330, -120, 0), rot=(0, -20, 6), base=True)])
    m.mesh(b, "sandstonePale", [sphere(30, at=(220, 140, 10), squash=(1.2, 1, 0.7), steps=6),
                                sphere(22, at=(-330, -90, 8), squash=(1, 1.2, 0.7), steps=6)], solid=False)
    return m


# --- sunk hauler: a dead ore hauler, nose down in the ground, its bed and rear wheels up --------------------------------
def sunk_hauler():
    m = Model("doodad_sunkHauler", category="doodad", notes=[
        "A dead heavy ore hauler sunk nose first (+X) into the ground: the cab half buried in a drift, the raised ore "
        "bed and the big rear wheels up, yellow paint worn to rust. About 5.3 cells long; its feet are three circles "
        "along x (NavGrid::footprint)."])
    m.material("haulerYellow", parent="hull", color=(0.46, 0.3, 0.035), metallic=0.2, roughness=0.62)
    m.material("rustSteel", parent="hull", color=(0.17, 0.075, 0.035), metallic=0.3, roughness=0.8)
    m.material("tyre", parent="hull", color=(0.025, 0.023, 0.022), roughness=0.9)
    m.material("drift", parent="hull", color=(0.36, 0.27, 0.17), roughness=0.97)
    m.material("hullDark")
    m.material("soot")
    b = body(m)
    yellow, rust, tyre, dark = [], [], [], []
    # The truck as it stood (ground at z = 0), nose toward +X.
    yellow += [box((470, 170, 34), at=(0, 0, 92)),                                   # frame
               box((112, 176, 132), at=(196, 0, 108), base=True),                     # cab
               box((96, 186, 92), at=(272, 0, 96), base=True)]                         # hood and grille block
    dark += [box((4, 120, 52), at=(254, 0, 182)),                                     # cab windows
             box((80, 4, 50), at=(196, 89, 182)), box((80, 4, 50), at=(196, -89, 182)),
             box((6, 150, 60), at=(320, 0, 130))]                                     # grille
    # The ore bed, tipped up at the back as it was dumping when it died.
    bed = [box((330, 236, 16), at=(-70, 0, 0)),
           box((330, 10, 92), at=(-70, 114, 0), base=True), box((330, 10, 92), at=(-70, -114, 0), base=True),
           box((12, 236, 120), at=(96, 0, 0), base=True)]
    turned(bed, pitch=-14.0, shift=(0, 0, 136))
    yellow += bed
    rust += turned([box((300, 238, 3), at=(-70, 0, 9))], pitch=-14.0, shift=(0, 0, 136))   # the worn floor
    rust += [box((310, 4, 30), at=(-60, s * 121, 175), rot=(-14, 0, 0)) for s in (-1, 1)]  # rusted rails
    # Wheels: big rear pairs, the front ones going under.
    for x, r in ((-150, 88), (190, 80)):
        for s in (-1, 1):
            tyre.append(cyl(r, 52, at=(x, s * 112, r), axis="y"))
            dark.append(cyl(r * 0.42, 54, at=(x, s * 112, r), axis="y"))
    allp = yellow + rust + tyre + dark
    # Sunk: the whole truck pitched nose down and pushed into the ground.
    turned(allp, pitch=-17.0, shift=(0, 0, -40))
    m.mesh(b, "haulerYellow", yellow)
    m.mesh(b, "rustSteel", rust, solid=False)
    m.mesh(b, "tyre", tyre)
    m.mesh(b, "hullDark", dark, solid=False)
    # The drift heaped over the nose and along the sides.
    m.mesh(b, "drift", [sphere(150, at=(250, 0, -30), squash=(1.2, 1.25, 0.6), steps=12),
                        sphere(90, at=(120, 120, -20), squash=(1.4, 0.8, 0.55), steps=10),
                        sphere(80, at=(120, -125, -20), squash=(1.3, 0.8, 0.5), steps=10),
                        sphere(60, at=(-210, 40, -18), squash=(1.2, 1.5, 0.45), steps=8)])
    m.mesh(b, "soot", [box((60, 2, 26), at=(-120, 124, 150), rot=(-30, 0, 6))], solid=False)
    return m


# --- derrick ruin: a bent steel lattice tower, its top section fallen beside it ----------------------------------------
def derrick_ruin():
    m = Model("doodad_derrickRuin", category="doodad", notes=[
        "The ruin of a drilling derrick: a four-legged steel lattice on a cracked concrete pad, straight to 5.2 m, its "
        "upper part bent over; the snapped top section lies on the ground beside it; a gutted pump housing at its foot. "
        "About 8.5 cells high."])
    m.material("rustSteel", parent="hull", color=(0.17, 0.075, 0.035), metallic=0.3, roughness=0.8)
    m.material("rustDark", parent="hull", color=(0.09, 0.045, 0.028), metallic=0.3, roughness=0.85)
    m.material("concrete", parent="hull", color=(0.24, 0.23, 0.21), roughness=0.95)
    m.material("soot")
    b = body(m)
    steel, dark = [], []

    def lattice(levels, out, rung=9, brace=6):
        """A four-legged lattice through the `levels` (z, half width, centre x, centre y), legs and rungs to `out`."""
        corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
        for (z0, h0, x0, y0), (z1, h1, x1, y1) in zip(levels, levels[1:]):
            for sx, sy in corners:
                out.append(bar((x0 + sx * h0, y0 + sy * h0, z0), (x1 + sx * h1, y1 + sy * h1, z1), 16))
            for k in range(4):
                (ax, ay), (cx2, cy2) = corners[k], corners[(k + 1) % 4]
                out.append(bar((x1 + ax * h1, y1 + ay * h1, z1), (x1 + cx2 * h1, y1 + cy2 * h1, z1), rung))
                # One diagonal a face, alternating.
                dark.append(bar((x0 + ax * h0, y0 + ay * h0, z0), (x1 + cx2 * h1, y1 + cy2 * h1, z1), brace))

    # Straight to 520, then bending over toward -X and +Y.
    levels = [(0, 115, 0, 0), (130, 100, 0, 0), (260, 86, 0, 0), (390, 72, 0, 0), (520, 60, 0, 0),
              (620, 52, -40, 12), (705, 46, -110, 30), (770, 42, -200, 52)]
    lattice(levels, steel)
    # The crown's torn platform at the top.
    steel.append(box((110, 110, 10), at=(-200, 52, 772), rot=(-25, 0, 10)))
    # The fallen top section, lying along +X beside the pad.
    fallen = []
    lattice([(0, 40, 0, 0), (110, 36, 0, 0), (220, 32, 0, 0), (320, 28, 0, 0)], fallen, rung=8, brace=5)
    turned(fallen, pitch=-84.0, yaw=-22.0, shift=(150, -110, 40))
    steel += fallen
    m.mesh(b, "rustSteel", steel)
    m.mesh(b, "rustDark", dark, solid=False)
    # The pad, cracked, a broken stair, the gutted pump housing at (1.3, 0.9) cells.
    m.mesh(b, "concrete", [box((290, 290, 22), at=(0, 0, -6), base=True), box((110, 120, 18), at=(150, 140, -6), rot=(0, 8, 2), base=True)])
    m.mesh(b, "rustSteel", [cyl(48, 84, at=(130, 90, 0), base=True)])
    m.mesh(b, "rustDark", [cyl(40, 6, at=(130, 90, 80), base=True), bar((90, 60, 40), (40, 20, 30), 14, shape="cyl"),
                           bar((80, -140, 0), (60, -100, 130), 30, 4)], solid=False)
    m.mesh(b, "soot", [box((140, 120, 2), at=(-30, -10, 16))], solid=False)
    return m


# --- relay dish: a crashed relay dish on its rim, its mast snapped ------------------------------------------------------
def relay_dish():
    m = Model("doodad_relayDish", category="doodad", notes=[
        "A crashed relay dish: a faceted white dish 4 cells across tipped up on its rim, cracked panels, the feed arm "
        "and its dead lamp; its lattice mast snapped and leaning beside it, cables trailing in the dirt."])
    m.material("dishWhite", parent="hull", color=(0.62, 0.61, 0.58), metallic=0.1, roughness=0.55)
    m.material("dishGrey", parent="hull", color=(0.36, 0.36, 0.35), metallic=0.1, roughness=0.6)
    m.material("rustSteel", parent="hull", color=(0.17, 0.075, 0.035), metallic=0.3, roughness=0.8)
    m.material("cable", parent="hull", color=(0.03, 0.03, 0.03), roughness=0.7)
    m.material("lampDead", parent="hull", color=(0.35, 0.05, 0.03), metallic=0.0, roughness=0.3)
    m.material("hullDark")
    m.material("drift", parent="hull", color=(0.36, 0.27, 0.17), roughness=0.97)
    b = body(m)
    white, grey, dark = [], [], []
    # The dish, facing up along +Z before it is tipped: two rings of panels, each tilted up toward the rim.
    for ring, (r, ln, wd, tilt, count) in enumerate(((62, 70, 52, 12, 8), (142, 96, 100, 26, 12))):
        for k in range(count):
            a = 360.0 * (k + 0.5 * ring) / count
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            z = 8 + (34 if ring else 0)
            p = box((ln, wd, 7), at=(r * ca, r * sa, z), rot=(tilt, a, 0))
            (grey if (k * 7 + ring) % 5 == 0 else white).append(p)
    dark.append(cyl(30, 10, at=(0, 0, 4), base=True))
    # The feed arm and its housing.
    feed = [bar((0, 0, 8), (0, 0, 150), 8, shape="cyl"), box((40, 30, 26), at=(0, 0, 160))]
    lamp = [box((6, 18, 14), at=(22, 0, 162))]
    dish = white + grey + dark + feed + lamp
    # Tipped on its rim toward -X, half in the dirt.
    turned(dish, pitch=62.0, yaw=0.0, shift=(30, 0, 120))
    m.mesh(b, "dishWhite", white)
    m.mesh(b, "dishGrey", grey)
    m.mesh(b, "hullDark", dark + feed[1:])
    m.mesh(b, "rustSteel", feed[:1], solid=False)
    m.mesh(b, "lampDead", lamp, solid=False)
    # The snapped mast at (-1.6, 0.6) cells, leaning out; its broken stump at (1.5, -0.4).
    mast = []
    for (x0, y0, z0), (x1, y1, z1) in [((-150, 60, 0), (-120, 70, 330))]:
        h0, h1 = 26, 18
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            mast.append(bar((x0 + sx * h0, y0 + sy * h0, z0), (x1 + sx * h1, y1 + sy * h1, z1), 9))
        for k in range(1, 6):
            t = k / 6
            z = z0 + (z1 - z0) * t
            x = x0 + (x1 - x0) * t
            mast.append(box((2 * h0 + 4, 6, 6), at=(x, y0 + h0, z), rot=(0, 0, 0)))
    mast_lean = turned(mast, roll=-18.0, shift=(0, 30, 0))
    m.mesh(b, "rustSteel", mast_lean + [box((60, 60, 50), at=(150, -40, 0), rot=(0, 20, 8), base=True)])
    # Cables trailing from the mast to the dish, a mound where the rim dug in.
    m.mesh(b, "cable", [bar((-130, 70, 40), (-40, 120, 2), 5, shape="cyl"), bar((-40, 120, 2), (60, 160, 2), 5, shape="cyl"),
                        bar((-110, 40, 20), (-20, -60, 2), 5, shape="cyl"), bar((-20, -60, 2), (80, -150, 2), 5, shape="cyl")],
           solid=False)
    m.mesh(b, "drift", [sphere(110, at=(-40, 0, -40), squash=(0.9, 1.6, 0.6), steps=10)])
    return m


for make in (arch, sunk_hauler, derrick_ruin, relay_dish):
    make().write()
