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
pose turns `head` to the target (yaw = -aim, as the Sentinel's), spins `barrels` about their axis while it fires,
and drives the glows: `tips` (the barrels' hot ends) and `hopper` (the furnace) brighten while firing and cool
after; `workGlow` slits are the building's yellow working light; `flashes_0` shows a frame per round at
`muzzles_0`; `beacon` blinks red once built.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, sphere  # noqa: E402

POSE = "AcPoseBuildings2.cpp:PoseGatling"

m = Model("gatling", category="building", notes=[
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
       [box((3, 30, 6), at=(84, y, 64), base=True) for y in (-10, 10)], solid=False)
# the heat vent at the back
m.mesh(m.root, "hullDark", [box((10, 70, 34), at=(-86, 0, 22), base=True)])
m.mesh(m.root, "gatlingFurnace", [box((3, 56, 4), at=(-91.5, 0, 26 + 7 * k), base=True) for k in range(4)], solid=False)

# --- the head: turret ring, yoke, ammunition drums, the furnace hopper -----------------------------------------
head = m.part("head", at=(0, 0, 78), driver=d("turret aim: yaw toward its target (-aim), an idle sweep with none"))
m.mesh(head, "hullDark", [cyl(62, 16, at=(0, 0, 78), base=True, sides=24)])
m.mesh(head, "chrome", [cyl(64, 3, at=(0, 0, 86), base=True, sides=24)], solid=False)
m.mesh(head, "hullMid", [box((84, 64, 34), at=(-4, 0, 94), base=True)])
# the yoke's two cheeks and their trunnion drums (the side drums of the concept)
for s in (-1, 1):
    m.mesh(head, "hullLight", [box((76, 16, 66), at=(18, s * 42, 94), base=True)])
    m.mesh(head, "hullDark", [cyl(24, 22, at=(18, s * 50 - 11, 128), axis="y", base=True, sides=20)])
    m.mesh(head, "chrome", [cyl(14, 4, at=(18, s * 61 - 2, 128), axis="y", base=True, sides=16)], solid=False)
    m.mesh(head, "team", [box((40, 3, 10), at=(18, s * 50.5, 100), base=True)], solid=False)
# the hopper: a tall square funnel of steel widening upward (the concept's furnace), a heavy rim, the amalgam molten
# in its mouth and glowing through slits in its faces
m.mesh(head, "hullDark", [cyl(36, 54, at=(-6, 0, 128), base=True, r1=66, sides=4, rot=(0, 45, 0))])
m.mesh(head, "hullLight", [box((100, 12, 10), at=(-6, s * 44, 180), base=True) for s in (-1, 1)] +
       [box((12, 100, 10), at=(-6 + s * 44, 0, 180), base=True) for s in (-1, 1)])
hopper = m.part("hopper", parent=head, at=(-6, 0, 184), driver=d("the furnace glow: brighter while it fires", ("emission",)))
m.mesh(hopper, "gatlingMolten", [box((78, 78, 4), at=(-6, 0, 178), base=True)], solid=False)
m.mesh(hopper, "gatlingFurnace", [box((3, 34, 7), at=(-6 + s * 35, 0, 150), base=True) for s in (-1, 1)] +
       [box((34, 3, 7), at=(-6, s * 35, 150), base=True) for s in (-1, 1)] +
       [box((3, 40, 4), at=(-6 + s * 46, 0, 174), base=True) for s in (-1, 1)] +
       [box((40, 3, 4), at=(-6, s * 46, 174), base=True) for s in (-1, 1)], solid=False)
beacon = m.part("beacon", parent=head, at=(-40, 30, 190), driver=d("beacon blink (0.25 s of every 1.2 s once built)",
                                                                      ("visibility",)))
m.mesh(beacon, "glowRed", sphere(5, at=(-52, 40, 192), steps=8), solid=False)

# --- the rotary cannon: the ribbed jacket, six barrels round the axle, their hot tips ---------------------------
AXIS = (30, 0, 128)
barrels = m.part("barrels", parent=head, at=AXIS, driver=d("spins about its axis while it fires"))
m.mesh(barrels, "gunmetal", [cyl(25, 46, at=(30, 0, 128), axis="x", base=True, sides=18)])
# the ribbed cooling jacket: deep fins round it, a chrome band at each end
m.mesh(barrels, "hullDark", [cyl(30, 3.5, at=(36 + 5.5 * k, 0, 128), axis="x", base=True, sides=18) for k in range(7)], solid=False)
m.mesh(barrels, "chrome", [cyl(28, 4, at=(x, 0, 128), axis="x", base=True, sides=18) for x in (31, 73)], solid=False)
m.mesh(barrels, "hullMid", [cyl(20, 6, at=(76, 0, 128), axis="x", base=True, sides=18),
                            cyl(20, 6, at=(112, 0, 128), axis="x", base=True, sides=18)])
bores = []
for k in range(6):
    a = k * math.pi / 3
    bores.append((math.cos(a) * 13, math.sin(a) * 13))
m.mesh(barrels, "gunmetal", [cyl(5.5, 62, at=(70, y, 128 + z), axis="x", base=True, sides=10) for y, z in bores])
tips = m.part("tips", parent=barrels, at=(132, 0, 128), driver=d("the barrels' white-hot ends: brighter while it fires",
                                                                 ("emission",)))
m.mesh(tips, "gatlingHeat", [cyl(5.8, 6, at=(130, y, 128 + z), axis="x", base=True, sides=10) for y, z in bores],
       solid=False)
# where the rounds leave (the effects read it), and the flash there
m.part("muzzles_0", parent=head, at=(140, 0, 128))
flash = m.part("flashes_0", parent=head, at=(146, 0, 128), hidden=True,
               driver=d("muzzle flash: shown a frame per round", ("visibility",)))
m.mesh(flash, "glowYellow", sphere(14, at=(146, 0, 128), squash=(1.6, 1, 1), steps=8), solid=False)

m.write()
