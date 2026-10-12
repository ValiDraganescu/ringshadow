"""The weather's materials (docs/weather.md): /Game/Weather/M_AcRain,
/Game/Weather/M_AcBolt, /Game/Weather/M_AcLensRain, and the wet hulls:
MPC_AcWorld's `Wetness` (added to it, not rewritten: a rewrite gives its
parameters new ids) and M_Hull rebuilt with it (make_materials.build_master,
against the existing MPC_AcTeams; a material reads at most two collections).

Rerunnable: rebuilt in place.

    UnrealEditor-Cmd unreal/Autocraft.uproject -run=pythonscript \\
        -script="$PWD/unreal/Tools/Editor/make_weather_materials.py" -unattended

M_AcRain: the rain streaks AAcWeather draws (Source/Autocraft/AcWeather.cpp).
One procedural mesh of quads whose four corners sit on one random point of
a box (local cm, the actor at the origin), told apart by UV0. The vertex
shader:
- moves the point with the rain (`RainOff`: the fall so far, wrapped per
  axis on the CPU in doubles, so no float here ever grows large),
- wraps it into the box round `RainC` (a point ahead of the camera), so a
  streak never swims with the camera: it is fixed in the world until it
  leaves the box and comes in again on the other side,
- opens it into a streak along the fall, facing the camera, `RainOff.w`
  cm wide per cm of distance (a pixel or two at any distance),
- collapses it to a point when its number is above the rain rate
  (`RainC.w`), so light rain shows a few, a storm all of them.
The pixel shader tapers it (thin ends, soft sides), fades it near the
camera and at the box's edges, and colours it with `RainColor` (rgb lit
by the sky and the lightning, a alpha).

Parameters (AAcWeather sets them every frame): RainC (box centre cm, rate),
RainBox (size cm, streak length cm), RainOff (fall so far cm, width per cm),
RainVel (velocity cm/s), RainColor.

M_AcBolt: a lightning bolt's ribbon (AAcWeather builds it on the CPU for
the second a strike lasts, facing the camera): additive, unlit; across the
ribbon (UV.x) a thin white-hot core and a soft blue glow, its brightness
the vertex colour's alpha (the strike's flicker), times `BoltColor`.

M_AcLensRain: rain on the glass while driving a unit in first person (a
post-process, after depth of field, like the pilot's veil): two sizes of
drops in screen cells, each a little lens that shows the scene through it
upside down, a faint dark rim; the big ones slide down; each beads up and
fades on its own clock. `LensRain` (0-1, the rain rate) sets how many.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_materials as MM  # noqa: E402
import make_resource_materials as RM  # noqa: E402

F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

RAIN = "/Game/Weather/M_AcRain"
BOLT = "/Game/Weather/M_AcBolt"

BOLT_HLSL = r"""
float x = (UV.x - 0.5) * 2.0;
float glow = exp(-x * x * 6.0);
float core = exp(-x * x * 120.0);
return (Color.rgb * glow * 0.6 + float3(1.0, 1.0, 1.0) * core * 1.6) * Color.a * VA;
"""

WPO_HLSL = r"""
float3 box = Box.xyz;
float3 seed = L / box;
float id = frac(sin(dot(seed, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float3 p = L + Off.xyz;
float3 rel = p - C.xyz;
rel -= box * floor(rel / box + 0.5);
float3 ctr = C.xyz + rel;
float3 ax = normalize(Vel.xyz);
float3 toCam = Cam - ctr;
float d = max(length(toCam), 1.0);
float3 side = cross(ax, toCam / d);
float sl = length(side);
// Looking straight along the fall: any side will do.
side = sl > 1e-3 ? side / sl : float3(1, 0, 0);
float w = max(d * Off.w, 0.3);
float len = Box.w * (0.6 + 0.8 * frac(id * 7.31));
float2 uv = UV - 0.5;
float3 pos = ctr + ax * (uv.y * len) + side * (uv.x * w);
pos = id < C.w ? pos : ctr;
return pos - W;
"""

COLOUR_HLSL = r"""
float2 uv = UV;
// Thin at the ends, soft at the sides.
float shape = sin(3.14159265 * uv.y) * (1.0 - abs(uv.x * 2.0 - 1.0));
// Not right in front of the lens; gone at the box's edges.
float d = length(Cam - W);
float nearFade = smoothstep(60.0, 250.0, d);
float3 rel = abs(W - C.xyz) / Box.xyz;
float edge = 1.0 - smoothstep(0.36, 0.5, max(rel.x, max(rel.y, rel.z)));
Opacity = saturate(shape * nearFade * edge * Color.a);
return Color.rgb;
"""


def build():
    mat, g = RM.new_master(RAIN, unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    cam = g.node(unreal.MaterialExpressionCameraPositionWS)
    c = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="RainC",
               default_value=unreal.LinearColor(0, 0, 0, 0), group="Ac")
    box = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="RainBox",
                 default_value=unreal.LinearColor(4000, 4000, 3000, 55), group="Ac")
    off = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="RainOff",
                 default_value=unreal.LinearColor(0, 0, 0, 0.0012), group="Ac")
    vel = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="RainVel",
                 default_value=unreal.LinearColor(0, 0, -700, 0), group="Ac")
    colour = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="RainColor",
                    default_value=unreal.LinearColor(0.5, 0.55, 0.6, 0.3), group="Ac")

    wpo = RM.custom(g, "AcRainFall", WPO_HLSL, [
        ("L", RM.local_position(g), ""),
        ("W", g.node(unreal.MaterialExpressionWorldPosition), ""),
        ("UV", uv, ""),
        ("Cam", cam, ""),
        ("C", c, "RGBA"),
        ("Box", box, "RGBA"),
        ("Off", off, "RGBA"),
        ("Vel", vel, "RGBA"),
    ])
    g.out(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    col = RM.custom(g, "AcRainColour", COLOUR_HLSL, [
        ("UV", uv, ""),
        ("W", g.node(unreal.MaterialExpressionWorldPosition), ""),
        ("Cam", cam, ""),
        ("C", c, "RGBA"),
        ("Box", box, "RGBA"),
        ("Color", colour, "RGBA"),
    ], outputs=[("Opacity", F1)])
    g.out(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(col, "Opacity", unreal.MaterialProperty.MP_OPACITY)
    RM.finish(mat, RAIN, [])
    unreal.EditorAssetLibrary.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_weather_materials.py")
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


LENS_HLSL = r"""
struct AcLensFns {
    float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
};
AcLensFns F;
float2 buv = GetDefaultSceneTextureUV(Parameters, 14);
float aspect = Size.x / max(Size.y, 1.0);
float2 off = float2(0.0, 0.0);
float rim = 0.0;
[unroll] for (int L = 0; L < 2; L++) {
    float cells = L == 0 ? 9.0 : 22.0;
    float2 p = float2(VUV.x * aspect, VUV.y) * cells;
    float2 ci = floor(p);
    float2 f = frac(p) - 0.5;
    float h = F.h(ci + float(L) * 31.7);
    float life = frac(T * (0.08 + 0.05 * h) + h * 17.0);
    float on = step(h, Rain * (L == 0 ? 0.45 : 0.6)) * smoothstep(0.0, 0.08, life) * (1.0 - smoothstep(0.7, 1.0, life));
    float2 c = (float2(F.h(ci + 3.1), F.h(ci + 7.9)) - 0.5) * 0.5;
    c.y += L == 0 ? life * 0.35 : 0.0;
    float r = (L == 0 ? 0.22 : 0.16) * (0.6 + 0.4 * F.h(ci + 5.3));
    float2 d = (f - c) / r;
    float dd = dot(d, d);
    if (dd < 1.0 && on > 0.0) {
        float z = sqrt(1.0 - dd);
        off -= d * (1.0 - z * 0.3) * (0.6 / cells) * float2(1.0 / aspect, 1.0) * on * 0.5;
        rim += smoothstep(0.7, 1.0, dd) * on;
    }
}
float3 c0 = SceneTextureLookup(buv + off, 14, false).rgb;
return c0 * (1.0 - saturate(rim) * 0.35);
"""


def build_lens():
    lens = "/Game/Weather/M_AcLensRain"
    mat, g = RM.new_master(lens, unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
    scene = g.node(unreal.MaterialExpressionSceneTexture)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    size = g.node(unreal.MaterialExpressionViewSize)
    sizes = g.node(unreal.MaterialExpressionAppendVector)
    g.link(size, "", sizes, "A")
    g.link(size, "", sizes, "B")
    e = RM.custom(g, "AcLensRain", LENS_HLSL, [
        ("VUV", g.node(unreal.MaterialExpressionScreenPosition), "ViewportUV"),
        ("Size", sizes, ""),
        ("T", g.node(unreal.MaterialExpressionTime), ""),
        ("Rain", g.node(unreal.MaterialExpressionScalarParameter, parameter_name="LensRain", default_value=0.0, group="Ac"), ""),
        ("S", scene, "Color"),
    ])
    g.out(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    RM.finish(mat, lens, [])
    unreal.EditorAssetLibrary.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_weather_materials.py")
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def build_bolt():
    mat, g = RM.new_master(BOLT, unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", True)
    colour = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="BoltColor",
                    default_value=unreal.LinearColor(0.65, 0.75, 1.0, 40.0), group="Ac")
    e = RM.custom(g, "AcBolt", BOLT_HLSL, [
        ("UV", g.node(unreal.MaterialExpressionTextureCoordinate), ""),
        ("VA", g.node(unreal.MaterialExpressionVertexColor), "A"),
        ("Color", colour, "RGBA"),
    ])
    g.out(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    RM.finish(mat, BOLT, [])
    unreal.EditorAssetLibrary.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_weather_materials.py")
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


unreal.log(f"make_weather_materials: built {build().get_path_name()}")
unreal.log(f"make_weather_materials: built {build_bolt().get_path_name()}")
unreal.log(f"make_weather_materials: built {build_lens().get_path_name()}")


def build_wet_hulls():
    import ac_models as A
    teams = MM.load(A.MPC_TEAMS)
    if teams is None:
        raise RuntimeError("no MPC_AcTeams: run make_materials.py first")
    import make_resource_materials
    return MM.build_master(A.M_HULL, "hull", teams, make_resource_materials.make_world_mpc())


unreal.log(f"make_weather_materials: built {build_wet_hulls().get_path_name()}")
