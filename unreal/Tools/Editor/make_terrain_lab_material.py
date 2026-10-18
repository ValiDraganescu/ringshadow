"""M_TerrainLab and its textures: the terrain lab's ground (docs/terrain-lab.md).

Rerunnable: reimports the slices over the old ones, remakes the two texture
arrays from them and rebuilds the material's graph from nothing each time.
The game's M_Terrain (make_terrain_material.py) is read, not touched: the
lab is drawn instead of it with `ac.TerrainLab 1` (AcTerrain.cpp).

Inputs: unreal/Content-src/terrain-lab/TL_<Layer>_CH.png and _NR.png, made
by `uv run unreal/Tools/terrain_lab_textures.py` (CC0 sets, listed in
unreal/Resources/Textures/lab/SOURCES.md), 2048 px:
  CH: rgb the colour (sRGB), a the height
  NR: rg the normal's x and y (DirectX), b the roughness, a the occlusion

Outputs:
  /Game/TerrainLab/Textures/Slices/T_TL_<Layer>_CH, _NR   the slices
  /Game/TerrainLab/Textures/T_TerrainLabCH, T_TerrainLabNR  the arrays
  /Game/TerrainLab/M_TerrainLab

The layers, in the arrays' order (AcGroundLook.cpp's `Layer` picks a
region's own): 0 Dirt (plain, and rust and ochre by their tint), 1 Grass,
2 Highland, 3 Rock (the cliffs), 4 Salt, 5 Ash, 6 Glass, 7 Slag, 8 Frost.

Round 1 (textures and tiling) over M_Terrain's shader: every layer is
sampled hex-tiled (three hex cells' samples, each turned and moved its own
random way, blended by the cells' weights sharpened by height), so no tile
repeats; a near scale and a far scale cross-fade by distance; a macro
colour noise over 30 to 80 m; sand ripples on the dirt, across the
region's x. Layers meet by height (the higher grain wins). The splat map,
the palettes' tint and effects, the wet ground, the haze and WallZ are
M_Terrain's, as are every parameter AcTerrain, AcDaylight and AcGroundLook
set.

Run it headless through the one hidden-run slot:
  uv run unreal/Tools/hidden_run.py --out unreal/Saved/TerrainLab --python unreal/Tools/Editor/make_terrain_lab_material.py
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Content-src", "terrain-lab")
LAB_DIR = "/Game/TerrainLab"
TEX_DIR = f"{LAB_DIR}/Textures"
SLICE_DIR = f"{TEX_DIR}/Slices"
MAT_NAME = "M_TerrainLab"

LAYERS = ["Dirt", "Grass", "Highland", "Rock", "Salt", "Ash", "Glass", "Slag", "Frost"]
# The base plating stays the game's (the pads' plates are not ground).
PLATING = "/Game/Terrain/Textures/T_BasePlating"

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_slices():
    tasks = []
    for layer in LAYERS:
        for kind in ("CH", "NR"):
            src = os.path.join(SRC, f"TL_{layer}_{kind}.png")
            if not os.path.isfile(src):
                raise RuntimeError(f"missing {src} (uv run unreal/Tools/terrain_lab_textures.py)")
            task = unreal.AssetImportTask()
            task.filename = src
            task.destination_path = SLICE_DIR
            task.destination_name = f"T_TL_{layer}_{kind}"
            task.replace_existing = True
            task.automated = True
            task.save = False
            tasks.append(task)
    tools.import_asset_tasks(tasks)
    out = {"CH": [], "NR": []}
    for layer in LAYERS:
        for kind in ("CH", "NR"):
            path = f"{SLICE_DIR}/T_TL_{layer}_{kind}"
            tex = unreal.load_asset(path)
            if tex is None:
                raise RuntimeError(f"import failed: {path}")
            # BC7 both: the colour sRGB (its alpha, the height, is linear
            # anyway), the normal, roughness and occlusion linear.
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
            tex.set_editor_property("srgb", kind == "CH")
            tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
            tex.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            tex.set_editor_property("max_texture_size", 0)
            assets.save_asset(path, only_if_is_dirty=False)
            out[kind].append(tex)
    return out


def make_array(name, slices, srgb):
    path = f"{TEX_DIR}/{name}"
    arr = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if arr is None:
        arr = tools.create_asset(name, TEX_DIR, unreal.Texture2DArray, unreal.Texture2DArrayFactory())
        if arr is None:
            raise RuntimeError(f"could not make {path}")
    # Setting the sources remakes the array's own source from them
    # (UTexture2DArray::PostEditChangeProperty).
    arr.set_editor_property("source_textures", slices)
    arr.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    arr.set_editor_property("srgb", srgb)
    arr.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    arr.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    arr.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    # Always on screen: no streaming (sharp from the first frame).
    arr.set_editor_property("never_stream", True)
    assets.save_asset(path, only_if_is_dirty=False)
    unreal.log(f"make_terrain_lab_material: {path}, {len(slices)} slices")
    return arr


# M_Terrain's shader (make_terrain_material.py) with round 1 on it. Unreal's
# axes, in cells (1 m): the ground plane is (X, Y), the height Z.
HLSL = r"""
// The lab's samplers, as functions (a Custom node's code is one function's
// body; a struct's static functions are the way to have more).
struct Lab
{
    static float2 Hash2(float2 p)
    {
        float2 r = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
        return frac(sin(r) * 43758.5453);
    }
    static float Noise(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        float a = Hash2(i).x, b = Hash2(i + float2(1, 0)).x, c = Hash2(i + float2(0, 1)).x, d = Hash2(i + float2(1, 1)).x;
        return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
    }
    // One layer, hex-tiled (stochastic tiling on a triangle grid): the three
    // hex cells round `uv` each take the texture turned and moved their own
    // way; their weights, sharpened by the samples' heights, blend them.
    // `ch`: colour and height; `nr`: the normal's xy in world X/Y (turned
    // back), roughness, occlusion.
    static void Hex(Texture2DArray CH, SamplerState CHs, Texture2DArray NR, SamplerState NRs, float2 uv, float2 dx, float2 dy,
                    float slice, float contrast, bool normals, out float4 ch, out float4 nr)
    {
        float2 st = uv * 3.4641016;
        float2 sk = float2(st.x - 0.57735027 * st.y, 1.15470054 * st.y);
        float2 base = floor(sk);
        float3 t = float3(frac(sk), 0.0);
        t.z = 1.0 - t.x - t.y;
        float s = step(0.0, -t.z);
        float s2 = 2.0 * s - 1.0;
        float3 w = float3(-t.z * s2, s - t.y * s2, s - t.x * s2);
        float2 v[3] = { base + float2(s, s), base + float2(s, 1.0 - s), base + float2(1.0 - s, s) };
        // Sharpen: mostly one cell's texture, the others only at the seams,
        // the higher grain on top where they meet. A cell that barely weighs
        // is not read at all (most pixels read one).
        float3 pw = pow(max(w, 1e-4), contrast);
        pw /= dot(pw, 1.0);
        ch = 0; nr = 0;
        float total = 0;
        [unroll] for (int k = 0; k < 3; k++)
        {
            [branch] if (pw[k] > 0.03)
            {
                float2 h = Hash2(v[k]);
                float a = h.x * 6.2831853;
                float ca = cos(a), sa = sin(a);
                float2x2 M = float2x2(ca, -sa, sa, ca);
                float2 u = mul(M, uv) + h * 7.31;
                float4 c = CH.SampleGrad(CHs, float3(u, slice), mul(M, dx), mul(M, dy));
                float4 n = float4(0.5, 0.5, 0.85, 1.0);
                if (normals) n = NR.SampleGrad(NRs, float3(u, slice), mul(M, dx), mul(M, dy));
                n.xy = mul(transpose(M), n.xy * 2.0 - 1.0);
                float g = pw[k] * (0.25 + c.a);
                ch += c * g; nr += n * g; total += g;
            }
        }
        ch /= max(total, 1e-5); nr /= max(total, 1e-5);
    }
    // The height alone of a hex-tiled layer (the parallax's steps): the same
    // cells, turns and weights as `Hex`, the height channel only.
    static float HexHeight(Texture2DArray CH, SamplerState CHs, float2 uv, float2 dx, float2 dy, float slice, float contrast)
    {
        float2 st = uv * 3.4641016;
        float2 sk = float2(st.x - 0.57735027 * st.y, 1.15470054 * st.y);
        float2 base = floor(sk);
        float3 t = float3(frac(sk), 0.0);
        t.z = 1.0 - t.x - t.y;
        float s = step(0.0, -t.z);
        float s2 = 2.0 * s - 1.0;
        float3 w = float3(-t.z * s2, s - t.y * s2, s - t.x * s2);
        float2 v[3] = { base + float2(s, s), base + float2(s, 1.0 - s), base + float2(1.0 - s, s) };
        float3 pw = pow(max(w, 1e-4), contrast);
        pw /= dot(pw, 1.0);
        float hs = 0, total = 0;
        [unroll] for (int k = 0; k < 3; k++)
        {
            [branch] if (pw[k] > 0.03)
            {
                float2 h = Hash2(v[k]);
                float a = h.x * 6.2831853;
                float ca = cos(a), sa = sin(a);
                float2x2 M = float2x2(ca, -sa, sa, ca);
                float hk = CH.SampleGrad(CHs, float3(mul(M, uv) + h * 7.31, slice), mul(M, dx), mul(M, dy)).a;
                float g = pw[k] * (0.25 + hk);
                hs += hk * g; total += g;
            }
        }
        return hs / max(total, 1e-5);
    }
    // A layer at both scales, cross-faded by distance (`near` 1: the near
    // scale only, 0: the far only; both sampled only in between).
    static void Layer(Texture2DArray CH, SamplerState CHs, Texture2DArray NR, SamplerState NRs, float2 g, float2 gdx, float2 gdy,
                      float slice, float nearTile, float farTile, float nearW, float contrast, out float4 ch, out float4 nr)
    {
        ch = 0; nr = 0;
        if (nearW > 0.001)
        {
            float4 a, b;
            Hex(CH, CHs, NR, NRs, g / nearTile, gdx / nearTile, gdy / nearTile, slice, contrast, true, a, b);
            ch += a * nearW; nr += b * nearW;
        }
        if (nearW < 0.999)
        {
            float4 a, b;
            // Far off, the colour alone: the normals' grain is under a pixel.
            Hex(CH, CHs, NR, NRs, g / farTile + 0.37, gdx / farTile, gdy / farTile, slice, contrast, false, a, b);
            ch += a * (1.0 - nearW); nr += b * (1.0 - nearW);
        }
    }
    // A layer's colour in the game's palette: `target` (rgb; a, how much)
    // carrying the texture's own light, dark and grain round its mean
    // (means.json, from terrain_lab_textures.py), `k` of it.
    static float3 Recolour(float3 mean, float3 c, float4 target, float k)
    {
        mean = max(mean, 1e-3);
        float3 graded = target.rgb * max(1.0 + (c / mean - 1.0) * k, 0.0);
        return lerp(c, graded, target.a);
    }
    // `b` over `a` by `t` (0-1), the higher grain first: a height blend.
    static float Over(float ha, float hb, float t, float sharp)
    {
        return saturate(((hb - ha) * 0.6 + (t * 2.0 - 1.0)) * sharp + 0.5) * step(0.001, t);
    }
};

float3 wp = WP / 100.0;
float3 wn = normalize(VN);
// World-grid cells (W4): Origin is the pilot's region's centre on the
// world's grid, so the ground's patterns stay put when the world's origin
// moves at a seamless crossing, and run on across the regions' edges.
float2 g = wp.xy + Origin.xy;
float2 gdx = ddx(g), gdy = ddy(g);
float2 suv = (g - Bounds.xy) / Bounds.zw;
float4 splat = Texture2DSampleLevel(Splat, SplatSampler, suv, 0);
float4 relief = Texture2DSampleLevel(Relief, ReliefSampler, suv, 0);
float dist = length(Cam / 100.0 - wp);
float nearW = 1.0 - smoothstep(NearFade.x, NearFade.y, dist);
float contrast = HexContrast;
float slope = 1.0 - wn.z;
float rockW = smoothstep(0.22, 0.45, slope);

// Round 2: parallax occlusion on the ground within ParallaxRange (start,
// end of its fade, m): the base layer's height marched along the view, the
// ground's every layer then read where the ray met it. Fades out to nothing
// before the range's end, so nothing pops; off on cliffs and plating.
{
    float3 V = normalize(Cam / 100.0 - wp);
    float depth = ParallaxDepth * (1.0 - smoothstep(ParallaxRange.x, ParallaxRange.y, dist)) * (1.0 - rockW) * (1.0 - splat.b);
    if (depth > 1e-4 && V.z > 0.08 && nearW > 0.001)
    {
        float steps = lerp(16.0, 8.0, V.z);
        float2 stepG = -V.xy / V.z * depth / steps;
        float2 off = 0;
        float level = 1.0;
        float2 dx = gdx / NearTile, dy = gdy / NearTile;
        float hNow = Lab::HexHeight(LabCH, LabCHSampler, g / NearTile, dx, dy, GroundLayer, contrast);
        float hBefore = hNow, levelBefore = level;
        [loop] for (int i = 0; i < 16 && hNow < level; i++)
        {
            hBefore = hNow; levelBefore = level;
            off += stepG;
            level -= 1.0 / steps;
            hNow = Lab::HexHeight(LabCH, LabCHSampler, (g + off) / NearTile, dx, dy, GroundLayer, contrast);
        }
        // Between the last two steps, where the ray crossed the height.
        float a = hNow - level, b = hBefore - levelBefore;
        float t = saturate(a / min(a - b, -1e-4));
        g += off - stepG * t;
    }
}

float lo2 = 0.0;
float2 lo = Bounds.xy, hi = Bounds.xy + Bounds.zw;

// The region's ground palette (docs/regions.md, "ground"; AcGroundLook.cpp
// sets the parameters), blended halfway to each neighbour's over the last
// 16 cells to an edge and wholly past it (as M_Terrain).
float4 gt = GroundTint;
float4 gf = GroundFx;
float nbW = 0.0, nbLayer = GroundLayer;
{
    float4 ds = float4(g.y - lo.y, hi.x - g.x, hi.y - g.y, g.x - lo.x);
    float4 inner = 0.5 * (1.0 - smoothstep(0.0, 16.0, ds));
    float4 outer = 0.5 + 0.5 * smoothstep(0.0, 10.0, -ds);
    float4 w = Neighbours * (ds > 0.0 ? inner : outer);
    gt = lerp(gt, NbTintN, w.x); gf = lerp(gf, NbFxN, w.x);
    gt = lerp(gt, NbTintE, w.y); gf = lerp(gf, NbFxE, w.y);
    gt = lerp(gt, NbTintS, w.z); gf = lerp(gf, NbFxS, w.z);
    gt = lerp(gt, NbTintW, w.w); gf = lerp(gf, NbFxW, w.w);
    // The lab's texture: the strongest neighbour's layer toward its edge.
    nbW = max(max(w.x, w.y), max(w.z, w.w));
    nbLayer = w.x >= nbW ? NbLayer.x : (w.y >= nbW ? NbLayer.y : (w.z >= nbW ? NbLayer.z : NbLayer.w));
}

// The ground's own layer (the region's: GroundLayer), the neighbour's toward
// an edge, grass and highland by the splat, height-blended.
float4 chB, nrB;
Lab::Layer(LabCH, LabCHSampler, LabNR, LabNRSampler, g, gdx, gdy, GroundLayer, NearTile, FarTile, nearW, contrast, chB, nrB);
if (nbW > 0.01 && abs(nbLayer - GroundLayer) > 0.5)
{
    float4 chN, nrN;
    Lab::Layer(LabCH, LabCHSampler, LabNR, LabNRSampler, g, gdx, gdy, nbLayer, NearTile, FarTile, nearW, contrast, chN, nrN);
    float k = Lab::Over(chB.a, chN.a, nbW, 3.0);
    chB = lerp(chB, chN, k); nrB = lerp(nrB, nrN, k);
}
// The plain dirt in the game's colour; a region's own texture as it is
// (near an edge the neighbour's, mixed in above, is recoloured as much).
float3 dirt = GroundLayer < 0.5 ? Lab::Recolour(MeanDirt.rgb, chB.rgb, DirtColour, LayerDetail.x) : chB.rgb;
float4 chG = 0, nrG = 0, chH = 0, nrH = 0;
if (splat.r > 0.001)
    Lab::Layer(LabCH, LabCHSampler, LabNR, LabNRSampler, g, gdx, gdy, 1.0, NearTile * 0.8, FarTile * 0.8, nearW, contrast, chG, nrG);
if (splat.g > 0.001)
    Lab::Layer(LabCH, LabCHSampler, LabNR, LabNRSampler, g, gdx, gdy, 2.0, NearTile * 1.2, FarTile * 1.2, nearW, contrast, chH, nrH);
float3 grass = Lab::Recolour(MeanGrass.rgb, chG.rgb, GrassColour, LayerDetail.y);
float3 high = Lab::Recolour(MeanHigh.rgb, chH.rgb, HighColour, LayerDetail.z);
float2 tp = g / 3.5;
float3 plate = Texture2DSample(Plate, PlateSampler, tp).rgb;

float kH = Lab::Over(chB.a, chH.a, splat.g, 3.0);
float3 c = lerp(dirt, high, kH);
float4 nr = lerp(nrB, nrH, kH);
float hTop = lerp(chB.a, chH.a, kH);
float kG = Lab::Over(hTop, chG.a, splat.r, 1.4);
c = lerp(c, grass, kG);
nr = lerp(nr, nrG, kG);
c = lerp(c, plate, splat.b);
c *= 1.0 - splat.a * 0.5;

// Macro colour over 30-80 m: wide plains in lighter and darker stretches.
float macroV = Lab::Noise(g / 31.0 + 0.23) * 0.6 + Lab::Noise(g / 83.0 + 5.1) * 0.4;
c *= lerp(1.0 - MacroGain, 1.0 + MacroGain, macroV);

// Cliff rock on steep faces, projected sideways (triplanar on X and Y),
// hex-tiled as the ground.
float2 ux = float2(g.y, -wp.z * 1.4) / RockTile;
float2 uy = float2(g.x, -wp.z * 1.4) / RockTile;
float ax = abs(wn.x), ay = abs(wn.y);
float3 rock = float3(0.3, 0.27, 0.24);
float3 dRock = 0;
float rockRough = 0.85;
if (rockW > 0.001)
{
    float4 cx = 0, nx = 0, cy = 0, ny = 0;
    float sx = ax / max(ax + ay, 1e-3);
    if (sx > 0.01)
        Lab::Hex(LabCH, LabCHSampler, LabNR, LabNRSampler, ux, float2(gdx.y, -ddx(wp.z) * 1.4) / RockTile, float2(gdy.y, -ddy(wp.z) * 1.4) / RockTile, 3.0, contrast, true, cx, nx);
    if (sx < 0.99)
        Lab::Hex(LabCH, LabCHSampler, LabNR, LabNRSampler, uy, float2(gdx.x, -ddx(wp.z) * 1.4) / RockTile, float2(gdy.x, -ddy(wp.z) * 1.4) / RockTile, 3.0, contrast, true, cy, ny);
    rock = Lab::Recolour(MeanRock.rgb, cx.rgb * sx + cy.rgb * (1.0 - sx), RockColour, LayerDetail.w);
    // U along +Y (or +X), V down the face.
    dRock = float3(0.0, nx.x, -nx.y) * sx + float3(ny.x, 0.0, -ny.y) * (1.0 - sx);
    rockRough = nx.z * sx + ny.z * (1.0 - sx);
}
c = lerp(c, rock * 0.95, rockW);

// Darken low ground a touch and cliff bases, like baked AO (round 2 bakes
// the real one); the textures' own occlusion.
c *= lerp(0.88, 1.0, saturate(wp.z / 2.4));
c *= lerp(1.0, lerp(nr.w, 1.0, rockW), AoGain);
// Round 2: the relief bake (AcTerrain: occlusion by the land round, and
// curvature): cliff feet, gullies and craters darker, crests lighter; the
// rock's cracks by its own occlusion.
// Open flat ground sees walls far off: it stays full bright (above 0.87).
c *= lerp(1.0, saturate(relief.r * 1.15), AoBake);
c *= 1.0 - clamp((relief.g - 0.5) * 2.0, -1.0, 1.0) * CurveGain;
c *= lerp(1.0, nr.w * nr.w, AoGain * rockW);

// Past the map's edge (the border scenery, out of play): wild highland and
// rock, dimmer, fading into the haze further out, with a faint dark seam
// along the edge itself. Haze.w is how far the border reaches.
float past = length(max(max(lo - g, g - hi), 0.0));
float4 dsSeam = float4(g.y - lo.y, hi.x - g.x, hi.y - g.y, g.x - lo.x) + Neighbours * 1e4;
float edge = past > 0.0 ? past : max(min(min(dsSeam.x, dsSeam.y), min(dsSeam.z, dsSeam.w)), 0.0);
c *= 1.0 - 0.3 * (1.0 - smoothstep(0.0, 0.9, edge));
float fade = 0.0;
if (past > 0.0) {
    float3 wild = lerp(lerp(dirt, high + dirt * (1.0 - splat.g), 0.65), rock * 0.95, rockW);
    c = lerp(c, wild, smoothstep(0.0, 1.5, past));
    c *= lerp(1.0, 0.72, smoothstep(0.0, 2.0, past));
    fade = smoothstep(Haze.w * 0.3, Haze.w * 0.9, past);
    splat = lerp(splat, float4(0, 0, 0, 0), smoothstep(0.0, 1.5, past));
}

// The walls' tops (W4b, AcTerrain sets WallZ): no region's palette there but
// one rock for the whole world.
float wallW = smoothstep(WallZ - 0.6, WallZ + 0.4, wp.z) * (1.0 - fade);
gt.a *= 1.0 - wallW;
gf *= 1.0 - wallW;
float crack = 0.0, plateV = 0.5, facet = 0.0;
float2 facetTilt = float2(0.0, 0.0);
float3 ember = float3(0.0, 0.0, 0.0);
if (gt.a > 0.001) {
    float lumC = dot(c, float3(0.3, 0.59, 0.11));
    float detail = clamp(lumC / 0.3, 0.45, 1.6);
    // A region with its own texture keeps most of that texture's colour;
    // the grass, highland and cliffs over it take the palette whole.
    float own = GroundLayer > 0.5 ? lerp(1.0, LabTint, (1.0 - kH) * (1.0 - kG) * (1.0 - rockW)) : 1.0;
    float k = gt.a * own * (1.0 - splat.b) * (1.0 - 0.35 * rockW);
    c = lerp(c, gt.rgb * detail, k);
    // Ochre bands: strata by height, strongest on the cliffs.
    if (gf.w > 0.001) {
        float bh = wp.z / 0.55 + 0.6 * sin(g.x * 0.13) + 0.5 * sin(g.y * 0.11 + 1.7);
        float bandI = frac(bh / 3.0);
        float3 bandC = bandI < 0.33 ? float3(1.18, 1.0, 0.78) : (bandI < 0.66 ? float3(0.86, 0.66, 0.5) : float3(1.05, 0.82, 0.55));
        bandC = lerp(float3(1, 1, 1), bandC, smoothstep(0.0, 0.08, abs(frac(bh) - 0.5)) * 0.6 + 0.4);
        c *= lerp(float3(1, 1, 1), bandC, gf.w * lerp(0.55, 1.0, rockW));
    }
    // Plates and cracks (salt crust, glass facets, slag): one cell pattern,
    // over the texture's own at FxKeep.
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
        float3 fx = gf.xyz * FxKeep;
        c = lerp(c, c * lerp(0.92, 1.08, plateV), fx.x);
        c = lerp(c, c * float3(0.45, 0.47, 0.5), fx.x * crack * (1.0 - rockW));
        facet = fx.y * (1.0 - rockW);
        facetTilt = (frac(float2(plateV * 7.13, plateV * 3.71)) - 0.5) * 0.9;
        c = lerp(c, c * lerp(0.7, 1.25, plateV), facet * 0.6);
        // Slag: the embers in the cracks keep their full glow.
        float slag = gf.z * (1.0 - rockW * 0.7);
        c = lerp(c, c * 0.55, slag * crack * FxKeep);
        float breathe = 0.65 + 0.35 * sin(T * 0.6 + plateV * 6.28);
        ember = float3(1.0, 0.3, 0.05) * slag * crack * breathe * EmberGain;
    }
    // Frost (Cold Fork, from night to mid-morning): rime on flat ground
    // and the tops of rocks, in patches.
    if (Frost > 0.001) {
        float fz = Lab::Noise(g / 3.0 + 0.71) * 0.6 + Lab::Noise(g / 1.1) * 0.4;
        float rime = smoothstep(0.35, 0.6, fz + (wn.z - 0.8)) * Frost;
        c = lerp(c, float3(0.62, 0.68, 0.76), rime * 0.8);
    }
}

// Normals (world space). Ground: the layers' normals, projected from above
// (tangent +X, bitangent +Y), blended as the colours are; sand ripples on
// the dirt across the region's x, more on slopes, near the eye only.
float2 dn = lerp(nr.xy, float2(0, 0), splat.b);
float ripW = RippleGain * (GroundLayer < 0.5 ? 1.0 : 0.0) * (1.0 - kG) * (1.0 - kH) * (1.0 - splat.b) * (1.0 - rockW)
           * lerp(0.35, 1.0, smoothstep(0.03, 0.2, slope)) * (1.0 - smoothstep(25.0, 45.0, dist));
if (ripW > 0.001)
{
    float warp = Lab::Noise(g * 0.35) * 2.6 + Lab::Noise(g * 1.3) * 0.5;
    float ph = (g.x + warp) * 6.2831853 / 0.42;
    dn.x += cos(ph) * 0.35 * ripW;
}
float3 nTop = normalize(wn + float3(dn, 0.0) * Detail);
float3 nRock = normalize(wn + dRock * Detail * 1.3);
float3 n = normalize(lerp(nTop, nRock, rockW));

// Wet ground (AcWeather, docs/weather.md): as M_Terrain.
float wet = Wetness * (1.0 - fade) * (1.0 - rockW * 0.6);
float puddle = 0.0;
if (wet > 0.0) {
    float2 q0 = g / 2.2, q1 = g / 0.9 + 17.3;
    float2 i0 = floor(q0), f0 = frac(q0), i1 = floor(q1), f1 = frac(q1);
    f0 = f0 * f0 * (3.0 - 2.0 * f0);
    f1 = f1 * f1 * (3.0 - 2.0 * f1);
    #define AC_H(p) frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453)
    float n0 = lerp(lerp(AC_H(i0), AC_H(i0 + float2(1, 0)), f0.x), lerp(AC_H(i0 + float2(0, 1)), AC_H(i0 + float2(1, 1)), f0.x), f0.y);
    float n1 = lerp(lerp(AC_H(i1), AC_H(i1 + float2(1, 0)), f1.x), lerp(AC_H(i1 + float2(0, 1)), AC_H(i1 + float2(1, 1)), f1.x), f1.y);
    #undef AC_H
    float pn = n0 * 0.65 + n1 * 0.35;
    // Water gathers in the texture's hollows first.
    pn += (0.5 - hTop) * 0.12;
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
// The world's rock on the walls' tops: weathered highland and cliff rock,
// grey-brown, the same in every region.
{
    float3 hi2 = chH.a > 0.0 ? high : dirt;
    float3 worldRock = lerp(hi2 * float3(0.95, 0.9, 0.85), rock, 0.45 + 0.3 * rockW) * float3(0.66, 0.62, 0.58);
    worldRock *= lerp(0.85, 1.12, macroV);
    c = lerp(c, worldRock * lerp(1.0, 0.6, wet), wallW);
}
Nrm = normalize(lerp(n, wn, fade));

// Roughness: the textures' own (plating smoother, a little metallic).
float texRough = lerp(nr.z, rockRough, rockW);
Rough = saturate(lerp(lerp(0.8, 1.0, texRough), 0.55, splat.b));
Rough = lerp(lerp(Rough, 0.55, wet * Gloss), 0.22, puddle * Gloss);
Rough = lerp(Rough, lerp(0.42, 0.22, step(0.78, plateV)), facet * (1.0 - wet));
Metal = splat.b * 0.35 + facet * 0.15;
Emis = Haze.rgb * fade * HazeGain + ember * (1.0 - fade);
return c * (1.0 - fade);
"""


def build_material(arrays):
    path = f"{LAB_DIR}/{MAT_NAME}"
    mat = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if mat is None:
        mat = tools.create_asset(MAT_NAME, LAB_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("tangent_space_normal", False)
    mat.set_editor_property("two_sided", False)

    def node(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    custom = node(unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("description", "Terrain lab ground shader")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    outputs = []
    for name, kind in (("Rough", "CMOT_FLOAT1"), ("Metal", "CMOT_FLOAT1"),
                       ("Emis", "CMOT_FLOAT3"), ("Nrm", "CMOT_FLOAT3")):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", getattr(unreal.CustomMaterialOutputType, kind))
        outputs.append(o)
    custom.set_editor_property("additional_outputs", outputs)

    wp = node(unreal.MaterialExpressionWorldPosition, -900, -460)
    vn = node(unreal.MaterialExpressionVertexNormalWS, -900, -400)
    cam = node(unreal.MaterialExpressionCameraPositionWS, -900, -340)
    sources = [("WP", wp), ("VN", vn), ("Cam", cam)]
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
    linear = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    tex_param("Splat", "Splat", unreal.load_asset("/Engine/EngineResources/Black"), linear)
    # Round 2's relief bake (AcTerrain); white: no occlusion, flat.
    tex_param("Relief", "Relief", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"), linear)
    tex_param("LabCH", "LabColour", arrays["CH"], color)
    tex_param("LabNR", "LabNormal", arrays["NR"], linear)
    tex_param("Plate", "Plating", unreal.load_asset(PLATING), color)

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

    # M_Terrain's parameters, the same names and defaults (AcTerrain,
    # AcDaylight and AcGroundLook set them on either material).
    sources.append(("Bounds", vec_param("Bounds", (-100.0, -64.0, 200.0, 128.0))))
    sources.append(("Haze", vec_param("Haze", (0.27, 0.22, 0.19, 90.0))))
    sources.append(("Detail", scalar_param("DetailNormal", 0.8)))
    sources.append(("HazeGain", scalar_param("HazeGain", 1.0)))
    sources.append(("Wetness", scalar_param("Wetness", 0.0)))
    sources.append(("RainRate", scalar_param("RainRate", 0.0)))
    sources.append(("Ripples", scalar_param("Ripples", 1.0)))
    sources.append(("Gloss", scalar_param("Gloss", 1.0)))
    sources.append(("GroundTint", vec_param("GroundTint", (0.2, 0.15, 0.1, 0.0))))
    sources.append(("GroundFx", vec_param("GroundFx", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("Frost", scalar_param("Frost", 0.0)))
    sources.append(("EmberGain", scalar_param("EmberGain", 1.0)))
    sources.append(("WallZ", scalar_param("WallZ", 100000.0)))
    sources.append(("Neighbours", vec_param("Neighbours", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("Origin", vec_param("Origin", (0.0, 0.0, 0.0, 0.0))))
    for e in "NESW":
        sources.append((f"NbTint{e}", vec_param(f"NbTint{e}", (0.2, 0.15, 0.1, 0.0))))
        sources.append((f"NbFx{e}", vec_param(f"NbFx{e}", (0.0, 0.0, 0.0, 0.0))))
    # The lab's own (round 1). The layers: AcGroundLook sets GroundLayer and
    # NbLayer (N, E, S, W); the rest are the round's knobs, tuned by eye.
    sources.append(("GroundLayer", scalar_param("GroundLayer", 0.0)))
    sources.append(("NbLayer", vec_param("NbLayer", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("NearTile", scalar_param("NearTile", 2.5)))
    sources.append(("FarTile", scalar_param("FarTile", 11.0)))
    sources.append(("RockTile", scalar_param("RockTile", 6.0)))
    sources.append(("NearFade", vec_param("NearFade", (14.0, 38.0, 0.0, 0.0))))
    sources.append(("HexContrast", scalar_param("HexContrast", 5.0)))
    sources.append(("MacroGain", scalar_param("MacroGain", 0.1)))
    sources.append(("RippleGain", scalar_param("RippleGain", 1.0)))
    sources.append(("AoGain", scalar_param("AoGain", 0.35)))
    sources.append(("LabTint", scalar_param("LabTint", 0.35)))
    # Round 2: parallax (depth m, its fade m) and the relief bake.
    sources.append(("ParallaxDepth", scalar_param("ParallaxDepth", 0.09)))
    sources.append(("ParallaxRange", vec_param("ParallaxRange", (8.0, 12.0, 0.0, 0.0))))
    sources.append(("AoBake", scalar_param("AoBake", 0.6)))
    sources.append(("CurveGain", scalar_param("CurveGain", 0.25)))
    sources.append(("FxKeep", scalar_param("FxKeep", 0.6)))
    # The game's own colours (M_Terrain's mean albedo of each, linear): the
    # look keeps the map's read; the textures bring the grain.
    sources.append(("DirtColour", vec_param("DirtColour", (0.275, 0.099, 0.026, 1.0))))
    sources.append(("GrassColour", vec_param("GrassColour", (0.102, 0.116, 0.016, 1.0))))
    sources.append(("HighColour", vec_param("HighColour", (0.389, 0.192, 0.041, 1.0))))
    sources.append(("RockColour", vec_param("RockColour", (0.1, 0.078, 0.052, 1.0))))
    # How much of each texture's own light and dark shows round the colour:
    # dirt, grass (a soft set: pushed), highland, rock.
    means = json.load(open(os.path.join(SRC, "means.json")))
    for name, layer in (("MeanDirt", "Dirt"), ("MeanGrass", "Grass"), ("MeanHigh", "Highland"), ("MeanRock", "Rock")):
        sources.append((name, vec_param(name, (*means[layer], 1.0))))
    sources.append(("LayerDetail", vec_param("LayerDetail", (0.6, 2.4, 0.7, 1.2))))
    sources.append(("T", node(unreal.MaterialExpressionTime, -900, y)))

    inputs = []
    for name, _ in sources:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    for name, src in sources:
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
    slices = import_slices()
    arrays = {
        "CH": make_array("T_TerrainLabCH", slices["CH"], True),
        "NR": make_array("T_TerrainLabNR", slices["NR"], False),
    }
    mat = build_material(arrays)
    unreal.log(f"make_terrain_lab_material: built {mat.get_path_name()}")


main()
