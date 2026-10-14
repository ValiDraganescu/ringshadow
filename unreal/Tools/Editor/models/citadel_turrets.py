"""The Citadel's four corner turrets (D1, docs/mmorpg-m8.md): rapid rotary close-in guns, one on each of the Citadel
model's four corner pylons, that track and shred incoming bugs, ground and air, with long tracer streams. They come
with every Citadel of a world (built with it, no blueprint): the core fires them (`Defence::cornerGun`, four guns per
armed Citadel) and the pose shows them only on an armed Citadel (`Structure::marks`), so the strategy game's
Citadels look as they always did. Small cousins of the gatling gun (gatling.py): a turret ring, a squat housing and a
six-barrel cluster with white-hot tips.

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/citadel_turrets.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/citadelguns/SM_<part>__<material>, then adds the parts to the `citadel_blue` and `citadel_red`
catalog entries (parented to the Citadel's root, hidden at rest) and leaves no entry of its own. Run again, it
replaces the parts it added.

UE cm in the Citadel's model frame (front +Y, Z up). Turret `k` stands on the pylon at corner k of the core's order:
0 (+x, +y), 1 (-x, +y), 2 (-x, -y), 3 (+x, -y), at the pylons' top (153 cm). Parts per turret: `turrets_k` (yaw to
its target: -aim), `turretBarrels_k` (spins about its axis while firing), `turretTips_k` (heat glow),
`turretMuzzles_k` (where the rounds leave), `turretFlashes_k` (a frame per round).
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, sphere, load_catalog, save_catalog  # noqa: E402

POSE = "AcPoseBuildings.cpp:PoseCitadel"
CORNER = 195.0
TOP = 153.0
PREFIXES = ("turrets_", "turretBarrels_", "turretTips_", "turretMuzzles_", "turretFlashes_")

m = Model("citadelguns", category="building")
for name in ("hullLight", "hullMid", "hullDark", "team", "chrome", "glowYellow"):
    m.material(name)
m.material("gatlingHeat", parent="emissive", color=(1, 1, 1), emissive=(2.2, 1.0, 0.35))


def d(what, channels=("rotation",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


for k, (sx, sy) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
    x, y = sx * CORNER, sy * CORNER
    t = m.part("turrets_%d" % k, at=(x, y, TOP), hidden=True,
               driver=d("corner turret %d: yaw toward its target (-aim); shown only on an armed Citadel" % k,
                        ("rotation", "visibility")))
    m.mesh(t, "hullDark", [cyl(26, 8, at=(x, y, TOP), base=True, sides=20)])
    m.mesh(t, "chrome", [cyl(27, 2, at=(x, y, TOP + 6), base=True, sides=20)], solid=False)
    m.mesh(t, "hullMid", [box((34, 30, 20), at=(x - 2, y, TOP + 8), base=True)])
    for s in (-1, 1):
        m.mesh(t, "hullLight", [box((28, 6, 26), at=(x + 6, y + s * 17, TOP + 8), base=True)])
    m.mesh(t, "team", [box((20, 2, 6), at=(x - 2, y + 15.5, TOP + 16), base=True)], solid=False)
    axis = (x + 12, y, TOP + 22)
    b = m.part("turretBarrels_%d" % k, parent=t, at=axis, hidden=True,
               driver=d("corner turret %d's barrels: spin about their axis while it fires" % k, ("rotation", "visibility")))
    m.mesh(b, "hullDark", [cyl(10, 18, at=axis, axis="x", base=True, sides=14)])
    m.mesh(b, "chrome", [cyl(11, 2.5, at=(axis[0] + 6 + 5 * j, y, axis[2]), axis="x", base=True, sides=14) for j in range(2)],
           solid=False)
    bores = [(math.cos(a) * 5.5, math.sin(a) * 5.5) for a in (j * math.pi / 3 for j in range(6))]
    m.mesh(b, "hullDark", [cyl(2.4, 30, at=(axis[0] + 16, y + by, axis[2] + bz), axis="x", base=True, sides=8)
                           for by, bz in bores])
    tips = m.part("turretTips_%d" % k, parent=b, at=(axis[0] + 46, y, axis[2]), hidden=True,
                  driver=d("corner turret %d's hot barrel ends: brighter while it fires" % k, ("emission", "visibility")))
    m.mesh(tips, "gatlingHeat", [cyl(2.6, 3, at=(axis[0] + 45, y + by, axis[2] + bz), axis="x", base=True, sides=8)
                                 for by, bz in bores], solid=False)
    m.part("turretMuzzles_%d" % k, parent=t, at=(axis[0] + 50, y, axis[2]))
    f = m.part("turretFlashes_%d" % k, parent=t, at=(axis[0] + 54, y, axis[2]), hidden=True,
               driver=d("corner turret %d's muzzle flash: a frame per round" % k, ("visibility",)))
    m.mesh(f, "glowYellow", sphere(8, at=(axis[0] + 54, y, axis[2]), squash=(1.6, 1, 1), steps=8), solid=False)

entry = m.write(catalog=False)

# Splice the turrets into the Citadel's entries.
doc = load_catalog()
added = [p for p in entry["parts"] if p["name"].startswith(PREFIXES)]
for model in doc["models"]:
    if model["name"] not in ("citadel_blue", "citadel_red"):
        continue
    root = model["parts"][0]["name"]
    model["parts"] = [p for p in model["parts"] if not p["name"].startswith(PREFIXES)]
    for p in added:
        q = dict(p)
        q["parent"] = root if p.get("parent") == entry["parts"][0]["name"] else p["parent"]
        q["path"] = p["path"].replace("/" + entry["name"], "/" + model["name"], 1)
        model["parts"].append(q)
    print("citadel_turrets: %d parts into %s" % (len(added), model["name"]))
save_catalog(doc)
