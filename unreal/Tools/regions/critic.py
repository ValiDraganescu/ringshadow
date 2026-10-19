# /// script
# requires-python = ">=3.11"
# dependencies = ["Pillow"]
# ///
"""One contact sheet to judge a region against its brief (docs/regions.md,
"Seeing a region").

On one JPEG, each picture labelled: the top-down preview
(`autocraft-region preview`), four oblique views from the north, east,
south and west (`autocraft-region view`), two pilot's-eye views from the
first pad toward its two most open directions, the reference image if one is
given, and the terrain lab's in-game stills of the region if a shots folder
is given (DIR/<region>/game/*.png, as `-AcTerrainLabShots` writes them; this
script never starts the game). Under them, the text of
`autocraft-region build` (its measures and problems).

  uv run unreal/Tools/regions/critic.py REGION [--ref IMAGE] [--shots DIR] -o SHEET.jpg

REGION: a region id (unreal/server/regions/<id>.json) or a file's path.
Its pictures are made in unreal/server/build/critic/<id>/.
"""
import argparse, math, re, subprocess, sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
SERVER = ROOT / "unreal/server"
TOOL = SERVER / "build/autocraft-region"
TILE_W, TILE_H, LABEL_H, GAP, COLUMNS = 640, 360, 30, 10, 3
MAX_SHOTS = 15
BG, INK, DIM = (24, 26, 30), (230, 232, 236), (150, 156, 166)


def font(size):
    for path in ("/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Monaco.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"):
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default(size=size)


def run(args):
    """The tool's output, and whether it ran clean."""
    r = subprocess.run([str(TOOL), *map(str, args)], capture_output=True, text=True)
    return r.stdout + r.stderr, r.returncode == 0


def resolve(region):
    p = Path(region)
    if p.suffix == ".json" and p.exists():
        return p.resolve()
    p = SERVER / "regions" / f"{region}.json"
    if not p.exists():
        sys.exit(f"no region file {region} (looked for {p})")
    return p


def open_directions(path, pad):
    """Compass bearings (degrees) from the pad toward its two most open
    directions: the lowest skyline within 60 cells on the ground's levels
    (a cliff as the level next to it, rock as a wall four levels up), the
    second at least 90 degrees from the first."""
    text, _ = run(["levels", path])
    rows = [r for r in text.splitlines() if r and set(r) <= set("0123456789#o")]
    if not rows or pad is None:
        return [0, 180]
    depth, width = len(rows), len(rows[0])

    def height(x, z):
        c = rows[z][x]
        if c.isdigit():
            return int(c) * 2.4
        if c == "o":
            return 4 * 2.4
        # A cliff: the highest level digit beside it.
        near = [rows[zz][xx] for zz in range(max(0, z - 1), min(depth, z + 2))
                for xx in range(max(0, x - 1), min(width, x + 2)) if rows[zz][xx].isdigit()]
        return max((int(c) for c in near), default=2) * 2.4

    px, pz = pad
    eye = height(int(px), int(pz)) + 1.3
    angles = []
    for k in range(32):
        a = k * 360 / 32
        dx, dz = math.sin(math.radians(a)), -math.cos(math.radians(a))
        worst, t = -90.0, 2.0
        while t <= 60:
            x, z = int(px + dx * t), int(pz + dz * t)
            if not (0 <= x < width and 0 <= z < depth):
                break
            worst = max(worst, math.degrees(math.atan2(height(x, z) - eye, t)))
            t += 1
        angles.append((worst, a))
    angles.sort()
    first = angles[0][1]
    second = next((a for _, a in angles if abs((a - first + 180) % 360 - 180) >= 90), (first + 180) % 360)
    return [first, second]


def compass(a):
    return ["N", "NE", "E", "SE", "S", "SW", "W", "NW"][round(a / 45) % 8]


def fitted(path, w, h):
    im = Image.open(path).convert("RGB")
    im.thumbnail((w, h), Image.LANCZOS)
    return im


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("region")
    ap.add_argument("--ref", help="the brief's reference image")
    ap.add_argument("--shots", help="a -AcTerrainLabShots folder: its <region>/game/*.png go on the sheet")
    ap.add_argument("-o", "--out", required=True, help="the sheet (.jpg)")
    a = ap.parse_args()

    subprocess.run(["make", "-s", "-C", str(SERVER), "build/autocraft-region"], check=True)
    path = resolve(a.region)
    rid = path.stem
    tmp = SERVER / "build/critic" / rid
    tmp.mkdir(parents=True, exist_ok=True)

    summary, _ = run(["build", path])
    m = re.search(r'^pad "[^"]*" at \(([-\d.e]+), ([-\d.e]+)\)', summary, re.M)
    pad = (float(m.group(1)), float(m.group(2))) if m else None

    tiles = []  # (label, image path or None, span, note)
    out, _ = run(["preview", path, "-o", tmp / "top.png"])
    tiles.append(("top-down preview (north up)", tmp / "top.png", 2, out))
    size = f"{TILE_W * 2}x{TILE_H * 2}"
    for side, name in (("n", "north"), ("e", "east"), ("s", "south"), ("w", "west")):
        out, _ = run(["view", path, "-o", tmp / f"from-{side}.png", "--from", side, "--size", size])
        tiles.append((f"from the {name}, 35 deg down", tmp / f"from-{side}.png", 1, out))
    for k, bearing in enumerate(open_directions(path, pad)):
        out, _ = run(["view", path, "-o", tmp / f"eye-{k + 1}.png", "--eye", "--from", f"{bearing:g}", "--size", size])
        where = "the first pad" if pad else "the region's middle"
        tiles.append((f"pilot's eye at {where}, looking {compass(bearing)} ({bearing:.0f} deg)",
                      tmp / f"eye-{k + 1}.png", 1, out))
    if a.ref:
        tiles.append(("reference: " + Path(a.ref).name, Path(a.ref), 1, f"cannot read {a.ref}"))
    shots_note = ""
    if a.shots:
        shots = sorted((Path(a.shots) / rid / "game").glob("*.png"))
        if not shots:
            shots_note = f"no in-game stills in {Path(a.shots) / rid / 'game'}"
        if len(shots) > MAX_SHOTS:
            shots_note = f"{len(shots) - MAX_SHOTS} more in-game stills in {Path(a.shots) / rid / 'game'} not shown"
            shots = shots[:MAX_SHOTS]
        for s in shots:
            tiles.append(("in game: " + s.stem, s, 1, ""))

    # Lay the tiles on a grid of COLUMNS, the preview taking 2 by 2.
    taken, places, row = set(), [], 0
    for label, img, span, note in tiles:
        while True:
            free = [c for c in range(COLUMNS - span + 1)
                    if all((row + dr, c + dc) not in taken for dr in range(span) for dc in range(span))]
            if free:
                c = free[0]
                for dr in range(span):
                    for dc in range(span):
                        taken.add((row + dr, c + dc))
                places.append((row, c, span, label, img, note))
                break
            row += 1
    rows_used = max(r for r, _ in taken) + 1
    cell_w, cell_h = TILE_W + GAP, TILE_H + LABEL_H + GAP

    small, big = font(16), font(22)
    lines = [f"autocraft-region build {path.relative_to(ROOT) if path.is_relative_to(ROOT) else path}", ""]
    lines += summary.rstrip().splitlines()
    if shots_note:
        lines += ["", shots_note]
    line_h = 21
    width = GAP + COLUMNS * cell_w
    text_top = 50 + rows_used * cell_h + 10
    sheet = Image.new("RGB", (width, text_top + line_h * (len(lines) + 1) + GAP), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((GAP + 4, 12), f"{rid}: the critic's sheet", font=big, fill=INK)

    for r, c, span, label, img, note in places:
        x, y = GAP + c * cell_w, 50 + r * cell_h
        w, h = span * cell_w - GAP, span * cell_h - GAP - LABEL_H
        draw.text((x + 2, y + 4), label, font=small, fill=INK)
        try:
            im = fitted(img, w, h)
            sheet.paste(im, (x + (w - im.width) // 2, y + LABEL_H + (h - im.height) // 2))
        except Exception:
            draw.rectangle((x, y + LABEL_H, x + w, y + LABEL_H + h), outline=DIM)
            draw.multiline_text((x + 10, y + LABEL_H + 10), (note or "failed").strip()[:600], font=small, fill=(255, 110, 110))

    y = text_top
    for line in lines:
        draw.text((GAP + 4, y), line, font=small, fill=INK if line else DIM)
        y += line_h

    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out, quality=88)
    print(f"wrote {out} ({sheet.width}x{sheet.height}); pictures in {tmp}")


if __name__ == "__main__":
    main()
