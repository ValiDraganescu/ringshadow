"""M_Terrain and its textures: the ground (docs/terrain-lab.md, rounds 1 and 2,
promoted 2026-10-19; the material before it is in git history).

Rerunnable: imports the slices into a scratch folder, remakes the two
texture arrays from them (each keeps its own copy of their source), then
deletes the slices, and rebuilds the material's graph from nothing.

Inputs: unreal/Content-src/terrain/TL_<Layer>_CH.png and _NR.png, made by
`uv run unreal/Tools/terrain_textures.py` (CC0 sets, listed in
unreal/Resources/Textures/terrain/SOURCES.md), 1024 px:
  CH: rgb the colour (sRGB), a the height
  NR: rg the normal's x and y (DirectX), b the roughness, a the occlusion

Outputs:
  /Game/Terrain/Textures/T_TerrainCH, T_TerrainNR  the arrays
  /Game/Terrain/M_Terrain
  /Game/Terrain/M_GroundBlendProbe  the palettes' blend field alone (a test's)

Where palettes meet (GroundBlend.h): the field's functions are C++'s, ported
to HLSL (GB_HLSL), their numbers read from GroundBlend.h's table.

The layers, in the arrays' order (AcGroundLook.cpp's `Layer` picks a
region's own): 0 Dirt (plain, and rust and ochre by their tint), 1 Grass,
2 Highland, 3 Rock (the cliffs), 4 Salt, 5 Ash, 6 Glass, 7 Slag, 8 Frost.

Every layer is sampled hex-tiled (three hex cells' samples, each turned and
moved its own random way, blended by the cells' weights sharpened by
height), so no tile repeats; a near scale and a far scale cross-fade by
distance; a macro colour noise over 30 to 80 m; sand ripples on the dirt.
Layers meet by height (the higher grain wins). Within 12 m the ground has
parallax; AcTerrain's relief bake (occlusion, curvature) darkens hollows
and cliff feet. The splat map, the palettes' tint and effects, the wet
ground, the haze and WallZ are set by AcTerrain, AcDaylight and AcGroundLook.
The ground is dry in a Citadel's hall (MPC_AcWorld's shelter boxes, AcShelter.h:
no wetness, puddles or rings where it shows through the floor).

Without the sources (they are git-ignored: a worktree has none) the arrays
and the layers' means are kept as they are and only the material is rebuilt.

Run it headless through the one hidden-run slot:
  uv run unreal/Tools/hidden_run.py --out unreal/Saved/Terrain --python unreal/Tools/Editor/make_terrain_material.py
"""
import json
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_resource_materials as RM  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Content-src", "terrain")
LAB_DIR = "/Game/Terrain"
TEX_DIR = f"{LAB_DIR}/Textures"
SLICE_DIR = "/Game/TerrainScratch"  # deleted once the arrays hold their source
MAT_NAME = "M_Terrain"

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
                raise RuntimeError(f"missing {src} (uv run unreal/Tools/terrain_textures.py)")
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
    unreal.log(f"make_terrain_material: {path}, {len(slices)} slices")
    return arr


# Where palettes meet: GroundBlend.cpp's functions in HLSL, members of the
# shaders' `Lab` struct (M_Terrain's and M_GroundBlendProbe's: one text).
GB_HLSL = r"""
    // Where palettes meet (AutocraftCore GroundBlend.h/.cpp): its twin, the
    // same operations in the same order, so the GPU's field is the CPU's
    // (the scenery's) to a rounding; Autocraft.Ground.Blend.Shader checks it.
    static uint GbHash(int x, int y, uint s)
    {
        uint v = ((uint)x * GB_hashX) ^ ((uint)y * GB_hashY) ^ (s * GB_hashS);
        v ^= v >> 16;
        v *= GB_mixA;
        v ^= v >> 15;
        v *= GB_mixB;
        v ^= v >> 16;
        return v;
    }
    static float GbUnit(uint v) { return (float)(v >> 8) * (1.0 / 16777216.0); }
    static float GbSmooth(float e0, float e1, float v)
    {
        float t = saturate((v - e0) / (e1 - e0));
        return t * t * (3.0 - 2.0 * t);
    }
    static float GbValue(float x, float y, uint s)
    {
        float fx = floor(x), fy = floor(y);
        int ix = (int)fx, iy = (int)fy;
        float tx = x - fx, ty = y - fy;
        tx = tx * tx * (3.0 - 2.0 * tx);
        ty = ty * ty * (3.0 - 2.0 * ty);
        float h00 = GbUnit(GbHash(ix, iy, s)), h10 = GbUnit(GbHash(ix + 1, iy, s));
        float h01 = GbUnit(GbHash(ix, iy + 1, s)), h11 = GbUnit(GbHash(ix + 1, iy + 1, s));
        float top = h00 + (h10 - h00) * tx;
        float bottom = h01 + (h11 - h01) * tx;
        return top + (bottom - top) * ty;
    }
    static float GbWander(float x, float y, uint s)
    {
        float px = x / GB_wanderWave, py = y / GB_wanderWave;
        float qx = (0.8 * px - 0.6 * py) * 2.0 + 37.1, qy = (0.6 * px + 0.8 * py) * 2.0 + 11.7;
        float n0 = GbValue(px, py, s), n1 = GbValue(qx, qy, s);
        return ((n0 - 0.5) + (n1 - 0.5) * 0.5) * (2.0 / 1.5);
    }
    static float GbPatch(float x, float y, uint s)
    {
        uint ps = s ^ GB_patchSalt;
        float wv = (GbValue(x / GB_warpWave, y / GB_warpWave, ps + 1u) - 0.5) * (2.0 * GB_warpCells);
        float ux = x - 0.6 * wv, uy = y + 0.8 * wv;
        float px = (0.8 * ux + 0.6 * uy) / (GB_patchWave * GB_patchStretch), py = (0.8 * uy - 0.6 * ux) / GB_patchWave;
        float qx = (0.8 * px - 0.6 * py) * 2.0 + 19.3, qy = (0.6 * px + 0.8 * py) * 2.0 + 7.1;
        float rx = (0.8 * qx - 0.6 * qy) * 2.0 + 3.7, ry = (0.6 * qx + 0.8 * qy) * 2.0 + 29.3;
        float rx2 = (0.8 * rx - 0.6 * ry) * 2.0 + 13.1, ry2 = (0.6 * rx + 0.8 * ry) * 2.0 + 2.9;
        float raw = (GbValue(px, py, ps) + GbValue(qx, qy, ps) * 0.5 + GbValue(rx2, ry2, ps) * GB_patchRough) * (1.0 / (1.5 + GB_patchRough));
        return saturate((raw - 0.5) * GB_patchContrast + 0.5);
    }
    static float GbBand(float d, float w) { return saturate((d + w * GB_wanderCells) / GB_bandCells + 0.5); }
    static float GbPlus(float d, float w, float m)
    {
        float b = GbBand(d, w);
        float mm = GB_patchSoft + (1.0 - 2.0 * GB_patchSoft) * m;
        return GbSmooth(mm - GB_patchSoft, mm + GB_patchSoft, b);
    }
    // One grid line's field: the plus side's weight (east, south) at d
    // cells toward it; its two segments' seeds cross-faded at the corner t0.
    static float GbLine(float d, float x, float y, float t, float t0, uint minus, uint plus)
    {
        float reach = GB_wanderCells + GB_bandCells * 0.5;
        if (d <= -reach) return 0.0;
        if (d >= reach) return 1.0;
        float k = GbSmooth(-GB_seamCells, GB_seamCells, t - t0);
        // The wander first: past the band's ends the patches are not read.
        float w = 0.0, m = 0.0;
        [branch] if (k < 1.0) w = GbWander(x, y, minus) * (1.0 - k);
        [branch] if (k > 0.0) w = w + GbWander(x, y, plus) * k;
        float b = GbBand(d, w);
        if (b <= 0.0) return 0.0;
        if (b >= 1.0) return 1.0;
        [branch] if (k < 1.0) m = GbPatch(x, y, minus) * (1.0 - k);
        [branch] if (k > 0.0) m = m + GbPatch(x, y, plus) * k;
        return GbPlus(d, w, m);
    }
    // The field in a region's frame (groundblend::field): across the nearer
    // vertical line (x, toward the side away from the region) and the nearer
    // horizontal one (y). `has`: a region beyond N, E, S, W; `edge`: the
    // seeds across them; `beyondV`, `beyondH`: past the corners NE, SE, SW, NW.
    static float2 GbField(float x, float y, float2 lo, float2 hi, float4 has, float4 edge, float4 beyondV, float4 beyondH)
    {
        float midX = (lo.x + hi.x) * 0.5, midY = (lo.y + hi.y) * 0.5;
        bool e = x >= midX, s = y >= midY;
        float x0 = e ? hi.x : lo.x, y0 = s ? hi.y : lo.y;
        // A line with no region beyond it has no field (the world's rim).
        bool hasV = (e ? has.y : has.w) > 0.5, hasH = (s ? has.z : has.x) > 0.5;
        uint vOwn = (uint)(e ? edge.y : edge.w);
        uint vBeyond = (uint)(e ? (s ? beyondV.y : beyondV.x) : (s ? beyondV.z : beyondV.w));
        float pv = 0.0;
        [branch] if (hasV) pv = GbLine(x - x0, x, y, y, y0, s ? vOwn : vBeyond, s ? vBeyond : vOwn);
        uint hOwn = (uint)(s ? edge.z : edge.x);
        uint hBeyond = (uint)(s ? (e ? beyondH.y : beyondH.z) : (e ? beyondH.x : beyondH.w));
        float ph = 0.0;
        [branch] if (hasH) ph = GbLine(y - y0, x, y, x, x0, e ? hOwn : hBeyond, e ? hBeyond : hOwn);
        float a = hasV ? (e ? pv : 1.0 - pv) : 0.0;
        float b = hasH ? (s ? ph : 1.0 - ph) : 0.0;
        return float2(a, b);
    }
"""


# M_Terrain's shader (make_terrain_material.py) with round 1 on it. Unreal's
# axes, in cells (1 m): the ground plane is (X, Y), the height Z.
HLSL = r"""
%GROUNDBLEND%
// The lab's samplers, as functions (a Custom node's code is one function's
// body; a struct's static functions are the way to have more).
struct Lab
{
%GBFUNCS%
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

float2 lo = Bounds.xy, hi = Bounds.xy + Bounds.zw;

// Where palettes meet (docs/regions.md, "ground"; GroundBlend.h; AcGroundLook
// sets the parameters): the field across the nearer vertical edge line (a)
// and horizontal one (b), on the world's grid (BleedShift: the map bake's
// place for a region shown alone), and the four palettes it weighs: own
// (1-a)(1-b), the side neighbour's a(1-b), the one ahead (1-a)b, the
// diagonal one ab. Every material round a point gets the same weights, and
// everything below that a palette decides (its tint, fx, texture, frost,
// ripples) follows them, so both sides of an edge draw the same ground.
float2 gb = g + BleedShift.xy;
float2 gbAB = Lab::GbField(gb.x, gb.y, lo + BleedShift.xy, hi + BleedShift.xy, Neighbours, BleedEdge, BleedV, BleedH);
bool gbE = gb.x >= (lo.x + hi.x) * 0.5 + BleedShift.x, gbS = gb.y >= (lo.y + hi.y) * 0.5 + BleedShift.y;
float4 pw = float4((1.0 - gbAB.x) * (1.0 - gbAB.y), gbAB.x * (1.0 - gbAB.y), (1.0 - gbAB.x) * gbAB.y, gbAB.x * gbAB.y);
float4 tintS = gbE ? NbTintE : NbTintW, fxS = gbE ? NbFxE : NbFxW;
float4 tintA = gbS ? NbTintS : NbTintN, fxA = gbS ? NbFxS : NbFxN;
float4 tintD = gbE ? (gbS ? NbTintSE : NbTintNE) : (gbS ? NbTintSW : NbTintNW);
float4 fxD = gbE ? (gbS ? NbFxSE : NbFxNE) : (gbS ? NbFxSW : NbFxNW);
float4 pLayers = float4(GroundLayer, gbE ? NbLayer.y : NbLayer.w, gbS ? NbLayer.z : NbLayer.x,
                        gbE ? (gbS ? NbLayerD.y : NbLayerD.x) : (gbS ? NbLayerD.z : NbLayerD.w));
// The tint, the fx and the frost follow the field too, grained once the
// layers are read (below).
float4 gt, gf;
float frostW;
// The texture layers: one weight a layer (two palettes may share one), and
// the two heaviest read (as many as the old edge blend read: four cost
// 0.1-0.2 ms more at 1600 x 1000 for their registers, wherever they were not
// read), the lighter dropped (a corner's soft rims where three textured
// palettes meet). Chosen by the weights alone, the same on every side.
float4 lw = pw, ll = pLayers;
[unroll] for (int li = 1; li < 4; li++)
    [unroll] for (int lj = 0; lj < li; lj++)
        if (abs(ll[li] - ll[lj]) < 0.5 && lw[li] > 0.0) { lw[lj] += lw[li]; lw[li] = 0.0; }
float wA = lw.x, lA = ll.x, wB = 0.0, lB = ll.x;
[unroll] for (int pi = 1; pi < 4; pi++)
{
    if (lw[pi] > wA || (lw[pi] == wA && ll[pi] < lA)) { wB = wA; lB = lA; wA = lw[pi]; lA = ll[pi]; }
    else if (lw[pi] > wB || (lw[pi] == wB && lw[pi] > 0.0 && ll[pi] < lB)) { wB = lw[pi]; lB = ll[pi]; }
}
// The parallax marches the heaviest.
float pLayer = lA;

// Round 2: parallax occlusion on the ground within ParallaxRange (start,
// end of its fade, m): the base layer's height marched along the view, the
// ground's every layer then read where the ray met it. Fades out to nothing
// before the range's end, so nothing pops; off on cliffs and plating.
{
    float3 V = normalize(Cam / 100.0 - wp);
    // At a grazing look (a cockpit's eye 1.3 m up sees the ground at 5-20
    // degrees) the march would stretch over a metre and show its steps as
    // stacked slices that swim with the camera's height: the depth fades
    // out below 30 degrees and the march never reaches past 6 cm.
    float graze = smoothstep(0.3, 0.6, V.z);
    float depth = ParallaxDepth * graze * (1.0 - smoothstep(ParallaxRange.x, ParallaxRange.y, dist)) * (1.0 - rockW) * (1.0 - splat.b);
    if (depth > 1e-4 && nearW > 0.001)
    {
        float steps = 16.0;
        float2 reach = -V.xy / V.z * depth;
        reach *= min(1.0, 0.06 / max(length(reach), 1e-5));
        float2 stepG = reach / steps;
        float2 off = 0;
        float level = 1.0;
        float2 dx = gdx / NearTile, dy = gdy / NearTile;
        float hNow = Lab::HexHeight(LabCH, LabCHSampler, g / NearTile, dx, dy, pLayer, contrast);
        float hBefore = hNow, levelBefore = level;
        [loop] for (int i = 0; i < 16 && hNow < level; i++)
        {
            hBefore = hNow; levelBefore = level;
            off += stepG;
            level -= 1.0 / steps;
            hNow = Lab::HexHeight(LabCH, LabCHSampler, (g + off) / NearTile, dx, dy, pLayer, contrast);
        }
        // Between the last two steps, where the ray crossed the height.
        float a = hNow - level, b = hBefore - levelBefore;
        float t = saturate(a / min(a - b, -1e-4));
        g += off - stepG * t;
    }
}

float lo2 = 0.0;

// The ground's layers: the lower-numbered of the two first (the same order
// on every side), the other over it by height (the higher grain wins where
// they meet: `Lab::Over`); the plain dirt recoloured to the game's colour as
// it is read; `texB`: how much of what shows is a palette's own texture.
bool twoL = wB > 0.01;
float l1 = twoL ? min(lA, lB) : lA, l2 = max(lA, lB);
float w1 = twoL ? (lA <= lB ? wA : wB) : wA, w2 = lA <= lB ? wB : wA;
float4 chB, nrB;
Lab::Layer(LabCH, LabCHSampler, LabNR, LabNRSampler, g, gdx, gdy, l1, NearTile, FarTile, nearW, contrast, chB, nrB);
if (l1 < 0.5) chB.rgb = Lab::Recolour(MeanDirt.rgb, chB.rgb, DirtColour, LayerDetail.x);
float texB = l1 > 0.5 ? 1.0 : 0.0;
[branch] if (twoL)
{
    float4 chL, nrL;
    Lab::Layer(LabCH, LabCHSampler, LabNR, LabNRSampler, g, gdx, gdy, l2, NearTile, FarTile, nearW, contrast, chL, nrL);
    if (l2 < 0.5) chL.rgb = Lab::Recolour(MeanDirt.rgb, chL.rgb, DirtColour, LayerDetail.x);
    float k = Lab::Over(chB.a, chL.a, w2 / (w1 + w2), 3.0);
    chB = lerp(chB, chL, k); nrB = lerp(nrB, nrL, k);
    texB = lerp(texB, l2 > 0.5 ? 1.0 : 0.0, k);
}
float3 dirt = chB.rgb;
// The colours where palettes mix: each line's plus side's weight sharpened
// round the ground's texture height there (the same on both sides: the
// layers are read in one order), so a patch's rim is grainy like soil.
{
    float2 plusAB = float2(gbE ? gbAB.x : 1.0 - gbAB.x, gbS ? gbAB.y : 1.0 - gbAB.y);
    float2 grained = saturate((plusAB - 0.5) * GB_grainSharp + 0.5 + (chB.a - 0.5) * GB_grainGain);
    plusAB = (plusAB > 0.0 && plusAB < 1.0) ? grained : plusAB;
    float2 ab = float2((gbE ? Neighbours.y : Neighbours.w) > 0.5 ? (gbE ? plusAB.x : 1.0 - plusAB.x) : 0.0,
                       (gbS ? Neighbours.z : Neighbours.x) > 0.5 ? (gbS ? plusAB.y : 1.0 - plusAB.y) : 0.0);
    float4 cw = float4((1.0 - ab.x) * (1.0 - ab.y), ab.x * (1.0 - ab.y), (1.0 - ab.x) * ab.y, ab.x * ab.y);
    // The tint pre-multiplied by its strength (plain ground has none: next
    // to rust it is rust at half strength, not rust halfway to a brown).
    float4 ctw = cw * float4(GroundTint.a, tintS.a, tintA.a, tintD.a);
    float ctSum = dot(ctw, 1.0);
    gt = float4((GroundTint.rgb * ctw.x + tintS.rgb * ctw.y + tintA.rgb * ctw.z + tintD.rgb * ctw.w) / max(ctSum, 1e-4), ctSum);
    gf = GroundFx * cw.x + fxS * cw.y + fxA * cw.z + fxD * cw.w;
    // Frost is the frost ground's (its layer, 8), wherever it bleeds.
    frostW = dot(cw, step(7.5, pLayers));
}
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
    float own = lerp(1.0, LabTint, texB * (1.0 - kH) * (1.0 - kG) * (1.0 - rockW));
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
    float frostK = Frost * frostW;
    if (frostK > 0.001) {
        float fz = Lab::Noise(g / 3.0 + 0.71) * 0.6 + Lab::Noise(g / 1.1) * 0.4;
        float rime = smoothstep(0.35, 0.6, fz + (wn.z - 0.8)) * frostK;
        c = lerp(c, float3(0.62, 0.68, 0.76), rime * 0.8);
    }
}

// Normals (world space). Ground: the layers' normals, projected from above
// (tangent +X, bitangent +Y), blended as the colours are; sand ripples on
// the dirt across the region's x, more on slopes, near the eye only.
float2 dn = lerp(nr.xy, float2(0, 0), splat.b);
float ripW = RippleGain * (1.0 - texB) * (1.0 - kG) * (1.0 - kH) * (1.0 - splat.b) * (1.0 - rockW)
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

// Wet ground (AcWeather, docs/weather.md): as M_Terrain. Dry in a
// Citadel's hall (AcShelter.h): no wetness, puddles or rings.
%SHELTER%Wetness *= 1.0 - shelter;
RainRate *= 1.0 - shelter;
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


# Where palettes meet: the field's numbers live in one table, C++'s
# (GroundBlend.h); the shader takes them from it.
GROUND_BLEND_H = os.path.join(ROOT, "Source", "AutocraftCore", "Public", "GroundBlend.h")


def ground_blend_defines():
    """GroundBlend.h's table (between its markers) as `#define GB_name value`."""
    text = open(GROUND_BLEND_H).read()
    try:
        table = text.split("// groundblend-table begin", 1)[1].split("// groundblend-table end", 1)[0]
    except IndexError:
        raise RuntimeError(f"no groundblend table in {GROUND_BLEND_H}")
    lines = []
    for kind, name, value in re.findall(r"inline constexpr (float|uint32_t) (\w+) = ([^;]+);", table):
        value = value.strip()
        if kind == "float":
            value = value.rstrip("f")
        lines.append(f"#define GB_{name} {value}")
    if len(lines) < 19:
        raise RuntimeError(f"groundblend table: {len(lines)} numbers in {GROUND_BLEND_H}, expected 19")
    return "// Where palettes meet: GroundBlend.h's numbers.\n" + "\n".join(lines)


def build_material(arrays, means):
    path = f"{LAB_DIR}/{MAT_NAME}"
    # Made fresh each time: a material rebuilt in place keeps its old
    # textures among its dependencies (the old M_Terrain's, long gone).
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    mat = tools.create_asset(MAT_NAME, LAB_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("tangent_space_normal", False)
    mat.set_editor_property("two_sided", False)

    def node(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    custom = node(unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", HLSL.replace("%SHELTER%", RM.shelter_hlsl("WP")).replace("%GROUNDBLEND%", ground_blend_defines()).replace("%GBFUNCS%", GB_HLSL))
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
    world = RM.make_world_mpc()
    for name in RM.SHELTER_VECTORS:
        m = node(unreal.MaterialExpressionCollectionParameter, -900, -520)
        m.set_editor_property("collection", world)
        m.set_editor_property("parameter_name", name)
        sources.append((name, m))
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
    # The palettes round it (AcGroundLook): the edges' N, E, S, W and the
    # corners' NE, SE, SW, NW.
    for e in ("N", "E", "S", "W", "NE", "SE", "SW", "NW"):
        sources.append((f"NbTint{e}", vec_param(f"NbTint{e}", (0.2, 0.15, 0.1, 0.0))))
        sources.append((f"NbFx{e}", vec_param(f"NbFx{e}", (0.0, 0.0, 0.0, 0.0))))
    # Where they meet (GroundBlend.h, `Frame`): the seeds across the edges
    # (N, E, S, W) and past the corners on the vertical and horizontal lines
    # (NE, SE, SW, NW); the map bake's place on the world's grid.
    sources.append(("BleedEdge", vec_param("BleedEdge", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("BleedV", vec_param("BleedV", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("BleedH", vec_param("BleedH", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("BleedShift", vec_param("BleedShift", (0.0, 0.0, 0.0, 0.0))))
    # The lab's own (round 1). The layers: AcGroundLook sets GroundLayer,
    # NbLayer (N, E, S, W) and NbLayerD (NE, SE, SW, NW); the rest are the round's knobs, tuned by eye.
    sources.append(("GroundLayer", scalar_param("GroundLayer", 0.0)))
    sources.append(("NbLayer", vec_param("NbLayer", (0.0, 0.0, 0.0, 0.0))))
    sources.append(("NbLayerD", vec_param("NbLayerD", (0.0, 0.0, 0.0, 0.0))))
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
    for name, layer in MEANS:
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


PROBE_NAME = "M_GroundBlendProbe"

# The probe: the field alone, unlit, at a rectangle of world-grid points
# across its UVs (rg: a and b, `Lab::GbField`), for Autocraft.Ground.Blend.Shader
# to draw into a render target and read back against the C++ field.
PROBE_HLSL = r"""
%GROUNDBLEND%
struct Lab
{
%GBFUNCS%
};
float2 p = ProbeRect.xy + UV * ProbeRect.zw;
float2 lo = Bounds.xy, hi = Bounds.xy + Bounds.zw;
return float3(Lab::GbField(p.x, p.y, lo, hi, Neighbours, BleedEdge, BleedV, BleedH), 1.0);
"""


def build_probe():
    path = f"{LAB_DIR}/{PROBE_NAME}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    mat = tools.create_asset(PROBE_NAME, LAB_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("code", PROBE_HLSL.replace("%GROUNDBLEND%", ground_blend_defines()).replace("%GBFUNCS%", GB_HLSL))
    custom.set_editor_property("description", "Ground blend probe")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sources = [("UV", mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, -100))]
    y = 0
    for name, value in (("ProbeRect", (0.0, 0.0, 1.0, 1.0)), ("Bounds", (-100.0, -64.0, 200.0, 128.0)), ("Neighbours", (0.0, 0.0, 0.0, 0.0)),
                        ("BleedEdge", (0.0, 0.0, 0.0, 0.0)), ("BleedV", (0.0, 0.0, 0.0, 0.0)), ("BleedH", (0.0, 0.0, 0.0, 0.0))):
        v = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, y)
        y += 70
        v.set_editor_property("parameter_name", name)
        v.set_editor_property("default_value", unreal.LinearColor(*value))
        sources.append((name, v))
    inputs = []
    for name, _ in sources:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    for name, src in sources:
        out = "RGBA" if isinstance(src, unreal.MaterialExpressionVectorParameter) else ""
        if not mel.connect_material_expressions(src, out, custom, name):
            raise RuntimeError(f"could not connect probe {name}")
    if not mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("could not connect the probe's emissive")
    mel.recompile_material(mat)
    assets.save_asset(path, only_if_is_dirty=False)
    return mat


MEANS = (("MeanDirt", "Dirt"), ("MeanGrass", "Grass"), ("MeanHigh", "Highland"), ("MeanRock", "Rock"))


def kept_means():
    """The layers' means as the material has them now (means.json missing)."""
    old = unreal.load_asset(f"{LAB_DIR}/{MAT_NAME}")
    if old is None:
        raise RuntimeError(f"no {SRC}/means.json and no {LAB_DIR}/{MAT_NAME}: run terrain_textures.py first")
    out = {}
    for name, layer in MEANS:
        c = mel.get_material_default_vector_parameter_value(old, name)
        out[layer] = [c.r, c.g, c.b]
    return out


def main():
    if not os.path.exists(os.path.join(SRC, "means.json")):
        # A worktree without the sources: the arrays and means as they are.
        means = kept_means()
        arrays = {"CH": unreal.load_asset(f"{TEX_DIR}/T_TerrainCH"), "NR": unreal.load_asset(f"{TEX_DIR}/T_TerrainNR")}
        unreal.log(f"make_terrain_material: no sources in {SRC}: the texture arrays kept, means {means}")
        mat = build_material(arrays, means)
        unreal.log(f"make_terrain_material: built {mat.get_path_name()}, {build_probe().get_path_name()}")
        return
    means = json.load(open(os.path.join(SRC, "means.json")))
    slices = import_slices()
    arrays = {
        "CH": make_array("T_TerrainCH", slices["CH"], True),
        "NR": make_array("T_TerrainNR", slices["NR"], False),
    }
    # The arrays keep their own copy of the slices' source: let the slices
    # go (without telling the arrays, which would remake themselves empty).
    for arr in arrays.values():
        arr.set_editor_property("source_textures", [], unreal.PropertyAccessChangeNotifyMode.NEVER)
        assets.save_loaded_asset(arr, only_if_is_dirty=False)
    assets.delete_directory(SLICE_DIR)
    mat = build_material(arrays, means)
    unreal.log(f"make_terrain_material: built {mat.get_path_name()}, {build_probe().get_path_name()}")


main()
