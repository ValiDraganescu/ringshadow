"""Gatling gun: the realm's ground and close-air defence (D2, docs/mmorpg-m8.md; concept
art/models/gatling/concepts/c1-forge.jpg): a squat square base with cooling fins and a glowing heat vent at the back,
a turret ring, a yoke holding a rotary cannon of six barrels in a ribbed cooling jacket, and a furnace hopper on top
where the stardust ore and MH amalgam glows before it is fired as red-hot rounds. Blockout from the concept
(pipeline steps 0, 1, 6, 7).

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/gatling.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/gatling/SM_<part>__<material> and the catalog entry `gatling_blue`.

UE cm, a building: its front (+Y) toward the game camera, Z up, the pad on Z = 0, a 2 by 2 footprint (200 cm a
side) like the Hab Dome and the Sentinel, about 1.3 cells high at the hopper. The gun points along +X at rest: the
pose turns `head` to the target (yaw = -aim, as the Sentinel's), pitches `cradle` (C2: on the barrel axis 0.3 along
and 1.28 up, SceneKit euler (0, 0, +pitch) as the corner guns'), spins `barrels` about their axis while it fires,
and drives the glows: `tips` (the barrels' hot ends) and `hopper` (the furnace) brighten while firing and cool
after; `workGlow` slits are the building's yellow working light; `flashes_0` shows a frame per round at
`muzzles_0`; `beacon` blinks red once built.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, bar, box, cyl, sphere  # noqa: E402

POSE = "AcPoseBuildings2.cpp:PoseGatling"

m = Model("gatling", category="building", coplanar_check=True, notes=[
    "D2's gatling gun (docs/mmorpg-m8.md). Pose: PoseGatling: head yaw to the target, barrels spin while firing, "
    "tips and hopper glow with the heat, workGlow slits the working light, flashes_0 per round, beacon.",
    "Under construction the body rises out of the ground like every building. height 1.3"])
for name in ("hullLight", "hullMid", "hullDark", "hazard", "team", "chrome", "workGlow", "glowRed", "glowYellow"):
    m.material(name)
# The amalgam's heat: white-hot orange at the barrel tips, furnace orange in the hopper and the back vent. N1b: the
# base colour is the hue (the unlit master adds it to the lamp: a white base washed both to white).
m.material("gatlingHeat", parent="emissive", color=(0.55, 0.24, 0.05), emissive=(2.6, 1.0, 0.22))
m.material("gatlingFurnace", parent="emissive", color=(0.45, 0.10, 0.0), emissive=(2.2, 0.55, 0.04))
m.material("gatlingMolten", parent="emissive", color=(0.6, 0.22, 0.02), emissive=(3.0, 1.1, 0.12))
m.material("gunmetal", parent="hull", color=(0.035, 0.036, 0.04), metallic=0.85, roughness=0.32)


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


# --- the base: plinth with hazard edges, a squat block with cooling fins, the team band, working slits ----------
m.mesh(m.root, "hullDark", [box((196, 196, 12), base=True)])
m.mesh(m.root, "hazard", [box((150, 4, 6), at=(0, s * 97, 3), base=True) for s in (-1, 1)] +
       [box((4, 150, 6), at=(s * 97, 0, 3), base=True) for s in (-1, 1)], solid=False)
m.mesh(m.root, "hullMid", [box((164, 164, 58), at=(0, 0, 12), base=True)])
# chamfered corners
m.mesh(m.root, "hullDark", [box((26, 26, 58), at=(sx * 76, sy * 76, 12), rot=(0, 45, 0), base=True)
                            for sx in (-1, 1) for sy in (-1, 1)])
# cooling fins down the two sides and the back, like the concept's
fins = []
for k in range(5):
    x = -48 + k * 24
    fins += [box((8, 14, 46), at=(x, s * 85, 16), base=True) for s in (-1, 1)]
    fins.append(box((14, 8, 46), at=(-85, x, 16), base=True))
m.mesh(m.root, "hullLight", fins)
# a riveted front plate
m.mesh(m.root, "hullLight", [box((70, 6, 40), at=(0, 84, 18), base=True)])
m.mesh(m.root, "chrome", [sphere(2.5, at=(x, 87.5, z), steps=5) for x in (-30, 30) for z in (24, 52)], solid=False)
m.mesh(m.root, "hullDark", [box((160, 160, 6), at=(0, 0, 70), base=True)])
m.mesh(m.root, "team", [box((168, 12, 8), at=(0, s * 78, 70), base=True) for s in (-1, 1)] +
       [box((12, 144, 8), at=(s * 78, 0, 70), base=True) for s in (-1, 1)])
m.mesh(m.root, "workGlow", [box((26, 3, 7), at=(x, 83, 60), base=True) for x in (-58, 58)] +
       [box((3, 26, 7), at=(83, y, 60), base=True) for y in (-40, 40)], solid=False)
# the concept's orange heat slits over the fins, front and sides
m.mesh(m.root, "gatlingFurnace", [box((30, 3, 6), at=(x, s * 84, 64), base=True) for x in (-30, 30) for s in (-1, 1)] +
       [box((3, 30, 6), at=(84, y, 64), base=True) for y in (-17, 17)], solid=False)
# the heat vent at the back
m.mesh(m.root, "hullDark", [box((10, 70, 34), at=(-86, 0, 22), base=True)])
m.mesh(m.root, "gatlingFurnace", [box((3, 54, 4), at=(-91.5, 0, 26 + 7 * k), base=True) for k in range(4)], solid=False)

# --- the head: turret ring, yoke, ammunition drums, the furnace hopper -----------------------------------------
head = m.part("head", at=(0, 0, 78), driver=d("turret aim: yaw toward its target (-aim), an idle sweep with none"))
m.mesh(head, "hullDark", [cyl(62, 16, at=(0, 0, 78), base=True, sides=24)])
m.mesh(head, "chrome", [cyl(64, 3, at=(0, 0, 86), base=True, sides=24)], solid=False)
m.mesh(head, "hullMid", [box((84, 64, 34), at=(-4, 0, 94), base=True)])
# the yoke's two cheeks round the trunnion and their drums (the side drums of the concept)
for s in (-1, 1):
    m.mesh(head, "hullLight", [box((40, 16, 66), at=(18, s * 42, 94), base=True)])
    m.mesh(head, "hullDark", [cyl(24, 22, at=(18, s * 50 - 11, 128), axis="y", base=True, sides=20)])
    m.mesh(head, "chrome", [cyl(14, 4, at=(18, s * 61 - 2, 128), axis="y", base=True, sides=16)], solid=False)
    m.mesh(head, "team", [box((36, 3, 10), at=(18, s * 50.5, 100), base=True)], solid=False)
# the hopper behind the gun: a tall square funnel of steel widening upward (the concept's furnace), a heavy rim (its
# four bars butted at the corners), the amalgam molten in its mouth 2 cm over the funnel's top, glowing through slits
HX = -26.0
m.mesh(head, "hullDark", [cyl(36, 54, at=(HX, 0, 128), base=True, r1=66, sides=4, rot=(0, 45, 0))])
m.mesh(head, "hullLight", [box((100, 12, 10), at=(HX, s * 44, 180), base=True) for s in (-1, 1)] +
       [box((12, 76, 10), at=(HX + s * 44, 0, 180), base=True) for s in (-1, 1)])
hopper = m.part("hopper", parent=head, at=(HX, 0, 184), driver=d("the furnace glow: brighter while it fires", ("emission",)))
m.mesh(hopper, "gatlingMolten", [box((76, 76, 4), at=(HX, 0, 180), base=True)], solid=False)
m.mesh(hopper, "gatlingFurnace", [box((3, 34, 7), at=(HX + s * 35, 0, 150), base=True) for s in (-1, 1)] +
       [box((34, 3, 7), at=(HX, s * 35, 150), base=True) for s in (-1, 1)] +
       [box((3, 40, 4), at=(HX + s * 46, 0, 174), base=True) for s in (-1, 1)] +
       [box((40, 3, 4), at=(HX, s * 46, 174), base=True) for s in (-1, 1)], solid=False)
beacon = m.part("beacon", parent=head, at=(HX - 34, 30, 190), driver=d("beacon blink (0.25 s of every 1.2 s once built)",
                                                                         ("visibility",)))
m.mesh(beacon, "glowRed", sphere(5, at=(HX - 46, 40, 192), steps=8), solid=False)

# --- the rotary cannon, the minigun look (docs/guns-and-practice.md section 2, art/models/guns/ref/minigun-reference.png):
# a stubby rotor housing on the yoke, six long barrels round a spindle held by a mid and a front clamp, a flash hider
# collar near the muzzle, the barrels' hot ends; an ammunition box on the head's flank feeding the housing by a chute
AXIS = (30, 0, 128)
HOUSING = (22.0, 54.0)  # the rotor housing along x; the barrels run on from its front to MUZZLE
MUZZLE = 150.0
# C2: the gun pitches (the AI's ballistic solve) on `cradle`, its pivot on the barrel axis 0.3 along and 1.28 up from
# the gun's middle, where the core's gatling pitches from (the muzzle 1.28 past it: 1.58 out when level); as the corner
# guns' turretCradles_k: SceneKit euler (0, 0, +pitch)
cradle = m.part("cradle", parent=head, at=AXIS, driver=d(
    "the gun's pitch about the barrel axis, 0.3 along and 1.28 up from the gun's middle (the AI's ballistic solve), as "
    "the corner guns' turretCradles_k"))
m.mesh(cradle, "gunmetal", [cyl(22, HOUSING[1] - HOUSING[0], at=(HOUSING[0], 0, 128), axis="x", base=True, sides=20),
                          cyl(14, 8, at=(HOUSING[0] - 8, 0, 128), axis="x", base=True, r1=22, sides=20)])
m.mesh(cradle, "chrome", [cyl(23, 3, at=(HOUSING[1] - 4, 0, 128), axis="x", base=True, sides=20)], solid=False)
m.mesh(cradle, "hullDark", [cyl(23, 3, at=(HOUSING[0] + 6 + 8 * k, 0, 128), axis="x", base=True, sides=20) for k in range(3)],
       solid=False)  # the housing's ribs
# the ammunition box on the left flank (+Y, outside the yoke's cheek) and its chute into the yoke's drum (the feed goes
# on inside the trunnion, which the pitch does not move)
m.mesh(head, "hullMid", [box((48, 26, 38), at=(-6, 64, 96), base=True)])
m.mesh(head, "hazard", [box((50, 2, 8), at=(-6, 78, 122), base=True)], solid=False)
m.mesh(head, "hullDark", [box((44, 22, 4), at=(-6, 64, 134), base=True)], solid=False)  # its lid
m.mesh(head, "hullDark", [bar((-4, 62, 136), (16, 50, 140), 11, 4)], solid=False)
barrels = m.part("barrels", parent=cradle, at=AXIS, driver=d("spins about its axis while it fires"))
bores = []
for k in range(6):
    a = k * math.pi / 3
    bores.append((math.cos(a) * 11, math.sin(a) * 11))
m.mesh(barrels, "gunmetal", [cyl(3.8, MUZZLE - HOUSING[1], at=(HOUSING[1], y, 128 + z), axis="x", base=True, sides=10)
                             for y, z in bores])
m.mesh(barrels, "hullDark", [cyl(4.5, MUZZLE - HOUSING[1] - 8, at=(HOUSING[1], 0, 128), axis="x", base=True, sides=10)])
m.mesh(barrels, "hullDark", [cyl(15.5, 6, at=(x, 0, 128), axis="x", base=True, sides=18) for x in (88, 126)])  # clamps
m.mesh(barrels, "chrome", [cyl(16.5, 6, at=(MUZZLE - 10, 0, 128), axis="x", base=True, sides=18)])  # the flash hider
tips = m.part("tips", parent=barrels, at=(MUZZLE + 1.5, 0, 128), driver=d("the barrels' white-hot ends: brighter while it fires",
                                                                          ("emission",)))
m.mesh(tips, "gatlingHeat", [cyl(3.8, 3, at=(MUZZLE, y, 128 + z), axis="x", base=True, sides=10) for y, z in bores],
       solid=False)
# where the rounds leave (the effects read it), and the flash there
m.part("muzzles_0", parent=cradle, at=(MUZZLE + 8, 0, 128))
flash = m.part("flashes_0", parent=cradle, at=(MUZZLE + 14, 0, 128), hidden=True,
               driver=d("muzzle flash: shown a frame per round", ("visibility",)))
m.mesh(flash, "glowYellow", sphere(14, at=(MUZZLE + 14, 0, 128), squash=(1.6, 1, 1), steps=8), solid=False)

m.write()
