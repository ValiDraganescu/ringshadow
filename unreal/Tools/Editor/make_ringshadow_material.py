"""M_Ringshadow: the ringshadow sweep's look (docs/weather.md, "Ringshadow
sweep"; docs/mmorpg-m7.md E1).

Rerunnable: rebuilds the material's graph from nothing each time.

Output: /Game/Sky/M_Ringshadow, a post-process material (before depth of
field, on the linear HDR colour, so the flaring ore blooms) that
AAcRingshadow (Source/Autocraft/AcRingshadow.cpp) puts on an unbound
post-process component:

- the ground under the band (MPC_AcWorld's `SweepDirX`, `SweepDirY`,
  `SweepLead`, `SweepTrail`, `SweepSoft`, cells, the same band as
  `ac::Ringshadow::band`) goes to the sky's cold light: `Shade` (0.14) of
  its colour in linear light, bluer (about 0.4 of its brightness on screen), with soft edges, divided by `SunDim` (what the sun at the
  camera was dimmed to, AAcDaylight::RingshadowSun), so the band reads the
  same seen from inside it as from outside;
- god rays: the sky's pixels brighter than `RayFloor` (of l/(1+l): the
  sun's disc counts as about 1, not its full HDR) smeared toward the
  sun (`SunUV`, viewport UV) by `Rays` (0: none), strongest as the ring's
  edge crosses the sun; the ring's bands, the clouds and the land cut them
  into shafts. The ring's arc itself is on the sky dome (M_AcSky,
  make_sky_material.py), behind the clouds;
- the air: a cold tint, `CamCover` (how deep the camera is in the band).

Run it in the editor (`py <path>`), or headless while the live editor does
not have the asset open:
  UnrealEditor-Cmd Autocraft.uproject -run=pythonscript -script=<abs path> -unattended -nullrhi
Run make_resource_materials.py first: it adds the band to MPC_AcWorld.
"""
import os

import unreal

MAT_DIR = "/Game/Sky"
MAT_NAME = "M_Ringshadow"
MPC_WORLD = "/Game/Materials/MPC_AcWorld"
LOCATION = unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary

HLSL = r"""
float3 c = C.rgb;
// How far the pixel is (cm): the sky dome and the clouds lie beyond SkyDepth.
float dist = length(W - Cam);
if (dist < SkyDepth) {
    // The ground: in the band it keeps only the sky's cold light.
    float s = dot(W.xy / 100.0, float2(SweepDirX, SweepDirY));
    float cov = smoothstep(SweepTrail, SweepTrail + SweepSoft, s) * (1.0 - smoothstep(SweepLead - SweepSoft, SweepLead, s));
    float3 shade = Shade * float3(0.72, 0.9, 1.35);
    c *= lerp(float3(1, 1, 1), shade, cov) / max(SunDim, 0.05);
    if (Debug > 0.5) return float3(cov, frac(s / 20.0), saturate(dist / 10000.0));
}
// God rays: the bright sky toward the sun (its disc and the ring's gaps
// glowing round it, the clouds' lit edges) smeared along the line to the
// sun, only from the sky's pixels, so the ring's bands, the clouds and the
// land cut them into shafts.
if (Rays > 0.001) {
    float2 aspect = float2(View.ViewSizeAndInvSize.x * View.ViewSizeAndInvSize.w, 1.0);
    float2 toSun = SunUV - UV;
    float far = length(toSun * aspect);
    float3 acc = 0;
    float decay = 1.0;
    const int N = 48;
    [loop] for (int i = 1; i <= N; i++) {
        float2 q = UV + toSun * (float(i) / float(N));
        if (any(q < 0.0) || any(q > 1.0)) { decay *= 0.97; continue; }
        float2 bq = ViewportUVToBufferUV(q);
        float3 sc = SceneTextureLookup(Parameters, bq, 14, true).rgb;
        float dep = SceneTextureLookup(Parameters, bq, 1, false).r;
        // Compressed (the sun's disc counts as about 1, not its full HDR),
        // and only what stands out of the sky: the ring's dense bands cut it.
        float l = dot(sc, float3(0.3, 0.59, 0.11));
        float k = l / (1.0 + l);
        acc += sc / (1.0 + l) * saturate((k - RayFloor) / (1.0 - RayFloor)) * step(SkyDepth, dep) * decay;
        decay *= 0.97;
    }
    c += acc / float(N) * Rays * 1.2 * float3(1.0, 0.93, 0.8) * exp(-far * 1.4);
}
// The air in the band: colder and a little greyer.
float g = dot(c, float3(0.3, 0.59, 0.11));
c = lerp(c, g * float3(0.72, 0.86, 1.18), 0.35 * CamCover);
return max(c, 0.0);
"""


def build():
    path = f"{MAT_DIR}/{MAT_NAME}"
    mat = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", LOCATION)
    # World positions in cells and the sky's distance overflow half floats.
    try:
        mat.set_editor_property("float_precision_mode", unreal.MaterialFloatPrecisionMode.MFPM_FULL)
    except Exception:
        mat.set_editor_property("use_full_precision", True)
    mpc = unreal.load_asset(MPC_WORLD)
    if mpc is None:
        raise RuntimeError(f"no {MPC_WORLD}: run make_resource_materials.py first")

    y = [0]

    def node(cls, x=-700, **props):
        e = mel.create_material_expression(mat, cls, x, y[0])
        y[0] += 120
        for k, val in props.items():
            e.set_editor_property(k, val)
        return e

    def connect(a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            raise RuntimeError(f"could not connect {b_in}")

    def scalar(name, value):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value, group="Ac")

    def vector(name, rgb):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                    default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 0), group="Ac")

    scene = node(unreal.MaterialExpressionSceneTexture, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    # The depth, read in the rays' loop: this node only tells the material it is used.
    depth = node(unreal.MaterialExpressionSceneTexture, scene_texture_id=unreal.SceneTextureId.PPI_SCENE_DEPTH)
    inputs = [
        ("C", scene, "Color"),
        ("Dep", depth, "Color"),
        ("UV", node(unreal.MaterialExpressionScreenPosition), "ViewportUV"),
        ("W", node(unreal.MaterialExpressionWorldPosition), ""),
        ("V", node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("Cam", node(unreal.MaterialExpressionCameraPositionWS), ""),
        ("SkyDepth", scalar("SkyDepth", 1.5e6), ""),
    ]
    for name in ("SweepDirX", "SweepDirY", "SweepLead", "SweepTrail", "SweepSoft"):
        inputs.append((name, node(unreal.MaterialExpressionCollectionParameter, x=-900, collection=mpc, parameter_name=name), ""))
    inputs += [
        ("Shade", scalar("Shade", 0.14), ""),
        # Debug 1 (RINGSHADOW_DEBUG=1 when the script runs): the ground shows
        # the cover (red), the band coordinate's 20-cell stripes (green) and
        # the distance (blue, 100 m full).
        ("Debug", scalar("Debug", 1.0 if os.environ.get("RINGSHADOW_DEBUG") else 0.0), ""),
        ("SunDim", scalar("SunDim", 1.0), ""),
        ("CamCover", scalar("CamCover", 0.0), ""),
        ("SunUV", vector("SunUV", (0.5, -1.0, 0)), ""),
        ("Rays", scalar("Rays", 0.0), ""),
        ("RayFloor", scalar("RayFloor", 0.6), ""),
    ]

    custom = node(unreal.MaterialExpressionCustom, x=-300, code=HLSL, description="Ringshadow sweep",
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for name, _, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    custom.set_editor_property("inputs", ins)
    for name, src, pin in inputs:
        connect(src, pin, custom, name)
    if not mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("could not connect emissive")

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    assets.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_ringshadow_material.py")
    assets.save_asset(path, only_if_is_dirty=False)
    return mat


unreal.log(f"make_ringshadow_material: built {build().get_path_name()}")
