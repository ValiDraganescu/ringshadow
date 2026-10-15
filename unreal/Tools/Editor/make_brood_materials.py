"""The Brood's painted materials (N1b, docs/creatures.md "Look and feel"): what the blockout's flat lamps could not
show. Rerunnable: the masters are rebuilt in place. The brood model scripts (brood_kit.materials) make their instances
on these masters by the parent names below; ac_modelkit.make_material_instance re-parents them.

- /Game/Materials/M_AcBroodEgg (parent `broodEgg`): an egg pod or sac lit from inside. Lit, opaque: a pale glassy
  shell (BaseColorTint, low roughness, so the sun puts a highlight on it) over a glow that is deepest where the shell
  is thickest toward the eye and lighter at the rim (it reads translucent), mottled, crossed by veins (VeinColor,
  isolines of a warped noise in the part's own space, filtered by the pixel's footprint). The glow is the instance's
  EmissiveColor; big surfaces keep it near half the hue so daylight doesn't wash it to white. × custom data 1 (the
  pose's glow) × MPC_AcTeams.EmissiveBoost.
- /Game/Materials/M_AcBroodOpal (parent `broodOpal`): opal chitin (quills, ore crystals, flecks). Lit, opaque:
  a dark pearly base and a thin-film sheen (teal, violet, gold) that follows the view angle, brightest at grazing
  angles, banded along the part's own axes, so a quill reads iridescent and never white. EmissiveColor adds a
  steady glow (0 for none).
- /Game/Materials/M_AcBroodGlass (parent `broodGlass`): a glass sac or a wing membrane. Translucent, two-sided,
  lit: BaseColorTint tinted, Opacity at the face rising to the rim, EmissiveColor glowing through it (rim-weighted),
  VeinColor veins. What sits inside it (a Lodeback's ore) shows through.

Run it on its own (the brood model scripts run it too, when a master is missing):

    uv run unreal/Tools/hidden_run.py --out <dir> --python unreal/Tools/Editor/make_brood_materials.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ac_models as A  # noqa: E402
import make_materials as MM  # noqa: E402
import make_resource_materials as R  # noqa: E402

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1

M_EGG = A.MASTER_DIR + "/M_AcBroodEgg"
M_OPAL = A.MASTER_DIR + "/M_AcBroodOpal"
M_GLASS = A.MASTER_DIR + "/M_AcBroodGlass"
# The modelkit's parent names → masters.
MASTERS = {"broodEgg": M_EGG, "broodOpal": M_OPAL, "broodGlass": M_GLASS}

# Shared noise and veins (the ore's M_Opal technique, make_resource_materials.OPAL_HLSL).
LIB = """
struct AcBrood
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
    // A vein network: the isoline Noise = 0.5 of a warped field, by its
    // distance over the gradient (even width), filtered by the footprint fp.
    float Veins(float3 p, float w, float fp, out float nv)
    {
        float3 wv = float3(Noise(p * 0.5 + 11.0), Noise(p * 0.5 + 23.0), Noise(p * 0.5 + 37.0)) - 0.5;
        float3 q = p + wv * 1.6;
        nv = Noise(q);
        const float e = 0.03;
        float3 gr = float3(Noise(q + float3(e, 0, 0)), Noise(q + float3(0, e, 0)), Noise(q + float3(0, 0, e))) - nv;
        float d = abs(nv - 0.5) / max(length(gr) / e, 0.15);
        return smoothstep(w + fp, max(w - fp, 0.0), d) * saturate(w * 2.5 / (w + fp));
    }
};
AcBrood b;
"""

EGG_HLSL = LIB + """
float3 n = normalize(N), v = normalize(V);
float ndv = saturate(dot(n, v));
float3 p = LP * PatternScale;
float fp = Depth * PatternScale * 0.0016;
float nv;
float vein = b.Veins(p, 0.05, fp, nv);
// Far off the veins fade to their average (no shimmer).
float far = smoothstep(0.08, 0.35, fp);
vein = lerp(vein, 0.12, far);
float mott = 0.7 + 0.6 * b.Noise(p * 0.7 + 3.0);
// Lit from inside: deepest through the thick middle, a lighter haze at the
// rim where the shell is thin (the shell's own pale colour, lit).
float core = pow(ndv, 1.4);
float rim = pow(1.0 - ndv, 2.2);
float pulse = 0.92 + 0.08 * sin(T * 1.7 + nv * 12.0);
float3 e = Glow * (0.3 + 0.9 * core) * mott * pulse;
e += lerp(Glow, Base * 2.0, 0.5) * rim * 0.55;
e += Vein * vein * (0.5 + 0.9 * core);
// Night: the glow carries further.
e *= (1.0 + 0.6 * Dark) * Scale * Boost;
Diffuse = Base * (1.0 - 0.6 * vein);
return e;
"""

OPAL_HLSL = LIB + """
float3 n = normalize(N), v = normalize(V);
float ndv = saturate(dot(n, v));
float3 p = LP * PatternScale;
float fp = Depth * PatternScale * 0.0016;
float far = smoothstep(0.1, 0.4, fp);
float3 r = reflect(-v, n);
// Bands along the part (a quill's length) and a slow mottle: the film's hue
// follows them, the view angle and the reflected direction.
float band = b.Noise(float3(p.x * 0.6, p.y * 0.6, p.z * 2.2));
float hue = band * 1.4 + (1.0 - ndv) * 1.3 + r.z * 0.35 + r.x * 0.2 + T * 0.01;
float3 film = b.Opal(hue);
float grazing = pow(1.0 - ndv, 2.0);
float3 e = film * film * (0.05 + 0.55 * grazing) * lerp(1.0, 0.6, far) * Gain;
e += Glow;
e *= (1.0 + 0.5 * Dark) * Scale * Boost;
Diffuse = lerp(Base, Base * (0.6 + 0.8 * film), 0.5);
return e;
"""

GLASS_HLSL = LIB + """
float3 n = normalize(N), v = normalize(V);
float ndv = abs(dot(n, v));
float3 p = LP * PatternScale;
float fp = Depth * PatternScale * 0.0016;
float nv;
float vein = b.Veins(p, 0.04, fp, nv);
vein = lerp(vein, 0.1, smoothstep(0.08, 0.35, fp));
float rim = pow(1.0 - ndv, 2.0);
float3 e = Glow * (0.35 + 0.9 * rim) + Vein * vein;
e *= (1.0 + 0.6 * Dark) * Scale * Boost;
Alpha = saturate(Opacity + (1.0 - Opacity) * rim * 0.85 + vein * 0.5);
Diffuse = Base;
return e;
"""


def log(msg):
    unreal.log("[make_brood_materials] " + msg)


def inputs(g, mpc_world, extra):
    teams = MM.load(A.MPC_TEAMS)
    boost = g.mpc(teams, "EmissiveBoost") if teams else g.scalar("EmissiveBoost", 1.0)
    return [
        ("LP", R.local_position(g), ""),
        ("N", g.node(unreal.MaterialExpressionVertexNormalWS), ""),
        ("V", g.node(unreal.MaterialExpressionCameraVectorWS), ""),
        ("T", g.node(unreal.MaterialExpressionTime), ""),
        ("Depth", g.node(unreal.MaterialExpressionPixelDepth), ""),
        ("Dark", g.mpc(mpc_world, "Dark"), ""),
        ("Base", g.vector("BaseColorTint", (0.3, 0.2, 0.4)), ""),
        ("Glow", g.vector("EmissiveColor", (0, 0, 0)), ""),
        ("Scale", g.custom_data(1, "EmissionScale", 1.0), ""),
        ("Boost", boost, ""),
    ] + extra


def build(path, code, shading, blend, extra, outputs, wire):
    mpc_world = R.make_world_mpc()
    mat, g = R.new_master(path, shading, blend)
    c = R.custom(g, os.path.basename(path), code, inputs(g, mpc_world, extra(g)), outputs=outputs)
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(c, "Diffuse", unreal.MaterialProperty.MP_BASE_COLOR)
    wire(mat, g, c)
    usages = [unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES]
    if blend == unreal.BlendMode.BLEND_OPAQUE:
        usages.append(unreal.MaterialUsage.MATUSAGE_NANITE)
    R.finish(mat, path, usages)
    EAL.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_brood_materials.py")
    EAL.save_loaded_asset(mat)
    log("master " + path)
    return mat


def lit(mat, g, c):
    g.out(g.scalar("Metallic", 0.0), "", unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar("Roughness", 0.2), "", unreal.MaterialProperty.MP_ROUGHNESS)


def glass(mat, g, c):
    mat.set_editor_property("two_sided", True)
    lit(mat, g, c)
    g.out(c, "Alpha", unreal.MaterialProperty.MP_OPACITY)


def build_all():
    MM.ensure_dir(A.MASTER_DIR)
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    DL = unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    out = {}
    out[M_EGG] = build(M_EGG, EGG_HLSL, DL, unreal.BlendMode.BLEND_OPAQUE,
                       lambda g: [("Vein", g.vector("VeinColor", (0.1, 0.9, 0.75)), ""),
                                  ("PatternScale", g.scalar("PatternScale", 0.04), "")],
                       [("Diffuse", F3)], lit)
    out[M_OPAL] = build(M_OPAL, OPAL_HLSL, DL, unreal.BlendMode.BLEND_OPAQUE,
                        lambda g: [("Gain", g.scalar("OpalGain", 1.0), ""),
                                   ("PatternScale", g.scalar("PatternScale", 0.05), "")],
                        [("Diffuse", F3)], lit)
    out[M_GLASS] = build(M_GLASS, GLASS_HLSL, DL, unreal.BlendMode.BLEND_TRANSLUCENT,
                         lambda g: [("Vein", g.vector("VeinColor", (0.1, 0.9, 0.75)), ""),
                                    ("PatternScale", g.scalar("PatternScale", 0.04), ""),
                                    ("Opacity", g.scalar("Opacity", 0.35), "")],
                         [("Diffuse", F3), ("Alpha", F1)], glass)
    return out


def ensure():
    """The masters, built when one is missing (the model scripts call this)."""
    if all(EAL.does_asset_exist(p) for p in MASTERS.values()):
        return {p: unreal.load_asset(p) for p in MASTERS.values()}
    return build_all()


if __name__ == "__main__":
    build_all()
