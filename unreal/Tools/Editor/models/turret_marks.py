"""A placed turret's marks on its base (D3, docs/mmorpg-m8.md): a small steel plate at the front and the back of the
Sentinel's and the Gatling Gun's plinth, a column of three chevrons a stat (fire rate, damage, range, and the Gatling
Gun's air reach), each chevron a dark socket that lights in the stat's colour when that mark is bought; a plate on
each of the four sides, so a pilot sees one from wherever it stands. The turret card (U) draws the same chevrons in
the same colours.

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/turret_marks.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/turretmarks/SM_<part>__<material>, then adds the parts to the `sentinel_*` and `gatling_*` catalog
entries (parented to the root; the plate always shown, the lit chevrons hidden at rest) and leaves no entry of its
own. Run again, it replaces the parts it added.

UE cm in the buildings' model frame (front +Y toward the game camera, Z up, a 2 by 2 footprint: ±100 cm). Parts:
`markPlate` (the plates and their dark sockets) and `marks_<n>`, n = stat × 3 + mark (stat 0 fire rate, 1 damage,
2 range, 3 air reach; mark 0 to 2), the four faces' chevron in one part; the pose (`PoseSentinel`, `PoseGatling`) shows
`marks_<n>` while the turret has more than `mark` marks of `stat` (`Structure::marks`).
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, prism, load_catalog, save_catalog  # noqa: E402

POSES = ["AcPoseBuildings2.cpp:PoseSentinel", "AcPoseBuildings2.cpp:PoseGatling"]
PREFIXES = ("markPlate", "marks_")
# The stats' colours, as the turret card's (SAcTurretCard.cpp StatColor): amber, ember red, ice, sky blue. N1b: strong
# and saturated (the unlit master adds the base colour to the lamp: a white base washed them pale), the base the hue
# itself so the lit sum keeps it.
COLOURS = {
    "markRate": (2.6, 0.95, 0.0),
    "markDamage": (2.8, 0.10, 0.02),
    "markRange": (0.05, 1.5, 2.4),
    "markAir": (0.12, 0.30, 3.0),
}
STATS = list(COLOURS)
FACE = 98.0      # the plates' outer face, just inside the footprint's edge
COL = 21.0       # between two stats' columns
ROWS = (18.0, 30.0, 42.0)  # under the rim (48): the top chevron ends at 46.9

m = Model("turretmarks", category="building", coplanar_check=True)
for name in ("hullDark", "hullMid"):
    m.material(name)
for name, emissive in COLOURS.items():
    m.material(name, parent="emissive", color=tuple(min(v * 0.15, 0.4) for v in emissive), emissive=emissive)


# The four faces: (normal axis, sign). A chevron is drawn in the XZ plane on a Y face; on an X face the same shape
# turned a quarter about Z (yaw 90: +X to +Y), so its arms rise toward its middle on every face.
FACES = (("y", 1), ("y", -1), ("x", 1), ("x", -1))


def at_face(axis, sign, along, out, z):
    return (out * sign, along, z) if axis == "x" else (along, out * sign, z)


# An upward chevron (^), 20 cm wide and bold (N1b: read from the usual camera): the outline of two 11 by 4.2 cm arms at
# 35 degrees, one solid (two crossing arms shared their faces at the apex: z-fighting, the coplanar check).
CHEVRON = [(7.501, -4.875), (9.910, -1.435), (0.899, 4.875), (0.0, 3.592), (-0.899, 4.875), (-9.910, -1.435),
           (-7.501, -4.875), (0.0, 0.377)]


def chevron(axis, sign, c, z, out, depth):
    """A chevron centred `c` along the face (axis, sign) at height z, `depth` thick, its middle `out` cm from the
    model's middle."""
    return [prism(CHEVRON, depth, "yz" if axis == "x" else "xz", at=at_face(axis, sign, c, out, z))]


cols = [(s - 1.5) * COL for s in range(4)]
plate = m.part("markPlate")
for axis, sign in FACES:
    w = 4 * COL + 8
    size = (4, w, 36) if axis == "x" else (w, 4, 36)
    rim = (5, w + 4, 3) if axis == "x" else (w + 4, 5, 3)
    m.mesh(plate, "hullDark", [box(size, at=at_face(axis, sign, 0, FACE - 2, 12), base=True)])
    m.mesh(plate, "hullMid", [box(rim, at=at_face(axis, sign, 0, FACE - 2, 48), base=True)])
    sockets = []
    for c in cols:
        for z in ROWS:
            sockets += chevron(axis, sign, c, z, FACE + 0.3, 1.0)  # 0.2 into the plate, its face at 98.8
    m.mesh(plate, "hullMid", sockets, solid=False)
for s, mat in enumerate(STATS):
    for k, z in enumerate(ROWS):
        n = s * 3 + k
        part = m.part("marks_%d" % n, hidden=True,
                      driver=dict(what="%s mark %d: lit once bought" % (mat[4:].lower(), k + 1), channels=["visibility"],
                                  functions=POSES))
        lit = []
        for axis, sign in FACES:
            lit += chevron(axis, sign, cols[s], z, FACE + 1.45, 1.1)  # just in front of its socket: 98.9 to 100
        m.mesh(part, mat, lit, solid=False)

entry = m.write(catalog=False)

# Splice the plates and chevrons into the turrets' entries.
doc = load_catalog()
added = [p for p in entry["parts"] if p["name"].startswith(PREFIXES)]
for model in doc["models"]:
    if not model["name"].startswith(("sentinel_", "gatling_")):
        continue
    root = model["parts"][0]["name"]
    model["parts"] = [p for p in model["parts"] if not p["name"].startswith(PREFIXES)]
    for p in added:
        q = dict(p)
        q["parent"] = root if p.get("parent") in (None, entry["parts"][0]["name"]) else p["parent"]
        q["path"] = p["path"].replace("/" + entry["name"], "/" + model["name"], 1)
        model["parts"].append(q)
    print("turret_marks: %d parts into %s" % (len(added), model["name"]))
save_catalog(doc)
