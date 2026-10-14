"""The Brood's shared look (docs/creatures.md, "Look and feel"): charcoal chitin with a violet-black sheen, opal ore
flecks crusted on the shell, and a glow from inside in each kind's own colour. Used by shardling.py, gloamwing.py,
lodeback.py and matriarch.py. No team colour: the brood is one side and its glow is fixed per kind.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import sphere  # noqa: E402

# Linear lamp colours, above 1 so they bloom like the game's other lamps.
GLOW = {
    "teal": (0.10, 1.00, 0.80),
    "violet": (0.75, 0.12, 1.00),
    "gold": (1.00, 0.62, 0.16),
    "queen": (0.55, 0.10, 1.00),
}


def materials(m, glow, strength=1.6):
    """Declare the chitin family and one glow (`glow` a GLOW key): `glow_<c>` for eyes and small lights,
    `glowBody_<c>` for big lit surfaces (a sac, a lantern, an egg sac: brighter ones bloom to white), `glowDim_<c>`
    for seams. The strength is the game's lamps' (the sites' lamps peak at 1.5); 6 washed every glow out to white."""
    m.material("chitin", parent="hull", color=(0.030, 0.026, 0.040), metallic=0.25, roughness=0.42)
    m.material("chitinMid", parent="hull", color=(0.060, 0.052, 0.072), metallic=0.2, roughness=0.5)
    m.material("chitinUnder", parent="hull", color=(0.090, 0.080, 0.075), metallic=0.0, roughness=0.8)
    m.material("oreFleck", parent="hull", color=(0.55, 0.52, 0.62), metallic=0.7, roughness=0.18)
    c = GLOW[glow]
    m.material("glow_" + glow, parent="emissive", color=c, emissive=tuple(v * strength for v in c))
    m.material("glowBody_" + glow, parent="emissive", color=c, emissive=tuple(v * strength * 0.3 for v in c))
    m.material("glowDim_" + glow, parent="emissive", color=c, emissive=tuple(v * strength * 0.3 for v in c))
    return "glow_" + glow, "glowDim_" + glow


def flecks(points, r=1.6):
    """Opal ore flecks crusted on the shell: small bright metal beads (decoration, no ray piece)."""
    return [sphere(r * (0.7 + 0.6 * ((i * 37) % 10) / 10), at=p, steps=5) for i, p in enumerate(points)]


def scatter(cx, cy, cz, rx, ry, rz, n, seed=1, top_only=True):
    """n points spread over the upper half of an ellipsoid (cx..; rx..), a fixed pseudo-random pattern."""
    pts = []
    for i in range(n):
        u = ((i * 0.6180339 + seed * 0.137) % 1.0)
        v = ((i * 0.4142135 + seed * 0.291) % 1.0)
        a = 2 * math.pi * u
        e = math.asin(v) if top_only else math.asin(2 * v - 1)
        pts.append((cx + rx * math.cos(e) * math.cos(a), cy + ry * math.cos(e) * math.sin(a), cz + rz * math.sin(e)))
    return pts
