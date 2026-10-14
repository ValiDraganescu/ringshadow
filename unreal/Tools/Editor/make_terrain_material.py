"""M_Terrain and its textures (GAME-LAYER.md §2.6 "Terrain", chunk A2).

Rerunnable: reimports the textures over the old ones and rebuilds the
material's graph from nothing each time.

Inputs: unreal/Content-src/terrain/*.png, made by
`uv run unreal/Tools/terrain_textures.py` (the Swift game's ground textures
plus baked normal maps).

Outputs:
  /Game/Terrain/Textures/T_GroundDirt, _Grass, _Highland, T_CliffRock,
      T_BasePlating, and each one's _N normal map
  /Game/Terrain/M_Terrain

The material is the Swift ground shader (`TerrainBuilder.surfaceShader`,
Sources/Autocraft/Terrain.swift) as one Custom HLSL node, so the two read
side by side. AAcTerrain (Source/Autocraft/AcTerrain.cpp) sets its
parameters: `Splat` (the RGBA splat map: grass, highland, plating, scorch),
`Bounds` (minX, minY, width, depth of the map, cells) and `Haze` (rgb, and
the border's width in cells). On top of the Swift look: normal maps on every
ground texture and on the cliff rock, roughness that follows the texture,
and a faint large-scale tint variation, all in the Swift palette. And the
weather's wet ground (docs/weather.md; AAcDaylight sets `Wetness` and
`RainRate`): darker, glossier, puddles on flat ground with rain rings.

Run it in the editor (Tools > Execute Python Script, or `py <path>` in the
console), or headless when the live editor does not have these assets open:
  UnrealEditor-Cmd Autocraft.uproject -run=pythonscript -script=<abs path>
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Content-src", "terrain")
TEX_DIR = "/Game/Terrain/Textures"
MAT_DIR = "/Game/Terrain"
MAT_NAME = "M_Terrain"

ALBEDO = ["T_GroundDirt", "T_GroundGrass", "T_GroundHighland", "T_CliffRock", "T_BasePlating"]

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary


def import_textures():
    # The source PNGs went with the Swift game's exporter: without them the
    # textures already imported are used as they are.
    if not os.path.isdir(SRC):
        out = {}
        for name in ALBEDO:
            for suffix in ("", "_N"):
                tex = unreal.load_asset(f"{TEX_DIR}/{name}{suffix}")
                if tex is None:
                    raise RuntimeError(f"no source and no asset: {TEX_DIR}/{name}{suffix}")
                out[name + suffix] = tex
        return out
    tasks = []
    for name in ALBEDO:
        for suffix in ("", "_N"):
            task = unreal.AssetImportTask()
            task.filename = os.path.join(SRC, name + suffix + ".png")
            task.destination_path = TEX_DIR
            task.destination_name = name + suffix
            task.replace_existing = True
            task.automated = True
            task.save = False
            tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    out = {}
    for name in ALBEDO:
        for suffix in ("", "_N"):
            path = f"{TEX_DIR}/{name}{suffix}"
            tex = unreal.load_asset(path)
            if tex is None:
                raise RuntimeError(f"import failed: {path}")
            if suffix == "_N":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
            else:
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                tex.set_editor_property("srgb", True)
            tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
            tex.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            tex.set_editor_property("max_texture_size", 0)
            # Always on screen: no streaming (sharp from the first frame).
            tex.set_editor_property("never_stream", True)
            assets.save_asset(path, only_if_is_dirty=False)
            out[name + suffix] = tex
    return out


# The Swift surface shader, in Unreal's axes: Swift's ground plane (x, z) is
# Unreal's (X, Y) and its height y is Unreal's Z; everything below is in
# cells (Unreal centimetres / 100).
HLSL = r"""
float3 wp = WP / 100.0;
float3 wn = normalize(VN);
float2 g = wp.xy;
float2 suv = (g - Bounds.xy) / Bounds.zw;
float4 splat = Texture2DSampleLevel(Splat, SplatSampler, suv, 0);

// Two scales per texture break up visible tiling.
float2 t1 = g / 7.0;
float2 t2 = float2(g.x * 0.7 - g.y * 0.7, g.x * 0.7 + g.y * 0.7) / 17.0 + 0.37;
float2 tp = g / 3.5;
float3 dirt = lerp(Texture2DSample(Dirt, DirtSampler, t1).rgb, Texture2DSample(Dirt, DirtSampler, t2).rgb, 0.4);
float3 grass = lerp(Texture2DSample(Grass, GrassSampler, t1 * 1.3).rgb, Texture2DSample(Grass, GrassSampler, t2).rgb, 0.35);
float3 high = lerp(Texture2DSample(High, HighSampler, t1 * 0.9).rgb, Texture2DSample(High, HighSampler, t2).rgb, 0.3);
high = lerp(pow(max(high, 0.0), 1.5), dirt * 1.15, 0.3) * float3(0.8, 0.7, 0.6);
float3 plate = Texture2DSample(Plate, PlateSampler, tp).rgb;

dirt *= float3(0.78, 0.7, 0.64);
grass *= float3(0.85, 0.9, 0.8);
float3 c = dirt;
c = lerp(c, high, splat.g);
c = lerp(c, grass, splat.r);
c = lerp(c, plate, splat.b);
c *= 1.0 - splat.a * 0.5;

// Richer look: a faint tint variation over tens of cells, so wide plains do
// not read as one repeated tile (kept within +-6%, the palette stays).
float macroV = Texture2DSample(High, HighSampler, g / 61.0 + 0.23).g;
c *= lerp(0.94, 1.06, saturate(macroV * 1.6 - 0.3));

// Cliff rock on steep faces, projected sideways (triplanar on X and Y).
float slope = 1.0 - wn.z;
float rockW = smoothstep(0.22, 0.45, slope);
float2 ux = float2(wp.y, -wp.z * 1.4) / 5.0;
float2 uy = float2(wp.x, -wp.z * 1.4) / 5.0;
float3 rx = Texture2DSample(Rock, RockSampler, ux).rgb;
float3 ry = Texture2DSample(Rock, RockSampler, uy).rgb;
float ax = abs(wn.x), ay = abs(wn.y);
float3 rock = (rx * ax + ry * ay) / max(ax + ay, 1e-3);
c = lerp(c, rock * 0.95, rockW);

// Darken low ground a touch and cliff bases, like baked AO.
c *= lerp(0.88, 1.0, saturate(wp.z / 2.4));

// Past the map's edge (the border scenery, out of play): wild highland and
// rock, dimmer, fading into the haze further out, with a faint dark seam
// along the edge itself. Haze.w is how far the border reaches.
float2 lo = Bounds.xy, hi = Bounds.xy + Bounds.zw;
float past = length(max(max(lo - g, g - hi), 0.0));
float inside = min(min(g.x - lo.x, hi.x - g.x), min(g.y - lo.y, hi.y - g.y));
float edge = past > 0.0 ? past : max(inside, 0.0);
c *= 1.0 - 0.3 * (1.0 - smoothstep(0.0, 0.9, edge));
float fade = 0.0;
if (past > 0.0) {
    float3 wild = lerp(lerp(dirt, high, 0.65), rock * 0.95, rockW);
    c = lerp(c, wild, smoothstep(0.0, 1.5, past));
    c *= lerp(1.0, 0.72, smoothstep(0.0, 2.0, past));
    fade = smoothstep(Haze.w * 0.3, Haze.w * 0.9, past);
    splat = lerp(splat, float4(0, 0, 0, 0), smoothstep(0.0, 1.5, past));
}

// The region's ground palette (docs/regions.md, "ground"; AcGroundLook.cpp
// sets the parameters): GroundTint rgb the ground's colour, a how much of
// it; GroundFx (salt crust, glass facets, slag embers, ochre bands). Within
// 16 cells of an edge the palette goes halfway to the neighbour's
// (NbTint and NbFx N, E, S, W, laid out as the own ones), so both sides of
// an edge meet in one colour, and past the edge it is the neighbour's.
// Neighbours says which edges have one (N, E, S, W: 1 or 0).
float4 gt = GroundTint;
float4 gf = GroundFx;
{
    float4 ds = float4(g.y - lo.y, hi.x - g.x, hi.y - g.y, g.x - lo.x);
    float4 inner = 0.5 * (1.0 - smoothstep(0.0, 16.0, ds));
    float4 outer = 0.5 + 0.5 * smoothstep(0.0, 10.0, -ds);
    float4 w = Neighbours * (ds > 0.0 ? inner : outer);
    gt = lerp(gt, NbTintN, w.x); gf = lerp(gf, NbFxN, w.x);
    gt = lerp(gt, NbTintE, w.y); gf = lerp(gf, NbFxE, w.y);
    gt = lerp(gt, NbTintS, w.z); gf = lerp(gf, NbFxS, w.z);
    gt = lerp(gt, NbTintW, w.w); gf = lerp(gf, NbFxW, w.w);
}
float crack = 0.0, plateV = 0.5, facet = 0.0;
float2 facetTilt = float2(0.0, 0.0);
float3 ember = float3(0.0, 0.0, 0.0);
if (gt.a > 0.001) {
    float lumC = dot(c, float3(0.3, 0.59, 0.11));
    // The texture's own light and dark round the palette's colour.
    // The plain dirt's luminance is about 0.3 at the textures' middle.
    float detail = clamp(lumC / 0.3, 0.45, 1.6);
    float k = gt.a * (1.0 - splat.b) * (1.0 - 0.35 * rockW);
    c = lerp(c, gt.rgb * detail, k);
    // Ochre bands: strata by height, strongest on the cliffs.
    if (gf.w > 0.001) {
        float bh = wp.z / 0.55 + 0.6 * sin(g.x * 0.13) + 0.5 * sin(g.y * 0.11 + 1.7);
        float bandI = frac(bh / 3.0);
        float3 bandC = bandI < 0.33 ? float3(1.18, 1.0, 0.78) : (bandI < 0.66 ? float3(0.86, 0.66, 0.5) : float3(1.05, 0.82, 0.55));
        bandC = lerp(float3(1, 1, 1), bandC, smoothstep(0.0, 0.08, abs(frac(bh) - 0.5)) * 0.6 + 0.4);
        c *= lerp(float3(1, 1, 1), bandC, gf.w * lerp(0.55, 1.0, rockW));
    }
    // Plates and cracks (salt crust, glass facets, slag): one cell pattern.
    if (gf.x + gf.y + gf.z > 0.001) {
        float2 vp = g / 1.8;
        float2 vi = floor(vp), vf = frac(vp);
        float f1 = 8.0, f2 = 8.0;
        float2 id1 = vi;
        for (int yy = -1; yy <= 1; yy++)
            for (int xx = -1; xx <= 1; xx++) {
                float2 o = float2(xx, yy);
                float2 h2 = frac(sin(float2(dot(vi + o, float2(127.1, 311.7)), dot(vi + o, float2(269.5, 183.3)))) * 43758.5453);
                float d = length(o + h2 - vf);
                if (d < f1) { f2 = f1; f1 = d; id1 = vi + o; } else if (d < f2) { f2 = d; }
            }
        float edgeD = f2 - f1;
        crack = 1.0 - smoothstep(0.03, 0.11, edgeD);
        plateV = frac(sin(dot(id1, float2(41.3, 289.1))) * 15731.743);
        // Salt: pale plates, each a shade apart, grey brine in the cracks.
        c = lerp(c, c * lerp(0.92, 1.08, plateV), gf.x);
        c = lerp(c, c * float3(0.45, 0.47, 0.5), gf.x * crack * (1.0 - rockW));
        // Glass: each facet tilted its own way and glossy; a few catch the
        // sun as glints.
        facet = gf.y * (1.0 - rockW);
        facetTilt = (frac(float2(plateV * 7.13, plateV * 3.71)) - 0.5) * 0.9;
        c = lerp(c, c * lerp(0.7, 1.25, plateV), facet * 0.6);
        // Slag: black, cracks that glow like embers, breathing slowly.
        float slag = gf.z * (1.0 - rockW * 0.7);
        c = lerp(c, c * 0.55, slag * crack);
        float breathe = 0.65 + 0.35 * sin(T * 0.6 + plateV * 6.28);
        ember = float3(1.0, 0.3, 0.05) * slag * crack * breathe * EmberGain;
    }
    // Frost (Cold Fork, from night to mid-morning): rime on flat ground
    // and the tops of rocks, in patches.
    if (Frost > 0.001) {
        float fz = Texture2DSample(High, HighSampler, g / 9.0 + 0.71).g;
        float rime = smoothstep(0.35, 0.6, fz + (wn.z - 0.8)) * Frost;
        c = lerp(c, float3(0.62, 0.68, 0.76), rime * 0.8);
    }
}

// Normals (world space). Ground: the textures' normal maps, projected from
// above (tangent +X, bitangent +Y), blended as the colours are.
float3 nd = Texture2DSample(DirtN, DirtNSampler, t1).xyz * 2.0 - 1.0;
float3 nh = Texture2DSample(HighN, HighNSampler, t1 * 0.9).xyz * 2.0 - 1.0;
float3 ng = Texture2DSample(GrassN, GrassNSampler, t1 * 1.3).xyz * 2.0 - 1.0;
float3 np = Texture2DSample(PlateN, PlateNSampler, tp).xyz * 2.0 - 1.0;
float2 dn = nd.xy;
dn = lerp(dn, nh.xy, splat.g);
dn = lerp(dn, ng.xy, splat.r);
dn = lerp(dn, np.xy, splat.b);
float3 nTop = normalize(wn + float3(dn, 0.0) * Detail);
// Cliffs: the rock's normal map on the same sideways projections (U along
// +Y or +X, V down the face).
float3 nrx = Texture2DSample(RockN, RockNSampler, ux).xyz * 2.0 - 1.0;
float3 nry = Texture2DSample(RockN, RockNSampler, uy).xyz * 2.0 - 1.0;
float3 dRock = (float3(0.0, nrx.x, -nrx.y) * ax + float3(nry.x, 0.0, -nry.y) * ay) / max(ax + ay, 1e-3);
float3 nRock = normalize(wn + dRock * Detail * 1.3);
float3 n = normalize(lerp(nTop, nRock, rockW));

// Wet ground (AcWeather, docs/weather.md: `Wetness` 0-1 lags the rain,
// `RainRate` 0-1): darker and glossier, steep rock and the border less;
// puddles spread over flat ground as it soaks, mirror-smooth, holding the
// sky; rings where the drops land, more of them in heavier rain.
float wet = Wetness * (1.0 - fade) * (1.0 - rockW * 0.6);
float puddle = 0.0;
if (wet > 0.0) {
    // Puddles: two octaves of value noise (a known spread, so a known
    // share), 1-3 m patches on flat ground: about 15 % of it when damp,
    // 21 % soaked. They stay patches: they are what Lumen traces
    // reflections for, and puddles over most of the map cost 2-3 ms at 5K.
    float2 q0 = g / 2.2, q1 = g / 0.9 + 17.3;
    float2 i0 = floor(q0), f0 = frac(q0), i1 = floor(q1), f1 = frac(q1);
    f0 = f0 * f0 * (3.0 - 2.0 * f0);
    f1 = f1 * f1 * (3.0 - 2.0 * f1);
    #define AC_H(p) frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453)
    float n0 = lerp(lerp(AC_H(i0), AC_H(i0 + float2(1, 0)), f0.x), lerp(AC_H(i0 + float2(0, 1)), AC_H(i0 + float2(1, 1)), f0.x), f0.y);
    float n1 = lerp(lerp(AC_H(i1), AC_H(i1 + float2(1, 0)), f1.x), lerp(AC_H(i1 + float2(0, 1)), AC_H(i1 + float2(1, 1)), f1.x), f1.y);
    #undef AC_H
    float pn = n0 * 0.65 + n1 * 0.35;
    float level = 0.69 - 0.07 * Wetness;
    puddle = smoothstep(level, level + 0.04, pn) * smoothstep(0.92, 0.98, wn.z) * wet;
    c *= lerp(1.0, 0.6, wet);
    c *= lerp(1.0, 0.6, puddle);
    float2 rc = g / 0.35;
    float2 ci = floor(rc);
    float h = frac(sin(dot(ci, float2(127.1, 311.7))) * 43758.5453);
    float ph = frac(T * 1.1 + h * 7.0);
    float2 dd = frac(rc) - 0.5 - (float2(frac(h * 13.1), frac(h * 7.7)) - 0.5) * 0.4;
    float r = length(dd);
    float ring = (1.0 - ph) * (1.0 - smoothstep(0.0, 0.05, abs(r - ph * 0.42))) * step(h, RainRate);
    float2 rip = (r > 1e-3 ? dd / r : float2(0.0, 0.0)) * ring * 0.8;
    n = normalize(lerp(n, wn, puddle) + float3(rip * (puddle + wet * 0.25) * Ripples, 0.0));
}
n = normalize(n + float3(facetTilt * facet, 0.0));
Nrm = normalize(lerp(n, wn, fade));

// Roughness: the Swift value (plating is smoother and a little metallic),
// varied by the texture's own light and dark.
float lum = dot(c, float3(0.3, 0.59, 0.11));
Rough = saturate(lerp(0.92, 0.55, splat.b) + (0.35 - lum) * 0.25);
// The wet sheen stays above Lumen's traced-reflection roughness (0.4);
// only the puddles go below it.
Rough = lerp(lerp(Rough, 0.55, wet * Gloss), 0.22, puddle * Gloss);
// Glass facets: glossy (a share mirror-like, the glints), a touch metallic.
Rough = lerp(Rough, lerp(0.42, 0.22, step(0.78, plateV)), facet * (1.0 - wet));
Metal = splat.b * 0.35 + facet * 0.15;
Emis = Haze.rgb * fade * HazeGain + ember * (1.0 - fade);
return c * (1.0 - fade);
"""


def build_material(tex):
    path = f"{MAT_DIR}/{MAT_NAME}"
    mat = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("tangent_space_normal", False)
    mat.set_editor_property("two_sided", False)

    def node(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    custom = node(unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("description", "Swift ground shader")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    outputs = []
    for name, kind in (("Rough", "CMOT_FLOAT1"), ("Metal", "CMOT_FLOAT1"),
                       ("Emis", "CMOT_FLOAT3"), ("Nrm", "CMOT_FLOAT3")):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", getattr(unreal.CustomMaterialOutputType, kind))
        outputs.append(o)
    custom.set_editor_property("additional_outputs", outputs)

    # Inputs, in order: (input name, source node).
    wp = node(unreal.MaterialExpressionWorldPosition, -900, -400)
    vn = node(unreal.MaterialExpressionVertexNormalWS, -900, -340)
    sources = [("WP", wp), ("VN", vn)]
    y = -260

    def tex_param(input_name, param, texture, sampler):
        nonlocal y
        t = node(unreal.MaterialExpressionTextureObjectParameter, -900, y)
        y += 70
        t.set_editor_property("parameter_name", param)
        t.set_editor_property("texture", texture)
        t.set_editor_property("sampler_type", sampler)
        sources.append((input_name, t))

    color = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    normal = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    linear = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    tex_param("Splat", "Splat", unreal.load_asset("/Engine/EngineResources/Black"), linear)
    tex_param("Dirt", "Dirt", tex["T_GroundDirt"], color)
    tex_param("Grass", "Grass", tex["T_GroundGrass"], color)
    tex_param("High", "Highland", tex["T_GroundHighland"], color)
    tex_param("Rock", "Rock", tex["T_CliffRock"], color)
    tex_param("Plate", "Plating", tex["T_BasePlating"], color)
    tex_param("DirtN", "DirtNormal", tex["T_GroundDirt_N"], normal)
    tex_param("GrassN", "GrassNormal", tex["T_GroundGrass_N"], normal)
    tex_param("HighN", "HighlandNormal", tex["T_GroundHighland_N"], normal)
    tex_param("RockN", "RockNormal", tex["T_CliffRock_N"], normal)
    tex_param("PlateN", "PlatingNormal", tex["T_BasePlating_N"], normal)

    def vec_param(name, value):
        nonlocal y
        v = node(unreal.MaterialExpressionVectorParameter, -900, y)
        y += 70
        v.set_editor_property("parameter_name", name)
        v.set_editor_property("default_value", unreal.LinearColor(*value))
        return v

    def scalar_param(name, value):
        nonlocal y
        s = node(unreal.MaterialExpressionScalarParameter, -900, y)
        y += 70
        s.set_editor_property("parameter_name", name)
        s.set_editor_property("default_value", value)
        return s

    # Defaults: a 200 x 128 map centred on the origin, the Swift haze.
    sources.append(("Bounds", vec_param("Bounds", (-100.0, -64.0, 200.0, 128.0))))
    sources.append(("Haze", vec_param("Haze", (0.27, 0.22, 0.19, 90.0))))
    sources.append(("Detail", scalar_param("DetailNormal", 0.55)))
    sources.append(("HazeGain", scalar_param("HazeGain", 1.0)))
    # The weather (AAcDaylight sets them): docs/weather.md.
    sources.append(("Wetness", scalar_param("Wetness", 0.0)))
    sources.append(("RainRate", scalar_param("RainRate", 0.0)))
    sources.append(("Ripples", scalar_param("Ripples", 1.0)))
    sources.append(("Gloss", scalar_param("Gloss", 1.0)))
    # The region's ground palette (AcGroundLook.cpp): plain by default.
    sources.append(("GroundTint", vec_param("GroundTint", (0.2, 0.15, 0.1, 0.0))))
    sources.append(("GroundFx", vec_param("GroundFx", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("Frost", scalar_param("Frost", 0.0)))
    sources.append(("EmberGain", scalar_param("EmberGain", 1.0)))
    sources.append(("Neighbours", vec_param("Neighbours", (0.0, 0.0, 0.0, 0.0))))
    for e in "NESW":
        sources.append((f"NbTint{e}", vec_param(f"NbTint{e}", (0.2, 0.15, 0.1, 0.0))))
        sources.append((f"NbFx{e}", vec_param(f"NbFx{e}", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("T", node(unreal.MaterialExpressionTime, -900, y)))

    inputs = []
    for name, _ in sources:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    for name, src in sources:
        # Vector parameters: the RGBA output (index 0 is RGB).
        out = ""
        if isinstance(src, unreal.MaterialExpressionVectorParameter):
            out = "RGBA"
        if not mel.connect_material_expressions(src, out, custom, name):
            raise RuntimeError(f"could not connect {name}")

    P = unreal.MaterialProperty
    for out, prop in (("", P.MP_BASE_COLOR), ("Rough", P.MP_ROUGHNESS), ("Metal", P.MP_METALLIC),
                      ("Emis", P.MP_EMISSIVE_COLOR), ("Nrm", P.MP_NORMAL)):
        if not mel.connect_material_property(custom, out, prop):
            raise RuntimeError(f"could not connect output {out or 'main'}")

    mel.recompile_material(mat)
    assets.save_asset(path, only_if_is_dirty=False)
    return mat


def main():
    tex = import_textures()
    mat = build_material(tex)
    unreal.log(f"make_terrain_material: built {mat.get_path_name()}")


main()
