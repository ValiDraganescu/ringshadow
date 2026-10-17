# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "scipy", "matplotlib"]
# ///
"""Lift the tracked markers to hinge angles of the near leg (hip swing, knee bend, the sole's pitch), find the
stride period, normalise to one stride and compare with the game's procedural gait (PoseProspector).
Writes gait.json (32 bins over the stride, radians) and gait.png (the measured dots over the game's curves).

    uv run lift.py <track_dir> <plate.json> <label>
"""
import sys, json, math
import numpy as np
from scipy.signal import savgol_filter
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

d, plate_path, label = sys.argv[1], sys.argv[2], sys.argv[3]
T = json.load(open(f"{d}/tracks.json")); plate = json.load(open(plate_path))
fps, tracks = T["fps"], T["tracks"]
ppc = plate["pixels_per_cell"]  # pixels per cell on the plate (the clip keeps the plate's size)
# rest lengths from the plate (pixels)
M = {k: np.array(v["xy"]) for k, v in plate["markers"].items()}
L_thigh = np.linalg.norm(M["knee"] - M["hip"]); L_shin = np.linalg.norm(M["ankle"] - M["knee"]); L_foot = np.linalg.norm(M["toe"] - M["heel"])

def pick(cands, near, length=None, origin=None):
    """The candidate nearest `near` (or, with a length, whose distance from origin is nearest `length` and that is nearest `near`)."""
    best, bc = None, 1e9
    for c in cands:
        if c is None: continue
        c = np.array(c); cost = np.linalg.norm(c - near)
        if length is not None: cost += 2 * abs(np.linalg.norm(c - origin) - length)
        if cost < bc: best, bc = c, cost
    return best

rows = []
prev = {"ankle": M["ankle"], "toe": M["toe"], "heel": M["heel"]}
for i, t in enumerate(tracks):
    hip = t["hip"][0]; knee = t["knee"][0]
    if hip is None or knee is None: rows.append(None); continue
    hip, knee = np.array(hip), np.array(knee)
    ankle = pick(t["ankle"], prev["ankle"], L_shin, knee)
    if ankle is None: rows.append(None); continue
    toe = pick(t["toe"], ankle + (M["toe"] - M["ankle"])); heel = pick(t["heel"], ankle + (M["heel"] - M["ankle"]))
    prev.update(ankle=ankle, toe=toe if toe is not None else prev["toe"], heel=heel if heel is not None else prev["heel"])
    # angles: image y points down. thigh angle + when the knee is ahead (x larger) of the hip
    th = math.atan2(knee[0] - hip[0], knee[1] - hip[1])
    sh = math.atan2(ankle[0] - knee[0], ankle[1] - knee[1])
    kn = th - sh  # flexion positive: the shin trails the thigh
    foot = math.atan2(-(toe[1] - heel[1]), toe[0] - heel[0]) if toe is not None and heel is not None else float("nan")
    rows.append(dict(f=i, hip=th, knee=kn, foot=foot, hx=hip[0], hy=hip[1], body=t["body"][0], drill=t["drill"][0],
                     thigh=np.linalg.norm(knee - hip), shin=np.linalg.norm(ankle - knee)))
good = [r for r in rows if r]
f = np.array([r["f"] for r in good]); hip = np.unwrap([r["hip"] for r in good]); knee = np.array([r["knee"] for r in good]); foot = np.array([r["foot"] for r in good])
hx = np.array([r["hx"] for r in good]); hy = np.array([r["hy"] for r in good])
thigh = np.array([r["thigh"] for r in good]); shin = np.array([r["shin"] for r in good])
w = 9
hip_s, knee_s, foot_s = (savgol_filter(x, w, 3) for x in (hip, knee, np.nan_to_num(foot)))

# stride: the hip angle's cycle. Period from the autocorrelation of the hip angle.
h0 = hip_s - hip_s.mean(); ac = np.correlate(h0, h0, "full")[len(h0) - 1:]; ac /= ac[0]
cands = [k for k in range(8, len(ac) - 1) if ac[k] > ac[k - 1] and ac[k] > ac[k + 1] and ac[k] > 0.2]
period = cands[0] if cands else len(h0)
# phase 0 at the hip angle's upward zero crossing (leg passing under the body, swinging forward)
zc = [k for k in range(1, len(h0)) if h0[k - 1] < 0 <= h0[k]]
start = zc[0] if zc else 0
phase = ((f - f[start]) / period) % 1.0
speed_px = (hx[-1] - hx[0]) / (f[-1] - f[0]) * fps  # px / s
stride_cells = speed_px * period / fps / ppc
bob = (hy.max() - hy.min()) / ppc
rep = dict(label=label, fps=fps, frames=len(tracks), tracked=len(good), period_frames=int(period), period_s=period / fps,
           speed_cells_s=speed_px / ppc, stride_cells=stride_cells, bob_cells=bob,
           thigh_len_ratio=[float(thigh.min() / L_thigh), float(thigh.max() / L_thigh)], shin_len_ratio=[float(shin.min() / L_shin), float(shin.max() / L_shin)],
           hip_deg=[float(np.degrees(hip_s.min())), float(np.degrees(hip_s.max()))], knee_deg=[float(np.degrees(knee_s.min())), float(np.degrees(knee_s.max()))],
           foot_deg=[float(np.degrees(np.nanmin(foot_s))), float(np.degrees(np.nanmax(foot_s)))])
print(json.dumps(rep, indent=1))

# the game's curves (PoseProspector): Phase = stride * 3.2; one leg: Hip = 0.45 sin(P), Knee = 0.1 + 1.1 max(0, cos(P + 0.2))^1.2, Ankle = -(Hip - Knee) * 0.8
P = np.linspace(0, 2 * np.pi, 200); gh = 0.45 * np.sin(P); gk = 0.1 + 1.1 * np.maximum(0, np.cos(P + 0.2)) ** 1.2; ga = -(gh - gk) * 0.8
# the game's phase 0 is also the hip's upward zero crossing (sin): same convention. Its foot pitch in the world = hip - knee + ankle... the sole stays near level: plot the ankle term
# a gait table: 32 bins over the stride, the measured curves' mean per bin
bins = 32; tbl = {"hip": [], "knee": [], "foot": []}
for b in range(bins):
    m = (phase >= b / bins) & (phase < (b + 1) / bins)
    for k, arr in (("hip", hip_s), ("knee", knee_s), ("foot", foot_s)): tbl[k].append(float(np.mean(arr[m])) if m.any() else None)
json.dump(dict(rep, bins=bins, table=tbl), open(f"{d}/gait.json", "w"), indent=1)

# chart: three small multiples, measured (series 1) against the game (series 2), one y axis each (degrees)
C1, C2, INK, GRID = "#2a78d6", "#eb6834", "#1a1a19", "#e6e4df"
fig, axes = plt.subplots(1, 3, figsize=(13, 3.8), dpi=130); fig.patch.set_facecolor("white")
order = np.argsort(phase)
for ax, (name, meas, game) in zip(axes, [("Hip swing", hip_s, gh), ("Knee bend", knee_s, gk), ("Foot pitch", foot_s, None)]):
    ax.scatter(phase, np.degrees(meas), s=9, color=C1, alpha=0.7, label=f"Grok clip ({label})", zorder=3)
    if game is not None: ax.plot(P / (2 * np.pi), np.degrees(game), color=C2, lw=2, label="game today (PoseProspector)")
    ax.set_title(name, loc="left", color=INK, fontsize=11); ax.set_xlabel("stride phase", color=INK)
    ax.set_ylabel("degrees", color=INK); ax.grid(color=GRID, lw=0.8); ax.set_axisbelow(True)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    for s in ("left", "bottom"): ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK)
axes[0].legend(frameon=False, fontsize=8, loc="upper left")
fig.suptitle(f"Prospector walk: markers tracked in the Grok clip vs the game's gait  ·  period {period / fps:.2f} s, stride {stride_cells:.2f} cells, bob {bob * 100:.0f} cm", x=0.01, ha="left", fontsize=10, color=INK)
fig.tight_layout(rect=(0, 0, 1, 0.93)); fig.savefig(f"{d}/gait.png"); print("chart", f"{d}/gait.png")
