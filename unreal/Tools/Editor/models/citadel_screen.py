# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""The picture on the Citadel's gun-station monitors (citadel.py, `gunScreens_k`): the gun's view, a dusty horizon
with dark mesas over a stony plain, after the user's console reference (art/models/citadel/ref/console-reference.jpg).
Baked once into unreal/Resources/Textures/citadel_gun_screen.png; the material lights it (the pose's lamp scale).

    uv run unreal/Tools/Editor/models/citadel_screen.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

W, H = 512, 320
HORIZON = 150
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "Resources", "Textures",
                                    "citadel_gun_screen.png"))
rnd = random.Random(18)


def lerp(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


img = Image.new("RGB", (W, H))
px = img.load()
# the sky: hazy pale dust at the horizon, a darker ochre overhead
for y in range(HORIZON + 1):
    t = (y / HORIZON) ** 1.4
    c = lerp((92, 74, 60), (196, 170, 140), t)
    for x in range(W):
        px[x, y] = c
# the plain: brown, darker toward the eye, with grain
for y in range(HORIZON, H):
    t = (y - HORIZON) / (H - HORIZON)
    c = lerp((150, 118, 88), (74, 54, 40), t ** 0.8)
    for x in range(W):
        n = rnd.randint(-14, 14)
        px[x, y] = tuple(max(0, min(255, v + n)) for v in c)
d = ImageDraw.Draw(img)


def mesa(x0, width, height, top_frac, colour):
    """A flat-topped butte standing on the horizon."""
    top = width * top_frac
    pts = [(x0, HORIZON + 2)]
    steps = 14
    for i in range(steps + 1):  # the left flank
        t = i / steps
        pts.append((x0 + (width - top) / 2 * t + rnd.uniform(-2, 2), HORIZON - height * min(1, t * 1.15) + rnd.uniform(-2, 2)))
    for i in range(1, steps):  # the ragged top
        pts.append((x0 + (width - top) / 2 + top * i / steps, HORIZON - height + rnd.uniform(-3, 3)))
    for i in range(steps + 1):  # the right flank
        t = 1 - i / steps
        pts.append((x0 + width - (width - top) / 2 * t + rnd.uniform(-2, 2), HORIZON - height * min(1, t * 1.15) + rnd.uniform(-2, 2)))
    pts.append((x0 + width, HORIZON + 2))
    d.polygon(pts, fill=colour)


# far mesas, hazy; then the near ones, dark
for x0, w, h, f in ((300, 120, 26, 0.5), (420, 90, 18, 0.4), (20, 70, 14, 0.5)):
    mesa(x0, w, h, f, (120, 98, 80))
for x0, w, h, f in ((60, 150, 62, 0.35), (210, 46, 70, 0.25), (380, 100, 40, 0.45)):
    mesa(x0, w, h, f, (52, 40, 34))
# a spire
d.polygon([(250, HORIZON), (258, HORIZON - 92), (266, HORIZON - 96), (272, HORIZON)], fill=(46, 36, 30))
# rocks on the plain, bigger toward the eye
for _ in range(140):
    y = rnd.uniform(HORIZON + 4, H)
    t = (y - HORIZON) / (H - HORIZON)
    r = 1 + 9 * t ** 1.6 * rnd.uniform(0.4, 1.0)
    x = rnd.uniform(0, W)
    d.ellipse((x - r * 1.4, y - r * 0.6, x + r * 1.4, y + r * 0.6), fill=lerp((60, 44, 34), (40, 30, 24), t))
img = img.filter(ImageFilter.GaussianBlur(0.6))
# a faint reticle: the gun's view
d = ImageDraw.Draw(img)
cx, cy = W // 2, HORIZON + 10
for a in range(0, 360, 90):
    ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
    d.line((cx + ca * 10, cy + sa * 10, cx + ca * 26, cy + sa * 26), fill=(150, 210, 230), width=2)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
img.save(OUT)
print("wrote", OUT)
