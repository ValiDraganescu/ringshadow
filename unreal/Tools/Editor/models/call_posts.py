"""The call posts (C5, docs/acting.md): a short hazard-striped post with a big lit button on its head, the act key's
target beside a Landing Pad (calls the Dropship) and a Foundry (buys a Mule). E works only what the crosshair is on,
so the call needs a thing to aim at: this post, like the Dropship's ramp post.

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/call_posts.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/callpost/SM_<part>__<material>, then adds the parts to the `landingPad_*` and `foundry_*` catalog
entries (parented to the building's root) and leaves no entry of its own. Run it after landing_pad.py (which rewrites
the pad's entry); run again, it replaces the parts it added.

UE cm in the post's own frame: its foot's middle at the origin, the button facing +Y (a building's front, toward the
game camera and the approach), Z up. The pose (`AcBuildingPose::CallPost`, from `PoseLandingPad` and `PoseFoundry`)
moves `callPost` to `ac::Act::padPostAt` / `foundryPostAt` of the building's radius each frame and keeps its size when
the building is drawn stretched; it lights `callButton` (brighter while the pilot's crosshair is on it). The core's
box for the act (`Act::padPost`) covers the head and the button, 0.75 to 1.25 up.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, load_catalog, save_catalog  # noqa: E402

POSE = "AcPoseBuildings.cpp:CallPost"
PREFIXES = ("callPost", "callButton")
HOSTS = ("landingPad_blue", "landingPad_red", "foundry_blue", "foundry_red")

m = Model("callpost", category="building", coplanar_check=True)
for name in ("hullMid", "hullDark", "hazard", "chrome"):
    m.material(name)
m.material("callGlow", parent="emissive", color=(0.1, 0.5, 0.15), emissive=(0.6, 3.2, 0.9))


def d(what, channels=("emission",)):
    return dict(what=what, channels=list(channels), functions=[POSE])


post = m.part("callPost", at=(0, 0, 0), driver=d(
    "the call post: moved to the building's post point (Act::padPostAt / foundryPostAt), its size kept", ("position", "scale")))
# the foot plate, the post, the hazard band on its front and back, the head
m.mesh(post, "hullDark", [box((32, 32, 4), at=(0, 0, 0), base=True)])
m.mesh(post, "hullMid", [box((24, 24, 100), at=(0, 0, 4), base=True)])
m.mesh(post, "hazard", [box((26, 2, 44), at=(0, s * 13, 30), base=True) for s in (-1, 1)], solid=False)
m.mesh(post, "hullDark", [box((30, 30, 18), at=(0, 0, 104), base=True)])
m.mesh(post, "chrome", [box((34, 34, 2), at=(0, 0, 122), base=True)], solid=False)  # the cap's rim, 2 cm proud
# the button's plate on the head's front face, the button standing out of it
m.mesh(post, "hullMid", [box((24, 3, 16), at=(0, 16.5, 105), base=True)], solid=False)
# (back to back: the plate 15 to 18, the bezel 18 to 20, the button 20 to 25)
m.mesh(post, "chrome", [cyl(11, 2, at=(0, 18, 113), axis="y", base=True, sides=16)], solid=False)  # its bezel
btn = m.part("callButton", parent=post, at=(0, 20, 113), driver=d(
    "the call button: lit green once the building stands, brighter while the pilot's crosshair is on it"))
m.mesh(btn, "callGlow", [cyl(9, 5, at=(0, 20, 113), axis="y", base=True, sides=16)], solid=False)

entry = m.write(catalog=False)

# Splice the post into the hosts' entries.
doc = load_catalog()
added = [p for p in entry["parts"] if p["name"].startswith(PREFIXES)]
for model in doc["models"]:
    if model["name"] not in HOSTS:
        continue
    root = model["parts"][0]["name"]
    model["parts"] = [p for p in model["parts"] if not p["name"].startswith(PREFIXES)]
    for p in added:
        q = dict(p)
        q["parent"] = root if p.get("parent") == entry["parts"][0]["name"] else p["parent"]
        q["path"] = p["path"].replace("/" + entry["name"], "/" + model["name"], 1)
        model["parts"].append(q)
    print("call_posts: %d parts into %s" % (len(added), model["name"]))
save_catalog(doc)
