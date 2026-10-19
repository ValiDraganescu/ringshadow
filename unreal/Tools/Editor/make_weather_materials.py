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
  (`RainC.w`), so light rain shows a few, a storm all of them,
- and when its middle or either end is in a Citadel's hall (a shelter
  box, MPC_AcWorld `Shelter0..3`, `ShelterLow`, `ShelterHigh`: AcShelter.h),
  so no rain falls indoors and, seen through the open door, it stops at the
  doorway; it still falls on the roof and past the walls.
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
// Not in a Citadel's hall: its middle and both ends (AcShelter.h).
float3 e0 = ctr + ax * (len * 0.5), e1 = ctr - ax * (len * 0.5);
%SHELTER%pos = shelter + shelterTop + shelterBottom > 0.0 ? ctr : pos;
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
    world = RM.make_world_mpc()
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

    shelter = RM.shelter_hlsl("ctr") + RM.shelter_hlsl("e0", "shelterTop") + RM.shelter_hlsl("e1", "shelterBottom")
    wpo = RM.custom(g, "AcRainFall", WPO_HLSL.replace("%SHELTER%", shelter), [
        ("L", RM.local_position(g), ""),
        ("W", g.node(unreal.MaterialExpressionWorldPosition), ""),
        ("UV", uv, ""),
        ("Cam", cam, ""),
        ("C", c, "RGBA"),
        ("Box", box, "RGBA"),
        ("Off", off, "RGBA"),
        ("Vel", vel, "RGBA"),
    ] + RM.shelter_inputs(g, world))
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
struct AcGlassFns {
    float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
    float h3(float3 p) { return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453); }
    // Value noise; its lattice wraps every 256 (the flow's clock does too).
    float vn(float2 p) {
        float2 i = floor(p);
        float2 f = frac(p);
        f = f * f * (3.0 - 2.0 * f);
        float2 a = fmod(i, 256.0);
        float2 b = fmod(i + 1.0, 256.0);
        return lerp(lerp(h(a), h(float2(b.x, a.y)), f.x), lerp(h(float2(a.x, b.y)), h(b), f.x), f.y);
    }
    // Where the blade last crossed glass point q: x how long ago (s), y the
    // water landed there since, z the pass that wiped it (a seed; -1 never).
    float3 Wipe(float2 q, float4 A, float4 B, float4 S, float4 Arc) {
        float2 rel = q - float2(0.0, Arc.x);
        float3 never = float3(1e5, S.z, -1.0);
        if (dot(rel, rel) > Arc.y * Arc.y) return never;
        float rr = saturate((atan2(rel.x, -rel.y) - Arc.z) / (Arc.w - Arc.z));
        float u = acos(1.0 - 2.0 * rr) * 0.318309886;
        [unroll] for (int k = 0; k < 2; k++) {
            float4 W = k == 0 ? A : B;
            if (W.y <= 0.0) return never;
            float back = W.x - W.y * (2.0 - u);
            float outp = W.x - W.y * u;
            float at = back >= 0.0 ? back : outp;
            if (at >= 0.0) {
                float span = max(W.x - max(W.x - 2.0 * W.y, 0.0), 1e-4);
                float n = k == 0 ? S.x : S.y;
                return float3(at, lerp(W.z, W.w, saturate((W.x - at) / span)), n * 2.0 + (back >= 0.0 ? 1.0 : 0.0));
            }
        }
        return never;
    }
    // Water landing a second, `a` seconds ago (the history at 0.5, 1, 2, 4 s).
    float Landing(float a, float4 H) {
        if (a < 0.5) return H.x * 2.0;
        if (a < 1.0) return (H.y - H.x) * 2.0;
        if (a < 2.0) return H.z - H.y;
        return (H.w - H.z) * 0.5;
    }
    float smin(float a, float b, float k) {
        float t = saturate(0.5 + 0.5 * (b - a) / k);
        return lerp(b, a, t) - k * t * (1.0 - t);
    }
};
AcGlassFns G;
float aspect = Size.x / max(Size.y, 1.0);
// Glass coordinates: x from the view's middle, y down, both in view heights.
float2 q = float2((VUV.x - 0.5) * aspect, VUV.y);
float T = Air.w;
float3 sky = float3(0.72, 0.78, 0.86) * (0.12 + 0.88 * Air.z) + float3(2.0, 2.1, 2.5) * Air.y;
float glitter = 1.0 + 5.0 * Air.y;
float3 wpix = G.Wipe(q, A, B, Seed, Arc);
float apix = wpix.x;
float wp = wpix.y;
float sheet = smoothstep(0.55, 0.95, Rain);

// What the water does to this pixel: where it shows the scene from (q
// units), its dark edges, its highlights, how much it dims and smears.
float2 off = float2(0.0, 0.0);
float rim = 0.0;
float spec = 0.0;
float wet = 0.0;
float smear = 0.0;
float3 Lt = normalize(float3(-0.35, -0.6, 0.72));

// Rivulets: lanes down the glass, sheared by the wind; in each a heavy
// drop now and then lets go, runs down meandering and leaves a wet track.
float shear = Air.x * 0.5;
float u = q.x - shear * q.y;
const float LW = 0.05;
const float P = 1.6;
float field = 1.0;
float best = 1.0;
float2 rn = float2(0.0, 0.0);
float rs = 0.0;
[unroll] for (int dl = -1; dl <= 1; dl++) {
    float lane = floor(u / LW) + float(dl);
    float hl = G.h(float2(lane, 7.7));
    [unroll] for (int j = 0; j < 2; j++) {
        float slot = floor(T / P - hl) - float(j);
        float age = T - (slot + hl + 0.6 * G.h(float2(slot, lane * 1.37))) * P;
        if (age < 0.0) continue;
        float density = saturate((G.Landing(age, Hist) - 0.2) * 0.45);
        if (G.h(float2(lane, slot)) >= density) continue;
        float big = G.h(float2(slot * 1.7, lane));
        float x0 = (lane + 0.3 + 0.4 * G.h(float2(lane, slot * 2.3))) * LW;
        float y0 = -0.05 + 0.8 * G.h(float2(slot * 3.1, lane + 0.5));
        float hold = 0.3 + 0.9 * big * (1.0 - 0.6 * sheet);
        float V = 0.16 + 0.22 * big + 0.3 * sheet;
        float run = max(age - hold, 0.0);
        // Stick and slip.
        float yH = y0 + V * (run - 0.04 * sin(run * 9.0 + big * 6.0) * min(run, 1.0));
        float hr = LW * (0.13 + 0.12 * big) * (1.0 + 0.7 * sheet);
        if (q.y < y0 - hr || q.y > yH + hr * 1.6) continue;
        float amp = LW * 0.32;
        float zz = q.y * 18.0 + big * 40.0;
        float n0 = G.h(float2(lane * 3.1 + floor(zz), slot));
        float n1 = G.h(float2(lane * 3.1 + floor(zz) + 1.0, slot));
        float fz = frac(zz);
        float xp = x0 + (lerp(n0, n1, lerp(fz, fz * fz * (3.0 - 2.0 * fz), 0.4)) - 0.5) * 2.0 * amp;
        float slope = (n1 - n0) * 2.0 * amp * 18.0;
        float zh = yH * 18.0 + big * 40.0;
        float fh = frac(zh);
        float xh = x0 + (lerp(G.h(float2(lane * 3.1 + floor(zh), slot)), G.h(float2(lane * 3.1 + floor(zh) + 1.0, slot)), lerp(fh, fh * fh * (3.0 - 2.0 * fh), 0.4)) - 0.5) * 2.0 * amp;
        // Swept away: the blade passed its head since it was born.
        if (G.Wipe(float2(xh + shear * yH, yH), A, B, Seed, Arc).x < age) continue;
        float tw = LW * (0.035 + 0.04 * big) * (1.0 + 3.0 * sheet);
        if (q.y <= yH) {
            // The track, laid as the head passed; thinner as it ages.
            float laid = age - hold - (q.y - y0) / V;
            if (laid <= apix) {
                float w = tw * lerp(1.0, 0.55, saturate(laid * 0.5));
                float du = (u - xp) / sqrt(1.0 + slope * slope);
                float d = abs(du) - w;
                field = G.smin(field, d, 0.004);
                if (d < best) { best = d; rn = float2(du / w, 0.0); rs = w; }
            }
        }
        // The head: a heavy drop, a little taller than wide.
        float2 hd = float2(u - xh, (q.y - yH) * 0.8);
        float dh = length(hd) - hr;
        field = G.smin(field, dh, 0.006);
        if (dh < best) { best = dh; rn = hd / hr; rs = hr; }
    }
}
float trailNear = 1.0 - smoothstep(0.0, 0.02, field);
if (field < 0.0) {
    float2 n2 = rn / max(1.0, length(rn));
    float dd = dot(n2, n2);
    float3 n3 = float3(n2, sqrt(max(1.0 - dd, 0.0)));
    off = -n2 * rs * 3.2;
    rim = smoothstep(0.55, 1.0, dd) * 0.9;
    spec = pow(saturate(dot(n3, Lt)), 40.0) * 1.1;
    wet = 1.0;
}

// Beads: small, middling and big ones in cells of the glass, each landing
// when enough water has come since the blade last passed, growing as more
// lands; at the end of its life a heavy one slides off and a new one lands.
// A runner's track keeps only the small ones.
[unroll] for (int L = 0; L < 3; L++) {
    float cells = L == 0 ? 40.0 : (L == 1 ? 17.0 : 8.0);
    float Wl = L == 0 ? 0.7 : (L == 1 ? 1.8 : 4.0);
    float Cl = L == 0 ? 2.0 : (L == 1 ? 3.0 : 5.0);
    float Rl = L == 0 ? 0.3 : (L == 1 ? 0.34 : 0.38);
    float K = L == 0 ? 2.2 : (L == 1 ? 3.0 : 3.8);
    if (L > 0 && trailNear > 0.6) continue;
    float2 p = q * cells + float(L) * 7.31;
    float2 ci = floor(p);
    float2 f = frac(p) - 0.5;
    float3 wc = G.Wipe((ci + 0.5 - float(L) * 7.31) / cells, A, B, Seed, Arc);
    float thr = -Wl * log(1.0 - 0.985 * G.h3(float3(ci, wc.z + float(L) * 13.0)));
    if (L == 0) thr *= 1.0 - 0.7 * trailNear;
    float m = wc.y - thr;
    if (m <= 0.0) continue;
    float k = floor(m / Cl);
    m -= k * Cl;
    // Landed before the blade passed this pixel: gone with it.
    if (m > wp + 0.02) continue;
    float hk = G.h3(float3(ci, wc.z * 3.1 + k * 17.0 + float(L)));
    float grow = smoothstep(0.0, Cl * 0.7, m);
    float slide = saturate((m / Cl - 0.88) / 0.12);
    float r = Rl * (0.5 + 0.5 * hk) * (0.45 + 0.55 * grow) * (1.0 - 0.4 * slide);
    float2 c = (float2(G.h3(float3(ci, k + 3.1)), G.h3(float3(ci, k + 7.9))) - 0.5) * 2.0 * (0.5 - r);
    c.y += slide * slide * 0.5;
    float2 d = (f - c) / r;
    d.y *= d.y < 0.0 ? 1.08 : lerp(1.0, 0.82, grow * float(L) * 0.5);
    float dd = dot(d, d);
    if (dd < 1.0) {
        float3 n3 = float3(d, sqrt(1.0 - dd));
        off = -d * (r / cells) * K;
        rim = smoothstep(0.5, 1.0, dd) * (L == 0 ? 0.6 : 0.85);
        spec = pow(saturate(dot(n3, Lt)), 50.0) * (L == 0 ? 0.5 : 1.0);
        wet = 1.0;
    }
}

// Sheets in heavy rain: a running film over the glass, flowing down and
// with the wind, smearing the view.
float fa = sheet * smoothstep(0.6, 2.5, wp);
if (fa > 0.01) {
    float2 fq = float2(u * 9.0, q.y * 3.0 - fmod(T * 1.1, 256.0));
    float2 fq2 = float2(u * 23.0 + 5.2, q.y * 7.0 - fmod(T * 1.7, 256.0));
    const float e = 0.03;
    float n = G.vn(fq) + 0.45 * G.vn(fq2);
    float nx = G.vn(fq + float2(e, 0.0)) + 0.45 * G.vn(fq2 + float2(e * 2.5, 0.0));
    float ny = G.vn(fq + float2(0.0, e)) + 0.45 * G.vn(fq2 + float2(0.0, e * 2.3));
    float2 g = float2(nx - n, ny - n) / e;
    off += g * 0.0035 * fa;
    smear = fa;
    wet = max(wet, fa * 0.6);
    spec += pow(saturate(0.5 + 0.12 * (g.y - g.x)), 8.0) * 0.5 * fa;
}

// The blade: its ridge of water pushed ahead, the smear behind.
float2 rel = q - float2(0.0, Arc.x);
float th = Blade.x;
float mv = Blade.y;
float2 dir = float2(sin(th), -cos(th));
float2 nrm = float2(cos(th), sin(th));
float s = dot(rel, dir);
float dn = dot(rel, nrm);
float R = Arc.y;
float bw = Blade.w * lerp(0.65, 1.0, smoothstep(0.22 * R, 0.3 * R, s)) * (1.0 - 0.25 * smoothstep(R - 0.05, R, s));
float along = step(0.0, s) * (1.0 - smoothstep(R, R + 0.006, s));
if (mv != 0.0 && along > 0.0 && s > 0.03) {
    float prog = mv > 0.0 ? saturate((th - Arc.z) / (Arc.w - Arc.z)) : saturate((Arc.w - th) / (Arc.w - Arc.z));
    float load = saturate(wp / 3.0) * (0.35 + 0.65 * prog);
    float rw = 0.003 + 0.014 * load;
    float ahead = dn * mv - bw;
    if (ahead > 0.0 && ahead < rw && load > 0.02) {
        float x = ahead / rw * 2.0 - 1.0;
        off = nrm * mv * x * rw * 3.0;
        rim = smoothstep(0.5, 1.0, x * x) * 0.8;
        spec = pow(saturate(1.0 - abs(x + 0.3) * 2.5), 3.0) * (0.6 + 0.8 * load);
        wet = 1.0;
        smear = 0.0;
    }
}
if (wpix.z >= 0.0 && apix < Blade.z) {
    float sm = 1.0 - apix / Blade.z;
    sm *= sm;
    float st = G.h(float2(floor(length(rel) * 900.0), 3.3));
    off += normalize(rel) * (st - 0.5) * 0.002 * sm;
    spec += smoothstep(0.55, 0.9, st) * sm * 0.12;
}

// The scene through the water.
float2 vs = VUV + float2(off.x / aspect, off.y);
float3 col = SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(vs, 14), 14), 14, true).rgb;
if (smear > 0.01) {
    float2 sv = float2(0.0, 0.007 * smear);
    col += SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(vs - sv, 14), 14), 14, true).rgb;
    col += SceneTextureLookup(ClampSceneTextureUV(ViewportUVToSceneTextureUV(vs + sv, 14), 14), 14, true).rgb;
    col /= 3.0;
}
col *= (1.0 - 0.08 * wet) * (1.0 - 0.55 * saturate(rim));
col += sky * spec * glitter;

// The blade over the glass: its soft shadow, the dark arm and blade, the
// rubber's wet lip on the side it moves toward.
float2 relS = rel - float2(0.008, 0.012);
float sS = dot(relS, dir);
float shadow = step(0.0, sS) * (1.0 - smoothstep(R - 0.02, R + 0.02, sS)) * (1.0 - smoothstep(bw, bw + 0.02, abs(dot(relS, nrm))));
col *= 1.0 - 0.45 * shadow;
float body = along * (1.0 - smoothstep(bw - 0.002, bw + 0.002, abs(dn)));
float lead = dn * (mv != 0.0 ? mv : 1.0);
float lip = along * smoothstep(bw - 0.0035, bw - 0.0015, lead) * (1.0 - smoothstep(bw - 0.0005, bw + 0.0015, lead));
float edge = along * smoothstep(bw - 0.0025, bw - 0.0012, -lead) * (1.0 - smoothstep(bw - 0.0006, bw + 0.001, -lead));
float3 bladeCol = float3(0.012, 0.013, 0.015) + sky * (0.03 + 0.3 * lip + 0.08 * edge) * glitter;
return lerp(col, bladeCol, body);
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

    def vec(name, default):
        return g.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                      default_value=unreal.LinearColor(*default), group="Ac")

    e = RM.custom(g, "AcLensRain", LENS_HLSL, [
        ("VUV", g.node(unreal.MaterialExpressionScreenPosition), "ViewportUV"),
        ("Size", sizes, ""),
        ("Rain", g.node(unreal.MaterialExpressionScalarParameter, parameter_name="LensRain", default_value=0.0, group="Ac"), ""),
        ("A", vec("GlassA", (0, 0, 0, 0)), "RGBA"),
        ("B", vec("GlassB", (0, 0, 0, 0)), "RGBA"),
        ("Seed", vec("GlassSeed", (0, 0, 0, 0)), "RGBA"),
        ("Arc", vec("GlassArc", (1.06, 1.12, -1.5707963, 1.5707963)), "RGBA"),
        ("Blade", vec("GlassBlade", (-1.5707963, 0, 0.4, 0.0085)), "RGBA"),
        ("Hist", vec("GlassHist", (0, 0, 0, 0)), "RGBA"),
        ("Air", vec("GlassAir", (0, 0, 1, 0)), "RGBA"),
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
