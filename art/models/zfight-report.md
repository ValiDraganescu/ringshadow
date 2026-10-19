# Z-fighting report

Written by `unreal/Tools/Editor/check_models.py` (run it with `hidden_run.py --python` after any model change). For each model: pairs of meshes (part/material) with triangles facing the same way in one plane (normals within 0.5 degrees, planes within 0.5 cm) that overlap where someone can see them (A = B: two solids in one mesh), and meshes that are another mesh's copy. The `_red` entries draw the `_blue` meshes and are left out. Fix by the 2 cm rule (MODELLING.md, "No shared faces").

Checked 100 models; 0 with hits (the ones still waiting for their fix are listed in `art/models/zfight-allow.txt`).
