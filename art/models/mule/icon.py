# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""The Mule's console icon, unreal/Content-src/icons/mule.png (192 px with alpha, as the other model icons), from two
model-row renders of the same 3/4 camera (shots.sh icon-lit icon-mask): the lit picture, and the flat part colours on
black as its mask. Then import it: AC_ICONS=mule, unreal/Tools/Editor/import_icons.py (a hidden --python run).

    art/models/mule/shots.sh icon-lit icon-mask && uv run art/models/mule/icon.py
"""
import os

from PIL import Image, ImageEnhance, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
SIZE = 192


def main():
    lit = Image.open(os.path.join(HERE, "previews", "icon-lit.png")).convert("RGB")
    mask = Image.open(os.path.join(HERE, "previews", "icon-mask.png")).convert("L")
    alpha = mask.point(lambda v: 255 if v > 8 else 0).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.7))
    icon = ImageEnhance.Brightness(lit).enhance(1.3)  # the stencil lights are dimmer than the old icons' renders
    icon.putalpha(alpha)
    box = alpha.getbbox()
    icon = icon.crop(box)
    side = int(max(icon.width, icon.height) * 1.06)
    sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    sq.paste(icon, ((side - icon.width) // 2, (side - icon.height) // 2))
    out = os.path.join(REPO, "unreal", "Content-src", "icons", "mule.png")
    sq.resize((SIZE, SIZE), Image.LANCZOS).save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
