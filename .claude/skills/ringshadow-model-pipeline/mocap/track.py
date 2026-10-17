# /// script
# requires-python = ">=3.11"
# dependencies = ["opencv-python-headless", "numpy", "scipy"]
# ///
"""Track the coloured markers and the corner fiducials of a Grok clip made from a marker plate.

    uv run track.py <clip.mp4> <plate.json> <out_dir>

Writes tracks.json (per frame, per marker: up to two blobs for a leg marker, since the video model draws
the far leg with the same colours; slot 0 starts on the plate's marker and follows it by continuity),
fiducials.json (the four corner squares per frame: their drift says whether the camera held) and
tracked-sheet.jpg, every 12th frame with the tracked points. Prints each marker's blob counts."""
import sys, json, math
import numpy as np, cv2
from scipy.optimize import linear_sum_assignment

clip, plate_path, out = sys.argv[1], sys.argv[2], sys.argv[3]
import os; os.makedirs(out, exist_ok=True)
plate = json.load(open(plate_path))
cap = cv2.VideoCapture(clip)
fps = cap.get(cv2.CAP_PROP_FPS); n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
W, H = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)), int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
print(f"{clip}: {W}x{H} {fps:.2f} fps {n} frames")

# HSV ranges per marker colour (OpenCV hue 0..179)
HUES = {"hip": (0, 255, 0, 0), "knee": (60,), "ankle": (120,), "toe": (150,), "heel": (90,), "body": (25,), "drill": (12,)}
def hue_of(rgb):
    c = np.uint8([[list(rgb)[::-1]]]); return int(cv2.cvtColor(c, cv2.COLOR_BGR2HSV)[0, 0, 0])
HUE = {name: hue_of(m["rgb"]) for name, m in plate["markers"].items()}
print("hues", HUE)

def blobs(frame_bgr, hue, tol=7, min_area=60):
    hsv = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2HSV)
    h, s, v = hsv[..., 0].astype(int), hsv[..., 1], hsv[..., 2]
    d = np.minimum(np.abs(h - hue), 180 - np.abs(h - hue))
    mask = ((d <= tol) & (s > 120) & (v > 90)).astype(np.uint8) * 255
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    cnt, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    res = []
    for c in cnt:
        a = cv2.contourArea(c)
        if a < min_area: continue
        m = cv2.moments(c); res.append((m["m10"] / m["m00"], m["m01"] / m["m00"], a))
    return sorted(res, key=lambda b: -b[2])[:3]

def fiducials(frame_bgr):
    g = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)
    found = []
    for (fx, fy) in plate["fiducials"]:
        x0, y0 = max(0, int(fx) - 40), max(0, int(fy) - 40)
        win = g[y0:y0 + 80, x0:x0 + 80]
        m = (win < 60).astype(np.uint8)
        if m.sum() < 100: found.append(None); continue
        ys, xs = np.nonzero(m); found.append((x0 + xs.mean(), y0 + ys.mean()))
    return found

frames = []; tracks = []; fids = []
prev = {}  # name -> list of (x, y) per slot
LEG = {"knee", "ankle", "toe", "heel"}
i = 0
while True:
    ok, f = cap.read()
    if not ok: break
    rec = {}
    for name, hue in HUE.items():
        bs = blobs(f, hue)
        slots = 2 if name in LEG else 1
        pts = [(b[0], b[1]) for b in bs]
        # assign to slots by continuity with the previous frame
        p = prev.get(name, [None] * slots)
        out_slots = [None] * slots
        if pts:
            cost = np.zeros((slots, len(pts)))
            for s in range(slots):
                for k, q in enumerate(pts):
                    if p[s] is None: cost[s, k] = 300 + k  # free slot: prefer the biggest blob
                    else: cost[s, k] = math.hypot(q[0] - p[s][0], q[1] - p[s][1])
            r, c = linear_sum_assignment(cost)
            for s, k in zip(r, c):
                if cost[s, k] < 400: out_slots[s] = pts[k]
        rec[name] = out_slots
        prev[name] = [o if o is not None else (p[s] if s < len(p) else None) for s, o in enumerate(out_slots)]
    tracks.append(rec); fids.append(fiducials(f))
    if i % 12 == 0: frames.append((i, f.copy()))
    i += 1
cap.release()

# fiducial drift
fx = np.array([[q if q else (np.nan, np.nan) for q in fr] for fr in fids], dtype=float)
drift = np.nanmax(np.abs(fx - fx[0]), axis=(0, 1))
print("fiducial drift max px:", drift)
json.dump({"fps": fps, "frames": n, "size": [W, H], "tracks": tracks}, open(f"{out}/tracks.json", "w"))
json.dump(fx.tolist(), open(f"{out}/fiducials.json", "w"))
# presence stats
for name in HUE:
    have = [sum(1 for s in t[name] if s) for t in tracks]
    print(f"{name:6s} slots seen: 0={have.count(0)} 1={have.count(1)} 2={have.count(2)}")

# contact sheet with tracked points
cols = 4; rows = math.ceil(len(frames) / cols)
sheet = np.full((rows * 360, cols * 640, 3), 255, np.uint8)
for k, (idx, f) in enumerate(frames):
    t = tracks[idx]
    for name, slots in t.items():
        for s, q in enumerate(slots):
            if q is None: continue
            cv2.circle(f, (int(q[0]), int(q[1])), 16, (0, 0, 0) if s == 0 else (255, 255, 255), 3)
            cv2.putText(f, f"{name[:2]}{s}", (int(q[0]) + 14, int(q[1]) - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)
    for name in ("hip", "knee", "ankle"):
        pass
    small = cv2.resize(f, (640, 360)); cv2.putText(small, f"f{idx}", (8, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
    r, c = divmod(k, cols); sheet[r * 360:(r + 1) * 360, c * 640:(c + 1) * 640] = small
cv2.imwrite(f"{out}/tracked-sheet.jpg", sheet, [cv2.IMWRITE_JPEG_QUALITY, 85])
print("sheet", f"{out}/tracked-sheet.jpg")
