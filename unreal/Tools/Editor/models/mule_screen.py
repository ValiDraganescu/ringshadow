# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""The Mule's load screen (mule.py, `screens_k`; docs/haul-and-ferry.md section 4): the screen's fixed face, its frame,
labels and the dark wells the live readouts stand in. The readouts are their own parts on the readout material
(make_readout_material.py): `roomDigits_k` (room left), `roomBar_k` (ten cells, one a hundred of room), `oreDigits_k`,
`mhDigits_k`. Baked once into unreal/Resources/Textures/mule_screen.png; the material lights it.

    uv run unreal/Tools/Editor/models/mule_screen.py

The layout's numbers are shared with mule.py (SCREEN, WELLS): a well's box in the picture's 0..1 coordinates (u right,
v down), where mule.py stands its readout.
"""
import os

W, H = 600, 360
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
OUT = os.path.join(REPO, "unreal", "Resources", "Textures", "mule_screen.png")
FONT = os.path.join(REPO, "unreal", "Content-src", "fonts", "BarlowCondensed-Bold.ttf")

# the readouts' wells (u0, v0, u1, v1), mule.py reads these
WELLS = {
    "room": (0.05, 0.20, 0.60, 0.56),
    "bar": (0.05, 0.62, 0.95, 0.72),
    "ore": (0.17, 0.79, 0.47, 0.95),
    "mh": (0.63, 0.79, 0.93, 0.95),
}
AMBER = (255, 170, 60)
CYAN = (120, 220, 255)
DIM = (70, 96, 110)


def box(w):
    u0, v0, u1, v1 = w
    return (u0 * W, v0 * H, u1 * W, v1 * H)


def main():
    from PIL import Image, ImageDraw, ImageFont  # (here: mule.py imports WELLS in the editor, which has no pillow)

    img = Image.new("RGB", (W, H), (6, 10, 14))
    d = ImageDraw.Draw(img)
    big = ImageFont.truetype(FONT, 46)
    mid = ImageFont.truetype(FONT, 34)
    small = ImageFont.truetype(FONT, 26)
    # the bezel's inner line and corner ticks
    d.rectangle((6, 6, W - 7, H - 7), outline=(40, 70, 86), width=3)
    for x, y in ((6, 6), (W - 7, 6), (6, H - 7), (W - 7, H - 7)):
        sx = 1 if x < W / 2 else -1
        sy = 1 if y < H / 2 else -1
        d.line((x, y, x + sx * 26, y), fill=CYAN, width=4)
        d.line((x, y, x, y + sy * 26), fill=CYAN, width=4)
    d.text((0.05 * W, 0.045 * H), "ROOM LEFT", font=mid, fill=CYAN)
    d.text((0.95 * W, 0.045 * H), "LOAD", font=small, fill=DIM, anchor="ra")
    # the room readout's well, then "/ 1000" beside it
    for k, w in WELLS.items():
        d.rectangle(box(w), fill=(2, 4, 6), outline=(26, 44, 54), width=2)
    d.text((0.64 * W, 0.40 * H), "/ 1000", font=big, fill=DIM, anchor="lm")
    d.text((0.05 * W, 0.87 * H), "ORE", font=mid, fill=AMBER, anchor="lm")
    d.text((0.51 * W, 0.87 * H), "MH", font=mid, fill=CYAN, anchor="lm")
    # the bar's scale: ticks under it at 0, 500, 1000
    for f, label in ((0.0, "0"), (0.5, "500"), (1.0, "1000")):
        x = (WELLS["bar"][0] + (WELLS["bar"][2] - WELLS["bar"][0]) * f) * W
        d.line((x, 0.725 * H, x, 0.755 * H), fill=DIM, width=2)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
