#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Make the admin's world map (admin/world.json) from Natural Earth.

Natural Earth's 1:110m admin-0 countries give the shapes. Countries too small
to see at that scale, and the 65 places that only its 1:50m file has
(Singapore, Malta, Hong Kong, Bahrain...), get a dot at Natural Earth's label
point instead. Both files are public domain and are fetched at a pinned
release and checked against their SHA-256.

The output, in the Natural Earth projection, 1000 units wide, Antarctica left
out:

  {"source": ..., "width": 1000, "height": ...,
   "paths": {"DE": "M...z", ...},     ISO 3166-1 alpha-2 code -> SVG path
   "dots":  {"SG": [x, y], ...}}      places too small for a shape

Codes are ISO_A2_EH (France and Norway have -99 in ISO_A2 for their
overseas parts; Taiwan has CN-TW). Three shapes have no code: Northern
Cyprus is merged into Cyprus (CY) and Somaliland into Somalia (SO), as
Cloudflare reports both under those codes; Kosovo is XK, as Cloudflare
reports it.

  uv run website/tools/world_map.py
"""

import hashlib
import json
import math
import urllib.request
from pathlib import Path

SITE = Path(__file__).resolve().parent.parent
OUT = SITE / "admin" / "world.json"
RELEASE = "v5.1.2"
BASE = f"https://raw.githubusercontent.com/nvkelso/natural-earth-vector/{RELEASE}/geojson/"
FILES = {
    "ne_110m_admin_0_countries.geojson": "6866c877d39cba9c357620878839b336d569f8c662d3cfab4cb1dbe2d39c977f",
    "ne_50m_admin_0_countries.geojson": "3e458fc036ad0a66411f2c1e6cac49c5d7bfb81cb1123bc513b22511a2b7fdeb",
}
# The shapes without an ISO code of their own, by Natural Earth's ADM0_A3.
# Northern Cyprus and Somaliland join another shape; Kosovo is its own.
MERGE = {"CYN": "CY", "SOL": "SO", "KOS": "XK"}
JOINED = {"CYN", "SOL"}
WIDTH = 1000
SOUTH = -58  # degrees: below South America and the southern islands
TINY = 7  # a shape whose box is under this many units both ways (2.5 px on a phone) gets a dot too


def fetch(name):
    body = urllib.request.urlopen(BASE + name, timeout=60).read()
    digest = hashlib.sha256(body).hexdigest()
    if digest != FILES[name]:
        raise SystemExit(f"{name}: sha256 {digest}, expected {FILES[name]}")
    return json.loads(body)


def natural_earth(lon, lat):
    """The Natural Earth projection (Šavrič, Jenny, Patterson, Petrovič, Hurni 2011)."""
    l, p = math.radians(lon), math.radians(lat)
    p2 = p * p
    p4 = p2 * p2
    x = l * (0.870700 - 0.131979 * p2 + p4 * (-0.013791 + p4 * (0.003971 * p2 - 0.001529 * p4)))
    y = p * (1.007226 + p2 * (0.015085 + p4 * (-0.044475 + 0.028874 * p2 - 0.005916 * p4)))
    return x, y


X_MAX = natural_earth(180, 0)[0]
Y_TOP = natural_earth(0, 84)[1]
Y_BOTTOM = natural_earth(0, SOUTH)[1]
SCALE = WIDTH / (2 * X_MAX)
HEIGHT = round((Y_TOP - Y_BOTTOM) * SCALE)


def project(lon, lat):
    x, y = natural_earth(lon, lat)
    return round((x + X_MAX) * SCALE, 1), round((Y_TOP - y) * SCALE, 1)


def num(v):
    s = f"{v:.1f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def ring_path(ring):
    pts = []
    for lon, lat in ring:
        pt = project(lon, lat)
        if not pts or pt != pts[-1]:
            pts.append(pt)
    if len(pts) > 1 and pts[0] == pts[-1]:
        pts.pop()
    if len(pts) < 3:
        return "", pts
    out = [f"M{num(pts[0][0])} {num(pts[0][1])}l"]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        dx, dy = num(x1 - x0), num(y1 - y0)
        sep = "" if out[-1].endswith("l") else ("" if dx.startswith("-") else " ")
        out.append(f"{sep}{dx}{'' if dy.startswith('-') else ' '}{dy}")
    return "".join(out) + "z", pts


def polygons(geometry):
    if geometry["type"] == "Polygon":
        return [geometry["coordinates"]]
    return geometry["coordinates"]


def code_of(props):
    return MERGE.get(props["ADM0_A3"]) or props["ISO_A2_EH"]


def main():
    shapes = fetch("ne_110m_admin_0_countries.geojson")
    labels = fetch("ne_50m_admin_0_countries.geojson")
    paths, boxes, points = {}, {}, {}
    for f in shapes["features"]:
        props = f["properties"]
        code = code_of(props)
        if code == "AQ":
            continue
        if len(code) != 2:
            raise SystemExit(f"no code for {props['ADM0_A3']} {props['NAME']}")
        box = boxes.setdefault(code, [math.inf, math.inf, -math.inf, -math.inf])
        for poly in polygons(f["geometry"]):
            for ring in poly:
                d, pts = ring_path(ring)
                paths[code] = paths.get(code, "") + d
                for x, y in pts:
                    box[:] = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
        if props["ADM0_A3"] not in JOINED:
            points[code] = project(props["LABEL_X"], props["LABEL_Y"])
    dots = {c: list(points[c]) for c, b in boxes.items() if max(b[2] - b[0], b[3] - b[1]) < TINY}
    for f in labels["features"]:
        props = f["properties"]
        code = code_of(props)
        if len(code) == 2 and code != "AQ" and code not in paths:
            dots[code] = list(project(props["LABEL_X"], props["LABEL_Y"]))
    out = {
        "source": f"Natural Earth {RELEASE}, 1:110m admin-0 countries (shapes) and 1:50m (dots), public domain",
        "projection": "Natural Earth",
        "width": WIDTH,
        "height": HEIGHT,
        "paths": dict(sorted((c, d) for c, d in paths.items() if d)),
        "dots": dict(sorted(dots.items())),
    }
    OUT.write_text(json.dumps(out, separators=(",", ":")) + "\n")
    print(f"{OUT.relative_to(SITE)}: {len(out['paths'])} shapes, {len(out['dots'])} dots, {OUT.stat().st_size} bytes")


if __name__ == "__main__":
    main()
