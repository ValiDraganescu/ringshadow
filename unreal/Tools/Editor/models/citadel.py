"""The Citadel (C1, docs/citadel-interior.md): the realm's keep, 12 by 12 cells, with a hall the Prospector walks into.
Replaces the SceneKit export. A fortress block: walls a cell thick and 7 high round a hall 10 by 10 by 6, the roof
slab over it, a doorway 3 by 3 on the front (+Y) closed by the `ramp` (the door standing up, a ramp once it falls
open), the roof deck at 7 with the four corner pylons (the turrets on them come from citadel_turrets.py) and the old
keep's tower, dish and beacon on its centre. Inside: the floor, eight yellow working lamps on the walls, a gun
station console under each corner gun.

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/citadel.py

then citadel_turrets.py and turret_marks.py are not needed again unless the turrets change; citadel_turrets.py
splices its parts into the entries this script writes, so run it after this one, every time.

Writes /Game/Models/citadel/SM_<part>__<material> and the catalog entries `citadel_blue` and `citadel_red` (the same
meshes), and /Game/Models/scaffold_citadel/... with `scaffold_citadel_blue` and `scaffold_citadel_red`.

UE cm, a building: front +Y (the sim's +y, toward the game camera), Z up, the ground at Z = 0, the footprint
+-600 (Rules::radius 6.0). The slabs are those of Rules::footprint (docs/citadel-interior.md section 2); cladding,
lamps and trim stand at most 8 cm proud of them. Parts the pose drives (AcPoseBuildings.cpp PoseCitadel):
`progressLamps_0..3` (the tower's windows), `queueLamps_0..4` (the tower's ring), `doorSegments_0..5` (the strip on
the lintel), `dish`, `beacon`, `light_2` (the door light), `emitter_1` (steam), the glow groups `ventGlow`,
`markerGlow`, `floodGlow`, and C1's `ramp`, `doorPanels_0` (outside) / `_1` (inside), `gunStations_k` with
`gunScreens_k`, `hallLamps_0..7`.
"""
import copy
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, bar, box, cyl, panel, sphere, load_catalog, save_catalog, merge_entry, log  # noqa: E402

POSE = "AcPoseBuildings.cpp:PoseCitadel"
H = 600.0          # the footprint's half side
IN = 500.0         # the hall's half side (walls 100 thick)
DECK = 700.0       # the roof deck
CEIL = 600.0       # the hall's ceiling
DOOR = 150.0       # the doorway's half width
DOOR_TOP = 300.0
SINK = 100.0       # walls and floor reach this far below the ground, to hide a slope
FLOOR = 5.0        # the hall floor's top
CORNER = 550.0     # the pylons (Defence::cornerInset 5.5)
PYLON_TOP = 900.0  # the turrets' mounts (Defence::mountHeight 9.0, citadel_turrets.py TOP)
STATION = 400.0    # the gunner's point (Defence::gunStation)


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


def slab(x0, x1, y0, y1, z0, z1):
    """An axis-aligned box from its two corners."""
    return box((x1 - x0, y1 - y0, z1 - z0), at=((x0 + x1) / 2, (y0 + y1) / 2, z0), base=True)


def on_face(axis, sign, along, z, size, out=H, depth=6.0):
    """A plate on an outer face (axis 'x' or 'y', sign +-1), centred `along` the face at height z (its bottom),
    size (width along the face, height), standing `depth` proud of `out`."""
    w, h = size
    c = out * sign + sign * depth / 2
    if axis == "y":
        return box((w, depth, h), at=(along, c, z), base=True)
    return box((depth, w, h), at=(c, along, z), base=True)


SKIN = 585.0       # the walls' own outer face; the skin (panels, ribs, bands, towers) stands on it, to at most 608
TOWER = 400.0      # the corner towers' inner edge along each face (they are the wall corners: solid inside +-600)


m = Model("citadel", category="building", coplanar_check=True, notes=[
    "C1's Citadel (docs/citadel-interior.md): 12 by 12 cells, walls 1 thick and 7 high, the hall 10 by 10 by 6 inside, "
    "the doorway 3 by 3 on the front closed by the ramp, the roof deck at 7, four corner towers carrying the turrets at "
    "9.0, the central tower and beacon to 12.3.",
    "Pose: PoseCitadel (production lamps, door strip, dish, glow groups, the ramp, door panels, gun stations, hall "
    "lamps, the corner turrets of citadel_turrets.py).",
    "Under construction the body rises out of the ground like every building, inside scaffold_citadel. height 9.0"])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "glow_2399ff", "glowRed", "doorGlow",
             "ventGlow", "floodGlow", "markerGlow"):
    m.material(name)
# C1's new lamps: the hall's yellow working strips (lit enough to light the hall under Lumen) and the door panels'
# amber. The base colour is the hue (the unlit master adds it: a white base washes a lamp out).
m.material("hallGlow", parent="emissive", color=(0.6, 0.42, 0.12), emissive=(18.0, 12.0, 3.4))
m.material("panelGlow", parent="emissive", color=(0.5, 0.25, 0.03), emissive=(2.4, 1.1, 0.12))
root = m.root


def face(axis, sign, a0, a1, z0, z1, out0, out1):
    """A box on a face (axis 'x' or 'y', sign +-1) from a0 to a1 along it, z0 to z1 high, out0 to out1 from the
    middle (cm, positive)."""
    o0, o1 = sorted((sign * out0, sign * out1))
    if axis == "y":
        return slab(a0, a1, o0, o1, z0, z1)
    return slab(o0, o1, a0, a1, z0, z1)


FACES = (("y", 1), ("y", -1), ("x", 1), ("x", -1))

# --- the shell: Rules::footprint's slabs (their outer face SKIN; the skin fills to 600), sunk SINK into the ground --
m.mesh(root, "hullMid", [
    slab(-SKIN, -DOOR, IN, SKIN, -SINK, DECK),       # front wall, left of the door
    slab(DOOR, SKIN, IN, SKIN, -SINK, DECK),         # front wall, right of the door
    slab(-DOOR, DOOR, IN, SKIN, DOOR_TOP, DECK),     # the lintel
    slab(-SKIN, SKIN, -SKIN, -IN, -SINK, DECK),      # back wall
    slab(-SKIN, -IN, -IN, IN, -SINK, DECK),          # left wall
    slab(IN, SKIN, -IN, IN, -SINK, DECK),            # right wall
    slab(-IN, IN, -IN, IN, CEIL, DECK),              # the roof slab
])
m.mesh(root, "hullDark", [slab(-DOOR, DOOR, SKIN, H, DOOR_TOP + 30, DECK - 40)])  # the lintel's face out to the door's plane
# the hall floor and the doorway's threshold
m.mesh(root, "hullDark", [slab(-IN, IN, -IN, IN, -SINK, FLOOR), slab(-DOOR, DOOR, IN, H + 4, -SINK, FLOOR)])

# --- the skin: buttress ribs every 2 cells, light panels in the bays, the service band, the team band, coping -----
RIBS = {"y1": (-290, 290), "y-1": (-200, 0, 200), "x1": (-200, 0, 200), "x-1": (-200, 0, 200)}
panels, ribs, service = [], [], []
for axis, sign in FACES:
    key = axis + str(sign)
    cuts = sorted(RIBS[key])
    edges = [-TOWER] + [c for c in cuts] + [TOWER]
    for n in range(len(edges) - 1):
        a0 = edges[n] + (20 if n > 0 else 0) + 4
        a1 = edges[n + 1] - (20 if n < len(edges) - 2 else 0) - 4
        if key == "y1" and a0 < DOOR + 30 and a1 > -DOOR - 30:  # the bay round the door: panels left and right only
            for b0, b1 in ((a0, -DOOR - 34), (DOOR + 34, a1)):
                if b1 - b0 > 20:
                    panels.append(face(axis, sign, b0, b1, 30, 290, SKIN, SKIN + 11))
                    panels.append(face(axis, sign, b0, b1, 345, 572, SKIN, SKIN + 11))
            continue
        panels.append(face(axis, sign, a0, a1, 30, 290, SKIN, SKIN + 11))
        panels.append(face(axis, sign, a0, a1, 345, 572, SKIN, SKIN + 11))
    for c in cuts:
        ribs.append(face(axis, sign, c - 20, c + 20, -20, DECK - 32, SKIN, H + 8))
        ribs.append(face(axis, sign, c - 26, c + 26, -22, 30, SKIN, H + 10))  # the rib's foot
    if key == "y1":
        service += [face(axis, sign, -TOWER, -DOOR - 30, 295, 337, SKIN, H + 4), face(axis, sign, DOOR + 30, TOWER, 295, 337, SKIN, H + 4)]
    else:
        service.append(face(axis, sign, -TOWER, TOWER, 295, 337, SKIN, H + 4))
m.mesh(root, "hullLight", panels, solid=False)
m.mesh(root, "hullDark", ribs)
m.mesh(root, "hullDark", service, solid=False)
# the service band's pipe and its brackets, on the sides and the back
pipes = []
for axis, sign in FACES[1:]:
    pipes.append(cyl(6, 2 * TOWER, at=(0, sign * (H + 6), 316) if axis == "y" else (sign * (H + 6), 0, 316),
                     axis="x" if axis == "y" else "y", sides=10))
m.mesh(root, "chrome", pipes, solid=False)
m.mesh(root, "team", [face(axis, sign, -TOWER, TOWER, 588, 628, SKIN, H + 5) for axis, sign in FACES], solid=False)
m.mesh(root, "hullMid", [face(axis, sign, -TOWER, TOWER, 572, 700, SKIN, H - 2) for axis, sign in FACES], solid=False)
m.mesh(root, "hullDark", [face(axis, sign, -TOWER, TOWER, DECK - 32, DECK + 6, H - 16, H + 6) for axis, sign in FACES],
       solid=False)  # the coping round the deck's edge, between the towers
# vents (ventGlow) in the lower bays of the sides and the back
vents_dark, vents_glow = [], []
for axis, sign in FACES[1:]:
    for a in (-300, -100, 100, 300):
        vents_dark.append(face(axis, sign, a - 62, a + 62, 140, 212, SKIN + 11, SKIN + 17))
        for j in range(3):
            vents_glow.append(face(axis, sign, a - 54, a + 54, 150 + 20 * j, 158 + 20 * j, SKIN + 17, SKIN + 19))
m.mesh(root, "hullDark", vents_dark, solid=False)
m.mesh(root, "ventGlow", vents_glow, solid=False)


# --- the corner towers: the wall corners carried up past the deck to the turrets' mounts at 9.0 ------------------
def ctr(sx, sy, i0, i1, o0, o1, z0, z1):
    """A box in corner (sx, sy): x from i0 to i1 and y from o0 to o1 (cm from the middle, positive), mirrored."""
    xs, ys = sorted((sx * i0, sx * i1)), sorted((sy * o0, sy * o1))
    return slab(xs[0], xs[1], ys[0], ys[1], z0, z1)


MOUNT = PYLON_TOP
CROWN = MOUNT - 30.0
tw_mid, tw_dark, tw_haz, tw_slit, tw_mark = [], [], [], [], []
for sx in (-1, 1):
    for sy in (-1, 1):
        # below the deck: a skin on both outer faces of the corner, 2 cells wide, on the feet, butted against a dark
        # corner edge that stands 2 cm prouder (no two solids share a face: the coplanar check, art/models/citadel/c1/zfight/)
        tw_mid += [ctr(sx, sy, TOWER, H - 30, SKIN, H + 8, 52, DECK), ctr(sx, sy, SKIN, H + 8, TOWER, H - 30, 52, DECK)]
        tw_dark += [ctr(sx, sy, H - 30, H + 10, H - 30, H + 10, 52, CROWN - 52)]  # the corner edge, up to the hazard band
        # above the deck: the tower's block over the roof, its crown, the mount's pedestal
        tw_mid.append(ctr(sx, sy, TOWER, H, TOWER, H, DECK, CROWN - 40))
        tw_dark.append(ctr(sx, sy, TOWER - 10, H + 8, TOWER - 10, H + 8, CROWN - 40, CROWN))
        tw_haz.append(ctr(sx, sy, TOWER - 12, H + 8, TOWER - 12, H + 8, CROWN - 52, CROWN - 40))
        # the anchor feet (the old landing pads): a stepped L round each corner, outside the walls (sunk 2 cm into
        # them; a square block filled the hall's corner under the gun station), its two arms butted
        for i0, z0, z1 in ((TOWER - 20, -22, 34), (TOWER, 34, 52)):
            tw_dark += [ctr(sx, sy, i0, H + 8, SKIN - 2, H + 8, z0, z1), ctr(sx, sy, SKIN - 2, H + 8, i0, SKIN - 2, z0, z1)]
        tw_haz += [ctr(sx, sy, TOWER - 22, TOWER + 20, H + 8, H + 10, 2, 30), ctr(sx, sy, H + 8, H + 10, TOWER - 22, TOWER + 20, 2, 30)]
        # yellow working slits (floodGlow) on the two outer faces: three in the shaft, three over the deck (on the
        # tower's block there, its face at H)
        for a in (450, 490, 530):
            for z0 in (430, 750):
                out = H + 8 if z0 < DECK else H
                tw_slit.append(ctr(sx, sy, a - 6, a + 6, out, out + 2, z0, z0 + 70))
                tw_slit.append(ctr(sx, sy, out, out + 2, a - 6, a + 6, z0, z0 + 70))
        tw_mark.append(ctr(sx, sy, H - 4, H + 10, H - 4, H + 10, CROWN - 12, CROWN - 2))
        m.mesh(root, "hullDark", [cyl(56, MOUNT - CROWN - 4, at=(sx * CORNER, sy * CORNER, CROWN), base=True, sides=16)])
        m.mesh(root, "chrome", [cyl(58, 5, at=(sx * CORNER, sy * CORNER, MOUNT - 5), base=True, sides=16)], solid=False)
m.mesh(root, "hullMid", tw_mid)
m.mesh(root, "hullDark", tw_dark)
m.mesh(root, "hazard", tw_haz, solid=False)
m.mesh(root, "floodGlow", tw_slit, solid=False)
m.mesh(root, "markerGlow", tw_mark, solid=False)

# --- the front: the doorway's hazard frame, the door strip on the lintel, floodlights, markers, door panels, apron ----
m.mesh(root, "hazard", [slab(sx * DOOR - (30 if sx < 0 else 0), sx * DOOR + (30 if sx > 0 else 0), SKIN, H + 6, 0, DOOR_TOP)
                        for sx in (-1, 1)] + [slab(-DOOR - 30, DOOR + 30, SKIN, H + 6, DOOR_TOP, DOOR_TOP + 30)])
# the doorway's reveal lined dark, hazard strips on its jambs inside
m.mesh(root, "hullDark", [slab(sx * DOOR - (4 if sx > 0 else 0), sx * DOOR + (4 if sx < 0 else 0), IN, SKIN, 0, DOOR_TOP)
                          for sx in (-1, 1)] + [slab(-DOOR + 4, DOOR - 4, IN, SKIN, DOOR_TOP - 4, DOOR_TOP)], solid=False)
m.mesh(root, "hazard", [slab(sx * DOOR - 30 * (sx < 0), sx * DOOR + 30 * (sx > 0), IN - 5, IN, 0, DOOR_TOP + 30)
                        for sx in (-1, 1)], solid=False)
SEG_W = 42.0
for i in range(6):
    x = -125.0 + 50.0 * i
    seg = m.part("doorSegments_%d" % i, at=(x, H + 3, 352),
                 driver=d("the door strip over the doorway: chases toward the door while training", ("emission", "visibility")))
    m.mesh(seg, "doorGlow", [box((SEG_W, 3, 18), at=(x, H + 5.5, 343), base=True)], solid=False)
m.mesh(root, "hullDark", [box((300, 4, 34), at=(0, H + 2, 335), base=True)], solid=False)  # the strip's backing
# two floodlights on the lintel, over the strip, looking down on the apron
for sx in (-1, 1):
    m.mesh(root, "hullDark", [box((70, 16, 30), at=(sx * 95, H + 8, 400), base=True)], solid=False)
    m.mesh(root, "floodGlow", [box((60, 3, 16), at=(sx * 95, H + 16.5, 404), base=True)], solid=False)
m.mesh(root, "markerGlow", [box((12, 3, 12), at=(sx * 236, H + 5.5, 300), base=True) for sx in (-1, 1)], solid=False)
# the door panels: a dark frame right of the doorway, outside and inside, the lit face the pose drives
PANEL_X, PANEL_Z = 225.0, 120.0
for k, (y, s) in enumerate(((H, 1), (IN, -1))):
    m.mesh(root, "hullDark", [box((44, 8 if k == 0 else 6, 56), at=(PANEL_X, y + s * (-1 if k == 0 else 3), PANEL_Z - 28),
                                  base=True)], solid=False)
    p = m.part("doorPanels_%d" % k, at=(PANEL_X, y + s * 7, PANEL_Z),
               driver=d("the door panel %s: lit while a pilot is in reach of the door" % ("outside" if k == 0 else "inside")))
    m.mesh(p, "panelGlow", [box((32, 2, 40), at=(PANEL_X, y + s * (5 if k == 0 else 7), PANEL_Z - 20), base=True)], solid=False)
m.part("light_2", at=(0, H + 100, 250), handles=["doorLight"],
       light={"attenuationEnd": "6.0", "color": "SIMD4<Float>(1.0, 0.6600805, 0.19007224, 1.0)", "handles": "doorLight",
              "intensity": "0.0", "type": "omni"},
       driver=d("the door light: on while a unit trains", ("light",)))
# the apron before the door, on the ground (the open ramp lies on it)
APRON = (260.0, 1020.0)
m.mesh(root, "hullDark", [slab(-APRON[0], APRON[0], H, APRON[1], -15, 2)], solid=False)
m.mesh(root, "hazard", [slab(sx * APRON[0] - 16 * (sx > 0), sx * APRON[0] + 16 * (sx < 0), H + 10, APRON[1] - 16, 2, 3)
                        for sx in (-1, 1)] + [slab(-APRON[0], APRON[0], APRON[1] - 16, APRON[1], 2, 3)], solid=False)
m.mesh(root, "markerGlow", [box((10, 10, 6), at=(sx * (APRON[0] - 30), APRON[1] - 30, 2), base=True) for sx in (-1, 1)],
       solid=False)

# --- the ramp: the door standing in the doorway, hinged at its outer bottom edge --------------------------------
ramp = m.part("ramp", at=(0, H, 0), driver=d(
    "the door: turns about its hinge (the doorway's outer bottom edge, local X) by door.open x 90 degrees, its top "
    "outward (+Y), to lie on the ground as a ramp", ("rotation",)))
m.mesh(ramp, "hullLight", [slab(-DOOR + 4, DOOR - 4, H - 30, H, 0, DOOR_TOP - 4)])
m.mesh(ramp, "hullDark", [slab(-DOOR + 4, DOOR - 4, H - 34, H - 30, 20 + 46 * j, 30 + 46 * j) for j in range(6)] +
       [slab(-DOOR + 20 + 65 * j, -DOOR + 30 + 65 * j, H, H + 3, 10, DOOR_TOP - 20) for j in range(5)], solid=False)
m.mesh(ramp, "hazard", [slab(-DOOR + 4, DOOR - 4, H, H + 2, 0, 18), slab(-DOOR + 4, DOOR - 4, H, H + 2, DOOR_TOP - 22, DOOR_TOP - 4)],
       solid=False)

# --- inside: baseboards, ceiling beams and cable trays, floor markings ---------------------------------------------
m.mesh(root, "hullDark", [slab(-IN, -DOOR - 30, IN - 4, IN, FLOOR, 60), slab(DOOR + 30, IN, IN - 4, IN, FLOOR, 60),
                          slab(-IN, IN, -IN, -IN + 4, FLOOR, 60), slab(-IN, -IN + 4, -IN + 4, IN - 4, FLOOR, 60),
                          slab(IN - 4, IN, -IN + 4, IN - 4, FLOOR, 60)], solid=False)
m.mesh(root, "hullDark", [slab(-IN, IN, y - 20, y + 20, CEIL - 45, CEIL) for y in (-250, 0, 250)] +
       [slab(x - 15, x + 15, -IN, IN, CEIL - 30, CEIL) for x in (-250, 250)], solid=False)
trays = [slab(-IN + 55, IN - 55, -IN + 25, -IN + 55, CEIL - 62, CEIL - 54)] + \
        [slab(*sorted((sx * (IN - 55), sx * (IN - 25))), -IN + 25, IN - 10, CEIL - 62, CEIL - 54) for sx in (-1, 1)]
m.mesh(root, "hullDark", trays + [slab(x - 4, x + 4, -IN + 20, -IN + 60, CEIL - 64, CEIL - 45) for x in range(-400, 401, 200)],
       solid=False)
m.mesh(root, "chrome", [cyl(5, 2 * IN - 30, at=(-IN + 15, -IN + 40, CEIL - 50), axis="x", base=True, sides=8)] +
       [cyl(5, 2 * IN - 44, at=(sx * (IN - 40), -IN + 30, CEIL - 50), axis="y", base=True, sides=8) for sx in (-1, 1)],
       solid=False)
m.mesh(root, "team", [cyl(4, 2 * IN - 34, at=(-IN + 17, -IN + 32, CEIL - 50), axis="x", base=True, sides=8)], solid=False)
m.mesh(root, "hazard", [slab(sx * (DOOR - 10) - 6, sx * (DOOR - 10) + 6, 0, IN, FLOOR, FLOOR + 1) for sx in (-1, 1)],
       solid=False)  # the walk in from the door
m.mesh(root, "team", [cyl(130, 1, at=(0, -60, FLOOR), base=True, sides=32)], solid=False)
m.mesh(root, "hullDark", [cyl(105, 3, at=(0, -60, FLOOR), base=True, sides=32)], solid=False)

# --- the catwalk along the back and the two sides at 4 cells, its railing, brackets and a ladder -------------------
CAT_Z, CAT_W = 400.0, 90.0
cat = [slab(-IN, IN, -IN, -IN + CAT_W, CAT_Z - 8, CAT_Z)]
for sx in (-1, 1):
    xs = sorted((sx * IN, sx * (IN - CAT_W)))
    cat.append(slab(xs[0], xs[1], -IN + CAT_W, IN, CAT_Z - 8, CAT_Z))
m.mesh(root, "hullDark", cat)
posts, rails, brackets = [], [], []
EDGE = IN - CAT_W
for x in range(-375, 376, 125):
    posts.append(cyl(3.5, 95, at=(x, -EDGE, CAT_Z), base=True, sides=6))
    brackets.append(slab(x - 5, x + 5, -IN, -EDGE, CAT_Z - 38, CAT_Z - 8))
for sx in (-1, 1):
    for y in range(-375, 501, 125):
        posts.append(cyl(3.5, 95, at=(sx * EDGE, min(y, IN - 10), CAT_Z), base=True, sides=6))
        xs = sorted((sx * IN, sx * EDGE))
        brackets.append(slab(xs[0], xs[1], min(y, IN - 10) - 5, min(y, IN - 10) + 5, CAT_Z - 38, CAT_Z - 8))
for z in (CAT_Z + 45, CAT_Z + 90):
    rails.append(cyl(2.5, 2 * EDGE, at=(-EDGE, -EDGE, z), axis="x", base=True, sides=6))
    for sx in (-1, 1):
        rails.append(cyl(2.5, IN + EDGE, at=(sx * EDGE, -EDGE, z), axis="y", base=True, sides=6))
m.mesh(root, "chrome", posts + rails, solid=False)
m.mesh(root, "hullDark", brackets, solid=False)
m.mesh(root, "hazard", [slab(-EDGE, EDGE, -EDGE, -EDGE + 2, CAT_Z - 8, CAT_Z)] +
       [slab(min(sx * EDGE, sx * (EDGE - 2)), max(sx * EDGE, sx * (EDGE - 2)), -EDGE + 2, IN, CAT_Z - 8, CAT_Z) for sx in (-1, 1)],
       solid=False)  # its edge, a lip on the inner face
LADDER_X = 120.0
m.mesh(root, "chrome", [cyl(3, CAT_Z + 90, at=(LADDER_X + dx, -IN + 14, FLOOR), base=True, sides=6) for dx in (-22, 22)] +
       [cyl(2, 44, at=(LADDER_X - 22, -IN + 14, z), axis="x", base=True, sides=6) for z in range(35, int(CAT_Z), 30)],
       solid=False)

# --- the hoist under the tower: a trolley on the middle beam, cables, the hook block, 2.5 cells over the floor ------
m.mesh(root, "hullDark", [slab(-30, 30, -25, 25, CEIL - 75, CEIL - 45)], solid=False)
m.mesh(root, "chrome", [cyl(1.5, CEIL - 75 - 300, at=(dx, 0, 300), base=True, sides=6) for dx in (-10, 10)], solid=False)
m.mesh(root, "hazard", [slab(-20, 20, -15, 15, 262, 300)], solid=False)
m.mesh(root, "chrome", [cyl(4, 12, at=(0, 0, 250), base=True, sides=8)], solid=False)

# --- the eight hall lamps: two on the front wall, six under the catwalk's edge looking down ----------------------
HALL_LAMPS = [((-330, IN), "y", -1, 430.0), ((330, IN), "y", -1, 430.0)]
LAMP_LOW = CAT_Z - 40
for x, y in ((EDGE, 250), (EDGE, -250), (250, -EDGE), (-250, -EDGE), (-EDGE, -250), (-EDGE, 250)):
    HALL_LAMPS.append(((x, y), "down", 0, LAMP_LOW))
for k, ((x, y), axis, inward, z) in enumerate(HALL_LAMPS):
    if axis == "y":
        housing = box((172, 16, 40), at=(x, y + inward * 8, z - 20), base=True)
        strip = box((160, 4, 26), at=(x, y + inward * 18, z - 13), base=True)
    else:  # under the catwalk's inner edge, the strip on its underside
        along_x = abs(y) == EDGE
        size = (160, 26, 4) if along_x else (26, 160, 4)
        inset = 22
        cx = x if along_x else x - (inset if x > 0 else -inset)
        cy = y - (inset if y > 0 else -inset) if along_x else y
        housing = box((172, 40, 12) if along_x else (40, 172, 12), at=(cx, cy, z), base=True)
        strip = box(size, at=(cx, cy, z - 4), base=True)
    m.mesh(root, "hullDark", [housing], solid=False)
    lamp = m.part("hallLamps_%d" % k, at=strip.at, driver=d("hall lamp %d: the yellow working light, steady once built" % k))
    m.mesh(lamp, "hallGlow", [strip], solid=False)

# --- the gun stations: a console in each corner under its gun, after the user's reference
# (art/models/citadel/ref/console-reference.jpg): a squat armoured box on a hazard plinth with lit yellow strips, a
# control deck with a joystick (the gunner's left) and a cluster of coloured buttons, a tilted monitor on a stand
# showing the gun's view (citadel_screen.py's picture; `gunScreens_k`, lit while the gun is manned), cables, the
# conduit up to the gun. 1.0 wide, 0.8 deep, 0.9 tall, the monitor to 1.7; turned on the corner's diagonal toward the
# gunner's point, every solid of it 0.5 cells from that point at least, so its back corners sink into the walls.
CONSOLE = 97.0      # the console's middle from the gunner's point, along the corner's diagonal (its front: 46 out)
m.material("gunScreen", parent="emissive", color=(0, 0, 0), emissive=(0.026, 0.026, 0.026),
           emissive_texture="unreal/Resources/Textures/citadel_gun_screen.png")  # the pose lights it 47.5 times
TILT = 15.0         # the monitor leans back
BUTTONS = [("glowRed", -6, -12), ("glowGreen", -6, -22), ("glowYellow", -6, -32), ("glowBlue", 3, -14),
           ("glowRed", 3, -24), ("glowOrange", 3, -34), ("glowGreen", 12, -18), ("glowBlue", 12, -28)]
for k, (sx, sy) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
    diag = (sx / math.sqrt(2), sy / math.sqrt(2))
    c = (sx * STATION + diag[0] * CONSOLE, sy * STATION + diag[1] * CONSOLE)
    yaw = math.degrees(math.atan2(-sy, -sx))  # local +X toward the gunner (the hall's middle)
    cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def L(lx, ly, lz):
        """A point in the console's frame (x toward the gunner, y to the gunner's right... of the console: the
        gunner's left, z up from the ground) in the model frame."""
        return (c[0] + lx * cy_ - ly * sy_, c[1] + lx * sy_ + ly * cy_, lz)

    def B(size, lx, ly, z, pitch=0.0):
        return box(size, at=L(lx, ly, z), rot=(pitch, yaw, 0), base=True)

    st = m.part("gunStations_%d" % k, at=(c[0], c[1], FLOOR),
                driver=d("gun station %d's console (corner %d's gun): still" % (k, k), ()))
    # the plinth, its hazard band and lit strips at the front, the armoured body, the desk top and the control deck
    m.mesh(st, "hullDark", [B((84, 104, 10), 0, 0, FLOOR), B((84, 104, 6), 0, 0, FLOOR + 67)])
    m.mesh(st, "hullMid", [B((80, 100, 57), 0, 0, FLOOR + 10), B((40, 92, 10), 20, 0, FLOOR + 73)])
    m.mesh(st, "hazard", [B((2, 104, 10), 43, 0, FLOOR)], solid=False)
    m.mesh(st, "floodGlow", [B((2, 26, 3), 45, y, FLOOR + 3.5) for y in (-36, 0, 36)], solid=False)
    m.mesh(st, "hullDark", [B((2, 70, 40), 41, 0, FLOOR + 17)] +                       # the front's armour plate
           [B((6, 6, 57), 41, s * 48, FLOOR + 10) for s in (-1, 1)] +                 # its corner rails
           [B((2, 60, 3), 43, 0, FLOOR + 17 + 9 * j) for j in range(1, 4)], solid=False)  # and the plate's ribs
    m.mesh(st, "team", [B((2, 30, 6), 41, 0, FLOOR + 61)], solid=False)
    # the joystick on the gunner's left, the buttons on the right
    J = (20.0, 24.0)
    m.mesh(st, "hullDark", [cyl(7, 3, at=L(J[0], J[1], FLOOR + 83), base=True, sides=12),
                            cyl(5, 3, at=L(J[0], J[1], FLOOR + 86), base=True, r1=2.5, sides=12),
                            cyl(3, 10, at=L(J[0], J[1], FLOOR + 102), base=True, sides=10)], solid=False)
    m.mesh(st, "chrome", [cyl(1.6, 14, at=L(J[0], J[1], FLOOR + 89), base=True, sides=8)], solid=False)
    m.mesh(st, "glowRed", [sphere(2, at=L(J[0] + 1, J[1], FLOOR + 112.5), steps=8)], solid=False)
    m.mesh(st, "hullDark", [B((30, 34, 2), 20, -23, FLOOR + 83)], solid=False)
    for mat, bx, by in BUTTONS:
        m.mesh(st, mat, [cyl(2.6, 2, at=L(20 + bx, by, FLOOR + 85), base=True, sides=10)], solid=False)
    # the monitor's stand, its housing leaning back, the screen on its face; cables to the floor; the conduit
    m.mesh(st, "hullDark", [B((12, 24, 30), -22, 0, FLOOR + 73)])
    t = math.radians(TILT)
    hx, hz = -22.0, FLOOR + 100.0  # the housing's bottom middle (console frame, along x and up)
    m.mesh(st, "hullMid", [B((10, 96, 64), hx, 0, hz, TILT)])
    # the screen: 2 cm proud of the housing's face, 6 cm over its bottom edge (along the leaning face)
    sx_, sz_ = hx + 6 * math.cos(t) - 6 * math.sin(t), hz + 6 * math.sin(t) + 6 * math.cos(t)
    scr_mid = L(sx_ - 26 * math.sin(t), 0, sz_ + 26 * math.cos(t))
    scr = m.part("gunScreens_%d" % k, parent=st, at=scr_mid,
                 driver=d("gun station %d's monitor: the gun's view, lit while the gun is manned" % k))
    m.mesh(scr, "gunScreen", [panel((2, 86, 52), at=L(sx_, 0, sz_), rot=(TILT, yaw, 0), base=True)], solid=False)
    m.mesh(st, "hullDark", [bar(L(-24, s * 53, FLOOR + 70), L(-40, s * 56, FLOOR), 5, shape="cyl", sides=8)
                            for s in (-1, 1)], solid=False)
    # the conduit out of the back of the monitor's top (behind the leaning screen), up to the gun over the corner
    m.mesh(st, "hullDark", [cyl(6, CEIL - FLOOR - 150, at=L(-42, 0, FLOOR + 150), base=True, sides=10)], solid=False)
    m.mesh(st, "chrome", [cyl(8, 4, at=L(-42, 0, z), base=True, sides=10) for z in (200, 320, 440, 560)], solid=False)
    # the gunner's spot on the floor: a hazard ring on the 0.5 cell the console keeps clear
    m.mesh(st, "hazard", [cyl(48, 1, at=(sx * STATION, sy * STATION, FLOOR), base=True, sides=24)], solid=False)
    m.mesh(st, "hullDark", [cyl(40, 3, at=(sx * STATION, sy * STATION, FLOOR), base=True, sides=24)], solid=False)

# --- the roof deck: plating, the walkway between the towers, tanks, a crane, the dish, the steam stack ------------
m.mesh(root, "hullMid", [slab(-H + 16, H - 16, -H + 16, H - 16, DECK, DECK + 3)])
SEAMS = (-300, 0, 300)
cuts = [-H + 16] + [c for a in SEAMS for c in (a - 3, a + 3)] + [H - 16]
m.mesh(root, "hullDark", [slab(cuts[2 * i], cuts[2 * i + 1], a - 3, a + 3, DECK + 3, DECK + 4) for a in SEAMS for i in range(4)] +
       [slab(a - 3, a + 3, -H + 16, H - 16, DECK + 3, DECK + 4) for a in SEAMS], solid=False)  # seams
WALK = (450.0, 580.0)   # the walkway's inner and outer edge from the middle
walk, walk_haz, walk_posts, walk_rails = [], [], [], []
for axis, sign in FACES:
    walk.append(face(axis, sign, -TOWER, TOWER, DECK + 3, DECK + 22, WALK[0], WALK[1]))
    walk_haz.append(face(axis, sign, -TOWER, TOWER, DECK + 22, DECK + 23, WALK[0], WALK[0] + 8))
    for a in range(-350, 351, 100):
        p = (a, sign * WALK[0]) if axis == "y" else (sign * WALK[0], a)
        walk_posts.append(cyl(3.5, 83, at=(p[0], p[1], DECK + 22), base=True, sides=6))
    for z in (DECK + 62, DECK + 100):
        walk_rails.append(cyl(2.5, 2 * TOWER, at=(-TOWER, sign * WALK[0], z) if axis == "y" else (sign * WALK[0], -TOWER, z),
                              axis="x" if axis == "y" else "y", base=True, sides=6))
m.mesh(root, "hullDark", walk)
m.mesh(root, "hazard", walk_haz, solid=False)
m.mesh(root, "chrome", walk_posts + walk_rails, solid=False)
# two tanks on saddles, front right
for (x0, x1, y) in ((170.0, 380.0, 340.0), (210.0, 400.0, 245.0)):
    m.mesh(root, "hullLight", [cyl(42, x1 - x0, at=(x0, y, DECK + 60), axis="x", base=True, sides=16)])
    m.mesh(root, "hullDark", [slab(x - 8, x + 8, y - 36, y + 36, DECK + 3, DECK + 40) for x in (x0 + 30, x1 - 30)])
    m.mesh(root, "team", [cyl(43, 10, at=(x0 + (x1 - x0) / 2 - 5, y, DECK + 60), axis="x", base=True, sides=16)], solid=False)
    m.mesh(root, "chrome", [sphere(42, at=(x, y, DECK + 60), squash=(0.35, 1, 1), steps=12) for x in (x0, x1)], solid=False)
# a deck crane, front left: a turntable, a column, a boom out over the left side with its hook
CRANE = (-330.0, 330.0)
m.mesh(root, "hullDark", [cyl(40, 30, at=(CRANE[0], CRANE[1], DECK + 3), base=True, sides=16),
                          box((40, 40, 130), at=(CRANE[0], CRANE[1], DECK + 33), base=True)])
BOOM_A, BOOM_B = (CRANE[0], CRANE[1], DECK + 150), (CRANE[0] - 200, CRANE[1] - 70, DECK + 250)
m.mesh(root, "hazard", [bar(BOOM_A, BOOM_B, 22, 18)])
m.mesh(root, "chrome", [cyl(1.5, 150, at=(BOOM_B[0], BOOM_B[1], DECK + 100), base=True, sides=6)], solid=False)
m.mesh(root, "hullDark", [box((18, 18, 20), at=(BOOM_B[0], BOOM_B[1], DECK + 82), base=True)], solid=False)
# --- the central tower (the old keep's), deck to 12 cells: an octagon with the progress windows toward the camera, a
# chrome ring, a narrower octagon, the window drum with the queue ring, a dome, the beacon's mast ------------------
TR = 240.0                 # the octagon's corner radius
TRI = TR * math.cos(math.pi / 8)
T0, T1 = DECK + 3, 900.0
OCT = dict(sides=8, rot=(0, 22.5, 0))
m.mesh(root, "hullMid", [cyl(TR, T1 - T0, at=(0, 0, T0), base=True, **OCT)])
m.mesh(root, "hullLight", [cyl(TR + 4, 30, at=(0, 0, T0), base=True, **OCT)], solid=False)
m.mesh(root, "team", [cyl(TR + 3, 12, at=(0, 0, T1 - 30), base=True, **OCT)], solid=False)
m.mesh(root, "chrome", [cyl(TR + 8, 8, at=(0, 0, T1), base=True, **OCT)])
U0, U1, UR = T1 + 8, 1060.0, 170.0
m.mesh(root, "hullLight", [cyl(UR, U1 - U0, at=(0, 0, U0), base=True, **OCT)])
m.mesh(root, "hullDark", [cyl(UR + 3, 10, at=(0, 0, 1000), base=True, **OCT),
                          cyl(UR + 6, 6, at=(0, 0, U1 - 8), base=True, **OCT)], solid=False)
DRUM, D0 = 150.0, U1
m.mesh(root, "hullMid", [cyl(DRUM, 60, at=(0, 0, D0), base=True, sides=32)])
m.mesh(root, "glow_2399ff", [cyl(DRUM + 1, 18, at=(0, 0, D0 + 15), base=True, sides=32)], solid=False)  # dark glass
m.mesh(root, "chrome", [cyl(DRUM + 3, 5, at=(0, 0, D0 + 58), base=True, sides=32)], solid=False)
m.mesh(root, "hullMid", [sphere(DRUM, at=(0, 0, D0 + 60), squash=(1, 1, 0.35), steps=24)])
m.mesh(root, "chrome", [cyl(4, 44, at=(0, 0, D0 + 108), base=True, sides=8)], solid=False)
beacon = m.part("beacon", at=(0, 0, D0 + 162), driver=d("beacon blink (0.25 s of every 1.2 s once built)", ("visibility",)))
m.mesh(beacon, "glowRed", sphere(9, at=(0, 0, D0 + 162), steps=10), solid=False)
# progress windows on the octagon's faces toward the camera, screen left to right (-X first)
WIN_Z = 800.0
for k, (ang, off) in enumerate(((135, 0), (90, -55), (90, 55), (45, 0))):
    a = math.radians(ang)
    px = math.cos(a) * (TRI + 2) + (off if ang == 90 else 0)
    py = math.sin(a) * (TRI + 2)
    m.mesh(root, "hullDark", [box((84, 4, 48), at=(px - math.cos(a) * 1.5, py - math.sin(a) * 1.5, WIN_Z - 24),
                                  rot=(0, ang - 90, 0), base=True)], solid=False)  # the frame
    lamp = m.part("progressLamps_%d" % k, at=(px, py, WIN_Z), rot=(0, ang - 90, 0),
                  driver=d("training progress: lamp k on when k < progress x 4, the next blinking"))
    m.mesh(lamp, "glow_2399ff", [box((70, 4, 34), at=(px, py, WIN_Z - 17), rot=(0, ang - 90, 0), base=True)], solid=False)
# the queue ring: five segments round the drum's front, screen left to right
for k, ang in enumerate((161, 125.5, 90, 54.5, 19)):
    a = math.radians(ang)
    px, py = math.cos(a) * (DRUM + 3), math.sin(a) * (DRUM + 3)
    q = m.part("queueLamps_%d" % k, at=(px, py, D0 + 24), rot=(0, ang - 90, 0),
               driver=d("the build queue: one segment lit per queued item"))
    m.mesh(q, "glow_2399ff", [box((46, 4, 20), at=(px, py, D0 + 14), rot=(0, ang - 90, 0), base=True)], solid=False)
# the dish on its mast, back right; the pose turns it about the mast
DISH_AT = (330.0, -300.0, 980.0)
m.mesh(root, "hullDark", [cyl(34, 24, at=(DISH_AT[0], DISH_AT[1], DECK + 3), base=True, sides=12)])
m.mesh(root, "chrome", [cyl(9, DISH_AT[2] - DECK - 24, at=(DISH_AT[0], DISH_AT[1], DECK + 24), base=True, sides=10)])
m.mesh(root, "chrome", [bar((DISH_AT[0] + dx, DISH_AT[1] + dy, DECK + 24), (DISH_AT[0], DISH_AT[1], DECK + 160), 4,
                            shape="cyl", sides=6) for dx, dy in ((30, 0), (-15, 26), (-15, -26))], solid=False)
dish = m.part("dish", at=DISH_AT, driver=d("the dish: turns slowly once built", ("rotation",)))
m.mesh(dish, "chrome", [cyl(12, 26, at=DISH_AT, rot=(30, 0, 0), base=True, r1=80, sides=20, axis="x"),
                        cyl(3, 68, at=(DISH_AT[0] + 2 * math.cos(math.radians(30)), DISH_AT[1], DISH_AT[2] + 1.0),
                            rot=(30, 0, 0), base=True, sides=6, axis="x")], solid=False)
m.mesh(dish, "hullDark", [box((20, 24, 24), at=(DISH_AT[0] - 10, DISH_AT[1], DISH_AT[2] - 12), base=True)], solid=False)
# the steam stack, back left: emitter_1 at its mouth
STACK = (-330.0, -300.0)
m.mesh(root, "hullDark", [cyl(34, 110, at=(STACK[0], STACK[1], DECK + 3), base=True, sides=16)])
m.mesh(root, "ventGlow", [cyl(35, 8, at=(STACK[0], STACK[1], DECK + 76), base=True, sides=16)], solid=False)
m.mesh(root, "chrome", [cyl(37, 6, at=(STACK[0], STACK[1], DECK + 110), base=True, sides=16)], solid=False)
m.part("emitter_1", at=(STACK[0], STACK[1], DECK + 120), particles={"handles": "steam"},
       driver=d("steam from the stack: more while training", ("particles",)))

entry = m.write()


# --- the scaffold: poles round the walls, rails, braces ------------------------------------------------------------
s = Model("scaffold_citadel", category="construction", coplanar_check=True, notes=[
    "Construction scaffold for the citadel (C1), radius 6.3, height 9.2 (the corner towers' mounts at 9.0); shown "
    "until the building is complete."])
for name in ("chrome", "hazard"):
    s.material(name)
R = H + 25.0
TOPZ = 920.0
poles, rails, bands = [], [], []
ticks = [-R + k * (2 * R / 6) for k in range(7)]
pts = sorted({(round(x, 3), round(sy * R, 3)) for x in ticks for sy in (-1, 1)} |
             {(round(sx * R, 3), round(y, 3)) for y in ticks for sx in (-1, 1)})
for x, y in pts:
    poles.append(cyl(6, TOPZ, at=(x, y, 0), base=True, sides=6))
    bands.append(cyl(9, 32, at=(x, y, -2), base=True, sides=6))
for z in (150.0, 300.0, 450.0, 600.0, 750.0, TOPZ - 6):
    for sy in (-1, 1):
        rails.append(box((2 * R + 12, 8, 8), at=(0, sy * R, z), base=True))
    for sx in (-1, 1):
        rails.append(box((8, 2 * R + 12, 8), at=(sx * R, 0, z + 3), base=True))
braces = []
for i in range(6):
    a, b = ticks[i], ticks[i + 1]
    for lo, hi in ((0.0, 300.0), (300.0, 600.0), (600.0, 900.0)):
        for sy in (-1, 1):
            if sy > 0 and abs((a + b) / 2) < DOOR + 60:
                continue  # leave the doorway clear
            braces.append(bar((a, sy * R, lo), (b, sy * R, hi), 6, shape="cyl", sides=6))
        for sx in (-1, 1):
            braces.append(bar((sx * R, a, lo), (sx * R, b, hi), 6, shape="cyl", sides=6))
s.mesh(s.root, "chrome", poles + rails + braces)
s.mesh(s.root, "hazard", bands + [box((2 * R, 10, 10), at=(0, sy * R, TOPZ), base=True) for sy in (-1, 1)], solid=False)
scaffold = s.write()


# --- the red entries: the same meshes, team 1 ---------------------------------------------------------------------
def red_of(blue):
    base = blue["base"]
    b, r = base + "_blue", base + "_red"
    red = copy.deepcopy(blue)
    red["name"], red["team"] = r, "red"
    for p in red["parts"]:
        if p["name"] == b + "_root":
            p["name"] = r + "_root"
        if p.get("parent") == b + "_root":
            p["parent"] = r + "_root"
        p["path"] = "/" + r + p["path"][len("/" + b):]
        for x in p["meshes"]:
            x["prim"] = x["prim"].replace(b + "_root__", r + "_root__")
    return red


doc = load_catalog()
for blue in (entry, scaffold):
    log("catalog: %s %s" % (merge_entry(doc, red_of(blue)), blue["base"] + "_red"))
save_catalog(doc)
