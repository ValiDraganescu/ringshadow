"""The clouds (GAME-LAYER.md, "Clouds" row): M_AcCloud / MI_AcCloud, the
volumetric cloud layer, and M_AcCloudShadow / MI_AcCloudShadow, their shadows
on the map (a light function on the ground's sun and moon).

Rerunnable: everything is rebuilt from nothing each time.

Inputs: the baked noise in unreal/Content-src/sky (Tools/Editor/bake_clouds.py):
  cloud_weather.png -> T_AcCloudWeather (2D, wraps)
  cloud_shape.png   -> VT_AcCloudShape  (128^3, from a 16 x 8 slice atlas)
  cloud_detail.png  -> VT_AcCloudDetail (32^3, from an 8 x 4 slice atlas)

The cloud (a Volume-domain material the VolumetricCloud component traces),
the real-time cloudscape recipe (docs/weather.md, "Clouds (2026-10-18)"):
- the weather map (`WeatherKm` across, read along the wind and stretched
  `CloudW.z` times along it, drifting with it) says where clouds are (R, cut
  by `Coverage`), what kind (G: flat layer, cumulus, towering: a height
  profile over the layer), where a rain deck is thick (B) and where a
  cloud's base sits (A);
- `body()` (the cloud's and its shadow's): the shape noise, Perlin-Worley
  eroded by its own Worley fbm, times the profile, cut by the coverage; a
  rain deck never below the shape itself (no holes to the sky); fair
  weather sits `CloudFine.z` km up in the layer;
- the detail noise eats the edges: billows at the top, wisps at the bottom,
  a finer octave within `CloudFine.x` km. Shadow rays skip it;
- a soft rim and denser cores (`CloudD.x`, `CloudE.z`), density 0.04 per
  metre (`CloudB.y`); far fair-weather cloud thins out (`CloudE.xy` km) and
  takes the horizon's air (`Haze`, `CloudF.zw`);
- lighting through the Volumetric Advanced Output: two-lobe phase and 2
  multiple-scattering octaves wired from `CloudP` and `CloudM` (so
  `ac.Daylight.CloudParam` can try others live), ray-marched self-shadow,
  the ground's bounce on the bases; `ConservativeDensity` from the weather
  map and the height profile lets the tracer skip empty air.
Parameters AAcDaylight sets every frame: `CloudA` (weather offset along the
wind km, -, shape offset km xy), `CloudB` (Coverage, Density (extinction per
metre at full density), WeatherKm, ShapeKm), `CloudC` (DetailKm, Erosion,
TypeBias, storm: the sky light left at a cloud's base falls from CloudD.z to
0.33), `CloudW` (the wind's direction xy, the stretch, the layer's height
km), `Haze`, `Albedo`, `Glow` (a lightning strike's light at full density,
linear) and `Bolt` (where it is: world km xyz, radius km): the cloud lights
up from inside round the strike, a bright core in a soft halo, not the whole
sky at once. `Giant` (toward the gas giant xyz, the cosine of its angular
radius): the clouds beyond 1.5-4.5 km thin out in front of it, so the planet
stays in sight (docs/mmorpg-m6.md V2). The look (defaults here, not set at
run time): `CloudD`, `CloudE`, `CloudF`, `CloudFine`, `CloudP`, `CloudM`.

The shadow (a LightFunction-domain material) is cast by those clouds: from
the lit pixel toward the light, four samples up through the lower layer
(where the mass is), each the cloud's own density without the detail
erosion (same weather map, cover, height profile, shape noise one mip
softer, same wind), so the shade on the ground is the cloud overhead, its
size and its drift. `CloudA`/`CloudB`/`CloudC` as the cloud's;
`ShadowL` (toward the light xyz, Strength: the light keeps 1 - Strength
under a thick cloud), `ShadowC` (layer bottom km, layer height km,
Softness: how much density makes full shade, -).

Run headless while the live editor does not have the assets open:
  UnrealEditor-Cmd Autocraft.uproject -run=pythonscript -script=<abs path>
"""
import os

import unreal

DIR = "/Game/Sky"
SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Content-src", "sky"))

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

CLOUD_FNS = r"""
struct AcCloudFns {
    float remap(float x, float a, float b, float c, float d) { return c + (x - a) / (b - a) * (d - c); }
    // How much the sky is one deck (rain, storm) rather than clouds apart.
    float deck(float cover) { return smoothstep(0.7, 0.95, cover); }
    // The weather map's cover at a place (its R cut by the sky's cover);
    // past 0.85 cover a layer fills the gaps: rain cloud, thick and dark
    // where the fine clumps (B) are, thin rifts the light comes through
    // between them.
    float cover(float4 w, float c) {
        return max(saturate(remap(w.r, 1.0 - c, 1.0, 0.0, 1.0)), smoothstep(0.85, 1.0, c) * 0.55 * lerp(0.25, 1.0, w.b));
    }
    // How much of the shape a weather map's cover lets through: only its
    // densest part (0.55) while there is sky between clouds, so even a
    // wide cloud is carved into billows; all of it (1) under a full deck,
    // which must not tear open.
    float carve(float cover, float c) { return lerp(c, 1.0, deck(cover)); }
    // Where on the weather map: along the wind its patterns are stretched
    // (W.z: streets of cloud, not round blots) and move (A.x, km, wrapped at
    // a whole stretched tile, so it never jumps); W.xy the wind's direction.
    float2 weatherUv(float2 xy, float4 A, float4 B, float4 W) {
        float2 dir = normalize(W.xy + 1.0e-5);
        return float2((dot(xy, dir) - A.x) / W.z, dot(xy, float2(-dir.y, dir.x))) / B.z;
    }
    // How far fair-weather clouds sit up in the layer (in the layer's
    // height): Fine.z km over its bottom while there is sky between them,
    // none under a rain deck, whose ragged base hangs low (the bolts come
    // down from the layer's bottom). W.w: the layer's height, km.
    float lift(float cover, float4 Fine, float4 W) { return Fine.z / max(W.w, 0.1) * (1.0 - deck(cover)); }
    // Height profile over the layer (0 bottom, 1 top) for a cloud type
    // (0 a flat layer low down, 0.5 cumulus, 1 towering). A cumulus base
    // rises over its first 270 m (flat, not a knife edge: at 150 m a field
    // of far bases read as one straight cut), its top rounds off.
    float profile(float h, float t) {
        float layer = saturate(remap(h, 0.0, 0.05, 0.0, 1.0)) * saturate(remap(h, 0.1, 0.22, 1.0, 0.0));
        // Cumulus about as tall as wide (to ~1.4 km); towers are the storm's.
        float cumulus = saturate(remap(h, 0.0, 0.08, 0.0, 1.0)) * saturate(remap(h, 0.18, 0.42, 1.0, 0.0));
        float tower = saturate(remap(h, 0.0, 0.08, 0.0, 1.0)) * saturate(remap(h, 0.6, 1.0, 1.0, 0.0));
        return t < 0.5 ? lerp(layer, cumulus, t * 2.0) : lerp(cumulus, tower, t * 2.0 - 1.0);
    }
    // The cloud's body without its edge detail, 0-1 (the cloud and its
    // shadow share it). `km` world km, `H` height in the layer, `rep` the
    // shape noise's repeats per ShapeKm, `mip` the shape noise's mip.
    float body(float3 km, float H, float4 A, float4 B, float4 C, float4 W, float4 Fl, float rep, float mip,
               Texture2D WT, SamplerState WS, Texture3D ST, SamplerState SS) {
        float4 w = Texture2DSampleLevel(WT, WS, weatherUv(km.xy, A, B, W), 0.0);
        float cov = cover(w, B.x);
        if (cov <= 0.0) return 0.0;
        // The base up or down by Fl.y of the layer (A: where it sits); a
        // deck's base ragged too, by the fine clumps (400 m).
        float g = profile(H - (w.a - 0.5) * Fl.y - (w.b - 0.5) * 0.12 * deck(B.x), saturate(w.g + C.z));
        if (g <= 0.0) return 0.0;
        // Clouds lean downwind with height. `rep` whole repeats per ShapeKm,
        // so the offset's wrap at ShapeKm lands on the same noise.
        float3 sp = (km + float3(A.zw, 0.0) + float3(normalize(W.xy + 1.0e-5) * H * 0.4, 0.0)) / B.w * rep;
        float4 s = Texture3DSampleLevel(ST, SS, sp, mip);
        float fbm = s.g * 0.625 + s.b * 0.25 + s.a * 0.125;
        float base = saturate(remap(s.r, fbm - 1.0, 1.0, 0.0, 1.0)) * g;
        float d = saturate(remap(base, 1.0 - cov * carve(B.x, Fl.x), 1.0, 0.0, 1.0)) * cov;
        // Under a rain deck no holes to the sky (seen edge on, the rifts'
        // holes lined up into a clear band along the horizon): at least the
        // shape itself, thick where the fine clumps are, thin between.
        return max(d, base * smoothstep(0.85, 1.0, B.x) * lerp(0.2, 0.6, w.b));
    }
};
AcCloudFns F;
"""

CLOUD_HLSL = CLOUD_FNS + r"""
float3 km = P * 1.0e-5;
float dist = length(P - Cam) * 1.0e-5;
// Level of detail with distance: past ~8 km the fine billows and the
// erosion are smaller than a traced pixel and only shimmer, so far cloud
// goes back to the wide shape without erosion (0 near, 1 by 25 km).
float lod = smoothstep(8.0, 25.0, dist);
// Whole repeats only (a deck and far cloud 1, near D.w): a fraction would
// jump when the wind's offset wraps.
float rep = round(lerp(lerp(D.w, 1.0, F.deck(B.x)), 1.0, lod));
// The height over the cloud's own base level (fair weather sits higher).
float Hc = H - F.lift(B.x, Fine, W);
float d = F.body(km, Hc, A, B, C, W, Fl, rep, 0.0, WeatherTex, WeatherTexSampler, ShapeTex, ShapeTexSampler);
if (d <= 0.0) return 0.0;
if (Shadow <= 0.0 && lod < 1.0) {
    float3 dt = Texture3DSampleLevel(DetailTex, DetailTexSampler, (km + float3(A.zw * 2.0, 0.0)) / C.x, 0.0).rgb;
    float dfbm = dt.r * 0.625 + dt.g * 0.25 + dt.b * 0.125;
    // Up close (within Fine.x km) a second octave four times finer:
    // crisp billows on the masses overhead, not a soft mush.
    float near = 1.0 - smoothstep(Fine.x * 0.5, Fine.x, dist);
    if (near > 0.0) {
        float3 df = Texture3DSampleLevel(DetailTex, DetailTexSampler, (km + float3(A.zw * 2.0, 0.0)) * 4.0 / C.x, 0.0).rgb;
        dfbm = lerp(dfbm, dfbm * 0.6 + (df.r * 0.625 + df.g * 0.25 + df.b * 0.125) * 0.4, near);
    }
    // Billows on the top, wisps at the base (E.w: how far up the wisps go).
    float dm = lerp(dfbm, 1.0 - dfbm, saturate(Hc / max(E.w, 1.0e-3)));
    // Erode the thin edges, not the cores (D.y: how fast the bite fades
    // with density); harder up close (Fine.y).
    d = saturate(F.remap(d, dm * C.y * (1.0 + Fine.y * near) * (1.0 - lod) * saturate(1.0 - d * D.y), 1.0, 0.0, 1.0));
}
// A cloud's edge: density grows over D.x into it (a soft rim the sun
// lights up from behind; at 0.7 a cloud toward the sun read as a cut-out),
// and on to E.z times that in the core.
d = saturate(d / lerp(D.x, 1.0, F.deck(B.x))) * lerp(1.0, E.z, saturate(d * 2.0 - 1.0));
// Far cloud thins out (E.x to E.y km) into the haze, so the far band is
// not a stacked line of white cloudlets (at 12-45 km single puffs survived
// at the horizon). A rain deck stays whole all the way: thinned
// (or cut where the tracing ends), it opened onto the bright horizon behind
// it, a white strip under the dark deck.
float fade = 1.0 - smoothstep(E.x, E.y, dist) * (1.0 - F.deck(B.x));
// Over the gas giant the far clouds thin out, so it stays in sight
// (`Giant`: toward it, the cosine of its angular radius; AAcDaylight);
// a rain deck mostly not (a clear hole in a storm read as a fault).
float over = smoothstep(Giant.w - 0.05, Giant.w + 0.07, dot(normalize(P - Cam), Giant.xyz))
    * smoothstep(1.5, 4.5, dist) * (1.0 - 0.75 * F.deck(B.x));
return d * B.y * fade * (1.0 - 0.8 * over);
"""

# CloudD: the edge's depth to full density, erosion gone by 1/y of full
# density, the sky light left at a cloud's base, the shape noise's repeats
# per ShapeKm. CloudE: the far thinning (km from, to; a rain deck's from 40
# to 49), the core's density over the edge's, the height the base's wisps
# reach (in the layer). CloudFine: the fine erosion's reach (km) and how
# much harder it bites there, how far fair-weather clouds sit up in the
# layer (km; none under a deck).
CLOUD_LOOK = (1.0, 2.0, 0.45, 2.0)
CLOUD_LOOK_E = (8.0, 30.0, 2.0, 0.15)
# CloudF: how much of the shape noise's range a cloud keeps between clouds
# (the carve; the rest is sky), how far bases move up and down (in the
# layer), how much of the horizon's haze far cloud takes, and over how many
# km it grows to that.
CLOUD_LOOK_F = (0.65, 0.15, 0.7, 25.0)
CLOUD_FINE = (4.0, 1.0, 0.9, 0.0)
if os.environ.get("AC_CLOUD_LOOK"):  # trying looks: "x,y,z,w"
    CLOUD_LOOK = tuple(float(v) for v in os.environ["AC_CLOUD_LOOK"].split(","))
# The light (the Volumetric Advanced Output, wired from parameters so
# `ac.Daylight.CloudParam` can try others live): CloudP the phase (g, g2,
# blend toward g2, -), CloudM the multiple scattering (contribution,
# occlusion, eccentricity, -). The engine's sample clouds take g 0.8, a
# mild 0.17 lobe for 57.5 % of the light, multiple scattering 0.67/0.25/
# 0.18; a sharper forward lobe and more, more occluded multiple scattering
# give a brighter rim against the sun and darker bases (side by side,
# docs/weather.md "Clouds (2026-10-18)").
CLOUD_PHASE = (0.85, 0.25, 0.5, 0.0)
CLOUD_MS = (0.8, 0.4, 0.4, 0.0)

# The horizon's haze over far cloud: as the sky dome (M_AcSky) mixes its
# `Haze` in toward the horizon (full at 0°, gone `Haze.a` degrees up), the far
# clouds take it too, by Fl.z, growing over the first Fl.w km: they melt into
# the horizon's band instead of standing white against it.
HAZE_HLSL = r"""
float3 v = normalize(P - Cam);
float e = asin(clamp(v.z, -1.0, 1.0)) * 57.29578;
float rise = pow(1.0 - smoothstep(0.0, max(Haze.a, 1.0), e), 1.6);
return saturate(rise * Fl.z * smoothstep(0.0, max(Fl.w, 1.0e-3), length(P - Cam) * 1.0e-5));
"""

AO_HLSL = CLOUD_FNS + r"""
return lerp(lerp(D.z, 0.33, C.w), 1.0, smoothstep(0.0, 0.3, H - F.lift(B.x, Fine, W)));
"""

# A strike's light inside the cloud: a core a fifth of the radius across and
# a halo out to the radius, the light scattered through the cloud round it.
BOLT_HLSL = r"""
float r = length(P * 1.0e-5 - Bolt.xyz) / max(Bolt.w, 1.0e-3);
return 4.0 * exp(-r * r * 25.0) + exp(-r * 3.0);
"""

# The tracer's empty-space skip: matter is possible where the weather map
# has cover and the height profile there is not zero (the same base and
# raggedness as body(); fair weather's empty air under its lifted clouds is
# skipped, not marched through at full cost).
CONSERVATIVE_HLSL = CLOUD_FNS + r"""
float3 km = P * 1.0e-5;
float4 w = Texture2DSampleLevel(WeatherTex, WeatherTexSampler, F.weatherUv(km.xy, A, B, W), 0.0);
float Hc = H - F.lift(B.x, Fine, W) - (w.a - 0.5) * Fl.y - (w.b - 0.5) * 0.12 * F.deck(B.x);
return float3(F.cover(w, B.x) > 0.0 && F.profile(Hc, saturate(w.g + C.z)) > 0.0 ? 1.0 : 0.0, 0.0, 0.0);
"""

SHADOW_HLSL = CLOUD_FNS + r"""
float3 p = P * 1.0e-5;
float3 L = SL.xyz / max(length(SL.xyz), 1.0e-4);
float sum = 0.0;
[unroll] for (int i = 0; i < 4; i++) {
    // The lower 60 % of the clouds' heights, where they have their mass.
    float H = (float(i) + 0.5) * 0.15;
    float3 q = p + L * ((SC.x + (H + F.lift(B.x, Fine, W)) * SC.y - p.z) / max(L.z, 0.1));
    // The cloud's own body (no cover: zero, never a NaN that lights the map).
    sum += F.body(q, H, A, B, C, W, Fl, round(lerp(D.w, 1.0, F.deck(B.x))), 1.0, WeatherTex, WeatherTexSampler, ShapeTex, ShapeTexSampler);
}
float shade = 1.0 - exp(-sum * SC.z);
return 1.0 - SL.w * shade;
"""


def import_png(name, png):
    path = f"{DIR}/{name}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SRC, png))
    task.set_editor_property("destination_path", DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property("never_stream", True)
    assets.save_asset(path, only_if_is_dirty=False)
    return tex


def make_volume(name, atlas_tex, tile):
    path = f"{DIR}/{name}"
    # Kept if it exists (the material refers to it), its slices redone.
    vt = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if vt is None:
        vt = tools.create_asset(name, DIR, unreal.VolumeTexture, unreal.VolumeTextureFactory())
    vt.set_editor_property("srgb", False)
    vt.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    vt.set_editor_property("address_mode", unreal.TextureAddress.TA_WRAP)
    vt.set_editor_property("never_stream", True)
    # Each set re-slices the atlas (UVolumeTexture::PostEditChangeProperty);
    # the source texture's guess at the slice size is wrong for a 2:1 atlas.
    vt.set_editor_property("source2d_texture", atlas_tex)
    vt.set_editor_property("source2d_tile_size_x", tile)
    vt.set_editor_property("source2d_tile_size_y", tile)
    w = vt.blueprint_get_size_x() if hasattr(vt, "blueprint_get_size_x") else -1
    unreal.log(f"make_cloud_material: {name} slices {tile}, size x {w}")
    assets.save_asset(path, only_if_is_dirty=False)
    return vt


def fresh_material(name, domain):
    path = f"{DIR}/{name}"
    for p in (f"{DIR}/MI_{name[2:]}", path):
        if assets.does_asset_exist(p):
            assets.delete_asset(p)
    mat = tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", domain)
    return mat


class Graph:
    def __init__(self, mat):
        self.mat = mat

    def node(self, cls, x, y):
        return mel.create_material_expression(self.mat, cls, x, y)

    def link(self, a, a_out, b, b_in):
        if not mel.connect_material_expressions(a, a_out, b, b_in):
            raise RuntimeError(f"could not connect {b_in}")

    def out(self, a, a_out, prop):
        if not mel.connect_material_property(a, a_out, prop):
            raise RuntimeError(f"could not connect {prop}")

    def vec(self, name, default, x, y, alpha=True):
        vp = self.node(unreal.MaterialExpressionVectorParameter, x, y)
        vp.set_editor_property("parameter_name", name)
        vp.set_editor_property("default_value", unreal.LinearColor(*default))
        if not alpha:
            return vp
        ap = self.node(unreal.MaterialExpressionAppendVector, x + 200, y)
        self.link(vp, "", ap, "A")
        self.link(vp, "A", ap, "B")
        return ap

    def tex(self, name, texture, x, y, param=True):
        t = self.node(unreal.MaterialExpressionTextureObjectParameter if param else unreal.MaterialExpressionTextureObject, x, y)
        if param:
            t.set_editor_property("parameter_name", name)
        t.set_editor_property("texture", texture)
        return t

    def custom(self, code, desc, out_type, sources, x, y):
        c = self.node(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property("code", code)
        c.set_editor_property("description", desc)
        c.set_editor_property("output_type", out_type)
        ins = []
        for name, _, _ in sources:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        for name, src, src_out in sources:
            self.link(src, src_out, c, name)
        return c


def world_position(g, x, y):
    wp = g.node(unreal.MaterialExpressionWorldPosition, x, y)
    tr = g.node(unreal.MaterialExpressionTruncateLWC, x + 200, y)
    g.link(wp, "", tr, "")
    return tr


def build_cloud(weather, shape, detail):
    mat = fresh_material("M_AcCloud", unreal.MaterialDomain.MD_VOLUME)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("used_with_volumetric_cloud", True)
    g = Graph(mat)
    pos = world_position(g, -1600, 0)
    attr = g.node(unreal.MaterialExpressionCloudSampleAttribute, -1600, 150)
    a = g.vec("CloudA", (0, 0, 0, 0), -1600, 450)
    b = g.vec("CloudB", (0.6, 0.15, 12.0, 4.0), -1600, 600)
    c = g.vec("CloudC", (0.8, 0.35, 0.0, 0.0), -1600, 750)
    # The look (not set at run time): x the edge's depth to full density,
    # y how fast erosion fades into the core, z the sky light left at a
    # cloud's base (its top keeps all of it).
    d = g.vec("CloudD", CLOUD_LOOK, -1600, 1500)
    e = g.vec("CloudE", CLOUD_LOOK_E, -1600, 2100)
    fl = g.vec("CloudF", CLOUD_LOOK_F, -1600, 2250)
    fine = g.vec("CloudFine", CLOUD_FINE, -1600, 2700)
    # The wind's direction (xy), the weather map's stretch along it (z) and
    # the layer's height, km (w): AAcDaylight's.
    wind = g.vec("CloudW", (0.944, 0.330, 2.0, 3.4), -1600, 2400)
    # Off by default (a cosine past 1 is never reached).
    giant = g.vec("Giant", (0, 0, 1, 2), -1600, 1950)
    wt = g.tex("WeatherTex", weather, -1600, 900)
    st = g.tex("ShapeTex", shape, -1600, 1100, param=False)
    dt = g.tex("DetailTex", detail, -1600, 1300, param=False)
    eye = g.node(unreal.MaterialExpressionCameraPositionWS, -1600, 1800)
    dens = g.custom(CLOUD_HLSL, "Cloud density", unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                    [("P", pos, ""), ("Cam", eye, ""), ("H", attr, "NormAltitudeInLayer"), ("Shadow", attr, "ShadowSampleDistance"),
                     ("A", a, ""), ("B", b, ""), ("C", c, ""), ("D", d, ""), ("E", e, ""), ("Fl", fl, ""), ("W", wind, ""),
                     ("Fine", fine, ""), ("Giant", giant, ""),
                     ("WeatherTex", wt, ""), ("ShapeTex", st, ""), ("DetailTex", dt, "")], -900, 300)
    cons = g.custom(CONSERVATIVE_HLSL, "Cloud conservative density", unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    [("P", pos, ""), ("H", attr, "NormAltitudeInLayer"), ("A", a, ""), ("B", b, ""), ("C", c, ""),
                     ("W", wind, ""), ("Fl", fl, ""), ("Fine", fine, ""), ("WeatherTex", wt, "")], -900, 800)

    albedo = g.vec("Albedo", (0.95, 0.95, 0.95, 1), -600, 0, alpha=False)
    # The far cloud's own light gives way to the haze's (an emissive per
    # unit of extinction: a thick far cloud shows the haze's colour).
    haze_p = g.node(unreal.MaterialExpressionVectorParameter, -1600, 2550)
    haze_p.set_editor_property("parameter_name", "Haze")
    haze_p.set_editor_property("default_value", unreal.LinearColor(0.5, 0.55, 0.65, 12.0))
    haze4 = g.node(unreal.MaterialExpressionAppendVector, -1400, 2550)
    g.link(haze_p, "", haze4, "A")
    g.link(haze_p, "A", haze4, "B")
    hz = g.custom(HAZE_HLSL, "Cloud haze", unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                  [("P", pos, ""), ("Cam", eye, ""), ("Haze", haze4, ""), ("Fl", fl, "")], -900, 0)
    keep = g.node(unreal.MaterialExpressionOneMinus, -700, 0)
    g.link(hz, "", keep, "")
    lit_albedo = g.node(unreal.MaterialExpressionMultiply, -450, 0)
    g.link(albedo, "", lit_albedo, "A")
    g.link(keep, "", lit_albedo, "B")
    g.out(lit_albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    haze_dens = g.node(unreal.MaterialExpressionMultiply, -600, 250)
    g.link(dens, "", haze_dens, "A")
    g.link(hz, "", haze_dens, "B")
    haze_emit = g.node(unreal.MaterialExpressionMultiply, -450, 250)
    g.link(haze_p, "", haze_emit, "A")
    g.link(haze_dens, "", haze_emit, "B")
    g.out(dens, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    glow = g.vec("Glow", (0, 0, 0, 1), -600, 150, alpha=False)
    bolt = g.vec("Bolt", (0, 0, 0, 1), -1600, 1650)
    near = g.custom(BOLT_HLSL, "Cloud lightning", unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                    [("P", pos, ""), ("Bolt", bolt, "")], -900, 150)
    lit = g.node(unreal.MaterialExpressionMultiply, -450, 150)
    g.link(dens, "", lit, "A")
    g.link(near, "", lit, "B")
    emit = g.node(unreal.MaterialExpressionMultiply, -300, 150)
    g.link(glow, "", emit, "A")
    g.link(lit, "", emit, "B")
    emit_all = g.node(unreal.MaterialExpressionAdd, -150, 200)
    g.link(emit, "", emit_all, "A")
    g.link(haze_emit, "", emit_all, "B")
    g.out(emit_all, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Sky light occluded toward a cloud's base: dark undersides, bright tops.
    ao = g.custom(AO_HLSL, "Cloud sky occlusion", unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                  [("H", attr, "NormAltitudeInLayer"), ("B", b, ""), ("D", d, ""), ("C", c, ""), ("Fine", fine, ""),
                   ("W", wind, "")], -600, 450)
    g.out(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    adv = g.node(unreal.MaterialExpressionVolumetricAdvancedMaterialOutput, -300, 600)
    for prop, v in (("multi_scattering_approximation_octave_count", 2),
                    ("ground_contribution", os.environ.get("AC_CLOUD_GROUND", "1") == "1"),
                    ("ray_march_volume_shadow", os.environ.get("AC_CLOUD_RMSHADOW", "1") == "1")):
        adv.set_editor_property(prop, v)
    phase = g.vec("CloudP", CLOUD_PHASE, -900, 1100, alpha=False)
    ms = g.vec("CloudM", CLOUD_MS, -900, 1300, alpha=False)
    for pin, src, comp in (("PhaseG", phase, "R"), ("PhaseG2", phase, "G"), ("PhaseBlend", phase, "B"),
                           ("MultiScatteringContribution", ms, "R"), ("MultiScatteringOcclusion", ms, "G"),
                           ("MultiScatteringEccentricity", ms, "B")):
        g.link(src, comp, adv, pin)
    g.link(cons, "", adv, "ConservativeDensity")
    mel.recompile_material(mat)
    assets.save_asset(mat.get_path_name(), only_if_is_dirty=False)

    mi = tools.create_asset("MI_AcCloud", DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.update_material_instance(mi)
    assets.save_asset(mi.get_path_name(), only_if_is_dirty=False)
    return mi


def build_shadow(weather, shape):
    mat = fresh_material("M_AcCloudShadow", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    g = Graph(mat)
    pos = world_position(g, -1200, 0)
    a = g.vec("CloudA", (0, 0, 0, 0), -1200, 150)
    b = g.vec("CloudB", (0.6, 0.15, 12.0, 4.0), -1200, 300)
    c = g.vec("CloudC", (2.0, 0.35, 0.0, 0.0), -1200, 450)
    sl = g.vec("ShadowL", (0.3, 0.2, 0.9, 0.5), -1200, 600)
    sc = g.vec("ShadowC", (1.6, 3.4, 2.5, 0.0), -1200, 750)
    d = g.vec("CloudD", CLOUD_LOOK, -1200, 1300)
    fl = g.vec("CloudF", CLOUD_LOOK_F, -1200, 1450)
    wind = g.vec("CloudW", (0.944, 0.330, 2.0, 3.4), -1200, 1600)
    fine = g.vec("CloudFine", CLOUD_FINE, -1200, 1750)
    wt = g.tex("WeatherTex", weather, -1200, 900)
    st = g.tex("ShapeTex", shape, -1200, 1100, param=False)
    sh = g.custom(SHADOW_HLSL, "Cloud shadow", unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                  [("P", pos, ""), ("A", a, ""), ("B", b, ""), ("C", c, ""), ("D", d, ""), ("Fl", fl, ""), ("W", wind, ""), ("Fine", fine, ""), ("SL", sl, ""), ("SC", sc, ""),
                   ("WeatherTex", wt, ""), ("ShapeTex", st, "")], -600, 150)
    g.out(sh, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    assets.save_asset(mat.get_path_name(), only_if_is_dirty=False)
    mi = tools.create_asset("MI_AcCloudShadow", DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.update_material_instance(mi)
    assets.save_asset(mi.get_path_name(), only_if_is_dirty=False)
    return mi


def build():
    weather = import_png("T_AcCloudWeather", "cloud_weather.png")
    shape_src = import_png("T_AcCloudShapeAtlas", "cloud_shape.png")
    detail_src = import_png("T_AcCloudDetailAtlas", "cloud_detail.png")
    shape = make_volume("VT_AcCloudShape", shape_src, 128)
    detail = make_volume("VT_AcCloudDetail", detail_src, 32)
    cloud = build_cloud(weather, shape, detail)
    shadow = build_shadow(weather, shape)
    return cloud, shadow


for built in build():
    unreal.log(f"make_cloud_material: built {built.get_path_name()}")
