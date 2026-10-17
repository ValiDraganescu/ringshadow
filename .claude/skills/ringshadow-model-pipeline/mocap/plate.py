# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""The marker plate: a unit's RIGHT SIDE stencil panel on a 1280x720 white plate, the unit in the left
third with floor to walk into, one unique-colour dot per joint and four black corner fiducials.

    uv run plate.py <unit> <markers.json> [scale] [--name <motion>] [--prop <image> <x_cells> <height_cells>]

<markers.json> names each marker and its model point (SceneKit frame, cells: x ahead, y up), e.g.
    {"hip": [0, 0.56], "knee": [0.04, 0.30], "ankle": [0.035, 0.08], "heel": [-0.14, -0.005], "toe": [0.26, -0.005]}
The colours are given in order from a fixed list (red, green, blue, magenta, cyan, yellow, orange, ...).
Writes art/models/<unit>/mocap/plate.png and plate.json (every marker's pixel, colour and model point,
the pixels per cell, the floor row, the fiducials); with --name <motion>, plate-<motion>.png and .json.
--prop puts a prop (a picture on white, e.g. an ore nodule from grok_image) on the floor, its centre
<x_cells> ahead of the unit's origin, scaled to <height_cells> tall: the thing the motion acts on, so the
video model knows what the unit is doing. The dashed guides, the panel border and the header of the
stencil panel are whitened; the panel's clay stays as it is."""
import json, sys
from pathlib import Path
from PIL import Image, ImageDraw

COLOURS = [(255, 0, 0), (0, 255, 0), (0, 80, 255), (255, 0, 255), (0, 255, 255), (255, 200, 0), (255, 120, 0), (120, 0, 255)]
root = Path(__file__).resolve().parents[4]
args = sys.argv[1:]; plate_name = "plate"; prop = None
if "--name" in args:
    k = args.index("--name"); plate_name = f"plate-{args[k + 1]}"; del args[k:k + 2]
if "--prop" in args:
    k = args.index("--prop"); prop = (args[k + 1], float(args[k + 2]), float(args[k + 3])); del args[k:k + 4]
unit = args[0]; markers = json.load(open(args[1])); SCALE = float(args[2]) if len(args) > 2 else 1.25
d = root / "art/models" / unit
st = json.load(open(d / "stencil.json"))
p = next(p for p in st["panels"] if p["view"] == "right")
x0, y0, w, h = p["rect"]; ppu = p["pixelsPerUnit"]; c = p["center"]
panel = Image.open(d / "stencil.png").convert("RGB").crop((x0, y0, x0 + w, y0 + h))
px = panel.load()
# guide rows: the dashed quarter-height guides reach into the white margin left of the figure
guide_rows = [yy for yy in range(50, h - 6) if sum(1 for xx in range(10, 50) if px[xx, yy] != (255, 255, 255)) >= 8]
for yy in range(h):
    for xx in range(w):
        if xx < 6 or xx >= w - 6 or yy < 46 or yy >= h - 6: px[xx, yy] = (255, 255, 255); continue
        if any(abs(yy - g) <= 2 for g in guide_rows) and px[xx, yy][0] >= 150 and px[xx, yy - 4] == (255, 255, 255) and px[xx, yy + 4] == (255, 255, 255):
            px[xx, yy] = (255, 255, 255)

def proj(pt):
    return w / 2 + (pt[0] - c[0]) * ppu, h / 2 - (pt[1] - c[1]) * ppu

pw, ph = int(w * SCALE), int(h * SCALE)
panel = panel.resize((pw, ph), Image.LANCZOS)
plate = Image.new("RGB", (1280, 720), (255, 255, 255))
ground_y = 600
ox, oy = 60, int(ground_y - (p["groundRow"] - y0) * SCALE)
plate.paste(panel, (ox, oy))
dr = ImageDraw.Draw(plate)
dr.line([(0, ground_y), (1280, ground_y)], fill=(150, 150, 150), width=3)
out = {"unit": unit, "plate": [1280, 720], "scale": SCALE, "panel_origin": [ox, oy], "pixels_per_cell": ppu * SCALE, "ground_y": ground_y, "markers": {}}
if prop:
    # the prop's near-white background goes transparent; its bottom row sits on the floor line
    im = Image.open(prop[0]).convert("RGB"); bb = im.point(lambda v: 0 if v > 235 else 255).convert("L").getbbox()
    im = im.crop(bb); ph_px = int(prop[2] * ppu * SCALE); im = im.resize((int(im.width * ph_px / im.height), ph_px), Image.LANCZOS)
    mask = im.convert("L").point(lambda v: 0 if v > 235 else 255)
    cx = ox + proj((prop[1], 0))[0] * SCALE
    plate.paste(im, (int(cx - im.width / 2), ground_y + 1 - im.height), mask)
    out["prop"] = {"image": prop[0], "x_cells": prop[1], "height_cells": prop[2], "box": [int(cx - im.width / 2), ground_y + 1 - im.height, im.width, im.height]}
R = 13
for (name, pt), col in zip(markers.items(), COLOURS):
    cx, cy = proj(pt); X = ox + cx * SCALE; Y = oy + cy * SCALE
    dr.ellipse([X - R, Y - R, X + R, Y + R], fill=col, outline=(0, 0, 0), width=2)
    out["markers"][name] = {"xy": [round(X, 1), round(Y, 1)], "rgb": list(col), "model": list(pt)}
fids = [[24, 24], [1256, 24], [24, 696], [1256, 696]]
for fx, fy in fids:
    dr.rectangle([fx - 14, fy - 14, fx + 14, fy + 14], fill=(0, 0, 0)); dr.rectangle([fx - 5, fy - 5, fx + 5, fy + 5], fill=(255, 255, 255))
out["fiducials"] = fids
(d / "mocap").mkdir(exist_ok=True)
plate.save(d / f"mocap/{plate_name}.png"); json.dump(out, open(d / f"mocap/{plate_name}.json", "w"), indent=1)
print(f"wrote {d}/mocap/{plate_name}.png and {plate_name}.json; markers: {', '.join(out['markers'])}")
