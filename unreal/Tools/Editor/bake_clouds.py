# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "pillow"]
# ///
"""Bakes the cloud noise once (GAME-LAYER.md, "Clouds" row).

  uv run unreal/Tools/Editor/bake_clouds.py

Writes (rerunnable, deterministic, every texture tiles):
  unreal/Content-src/sky/cloud_weather.png  512 x 512 weather map, linear:
      R coverage (Perlin-Worley fbm: banks and clear gaps), G cloud type
      (0 flat layer, 0.5 cumulus, 1 towering), B a finer coverage field the
      ground shadows mix in, A unused (1).
  unreal/Content-src/sky/cloud_shape.png    128^3 shape noise as a 16 x 8 atlas
      of 128 x 128 slices (2048 x 1024): R Perlin-Worley, G/B/A Worley fbm
      at 4, 8 and 16 cells. The cloud's body.
  unreal/Content-src/sky/cloud_detail.png   32^3 detail noise as an 8 x 4
      atlas (256 x 128): R/G/B Worley fbm at 2, 4 and 8 cells. Eats the
      edges into wisps and billows.

The real-time volumetric cloudscape recipe (the SIGGRAPH Advances talks,
2015-2022): low-frequency Perlin-Worley for the shapes, Worley fbm to erode
them, a 2D weather map for where and which kind. make_cloud_material.py
imports the PNGs and turns the atlases into volume textures.
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "Content-src", "sky")
f32 = np.float32


def remap(x, a, b, c, d):
    return c + (x - a) / (b - a) * (d - c)


def worley(coords, cells, seed):
    """Tiling Worley noise, 1 - distance to the nearest feature point.

    `coords` (..., dims) in [0, 1); `cells` points per side, one per cell."""
    dims = coords.shape[-1]
    rng = np.random.default_rng(seed)
    pts = rng.random((cells,) * dims + (dims,), dtype=f32)
    p = coords * cells
    cell = np.floor(p).astype(np.int32)
    frac = (p - cell).astype(f32)
    best = np.full(coords.shape[:-1], 9.0, f32)
    offs = np.stack(np.meshgrid(*([np.arange(-1, 2)] * dims), indexing="ij"), -1).reshape(-1, dims)
    for o in offs:
        n = (cell + o) % cells
        fp = pts[tuple(n[..., i] for i in range(dims))]
        d = o + fp - frac
        best = np.minimum(best, np.sum(d * d, -1))
    return np.clip(1.0 - np.sqrt(best), 0.0, 1.0).astype(f32)


def worley_fbm(coords, cells, seed):
    return (worley(coords, cells, seed) * 0.625 + worley(coords, cells * 2, seed + 1) * 0.25
            + worley(coords, cells * 4, seed + 2) * 0.125).astype(f32)


def perlin(coords, cells, seed):
    """Tiling gradient noise in about [-1, 1]."""
    dims = coords.shape[-1]
    rng = np.random.default_rng(seed)
    g = rng.normal(size=(cells,) * dims + (dims,)).astype(f32)
    g /= np.linalg.norm(g, axis=-1, keepdims=True)
    p = coords * cells
    cell = np.floor(p).astype(np.int32)
    t = (p - cell).astype(f32)
    fade = t * t * t * (t * (t * 6 - 15) + 10)
    out = np.zeros(coords.shape[:-1], f32)
    for corner in np.ndindex(*([2] * dims)):
        c = np.array(corner, np.int32)
        n = (cell + c) % cells
        gr = g[tuple(n[..., i] for i in range(dims))]
        dot = np.sum(gr * (t - c), -1)
        w = np.ones(coords.shape[:-1], f32)
        for i in range(dims):
            w *= fade[..., i] if corner[i] else 1 - fade[..., i]
        out += w * dot
    return out * (1.5 if dims == 3 else 1.4)


def perlin_fbm(coords, cells, seed, octaves=5):
    s, a, tot = np.zeros(coords.shape[:-1], f32), 1.0, 0.0
    for i in range(octaves):
        s += a * perlin(coords, cells << i, seed + i)
        tot += a
        a *= 0.5
    return np.clip(s / tot * 0.5 + 0.5, 0, 1).astype(f32)


def perlin_worley(coords, cells, seed):
    p = perlin_fbm(coords, cells, seed)
    w = worley_fbm(coords, cells, seed + 50)
    return np.clip(remap(p, w - 1.0, 1.0, 0.0, 1.0), 0, 1).astype(f32)


def grid(n, dims):
    a = (np.arange(n, dtype=f32) + 0.5) / n
    return np.stack(np.meshgrid(*([a] * dims), indexing="ij"), -1)


def to8(x):
    return (np.clip(x, 0, 1) * 255 + 0.5).astype(np.uint8)


def atlas(vol, cols):
    """(z, y, x, c) slices left to right, top to bottom (UVolumeTexture's order)."""
    n, h, w, c = vol.shape
    rows = n // cols
    return vol.reshape(rows, cols, h, w, c).transpose(0, 2, 1, 3, 4).reshape(rows * h, cols * w, c)


def contrast(x, lo, hi):
    return np.clip((x - lo) / (hi - lo), 0, 1)


def stretch(x, lo_pct, hi_pct):
    """0 at the lo_pct percentile, 1 at hi_pct: the same share of clear sky
    whatever the noise's own spread."""
    lo, hi = np.percentile(x, [lo_pct, hi_pct])
    return contrast(x, lo, hi).astype(f32)


def weather():
    c = grid(512, 2)
    # The Perlin-Worley clumps, a little thinner where the low-frequency
    # banks are low: strong banks gave 15 km clear zones, and the map (its
    # shadows are the clouds') sat in sun for 20 minutes.
    banks = stretch(perlin_fbm(c, 2, 20, 3), 5, 95)
    cover = stretch(perlin_worley(c, 6, 10) * (0.7 + 0.3 * banks), 20, 97)
    kind = stretch(perlin_fbm(c, 2, 30, 3), 15, 85)
    fine = stretch(perlin_worley(c, 16, 40), 10, 97)
    img = np.stack([cover, kind, fine, np.ones_like(cover)], -1)
    # numpy rows run along the first axis: x down, y across; the material
    # reads u from world X, v from world Y, so put x across.
    return to8(img.transpose(1, 0, 2))


def shape():
    c = grid(128, 3)  # (x, y, z, 3)
    r = stretch(perlin_worley(c, 4, 100), 1, 99.5)
    g = worley_fbm(c, 4, 200)
    b = worley_fbm(c, 8, 300)
    a = worley_fbm(c, 16, 400)
    vol = np.stack([r, g, b, a], -1).transpose(2, 1, 0, 3)  # (z, y, x, c)
    return to8(atlas(vol, 16))


def detail():
    c = grid(32, 3)
    vol = np.stack([worley_fbm(c, 2, 500), worley_fbm(c, 4, 600), worley_fbm(c, 8, 700)], -1)
    vol = np.concatenate([vol, np.ones(vol.shape[:-1] + (1,), f32)], -1).transpose(2, 1, 0, 3)
    return to8(atlas(vol, 8))


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (("cloud_weather.png", weather), ("cloud_detail.png", detail), ("cloud_shape.png", shape)):
        img = fn()
        Image.fromarray(img, "RGBA").save(os.path.join(OUT, name))
        print(f"{name}: {img.shape[1]} x {img.shape[0]}")


if __name__ == "__main__":
    main()
