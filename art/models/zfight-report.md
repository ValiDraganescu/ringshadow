# Z-fighting report

Written by `unreal/Tools/Editor/check_models.py` (run it with `hidden_run.py --python` after any model change). For each model: pairs of meshes (part/material) with triangles facing the same way in one plane (normals within 0.5 degrees, planes within 0.5 cm) that overlap where someone can see them (A = B: two solids in one mesh), and meshes that are another mesh's copy. The `_red` entries draw the `_blue` meshes and are left out. Fix by the 2 cm rule (MODELLING.md, "No shared faces").

Checked 100 models; 44 with hits (the ones still waiting for their fix are listed in `art/models/zfight-allow.txt`).

## bastion_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| bastion_blue_root/hullMid | bastion_blue_root/team | 8 | 49282 | (-57, -57, 109) |

## sentinel_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| sentinel_blue_root/hullMid | sentinel_blue_root/team | 16 | 21104 | (49, 0, 60) |
| pods_1/hullDark | pods_1/plain_110100 | 72 | 733 | (32, 38, 141) |
| pods_0/hullDark | pods_0/plain_110100 | 72 | 733 | (32, -38, 141) |
| head/hullDark | head/hullLight | 4 | 96 | (-24, 1, 88) |

## garrison_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| garrison_blue_root/bayGlow | garrison_blue_root/hullDark | 3 | 9072 | (18, 130, 52) |
| garrison_blue_root/bayGlow | garrison_blue_root/hazard | 3 | 9072 | (-18, 126, 52) |
| garrison_blue_root/hullDark | garrison_blue_root/ventGlow | 8 | 2432 | (-85, 31, 180) |
| garrison_blue_root/hazard | garrison_blue_root/hullDark | 2 | 436 | (-14, 144, 20) |

## foundry_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| foundry_blue_root/team | foundry_blue_root/team | 5 | 4324 | (89, -89, 126) |
| foundry_blue_root/hullDark | foundry_blue_root/weldGlow | 32 | 1232 | (-103, 144, 62) |
| doorLeaves_0/hullDark | doorLeaves_1/hullDark | 9 | 330 | (-0, 130, 49) |
| doorLeaves_0/hullDark | foundry_blue_root/hull | 3 | 168 | (-45, 128, 17) |
| doorLeaves_1/hullDark | foundry_blue_root/hull | 2 | 168 | (20, 128, 17) |
| foundry_blue_root/floodGlow | foundry_blue_root/hull | 4 | 112 | (-63, 140, 93) |
| foundry_blue_root/bayGlow | foundry_blue_root/hull | 6 | 18 | (-63, 52, 16) |

## spacedock_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| spacedock_blue_root/hullDark | spacedock_blue_root/hullLight | 198 | 1965 | (120, 41, 161) |
| spacedock_blue_root/hazard | spacedock_blue_root/hullDark | 64 | 1632 | (106, 102, 29) |
| padDoors_0/plain_797979 | padDoors_0/plain_797979 | 15 | 38 | (23, -46, 175) |
| padDoors_1/plain_797979 | padDoors_1/plain_797979 | 15 | 38 | (27, -14, 175) |
| spacedock_blue_root/plain_797979 | spacedock_blue_root/plain_797979 | 4 | 8 | (-89, 96, 144) |
| padDoors_0/plain_797979 | padDoors_1/plain_797979 | 12 | 5 | (25, -46, 175) |
| queueLamps_3/glow_2399ff | spacedock_blue_root/glow_2399ff | 1 | 0 | (71, 39, 168) |

## derrick_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| derrick_blue_root/hullDark | derrick_blue_root/hullDark | 8 | 479 | (-69, -44, 134) |
| derrick_blue_root/hazard | door/hullMid | 3 | 174 | (2, 149, 47) |
| valves_1/hullDark | valves_1/hullDark | 24 | 62 | (108, 128, 71) |
| valves_0/hullDark | valves_0/hullDark | 24 | 62 | (-108, 130, 71) |
| derrick_blue_root/hull | derrick_blue_root/hull | 4 | 16 | (-32, 50, 205) |

## scaffold_garrison_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_garrison_blue_root/hazard | scaffold_garrison_blue_root/hazard | 48 | 486 | (-163, 163, 74) |

## scaffold_bastion_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_bastion_blue_root/hazard | scaffold_bastion_blue_root/hazard | 48 | 486 | (-163, 163, 44) |

## scaffold_derrick_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_derrick_blue_root/hazard | scaffold_derrick_blue_root/hazard | 48 | 486 | (-163, 163, 76) |

## scaffold_foundry_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_foundry_blue_root/hazard | scaffold_foundry_blue_root/hazard | 48 | 486 | (-163, 163, 74) |

## scaffold_spacedock_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_spacedock_blue_root/hazard | scaffold_spacedock_blue_root/hazard | 48 | 486 | (-163, 163, 94) |

## scaffold_lab_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_lab_blue_root/hazard | scaffold_lab_blue_root/hazard | 48 | 486 | (-113, 113, 58) |

## scaffold_habdome_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_habdome_blue_root/hazard | scaffold_habdome_blue_root/hazard | 48 | 486 | (-113, 113, 44) |

## scaffold_sentinel_blue (construction)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| scaffold_sentinel_blue_root/hazard | scaffold_sentinel_blue_root/hazard | 48 | 486 | (-113, 113, 52) |

## lab_blue (building)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| lab_blue_root/hullLight | lab_blue_root/team | 4 | 472 | (82, -28, 33) |
| dish/chrome | dish/hullDark | 13 | 4 | (-55, -75, 112) |
| lab_blue_root/hullDark | lab_blue_root/hullDark | 1 | 0 | (46, -9, 68) |

## longbow_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| treads_1_3/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 307 | 4817 | (36, 72, 3) |
| treads_3_0/plain_080706 | treads_3_1/plain_080706 (hidden) | 307 | 4817 | (-36, 72, 48) |
| treads_0_3/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 307 | 4817 | (38, -72, 3) |
| treads_0_0/plain_080706 | treads_0_1/plain_080706 (hidden) | 307 | 4817 | (72, -72, 48) |
| treads_2_0/plain_080706 | treads_2_1/plain_080706 (hidden) | 307 | 4817 | (-38, -72, 48) |
| treads_2_3/plain_080706 (hidden) | treads_2_4/plain_080706 (hidden) | 307 | 4817 | (-72, -72, 3) |
| treads_3_3/plain_080706 (hidden) | treads_3_4/plain_080706 (hidden) | 307 | 4817 | (-72, 56, 3) |
| treads_1_0/plain_080706 | treads_1_1/plain_080706 (hidden) | 307 | 4817 | (72, 56, 48) |
| treads_0_0/plain_080706 | treads_0_5/plain_080706 (hidden) | 307 | 4817 | (62, -56, 3) |
| treads_2_0/plain_080706 | treads_2_5/plain_080706 (hidden) | 307 | 4817 | (-48, -56, 3) |
| treads_1_0/plain_080706 | treads_1_5/plain_080706 (hidden) | 307 | 4817 | (62, 72, 3) |
| treads_3_0/plain_080706 | treads_3_5/plain_080706 (hidden) | 307 | 4817 | (-48, 72, 3) |
| treads_2_2/plain_080706 (hidden) | treads_2_3/plain_080706 (hidden) | 307 | 4817 | (-62, -56, 48) |
| treads_1_2/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 307 | 4817 | (48, 72, 48) |
| treads_0_2/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 307 | 4817 | (48, -56, 48) |
| treads_3_2/plain_080706 (hidden) | treads_3_3/plain_080706 (hidden) | 307 | 4817 | (-62, 72, 48) |
| treads_1_1/plain_080706 (hidden) | treads_1_2/plain_080706 (hidden) | 305 | 4762 | (72, 56, 3) |
| treads_0_1/plain_080706 (hidden) | treads_0_2/plain_080706 (hidden) | 305 | 4762 | (72, -72, 3) |
| treads_0_4/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 305 | 4762 | (34, -56, 3) |
| treads_1_4/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 305 | 4762 | (37, 56, 3) |
| treads_2_4/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 305 | 4762 | (-73, -72, 3) |
| treads_3_4/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 305 | 4762 | (-73, 56, 3) |
| treads_3_1/plain_080706 (hidden) | treads_3_2/plain_080706 (hidden) | 305 | 4762 | (-37, 56, 48) |
| treads_2_1/plain_080706 (hidden) | treads_2_2/plain_080706 (hidden) | 305 | 4762 | (-38, -72, 3) |
| treads_0_0/plain_080706 | treads_0_2/plain_080706 (hidden) | 308 | 4110 | (72, -61, 3) |
| treads_0_3/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 308 | 4110 | (38, -61, 48) |
| treads_2_0/plain_080706 | treads_2_2/plain_080706 (hidden) | 308 | 4110 | (-38, -61, 3) |
| treads_1_0/plain_080706 | treads_1_2/plain_080706 (hidden) | 308 | 4110 | (72, 67, 3) |
| treads_1_3/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 308 | 4110 | (38, 67, 48) |
| treads_3_0/plain_080706 | treads_3_2/plain_080706 (hidden) | 308 | 4110 | (-38, 67, 3) |
| treads_2_3/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 308 | 4110 | (-72, -61, 48) |
| treads_3_3/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 308 | 4110 | (-72, 67, 48) |
| treads_0_0/plain_080706 | treads_0_4/plain_080706 (hidden) | 308 | 4110 | (41, -67, 48) |
| treads_1_0/plain_080706 | treads_1_4/plain_080706 (hidden) | 308 | 4110 | (41, 61, 48) |
| treads_2_1/plain_080706 (hidden) | treads_2_3/plain_080706 (hidden) | 308 | 4110 | (-40, -61, 3) |
| treads_3_1/plain_080706 (hidden) | treads_3_3/plain_080706 (hidden) | 308 | 4110 | (-40, 67, 3) |
| treads_2_0/plain_080706 | treads_2_4/plain_080706 (hidden) | 308 | 4110 | (-69, -67, 48) |
| treads_1_1/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 308 | 4110 | (70, 67, 3) |
| treads_3_0/plain_080706 | treads_3_4/plain_080706 (hidden) | 308 | 4110 | (-69, 61, 48) |
| treads_0_1/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 308 | 4110 | (70, -61, 3) |
| treads_3_2/plain_080706 (hidden) | treads_3_4/plain_080706 (hidden) | 302 | 3828 | (-41, 67, 3) |
| treads_0_1/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 302 | 3828 | (43, -67, 48) |
| treads_3_1/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 302 | 3828 | (-67, 61, 48) |
| treads_2_1/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 302 | 3828 | (-67, -67, 48) |
| treads_2_2/plain_080706 (hidden) | treads_2_4/plain_080706 (hidden) | 302 | 3828 | (-41, -61, 3) |
| treads_1_1/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 302 | 3828 | (43, 61, 48) |
| treads_1_2/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 302 | 3828 | (69, 67, 3) |
| treads_0_2/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 302 | 3828 | (69, -61, 3) |
| treads_3_0/plain_080706 | treads_3_3/plain_080706 (hidden) | 238 | 3662 | (-75, 61, 3) |
| treads_2_0/plain_080706 | treads_2_3/plain_080706 (hidden) | 238 | 3662 | (-75, -67, 3) |
| treads_0_0/plain_080706 | treads_0_3/plain_080706 (hidden) | 238 | 3662 | (35, -67, 3) |
| treads_1_0/plain_080706 | treads_1_3/plain_080706 (hidden) | 238 | 3662 | (74, 67, 48) |
| treads_3_1/plain_080706 (hidden) | treads_3_4/plain_080706 (hidden) | 232 | 3324 | (-41, 67, 3) |
| treads_1_1/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 232 | 3324 | (42, 61, 48) |
| treads_2_1/plain_080706 (hidden) | treads_2_4/plain_080706 (hidden) | 232 | 3324 | (-68, -67, 48) |
| treads_0_1/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 232 | 3324 | (69, -61, 3) |
| treads_2_2/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 232 | 3324 | (-73, -61, 48) |
| treads_3_2/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 232 | 3324 | (-38, 61, 3) |
| treads_1_2/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 232 | 3324 | (72, 61, 3) |
| treads_0_2/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 232 | 3324 | (72, -67, 3) |
| podBodies_0/longbow_nets_2 | podBodies_0/longbow_nets_3 | 894 | 2036 | (64, -79, 39) |
| podBodies_2/longbow_nets_2 | podBodies_2/longbow_nets_3 | 894 | 2036 | (-64, -49, 39) |
| podBodies_1/longbow_nets_2 | podBodies_1/longbow_nets_3 | 889 | 2025 | (64, 49, 39) |
| podBodies_3/longbow_nets_2 | podBodies_3/longbow_nets_3 | 889 | 2025 | (-64, 79, 39) |
| hull/longbow_nets_3 | turret/longbow_nets_2 | 977 | 1641 | (-44, 25, 84) |
| turret/longbow_nets_2 | turret/longbow_nets_3 | 341 | 640 | (-43, 19, 98) |
| hull/longbow_nets_3 | hull/longbow_nets_3 | 288 | 600 | (-54, -44, 44) |
| anchorGun/longbow_nets_4 | barrels_0/longbow_nets_5 | 133 | 423 | (60, -1, 118) |
| barrels_0/longbow_nets_4 | barrels_0/longbow_nets_5 | 87 | 367 | (56, 0, 112) |
| hull/longbow_nets_6 | hull/longbow_nets_6 | 40 | 207 | (-99, -18, 47) |
| anchorGun/longbow_nets_3 | anchorSlide/longbow_nets_3 | 20 | 106 | (-28, -3, 115) |
| anchorGun/longbow_nets_2 | anchorGun/longbow_nets_6 | 220 | 87 | (13, -9, 116) |
| claws_0/longbow_nets_3 | claws_0/longbow_nets_5 | 24 | 54 | (89, -54, 14) |
| claws_2/longbow_nets_3 | claws_2/longbow_nets_5 | 24 | 54 | (-89, -34, 14) |
| claws_1/longbow_nets_3 | claws_1/longbow_nets_5 | 24 | 54 | (89, 34, 14) |
| claws_3/longbow_nets_3 | claws_3/longbow_nets_5 | 24 | 54 | (-89, 54, 14) |
| hull/longbow_nets_3 | hull/longbow_nets_6 | 48 | 37 | (-20, -24, 81) |
| claws_3/longbow_nets_5 | podBodies_3/longbow_nets_2 | 13 | 18 | (-58, 49, 15) |
| claws_0/longbow_nets_5 | podBodies_0/longbow_nets_2 | 13 | 18 | (58, -49, 15) |
| claws_1/longbow_nets_5 | podBodies_1/longbow_nets_2 | 19 | 18 | (58, 49, 15) |
| claws_2/longbow_nets_5 | podBodies_2/longbow_nets_2 | 19 | 18 | (-58, -49, 15) |
| anchorGun/glowBlue | anchorSlide/lockGlow | 4 | 3 | (-13, 1, 124) |
| claws_2/longbow_nets_3 | podBodies_2/longbow_nets_3 | 5 | 3 | (-57, -50, 23) |
| claws_3/longbow_nets_3 | podBodies_3/longbow_nets_3 | 5 | 3 | (-57, 50, 23) |
| claws_0/longbow_nets_3 | podBodies_0/longbow_nets_3 | 5 | 3 | (57, -50, 22) |
| claws_1/longbow_nets_3 | podBodies_1/longbow_nets_3 | 5 | 3 | (57, 50, 22) |
| hull/longbow_nets_5 | hull/longbow_nets_6 | 4 | 2 | (13, -50, 65) |

## cockpit_longbow_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 307 | 4817 | (36, 72, 3) |
| model_treads_3_0/plain_080706 | model_treads_3_1/plain_080706 (hidden) | 307 | 4817 | (-36, 72, 48) |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 307 | 4817 | (38, -72, 3) |
| model_treads_0_0/plain_080706 | model_treads_0_1/plain_080706 (hidden) | 307 | 4817 | (72, -72, 48) |
| model_treads_2_0/plain_080706 | model_treads_2_1/plain_080706 (hidden) | 307 | 4817 | (-38, -72, 48) |
| model_treads_2_3/plain_080706 (hidden) | model_treads_2_4/plain_080706 (hidden) | 307 | 4817 | (-72, -72, 3) |
| model_treads_3_3/plain_080706 (hidden) | model_treads_3_4/plain_080706 (hidden) | 307 | 4817 | (-72, 56, 3) |
| model_treads_1_0/plain_080706 | model_treads_1_1/plain_080706 (hidden) | 307 | 4817 | (72, 56, 48) |
| model_treads_0_0/plain_080706 | model_treads_0_5/plain_080706 (hidden) | 307 | 4817 | (62, -56, 3) |
| model_treads_2_0/plain_080706 | model_treads_2_5/plain_080706 (hidden) | 307 | 4817 | (-48, -56, 3) |
| model_treads_1_0/plain_080706 | model_treads_1_5/plain_080706 (hidden) | 307 | 4817 | (62, 72, 3) |
| model_treads_3_0/plain_080706 | model_treads_3_5/plain_080706 (hidden) | 307 | 4817 | (-48, 72, 3) |
| model_treads_2_2/plain_080706 (hidden) | model_treads_2_3/plain_080706 (hidden) | 307 | 4817 | (-62, -56, 48) |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 307 | 4817 | (48, 72, 48) |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 307 | 4817 | (48, -56, 48) |
| model_treads_3_2/plain_080706 (hidden) | model_treads_3_3/plain_080706 (hidden) | 307 | 4817 | (-62, 72, 48) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_2/plain_080706 (hidden) | 305 | 4762 | (72, 56, 3) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_2/plain_080706 (hidden) | 305 | 4762 | (72, -72, 3) |
| model_treads_0_4/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 305 | 4762 | (34, -56, 3) |
| model_treads_1_4/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 305 | 4762 | (37, 56, 3) |
| model_treads_2_4/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 305 | 4762 | (-73, -72, 3) |
| model_treads_3_4/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 305 | 4762 | (-73, 56, 3) |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_2/plain_080706 (hidden) | 305 | 4762 | (-37, 56, 48) |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_2/plain_080706 (hidden) | 305 | 4762 | (-38, -72, 3) |
| model_treads_0_0/plain_080706 | model_treads_0_2/plain_080706 (hidden) | 308 | 4110 | (72, -61, 3) |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 308 | 4110 | (38, -61, 48) |
| model_treads_2_0/plain_080706 | model_treads_2_2/plain_080706 (hidden) | 308 | 4110 | (-38, -61, 3) |
| model_treads_1_0/plain_080706 | model_treads_1_2/plain_080706 (hidden) | 308 | 4110 | (72, 67, 3) |
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 308 | 4110 | (38, 67, 48) |
| model_treads_3_0/plain_080706 | model_treads_3_2/plain_080706 (hidden) | 308 | 4110 | (-38, 67, 3) |
| model_treads_2_3/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 308 | 4110 | (-72, -61, 48) |
| model_treads_3_3/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 308 | 4110 | (-72, 67, 48) |
| model_treads_0_0/plain_080706 | model_treads_0_4/plain_080706 (hidden) | 308 | 4110 | (41, -67, 48) |
| model_treads_1_0/plain_080706 | model_treads_1_4/plain_080706 (hidden) | 308 | 4110 | (41, 61, 48) |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_3/plain_080706 (hidden) | 308 | 4110 | (-40, -61, 3) |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_3/plain_080706 (hidden) | 308 | 4110 | (-40, 67, 3) |
| model_treads_2_0/plain_080706 | model_treads_2_4/plain_080706 (hidden) | 308 | 4110 | (-69, -67, 48) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 308 | 4110 | (70, 67, 3) |
| model_treads_3_0/plain_080706 | model_treads_3_4/plain_080706 (hidden) | 308 | 4110 | (-69, 61, 48) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 308 | 4110 | (70, -61, 3) |
| model_treads_3_2/plain_080706 (hidden) | model_treads_3_4/plain_080706 (hidden) | 302 | 3828 | (-41, 67, 3) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 302 | 3828 | (43, -67, 48) |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 302 | 3828 | (-67, 61, 48) |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 302 | 3828 | (-67, -67, 48) |
| model_treads_2_2/plain_080706 (hidden) | model_treads_2_4/plain_080706 (hidden) | 302 | 3828 | (-41, -61, 3) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 302 | 3828 | (43, 61, 48) |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 302 | 3828 | (69, 67, 3) |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 302 | 3828 | (69, -61, 3) |
| model_treads_3_0/plain_080706 | model_treads_3_3/plain_080706 (hidden) | 238 | 3662 | (-75, 61, 3) |
| model_treads_2_0/plain_080706 | model_treads_2_3/plain_080706 (hidden) | 238 | 3662 | (-75, -67, 3) |
| model_treads_0_0/plain_080706 | model_treads_0_3/plain_080706 (hidden) | 238 | 3662 | (35, -67, 3) |
| model_treads_1_0/plain_080706 | model_treads_1_3/plain_080706 (hidden) | 238 | 3662 | (74, 67, 48) |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_4/plain_080706 (hidden) | 232 | 3324 | (-41, 67, 3) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 232 | 3324 | (42, 61, 48) |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_4/plain_080706 (hidden) | 232 | 3324 | (-68, -67, 48) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 232 | 3324 | (69, -61, 3) |
| model_treads_2_2/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 232 | 3324 | (-73, -61, 48) |
| model_treads_3_2/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 232 | 3324 | (-38, 61, 3) |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 232 | 3324 | (72, 61, 3) |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 232 | 3324 | (72, -67, 3) |
| model_podBodies_0/longbow_nets_2 | model_podBodies_0/longbow_nets_3 | 894 | 2036 | (64, -49, 39) |
| model_podBodies_2/longbow_nets_2 | model_podBodies_2/longbow_nets_3 | 894 | 2036 | (-64, -79, 39) |
| model_podBodies_1/longbow_nets_2 | model_podBodies_1/longbow_nets_3 | 889 | 2025 | (64, 79, 39) |
| model_podBodies_3/longbow_nets_2 | model_podBodies_3/longbow_nets_3 | 889 | 2025 | (-64, 49, 39) |
| model_hull/longbow_nets_3 | model_turret/longbow_nets_2 (hidden) | 977 | 1641 | (-44, 25, 84) |
| model_turret/longbow_nets_2 (hidden) | model_turret/longbow_nets_3 (hidden) | 341 | 640 | (-43, 19, 98) |
| model_hull/longbow_nets_3 | model_hull/longbow_nets_3 | 288 | 600 | (-54, -44, 44) |
| model_anchorGun/longbow_nets_4 | model_barrels_0/longbow_nets_5 | 133 | 271 | (31, -60, -32) |
| model_barrels_0/longbow_nets_4 | model_barrels_0/longbow_nets_5 | 87 | 235 | (30, -57, -37) |
| model_hull/longbow_nets_6 | model_hull/longbow_nets_6 | 40 | 207 | (-99, -18, 47) |
| model_anchorGun/longbow_nets_3 | model_anchorSlide/longbow_nets_3 | 20 | 68 | (32, 10, -34) |
| model_anchorGun/longbow_nets_2 | model_anchorGun/longbow_nets_6 | 204 | 56 | (23, -23, -33) |
| model_claws_0/longbow_nets_3 | model_claws_0/longbow_nets_5 | 24 | 54 | (89, -54, 14) |
| model_claws_2/longbow_nets_3 | model_claws_2/longbow_nets_5 | 24 | 54 | (-89, -34, 14) |
| model_claws_1/longbow_nets_3 | model_claws_1/longbow_nets_5 | 24 | 54 | (89, 34, 14) |
| model_claws_3/longbow_nets_3 | model_claws_3/longbow_nets_5 | 24 | 54 | (-89, 54, 14) |
| model_hull/longbow_nets_3 | model_hull/longbow_nets_6 | 48 | 37 | (-20, -24, 81) |
| model_claws_3/longbow_nets_5 | model_podBodies_3/longbow_nets_2 | 13 | 18 | (-58, 49, 15) |
| model_claws_0/longbow_nets_5 | model_podBodies_0/longbow_nets_2 | 13 | 18 | (58, -49, 15) |
| model_claws_1/longbow_nets_5 | model_podBodies_1/longbow_nets_2 | 19 | 18 | (58, 49, 15) |
| model_claws_2/longbow_nets_5 | model_podBodies_2/longbow_nets_2 | 19 | 18 | (-58, -49, 15) |
| model_claws_2/longbow_nets_3 | model_podBodies_2/longbow_nets_3 | 5 | 3 | (-57, -50, 23) |
| model_claws_3/longbow_nets_3 | model_podBodies_3/longbow_nets_3 | 5 | 3 | (-57, 50, 23) |
| model_claws_0/longbow_nets_3 | model_podBodies_0/longbow_nets_3 | 5 | 3 | (57, -50, 22) |
| model_claws_1/longbow_nets_3 | model_podBodies_1/longbow_nets_3 | 5 | 3 | (57, 50, 22) |
| model_anchorGun/glowBlue | model_anchorSlide/model_lockGlow | 4 | 2 | (31, -2, -27) |
| model_hull/longbow_nets_5 | model_hull/longbow_nets_6 | 4 | 2 | (13, -50, 65) |

## hailstorm_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| treads_0_4/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 348 | 4132 | (-62, -33, 30) |
| treads_1_4/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 348 | 4132 | (-65, 33, 30) |
| treads_0_1/plain_080706 (hidden) | treads_0_2/plain_080706 (hidden) | 348 | 4132 | (-7, -41, -0) |
| treads_1_1/plain_080706 (hidden) | treads_1_2/plain_080706 (hidden) | 348 | 4132 | (-7, 33, -0) |
| treads_0_0/plain_080706 | treads_0_1/plain_080706 (hidden) | 348 | 4132 | (-56, -41, -0) |
| treads_1_0/plain_080706 | treads_1_1/plain_080706 (hidden) | 348 | 4132 | (-56, 33, -0) |
| treads_1_3/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 348 | 4132 | (-0, 33, -0) |
| treads_0_3/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 348 | 4132 | (-0, -41, -0) |
| treads_1_2/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 345 | 3945 | (-68, 33, 30) |
| treads_0_0/plain_080706 | treads_0_5/plain_080706 (hidden) | 345 | 3945 | (-4, -41, -0) |
| treads_1_0/plain_080706 | treads_1_5/plain_080706 (hidden) | 345 | 3945 | (-7, 41, -0) |
| treads_0_2/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 345 | 3945 | (-68, -41, 30) |
| treads_1_0/plain_080706 | treads_1_2/plain_080706 (hidden) | 350 | 3490 | (-7, 39, -0) |
| treads_0_3/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 350 | 3490 | (-2, -35, -0) |
| treads_1_3/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 350 | 3490 | (-2, 39, -0) |
| treads_0_0/plain_080706 | treads_0_2/plain_080706 (hidden) | 350 | 3490 | (-7, -35, -0) |
| treads_1_1/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 347 | 3398 | (-60, 39, -0) |
| treads_0_1/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 347 | 3398 | (-60, -35, -0) |
| treads_1_0/plain_080706 | treads_1_4/plain_080706 (hidden) | 347 | 3398 | (-15, 35, -0) |
| treads_0_0/plain_080706 | treads_0_4/plain_080706 (hidden) | 347 | 3398 | (-15, -39, -0) |
| treads_0_1/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 347 | 3398 | (-60, -39, 30) |
| treads_1_1/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 347 | 3398 | (-60, 35, 30) |
| treads_0_2/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 347 | 3398 | (-67, -35, 30) |
| treads_1_2/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 347 | 3398 | (-67, 39, 30) |
| treads_0_0/plain_080706 | treads_0_3/plain_080706 (hidden) | 262 | 2956 | (-53, -39, 30) |
| treads_1_0/plain_080706 | treads_1_3/plain_080706 (hidden) | 262 | 2956 | (-53, 35, 30) |
| treads_0_2/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 262 | 2956 | (-19, -39, 30) |
| treads_1_2/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 262 | 2956 | (-19, 35, 30) |
| treads_0_1/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 262 | 2955 | (-57, -39, -0) |
| treads_1_1/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 262 | 2955 | (-57, 35, -0) |
| wheels_0/hailstorm_nets_2 | wheels_0/plain_040302 | 118 | 1250 | (57, -46, 29) |
| wheels_1/hailstorm_nets_2 | wheels_1/plain_040302 | 118 | 1250 | (57, 28, 29) |
| hull/hailstorm_nets_1 | treads_0_4/plain_080706 (hidden) | 263 | 1232 | (-43, -31, 30) |
| hull/hailstorm_nets_1 | treads_1_4/plain_080706 (hidden) | 263 | 1232 | (-43, 43, 30) |
| hull/hailstorm_nets_1 | treads_0_1/plain_080706 (hidden) | 253 | 1232 | (-29, -43, 30) |
| hull/hailstorm_nets_1 | treads_1_1/plain_080706 (hidden) | 253 | 1232 | (-29, 31, 30) |
| hull/hailstorm_nets_1 | treads_0_0/plain_080706 | 249 | 1232 | (-72, -43, 30) |
| hull/hailstorm_nets_1 | treads_1_0/plain_080706 | 249 | 1232 | (-72, 31, 30) |
| hull/hailstorm_nets_1 | treads_0_2/plain_080706 (hidden) | 252 | 1232 | (-29, -43, 30) |
| hull/hailstorm_nets_1 | treads_1_2/plain_080706 (hidden) | 252 | 1232 | (-29, 31, 30) |
| hull/hailstorm_nets_1 | treads_0_3/plain_080706 (hidden) | 259 | 1232 | (-43, -31, 30) |
| hull/hailstorm_nets_1 | treads_1_3/plain_080706 (hidden) | 259 | 1232 | (-43, 43, 30) |
| hull/hailstorm_nets_1 | treads_0_5/plain_080706 (hidden) | 251 | 1232 | (-43, -43, 30) |
| hull/hailstorm_nets_1 | treads_1_5/plain_080706 (hidden) | 251 | 1232 | (-43, 31, 30) |
| hull/hailstorm_nets_2 | hull/hailstorm_nets_2 | 52 | 28 | (67, -26, 54) |
| hull/hailstorm_nets_3 | hull/hailstorm_nets_3 | 72 | 15 | (-22, -28, 10) |
| hull/hailstorm_nets_1 | hull/plain_ec5703 | 3 | 2 | (90, 14, 37) |

## cockpit_hailstorm_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| model_treads_1_4/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 348 | 4132 | (-65, 33, 30) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_2/plain_080706 (hidden) | 348 | 4132 | (-7, 33, -0) |
| model_treads_0_4/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 348 | 4132 | (-62, -33, 30) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_2/plain_080706 (hidden) | 348 | 4132 | (-7, -41, -0) |
| model_treads_0_0/plain_080706 | model_treads_0_1/plain_080706 (hidden) | 348 | 4132 | (-56, -41, -0) |
| model_treads_1_0/plain_080706 | model_treads_1_1/plain_080706 (hidden) | 348 | 4132 | (-56, 33, -0) |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 348 | 4132 | (-0, -41, -0) |
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 348 | 4132 | (-0, 33, -0) |
| model_treads_1_0/plain_080706 | model_treads_1_5/plain_080706 (hidden) | 345 | 3945 | (-7, 41, -0) |
| model_treads_0_0/plain_080706 | model_treads_0_5/plain_080706 (hidden) | 345 | 3945 | (-4, -41, -0) |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 345 | 3945 | (-68, 33, 30) |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 345 | 3945 | (-68, -41, 30) |
| model_treads_0_0/plain_080706 | model_treads_0_2/plain_080706 (hidden) | 350 | 3490 | (-7, -35, -0) |
| model_treads_1_0/plain_080706 | model_treads_1_2/plain_080706 (hidden) | 350 | 3490 | (-7, 39, -0) |
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 350 | 3490 | (-2, 39, -0) |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 350 | 3490 | (-2, -35, -0) |
| model_treads_1_0/plain_080706 | model_treads_1_4/plain_080706 (hidden) | 347 | 3398 | (-15, 35, -0) |
| model_treads_0_0/plain_080706 | model_treads_0_4/plain_080706 (hidden) | 347 | 3398 | (-15, -39, -0) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 347 | 3398 | (-58, -35, 30) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 347 | 3398 | (-58, 39, 30) |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 347 | 3398 | (-67, 39, 30) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 347 | 3398 | (-60, 35, 30) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 347 | 3398 | (-60, -39, 30) |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 347 | 3398 | (-67, -35, 30) |
| model_treads_1_0/plain_080706 | model_treads_1_3/plain_080706 (hidden) | 262 | 2956 | (-53, 35, 30) |
| model_treads_0_0/plain_080706 | model_treads_0_3/plain_080706 (hidden) | 262 | 2956 | (-53, -39, 30) |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 262 | 2956 | (-19, 35, 30) |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 262 | 2956 | (-19, -39, 30) |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 262 | 2955 | (-61, 35, 30) |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 262 | 2955 | (-61, -39, 30) |
| model_wheels_1/hailstorm_nets_2 | model_wheels_1/plain_040302 | 118 | 1250 | (57, 28, 29) |
| model_wheels_0/hailstorm_nets_2 | model_wheels_0/plain_040302 | 118 | 1250 | (57, -46, 29) |
| model_hull/hailstorm_nets_1 | model_treads_0_4/plain_080706 (hidden) | 263 | 1232 | (-43, -31, 30) |
| model_hull/hailstorm_nets_1 | model_treads_1_4/plain_080706 (hidden) | 263 | 1232 | (-43, 43, 30) |
| model_hull/hailstorm_nets_1 | model_treads_0_1/plain_080706 (hidden) | 253 | 1232 | (-29, -43, 30) |
| model_hull/hailstorm_nets_1 | model_treads_1_1/plain_080706 (hidden) | 253 | 1232 | (-29, 31, 30) |
| model_hull/hailstorm_nets_1 | model_treads_0_0/plain_080706 | 249 | 1232 | (-72, -43, 30) |
| model_hull/hailstorm_nets_1 | model_treads_1_0/plain_080706 | 249 | 1232 | (-72, 31, 30) |
| model_hull/hailstorm_nets_1 | model_treads_0_2/plain_080706 (hidden) | 252 | 1232 | (-29, -43, 30) |
| model_hull/hailstorm_nets_1 | model_treads_1_2/plain_080706 (hidden) | 252 | 1232 | (-29, 31, 30) |
| model_hull/hailstorm_nets_1 | model_treads_0_3/plain_080706 (hidden) | 259 | 1232 | (-43, -31, 30) |
| model_hull/hailstorm_nets_1 | model_treads_1_3/plain_080706 (hidden) | 259 | 1232 | (-43, 43, 30) |
| model_hull/hailstorm_nets_1 | model_treads_0_5/plain_080706 (hidden) | 251 | 1232 | (-43, -43, 30) |
| model_hull/hailstorm_nets_1 | model_treads_1_5/plain_080706 (hidden) | 251 | 1232 | (-43, 31, 30) |
| model_hull/hailstorm_nets_2 | model_hull/hailstorm_nets_2 | 52 | 28 | (67, 26, 54) |
| model_hull/hailstorm_nets_3 | model_hull/hailstorm_nets_3 | 72 | 15 | (-22, 28, 10) |
| model_hull/hailstorm_nets_1 | model_hull/plain_ec5703 | 3 | 2 | (90, 14, 37) |

## matriarch_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/chitin | body/chitinUnder | 72 | 13919 | (190, 12, 162) |
| abdomen/chitinMid | abdomen/chitinMid | 36 | 3893 | (-372, -129, 110) |
| jaw/chitinMid | jaw/glowDim_queen | 3 | 1786 | (433, 17, 574) |

## firefly_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| smears_3/plain_040302_2 (hidden) | wheels_3/plain_040302 | 128 | 4424 | (-65, 61, 28) |
| smears_2/plain_040302_2 (hidden) | wheels_2/plain_040302 | 128 | 4424 | (-65, -39, 28) |
| smears_0/plain_040302_2 (hidden) | wheels_0/plain_040302 | 128 | 3487 | (41, -60, 31) |
| smears_1/plain_040302_2 (hidden) | wheels_1/plain_040302 | 128 | 3487 | (41, 40, 31) |
| flameEnd/flameMaterials_4 (hidden) | flare/flameMaterials_5 (hidden) | 3 | 400 | (72, 0, 124) |
| turret/firefly_nets_3 | turret/firefly_nets_5 | 28 | 155 | (25, 1, 133) |
| body/firefly_nets_1 | body/firefly_nets_2 | 66 | 115 | (36, 0, 52) |
| tail_5/firefly_nets_6 | tail_5/firefly_nets_6 | 32 | 65 | (22, 1, 133) |
| turret/firefly_nets_2 | turret/firefly_nets_3 | 4 | 1 | (22, -0, 120) |

## cockpit_firefly_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| model_smears_3/plain_040302_2 (hidden) | model_wheels_3/plain_040302 | 128 | 4424 | (-65, 61, 28) |
| model_smears_2/plain_040302_2 (hidden) | model_wheels_2/plain_040302 | 128 | 4424 | (-65, -39, 28) |
| model_smears_0/plain_040302_2 (hidden) | model_wheels_0/plain_040302 | 128 | 3487 | (41, -60, 31) |
| model_smears_1/plain_040302_2 (hidden) | model_wheels_1/plain_040302 | 128 | 3487 | (41, 40, 31) |
| model_flameEnd/model_flameMaterials_4 (hidden) | model_flare/model_flameMaterials_5 (hidden) | 3 | 400 | (72, 0, 124) |
| model_turret/firefly_nets_3 | model_turret/firefly_nets_5 | 28 | 155 | (25, 1, 133) |
| hidden_1/firefly_nets_2 (hidden) | model_body/firefly_nets_1 | 66 | 115 | (36, 0, 52) |
| model_tail_5/firefly_nets_6 | model_tail_5/firefly_nets_6 | 32 | 65 | (22, 1, 133) |
| model_turret/firefly_nets_2 | model_turret/firefly_nets_3 | 4 | 1 | (22, -0, 120) |

## peregrine_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/hullLight | body/team | 56 | 6233 | (-20, -48, 48) |
| fins_0/hullLight | fins_0/team | 20 | 1332 | (-93, -43, 102) |
| fins_1/hullLight | fins_1/team | 20 | 1332 | (-93, 43, 102) |
| flaps_0/hullLight | flaps_0/team | 19 | 400 | (-100, -53, 46) |
| flaps_1/hullLight | flaps_1/team | 19 | 400 | (-100, 67, 46) |
| fins_0/hullDark | fins_0/team | 4 | 210 | (-95, -30, 62) |
| fins_1/hullDark | fins_1/team | 4 | 210 | (-95, 30, 62) |
| flaps_1/hullDark | flaps_1/team | 15 | 208 | (-79, 65, 44) |
| flaps_0/hullDark | flaps_0/team | 15 | 208 | (-79, -55, 44) |
| body/hullDark | body/hullDark | 60 | 193 | (-29, 64, 41) |
| body/hullMid | engines_0/hullDark | 24 | 168 | (-32, -27, 56) |
| body/hullMid | engines_1/hullDark | 24 | 168 | (-32, 25, 55) |
| body/hullMid | engines_1/hullMid | 18 | 168 | (-29, 25, 55) |
| engines_0/hullDark | engines_0/hullMid | 64 | 159 | (-55, -6, 54) |
| engines_1/hullDark | engines_1/hullMid | 64 | 159 | (-55, 38, 54) |
| body/hullMid | engines_0/hullMid | 19 | 156 | (-29, -27, 56) |
| body/hullMid | body/hullMid | 32 | 107 | (-30, -27, 45) |
| missiles_1/hullDark | missiles_1/hullLight | 154 | 82 | (-78, -53, 31) |
| missiles_2/hullDark | missiles_2/hullLight | 154 | 82 | (-78, 59, 31) |
| missiles_3/hullDark | missiles_3/hullLight | 154 | 82 | (-78, 77, 31) |
| missiles_0/hullDark | missiles_0/hullLight | 154 | 82 | (-78, -71, 31) |
| body/team | flaps_1/hullDark | 4 | 38 | (-78, 58, 44) |
| body/team | flaps_0/hullDark | 6 | 38 | (-78, -57, 44) |
| fins_1/hullDark | fins_1/hullLight | 3 | 35 | (-59, 29, 62) |
| fins_0/hullDark | fins_0/hullLight | 3 | 35 | (-59, -31, 62) |
| body/hullDark | body/team | 22 | 30 | (29, -5, 63) |
| missiles_0/hullDark | missiles_0/hullDark | 5 | 1 | (-79, -74, 33) |
| missiles_1/hullDark | missiles_1/hullDark | 5 | 1 | (-79, -56, 33) |
| missiles_3/hullDark | missiles_3/hullDark | 5 | 1 | (-79, 74, 33) |
| missiles_2/hullDark | missiles_2/hullDark | 5 | 1 | (-79, 56, 33) |
| body/hullDark | body/hullLight | 16 | 0 | (195, 0, 50) |

## dropship_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/hullDark | body/hullMid | 94 | 1511 | (-80, -14, 233) |
| body/plain_dfdfd2 | body/team | 40 | 1418 | (20, -44, 248) |
| fan/hullDark | fan/hullDark | 120 | 768 | (-14, -77, 256) |
| fan_2/hullDark | fan_2/hullDark | 120 | 768 | (-15, 77, 256) |
| pulse0/glow_bcfbbe (hidden) | pulse1/glow_bcfbbe (hidden) | 112 | 666 | (-5, 1, -1) |
| pulse0/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 | (-5, 1, -1) |
| pulse1/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 | (-5, 1, -1) |
| body/plain_9b0400 | body/plain_dfdfd2 | 30 | 353 | (22, -48, 247) |
| body/team | body/team | 8 | 312 | (21, -46, 248) |
| fan_2/chrome | fan_2/hullDark | 72 | 264 | (-15, 73, 258) |
| fan/chrome | fan/hullDark | 72 | 264 | (-15, -79, 258) |
| engines_1/hullDark | engines_1/hullDark | 24 | 201 | (-55, 69, 242) |
| engines_0/hullDark | engines_0/hullDark | 24 | 201 | (-55, -71, 241) |
| body/plain_dfdfd2 | body/plain_dfdfd2 | 16 | 142 | (20, -47, 248) |
| body/hullLight | body/plain_6c3d01 | 32 | 55 | (-61, -9, 272) |
| body/chrome | body/hullDark | 16 | 44 | (4, -35, 258) |
| ramp/hazard | ramp/hullLight | 12 | 36 | (-83, 2, 245) |
| engines_0/hullDark | engines_0/hullMid | 16 | 35 | (-56, -89, 246) |
| engines_1/hullDark | engines_1/hullMid | 16 | 35 | (-56, 89, 246) |
| shaft/glow_2cdf49 (hidden) | shaft/glow_bcfbbe (hidden) | 24 | 25 | (-0, -1, 100) |
| body/hullLight | body/hullMid | 6 | 24 | (-111, 15, 268) |
| body/plain_9b0400 | body/plain_9b0400 | 8 | 21 | (18, -50, 247) |
| pulse0/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse0/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse1/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse1/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse2/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse2/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | | |

## cockpit_peregrine_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| model_body/hullLight | model_body/team | 56 | 6233 | (-20, -48, 48) |
| model_flaps_1/hullLight | model_flaps_1/team | 19 | 400 | (-100, 67, 46) |
| model_flaps_0/hullLight | model_flaps_0/team | 19 | 400 | (-100, -53, 46) |
| model_flaps_1/hullDark | model_flaps_1/team | 15 | 208 | (-79, 65, 48) |
| model_flaps_0/hullDark | model_flaps_0/team | 15 | 208 | (-79, -55, 44) |
| model_body/hullDark | model_body/hullDark | 60 | 193 | (-29, 64, 41) |
| model_body/hullMid | model_body/hullMid | 32 | 107 | (-30, -27, 59) |
| model_missiles_1/hullDark | model_missiles_1/hullLight | 154 | 82 | (-78, -53, 31) |
| model_missiles_2/hullDark | model_missiles_2/hullLight | 154 | 82 | (-78, 59, 31) |
| model_missiles_3/hullDark | model_missiles_3/hullLight | 154 | 82 | (-78, 77, 31) |
| model_missiles_0/hullDark | model_missiles_0/hullLight | 154 | 82 | (-78, -71, 31) |
| model_body/team | model_flaps_1/hullDark | 4 | 38 | (-78, 58, 48) |
| model_body/team | model_flaps_0/hullDark | 6 | 38 | (-78, -57, 48) |
| rig/hullDark | rig/hullMid | 8 | 23 | (-23, -7, -9) |
| rig/glowBlue | rig/hullDark | 8 | 5 | (23, -9, -9) |
| rig/hullDark | rig/hullDark | 28 | 3 | (-0, -15, 11) |
| model_missiles_0/hullDark | model_missiles_0/hullDark | 5 | 1 | (-79, -74, 33) |
| model_missiles_1/hullDark | model_missiles_1/hullDark | 5 | 1 | (-79, -56, 33) |
| model_missiles_3/hullDark | model_missiles_3/hullDark | 5 | 1 | (-79, 74, 33) |
| model_missiles_2/hullDark | model_missiles_2/hullDark | 5 | 1 | (-79, 56, 33) |
| model_body/hullDark | model_body/hullLight | 16 | 0 | (195, 0, 50) |

## atlas_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| feet_0/hullMid | feet_0/hullMid | 20 | 1492 | (13, -60, -0) |
| feet_1/hullMid | feet_1/hullMid | 20 | 1492 | (13, 60, 10) |
| torso/hullDark | torso/hullDark | 68 | 1459 | (-61, -8, 224) |
| searchlight/hullDark | searchlight/hullDark | 16 | 600 | (57, 5, 194) |
| torso/hullDark | vents/ventGlow | 8 | 504 | (32, 66, 212) |
| searchlight/chrome | torso/team | 20 | 456 | (64, 2, 194) |
| knees_1/hullDark | knees_1/team | 20 | 268 | (37, 44, 90) |
| knees_0/hullDark | knees_0/team | 20 | 268 | (37, -76, 90) |
| feet_1/hullDark | feet_1/hullMid | 14 | 264 | (49, 81, 10) |
| feet_0/hullDark | feet_0/hullMid | 14 | 264 | (49, -39, 10) |
| cannons_0/chrome | cannons_0/hullDark | 16 | 89 | (151, -40, 267) |
| cannons_1/chrome | cannons_1/hullDark | 16 | 89 | (151, 44, 267) |

## gloamwing_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| wing_0/gloamWing | wing_1/gloamWing | 20 | 1236 | (-3, -46, 107) |
| wing_2/gloamWing | wing_3/gloamWing | 24 | 1236 | (-8, 46, 109) |

## cockpit_dropship_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| pulse0/glow_bcfbbe (hidden) | pulse1/glow_bcfbbe (hidden) | 112 | 666 | (1, 5, -1) |
| pulse0/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 | (1, 5, -1) |
| pulse1/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 | (1, 5, -1) |
| shaft/glow_2cdf49 (hidden) | shaft/glow_bcfbbe (hidden) | 24 | 25 | (-0, -1, 100) |
| rig/hullDark | rig/screens | 2 | 12 | (23, -39, -6) |
| rig/hull | rig/team | 2 | 0 | (30, -36, 4) |
| pulse0/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse0/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse1/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse1/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse2/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | | |
| pulse2/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | | |

## cockpit_prospector_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| fork_arm/hullDark | fork_arm/hullMid | 73 | 736 | (-42, -38, -47) |
| fork_hydrogen/chrome (hidden) | fork_hydrogen/glowCold (hidden) | 56 | 327 | (-27, -106, -27) |
| fork_hydrogen/chrome (hidden) | fork_hydrogen/hullDark (hidden) | 96 | 128 | (-24, -105, -27) |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/hullMid | 21 | 87 | (-38, -115, -32) |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/hullMid | 21 | 87 | (-18, -96, -34) |
| fork_arm/hullMid | fork_root/plain_57493c | 32 | 75 | (-38, -80, -39) |
| fork_tines_0/hullMid | fork_tines_0/plain_6c3d01 | 16 | 52 | (-37, -102, -35) |
| fork_tines_1/hullMid | fork_tines_1/plain_6c3d01 | 16 | 52 | (-18, -96, -35) |
| fork_arm/hullMid | fork_root/hullDark | 16 | 29 | (-36, -77, -28) |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/team | 16 | 28 | (-42, -103, -32) |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/team | 16 | 28 | (-18, -95, -32) |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/plain_6c3d01 | 9 | 27 | (-42, -104, -38) |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/plain_6c3d01 | 9 | 27 | (-17, -97, -37) |
| fork_hydrogen/glowCold (hidden) | fork_hydrogen/hullDark (hidden) | 28 | 9 | (-31, -106, -27) |
| fork_tines_1/plain_6c3d01 | fork_tines_1/team | 6 | 9 | (-23, -98, -32) |
| fork_tines_0/plain_6c3d01 | fork_tines_0/team | 6 | 9 | (-42, -104, -32) |
| fork_hydrogen/chrome (hidden) | fork_tines_1/plain_6c3d01 | 4 | 8 | (-22, -96, -35) |
| fork_hydrogen/chrome (hidden) | fork_tines_0/plain_6c3d01 | 4 | 8 | (-39, -101, -35) |
| fork_tines_1/hullMid | fork_tines_1/team | 4 | 6 | (-21, -104, -31) |
| fork_tines_0/hullMid | fork_tines_0/team | 4 | 6 | (-40, -110, -31) |
| fork_root/hullMid | fork_tines_0/hullDark | 3 | 4 | (-40, -95, -43) |
| fork_root/hullMid | fork_tines_1/hullDark | 3 | 3 | (-24, -90, -43) |
| fork_tines_1/hullDark | fork_tines_1/hullMid | 2 | 0 | (-16, -114, -28) |
| fork_tines_0/hullDark | fork_tines_0/hullMid | 2 | 0 | (-33, -120, -36) |

## prospector_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| fork_hydrogen/chrome (hidden) | fork_hydrogen/glowCold (hidden) | 56 | 327 | (64, -41, 76) |
| shins_0_foot_0_node/prospector_nets_2 | shins_0_foot_1_node/prospector_nets_6 | 136 | 254 | (26, -21, 6) |
| shins_1_foot_0_node/prospector_nets_2 | shins_1_foot_1_node/prospector_nets_6 | 136 | 254 | (26, 21, 6) |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/prospector_skin | 70 | 143 | (70, -52, 66) |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/prospector_skin | 70 | 143 | (65, -27, 66) |
| fork_hydrogen/chrome (hidden) | fork_hydrogen/hullDark (hidden) | 104 | 139 | (65, -45, 76) |
| legs_1/prospector_nets_3 | legs_1/prospector_nets_3 | 33 | 70 | (1, 12, 49) |
| legs_0/prospector_nets_3 | legs_0/prospector_nets_3 | 30 | 70 | (2, -12, 49) |
| fork_tines_0/prospector_skin | fork_tines_0/prospector_skin | 48 | 68 | (60, -54, 66) |
| fork_tines_1/prospector_skin | fork_tines_1/prospector_skin | 46 | 67 | (56, -34, 66) |
| legs_0/prospector_nets_3 | legs_0/prospector_nets_4 | 18 | 46 | (5, -13, 37) |
| legs_1/prospector_nets_3 | legs_1/prospector_nets_4 | 18 | 46 | (5, 32, 37) |
| body/prospector_nets_2 | body/prospector_nets_4 | 34 | 31 | (-31, 16, 78) |
| body/plain_000000 | body/prospector_nets_1 | 16 | 12 | (23, -27, 93) |
| fork_hydrogen/glowCold (hidden) | fork_hydrogen/hullDark (hidden) | 28 | 9 | (68, -43, 74) |
| fork_hydrogen/chrome (hidden) | fork_tines_0/prospector_skin | 5 | 8 | (57, -51, 68) |
| fork_hydrogen/chrome (hidden) | fork_tines_1/prospector_skin | 5 | 8 | (54, -33, 68) |
| fork_arm/prospector_skin | fork_root/prospector_skin | 4 | 2 | (34, -47, 71) |

## scorpion_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| plates_2/hullLight | plates_3/team | 19 | 311 | (-9, -10, 54) |
| plates_1/hullLight | plates_2/team | 19 | 287 | (11, -10, 54) |
| shins_2/hullDark | shins_2/hullMid | 6 | 149 | (-40, -50, 62) |
| shins_5/hullDark | shins_5/hullMid | 6 | 149 | (-40, 50, 62) |
| shins_1/hullDark | shins_1/hullMid | 6 | 149 | (-3, -50, 62) |
| shins_4/hullDark | shins_4/hullMid | 6 | 149 | (5, 50, 62) |
| shins_0/hullDark | shins_0/hullMid | 6 | 148 | (34, -50, 61) |
| shins_3/hullDark | shins_3/hullMid | 6 | 148 | (34, 50, 61) |

## cockpit_comet_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| jetHalos_0/plain_000000 (hidden) | jetHalos_2/plain_000000 (hidden) | 3 | 660 | (-15, -42, -14) |
| jetHalos_1/plain_000000 (hidden) | jetHalos_2/plain_000000 (hidden) | 3 | 660 | (15, -42, -14) |

## lodeback_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/chitin | body/chitinUnder | 36 | 888 | (70, 5, 57) |

## juggernaut_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/juggernaut_nets_1 | body/juggernaut_nets_3 | 226 | 612 | (-2, -1, 66) |
| body/juggernaut_nets_5 | body/juggernaut_nets_5 | 74 | 51 | (-42, -6, 92) |
| body/juggernaut_nets_4 | launchers_0/juggernaut_nets_3 | 18 | 31 | (5, 42, 71) |
| body/juggernaut_nets_4 | launchers_1/juggernaut_nets_3 | 20 | 31 | (8, -42, 72) |
| body/juggernaut_nets_4 | body/juggernaut_nets_4 | 18 | 29 | (4, -62, 72) |
| launchers_1/juggernaut_nets_2 | launchers_1/juggernaut_nets_3 | 7 | 6 | (20, -52, 77) |
| launchers_0/juggernaut_nets_2 | launchers_0/juggernaut_nets_3 | 7 | 6 | (20, 54, 77) |
| legs_0/juggernaut_nets_2 | legs_0/juggernaut_nets_5 | 5 | 4 | (10, -20, 50) |
| legs_1/juggernaut_nets_2 | legs_1/juggernaut_nets_5 | 5 | 4 | (10, 24, 50) |

## ranger_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| minigun_root/hullDark (hidden) | minigun_root/plain_575757 (hidden) | 64 | 231 | (36, 39, 65) |
| minigun_root/hullDark (hidden) | rifle/ranger_nets_5 | 63 | 69 | (28, 36, 71) |
| minigun_root/hull (hidden) | rifle/ranger_nets_1 | 8 | 29 | (50, 35, 76) |
| shins_0_lower_1_node/ranger_nets_2 | shins_0_lower_4_node/ranger_nets_4 | 24 | 28 | (0, -7, 11) |
| shins_1_lower_1_node/ranger_nets_2 | shins_1_lower_4_node/ranger_nets_4 | 24 | 28 | (0, 24, 11) |
| body/ranger_nets_1 | body/ranger_nets_5 | 11 | 21 | (21, -17, 78) |
| shins_0_foot_1_node/ranger_nets_2 | shins_0_foot_2_node/ranger_nets_5 | 30 | 19 | (-5, -23, 1) |
| shins_1_foot_1_node/ranger_nets_2 | shins_1_foot_2_node/ranger_nets_5 | 30 | 19 | (-5, 8, 1) |
| shins_0_foot_1_node/ranger_nets_2 | shins_0_foot_3_node/ranger_nets_5 | 29 | 19 | (5, -23, 1) |
| shins_1_foot_1_node/ranger_nets_2 | shins_1_foot_3_node/ranger_nets_5 | 29 | 19 | (5, 8, 1) |
| shins_1_foot_1_node/ranger_nets_2 | shins_1_foot_4_node/ranger_nets_5 | 38 | 19 | (14, 23, 1) |
| shins_0_foot_1_node/ranger_nets_2 | shins_0_foot_4_node/ranger_nets_5 | 38 | 19 | (14, -8, 1) |
| minigun_barrels/minigun_heat (hidden) | minigun_barrels/plain_575757 (hidden) | 122 | 16 | (96, 39, 76) |
| shins_0_foot_0_node/ranger_nets_2 | shins_0_foot_2_node/ranger_nets_5 | 15 | 13 | (-4, -19, 1) |
| shins_1_foot_0_node/ranger_nets_2 | shins_1_foot_2_node/ranger_nets_5 | 15 | 13 | (-4, 12, 1) |
| shins_1_foot_0_node/ranger_nets_2 | shins_1_foot_3_node/ranger_nets_5 | 14 | 13 | (5, 12, 1) |
| shins_0_foot_0_node/ranger_nets_2 | shins_0_foot_3_node/ranger_nets_5 | 14 | 13 | (5, -20, 1) |
| shins_0_foot_0_node/ranger_nets_2 | shins_0_foot_4_node/ranger_nets_5 | 12 | 13 | (14, -15, 1) |
| shins_1_foot_0_node/ranger_nets_2 | shins_1_foot_4_node/ranger_nets_5 | 12 | 13 | (14, 16, 1) |
| minigun_root/hullDark (hidden) | rifle/ranger_nets_4 | 4 | 9 | (15, 35, 73) |
| minigun_root/hull (hidden) | rifle/ranger_nets_5 | 5 | 8 | (42, 38, 71) |
| shins_1_lower_1_node/ranger_nets_2 | shins_1_lower_2_node/ranger_nets_4 | 5 | 8 | (7, 24, 17) |
| minigun_root/plain_575757 (hidden) | rifle/ranger_nets_1 | 3 | 6 | (19, 35, 81) |
| legs_1/ranger_nets_2 | legs_1/ranger_nets_4 | 9 | 5 | (-0, 7, 50) |
| legs_0/ranger_nets_2 | legs_0/ranger_nets_4 | 9 | 5 | (-0, -7, 50) |
| gun/ranger_nets_5 | minigun_root/hullDark (hidden) | 7 | 4 | (35, 31, 70) |
| minigun_barrels/hull (hidden) | minigun_blur/plain_565656 (hidden) | 11 | 4 | (84, 36, 72) |
| shins_0_lower_1_node/ranger_nets_2 | shins_0_lower_2_node/ranger_nets_4 | 3 | 3 | (2, -24, 21) |
| legs_1/ranger_nets_2 | legs_1/ranger_nets_3 | 6 | 3 | (2, 25, 35) |
| legs_0/ranger_nets_2 | legs_0/ranger_nets_3 | 6 | 3 | (2, -25, 35) |
| gun/ranger_nets_5 | minigun_root/plain_575757 (hidden) | 5 | 2 | (35, 31, 66) |
| minigun_barrels/glow_102eff (hidden) | minigun_barrels/hull (hidden) | 2 | 1 | (87, 36, 81) |

## cockpit_scorpion_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| model_plates_2/hullLight | model_plates_3/team | 19 | 311 | (-9, -10, 54) |
| model_plates_1/hullLight | model_plates_2/team | 19 | 287 | (11, -10, 54) |

## cockpit_juggernaut_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| launchers_1_node/juggernaut_nets_1 | launchers_1_node/juggernaut_nets_3 | 1148 | 258 | (-17, -46, -12) |
| launchers_0_node/juggernaut_nets_1 | launchers_0_node/juggernaut_nets_3 | 1145 | 258 | (17, -45, -5) |
| launchers_0_node/juggernaut_nets_2 | launchers_0_node/juggernaut_nets_3 | 5 | 1 | (20, -35, -4) |
| launchers_1_node/juggernaut_nets_2 | launchers_1_node/juggernaut_nets_3 | 5 | 1 | (-20, -35, -4) |

## kestrel_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| engines_0/kestrel_nets_2 | engines_0/kestrel_nets_3 | 135 | 190 | (-47, -40, 212) |
| engines_1/kestrel_nets_2 | engines_1/kestrel_nets_3 | 135 | 187 | (-47, 46, 216) |
| pods_1/kestrel_nets_3 | pods_1/kestrel_nets_3 | 5 | 12 | (4, 60, 203) |
| pods_0/kestrel_nets_3 | pods_0/kestrel_nets_3 | 5 | 12 | (4, -60, 203) |

## shardling_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/chitin | body/chitinUnder | 36 | 216 | (40, 10, 32) |
| body/chitinMid | body/glowDim_teal | 44 | 32 | (-27, 20, 33) |

## cockpit_ranger_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| minigun_root/hullDark (hidden) | minigun_root/plain_575757 (hidden) | 64 | 106 | (12, -39, -18) |
| minigun_root/hull_2 (hidden) | rifle/hullDark | 20 | 62 | (15, -41, -13) |
| minigun_root/hullDark (hidden) | rifle/hullDark | 153 | 36 | (15, -33, -13) |
| minigun_barrels/minigun_heat (hidden) | minigun_barrels/plain_575757 (hidden) | 110 | 7 | (16, -79, -9) |
| rifle/hullDark | rifle/hull_2 | 36 | 7 | (14, -74, -7) |
| gun/hull_2 | minigun_root/hullDark (hidden) | 5 | 6 | (12, -37, -16) |
| minigun_root/hullDark (hidden) | rifle/hull_2 | 10 | 5 | (15, -24, -12) |
| minigun_blur/plain_565656 (hidden) | rifle/hull_2 | 12 | 4 | (13, -59, -6) |
| minigun_root/plain_575757 (hidden) | rifle/hull_2 | 2 | 3 | (15, -27, -6) |
| gun/hull_2 | gun/plain_575757 | 8 | 2 | (15, -37, -12) |
| minigun_barrels/hull_2 (hidden) | minigun_blur/plain_565656 (hidden) | 11 | 2 | (14, -71, -11) |
| minigun_root/hull_2 (hidden) | rifle/ranger_rifle_2 | 2 | 2 | (13, -42, -11) |
| minigun_root/hull_2 (hidden) | rifle/ranger_rifle | 2 | 2 | (17, -42, -11) |
| minigun_barrels/glow_102eff (hidden) | minigun_barrels/hull_2 (hidden) | 2 | 0 | (14, -73, -5) |
| minigun_root/hullDark (hidden) | rifle/ranger_rifle | 2 | 0 | (17, -41, -12) |

## cockpit_atlas_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| model_cannons_0/chrome | model_cannons_0/hullDark | 16 | 89 | (151, -40, 267) |
| model_cannons_1/chrome | model_cannons_1/hullDark | 16 | 89 | (151, 44, 267) |
| rig/hullDark | rig/hullMid | 10 | 52 | (2, -11, -5) |

## comet_blue (unit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| body/comet_nets_2 | body/comet_nets_2 | 12 | 91 | (-15, -12, 44) |
| body/comet_nets_1 | body/comet_nets_2 | 68 | 37 | (2, -1, 83) |
| knees_0/comet_nets_1 | knees_0/comet_nets_3 | 7 | 12 | (9, -12, 3) |
| knees_1/comet_nets_1 | knees_1/comet_nets_3 | 7 | 12 | (9, 3, 3) |
| guns_1/comet_nets_2 | guns_1/comet_nets_3 | 12 | 8 | (-0, -20, 75) |
| guns_0/comet_nets_2 | guns_0/comet_nets_3 | 12 | 8 | (-0, 15, 75) |
| head/comet_nets_4 | head/plain_000000 | 36 | 5 | (1, 4, 88) |
| knees_0/comet_nets_2 | legs_0/comet_nets_1 | 8 | 1 | (3, -3, 27) |
| knees_1/comet_nets_2 | legs_1/comet_nets_1 | 8 | 1 | (3, 12, 27) |
| head/comet_nets_4 | head/comet_nets_4 | 8 | 1 | (14, 0, 93) |

## ore_0 (resource)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| ore_0_root/rock | stage1/oreRock | 1 | 32 | (10, 2, -11) |

## cockpit_kestrel_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 | the largest piece round (model cm) |
|---|---|---|---|---|
| rig/hull | rig/team | 5 | 26 | (-23, -35, 7) |
