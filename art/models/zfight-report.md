# Z-fighting report

Written by `unreal/Tools/Editor/check_models.py` (run it with `hidden_run.py --python` after any model change). For each model: pairs of meshes (part/material) with triangles facing the same way in one plane (normals within 0.5 degrees, planes within 0.5 cm) that overlap where someone can see them (A = B: two solids in one mesh), and meshes that are another mesh's copy. The `_red` entries draw the `_blue` meshes and are left out. Fix by the 2 cm rule (MODELLING.md, "No shared faces").

Checked 99 models; 53 with hits (the ones still waiting for their fix are listed in `art/models/zfight-allow.txt`).

## bastion_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| bastion_blue_root/hullMid | bastion_blue_root/team | 8 | 49282 |

## sentinel_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| sentinel_blue_root/hullMid | sentinel_blue_root/team | 16 | 21104 |
| pods_1/hullDark | pods_1/plain_110100 | 72 | 733 |
| pods_0/hullDark | pods_0/plain_110100 | 72 | 733 |
| markPlate/hullMid | markPlate/hullMid | 128 | 250 |
| markPlate/hullMid | marks_2/markRate (hidden) | 96 | 170 |
| markPlate/hullMid | marks_1/markRate (hidden) | 96 | 170 |
| markPlate/hullMid | marks_11/markAir (hidden) | 96 | 170 |
| markPlate/hullMid | marks_10/markAir (hidden) | 96 | 170 |
| markPlate/hullMid | marks_0/markRate (hidden) | 96 | 170 |
| markPlate/hullMid | marks_9/markAir (hidden) | 96 | 170 |
| markPlate/hullMid | marks_5/markDamage (hidden) | 96 | 170 |
| markPlate/hullMid | marks_8/markRange (hidden) | 96 | 170 |
| markPlate/hullMid | marks_7/markRange (hidden) | 96 | 170 |
| markPlate/hullMid | marks_4/markDamage (hidden) | 96 | 170 |
| markPlate/hullMid | marks_3/markDamage (hidden) | 96 | 170 |
| markPlate/hullMid | marks_6/markRange (hidden) | 96 | 170 |
| head/hullDark | head/hullLight | 4 | 96 |
| marks_2/markRate (hidden) | marks_2/markRate (hidden) | 8 | 19 |
| marks_0/markRate (hidden) | marks_0/markRate (hidden) | 8 | 19 |
| marks_1/markRate (hidden) | marks_1/markRate (hidden) | 8 | 19 |
| marks_9/markAir (hidden) | marks_9/markAir (hidden) | 8 | 19 |
| marks_11/markAir (hidden) | marks_11/markAir (hidden) | 8 | 19 |
| marks_10/markAir (hidden) | marks_10/markAir (hidden) | 8 | 19 |
| marks_8/markRange (hidden) | marks_8/markRange (hidden) | 8 | 19 |
| marks_3/markDamage (hidden) | marks_3/markDamage (hidden) | 8 | 19 |
| marks_7/markRange (hidden) | marks_7/markRange (hidden) | 8 | 19 |
| marks_6/markRange (hidden) | marks_6/markRange (hidden) | 8 | 19 |
| marks_4/markDamage (hidden) | marks_4/markDamage (hidden) | 8 | 19 |
| marks_5/markDamage (hidden) | marks_5/markDamage (hidden) | 8 | 19 |

## garrison_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| garrison_blue_root/bayGlow | garrison_blue_root/hullDark | 3 | 9072 |
| garrison_blue_root/bayGlow | garrison_blue_root/hazard | 3 | 9072 |
| garrison_blue_root/hullDark | garrison_blue_root/ventGlow | 8 | 2432 |
| garrison_blue_root/hazard | garrison_blue_root/hullDark | 2 | 436 |

## gatling_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| head/hullDark | hopper/gatlingMolten | 4 | 6084 |
| head/hullLight | head/hullLight | 52 | 2112 |
| head/hullLight | hopper/gatlingFurnace | 4 | 434 |
| barrels/gunmetal | tips/gatlingHeat | 180 | 408 |
| markPlate/hullMid | markPlate/hullMid | 128 | 250 |
| markPlate/hullMid | marks_11/markAir (hidden) | 96 | 170 |
| markPlate/hullMid | marks_1/markRate (hidden) | 96 | 170 |
| markPlate/hullMid | marks_2/markRate (hidden) | 96 | 170 |
| markPlate/hullMid | marks_10/markAir (hidden) | 96 | 170 |
| markPlate/hullMid | marks_0/markRate (hidden) | 96 | 170 |
| markPlate/hullMid | marks_9/markAir (hidden) | 96 | 170 |
| markPlate/hullMid | marks_8/markRange (hidden) | 96 | 170 |
| markPlate/hullMid | marks_7/markRange (hidden) | 96 | 170 |
| markPlate/hullMid | marks_6/markRange (hidden) | 96 | 170 |
| markPlate/hullMid | marks_5/markDamage (hidden) | 96 | 170 |
| markPlate/hullMid | marks_4/markDamage (hidden) | 96 | 170 |
| markPlate/hullMid | marks_3/markDamage (hidden) | 96 | 170 |
| gatling_blue_root/gatlingFurnace | gatling_blue_root/gatlingFurnace | 8 | 115 |
| gatling_blue_root/gatlingFurnace | gatling_blue_root/hullLight | 16 | 64 |
| marks_2/markRate (hidden) | marks_2/markRate (hidden) | 8 | 19 |
| marks_0/markRate (hidden) | marks_0/markRate (hidden) | 8 | 19 |
| marks_1/markRate (hidden) | marks_1/markRate (hidden) | 8 | 19 |
| marks_9/markAir (hidden) | marks_9/markAir (hidden) | 8 | 19 |
| marks_11/markAir (hidden) | marks_11/markAir (hidden) | 8 | 19 |
| marks_10/markAir (hidden) | marks_10/markAir (hidden) | 8 | 19 |
| marks_8/markRange (hidden) | marks_8/markRange (hidden) | 8 | 19 |
| marks_3/markDamage (hidden) | marks_3/markDamage (hidden) | 8 | 19 |
| marks_7/markRange (hidden) | marks_7/markRange (hidden) | 8 | 19 |
| marks_6/markRange (hidden) | marks_6/markRange (hidden) | 8 | 19 |
| marks_4/markDamage (hidden) | marks_4/markDamage (hidden) | 8 | 19 |
| marks_5/markDamage (hidden) | marks_5/markDamage (hidden) | 8 | 19 |

## foundry_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| foundry_blue_root/team | foundry_blue_root/team | 5 | 4324 |
| foundry_blue_root/hullDark | foundry_blue_root/weldGlow | 32 | 1232 |
| doorLeaves_0/hullDark | doorLeaves_1/hullDark | 9 | 330 |
| doorLeaves_0/hullDark | foundry_blue_root/hull | 3 | 168 |
| doorLeaves_1/hullDark | foundry_blue_root/hull | 2 | 168 |
| foundry_blue_root/floodGlow | foundry_blue_root/hull | 4 | 112 |
| foundry_blue_root/bayGlow | foundry_blue_root/hull | 6 | 18 |

## spacedock_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| spacedock_blue_root/hullDark | spacedock_blue_root/hullLight | 198 | 1965 |
| spacedock_blue_root/hazard | spacedock_blue_root/hullDark | 64 | 1632 |
| padDoors_0/plain_797979 | padDoors_0/plain_797979 | 15 | 38 |
| padDoors_1/plain_797979 | padDoors_1/plain_797979 | 15 | 38 |
| spacedock_blue_root/plain_797979 | spacedock_blue_root/plain_797979 | 4 | 8 |
| padDoors_0/plain_797979 | padDoors_1/plain_797979 | 12 | 5 |
| queueLamps_3/glow_2399ff | spacedock_blue_root/glow_2399ff | 1 | 0 |

## landingPad_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| landingPad_blue_root/chrome | landingPad_blue_root/team | 36 | 502 |
| landingPad_blue_root/hazard | landingPad_blue_root/hullDark | 48 | 240 |
| landingPad_blue_root/team | landingPad_blue_root/team | 52 | 227 |
| landingPad_blue_root/hullDark | landingPad_blue_root/hullDark | 32 | 32 |

## derrick_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| derrick_blue_root/hullDark | derrick_blue_root/hullDark | 8 | 479 |
| derrick_blue_root/hazard | door/hullMid | 3 | 174 |
| valves_1/hullDark | valves_1/hullDark | 24 | 62 |
| valves_0/hullDark | valves_0/hullDark | 24 | 62 |
| derrick_blue_root/hull | derrick_blue_root/hull | 4 | 16 |

## scaffold_garrison_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_garrison_blue_root/hazard | scaffold_garrison_blue_root/hazard | 48 | 486 |

## scaffold_bastion_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_bastion_blue_root/hazard | scaffold_bastion_blue_root/hazard | 48 | 486 |

## scaffold_derrick_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_derrick_blue_root/hazard | scaffold_derrick_blue_root/hazard | 48 | 486 |

## scaffold_foundry_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_foundry_blue_root/hazard | scaffold_foundry_blue_root/hazard | 48 | 486 |

## scaffold_spacedock_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_spacedock_blue_root/hazard | scaffold_spacedock_blue_root/hazard | 48 | 486 |

## scaffold_lab_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_lab_blue_root/hazard | scaffold_lab_blue_root/hazard | 48 | 486 |

## scaffold_habdome_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_habdome_blue_root/hazard | scaffold_habdome_blue_root/hazard | 48 | 486 |

## scaffold_sentinel_blue (construction)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| scaffold_sentinel_blue_root/hazard | scaffold_sentinel_blue_root/hazard | 48 | 486 |

## lab_blue (building)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| lab_blue_root/hullLight | lab_blue_root/team | 4 | 472 |
| dish/chrome | dish/hullDark | 13 | 4 |
| lab_blue_root/hullDark | lab_blue_root/hullDark | 1 | 0 |

## longbow_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| treads_1_3/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 307 | 4817 |
| treads_3_0/plain_080706 | treads_3_1/plain_080706 (hidden) | 307 | 4817 |
| treads_0_3/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 307 | 4817 |
| treads_0_0/plain_080706 | treads_0_1/plain_080706 (hidden) | 307 | 4817 |
| treads_2_0/plain_080706 | treads_2_1/plain_080706 (hidden) | 307 | 4817 |
| treads_2_3/plain_080706 (hidden) | treads_2_4/plain_080706 (hidden) | 307 | 4817 |
| treads_3_3/plain_080706 (hidden) | treads_3_4/plain_080706 (hidden) | 307 | 4817 |
| treads_1_0/plain_080706 | treads_1_1/plain_080706 (hidden) | 307 | 4817 |
| treads_0_0/plain_080706 | treads_0_5/plain_080706 (hidden) | 307 | 4817 |
| treads_2_0/plain_080706 | treads_2_5/plain_080706 (hidden) | 307 | 4817 |
| treads_1_0/plain_080706 | treads_1_5/plain_080706 (hidden) | 307 | 4817 |
| treads_3_0/plain_080706 | treads_3_5/plain_080706 (hidden) | 307 | 4817 |
| treads_2_2/plain_080706 (hidden) | treads_2_3/plain_080706 (hidden) | 307 | 4817 |
| treads_1_2/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 307 | 4817 |
| treads_0_2/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 307 | 4817 |
| treads_3_2/plain_080706 (hidden) | treads_3_3/plain_080706 (hidden) | 307 | 4817 |
| treads_1_1/plain_080706 (hidden) | treads_1_2/plain_080706 (hidden) | 305 | 4762 |
| treads_0_1/plain_080706 (hidden) | treads_0_2/plain_080706 (hidden) | 305 | 4762 |
| treads_0_4/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 305 | 4762 |
| treads_1_4/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 305 | 4762 |
| treads_2_4/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 305 | 4762 |
| treads_3_4/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 305 | 4762 |
| treads_3_1/plain_080706 (hidden) | treads_3_2/plain_080706 (hidden) | 305 | 4762 |
| treads_2_1/plain_080706 (hidden) | treads_2_2/plain_080706 (hidden) | 305 | 4762 |
| treads_0_0/plain_080706 | treads_0_2/plain_080706 (hidden) | 308 | 4110 |
| treads_0_3/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 308 | 4110 |
| treads_2_0/plain_080706 | treads_2_2/plain_080706 (hidden) | 308 | 4110 |
| treads_1_0/plain_080706 | treads_1_2/plain_080706 (hidden) | 308 | 4110 |
| treads_1_3/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 308 | 4110 |
| treads_3_0/plain_080706 | treads_3_2/plain_080706 (hidden) | 308 | 4110 |
| treads_2_3/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 308 | 4110 |
| treads_3_3/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 308 | 4110 |
| treads_0_0/plain_080706 | treads_0_4/plain_080706 (hidden) | 308 | 4110 |
| treads_1_0/plain_080706 | treads_1_4/plain_080706 (hidden) | 308 | 4110 |
| treads_2_1/plain_080706 (hidden) | treads_2_3/plain_080706 (hidden) | 308 | 4110 |
| treads_3_1/plain_080706 (hidden) | treads_3_3/plain_080706 (hidden) | 308 | 4110 |
| treads_2_0/plain_080706 | treads_2_4/plain_080706 (hidden) | 308 | 4110 |
| treads_1_1/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 308 | 4110 |
| treads_3_0/plain_080706 | treads_3_4/plain_080706 (hidden) | 308 | 4110 |
| treads_0_1/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 308 | 4110 |
| treads_3_2/plain_080706 (hidden) | treads_3_4/plain_080706 (hidden) | 302 | 3828 |
| treads_0_1/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 302 | 3828 |
| treads_3_1/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 302 | 3828 |
| treads_2_1/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 302 | 3828 |
| treads_2_2/plain_080706 (hidden) | treads_2_4/plain_080706 (hidden) | 302 | 3828 |
| treads_1_1/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 302 | 3828 |
| treads_1_2/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 302 | 3828 |
| treads_0_2/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 302 | 3828 |
| treads_3_0/plain_080706 | treads_3_3/plain_080706 (hidden) | 238 | 3662 |
| treads_2_0/plain_080706 | treads_2_3/plain_080706 (hidden) | 238 | 3662 |
| treads_0_0/plain_080706 | treads_0_3/plain_080706 (hidden) | 238 | 3662 |
| treads_1_0/plain_080706 | treads_1_3/plain_080706 (hidden) | 238 | 3662 |
| treads_3_1/plain_080706 (hidden) | treads_3_4/plain_080706 (hidden) | 232 | 3324 |
| treads_1_1/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 232 | 3324 |
| treads_2_1/plain_080706 (hidden) | treads_2_4/plain_080706 (hidden) | 232 | 3324 |
| treads_0_1/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 232 | 3324 |
| treads_2_2/plain_080706 (hidden) | treads_2_5/plain_080706 (hidden) | 232 | 3324 |
| treads_3_2/plain_080706 (hidden) | treads_3_5/plain_080706 (hidden) | 232 | 3324 |
| treads_1_2/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 232 | 3324 |
| treads_0_2/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 232 | 3324 |
| podBodies_0/longbow_nets_2 | podBodies_0/longbow_nets_3 | 894 | 2036 |
| podBodies_2/longbow_nets_2 | podBodies_2/longbow_nets_3 | 894 | 2036 |
| podBodies_1/longbow_nets_2 | podBodies_1/longbow_nets_3 | 889 | 2025 |
| podBodies_3/longbow_nets_2 | podBodies_3/longbow_nets_3 | 889 | 2025 |
| hull/longbow_nets_3 | turret/longbow_nets_2 | 977 | 1641 |
| turret/longbow_nets_2 | turret/longbow_nets_3 | 341 | 640 |
| hull/longbow_nets_3 | hull/longbow_nets_3 | 288 | 600 |
| anchorGun/longbow_nets_4 | barrels_0/longbow_nets_5 | 133 | 423 |
| barrels_0/longbow_nets_4 | barrels_0/longbow_nets_5 | 87 | 367 |
| hull/longbow_nets_6 | hull/longbow_nets_6 | 40 | 207 |
| anchorGun/longbow_nets_3 | anchorSlide/longbow_nets_3 | 20 | 106 |
| anchorGun/longbow_nets_2 | anchorGun/longbow_nets_6 | 220 | 87 |
| claws_0/longbow_nets_3 | claws_0/longbow_nets_5 | 24 | 54 |
| claws_2/longbow_nets_3 | claws_2/longbow_nets_5 | 24 | 54 |
| claws_1/longbow_nets_3 | claws_1/longbow_nets_5 | 24 | 54 |
| claws_3/longbow_nets_3 | claws_3/longbow_nets_5 | 24 | 54 |
| hull/longbow_nets_3 | hull/longbow_nets_6 | 48 | 37 |
| claws_3/longbow_nets_5 | podBodies_3/longbow_nets_2 | 13 | 18 |
| claws_0/longbow_nets_5 | podBodies_0/longbow_nets_2 | 13 | 18 |
| claws_1/longbow_nets_5 | podBodies_1/longbow_nets_2 | 19 | 18 |
| claws_2/longbow_nets_5 | podBodies_2/longbow_nets_2 | 19 | 18 |
| anchorGun/glowBlue | anchorSlide/lockGlow | 4 | 3 |
| claws_2/longbow_nets_3 | podBodies_2/longbow_nets_3 | 5 | 3 |
| claws_3/longbow_nets_3 | podBodies_3/longbow_nets_3 | 5 | 3 |
| claws_0/longbow_nets_3 | podBodies_0/longbow_nets_3 | 5 | 3 |
| claws_1/longbow_nets_3 | podBodies_1/longbow_nets_3 | 5 | 3 |
| hull/longbow_nets_5 | hull/longbow_nets_6 | 4 | 2 |

## cockpit_longbow_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 307 | 4817 |
| model_treads_3_0/plain_080706 | model_treads_3_1/plain_080706 (hidden) | 307 | 4817 |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 307 | 4817 |
| model_treads_0_0/plain_080706 | model_treads_0_1/plain_080706 (hidden) | 307 | 4817 |
| model_treads_2_0/plain_080706 | model_treads_2_1/plain_080706 (hidden) | 307 | 4817 |
| model_treads_2_3/plain_080706 (hidden) | model_treads_2_4/plain_080706 (hidden) | 307 | 4817 |
| model_treads_3_3/plain_080706 (hidden) | model_treads_3_4/plain_080706 (hidden) | 307 | 4817 |
| model_treads_1_0/plain_080706 | model_treads_1_1/plain_080706 (hidden) | 307 | 4817 |
| model_treads_0_0/plain_080706 | model_treads_0_5/plain_080706 (hidden) | 307 | 4817 |
| model_treads_2_0/plain_080706 | model_treads_2_5/plain_080706 (hidden) | 307 | 4817 |
| model_treads_1_0/plain_080706 | model_treads_1_5/plain_080706 (hidden) | 307 | 4817 |
| model_treads_3_0/plain_080706 | model_treads_3_5/plain_080706 (hidden) | 307 | 4817 |
| model_treads_2_2/plain_080706 (hidden) | model_treads_2_3/plain_080706 (hidden) | 307 | 4817 |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 307 | 4817 |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 307 | 4817 |
| model_treads_3_2/plain_080706 (hidden) | model_treads_3_3/plain_080706 (hidden) | 307 | 4817 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_2/plain_080706 (hidden) | 305 | 4762 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_2/plain_080706 (hidden) | 305 | 4762 |
| model_treads_0_4/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 305 | 4762 |
| model_treads_1_4/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 305 | 4762 |
| model_treads_2_4/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 305 | 4762 |
| model_treads_3_4/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 305 | 4762 |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_2/plain_080706 (hidden) | 305 | 4762 |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_2/plain_080706 (hidden) | 305 | 4762 |
| model_treads_0_0/plain_080706 | model_treads_0_2/plain_080706 (hidden) | 308 | 4110 |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 308 | 4110 |
| model_treads_2_0/plain_080706 | model_treads_2_2/plain_080706 (hidden) | 308 | 4110 |
| model_treads_1_0/plain_080706 | model_treads_1_2/plain_080706 (hidden) | 308 | 4110 |
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 308 | 4110 |
| model_treads_3_0/plain_080706 | model_treads_3_2/plain_080706 (hidden) | 308 | 4110 |
| model_treads_2_3/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 308 | 4110 |
| model_treads_3_3/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 308 | 4110 |
| model_treads_0_0/plain_080706 | model_treads_0_4/plain_080706 (hidden) | 308 | 4110 |
| model_treads_1_0/plain_080706 | model_treads_1_4/plain_080706 (hidden) | 308 | 4110 |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_3/plain_080706 (hidden) | 308 | 4110 |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_3/plain_080706 (hidden) | 308 | 4110 |
| model_treads_2_0/plain_080706 | model_treads_2_4/plain_080706 (hidden) | 308 | 4110 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 308 | 4110 |
| model_treads_3_0/plain_080706 | model_treads_3_4/plain_080706 (hidden) | 308 | 4110 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 308 | 4110 |
| model_treads_3_2/plain_080706 (hidden) | model_treads_3_4/plain_080706 (hidden) | 302 | 3828 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 302 | 3828 |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 302 | 3828 |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 302 | 3828 |
| model_treads_2_2/plain_080706 (hidden) | model_treads_2_4/plain_080706 (hidden) | 302 | 3828 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 302 | 3828 |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 302 | 3828 |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 302 | 3828 |
| model_treads_3_0/plain_080706 | model_treads_3_3/plain_080706 (hidden) | 238 | 3662 |
| model_treads_2_0/plain_080706 | model_treads_2_3/plain_080706 (hidden) | 238 | 3662 |
| model_treads_0_0/plain_080706 | model_treads_0_3/plain_080706 (hidden) | 238 | 3662 |
| model_treads_1_0/plain_080706 | model_treads_1_3/plain_080706 (hidden) | 238 | 3662 |
| model_treads_3_1/plain_080706 (hidden) | model_treads_3_4/plain_080706 (hidden) | 232 | 3324 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 232 | 3324 |
| model_treads_2_1/plain_080706 (hidden) | model_treads_2_4/plain_080706 (hidden) | 232 | 3324 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 232 | 3324 |
| model_treads_2_2/plain_080706 (hidden) | model_treads_2_5/plain_080706 (hidden) | 232 | 3324 |
| model_treads_3_2/plain_080706 (hidden) | model_treads_3_5/plain_080706 (hidden) | 232 | 3324 |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 232 | 3324 |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 232 | 3324 |
| model_podBodies_0/longbow_nets_2 | model_podBodies_0/longbow_nets_3 | 894 | 2036 |
| model_podBodies_2/longbow_nets_2 | model_podBodies_2/longbow_nets_3 | 894 | 2036 |
| model_podBodies_1/longbow_nets_2 | model_podBodies_1/longbow_nets_3 | 889 | 2025 |
| model_podBodies_3/longbow_nets_2 | model_podBodies_3/longbow_nets_3 | 889 | 2025 |
| model_hull/longbow_nets_3 | model_turret/longbow_nets_2 (hidden) | 977 | 1641 |
| model_turret/longbow_nets_2 (hidden) | model_turret/longbow_nets_3 (hidden) | 341 | 640 |
| model_hull/longbow_nets_3 | model_hull/longbow_nets_3 | 288 | 600 |
| model_anchorGun/longbow_nets_4 | model_barrels_0/longbow_nets_5 | 133 | 271 |
| model_barrels_0/longbow_nets_4 | model_barrels_0/longbow_nets_5 | 87 | 235 |
| model_hull/longbow_nets_6 | model_hull/longbow_nets_6 | 40 | 207 |
| model_anchorGun/longbow_nets_3 | model_anchorSlide/longbow_nets_3 | 20 | 68 |
| model_anchorGun/longbow_nets_2 | model_anchorGun/longbow_nets_6 | 204 | 56 |
| model_claws_0/longbow_nets_3 | model_claws_0/longbow_nets_5 | 24 | 54 |
| model_claws_2/longbow_nets_3 | model_claws_2/longbow_nets_5 | 24 | 54 |
| model_claws_1/longbow_nets_3 | model_claws_1/longbow_nets_5 | 24 | 54 |
| model_claws_3/longbow_nets_3 | model_claws_3/longbow_nets_5 | 24 | 54 |
| model_hull/longbow_nets_3 | model_hull/longbow_nets_6 | 48 | 37 |
| model_claws_3/longbow_nets_5 | model_podBodies_3/longbow_nets_2 | 13 | 18 |
| model_claws_0/longbow_nets_5 | model_podBodies_0/longbow_nets_2 | 13 | 18 |
| model_claws_1/longbow_nets_5 | model_podBodies_1/longbow_nets_2 | 19 | 18 |
| model_claws_2/longbow_nets_5 | model_podBodies_2/longbow_nets_2 | 19 | 18 |
| model_claws_2/longbow_nets_3 | model_podBodies_2/longbow_nets_3 | 5 | 3 |
| model_claws_3/longbow_nets_3 | model_podBodies_3/longbow_nets_3 | 5 | 3 |
| model_claws_0/longbow_nets_3 | model_podBodies_0/longbow_nets_3 | 5 | 3 |
| model_claws_1/longbow_nets_3 | model_podBodies_1/longbow_nets_3 | 5 | 3 |
| model_anchorGun/glowBlue | model_anchorSlide/model_lockGlow | 4 | 2 |
| model_hull/longbow_nets_5 | model_hull/longbow_nets_6 | 4 | 2 |

## hailstorm_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| treads_0_4/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 348 | 4132 |
| treads_1_4/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 348 | 4132 |
| treads_0_1/plain_080706 (hidden) | treads_0_2/plain_080706 (hidden) | 348 | 4132 |
| treads_1_1/plain_080706 (hidden) | treads_1_2/plain_080706 (hidden) | 348 | 4132 |
| treads_0_0/plain_080706 | treads_0_1/plain_080706 (hidden) | 348 | 4132 |
| treads_1_0/plain_080706 | treads_1_1/plain_080706 (hidden) | 348 | 4132 |
| treads_1_3/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 348 | 4132 |
| treads_0_3/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 348 | 4132 |
| treads_1_2/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 345 | 3945 |
| treads_0_0/plain_080706 | treads_0_5/plain_080706 (hidden) | 345 | 3945 |
| treads_1_0/plain_080706 | treads_1_5/plain_080706 (hidden) | 345 | 3945 |
| treads_0_2/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 345 | 3945 |
| treads_1_0/plain_080706 | treads_1_2/plain_080706 (hidden) | 350 | 3490 |
| treads_0_3/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 350 | 3490 |
| treads_1_3/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 350 | 3490 |
| treads_0_0/plain_080706 | treads_0_2/plain_080706 (hidden) | 350 | 3490 |
| treads_1_1/plain_080706 (hidden) | treads_1_3/plain_080706 (hidden) | 347 | 3398 |
| treads_0_1/plain_080706 (hidden) | treads_0_3/plain_080706 (hidden) | 347 | 3398 |
| treads_1_0/plain_080706 | treads_1_4/plain_080706 (hidden) | 347 | 3398 |
| treads_0_0/plain_080706 | treads_0_4/plain_080706 (hidden) | 347 | 3398 |
| treads_0_1/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 347 | 3398 |
| treads_1_1/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 347 | 3398 |
| treads_0_2/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 347 | 3398 |
| treads_1_2/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 347 | 3398 |
| treads_0_0/plain_080706 | treads_0_3/plain_080706 (hidden) | 262 | 2956 |
| treads_1_0/plain_080706 | treads_1_3/plain_080706 (hidden) | 262 | 2956 |
| treads_0_2/plain_080706 (hidden) | treads_0_5/plain_080706 (hidden) | 262 | 2956 |
| treads_1_2/plain_080706 (hidden) | treads_1_5/plain_080706 (hidden) | 262 | 2956 |
| treads_0_1/plain_080706 (hidden) | treads_0_4/plain_080706 (hidden) | 262 | 2955 |
| treads_1_1/plain_080706 (hidden) | treads_1_4/plain_080706 (hidden) | 262 | 2955 |
| wheels_0/hailstorm_nets_2 | wheels_0/plain_040302 | 118 | 1250 |
| wheels_1/hailstorm_nets_2 | wheels_1/plain_040302 | 118 | 1250 |
| hull/hailstorm_nets_1 | treads_0_4/plain_080706 (hidden) | 263 | 1232 |
| hull/hailstorm_nets_1 | treads_1_4/plain_080706 (hidden) | 263 | 1232 |
| hull/hailstorm_nets_1 | treads_0_1/plain_080706 (hidden) | 253 | 1232 |
| hull/hailstorm_nets_1 | treads_1_1/plain_080706 (hidden) | 253 | 1232 |
| hull/hailstorm_nets_1 | treads_0_0/plain_080706 | 249 | 1232 |
| hull/hailstorm_nets_1 | treads_1_0/plain_080706 | 249 | 1232 |
| hull/hailstorm_nets_1 | treads_0_2/plain_080706 (hidden) | 252 | 1232 |
| hull/hailstorm_nets_1 | treads_1_2/plain_080706 (hidden) | 252 | 1232 |
| hull/hailstorm_nets_1 | treads_0_3/plain_080706 (hidden) | 259 | 1232 |
| hull/hailstorm_nets_1 | treads_1_3/plain_080706 (hidden) | 259 | 1232 |
| hull/hailstorm_nets_1 | treads_0_5/plain_080706 (hidden) | 251 | 1232 |
| hull/hailstorm_nets_1 | treads_1_5/plain_080706 (hidden) | 251 | 1232 |
| hull/hailstorm_nets_2 | hull/hailstorm_nets_2 | 52 | 28 |
| hull/hailstorm_nets_3 | hull/hailstorm_nets_3 | 72 | 15 |
| hull/hailstorm_nets_1 | hull/plain_ec5703 | 3 | 2 |

## cockpit_hailstorm_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| model_treads_1_4/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 348 | 4132 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_2/plain_080706 (hidden) | 348 | 4132 |
| model_treads_0_4/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 348 | 4132 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_2/plain_080706 (hidden) | 348 | 4132 |
| model_treads_0_0/plain_080706 | model_treads_0_1/plain_080706 (hidden) | 348 | 4132 |
| model_treads_1_0/plain_080706 | model_treads_1_1/plain_080706 (hidden) | 348 | 4132 |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 348 | 4132 |
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 348 | 4132 |
| model_treads_1_0/plain_080706 | model_treads_1_5/plain_080706 (hidden) | 345 | 3945 |
| model_treads_0_0/plain_080706 | model_treads_0_5/plain_080706 (hidden) | 345 | 3945 |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 345 | 3945 |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 345 | 3945 |
| model_treads_0_0/plain_080706 | model_treads_0_2/plain_080706 (hidden) | 350 | 3490 |
| model_treads_1_0/plain_080706 | model_treads_1_2/plain_080706 (hidden) | 350 | 3490 |
| model_treads_1_3/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 350 | 3490 |
| model_treads_0_3/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 350 | 3490 |
| model_treads_1_0/plain_080706 | model_treads_1_4/plain_080706 (hidden) | 347 | 3398 |
| model_treads_0_0/plain_080706 | model_treads_0_4/plain_080706 (hidden) | 347 | 3398 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_3/plain_080706 (hidden) | 347 | 3398 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_3/plain_080706 (hidden) | 347 | 3398 |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 347 | 3398 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 347 | 3398 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 347 | 3398 |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 347 | 3398 |
| model_treads_1_0/plain_080706 | model_treads_1_3/plain_080706 (hidden) | 262 | 2956 |
| model_treads_0_0/plain_080706 | model_treads_0_3/plain_080706 (hidden) | 262 | 2956 |
| model_treads_1_2/plain_080706 (hidden) | model_treads_1_5/plain_080706 (hidden) | 262 | 2956 |
| model_treads_0_2/plain_080706 (hidden) | model_treads_0_5/plain_080706 (hidden) | 262 | 2956 |
| model_treads_1_1/plain_080706 (hidden) | model_treads_1_4/plain_080706 (hidden) | 262 | 2955 |
| model_treads_0_1/plain_080706 (hidden) | model_treads_0_4/plain_080706 (hidden) | 262 | 2955 |
| model_wheels_1/hailstorm_nets_2 | model_wheels_1/plain_040302 | 118 | 1250 |
| model_wheels_0/hailstorm_nets_2 | model_wheels_0/plain_040302 | 118 | 1250 |
| model_hull/hailstorm_nets_1 | model_treads_0_4/plain_080706 (hidden) | 263 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_1_4/plain_080706 (hidden) | 263 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_0_1/plain_080706 (hidden) | 253 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_1_1/plain_080706 (hidden) | 253 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_0_0/plain_080706 | 249 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_1_0/plain_080706 | 249 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_0_2/plain_080706 (hidden) | 252 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_1_2/plain_080706 (hidden) | 252 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_0_3/plain_080706 (hidden) | 259 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_1_3/plain_080706 (hidden) | 259 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_0_5/plain_080706 (hidden) | 251 | 1232 |
| model_hull/hailstorm_nets_1 | model_treads_1_5/plain_080706 (hidden) | 251 | 1232 |
| model_hull/hailstorm_nets_2 | model_hull/hailstorm_nets_2 | 52 | 28 |
| model_hull/hailstorm_nets_3 | model_hull/hailstorm_nets_3 | 72 | 15 |
| model_hull/hailstorm_nets_1 | model_hull/plain_ec5703 | 3 | 2 |

## site_vault_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/vaultMetal | body/vaultMetal | 44 | 15040 |
| body/soot | door_open/vaultMetal | 6 | 4389 |
| body/vaultMetal | seam/siteSeam | 12 | 2034 |
| body/hullDark | door_open/vaultMetal | 6 | 1856 |
| body/hullDark | body/hullDark | 10 | 1144 |
| body/hullDark | body/vaultMetal | 12 | 320 |
| door_open/vaultMetal | door_shut/vaultMetal | 5 | 112 |
| door_open/vaultMetal | seam/siteSeam | 6 | 18 |
| seam/siteSeam | seam/siteSeam | 16 | 12 |

## site_crate_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| lid_open/hazard | lid_open/hullLight | 8 | 19027 |
| body/hullDark | body/hullDark | 106 | 2204 |
| lid_shut/hazard | lid_shut/hullLight | 16 | 1632 |
| lid_shut/hullDark | lid_shut/hullDark | 4 | 64 |

## matriarch_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/chitin | body/chitinUnder | 72 | 13919 |
| abdomen/chitinMid | abdomen/chitinMid | 36 | 3893 |
| jaw/chitinMid | jaw/glowDim_queen | 3 | 1786 |

## firefly_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| smears_3/plain_040302_2 (hidden) | wheels_3/plain_040302 | 128 | 4424 |
| smears_2/plain_040302_2 (hidden) | wheels_2/plain_040302 | 128 | 4424 |
| smears_0/plain_040302_2 (hidden) | wheels_0/plain_040302 | 128 | 3487 |
| smears_1/plain_040302_2 (hidden) | wheels_1/plain_040302 | 128 | 3487 |
| flameEnd/flameMaterials_4 (hidden) | flare/flameMaterials_5 (hidden) | 3 | 400 |
| turret/firefly_nets_3 | turret/firefly_nets_5 | 28 | 155 |
| body/firefly_nets_1 | body/firefly_nets_2 | 66 | 115 |
| tail_5/firefly_nets_6 | tail_5/firefly_nets_6 | 32 | 65 |
| turret/firefly_nets_2 | turret/firefly_nets_3 | 4 | 1 |

## cockpit_firefly_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| model_smears_3/plain_040302_2 (hidden) | model_wheels_3/plain_040302 | 128 | 4424 |
| model_smears_2/plain_040302_2 (hidden) | model_wheels_2/plain_040302 | 128 | 4424 |
| model_smears_0/plain_040302_2 (hidden) | model_wheels_0/plain_040302 | 128 | 3487 |
| model_smears_1/plain_040302_2 (hidden) | model_wheels_1/plain_040302 | 128 | 3487 |
| model_flameEnd/model_flameMaterials_4 (hidden) | model_flare/model_flameMaterials_5 (hidden) | 3 | 400 |
| model_turret/firefly_nets_3 | model_turret/firefly_nets_5 | 28 | 155 |
| hidden_1/firefly_nets_2 (hidden) | model_body/firefly_nets_1 | 66 | 115 |
| model_tail_5/firefly_nets_6 | model_tail_5/firefly_nets_6 | 32 | 65 |
| model_turret/firefly_nets_2 | model_turret/firefly_nets_3 | 4 | 1 |

## peregrine_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/hullLight | body/team | 56 | 6233 |
| fins_0/hullLight | fins_0/team | 20 | 1332 |
| fins_1/hullLight | fins_1/team | 20 | 1332 |
| flaps_0/hullLight | flaps_0/team | 19 | 400 |
| flaps_1/hullLight | flaps_1/team | 19 | 400 |
| fins_0/hullDark | fins_0/team | 4 | 210 |
| fins_1/hullDark | fins_1/team | 4 | 210 |
| flaps_1/hullDark | flaps_1/team | 15 | 208 |
| flaps_0/hullDark | flaps_0/team | 15 | 208 |
| body/hullDark | body/hullDark | 60 | 193 |
| body/hullMid | engines_0/hullDark | 24 | 168 |
| body/hullMid | engines_1/hullDark | 24 | 168 |
| body/hullMid | engines_1/hullMid | 18 | 168 |
| engines_0/hullDark | engines_0/hullMid | 64 | 159 |
| engines_1/hullDark | engines_1/hullMid | 64 | 159 |
| body/hullMid | engines_0/hullMid | 19 | 156 |
| body/hullMid | body/hullMid | 32 | 107 |
| missiles_1/hullDark | missiles_1/hullLight | 154 | 82 |
| missiles_2/hullDark | missiles_2/hullLight | 154 | 82 |
| missiles_3/hullDark | missiles_3/hullLight | 154 | 82 |
| missiles_0/hullDark | missiles_0/hullLight | 154 | 82 |
| body/team | flaps_1/hullDark | 4 | 38 |
| body/team | flaps_0/hullDark | 6 | 38 |
| fins_1/hullDark | fins_1/hullLight | 3 | 35 |
| fins_0/hullDark | fins_0/hullLight | 3 | 35 |
| body/hullDark | body/team | 22 | 30 |
| missiles_0/hullDark | missiles_0/hullDark | 5 | 1 |
| missiles_1/hullDark | missiles_1/hullDark | 5 | 1 |
| missiles_3/hullDark | missiles_3/hullDark | 5 | 1 |
| missiles_2/hullDark | missiles_2/hullDark | 5 | 1 |
| body/hullDark | body/hullLight | 16 | 0 |

## site_wreck_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/hullDark | body/soot | 20 | 8678 |
| hatch_open/soot | hatch_shut/hullLight | 4 | 2592 |
| hatch_shut/hazard | hatch_shut/hullLight | 6 | 360 |
| hatch_open/hazard | hatch_open/hullLight | 3 | 180 |
| hatch_open/hullLight | hatch_shut/hullLight | 2 | 8 |

## dropship_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/hullDark | body/hullMid | 94 | 1511 |
| body/plain_dfdfd2 | body/team | 40 | 1418 |
| fan/hullDark | fan/hullDark | 120 | 768 |
| fan_2/hullDark | fan_2/hullDark | 120 | 768 |
| pulse0/glow_bcfbbe (hidden) | pulse1/glow_bcfbbe (hidden) | 112 | 666 |
| pulse0/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 |
| pulse1/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 |
| body/plain_9b0400 | body/plain_dfdfd2 | 30 | 353 |
| body/team | body/team | 8 | 312 |
| fan_2/chrome | fan_2/hullDark | 72 | 264 |
| fan/chrome | fan/hullDark | 72 | 264 |
| engines_1/hullDark | engines_1/hullDark | 24 | 201 |
| engines_0/hullDark | engines_0/hullDark | 24 | 201 |
| body/plain_dfdfd2 | body/plain_dfdfd2 | 16 | 142 |
| body/hullLight | body/plain_6c3d01 | 32 | 55 |
| body/chrome | body/hullDark | 16 | 44 |
| ramp/hazard | ramp/hullLight | 12 | 36 |
| engines_0/hullDark | engines_0/hullMid | 16 | 35 |
| engines_1/hullDark | engines_1/hullMid | 16 | 35 |
| shaft/glow_2cdf49 (hidden) | shaft/glow_bcfbbe (hidden) | 24 | 25 |
| body/hullLight | body/hullMid | 6 | 24 |
| body/plain_9b0400 | body/plain_9b0400 | 8 | 21 |
| pulse0/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse0/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse1/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse1/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse2/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse2/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | |

## cockpit_peregrine_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| model_body/hullLight | model_body/team | 56 | 6233 |
| model_flaps_1/hullLight | model_flaps_1/team | 19 | 400 |
| model_flaps_0/hullLight | model_flaps_0/team | 19 | 400 |
| model_flaps_1/hullDark | model_flaps_1/team | 15 | 208 |
| model_flaps_0/hullDark | model_flaps_0/team | 15 | 208 |
| model_body/hullDark | model_body/hullDark | 60 | 193 |
| model_body/hullMid | model_body/hullMid | 32 | 107 |
| model_missiles_1/hullDark | model_missiles_1/hullLight | 154 | 82 |
| model_missiles_2/hullDark | model_missiles_2/hullLight | 154 | 82 |
| model_missiles_3/hullDark | model_missiles_3/hullLight | 154 | 82 |
| model_missiles_0/hullDark | model_missiles_0/hullLight | 154 | 82 |
| model_body/team | model_flaps_1/hullDark | 4 | 38 |
| model_body/team | model_flaps_0/hullDark | 6 | 38 |
| rig/hullDark | rig/hullMid | 8 | 23 |
| rig/glowBlue | rig/hullDark | 8 | 5 |
| rig/hullDark | rig/hullDark | 28 | 3 |
| model_missiles_0/hullDark | model_missiles_0/hullDark | 5 | 1 |
| model_missiles_1/hullDark | model_missiles_1/hullDark | 5 | 1 |
| model_missiles_3/hullDark | model_missiles_3/hullDark | 5 | 1 |
| model_missiles_2/hullDark | model_missiles_2/hullDark | 5 | 1 |
| model_body/hullDark | model_body/hullLight | 16 | 0 |

## atlas_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| feet_0/hullMid | feet_0/hullMid | 20 | 1492 |
| feet_1/hullMid | feet_1/hullMid | 20 | 1492 |
| torso/hullDark | torso/hullDark | 68 | 1459 |
| searchlight/hullDark | searchlight/hullDark | 16 | 600 |
| torso/hullDark | vents/ventGlow | 8 | 504 |
| searchlight/chrome | torso/team | 20 | 456 |
| knees_1/hullDark | knees_1/team | 20 | 268 |
| knees_0/hullDark | knees_0/team | 20 | 268 |
| feet_1/hullDark | feet_1/hullMid | 14 | 264 |
| feet_0/hullDark | feet_0/hullMid | 14 | 264 |
| cannons_0/chrome | cannons_0/hullDark | 16 | 89 |
| cannons_1/chrome | cannons_1/hullDark | 16 | 89 |

## doodad_arch_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/sandstone | body/sandstoneRed | 10 | 3640 |

## gloamwing_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| wing_0/gloamWing | wing_1/gloamWing | 20 | 1236 |
| wing_2/gloamWing | wing_3/gloamWing | 24 | 1236 |

## doodad_sunkHauler_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/haulerYellow | body/rustSteel | 8 | 1800 |
| body/haulerYellow | body/haulerYellow | 23 | 458 |

## cockpit_dropship_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| pulse0/glow_bcfbbe (hidden) | pulse1/glow_bcfbbe (hidden) | 112 | 666 |
| pulse0/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 |
| pulse1/glow_bcfbbe (hidden) | pulse2/glow_bcfbbe (hidden) | 112 | 666 |
| shaft/glow_2cdf49 (hidden) | shaft/glow_bcfbbe (hidden) | 24 | 25 |
| rig/hullDark | rig/screens | 2 | 12 |
| rig/hull | rig/team | 2 | 0 |
| pulse0/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse0/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse1/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse1/glow_bcfbbe | pulse2/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse2/glow_bcfbbe | pulse0/glow_bcfbbe | duplicate: every triangle of A lies on B | |
| pulse2/glow_bcfbbe | pulse1/glow_bcfbbe | duplicate: every triangle of A lies on B | |

## cockpit_prospector_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| fork_arm/hullDark | fork_arm/hullMid | 73 | 736 |
| fork_hydrogen/chrome (hidden) | fork_hydrogen/glowCold (hidden) | 56 | 327 |
| fork_hydrogen/chrome (hidden) | fork_hydrogen/hullDark (hidden) | 96 | 128 |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/hullMid | 21 | 87 |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/hullMid | 21 | 87 |
| fork_arm/hullMid | fork_root/plain_57493c | 32 | 75 |
| fork_tines_0/hullMid | fork_tines_0/plain_6c3d01 | 16 | 52 |
| fork_tines_1/hullMid | fork_tines_1/plain_6c3d01 | 16 | 52 |
| fork_arm/hullMid | fork_root/hullDark | 16 | 29 |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/team | 16 | 28 |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/team | 16 | 28 |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/plain_6c3d01 | 9 | 27 |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/plain_6c3d01 | 9 | 27 |
| fork_hydrogen/glowCold (hidden) | fork_hydrogen/hullDark (hidden) | 28 | 9 |
| fork_tines_1/plain_6c3d01 | fork_tines_1/team | 6 | 9 |
| fork_tines_0/plain_6c3d01 | fork_tines_0/team | 6 | 9 |
| fork_hydrogen/chrome (hidden) | fork_tines_1/plain_6c3d01 | 4 | 8 |
| fork_hydrogen/chrome (hidden) | fork_tines_0/plain_6c3d01 | 4 | 8 |
| fork_tines_1/hullMid | fork_tines_1/team | 4 | 6 |
| fork_tines_0/hullMid | fork_tines_0/team | 4 | 6 |
| fork_root/hullMid | fork_tines_0/hullDark | 3 | 4 |
| fork_root/hullMid | fork_tines_1/hullDark | 3 | 3 |
| fork_tines_1/hullDark | fork_tines_1/hullMid | 2 | 0 |
| fork_tines_0/hullDark | fork_tines_0/hullMid | 2 | 0 |

## prospector_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| fork_hydrogen/chrome (hidden) | fork_hydrogen/glowCold (hidden) | 56 | 327 |
| shins_0_foot_0_node/prospector_nets_2 | shins_0_foot_1_node/prospector_nets_6 | 136 | 254 |
| shins_1_foot_0_node/prospector_nets_2 | shins_1_foot_1_node/prospector_nets_6 | 136 | 254 |
| fork_hydrogen/hullMid (hidden) | fork_tines_0/prospector_skin | 70 | 143 |
| fork_hydrogen/hullMid (hidden) | fork_tines_1/prospector_skin | 70 | 143 |
| fork_hydrogen/chrome (hidden) | fork_hydrogen/hullDark (hidden) | 104 | 139 |
| legs_1/prospector_nets_3 | legs_1/prospector_nets_3 | 33 | 70 |
| legs_0/prospector_nets_3 | legs_0/prospector_nets_3 | 30 | 70 |
| fork_tines_0/prospector_skin | fork_tines_0/prospector_skin | 48 | 68 |
| fork_tines_1/prospector_skin | fork_tines_1/prospector_skin | 46 | 67 |
| legs_0/prospector_nets_3 | legs_0/prospector_nets_4 | 18 | 46 |
| legs_1/prospector_nets_3 | legs_1/prospector_nets_4 | 18 | 46 |
| body/prospector_nets_2 | body/prospector_nets_4 | 34 | 31 |
| body/plain_000000 | body/prospector_nets_1 | 16 | 12 |
| fork_hydrogen/glowCold (hidden) | fork_hydrogen/hullDark (hidden) | 28 | 9 |
| fork_hydrogen/chrome (hidden) | fork_tines_0/prospector_skin | 5 | 8 |
| fork_hydrogen/chrome (hidden) | fork_tines_1/prospector_skin | 5 | 8 |
| fork_arm/prospector_skin | fork_root/prospector_skin | 4 | 2 |

## doodad_derrickRuin_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/rustSteel | body/rustSteel | 180 | 1518 |

## scorpion_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| plates_2/hullLight | plates_3/team | 19 | 311 |
| plates_1/hullLight | plates_2/team | 19 | 287 |
| shins_2/hullDark | shins_2/hullMid | 6 | 149 |
| shins_5/hullDark | shins_5/hullMid | 6 | 149 |
| shins_1/hullDark | shins_1/hullMid | 6 | 149 |
| shins_4/hullDark | shins_4/hullMid | 6 | 149 |
| shins_0/hullDark | shins_0/hullMid | 6 | 148 |
| shins_3/hullDark | shins_3/hullMid | 6 | 148 |

## cockpit_comet_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| jetHalos_0/plain_000000 (hidden) | jetHalos_2/plain_000000 (hidden) | 3 | 660 |
| jetHalos_1/plain_000000 (hidden) | jetHalos_2/plain_000000 (hidden) | 3 | 660 |

## lodeback_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/chitin | body/chitinUnder | 36 | 888 |

## juggernaut_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/juggernaut_nets_1 | body/juggernaut_nets_3 | 226 | 612 |
| body/juggernaut_nets_5 | body/juggernaut_nets_5 | 74 | 51 |
| body/juggernaut_nets_4 | launchers_0/juggernaut_nets_3 | 18 | 31 |
| body/juggernaut_nets_4 | launchers_1/juggernaut_nets_3 | 20 | 31 |
| body/juggernaut_nets_4 | body/juggernaut_nets_4 | 18 | 29 |
| launchers_1/juggernaut_nets_2 | launchers_1/juggernaut_nets_3 | 7 | 6 |
| launchers_0/juggernaut_nets_2 | launchers_0/juggernaut_nets_3 | 7 | 6 |
| legs_0/juggernaut_nets_2 | legs_0/juggernaut_nets_5 | 5 | 4 |
| legs_1/juggernaut_nets_2 | legs_1/juggernaut_nets_5 | 5 | 4 |

## ranger_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| minigun_root/hullDark (hidden) | minigun_root/plain_575757 (hidden) | 64 | 231 |
| minigun_root/hullDark (hidden) | rifle/ranger_nets_5 | 63 | 69 |
| minigun_root/hull (hidden) | rifle/ranger_nets_1 | 8 | 29 |
| shins_0_lower_1_node/ranger_nets_2 | shins_0_lower_4_node/ranger_nets_4 | 24 | 28 |
| shins_1_lower_1_node/ranger_nets_2 | shins_1_lower_4_node/ranger_nets_4 | 24 | 28 |
| body/ranger_nets_1 | body/ranger_nets_5 | 11 | 21 |
| shins_0_foot_1_node/ranger_nets_2 | shins_0_foot_2_node/ranger_nets_5 | 30 | 19 |
| shins_1_foot_1_node/ranger_nets_2 | shins_1_foot_2_node/ranger_nets_5 | 30 | 19 |
| shins_0_foot_1_node/ranger_nets_2 | shins_0_foot_3_node/ranger_nets_5 | 29 | 19 |
| shins_1_foot_1_node/ranger_nets_2 | shins_1_foot_3_node/ranger_nets_5 | 29 | 19 |
| shins_1_foot_1_node/ranger_nets_2 | shins_1_foot_4_node/ranger_nets_5 | 38 | 19 |
| shins_0_foot_1_node/ranger_nets_2 | shins_0_foot_4_node/ranger_nets_5 | 38 | 19 |
| minigun_barrels/minigun_heat (hidden) | minigun_barrels/plain_575757 (hidden) | 122 | 16 |
| shins_0_foot_0_node/ranger_nets_2 | shins_0_foot_2_node/ranger_nets_5 | 15 | 13 |
| shins_1_foot_0_node/ranger_nets_2 | shins_1_foot_2_node/ranger_nets_5 | 15 | 13 |
| shins_1_foot_0_node/ranger_nets_2 | shins_1_foot_3_node/ranger_nets_5 | 14 | 13 |
| shins_0_foot_0_node/ranger_nets_2 | shins_0_foot_3_node/ranger_nets_5 | 14 | 13 |
| shins_0_foot_0_node/ranger_nets_2 | shins_0_foot_4_node/ranger_nets_5 | 12 | 13 |
| shins_1_foot_0_node/ranger_nets_2 | shins_1_foot_4_node/ranger_nets_5 | 12 | 13 |
| minigun_root/hullDark (hidden) | rifle/ranger_nets_4 | 4 | 9 |
| minigun_root/hull (hidden) | rifle/ranger_nets_5 | 5 | 8 |
| shins_1_lower_1_node/ranger_nets_2 | shins_1_lower_2_node/ranger_nets_4 | 5 | 8 |
| minigun_root/plain_575757 (hidden) | rifle/ranger_nets_1 | 3 | 6 |
| legs_1/ranger_nets_2 | legs_1/ranger_nets_4 | 9 | 5 |
| legs_0/ranger_nets_2 | legs_0/ranger_nets_4 | 9 | 5 |
| gun/ranger_nets_5 | minigun_root/hullDark (hidden) | 7 | 4 |
| minigun_barrels/hull (hidden) | minigun_blur/plain_565656 (hidden) | 11 | 4 |
| shins_0_lower_1_node/ranger_nets_2 | shins_0_lower_2_node/ranger_nets_4 | 3 | 3 |
| legs_1/ranger_nets_2 | legs_1/ranger_nets_3 | 6 | 3 |
| legs_0/ranger_nets_2 | legs_0/ranger_nets_3 | 6 | 3 |
| gun/ranger_nets_5 | minigun_root/plain_575757 (hidden) | 5 | 2 |
| minigun_barrels/glow_102eff (hidden) | minigun_barrels/hull (hidden) | 2 | 1 |

## cockpit_scorpion_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| model_plates_2/hullLight | model_plates_3/team | 19 | 311 |
| model_plates_1/hullLight | model_plates_2/team | 19 | 287 |

## cockpit_juggernaut_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| launchers_1_node/juggernaut_nets_1 | launchers_1_node/juggernaut_nets_3 | 1148 | 258 |
| launchers_0_node/juggernaut_nets_1 | launchers_0_node/juggernaut_nets_3 | 1145 | 258 |
| launchers_0_node/juggernaut_nets_2 | launchers_0_node/juggernaut_nets_3 | 5 | 1 |
| launchers_1_node/juggernaut_nets_2 | launchers_1_node/juggernaut_nets_3 | 5 | 1 |

## kestrel_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| engines_0/kestrel_nets_2 | engines_0/kestrel_nets_3 | 135 | 190 |
| engines_1/kestrel_nets_2 | engines_1/kestrel_nets_3 | 135 | 187 |
| pods_1/kestrel_nets_3 | pods_1/kestrel_nets_3 | 5 | 12 |
| pods_0/kestrel_nets_3 | pods_0/kestrel_nets_3 | 5 | 12 |

## doodad_buildSpot_blue (doodad)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/paintAmber | body/paintAmber | 48 | 252 |

## shardling_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/chitin | body/chitinUnder | 36 | 216 |
| body/chitinMid | body/glowDim_teal | 44 | 32 |

## cockpit_ranger_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| minigun_root/hullDark (hidden) | minigun_root/plain_575757 (hidden) | 64 | 106 |
| minigun_root/hull_2 (hidden) | rifle/hullDark | 20 | 62 |
| minigun_root/hullDark (hidden) | rifle/hullDark | 153 | 36 |
| minigun_barrels/minigun_heat (hidden) | minigun_barrels/plain_575757 (hidden) | 110 | 7 |
| rifle/hullDark | rifle/hull_2 | 36 | 7 |
| gun/hull_2 | minigun_root/hullDark (hidden) | 5 | 6 |
| minigun_root/hullDark (hidden) | rifle/hull_2 | 10 | 5 |
| minigun_blur/plain_565656 (hidden) | rifle/hull_2 | 12 | 4 |
| minigun_root/plain_575757 (hidden) | rifle/hull_2 | 2 | 3 |
| gun/hull_2 | gun/plain_575757 | 8 | 2 |
| minigun_barrels/hull_2 (hidden) | minigun_blur/plain_565656 (hidden) | 11 | 2 |
| minigun_root/hull_2 (hidden) | rifle/ranger_rifle_2 | 2 | 2 |
| minigun_root/hull_2 (hidden) | rifle/ranger_rifle | 2 | 2 |
| minigun_barrels/glow_102eff (hidden) | minigun_barrels/hull_2 (hidden) | 2 | 0 |
| minigun_root/hullDark (hidden) | rifle/ranger_rifle | 2 | 0 |

## cockpit_atlas_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| model_cannons_0/chrome | model_cannons_0/hullDark | 16 | 89 |
| model_cannons_1/chrome | model_cannons_1/hullDark | 16 | 89 |
| rig/hullDark | rig/hullMid | 10 | 52 |

## comet_blue (unit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| body/comet_nets_2 | body/comet_nets_2 | 12 | 91 |
| body/comet_nets_1 | body/comet_nets_2 | 68 | 37 |
| knees_0/comet_nets_1 | knees_0/comet_nets_3 | 7 | 12 |
| knees_1/comet_nets_1 | knees_1/comet_nets_3 | 7 | 12 |
| guns_1/comet_nets_2 | guns_1/comet_nets_3 | 12 | 8 |
| guns_0/comet_nets_2 | guns_0/comet_nets_3 | 12 | 8 |
| head/comet_nets_4 | head/plain_000000 | 36 | 5 |
| knees_0/comet_nets_2 | legs_0/comet_nets_1 | 8 | 1 |
| knees_1/comet_nets_2 | legs_1/comet_nets_1 | 8 | 1 |
| head/comet_nets_4 | head/comet_nets_4 | 8 | 1 |

## ore_0 (resource)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| ore_0_root/rock | stage1/oreRock | 1 | 32 |

## cockpit_kestrel_blue (cockpit)

| part/material A | part/material B | triangles | area cm2 |
|---|---|---|---|
| rig/hull | rig/team | 5 | 26 |
