"""A live readout on a model (C4, the Mule's load screen: docs/haul-and-ferry.md section 4): /Game/Materials/M_AcReadout,
unlit, opaque, drawn on a `panel` (ac_modelkit: its +X face carries UV 0..1, u right, v down). It reads the part's
custom data 1 (the pose's emission scale, AcPose `Emission[part]`) as the number to show, so a pose shows a number by
setting the part's emission to it, with no new component and no texture per value:

- Mode 0, digits: `Cells` seven-segment digits, right aligned, leading zeros blank, the value rounded (a value over
  the cells shows all nines). Lit segments in `OnColor`, unlit ones as faint ghosts (`OffColor`) on `BackColor`.
- Mode 1, a bar: `Cells` cells left to right, cell i lit when i < value (the value counts lit cells).

A model takes it as `material(name, parent="readout", color=(0, 0, 0), params={"Mode": 0, "Cells": 4,
"OnColor": (r, g, b), ...})` (ac_modelkit.make_material_instance re-parents the instance onto it).

    uv run unreal/Tools/hidden_run.py --out <dir> --python unreal/Tools/Editor/make_readout_material.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ac_models as A  # noqa: E402
import make_materials as MM  # noqa: E402
import make_resource_materials as R  # noqa: E402

EAL = unreal.EditorAssetLibrary

M_READOUT = A.MASTER_DIR + "/M_AcReadout"
MASTERS = {"readout": M_READOUT}

HLSL = """
float n = max(floor(Cells + 0.5), 1.0);
float2 uv = saturate(UV);
float cell = min(floor(uv.x * n), n - 1.0);
float2 c = float2(frac(uv.x * n), uv.y);
float val = max(floor(Value + 0.5), 0.0);
float3 col = Back;
float scan = 0.92 + 0.08 * sin(uv.y * 180.0);
if (Mode < 0.5)
{
    // in integers: the GPU's pow(10, 1) lands a hair under 10 and 380 read 370
    uint iv = (uint)val, ip = (uint)(n - 1.0 - cell), cells = (uint)n;
    uint pw = 1u, top = 1u;
    for (uint i = 0u; i < cells; ++i) { if (i < ip) pw *= 10u; top *= 10u; }
    uint dgt = iv >= top ? 9u : (iv / pw) % 10u;
    bool blank = ip > 0u && iv < pw;
    const uint Masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    uint bits = blank ? 0u : Masks[dgt];
    // the glyph box: x -1..1, y -1 (top) .. 1, a slight slant
    float2 q = (c - 0.5) * float2(2.0 / 0.62, 2.0 / 0.80);
    q.x += q.y * 0.12;
    float t = 0.19;
    float aa = max(fwidth(q.y), 0.02);
    float s[7];
    // a top, b top right, c bottom right, d bottom, e bottom left, f top left, g middle
    s[0] = (1.0 - smoothstep(t - aa, t, abs(q.y + 1.0))) * (1.0 - smoothstep(1.0 - t - aa, 1.0 - t, abs(q.x)));
    s[6] = (1.0 - smoothstep(t - aa, t, abs(q.y))) * (1.0 - smoothstep(1.0 - t - aa, 1.0 - t, abs(q.x)));
    s[3] = (1.0 - smoothstep(t - aa, t, abs(q.y - 1.0))) * (1.0 - smoothstep(1.0 - t - aa, 1.0 - t, abs(q.x)));
    float up = (1.0 - smoothstep(0.5 - t - aa, 0.5 - t, abs(q.y + 0.5)));
    float dn = (1.0 - smoothstep(0.5 - t - aa, 0.5 - t, abs(q.y - 0.5)));
    float rt = 1.0 - smoothstep(t - aa, t, abs(q.x - 1.0));
    float lf = 1.0 - smoothstep(t - aa, t, abs(q.x + 1.0));
    s[1] = up * rt; s[2] = dn * rt; s[4] = dn * lf; s[5] = up * lf;
    float on = 0.0, off = 0.0;
    for (int k = 0; k < 7; ++k)
    {
        if ((bits >> k) & 1u) on = max(on, s[k]); else off = max(off, s[k]);
    }
    col = lerp(col, Off, off);
    col = lerp(col, On * scan, on);
}
else
{
    float inside = step(0.12, c.x) * step(c.x, 0.88) * step(0.16, c.y) * step(c.y, 0.84);
    col = lerp(col, cell < val ? On * scan : Off, inside);
}
return col * Boost;
"""


def log(msg):
    unreal.log("[make_readout_material] " + msg)


def build():
    MM.ensure_dir(A.MASTER_DIR)
    mat, g = R.new_master(M_READOUT, unreal.MaterialShadingModel.MSM_UNLIT)
    teams = MM.load(A.MPC_TEAMS)
    boost = g.mpc(teams, "EmissiveBoost") if teams else g.scalar("EmissiveBoost", 1.0)
    ins = [
        ("UV", g.node(unreal.MaterialExpressionTextureCoordinate), ""),
        ("Value", g.custom_data(1, "EmissionScale", 0.0), ""),
        ("Mode", g.scalar("Mode", 0.0), ""),
        ("Cells", g.scalar("Cells", 4.0), ""),
        ("On", g.vector("OnColor", (2.0, 1.1, 0.25)), ""),
        ("Off", g.vector("OffColor", (0.025, 0.02, 0.012)), ""),
        ("Back", g.vector("BackColor", (0.004, 0.006, 0.008)), ""),
        ("Boost", boost, ""),
    ]
    c = R.custom(g, "AcReadout", HLSL, ins)
    g.out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    R.finish(mat, M_READOUT, [unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES])
    EAL.set_metadata_tag(mat, "AcMadeBy", "Tools/Editor/make_readout_material.py")
    EAL.save_loaded_asset(mat)
    log("master " + M_READOUT)
    return {M_READOUT: mat}


def ensure():
    """The master, built when missing (the model scripts call this through ac_modelkit)."""
    if EAL.does_asset_exist(M_READOUT):
        return {M_READOUT: unreal.load_asset(M_READOUT)}
    return build()


if __name__ == "__main__":
    build()
