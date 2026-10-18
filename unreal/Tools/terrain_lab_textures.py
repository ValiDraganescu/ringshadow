#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "pillow"]
# ///
"""Ground textures for the terrain lab, round 1 (docs/terrain-lab.md, section 4).

Downloads nine CC0 PBR ground sets at 2K, from ambientCG and Poly Haven,
into unreal/Content-src/terrain-lab/raw/<asset id>/ (git-ignored, cached: a
set already there is not fetched again), then packs each layer into two
2048x2048 RGBA PNGs in unreal/Content-src/terrain-lab/:

    TL_<Layer>_CH.png   RGB = colour (sRGB as delivered)
                        A   = height (displacement), stretched to 0-255
    TL_<Layer>_NR.png   R,G = the normal's X and Y, DirectX convention
                        B   = roughness
                        A   = ambient occlusion (255 where the set has none)

The material rebuilds the normal's Z from X and Y. Every set ships a
DirectX normal (ambientCG NormalDX, Poly Haven nor_dx), so nothing is
flipped; a set offering only an OpenGL normal would get its green flipped
(`gl_normal` below). Every set ships a displacement map; a set without one
would get a height from its luminance, blurred a little (`height_from_colour`).
The sources and their licence are in unreal/Resources/Textures/lab/SOURCES.md.

    uv run unreal/Tools/terrain_lab_textures.py [--sheet OUT.jpg]

--sheet also writes a review sheet: each layer's colour and normal tiled
2x2 (seams show at the cross), and its height.
"""
from __future__ import annotations

import argparse
import io
import json
import sys
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "unreal/Content-src/terrain-lab"
RAW = OUT / "raw"
SIZE = 2048
UA = {"User-Agent": "ringshadow-terrain-lab/1.0"}


@dataclass(frozen=True)
class Layer:
    name: str      # TL_<name>_CH / _NR
    site: str      # "ambientCG" or "Poly Haven"
    asset: str     # the site's asset id
    look: str      # one line: what it shows

    @property
    def page(self) -> str:
        if self.site == "ambientCG":
            return f"https://ambientcg.com/view?id={self.asset}"
        return f"https://polyhaven.com/a/{self.asset}"

    @property
    def download(self) -> str:
        if self.site == "ambientCG":
            return f"https://ambientcg.com/get?file={self.asset}_2K-JPG.zip"
        return f"https://api.polyhaven.com/files/{self.asset}"


# The nine layers, in the material's order.
LAYERS = [
    Layer("Dirt", "Poly Haven", "brown_mud_dry",
          "dry brown packed dirt with small pebbles and crumbs"),
    Layer("Grass", "Poly Haven", "grass_ground",
          "sparse dry, yellowed grass over dirt, a few green blades"),
    Layer("Highland", "Poly Haven", "rocks_ground_01",
          "coarse weathered gravel and broken stones, pale beige-grey"),
    Layer("Rock", "Poly Haven", "cliff_side",
          "layered sedimentary cliff face, ochre and rust bands"),
    Layer("Salt", "Poly Haven", "dry_ground_01",
          "baked crust broken into small polygonal cracks, pale"),
    Layer("Ash", "Poly Haven", "gravel_stones",
          "fine dark-grey grit with a few lighter flecks"),
    Layer("Glass", "ambientCG", "Rock035",
          "black, blue-tinged glassy rock with glossy streaks"),
    Layer("Slag", "ambientCG", "Gravel024",
          "dark grey porous clinker, rounded cinders packed tight"),
    Layer("Frost", "Poly Haven", "snow_03",
          "thin patchy snow and rime over gravel and dead grass"),
]

# Map role -> (ambientCG file suffix, Poly Haven map key, Poly Haven format).
MAPS = {
    "colour": ("Color", "Diffuse", "jpg"),
    "normal_dx": ("NormalDX", "nor_dx", "jpg"),
    "normal_gl": ("NormalGL", "nor_gl", "jpg"),
    "rough": ("Roughness", "Rough", "jpg"),
    "ao": ("AmbientOcclusion", "AO", "jpg"),
    "height": ("Displacement", "Displacement", "png"),  # PH png is 16-bit
}


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=300) as r:
        return r.read()


def download(layer: Layer) -> Path:
    """The set's folder in raw/, fetched once."""
    folder = RAW / layer.asset
    done = folder / ".complete"
    if done.exists():
        return folder
    folder.mkdir(parents=True, exist_ok=True)
    print(f"  fetching {layer.site} {layer.asset} ...", flush=True)
    if layer.site == "ambientCG":
        wanted = {f"{layer.asset}_2K-JPG_{suffix}.jpg" for suffix, _, _ in MAPS.values()}
        with zipfile.ZipFile(io.BytesIO(fetch(layer.download))) as z:
            for member in z.namelist():  # only the known map names, flat
                if member in wanted:
                    (folder / member).write_bytes(z.read(member))
    else:
        files = json.loads(fetch(layer.download))
        for role, (_, key, fmt) in MAPS.items():
            entry = files.get(key, {}).get("2k", {}).get(fmt)
            if entry:
                (folder / f"{layer.asset}_{role}_2k.{fmt}").write_bytes(fetch(entry["url"]))
    done.write_text(layer.download + "\n")
    return folder


def map_path(layer: Layer, folder: Path, role: str) -> Path | None:
    suffix, _, fmt = MAPS[role]
    if layer.site == "ambientCG":
        p = folder / f"{layer.asset}_2K-JPG_{suffix}.jpg"
    else:
        p = folder / f"{layer.asset}_{role}_2k.{fmt}"
    return p if p.exists() else None


def load(path: Path, mode: str) -> np.ndarray:
    """An image as floats 0-1 (L or RGB), at SIZE x SIZE."""
    img = Image.open(path)
    if img.mode in ("I;16", "I;16B", "I;16L", "I"):
        a = np.asarray(img, dtype=np.float64) / 65535.0
        if a.ndim == 3:
            a = a[..., 0]
        if mode == "RGB":
            a = np.repeat(a[..., None], 3, axis=-1)
    else:
        if img.mode not in ("L", "RGB"):
            img = img.convert("RGB")
        if img.mode != mode:
            img = img.convert(mode)
        a = np.asarray(img, dtype=np.float64) / 255.0
    if a.shape[:2] != (SIZE, SIZE):
        print(f"    {path.name}: {a.shape[1]}x{a.shape[0]}, resized to {SIZE}")
        chans = [a] if a.ndim == 2 else [a[..., i] for i in range(a.shape[2])]
        chans = [np.asarray(Image.fromarray(c.astype(np.float32), "F")
                            .resize((SIZE, SIZE), Image.LANCZOS)) for c in chans]
        a = chans[0] if a.ndim == 2 else np.stack(chans, axis=-1)
    return np.clip(a, 0.0, 1.0)


def height_from_colour(rgb: np.ndarray) -> np.ndarray:
    """Fallback height: luminance, blurred a little, wrapping at the edges."""
    lum = 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]
    tiled = np.tile(lum, (3, 3)).astype(np.float32)
    blurred = np.asarray(Image.fromarray(tiled, "F").filter(ImageFilter.GaussianBlur(2.0)))
    return blurred[SIZE:2 * SIZE, SIZE:2 * SIZE].astype(np.float64)


def to8(a: np.ndarray) -> np.ndarray:
    return np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)


def pack(layer: Layer, folder: Path) -> list[str]:
    notes = []
    colour = load(map_path(layer, folder, "colour"), "RGB")

    hp = map_path(layer, folder, "height")
    if hp:
        height = load(hp, "L")
    else:
        height = height_from_colour(colour)
        notes.append("height derived from luminance (no displacement map)")
    lo, hi = height.min(), height.max()
    height = (height - lo) / (hi - lo) if hi > lo else np.full_like(height, 0.5)

    np_dx = map_path(layer, folder, "normal_dx")
    if np_dx:
        normal = load(np_dx, "RGB")
    else:
        normal = load(map_path(layer, folder, "normal_gl"), "RGB")
        normal[..., 1] = 1.0 - normal[..., 1]
        notes.append("normal green flipped (OpenGL to DirectX)")

    rough = load(map_path(layer, folder, "rough"), "L")
    ap = map_path(layer, folder, "ao")
    if ap:
        ao = load(ap, "L")
    else:
        ao = np.ones_like(rough)
        notes.append("no AO map: A = 255")

    ch = np.dstack([to8(colour), to8(height)])
    nr = np.dstack([to8(normal[..., 0]), to8(normal[..., 1]), to8(rough), to8(ao)])
    Image.fromarray(ch, "RGBA").save(OUT / f"TL_{layer.name}_CH.png", optimize=True)
    Image.fromarray(nr, "RGBA").save(OUT / f"TL_{layer.name}_NR.png", optimize=True)
    return notes


def sheet(path: Path) -> None:
    """Review sheet: per layer the colour and the normal tiled 2x2, the height."""
    T = 200                    # one tile; 2x2 = 400
    cw, ch_ = 2 * T * 2 + T + 20, 2 * T + 22
    cols = 3
    img = Image.new("RGB", (cols * cw, 3 * ch_), (24, 24, 24))
    d = ImageDraw.Draw(img)
    for i, layer in enumerate(LAYERS):
        x0, y0 = (i % cols) * cw, (i // cols) * ch_
        c = Image.open(OUT / f"TL_{layer.name}_CH.png")
        n = np.asarray(Image.open(OUT / f"TL_{layer.name}_NR.png"), dtype=np.float64) / 255.0
        nx, ny = n[..., 0] * 2 - 1, n[..., 1] * 2 - 1
        nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
        nrgb = Image.fromarray(to8(np.dstack([n[..., 0], n[..., 1], nz * 0.5 + 0.5])), "RGB")
        col = c.convert("RGB").resize((T, T), Image.LANCZOS)
        nor = nrgb.resize((T, T), Image.LANCZOS)
        for gx in (0, T):
            for gy in (0, T):
                img.paste(col, (x0 + gx, y0 + gy))
                img.paste(nor, (x0 + 2 * T + gx, y0 + gy))
        img.paste(c.getchannel("A").resize((T, T), Image.LANCZOS).convert("RGB"),
                  (x0 + 4 * T, y0))
        d.text((x0 + 4, y0 + 2 * T + 4), f"{layer.name}  {layer.site} {layer.asset}",
               fill=(255, 255, 255))
        d.text((x0 + 4 * T + 4, y0 + T + 4), "height", fill=(200, 200, 200))
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, quality=90)
    print(f"sheet: {path}")


def means() -> None:
    """Each layer's mean colour, linear, into means.json: the lab material's
    recolouring divides by it (a constant, not a texture read)."""
    out = {}
    for layer in LAYERS:
        rgb = np.asarray(Image.open(OUT / f"TL_{layer.name}_CH.png").convert("RGB").resize((256, 256)), dtype=np.float64) / 255.0
        lin = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
        out[layer.name] = [round(float(v), 5) for v in lin.reshape(-1, 3).mean(0)]
    (OUT / "means.json").write_text(json.dumps(out, indent=1))
    print("means:", out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--sheet", type=Path, help="also write a review sheet (JPG) here")
    args = ap.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    for layer in LAYERS:
        print(f"{layer.name}: {layer.site} {layer.asset}", flush=True)
        notes = pack(layer, download(layer))
        for note in notes:
            print(f"  {note}")
    means()
    if args.sheet:
        sheet(args.sheet)


if __name__ == "__main__":
    sys.exit(main())
