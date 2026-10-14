# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow>=10"]
# ///
"""Copy pictures into a blog post's album, resized and recompressed.

    uv run website/tools/album.py website/blog/media/<post> SRC:NAME [SRC:NAME ...]
    uv run website/tools/album.py website/blog/media/<post> --cover SRC

Each SRC becomes NAME.webp (at most 1600 px wide, quality lowered until it is
under 300 KB) and NAME-t.webp (a 640 px thumbnail). --cover writes cover.jpg
(1200 px), the post's card and its og:image. The sources are only read: never
move or delete a worker's file. Prints each output's size and pixel size, the
numbers the post's <img width height> need.
"""
import sys
from pathlib import Path

from PIL import Image

FULL_W, THUMB_W, COVER_W = 1600, 640, 1200
FULL_MAX = 300_000


def fit(im: Image.Image, w: int) -> Image.Image:
    if im.width <= w:
        return im
    return im.resize((w, round(im.height * w / im.width)), Image.LANCZOS)


def save_webp(im: Image.Image, path: Path, q: int, cap: int | None) -> None:
    while True:
        im.save(path, "WEBP", quality=q, method=6)
        if cap is None or path.stat().st_size <= cap or q <= 50:
            return
        q -= 5


def report(path: Path, im: Image.Image) -> None:
    print(f"{path.name:32} {im.width}x{im.height} {path.stat().st_size // 1024} KB")


def main(argv: list[str]) -> None:
    if len(argv) < 2:
        sys.exit(__doc__)
    out = Path(argv[0])
    out.mkdir(parents=True, exist_ok=True)
    args = argv[1:]
    if args[0] == "--cover":
        im = fit(Image.open(args[1]).convert("RGB"), COVER_W)
        path = out / "cover.jpg"
        im.save(path, "JPEG", quality=78, optimize=True, progressive=True)
        report(path, im)
        return
    for pair in args:
        src, name = pair.rsplit(":", 1)
        im = Image.open(src).convert("RGB")
        full = fit(im, FULL_W)
        save_webp(full, out / f"{name}.webp", 82, FULL_MAX)
        report(out / f"{name}.webp", full)
        thumb = fit(im, THUMB_W)
        save_webp(thumb, out / f"{name}-t.webp", 72, None)
        report(out / f"{name}-t.webp", thumb)


if __name__ == "__main__":
    main(sys.argv[1:])
