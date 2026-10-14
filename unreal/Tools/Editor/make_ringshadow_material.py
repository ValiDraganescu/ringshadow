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
- the sky (farther than `SkyDepth` from the camera): the ring, a dark strip `RingHalf` degrees
  wide along the great circle through the giant (`Giant`) and the sun
  (`SunDir`), offset `RingOff` degrees from the sun, with lit rims; the
  offset follows the camera's place in the band, so the strip's edge crosses
  the sun as the band's edge crosses the camera (`RingOn` 0: none);
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
float3 v = -normalize(V);
// How far the pixel is (cm): the sky dome and the clouds lie beyond SkyDepth.
float dist = length(W - Cam);
if (dist < SkyDepth) {
    // The ground: in the band it keeps only the sky's cold light.
    float s = dot(W.xy / 100.0, float2(SweepDirX, SweepDirY));
    float cov = smoothstep(SweepTrail, SweepTrail + SweepSoft, s) * (1.0 - smoothstep(SweepLead - SweepSoft, SweepLead, s));
    float3 shade = Shade * float3(0.72, 0.9, 1.35);
    c *= lerp(float3(1, 1, 1), shade, cov) / max(SunDim, 0.05);
    if (Debug > 0.5) return float3(cov, frac(s / 20.0), saturate(dist / 10000.0));
} else if (RingOn > 0.5) {
    // The sky: the ring's strip along the great circle through the giant
    // and the sun, `RingOff` degrees off the sun; dark dust with lit rims.
    float3 nrm = cross(Giant, SunDir);
    nrm = dot(nrm, nrm) > 1e-6 ? normalize(nrm) : float3(0, 0, 1);
    float a = degrees(asin(clamp(dot(v, nrm), -1.0, 1.0))) - RingOff;
    float e = 0.35;
    float strip = smoothstep(-RingHalf - e, -RingHalf + e, a) * (1.0 - smoothstep(RingHalf - e, RingHalf + e, a));
    // Only the arc on the sun's and the giant's side of the sky.
    float side = smoothstep(-0.2, 0.3, dot(v, normalize(SunDir + Giant + float3(0, 0, 1e-3))));
    float rim = exp(-pow((abs(a) - RingHalf) / 0.3, 2.0));
    float nearSun = pow(saturate(dot(v, SunDir)), 8.0);
    c = c * lerp(1.0, 0.28, strip * side) + float3(1.0, 0.9, 0.75) * rim * side * (0.04 + 0.6 * nearSun) * max(dot(c, float3(0.33, 0.33, 0.33)), 0.2);
}
// The air in the band: colder and a little greyer.
float g = dot(c, float3(0.3, 0.59, 0.11));
c = lerp(c, g * float3(0.72, 0.86, 1.18), 0.4 * CamCover);
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
    inputs = [
        ("C", scene, "Color"),
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
        ("RingOn", scalar("RingOn", 0.0), ""),
        ("RingOff", scalar("RingOff", 30.0), ""),
        ("RingHalf", scalar("RingHalf", 4.0), ""),
        ("SunDir", vector("SunDir", (0, 0, 1)), ""),
        ("Giant", vector("Giant", (-0.97, -0.17, 0.2)), ""),
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
