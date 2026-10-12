# /// script
# dependencies = ["pillow"]
# ///
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
src, out = sys.argv[1], sys.argv[2]
W, H = 960, 480
FONTS = "/Users/valentindraganescu/git/dev/autocraft/unreal/Content-src/fonts"
img = Image.open(src).convert("RGB").resize((W, H), Image.LANCZOS)
# darken the left side so the title reads
shade = Image.new("L", (W, H))
for x in range(W):
    a = int(170 * max(0.0, 1 - x / (W * 0.62)) ** 1.6)
    for y in range(0, H, 1):
        pass
    ImageDraw.Draw(shade).line([(x, 0), (x, H)], fill=a)
img = Image.composite(Image.new("RGB", (W, H), (4, 11, 19)), img, shade)
# bottom fade
fade = Image.new("L", (W, H))
d = ImageDraw.Draw(fade)
for y in range(H):
    d.line([(0, y), (W, y)], fill=int(140 * max(0.0, (y - H * 0.72) / (H * 0.28)) ** 1.5))
img = Image.composite(Image.new("RGB", (W, H), (4, 11, 19)), img, fade)

title = "RINGSHADOW"
f = ImageFont.truetype(f"{FONTS}/BarlowCondensed-ExtraBold.ttf", 92)
track = 14
x0, y0 = 56, 150
def draw_spaced(draw, xy, text, font, fill, track):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += font.getlength(ch) + track
    return x - track
# glow
glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
end = draw_spaced(ImageDraw.Draw(glow), (x0, y0), title, f, (89, 217, 255, 150), track)
glow = glow.filter(ImageFilter.GaussianBlur(14))
img = Image.alpha_composite(img.convert("RGBA"), glow)
d = ImageDraw.Draw(img)
draw_spaced(d, (x0, y0), title, f, (235, 247, 255, 255), track)
# rule + tagline
ry = y0 + 118
d.line([(x0 + 3, ry), (x0 + 3 + 64, ry)], fill=(89, 217, 255, 255), width=3)
d.line([(x0 + 3 + 74, ry), (end, ry)], fill=(140, 217, 242, 70), width=1)
t = ImageFont.truetype(f"{FONTS}/BarlowCondensed-SemiBold.ttf", 22)
draw_spaced(d, (x0 + 3, ry + 14), "PROSPECTORS OF THE FRONTIER", t, (168, 222, 242, 255), 5)
img.convert("RGB").save(out, optimize=True)
