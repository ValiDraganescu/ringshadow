"""M_TerrainRock: the ground's rock kit material (docs/terrain-lab.md, rounds 3 and 4, promoted).

Every mesh of the kit (terrain_rocks.py: slabs, talus, spire tops, the
clutter) is drawn with it, instanced by AAcTerrainScenery. It samples the
ground's texture arrays (make_terrain_material.py) in world space on
three planes, so a slab's face carries the same rock grain as the ground's
cliffs beside it, and recolours that grain to the instance's colour:

  per-instance custom data 0-2: the colour (linear albedo, the ground's own
  at that point), 3: the arrays' slice (3 the rock; clutter its palette's).

Its occlusion darkens the mesh's underside and the grain's cracks so a
stone sits in the ground. Needs the arrays: run make_terrain_material.py
first.

  uv run unreal/Tools/hidden_run.py --out unreal/Saved/Terrain --python unreal/Tools/Editor/make_terrain_rock_material.py
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Content-src", "terrain")
LAB_DIR = "/Game/Terrain"
MAT_NAME = "M_TerrainRock"
LAYERS = ["Dirt", "Grass", "Highland", "Rock", "Salt", "Ash", "Glass", "Slag", "Frost"]

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

HLSL = r"""
// World space, metres; the instance's colour and slice.
float3 p = WP / 100.0;
float3 n = normalize(VN);
float slice = Inst.w;
float3 w = pow(abs(n), 4.0);
w /= (w.x + w.y + w.z);
float tile = slice > 2.5 && slice < 3.5 ? RockTile : ClutterTile;
float4 cx = LabCH.Sample(LabCHSampler, float3(p.yz / tile, slice));
float4 cy = LabCH.Sample(LabCHSampler, float3(p.xz / tile, slice));
float4 cz = LabCH.Sample(LabCHSampler, float3(p.xy / tile, slice));
float4 nx = LabNR.Sample(LabNRSampler, float3(p.yz / tile, slice));
float4 ny = LabNR.Sample(LabNRSampler, float3(p.xz / tile, slice));
float4 nz = LabNR.Sample(LabNRSampler, float3(p.xy / tile, slice));
float4 ch = cx * w.x + cy * w.y + cz * w.z;
float4 nr = nx * w.x + ny * w.y + nz * w.z;
// The layer's mean for the slice: the grain's light and dark round the
// instance's colour (the ground's recolour, Lab::Recolour's idea).
float3 mean = Means[min((int)(slice + 0.5), 8)].rgb;
// Light and dark only: the texture's own hue would fight the palette's.
float lum = dot(ch.rgb, float3(0.3, 0.59, 0.11)) / max(dot(mean, float3(0.3, 0.59, 0.11)), 0.02);
float3 c = Inst.rgb * lerp(1.0, lum, Detail);
// Cracks and the underside: darker.
float under = saturate(0.5 + 0.5 * n.z);
c *= lerp(1.0, nr.a * nr.a, AoGain) * lerp(0.55, 1.0, under);
// The grain's normal on each plane, blended (whiteout-ish, world space).
float2 gx = nx.rg * 2.0 - 1.0, gy = ny.rg * 2.0 - 1.0, gz = nz.rg * 2.0 - 1.0;
float3 bent = n + NormalGain * (float3(0, gx.x, gx.y) * w.x + float3(gy.x, 0, gy.y) * w.y + float3(gz.x, gz.y, 0) * w.z);
Nrm = normalize(bent);
Rough = saturate(lerp(0.85, nr.b, 0.6));
return c;
"""


def node(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def build():
    path = f"{LAB_DIR}/{MAT_NAME}"
    mat = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if mat is None:
        mat = tools.create_asset(MAT_NAME, LAB_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("tangent_space_normal", False)
    # The tufts' blades carry their own back faces; nothing needs two sides.
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    custom = node(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("description", "Terrain rock")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    outs = []
    for name, kind in (("Rough", "CMOT_FLOAT1"), ("Nrm", "CMOT_FLOAT3")):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", getattr(unreal.CustomMaterialOutputType, kind))
        outs.append(o)
    custom.set_editor_property("additional_outputs", outs)

    y = -500
    sources = []

    def add(name, n):
        sources.append((name, n))

    add("WP", node(mat, unreal.MaterialExpressionWorldPosition, -1100, y))
    add("VN", node(mat, unreal.MaterialExpressionVertexNormalWS, -1100, y + 60))
    # The instance's four floats in one vector.
    parts = []
    for i in range(4):
        d = node(mat, unreal.MaterialExpressionPerInstanceCustomData, -1400, y + 140 + 70 * i)
        d.set_editor_property("data_index", i)
        parts.append(d)
    rg = node(mat, unreal.MaterialExpressionAppendVector, -1250, y + 160)
    mel.connect_material_expressions(parts[0], "", rg, "A")
    mel.connect_material_expressions(parts[1], "", rg, "B")
    rgb = node(mat, unreal.MaterialExpressionAppendVector, -1200, y + 220)
    mel.connect_material_expressions(rg, "", rgb, "A")
    mel.connect_material_expressions(parts[2], "", rgb, "B")
    rgba = node(mat, unreal.MaterialExpressionAppendVector, -1150, y + 280)
    mel.connect_material_expressions(rgb, "", rgba, "A")
    mel.connect_material_expressions(parts[3], "", rgba, "B")
    add("Inst", rgba)

    yy = [y + 420]

    def tex(name, param, asset, sampler):
        t = node(mat, unreal.MaterialExpressionTextureObjectParameter, -1100, yy[0])
        yy[0] += 70
        t.set_editor_property("parameter_name", param)
        t.set_editor_property("texture", unreal.load_asset(asset))
        t.set_editor_property("sampler_type", sampler)
        add(name, t)

    def scalar(name, value):
        s = node(mat, unreal.MaterialExpressionScalarParameter, -1100, yy[0])
        yy[0] += 70
        s.set_editor_property("parameter_name", name)
        s.set_editor_property("default_value", value)
        add(name, s)

    tex("LabCH", "LabColour", f"{LAB_DIR}/Textures/T_TerrainCH", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tex("LabNR", "LabNormal", f"{LAB_DIR}/Textures/T_TerrainNR", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    scalar("RockTile", 4.0)
    scalar("ClutterTile", 0.6)
    scalar("Detail", 0.9)
    scalar("AoGain", 0.5)
    scalar("NormalGain", 0.6)
    inputs = []
    for name, _ in sources:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    for name, src in sources:
        if not mel.connect_material_expressions(src, "", custom, name):
            raise RuntimeError(f"could not connect {name}")
    # The layers' means, written into the code as constants (round 1 did
    # the same for the ground: means.json).
    means = json.load(open(os.path.join(SRC, "means.json")))
    table = ", ".join("float4(%.4f, %.4f, %.4f, 1)" % tuple(means[l]) for l in LAYERS)
    code = "const float4 Means[9] = { " + table + " };\n" + HLSL
    custom.set_editor_property("code", code)

    P = unreal.MaterialProperty
    for out, prop in (("", P.MP_BASE_COLOR), ("Rough", P.MP_ROUGHNESS), ("Nrm", P.MP_NORMAL)):
        if not mel.connect_material_property(custom, out, prop):
            raise RuntimeError(f"could not connect output {out or 'main'}")
    mel.recompile_material(mat)
    assets.save_asset(path, only_if_is_dirty=False)
    unreal.log(f"make_terrain_rock_material: built {path}")


build()
