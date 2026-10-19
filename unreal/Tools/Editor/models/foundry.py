"""The Foundry (C5, docs/building-sizes.md): where the realm builds its vehicles, re-made at the MMO's 7 by 7 cells (it
was the SceneKit export, 3 by 3, drawn stretched 2.4x). The same building: a hall between four blue-capped corner pods,
the vehicle door on the front (+Y) between the front pods, the assembly gantry on the roof with its trolley and weld
arm over a hull being built, the steam stacks back left, the dish tower and the beacon's mast back right. What a
Prospector stands beside is at its own scale: the door 3 cells wide and 2.5 tall (a Mule passes), hazard bands 12 cm,
floodlights, vents, window lamps and a 1 m roof railing.

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/foundry.py

then call_posts.py (it splices the call post into foundry_blue and foundry_red, which this script rewrites).

Writes /Game/Models/foundry/SM_<part>__<material>, the catalog entries `foundry_blue` and `foundry_red` (the same
meshes), and /Game/Models/scaffold_foundry/... with `scaffold_foundry_blue` and `scaffold_foundry_red`.

UE cm, a building: front +Y (toward the game camera), Z up, the ground at Z = 0, the footprint +-350 (Rules::radius
3.5 in the MMO; the strategy game draws it at 3 by 3, `AcPose::BuildingScale`), the body 5.76 high (Rules::height).
Parts the pose drives (AcPoseBuildings2.cpp PoseFoundry): `door` with `doorLeaves_0/1` (they slide into the front pods
while a vehicle rolls out: half width 0.75 cells, travel 1.5), `progressLamps_0..3` (the lintel's windows),
`queueLamps_0..4` (the coping's pips), `trolley` (along X over the gantry) with `weldArm` (dips 0.25), `dish`,
`beacon`, `light_2` (door), `light_4` (weld), `emitter_1` (bay sparks), `emitter_3` (weld sparks), `emitter_5`
(steam), the glow groups `floodGlow`, `ventGlow`, `bayGlow`, `weldGlow`. The door stays within |x| < 0.7 R so the call
post (call_posts.py, `ac::Act::foundryPostAt`) stands clear of it.
"""
import copy
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, bar, box, cyl, sphere, load_catalog, save_catalog, merge_entry, log  # noqa: E402

POSE = "AcPoseBuildings2.cpp:PoseFoundry"
R = 350.0          # the footprint's half side
DECK = 330.0       # the roof deck's underside (the hall's roof)
DECK_TOP = 344.0
HALL = 262.0       # the hall's half width; its front wall at HALL_FRONT
HALL_FRONT = 112.0
POD_IN = 165.0     # the pods' inner faces either side of the door
POD_OUT = 345.0
POD_FRONT_IN = 140.0   # the front pods start this far forward; the back ones end at -BACK_IN
BACK_IN = 140.0
POD_TOP = 300.0
DOOR = 150.0       # the door's half width (a Mule is 1.58 wide, 1.79 tall)
DOOR_TOP = 262.0
FLOOR = 12.0       # the plinth's top
FRONT = 345.0      # the pods' front faces
LINTEL_FRONT = 306.0
DOOR_Y = 302.0     # the leaves' middle plane


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


def slab(x0, x1, y0, y1, z0, z1):
    """An axis-aligned box from its two corners."""
    return box((x1 - x0, y1 - y0, z1 - z0), at=((x0 + x1) / 2, (y0 + y1) / 2, z0), base=True)


m = Model("foundry", category="building", coplanar_check=True, notes=[
    "C5's Foundry (docs/building-sizes.md): 7 by 7 cells, the body 5.76 high, the vehicle door 3 wide and 2.5 tall on "
    "the front between the front pods; at a Prospector's scale: hazard bands, floodlights, vents, windows, railing.",
    "Pose: PoseFoundry (production lamps, the door leaves, trolley and weld arm, dish, glow groups; the call post of "
    "call_posts.py).",
    "Under construction the body rises out of the ground like every building, inside scaffold_foundry. height 5.76"])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "glow_2399ff", "glowRed",
             "floodGlow", "ventGlow", "weldGlow", "plain_c13d03"):
    m.material(name)
# the export's own instances of names the catalog has several of
for name, key in (("hull", "a0379f0165b2"), ("bayGlow", "8de9fc39708c"), ("plain_000000", "7b71fc2a58d8")):
    m.material(name, key=key)
root = m.root

# --- the plinth and the hall ----------------------------------------------------------------------------------------
# The plinth reaches 40 below the ground to hide a slope; its top is the floor of the door's bay.
m.mesh(root, "hullDark", [slab(-R, R, -R, R, -40, FLOOR)])
# hazard bands on the plinth's front edge either side of the door (a Prospector's scale: 10 cm)
m.mesh(root, "hazard", [slab(sx * 190, sx * 330, 334, 344, FLOOR - 2, FLOOR + 2) if sx > 0 else slab(-330, -190, 334, 344, FLOOR - 2, FLOOR + 2)
                        for sx in (-1, 1)], solid=False)
m.mesh(root, "hull", [slab(-HALL, HALL, -HALL, HALL_FRONT, -6, DECK + 2)])  # (bottoms staggered under the floor)
m.mesh(root, "hullMid", [slab(-HALL - 2, HALL + 2, -HALL - 2, LINTEL_FRONT + 2, DECK, DECK_TOP)])  # the deck, 2 proud of the lintel

# --- the door's bay: lintel, windows, jambs, the header band, the bay's glow --------------------------------------
m.mesh(root, "hullMid", [slab(-POD_IN - 2, POD_IN + 2, HALL_FRONT - 2, LINTEL_FRONT, DOOR_TOP + 14, DECK + 1)])
m.mesh(root, "hullDark", [slab(-140, 140, LINTEL_FRONT - 1, LINTEL_FRONT + 3, DOOR_TOP + 24, DECK - 6)], solid=False)
m.mesh(root, "hazard", [slab(-DOOR - 14, DOOR + 14, LINTEL_FRONT - 2, LINTEL_FRONT + 2, DOOR_TOP, DOOR_TOP + 16)], solid=False)
m.mesh(root, "hazard", [slab(sx * DOOR, sx * (DOOR + 14), DOOR_Y - 4, LINTEL_FRONT + 4, FLOOR - 2, DOOR_TOP - 2) if sx > 0 else
                        slab(-DOOR - 14, -DOOR, DOOR_Y - 4, LINTEL_FRONT + 4, FLOOR - 2, DOOR_TOP - 2) for sx in (-1, 1)])
# the bay's back wall: its glow strip and the hull being finished (seen while the door is open)
m.mesh(root, "bayGlow", [slab(-120, 120, HALL_FRONT - 2, HALL_FRONT + 3, 200, 214)], solid=False)
m.mesh(root, "plain_000000", [slab(-POD_IN + 4, POD_IN - 4, HALL_FRONT - 2, HALL_FRONT + 2, 30, 190)], solid=False)
# the four progress windows on the lintel, numbered left to right (blue: production state)
for k, x in enumerate((-99.0, -33.0, 33.0, 99.0)):
    lamp = m.part(f"progressLamps_{k}", at=(x, LINTEL_FRONT + 4, 305), driver=d(f"progress window {k} (blue)"))
    m.mesh(lamp, "glow_2399ff", [slab(x - 28, x + 28, LINTEL_FRONT + 1, LINTEL_FRONT + 5, 294, 316)], solid=False)
# the five queue pips on the deck's coping, over the windows
for k, x in enumerate((-104.0, -52.0, 0.0, 52.0, 104.0)):
    pip = m.part(f"queueLamps_{k}", at=(x, LINTEL_FRONT + 4, DECK + 7), driver=d(f"queue pip {k} (blue)"))
    m.mesh(pip, "glow_2399ff", [slab(x - 15, x + 15, LINTEL_FRONT, LINTEL_FRONT + 5, DECK + 3, DECK + 11)], solid=False)

# --- the door: two leaves meeting in the middle, sliding into the front pods ---------------------------------------
DOOR_MID = (FLOOR + DOOR_TOP) / 2
door = m.part("door", at=(0, DOOR_Y, DOOR_MID), driver=d("the door's frame (the leaves under it)", ("position",)))
for k, sx in enumerate((-1, 1)):
    cx = sx * DOOR / 2
    leaf = m.part(f"doorLeaves_{k}", parent=door, at=(cx, DOOR_Y, DOOR_MID),
                  driver=d(f"door leaf {k}: slides out by 1.5 cells while a vehicle rolls out", ("position",)))
    x0, x1 = sorted((sx * 1.0, sx * (DOOR - 1.0)))
    m.mesh(leaf, "hullDark", [slab(x0, x1, DOOR_Y - 3, DOOR_Y + 3, FLOOR + 1, DOOR_TOP - 1)])
    m.mesh(leaf, "hullMid", [slab(x0 + 14, x1 - 14, DOOR_Y + 1, DOOR_Y + 5, 44, 200)], solid=False)
    m.mesh(leaf, "hazard", [slab(x0 + 8, x1 - 8, DOOR_Y + 1, DOOR_Y + 6, z, z + 12) for z in (24.0, 222.0)], solid=False)
m.part("light_2", at=(0, 390, 190),
       light={"attenuationEnd": "4.8", "color": "SIMD4<Float>(1.0, 0.6615596, 0.23560244, 1.0)", "handles": "doorLight",
              "intensity": "0.0", "type": "omni"},
       driver=d("the door light: on while the door is open", ("light",)))
m.part("emitter_1", at=(70, 160, 85),
       particles={"birthRate": "0.0", "blend": "0", "color": "sRGB IEC61966-2.1 colorspace hdrm(1) 1 0.791286 0.422233 1",
                  "handles": "baySparks", "lifeSpan": "0.35", "size": "0.035", "velocity": "2.4"},
       driver=d("sparks in the bay while it builds", ("particles",)))

# --- the four corner pods: dark bodies, blue (team) caps, rollers; vents, panels and floodlights on the front -----
for sx in (-1, 1):
    for sy in (-1, 1):
        xa, xb = sorted((sx * POD_IN, sx * POD_OUT))
        ya, yb = (POD_FRONT_IN, FRONT) if sy > 0 else (-FRONT, -BACK_IN)
        cx, cy = (xa + xb) / 2, (ya + yb) / 2
        m.mesh(root, "hullDark", [slab(xa, xb, ya, yb, 4, POD_TOP)])
        m.mesh(root, "team", [slab(xa - 2, xb + 2, ya - 2, yb + 2, POD_TOP - 4, POD_TOP + 40),
                              slab(xa + 18, xb - 18, ya + 18, yb - 18, POD_TOP + 39, POD_TOP + 86),
                              slab(xa + 12, xb - 12, ya + 12, yb - 12, POD_TOP + 98, POD_TOP + 180)])
        m.mesh(root, "hullDark", [slab(xa + 8, xb - 8, ya + 8, yb - 8, POD_TOP + 85, POD_TOP + 99)])
        m.mesh(root, "hullDark", [cyl(10, 110, at=(cx + dx, cy - 55, POD_TOP + 188), axis="y", base=True, sides=10)
                                  for dx in (-45.0, 0.0, 45.0)])
        m.mesh(root, "hazard", [slab(xa + 2, xb - 2, (yb - 2) if sy > 0 else (ya - 4), (yb + 4) if sy > 0 else (ya + 2), 16, 28)],
               solid=False)
        if sy < 0:
            continue
        # the front face: a light panel, a vent below it, a floodlight on the door's side
        m.mesh(root, "hullLight", [slab(xa + 22, xb - 22, FRONT - 2, FRONT + 4, 150, 262)], solid=False)
        m.mesh(root, "hullMid", [slab(xa + 30, xb - 30, FRONT - 2, FRONT + 2, 40, 126)], solid=False)
        m.mesh(root, "ventGlow", [slab(xa + 36, xb - 36, FRONT, FRONT + 5, z, z + 6) for z in (50.0, 66.0, 82.0, 98.0, 114.0)],
               solid=False)
        fx = sx * (POD_IN + 30)
        m.mesh(root, "hullDark", [slab(fx - 16, fx + 16, FRONT - 2, FRONT + 4, 266, 298)], solid=False)
        m.mesh(root, "floodGlow", [slab(fx - 12, fx + 12, FRONT + 2, FRONT + 8, 272, 296)], solid=False)

# --- the sides: an annex on the left, a rack on the right ----------------------------------------------------------
m.mesh(root, "hullMid", [slab(-POD_OUT, -HALL + 2, -120, 120, 8, 230)])
m.mesh(root, "chrome", [cyl(8, 236, at=(-POD_OUT - 6, -118, 186), axis="y", base=True, sides=10)], solid=False)
m.mesh(root, "hazard", [slab(-POD_OUT - 4, -POD_OUT + 2, -110, 110, 16, 28)], solid=False)
m.mesh(root, "hullDark", [slab(HALL - 2, POD_OUT, -70, 70, 2, 62)])
m.mesh(root, "chrome", [bar((POD_OUT - 4, y, 62), (POD_OUT - 4, y, 180), 6, shape="cyl", sides=6) for y in (-64.0, 64.0)] +
       [bar((POD_OUT - 4, -64, 180), (POD_OUT - 4, 64, 180), 6, shape="cyl", sides=6)], solid=False)

# --- the roof railing (1 m, a Prospector's scale) along the front coping and the sides between the pods ----------
rails = []
for x in range(-140, 141, 56):
    rails.append(cyl(3, 102, at=(x, LINTEL_FRONT - 8, DECK_TOP - 2), base=True, sides=6))
rails += [bar((-142, LINTEL_FRONT - 8, DECK_TOP + z), (142, LINTEL_FRONT - 8, DECK_TOP + z), 4, shape="cyl", sides=6)
          for z in (50.0, 100.0)]
for sx in (-1, 1):
    for y in (-120.0, -40.0, 40.0, 120.0):
        rails.append(cyl(3, 102, at=(sx * (HALL - 8), y, DECK_TOP - 2), base=True, sides=6))
    rails += [bar((sx * (HALL - 8), -122, DECK_TOP + z), (sx * (HALL - 8), 122, DECK_TOP + z), 4, shape="cyl", sides=6)
              for z in (50.0, 100.0)]
m.mesh(root, "chrome", rails, solid=False)

# --- the assembly gantry: posts, rails, the trolley and its weld arm over a hull being built -----------------------
GANTRY_Y = (-40.0, 120.0)
RAIL_Z = 470.0
m.mesh(root, "hullDark", [slab(x - 7, x + 7, y - 7, y + 7, DECK_TOP - 2, RAIL_Z + 2) for x in (-95.0, 185.0) for y in GANTRY_Y])
m.mesh(root, "hullMid", [slab(-104, 194, y - 8, y + 8, RAIL_Z, RAIL_Z + 12) for y in GANTRY_Y])
m.mesh(root, "hazard", [slab(-102, 192, y - 10, y + 10, RAIL_Z + 2, RAIL_Z + 10) for y in GANTRY_Y], solid=False)
TROLLEY = (45.0, 40.0, RAIL_Z + 12)
trolley = m.part("trolley", at=TROLLEY, driver=d("the trolley: along X between weld stations while it builds", ("position",)))
m.mesh(trolley, "plain_c13d03", [slab(TROLLEY[0] - 30, TROLLEY[0] + 30, GANTRY_Y[0] - 12, GANTRY_Y[1] + 12, TROLLEY[2] - 2, TROLLEY[2] + 18)])
arm = m.part("weldArm", parent=trolley, at=TROLLEY, driver=d("the weld arm: dips at each station", ("position",)))
m.mesh(arm, "chrome", [cyl(5, 80, at=(TROLLEY[0], TROLLEY[1], TROLLEY[2] - 80), base=True, sides=8)])
m.mesh(arm, "hullDark", [slab(TROLLEY[0] - 8, TROLLEY[0] + 8, TROLLEY[1] - 8, TROLLEY[1] + 8, TROLLEY[2] - 96, TROLLEY[2] - 78)])
# the hull on the jig, its seam the weld's glow
m.mesh(root, "hullDark", [slab(-30, 120, 0, 82, DECK_TOP - 2, DECK_TOP + 26)])
m.mesh(root, "hullMid", [slab(-14, 104, 12, 70, DECK_TOP + 25, DECK_TOP + 36)])
m.mesh(root, "weldGlow", [slab(40, 50, 16, 66, DECK_TOP + 35, DECK_TOP + 38)], solid=False)
m.part("emitter_3", at=(TROLLEY[0], TROLLEY[1], DECK_TOP + 40),
       particles={"birthRate": "0.0", "blend": "0", "color": "sRGB IEC61966-2.1 colorspace hdrm(1) 1 0.791286 0.422233 1",
                  "handles": "sparks", "lifeSpan": "0.35", "size": "0.035", "velocity": "1.8"},
       driver=d("weld sparks at the arm's tip", ("particles",)))
m.part("light_4", at=(TROLLEY[0], TROLLEY[1], DECK_TOP + 80),
       light={"attenuationEnd": "3.8", "color": "SIMD4<Float>(0.7445536, 0.8313106, 1.0, 1.0)", "handles": "weldLight",
              "intensity": "0.0", "type": "omni"},
       driver=d("the weld's flash", ("light",)))

# --- the steam stacks, back left ------------------------------------------------------------------------------------
for x in (-150.0, -90.0):
    m.mesh(root, "hullDark", [cyl(22, 156, at=(x, -230, DECK_TOP - 2), base=True, sides=14)])
    m.mesh(root, "plain_c13d03", [cyl(24, 10, at=(x, -230, DECK_TOP + 112), base=True, sides=14)], solid=False)
    m.mesh(root, "chrome", [cyl(25, 6, at=(x, -230, DECK_TOP + 150), base=True, sides=14)], solid=False)
m.part("emitter_5", at=(-120, -230, DECK_TOP + 162),
       particles={"birthRate": "1.0", "blend": "0", "color": "sRGB IEC61966-2.1 colorspace hdrm(1) 0.85 0.85 0.85 0.22",
                  "handles": "steam", "lifeSpan": "1.8", "size": "0.22", "velocity": "0.7"},
       driver=d("steam from the stacks: more while it builds", ("particles",)))

# --- the dish tower and the beacon's mast, back right ---------------------------------------------------------------
TOWER_TOP = 555.0
m.mesh(root, "hullMid", [slab(20, 160, -300, -170, DECK_TOP - 2, TOWER_TOP)])
m.mesh(root, "team", [slab(18, 162, -302, -168, 500, 514)])
m.mesh(root, "hullDark", [slab(50, 130, -172, -166, 400, 430)], solid=False)
DISH_AT = (80.0, -215.0, TOWER_TOP + 30)
m.mesh(root, "chrome", [cyl(8, 32, at=(DISH_AT[0], DISH_AT[1], TOWER_TOP - 2), base=True, sides=8)])
dish = m.part("dish", at=DISH_AT, driver=d("the dish: sweeps slowly once built", ("rotation",)))
m.mesh(dish, "hullLight", [cyl(8, 18, at=DISH_AT, rot=(30, 0, 0), base=True, r1=58, sides=20, axis="x")], solid=False)
m.mesh(dish, "chrome", [cyl(2.5, 50, at=(DISH_AT[0] + 2, DISH_AT[1], DISH_AT[2] + 1), rot=(30, 0, 0), base=True, sides=6, axis="x")],
       solid=False)
MAST = (140.0, -285.0)
m.mesh(root, "chrome", [cyl(5, 192, at=(MAST[0], MAST[1], TOWER_TOP - 2), base=True, sides=8)], solid=False)
beacon = m.part("beacon", at=(MAST[0], MAST[1], TOWER_TOP + 198),
                driver=d("beacon blink (0.25 s of every 1.2 s once built)", ("visibility",)))
m.mesh(beacon, "glowRed", sphere(9, at=(MAST[0], MAST[1], TOWER_TOP + 198), steps=8), solid=False)

entry = m.write()


# --- the scaffold: poles round the footprint, rails, braces; the door's way left clear ------------------------------
s = Model("scaffold_foundry", category="construction", coplanar_check=True, notes=[
    "Construction scaffold for the foundry (C5), radius 3.75, height 6.0; shown until the building is complete."])
for name in ("chrome", "hazard"):
    s.material(name)
SR = R + 25.0
TOPZ = 600.0
ticks = [-SR + k * (2 * SR / 4) for k in range(5)]
pts = sorted({(round(x, 3), round(sy * SR, 3)) for x in ticks for sy in (-1, 1)} |
             {(round(sx * SR, 3), round(y, 3)) for y in ticks for sx in (-1, 1)})
poles, bands, rails_s, braces = [], [], [], []
for x, y in pts:
    poles.append(cyl(6, TOPZ, at=(x, y, 0), base=True, sides=6))
    bands.append(cyl(9, 32, at=(x, y, -2), base=True, sides=6))
for z in (150.0, 300.0, 450.0, TOPZ - 6):
    for sy in (-1, 1):
        rails_s.append(box((2 * SR + 12, 8, 8), at=(0, sy * SR, z), base=True))
    for sx in (-1, 1):
        rails_s.append(box((8, 2 * SR + 12, 8), at=(sx * SR, 0, z + 3), base=True))
for i in range(4):
    a, b = ticks[i], ticks[i + 1]
    for lo, hi in ((0.0, 300.0), (300.0, 600.0)):
        for sy in (-1, 1):
            if sy > 0 and lo == 0.0 and abs((a + b) / 2) < DOOR + 60:
                continue  # the door's way
            braces.append(bar((a, sy * SR, lo), (b, sy * SR, hi), 6, shape="cyl", sides=6))
        for sx in (-1, 1):
            braces.append(bar((sx * SR, a, lo), (sx * SR, b, hi), 6, shape="cyl", sides=6))
s.mesh(s.root, "chrome", poles + rails_s + braces)
s.mesh(s.root, "hazard", bands + [box((2 * SR, 10, 10), at=(0, sy * SR, TOPZ), base=True) for sy in (-1, 1)], solid=False)
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
