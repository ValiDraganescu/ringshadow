# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "scipy", "matplotlib"]
# ///
"""Lift the tracked markers of a drilling clip to the drilling channels of PoseProspector: the body's lean
(radians from the rest, + into the rock), the drill arm's push (cells along the arm, from the rest), the
drill's pitch (+ tip down) and the near leg's hip swing and knee bend (a crouch). Finds the stroke period
from the push's autocorrelation, sets phase 0 at the push's upward zero crossing (the stroke starting
in), and writes drill.json (32 bins over the stroke) and drill.png (the measured dots over the game's
curves: Push = 0.06 (0.5 + 0.5 sin), Roll = -0.1 - 0.8 Push).

    uv run lift_drill.py <track_dir> <plate.json> <label>
"""
import sys, json, math
import numpy as np
from scipy.signal import savgol_filter
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

d, plate_path, label = sys.argv[1], sys.argv[2], sys.argv[3]
T = json.load(open(f"{d}/tracks.json")); plate = json.load(open(plate_path))
fps, tracks = T["fps"], T["tracks"]
ppc = plate["pixels_per_cell"]
M = {k: np.array(v["xy"]) for k, v in plate["markers"].items()}
MODEL = {k: np.array(v["model"]) for k, v in plate["markers"].items()}
# the hull turns about the root, on the ground under the hip (the body part sits at the origin)
ROOT = M["hip"] + np.array([0, MODEL["hip"][1] * ppc])
L_shin = np.linalg.norm(M["ankle"] - M["knee"])

def ang(v):  # image y down: the angle of v from straight up, + forward (x right)
    return math.atan2(v[0], -v[1])
def rot(v, a):  # turn v by a (the image's sense, y down)
    c, s = math.cos(a), math.sin(a); return np.array([c * v[0] + s * v[1], -s * v[0] + c * v[1]])
def pick(cands, near, length=None, origin=None):
    best, bc = None, 1e9
    for c in cands:
        if c is None: continue
        c = np.array(c); cost = np.linalg.norm(c - near)
        if length is not None: cost += 2 * abs(np.linalg.norm(c - origin) - length)
        if cost < bc: best, bc = c, cost
    return best

lean0 = ang(M["body"] - ROOT); pitch0 = ang(M["drill"] - M["shoulder"]) - math.pi / 2
sh0 = M["shoulder"] - ROOT
rows = []; prev = {"ankle": M["ankle"]}
for i, t in enumerate(tracks):
    need = [t[k][0] for k in ("hip", "knee", "body", "drill", "shoulder")]
    if any(q is None for q in need): rows.append(None); continue
    hip, knee, body, drill, shoulder = (np.array(q) for q in need)
    root = hip + np.array([0, MODEL["hip"][1] * ppc])  # the hip is on the leg, not the hull: it does not lean
    lean = ang(body - root) - lean0
    # the arm, un-leaned: how far the shoulder (on the arm) slid along the hull's forward
    sh = rot(shoulder - root, -lean) - sh0
    push = sh[0] / ppc
    pitch = (ang(drill - shoulder) - math.pi / 2) - lean - pitch0
    ankle = pick(t["ankle"], prev["ankle"], L_shin, knee)
    if ankle is not None: prev["ankle"] = ankle
    th = math.atan2(knee[0] - hip[0], knee[1] - hip[1]); sh_a = math.atan2(ankle[0] - knee[0], ankle[1] - knee[1]) if ankle is not None else th
    rows.append(dict(f=i, lean=lean, push=push, pitch=pitch, hip=th, knee=th - sh_a, arm=np.linalg.norm(drill - shoulder) / np.linalg.norm(M["drill"] - M["shoulder"])))
good = [r for r in rows if r]
f = np.array([r["f"] for r in good]); ch = {k: np.array([r[k] for r in good]) for k in ("lean", "push", "pitch", "hip", "knee")}
arm = np.array([r["arm"] for r in good])
w = 9; sm = {k: savgol_filter(v, w, 3) for k, v in ch.items()}
p0 = sm["push"] - sm["push"].mean(); ac = np.correlate(p0, p0, "full")[len(p0) - 1:]; ac /= ac[0]
cands = [k for k in range(6, len(ac) - 1) if ac[k] > ac[k - 1] and ac[k] > ac[k + 1] and ac[k] > 0.2]
period = cands[0] if cands else len(p0)
zc = [k for k in range(1, len(p0)) if p0[k - 1] < 0 <= p0[k]]
start = zc[0] if zc else 0
phase = ((f - f[start]) / period) % 1.0
rep = dict(label=label, fps=fps, frames=len(tracks), tracked=len(good), period_frames=int(period), period_s=period / fps,
           lean_deg=[float(np.degrees(sm["lean"].min())), float(np.degrees(sm["lean"].max()))],
           push_cells=[float(sm["push"].min()), float(sm["push"].max())],
           pitch_deg=[float(np.degrees(sm["pitch"].min())), float(np.degrees(sm["pitch"].max()))],
           hip_deg=[float(np.degrees(sm["hip"].min())), float(np.degrees(sm["hip"].max()))],
           knee_deg=[float(np.degrees(sm["knee"].min())), float(np.degrees(sm["knee"].max()))],
           arm_len_ratio=[float(arm.min()), float(arm.max())], autocorr_peak=float(ac[period]) if period < len(ac) else None)
print(json.dumps(rep, indent=1))
bins = 32; tbl = {k: [] for k in sm}
for b in range(bins):
    m = (phase >= b / bins) & (phase < (b + 1) / bins)
    for k, arr in sm.items(): tbl[k].append(float(np.mean(arr[m])) if m.any() else None)
# fill an empty bin from its neighbours
for k, arr in tbl.items():
    for b in range(bins):
        if arr[b] is None: arr[b] = next(v for v in (arr[(b + j) % bins] for j in (1, -1, 2, -2, 3, -3)) if v is not None)
json.dump(dict(rep, bins=bins, table=tbl), open(f"{d}/drill.json", "w"), indent=1)

# the game's curves: Push = 0.06 (0.5 + 0.5 sin P) at 2.4 rad/s, Roll = -0.1 - 0.8 Push (the lean, + into the rock here)
P = np.linspace(0, 2 * np.pi, 200); gp = 0.06 * (0.5 + 0.5 * np.sin(P)); gl = 0.1 + 0.8 * gp
C1, C2, INK, GRID = "#2a78d6", "#eb6834", "#1a1a19", "#e6e4df"
fig, axes = plt.subplots(1, 4, figsize=(16, 3.8), dpi=130); fig.patch.set_facecolor("white")
panels = [("Body lean", np.degrees(sm["lean"]), np.degrees(gl), "degrees"), ("Arm push", sm["push"] * 100, gp * 100, "cm"),
          ("Drill pitch", np.degrees(sm["pitch"]), None, "degrees"), ("Knee bend", np.degrees(sm["knee"]), np.zeros_like(P), "degrees")]
for ax, (name, meas, game, unit) in zip(axes, panels):
    ax.scatter(phase, meas, s=9, color=C1, alpha=0.7, label=f"Grok clip ({label})", zorder=3)
    if game is not None: ax.plot(P / (2 * np.pi), game, color=C2, lw=2, label="game today (PoseProspector)")
    ax.set_title(name, loc="left", color=INK, fontsize=11); ax.set_xlabel("stroke phase", color=INK); ax.set_ylabel(unit, color=INK)
    ax.grid(color=GRID, lw=0.8); ax.set_axisbelow(True)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    for s in ("left", "bottom"): ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK)
axes[0].legend(frameon=False, fontsize=8, loc="upper left")
fig.suptitle(f"Prospector drilling: markers tracked in the Grok clip vs the game's strokes  ·  stroke period {period / fps:.2f} s", x=0.01, ha="left", fontsize=10, color=INK)
fig.tight_layout(rect=(0, 0, 1, 0.93)); fig.savefig(f"{d}/drill.png"); print("chart", f"{d}/drill.png")
