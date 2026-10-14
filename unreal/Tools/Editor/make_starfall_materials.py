"""The starfall's materials (docs/weather.md, "Starfall"; docs/mmorpg-m7.md
E2), drawn by AAcStarfall (Source/Autocraft/AcStarfall.cpp). Rerunnable:
each graph is rebuilt from nothing.

- /Game/Materials/M_StarfallGlow: unlit, additive, on a sphere: the streak's
  head, halo and tail, the impact's flash, the embers. A soft ball of light,
  brightest face on. Per-instance custom data: 0 intensity, 1-3 colour.
- /Game/Materials/M_StarfallSmoke: unlit, translucent, on a sphere: the
  trail's puffs, the dust ring, the smoke column. Soft-edged, broken up by
  noise, darkened by the night (MPC_AcWorld.Dark), lit by its own fire
  while warm. Per-instance custom data: 0 opacity, 1 brightness, 2 warmth.
- /Game/Materials/M_StarfallCrater: a deferred decal, translucent: the
  crater, a scorched pit, a pale rim of thrown rock and rays of dust; its
  floor cracked with glowing seams while `Heat` lasts; `Fade` 1 to 0 over
  the crater's life; `Seed` turns its rays.
- /Game/Materials/M_OpalHot: the starfall's deposit, lit: Stardust Ore
  fresh from the fire, a dark cooling crust, matte, cracked by molten veins
  (gold at their hearts, orange, deep red at their edges), breathing,
  brighter at night (MPC_AcWorld.Dark), with sparks in the cracks. Kept
  low enough that the tonemapper leaves it orange, not white.
- /Game/Materials/M_StarfallShimmer: translucent, unlit, on a sphere: heat
  shimmer over a hot deposit by day, the scene behind it bent by a slow
  drifting noise (2D-offset refraction), nothing drawn of its own.
  Per-instance custom data: 0 strength.

Run in a commandlet (no open editor has these assets):
  UnrealEditor-Cmd unreal/Autocraft.uproject -run=pythonscript \\
      -script="$PWD/unreal/Tools/Editor/make_starfall_materials.py" -unattended -nullrhi
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
M_GLOW = A.MASTER_DIR + "/M_StarfallGlow"
M_SMOKE = A.MASTER_DIR + "/M_StarfallSmoke"
M_CRATER = A.MASTER_DIR + "/M_StarfallCrater"
M_HOT = A.MASTER_DIR + "/M_OpalHot"
M_SHIMMER = A.MASTER_DIR + "/M_StarfallShimmer"

F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
ISM = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES


def log(msg):
    unreal.log("[make_starfall_materials] " + msg)


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


def new_master(path, shading, blend, domain=unreal.MaterialDomain.MD_SURFACE):
    mat = MM.load(path) or MM.create(path, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property("material_domain", domain)
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("two_sided", False)
    return mat, MM.Graph(mat)


def finish(mat, path, usages):
    for usage in usages:
        MEL.set_base_material_usage(mat, usage)
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_starfall_materials.py")
    EAL.save_loaded_asset(mat)
    log("master %s" % path)


NOISE = """
struct AcSf
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
    float Fbm(float3 p)
    {
        return 0.55 * Noise(p) + 0.3 * Noise(p * 2.1 + 7.0) + 0.15 * Noise(p * 4.3 + 13.0);
    }
};
AcSf o;
"""

# --- M_StarfallGlow -------------------------------------------------------------

GLOW_HLSL = """
// A soft ball of light: brightest face on, nothing at the rim.
float ndv = saturate(dot(normalize(N), normalize(V)));
return C * I * pow(ndv, 2.2);
"""


def build_glow():
    mat, g = new_master(M_GLOW, unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_ADDITIVE)
    col = g.node(unreal.MaterialExpressionAppendVector)
    rg = g.node(unreal.MaterialExpressionAppendVector)
    g.link(g.custom_data(1, "R", 1.0), "", rg, "A")
    g.link(g.custom_data(2, "G", 0.6), "", rg, "B")
    g.link(rg, "", col, "A")
    g.link(g.custom_data(3, "B", 0.2), "", col, "B")
    c = custom(g, "Starfall glow", GLOW_HLSL, [
        ("N", g.node(unreal.MaterialExpressionVertexNormalWS), ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("C", col, ""),
        ("I", g.custom_data(0, "Intensity", 1.0), ""),
    ])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat, M_GLOW, (ISM,))


# --- M_StarfallSmoke ------------------------------------------------------------

SMOKE_HLSL = NOISE + """
// A puff: soft at the rim, broken up by noise that drifts, grey dust by day
// and near black at night, glowing with its own fire while warm.
float ndv = saturate(dot(normalize(N), normalize(V)));
float n = o.Fbm(WP / 260.0 + float3(0, 0, T * 0.15));
Alpha = saturate(Op * pow(ndv, 1.4) * smoothstep(0.25, 0.75, n + 0.25 * ndv));
float3 dust = float3(0.30, 0.27, 0.24) * lerp(1.0, 0.1, Dark) * Bright;
float3 fire = float3(1.0, 0.42, 0.12) * 5.0 * (0.6 + 0.8 * n);
return lerp(dust, fire, saturate(Warm));
"""


def build_smoke():
    mat, g = new_master(M_SMOKE, unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
    mpc = MM.load(MPC_WORLD)
    c = custom(g, "Starfall smoke", SMOKE_HLSL, [
        ("N", g.node(unreal.MaterialExpressionVertexNormalWS), ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("WP", g.node(unreal.MaterialExpressionWorldPosition), ""),
        ("T", g.node(unreal.MaterialExpressionTime), ""),
        ("Dark", g.mpc(mpc, "Dark"), ""),
        ("Op", g.custom_data(0, "Opacity", 0.5), ""),
        ("Bright", g.custom_data(1, "Brightness", 1.0), ""),
        ("Warm", g.custom_data(2, "Warmth", 0.0), ""),
    ], outputs=[("Alpha", F1)])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, "Alpha", unreal.MaterialProperty.MP_OPACITY)
    finish(mat, M_SMOKE, (ISM,))


# --- M_StarfallCrater -----------------------------------------------------------

CRATER_HLSL = NOISE + """
// The decal's square is 2.6 crater radii either side: r = 1 at the rim.
float2 p = (UV - 0.5) * 2.0 * 2.6;
float r = length(p);
float a = atan2(p.y, p.x);
float3 q = float3(p * 3.0, Seed);
float n = o.Fbm(q);
float wob = (o.Noise(float3(a * 2.5, Seed, 1.0)) - 0.5) * 0.25;
float rr = r + wob;
// The pit: ash and glassy black, the rim a ring of pale thrown rock.
float pit = 1.0 - smoothstep(0.75, 1.0, rr);
float rim = smoothstep(0.8, 1.0, rr) * (1.0 - smoothstep(1.05, 1.45, rr));
// Rays of dust thrown out past the rim.
float ray = pow(saturate(o.Noise(float3(a * 7.0, Seed * 1.7, 3.0)) * 1.25 - 0.1), 3.0);
float rays = ray * smoothstep(1.0, 1.3, rr) * (1.0 - smoothstep(1.6, 2.5, r + ray * 0.6));
float3 ash = lerp(float3(0.012, 0.010, 0.009), float3(0.05, 0.042, 0.035), n);
float3 dust = lerp(float3(0.20, 0.17, 0.14), float3(0.34, 0.30, 0.25), n);
float3 scorch = float3(0.03, 0.025, 0.02);
float3 base = ash * pit + dust * rim + lerp(scorch, dust, 0.55) * rays * (1.0 - pit) * (1.0 - rim);
Base = base / max(pit + rim + rays * (1.0 - pit) * (1.0 - rim), 1e-3);
Alpha = saturate(pit + rim * 0.9 + rays * 0.75 + (1.0 - smoothstep(1.0, 2.0, r)) * 0.35 * n) * Fade;
Rough = lerp(0.95, 0.55, pit);
// Hot seams in the floor while it cools.
float cracks = 1.0 - smoothstep(0.0, 0.06, abs(o.Fbm(float3(p * 4.0, Seed + 5.0)) - 0.5));
float core = 1.0 - smoothstep(0.0, 0.85, rr);
float3 hot = lerp(float3(0.9, 0.16, 0.025), float3(1.0, 0.48, 0.1), core);
return hot * (cracks * 0.8 + core * 0.25) * pit * Heat * 1.6 * Fade;
"""


def build_crater():
    mat, g = new_master(M_CRATER, unreal.MaterialShadingModel.MSM_DEFAULT_LIT, unreal.BlendMode.BLEND_TRANSLUCENT,
                        domain=unreal.MaterialDomain.MD_DEFERRED_DECAL)
    # A translucent decal: the engine takes its outputs from the pins wired.
    c = custom(g, "Starfall crater", CRATER_HLSL, [
        ("UV", g.node(unreal.MaterialExpressionTextureCoordinate), ""),
        ("Seed", g.scalar("Seed", 0.0), ""),
        ("Fade", g.scalar("Fade", 1.0), ""),
        ("Heat", g.scalar("Heat", 0.0), ""),
    ], outputs=[("Base", F3), ("Alpha", F1), ("Rough", F1)])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, "Base", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(c, "Alpha", unreal.MaterialProperty.MP_OPACITY)
    g.out(c, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat, M_CRATER, ())


# --- M_OpalHot ------------------------------------------------------------------

HOT_HLSL = NOISE + """
// Stardust Ore fresh from the fire: a dark crust cracked by the opal's
// veins (isolines of a warped noise, M_Opal's way) running molten, gold at
// their hearts to orange to deep red at their edges, breathing; brighter at
// night. Never white: bright orange past the tonemapper's shoulder reads
// white, so the glow stays low and the colour deep.
float3 op = LP * 0.04;
float3 wv = float3(o.Noise(op * 0.55 + 11.0), o.Noise(op * 0.55 + 23.0), o.Noise(op * 0.55 + 37.0)) - 0.5;
float3 q = op * 0.75 + wv * 1.8;
float n1 = o.Noise(q);
float e1 = 0.03;
float3 gr = float3(o.Noise(q + float3(e1, 0, 0)), o.Noise(q + float3(0, e1, 0)), o.Noise(q + float3(0, 0, e1))) - n1;
float d1 = abs(n1 - 0.5) / max(length(gr) / e1, 0.15);
float n2 = o.Noise(q * 2.6 + 5.0);
float3 gr2 = float3(o.Noise(q * 2.6 + 5.0 + float3(e1, 0, 0)), o.Noise(q * 2.6 + 5.0 + float3(0, e1, 0)),
                    o.Noise(q * 2.6 + 5.0 + float3(0, 0, e1))) - n2;
float d2 = abs(n2 - 0.5) / max(length(gr2) / e1, 0.15);
float fp = Depth * 0.04 * 0.0016;
float vein = saturate(smoothstep(0.05 + fp, 0.012, d1) + 0.5 * smoothstep(0.035 + fp * 2.6, 0.01, d2));
// The heart of a vein runs gold, its edge orange to deep red.
float heart = saturate(smoothstep(0.03 + fp, 0.004, d1) + 0.4 * smoothstep(0.02 + fp * 2.6, 0.004, d2));
float halo = saturate(smoothstep(0.25, 0.03, d1) * 0.6 + smoothstep(0.15, 0.02, d2) * 0.3);
float t = saturate(n1 * 1.2 - 0.1 + 0.2 * sin(T * 0.9 + n2 * 6.0));
float3 edge = lerp(float3(1.0, 0.30, 0.04), float3(0.55, 0.06, 0.015), t);
float3 hot = lerp(edge, float3(1.0, 0.58, 0.14), heart);
float breathe = 0.8 + 0.2 * sin(T * 1.3 + n1 * 9.0);
float3 e = (hot * vein * 0.3 + float3(0.5, 0.05, 0.01) * halo * 0.02) * breathe;
// Sparks in the cracks: sparse points that flare and die.
float3 g = op * 5.0;
float3 cell = floor(g);
float h = o.Hash(cell + 0.37);
float3 at = cell + 0.5 + (float3(o.Hash(cell + 1.7), o.Hash(cell + 3.1), o.Hash(cell + 5.3)) - 0.5) * 0.5;
float flick = pow(saturate(sin(T * (2.0 + 3.0 * h) + h * 40.0)), 8.0);
e += float3(1.0, 0.5, 0.12) * step(0.7, h) * (1.0 - smoothstep(0.03, 0.1, length(g - at))) * flick * 1.2;
// The crust: matte, a faint sheen of glassy slag on the cooled veins.
Rough = lerp(0.92, 0.6, vein);
return e * (1.0 + 2.5 * Dark);
"""


def build_hot():
    mat, g = new_master(M_HOT, unreal.MaterialShadingModel.MSM_DEFAULT_LIT, unreal.BlendMode.BLEND_OPAQUE)
    mpc = MM.load(MPC_WORLD)
    g.out(g.vector("BaseColorTint", (0.022, 0.016, 0.012)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Metallic", 0.0), "", unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar("Specular", 0.2), "", unreal.MaterialProperty.MP_SPECULAR)
    c = custom(g, "Hot opal", HOT_HLSL, [
        ("LP", g.node(unreal.MaterialExpressionLocalPosition), ""),
        ("T", g.node(unreal.MaterialExpressionTime), ""),
        ("Dark", g.mpc(mpc, "Dark"), ""),
        ("Depth", g.node(unreal.MaterialExpressionPixelDepth), ""),
    ], outputs=[("Rough", F1)])
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(mat, M_HOT, (ISM, unreal.MaterialUsage.MATUSAGE_NANITE))


# --- M_StarfallShimmer -----------------------------------------------------------

SHIMMER_HLSL = NOISE + """
// Bent air: a screen offset from a noise rising through the column, strongest
// face on, nothing at the rim or the ends.
float ndv = saturate(dot(normalize(N), normalize(V)));
float3 q = WP / 90.0 - float3(0, 0, T * 0.9);
float a = o.Noise(q), b = o.Noise(q + float3(17.0, 5.0, 3.0));
float edge = pow(ndv, 1.5) * smoothstep(0.0, 0.6, UV.y) * smoothstep(1.0, 0.5, UV.y);
Alpha = 0.0;
return float2(a - 0.5, b - 0.5) * 0.012 * S * edge;
"""


def refraction_mode(name):
    for n in (name, name.replace("2D", "2_D"), "RM_2D_OFFSET", "RM2D_OFFSET"):
        if hasattr(unreal.RefractionMode, n):
            return getattr(unreal.RefractionMode, n)
    raise RuntimeError("no RefractionMode %s in %s" % (name, dir(unreal.RefractionMode)))


def build_shimmer():
    mat, g = new_master(M_SHIMMER, unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("refraction_method", refraction_mode("RM_2D_OFFSET"))
    c = custom(g, "Starfall shimmer", SHIMMER_HLSL, [
        ("N", g.node(unreal.MaterialExpressionVertexNormalWS), ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("WP", g.node(unreal.MaterialExpressionWorldPosition), ""),
        ("UV", g.node(unreal.MaterialExpressionTextureCoordinate), ""),
        ("T", g.node(unreal.MaterialExpressionTime), ""),
        ("S", g.custom_data(0, "Strength", 0.0), ""),
    ], outputs=[("Alpha", F1)], out_type=F2)
    g.out(c, "", unreal.MaterialProperty.MP_REFRACTION)
    g.out(c, "Alpha", unreal.MaterialProperty.MP_OPACITY)
    g.out(g.vector("Nothing", (0.0, 0.0, 0.0)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(mat, M_SHIMMER, (ISM,))


def main():
    MM.ensure_dir(A.MASTER_DIR)
    build_glow()
    build_smoke()
    build_crater()
    build_hot()
    build_shimmer()


if __name__ == "__main__":
    main()
