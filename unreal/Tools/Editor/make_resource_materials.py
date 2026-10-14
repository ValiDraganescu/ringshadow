"""The resource materials (chunk B8, GAME-LAYER.md §2.6 "Ore and wells"):
the Swift shader modifiers on Stardust Ore and Metallic Hydrogen rebuilt as
Unreal materials. Rerunnable: every asset is rebuilt in place.

Run it (make_materials.py also runs it as its last step, so the instances it
re-parents never fall back to M_Hull):

    UnrealEditor-Cmd unreal/Autocraft.uproject -run=pythonscript \
        -script="$PWD/unreal/Tools/Editor/make_resource_materials.py" -unattended -nullrhi

What it makes:

- /Game/Materials/MPC_AcWorld: world-wide values the C++ sets every frame.
  `Dark` (0 by day, 1 at night: AAcDaylight::GetDark, written by AAcResources);
  the ringshadow's band (`SweepDirX`, `SweepDirY`, `SweepLead`, `SweepTrail`,
  `SweepSoft`: AAcRingshadow), under which ore veins and well rims flare;
  `HullEnv` (0.6), how much of the Swift studio sky M_Hull's metals mirror
  on top of Lumen (`hull_env`, the chrome fix; 0 turns it off).
- /Game/Materials/M_Opal: Stardust Ore (docs/naming.md, "How they look").
  Near-black basalt run through with thin opal veins (bold veins and
  hairlines, isolines of a warped noise) whose thin-film hue (teal, violet,
  gold) follows the view angle, brightest at grazing angles, a faint sheen
  beside them, and twinkling dust glints. The veins get a clear coat (a
  glossy film, so they catch the real sky and sun in Lumen reflections).
  At night they glow softly (`NightGlow` x MPC_AcWorld.Dark), breathing
  slowly; far off a deposit keeps a faint glow, so dusk turns the plain into
  a map of veins. The veins are filtered by the pixel's footprint (from the
  depth), so far off they fade to their average instead of shimmering.
  Patterns are in the instance's own space (`PatternScale` maps cm to
  pattern units), so a carried chunk keeps its veins.
- /Game/Materials/M_Mercury: Metallic Hydrogen (`MaterialLibrary.mercury`,
  `mercuryLook`, `ModelLibrary.wellPool`). Mirror silver with the Swift
  studio sky faked in emission (`EnvGain`, dimmed at night) and a cold sheen
  at grazing angles. `Pool` = 1 adds the well pool's motion: the surface
  heaves and rings run out (world position offset, upper half only), and the
  normals ripple with three wave trains; each well's phase comes from where
  it stands. At night (`NightGlow` x MPC_AcWorld.Dark) the pool keeps a
  cold blue-white rim and a dim glow, so a well reads across the dark.
- /Game/Materials/M_AcPuff and its instances MI_AcVapour, MI_AcGlint: an
  additive sprite (a soft puff, or a star-shaped glint) for the CPU
  particles of AAcResources (the well's vapour, the ore's glitter); colour
  and alpha per instance (custom data 0-3).

It then re-parents the model instances made by make_materials.py: every
`oreRock` material (the deposits, a Prospector's carried chunk, the cockpit)
to M_Opal, and the well pool (`plain_494e59` with a geometry shader
modifier) to M_Mercury. Their parameters keep the names M_Hull uses
(BaseColorTint, Roughness, Metallic, NormalTex, NormalStrength), so
make_materials.py fills them as for any other instance.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ac_models as A  # noqa: E402
import make_materials as MM  # noqa: E402

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

MPC_WORLD = A.MASTER_DIR + "/MPC_AcWorld"
M_OPAL = A.MASTER_DIR + "/M_Opal"
M_MERCURY = A.MASTER_DIR + "/M_Mercury"
M_PUFF = A.MASTER_DIR + "/M_AcPuff"
MI_VAPOUR = A.MASTER_DIR + "/MI_AcVapour"
MI_GLINT = A.MASTER_DIR + "/MI_AcGlint"

F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3


def log(msg):
    unreal.log("[make_resource_materials] " + msg)


def is_opal(mat):
    return mat["name"].endswith("oreRock")


def is_pool(mat):
    return mat["name"].startswith("plain_494e59") and "SCNShaderModifierEntryPointGeometry" in mat.get("shaderModifiers", [])


# --- the world MPC -------------------------------------------------------------

def make_world_mpc():
    mpc = MM.load(MPC_WORLD) or MM.create(MPC_WORLD, unreal.MaterialParameterCollection,
                                          unreal.MaterialParameterCollectionFactoryNew())
    # Wetness: how wet the hulls are (AAcWeather, docs/weather.md). It lives
    # here because a material may read only two collections, and M_Hull
    # already reads this one and MPC_AcTeams.
    # The ringshadow's band (AAcRingshadow, Ringshadow.h; docs/weather.md):
    # the way it moves (x, y), its leading and trailing edges along it and
    # its soft edge, cells; far off (lead = trail = -1e6) when none runs.
    # Then the ring's arc in the sky (M_AcSky, Tools/Editor/make_sky_material.py):
    # on or off, its offset from the sun and its half width, degrees.
    wanted = (("Dark", 0.0), ("HullEnv", 0.6), ("Wetness", 0.0),
              ("SweepDirX", 1.0), ("SweepDirY", 0.0), ("SweepLead", -1.0e6), ("SweepTrail", -1.0e6),
              ("SweepSoft", 16.0), ("RingOn", 0.0), ("RingOff", 30.0), ("RingHalf", 6.0))
    kept = list(mpc.get_editor_property("scalar_parameters"))
    have = [str(x.get_editor_property("parameter_name")) for x in kept]
    if have == [n for n, _ in wanted]:
        return mpc
    # Rewriting a parameter gives it a new id, which would leave every
    # material compiled against the old one stale: keep the ones there and
    # add only the missing ones at the end.
    if have != [n for n, _ in wanted][:len(have)]:
        kept = []
    scalars = kept
    for name, value in wanted[len(kept):]:
        s = unreal.CollectionScalarParameter()
        s.set_editor_property("parameter_name", name)
        s.set_editor_property("default_value", value)
        scalars.append(s)
    mpc.set_editor_property("scalar_parameters", scalars)
    EAL.set_metadata_tag(mpc, "AcMadeBy", "Tools/Editor/make_resource_materials.py")
    EAL.save_loaded_asset(mpc)
    return mpc


# --- shared graph helpers ----------------------------------------------------

def custom(g, desc, code, inputs, outputs=(), out_type=F3):
    """A Custom node with `inputs` [(name, node, output pin)] wired and extra
    `outputs` [(name, type)]."""
    c = g.custom(desc, code, [n for n, _, _ in inputs], out_type)
    extra = []
    for name, kind in outputs:
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", kind)
        extra.append(o)
    c.set_editor_property("additional_outputs", extra)
    for name, src, pin in inputs:
        g.link(src, pin, c, name)
    return c


def new_master(path, shading, blend=unreal.BlendMode.BLEND_OPAQUE):
    mat = MM.load(path) or MM.create(path, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("two_sided", False)
    return mat, MM.Graph(mat)


def finish(mat, path, usages):
    for usage in usages:
        MEL.set_base_material_usage(mat, usage)
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_resource_materials.py")
    EAL.save_loaded_asset(mat)
    log("master %s" % path)


def local_position(g):
    """The position in the instance's own space (cm)."""
    return g.node(unreal.MaterialExpressionLocalPosition)


def instance_origin(g):
    """The instance's origin in world space (cm): local (0,0,0) → world."""
    zero = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 0, 0))
    t = g.node(unreal.MaterialExpressionTransformPosition,
               transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL,
               transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    g.link(zero, "", t, "")
    return t


# --- M_Hull's chrome (called by make_materials.build_master) -------------------

HULL_ENV_HLSL = """
// The Swift game lit every model with a sky map (MaterialLibrary.
// skyEnvironment, lightingEnvironment 0.75): brown ground, a bright warm
// horizon, blue sky. Its metals mirror it, which keeps chrome light grey from
// the top-down camera; under Lumen they mirror the dark ground and the parts
// beside them and come out darker. This adds that sky back to metals, by
// gloss, fading at night. Gain: MPC_AcWorld.HullEnv (0 turns it off).
float3 R = reflect(-normalize(V), normalize(N));
float t = asin(clamp(R.z, -1.0, 1.0)) / 3.14159265 + 0.5;
float3 c0 = float3(0.36, 0.28, 0.2), c1 = float3(0.55, 0.45, 0.36), c2 = float3(0.93, 0.83, 0.7);
float3 c3 = float3(0.55, 0.66, 0.82), c4 = float3(0.3, 0.42, 0.66);
float3 c = t < 0.42 ? lerp(c0, c1, t / 0.42) : t < 0.5 ? lerp(c1, c2, (t - 0.42) / 0.08)
         : t < 0.65 ? lerp(c2, c3, (t - 0.5) / 0.15) : lerp(c3, c4, (t - 0.65) / 0.35);
c = pow(c, 2.2);
float gloss = 1.0 - saturate(Rough);
return Base * saturate(Metal) * c * gloss * Gain * (1.0 - 0.85 * Dark);
"""


def hull_env(g, base_color, metallic, roughness):
    """The emission M_Hull adds for the Swift studio sky on metals."""
    mpc = make_world_mpc()
    return custom(g, "Swift sky map on metals", HULL_ENV_HLSL, [
        ("Base", base_color, ""), ("Metal", metallic, ""), ("Rough", roughness, ""),
        # Swift's _surface.normal carries the rock normal map: its streaks
        # break the film's hue into thin sheens (the vertex normal gave
        # broad blotches close up).
        ("N", g.node(unreal.MaterialExpressionPixelNormalWS), ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("Dark", g.mpc(mpc, "Dark"), ""),
        ("Gain", g.mpc(mpc, "HullEnv"), ""),
    ])


# --- M_Opal ------------------------------------------------------------------

OPAL_HLSL = """
// Stardust Ore (docs/naming.md, "How they look"; docs/mmorpg-m6.md V2):
// near-black rock run through with thin opal veins whose thin-film hue
// (teal, violet, gold) follows the view angle, brightest at grazing angles,
// a faint iridescent sheen beside them, and twinkling dust glints. At night
// the veins glow softly (NightGlow, by MPC_AcWorld.Dark), so dusk turns the
// plain into a map of veins. Patterns are in the instance's own space
// (PatternScale maps cm to pattern units), so a carried chunk keeps its
// veins. The veins are isolines of a domain-warped noise, filtered by the
// pixel's footprint (from the depth: no derivatives), so far off they fade
// to their average instead of shimmering.
struct AcOre
{
    float Hash(float3 p)
    {
        p = frac(p * 0.3183099 + 0.1);
        p *= 17.0;
        return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
    }
    float Noise(float3 x)
    {
        float3 i = floor(x), f = frac(x);
        f = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(lerp(Hash(i), Hash(i + float3(1, 0, 0)), f.x),
                         lerp(Hash(i + float3(0, 1, 0)), Hash(i + float3(1, 1, 0)), f.x), f.y),
                    lerp(lerp(Hash(i + float3(0, 0, 1)), Hash(i + float3(1, 0, 1)), f.x),
                         lerp(Hash(i + float3(0, 1, 1)), Hash(i + float3(1, 1, 1)), f.x), f.y), f.z);
    }
    float3 Opal(float t)
    {
        float3 teal = float3(0.05, 0.95, 0.8), violet = float3(0.62, 0.28, 1.0), gold = float3(1.0, 0.72, 0.16);
        t = frac(t) * 3.0;
        float k = smoothstep(0.0, 1.0, frac(t));
        return t < 1.0 ? lerp(teal, violet, k) : t < 2.0 ? lerp(violet, gold, k) : lerp(gold, teal, k);
    }
    // How far p is from the isoline Noise = 0.5, in p's units: the value
    // over the gradient's length, so a vein keeps its width where the
    // noise is flat (a plain band there smeared into broad patches).
    float Dist(float3 p, out float nv)
    {
        nv = Noise(p);
        const float e = 0.03;
        float3 gr = float3(Noise(p + float3(e, 0, 0)), Noise(p + float3(0, e, 0)), Noise(p + float3(0, 0, e))) - nv;
        return abs(nv - 0.5) / max(length(gr) / e, 0.15);
    }
    // A vein `w` wide (half width) at distance `d`, filtered by the pixel's
    // footprint `fp` (same units); .x the line, .y a wider halo.
    float2 Vein(float d, float w, float fp)
    {
        float ln = smoothstep(w + fp, max(w - fp, 0.0), d) * saturate(w * 2.5 / (w + fp));
        float halo = smoothstep(w * 8.0 + fp, w, d);
        return float2(ln, halo);
    }
};
AcOre o;
float3 n = normalize(N);
float3 v = normalize(V);
float ndv = saturate(dot(n, v));
float3 op = LP * PatternScale;
// The pixel's footprint in pattern units (a pixel spans ~1.6 mrad).
float fpx = Depth * PatternScale * 0.0016;
// Two vein sets through a warped field: bold veins and hairlines.
float3 wv = float3(o.Noise(op * 0.55 + 11.0), o.Noise(op * 0.55 + 23.0), o.Noise(op * 0.55 + 37.0)) - 0.5;
float3 q = op * 0.75 + wv * 1.8;
float n1, n2;
float d1 = o.Dist(q, n1);
float d2 = o.Dist(q * 2.6 + 5.0, n2);
float2 v1 = o.Vein(d1, 0.045, fpx * 0.75);
float2 v2 = o.Vein(d2, 0.05, fpx * 0.75 * 2.6);
float vein = saturate(v1.x + 0.55 * v2.x);
float halo = saturate(v1.y * 0.75 + v2.y * 0.25);
// How far the veins have faded into their average (0 near, 1 far).
float far = smoothstep(0.1, 0.4, fpx * 0.75);
// Thin film: the hue follows the view angle and the surface's turn.
float3 r = reflect(-v, n);
// The camera's basis from the view direction (left-handed, Z up).
float3 fwd = -v;
float3 right = cross(float3(0, 0, 1), fwd);
right = dot(right, right) > 1e-4 ? normalize(right) : float3(0, 1, 0);
float3 up = cross(fwd, right);
float hue = n1 * 1.6 + n2 * 0.4 + (1.0 - ndv) * 1.1 + dot(r, up) * 0.35 + dot(r, right) * 0.25 + T * 0.015;
float3 film = o.Opal(hue);
float grazing = pow(1.0 - ndv, 2.0);
float3 e = film * film * vein * (0.45 + 1.6 * grazing);
// A faint iridescent sheen beside the veins and over the rock at grazing angles.
e += o.Opal(hue + 0.4) * (halo * 0.12 + pow(1.0 - ndv, 3.0) * 0.08) * (1.0 - vein);
e *= OpalGain;
// Night: the veins breathe a soft light of their own (less view-bound, so
// they read from every side), and far off the deposit keeps a faint glow.
float3 nightCol = lerp(film, o.Opal(n1 * 1.6 + T * 0.02), 0.6);
float breathe = 0.82 + 0.18 * sin(T * 0.7 + n1 * 9.0);
float glow = (vein + halo * 0.07) * (1.0 - far) + far * 0.12;
// Under the ringshadow's band (docs/weather.md) the veins flare brighter
// than at night, the deposit a beacon in the shade.
float sws = dot(WP.xy / 100.0, float2(SweepDirX, SweepDirY));
float sweep = smoothstep(SweepTrail, SweepTrail + SweepSoft, sws) * (1.0 - smoothstep(SweepLead - SweepSoft, SweepLead, sws));
e += nightCol * glow * breathe * NightGlow * max(Dark, sweep * 1.8);
// Dust glints: sparse points that twinkle on and off (gone where too small to see).
float3 g = op * 5.0;
float3 cell = floor(g);
float h = o.Hash(cell + 0.37);
float3 at = cell + 0.5 + (float3(o.Hash(cell + 1.7), o.Hash(cell + 3.1), o.Hash(cell + 5.3)) - 0.5) * 0.5;
float twinkle = pow(saturate(sin(T * (1.4 + 2.6 * h) + h * 40.0)), 10.0);
float glint = step(0.62, h) * (1.0 - smoothstep(0.05, 0.18, length(g - at))) * twinkle * (1.0 - far);
e += lerp(float3(1.0, 0.95, 0.85), o.Opal(h * 5.0), 0.35) * glint * GlintGain * (1.0 + Dark + 2.0 * sweep);
Rough = lerp(R, 0.12, vein);
// Bare basalt takes little of the sky, so it stays near black.
AO = lerp(0.35, 1.0, saturate(vein + halo * 0.3));
Coat = saturate(vein + halo * 0.25) * CoatAmount;
return e * Scale;
"""


def build_opal(mpc_world):
    mat, g = new_master(M_OPAL, unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    # Clear coat has no MaterialProperty in Python: everything goes through
    # one MakeMaterialAttributes node.
    mat.set_editor_property("use_material_attributes", True)
    attrs = g.node(unreal.MaterialExpressionMakeMaterialAttributes, x=0)
    g.out(attrs, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    pins = {
        unreal.MaterialProperty.MP_BASE_COLOR: "BaseColor",
        unreal.MaterialProperty.MP_METALLIC: "Metallic",
        unreal.MaterialProperty.MP_NORMAL: "Normal",
        unreal.MaterialProperty.MP_EMISSIVE_COLOR: "EmissiveColor",
        unreal.MaterialProperty.MP_ROUGHNESS: "Roughness",
        unreal.MaterialProperty.MP_AMBIENT_OCCLUSION: "AmbientOcclusion",
        "coat": "ClearCoat",
        "coatRoughness": "ClearCoatRoughness",
    }

    class Attr:
        def __init__(self, graph):
            self.graph = graph

        def __getattr__(self, name):
            return getattr(self.graph, name)

        def out(self, node, pin, prop):
            self.graph.link(node, pin, attrs, pins[prop])

    g = Attr(g)
    tc = g.node(unreal.MaterialExpressionTextureCoordinate)
    color = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    base_tex = g.texture("BaseColorTex", "/Engine/EngineResources/WhiteSquareTexture", color, tc)
    tint = g.vector("BaseColorTint", (0.00841, 0.00541, 0.00375))
    base = g.binop(unreal.MaterialExpressionMultiply, base_tex, tint, a_out="RGB")
    g.out(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Metallic", 0.0), "", unreal.MaterialProperty.MP_METALLIC)

    normal_tex = g.texture("NormalTex", "/Engine/EngineMaterials/FlatNormal",
                           unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, tc)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 0))
    nl = g.node(unreal.MaterialExpressionLinearInterpolate, x=-300)
    g.link(flat, "", nl, "A")
    g.link(normal_tex, "RGB", nl, "B")
    g.link(g.scalar("NormalStrength", 1.0), "", nl, "Alpha")
    g.out(nl, "", unreal.MaterialProperty.MP_NORMAL)

    # Swift's `_surface.normal` is the normal-mapped one (rockNormal,
    # strength 3): its fine bumps break the thin film's hue into the thin
    # rainbow streaks inside the patches. PixelNormalWS cannot be read in a
    # material that writes Normal, so the same normal goes to world space here.
    n_ws = g.node(unreal.MaterialExpressionTransform, x=-300,
                  transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT,
                  transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(nl, "", n_ws, "")
    time = g.node(unreal.MaterialExpressionTime)
    scale = g.custom_data(1, "EmissionScale", 1.0)
    c = custom(g, "Swift opal (opalOre)", OPAL_HLSL, [
        ("LP", local_position(g), ""),
        ("N", n_ws, ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("T", time, ""),
        ("R", g.scalar("Roughness", 0.8), ""),
        ("Dark", g.mpc(mpc_world, "Dark"), ""),
        ("WP", g.node(unreal.MaterialExpressionWorldPosition), ""),
        ("SweepDirX", g.mpc(mpc_world, "SweepDirX"), ""),
        ("SweepDirY", g.mpc(mpc_world, "SweepDirY"), ""),
        ("SweepLead", g.mpc(mpc_world, "SweepLead"), ""),
        ("SweepTrail", g.mpc(mpc_world, "SweepTrail"), ""),
        ("SweepSoft", g.mpc(mpc_world, "SweepSoft"), ""),
        ("PatternScale", g.scalar("PatternScale", 0.04), ""),
        ("OpalGain", g.scalar("OpalGain", 1.0), ""),
        ("GlintGain", g.scalar("GlintGain", 7.0), ""),
        ("NightGlow", g.scalar("NightGlow", 2.2), ""),
        ("Depth", g.node(unreal.MaterialExpressionPixelDepth), ""),
        ("CoatAmount", g.scalar("CoatAmount", 1.0), ""),
        ("Scale", scale, ""),
    ], outputs=[("Rough", F1), ("AO", F1), ("Coat", F1)])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(c, "AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    g.out(c, "Coat", "coat")
    g.out(g.scalar("CoatRoughness", 0.06), "", "coatRoughness")
    finish(mat, M_OPAL, (unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, unreal.MaterialUsage.MATUSAGE_NANITE))
    return mat


# --- M_Mercury ---------------------------------------------------------------

POOL_WPO_HLSL = """
// ModelLibrary.wellPool, geometry: the pool heaves slowly and rings run out
// over it. The pool is a sphere of radius 0.5 squashed to 0.15 at 0.575 up
// the well (cells); only its upper half moves.
float2 q = LP.xy / 100.0;
float r = saturate(length(q) / 0.5);
float ph = dot(O.xy / 100.0, float2(0.37, 0.61));
float heave = (0.5 + 0.5 * sin(T * 0.7 + ph)) * 0.2 * (1.0 - r * r);
float ring = 0.05 * sin(r * 10.0 - T * 1.3 + ph) * (1.0 - r);
return float3(0, 0, (heave + ring) * 0.15 * 100.0 * step(57.5, LP.z) * Pool);
"""

MERCURY_HLSL = """
// ModelLibrary.wellPool (surface) and MaterialLibrary.mercuryLook, in world
// space (Z up).
float3 v = normalize(V);
float3 n = normalize(N);
float2 q = LP.xy / 100.0;
float r = length(q);
float2 dir = q / max(r, 1e-3);
float ph = dot(O.xy / 100.0, float2(0.37, 0.61));
float fadeOut = 1.0 - smoothstep(0.3, 0.5, r);
// The slope of three small wave trains: rings running out from the middle
// and two crossing swells.
float2 slope = dir * 0.2 * cos(r * 24.0 - T * 1.6 + ph)
    + float2(11.0, 6.0) * 0.006 * cos(dot(q, float2(11.0, 6.0)) + T * 0.9 + ph)
    + float2(-4.0, 14.0) * 0.005 * cos(dot(q, float2(-4.0, 14.0)) - T * 1.2);
slope *= fadeOut * Pool * step(0.0, n.z);
n = normalize(n + float3(-slope.x, -slope.y, 0.0));
Nrm = n;
// A cold studio sky mirrored in it (dark low down, silver-white overhead, a
// bright band across), and a blue-white sheen at grazing angles. Lumen
// reflects the real sky too, so the fake one is weaker than in Swift and
// fades at night.
float3 R = reflect(-v, n);
float up = smoothstep(0.72, 0.92, R.z);
float band = smoothstep(0.93, 0.99, sin(R.x * 5.0 + R.y * 2.0 + 1.0)) * smoothstep(0.5, 0.8, R.z);
float3 env = lerp(float3(0.05, 0.06, 0.08), float3(0.72, 0.78, 0.86), up) + band * float3(0.9, 0.95, 1.0);
float sheen = pow(1.0 - saturate(dot(n, v)), 3.0);
float day = 1.0 - 0.85 * Dark;
float3 e = env * 0.7 * EnvGain * day + float3(0.55, 0.75, 1.0) * sheen * 0.8 * lerp(1.0, 0.5, Dark);
// Night (docs/mmorpg-m6.md V2): the pool keeps a cold blue-white light, a
// bright rim and a dim, slowly shifting glow over the surface, so a well
// reads across the dark plain. The wave rings catch it.
float rim = smoothstep(0.30, 0.45, r) * smoothstep(0.53, 0.46, r);
float waves = 0.75 + 0.25 * sin(r * 24.0 - T * 1.6 + ph);
float3 cold = float3(0.62, 0.8, 1.0);
e += cold * (rim * 1.4 + 0.3 * fadeOut * waves) * NightGlow * Dark * Pool * step(0.0, n.z);
return e * Scale;
"""


def build_mercury(mpc_world):
    mat, g = new_master(M_MERCURY, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("tangent_space_normal", False)
    tint = g.vector("BaseColorTint", (0.2867, 0.30921, 0.35234))
    g.out(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Metallic", 1.0), "", unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar("Roughness", 0.05), "", unreal.MaterialProperty.MP_ROUGHNESS)
    time = g.node(unreal.MaterialExpressionTime)
    lp = local_position(g)
    origin = instance_origin(g)
    pool = g.scalar("Pool", 0.0)
    wpo = custom(g, "Swift well pool (geometry)", POOL_WPO_HLSL, [
        ("LP", lp, ""), ("O", origin, ""), ("T", time, ""), ("Pool", pool, ""),
    ])
    g.out(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    c = custom(g, "Swift mercury (wellPool, mercuryLook)", MERCURY_HLSL, [
        ("LP", lp, ""), ("O", origin, ""), ("T", time, ""), ("Pool", pool, ""),
        ("N", g.node(unreal.MaterialExpressionVertexNormalWS), ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("Dark", g.mpc(mpc_world, "Dark"), ""),
        ("EnvGain", g.scalar("EnvGain", 0.6), ""),
        ("NightGlow", g.scalar("NightGlow", 1.6), ""),
        ("Scale", g.custom_data(1, "EmissionScale", 1.0), ""),
    ], outputs=[("Nrm", F3)])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, "Nrm", unreal.MaterialProperty.MP_NORMAL)
    # WPO keeps Nanite off for the pool mesh's sake (it is < 1000 triangles).
    finish(mat, M_MERCURY, (unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, unreal.MaterialUsage.MATUSAGE_NANITE))
    return mat


# --- M_AcPuff ----------------------------------------------------------------

PUFF_HLSL = """
// A sprite on the engine plane (UV 0-1), turned to the eye by the C++.
// Star = 0: a soft round puff (lib.smoke); Star = 1: a glint, a small bright
// core with four thin rays (MaterialLibrary.glint).
float2 p = (UV - 0.5) * 2.0;
float d = length(p);
float soft = pow(saturate(1.0 - d), Sharp);
float rays = saturate(1.0 - abs(p.x) * 10.0) * saturate(1.0 - abs(p.y))
           + saturate(1.0 - abs(p.y) * 10.0) * saturate(1.0 - abs(p.x));
float star = max(pow(saturate(1.0 - d * 2.5), 2.0), rays * rays);
return C.rgb * C.a * lerp(soft, star, Star);
"""


def build_puff():
    mat, g = new_master(M_PUFF, unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", True)
    rgba = []
    for i, d in enumerate((1.0, 1.0, 1.0, 1.0)):
        rgba.append(g.node(unreal.MaterialExpressionPerInstanceCustomData, data_index=i, const_default_value=d))
    a3 = g.node(unreal.MaterialExpressionAppendVector)
    g.link(rgba[0], "", a3, "A")
    g.link(rgba[1], "", a3, "B")
    a4 = g.node(unreal.MaterialExpressionAppendVector)
    g.link(a3, "", a4, "A")
    g.link(rgba[2], "", a4, "B")
    a5 = g.node(unreal.MaterialExpressionAppendVector)
    g.link(a4, "", a5, "A")
    g.link(rgba[3], "", a5, "B")
    c = custom(g, "Sprite", PUFF_HLSL, [
        ("C", a5, ""),
        ("UV", g.node(unreal.MaterialExpressionTextureCoordinate), ""),
        ("Sharp", g.scalar("Sharp", 2.0), ""),
        ("Star", g.scalar("Star", 0.0), ""),
    ])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat, M_PUFF, (unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,))
    for path, sharp, star in ((MI_VAPOUR, 1.6, 0.0), (MI_GLINT, 1.0, 1.0)):
        mi = MM.load(path) or MM.create(path, unreal.MaterialInstanceConstant,
                                        unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, mat)
        MEL.set_material_instance_scalar_parameter_value(mi, "Sharp", sharp)
        MEL.set_material_instance_scalar_parameter_value(mi, "Star", star)
        EAL.set_metadata_tag(mi, "AcMadeBy", "Tools/Editor/make_resource_materials.py")
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
    return mat


# --- re-parenting --------------------------------------------------------------

def reparent(info, masters):
    n = 0
    for key, mat in sorted(info["materials"].items()):
        target = M_OPAL if is_opal(mat) else M_MERCURY if is_pool(mat) else None
        if target is None:
            continue
        mi = MM.load(A.mi_asset_path(mat))
        if mi is None:
            log("no instance yet for %s (%s): run make_materials.py" % (mat["name"], key))
            continue
        MEL.set_material_instance_parent(mi, masters[target])
        if target == M_MERCURY:
            MEL.set_material_instance_scalar_parameter_value(mi, "Pool", 1.0)
        EAL.set_metadata_tag(mi, "AcMadeBy", "Tools/Editor/make_materials.py + make_resource_materials.py")
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        log("%s → %s" % (A.mi_asset_path(mat), target))
        n += 1
    return n


def tint_well_halo(info):
    """The well's rim halo: SceneKit multiplies its emission by (0.62, 0.8, 1)
    (`ModelLibrary.well`, `halo.multiply`), which the export leaves out, so
    the halo came out white."""
    keys = set()
    for m in info["canonical"]:
        if m["name"].startswith("well_"):
            keys |= {x["key"] for x in m["materials"] if x["blend"] == "additive"}
    for key in sorted(keys):
        mat = info["materials"][key]
        mi = MM.load(A.mi_asset_path(mat))
        if mi is None:
            continue
        i = mat.get("emissiveIntensity", 1)
        MEL.set_material_instance_vector_parameter_value(mi, "EmissiveColor", MM.lc((0.62 * i, 0.8 * i, 1.0 * i)))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        log("%s: halo tint" % A.mi_asset_path(mat))


def main(info=None):
    MM.ensure_dir(A.MASTER_DIR)
    mpc = make_world_mpc()
    masters = {M_OPAL: build_opal(mpc), M_MERCURY: build_mercury(mpc)}
    build_puff()
    # The instances keep their parents once set: without the model export's
    # manifest (a worktree has none) only the masters are rebuilt.
    if info is None and not os.path.exists(A.MANIFEST):
        log("no %s: masters rebuilt, instances left as they are" % A.MANIFEST)
        return
    info = info or A.analyse(A.load_manifest())
    log("%d model instances re-parented" % reparent(info, masters))
    tint_well_halo(info)


if __name__ == "__main__":
    main()
