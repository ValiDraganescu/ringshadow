"""Cockpit of the Mule (`cockpit_mule_blue`, C4 docs/haul-and-ferry.md section 4): the driver's view standing on the
perch, the two levers and the console before it, the rail round it, the nose and the front wheels below, the MH tanks
at its shoulders and the hopper behind.

    uv run unreal/Tools/hidden_run.py --out DIR --python unreal/Tools/Editor/models/cockpit_mule.py

Contract (AcCockpitKinds.cpp, PoseMuleCockpit): rig > level > model_root > model_<unit part>, every part of the Mule
copied with its pivot and parent, so PoseMule drives them (the wheels, the steering, the levers, the heap, the gauges,
the readouts, the lamps); model_root also carries the unit root's meshes. The eye (AcKindTable: (0.45, 1.55, 0), behind
the driver's eyes over the perch, pitched 0.45 down) has nothing of the Mule within 40 cm of it, so nothing is left out.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_cockpitkit import make, copy_parts  # noqa: E402

cm, u, rig, root = make("mule", "mule", [
    "First-person view model hung off the camera while the Mule is driven: the whole Mule under the eye (model_*), "
    "posed by PoseMule; no muzzle (it has no gun)."])
copy_parts(cm, u, root, [p.name for p in u.parts if p is not u.root])
for mat, prims in u.root.meshes.items():
    cm.mesh(root, mat, prims, solid=False)
cm.write()
