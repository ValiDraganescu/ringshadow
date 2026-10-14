# /// script
# requires-python = ">=3.11"
# dependencies = ["skia-python==144.0.post2"]
# ///
"""The rig screen's icons (docs/prospector-progression.md, "Foundry and Lab").

One atlas in the skill icons' hand (skill_icons.py: its pen, stroke and
motifs): white line glyphs on clear, tinted by the screen. A row of eight
128 px cells: the five modules in `Modules::all()`'s order (drill head,
cargo pod, plate, torch tip, survey lens), then the Foundry, the Lab and
the fitting (a spanner) for the panels' heads.

    uv run unreal/Tools/hud/module_icons.py            # writes the atlas
    uv run unreal/Tools/hud/module_icons.py --sheet    # and a labelled sheet to look at

Out: unreal/Resources/Textures/Skills/module_icons.png (read at run time by
SAcRigScreen.cpp, staged loose by Autocraft.Build.cs).
"""

import math
import sys
from pathlib import Path

import skia

from skill_icons import CELL, Pen, canister, drill, flame, ore, spark

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Resources/Textures/Skills/module_icons.png"
NAMES = ["drillHead", "cargoPod", "plate", "torchTip", "surveyLens", "foundry", "lab", "fitting"]


def bolt(p: Pen, x, y):
    p.circle(x, y, 3.2, p.thin)


def g_drillHead(p: Pen):
    # The bit, point down, on a bolted mount plate: a part, not the skill's drill.
    p.path([(26, 14), (74, 14), (78, 28), (22, 28)], closed=True)
    bolt(p, 32, 21)
    bolt(p, 68, 21)
    p.path([(36, 28), (64, 28), (64, 38), (36, 38)], closed=True)
    p.path([(32, 38), (68, 38), (50, 88)], closed=True)
    p.path([(38, 52), (62, 46)], paint=p.thin)
    p.path([(42, 64), (58, 59)], paint=p.thin)
    p.path([(46, 75), (54, 72)], paint=p.thin)


def g_cargoPod(p: Pen):
    # A capsule pod with its straps and a gem of ore in the window.
    p.rect(14, 30, 86, 74, 20)
    p.path([(32, 30), (32, 74)], paint=p.thin)
    p.path([(68, 30), (68, 74)], paint=p.thin)
    p.path([(40, 22), (60, 22)])
    p.path([(44, 22), (44, 30)], paint=p.thin)
    p.path([(56, 22), (56, 30)], paint=p.thin)
    ore(p, 50, 53, 0.75)
    p.path([(22, 82), (78, 82)], paint=p.thin)


def g_plate(p: Pen):
    # An armour plate: a bevelled hexagon, its inner edge and four rivets.
    outer = [(50, 12), (84, 30), (84, 70), (50, 88), (16, 70), (16, 30)]
    inner = [(50, 24), (73, 36), (73, 64), (50, 76), (27, 64), (27, 36)]
    p.path(outer, closed=True)
    p.path(inner, closed=True, paint=p.thin)
    for (x, y) in [(50, 18), (78, 50), (50, 82), (22, 50)]:
        p.dot(x, y, 2.8)
    p.path([(40, 44), (60, 44)], paint=p.thin)
    p.path([(36, 54), (64, 54)], paint=p.thin)


def g_torchTip(p: Pen):
    # A nozzle on a threaded collar, its flame out of the tip, sparks.
    p.path([(30, 84), (30, 64), (70, 64), (70, 84)], closed=True)
    for y in (70, 77):
        p.path([(30, y), (70, y)], paint=p.thin)
    p.path([(36, 64), (42, 40), (58, 40), (64, 64)], closed=True)
    p.path([(44, 40), (46, 32), (54, 32), (56, 40)], closed=True)
    flame(p, 50, 17, 0.55)
    spark(p, 24, 26, 9)
    spark(p, 78, 30, 7)


def g_surveyLens(p: Pen):
    # A lens in its ring: two rims, a crosshair, a glint; a mount below.
    p.circle(50, 44, 30)
    p.circle(50, 44, 20, p.thin)
    p.path([(50, 30), (50, 38)], paint=p.thin)
    p.path([(50, 50), (50, 58)], paint=p.thin)
    p.path([(36, 44), (44, 44)], paint=p.thin)
    p.path([(56, 44), (64, 44)], paint=p.thin)
    p.arc(50, 44, 13, 200, 55)
    p.path([(40, 74), (60, 74), (64, 88), (36, 88)], closed=True)


def g_foundry(p: Pen):
    # A crucible tipping a stream of melt into a mould; heat lines over it.
    p.c.save()
    p.c.translate(38, 36)
    p.c.rotate(28)
    p.path([(-18, -12), (18, -12), (13, 16), (-13, 16)], closed=True)
    p.path([(-22, -12), (22, -12)])
    p.c.restore()
    p.curve([(58, 34), (66, 44), (64, 62)])
    p.path([(30, 66), (30, 84), (80, 84), (80, 66)])
    p.path([(30, 72), (80, 72)], paint=p.thin)
    p.dot(64, 66, 4)
    for x in (22, 32):
        p.curve([(x, 14), (x - 4, 10), (x, 6)], paint=p.thin)


def g_lab(p: Pen):
    # A flask with its neck and a liquid line, bubbles rising.
    p.path([(40, 14), (60, 14)])
    p.path([(43, 14), (43, 40), (20, 82), (80, 82), (57, 40), (57, 14)])
    p.path([(30, 64), (70, 64)], paint=p.thin)
    p.circle(44, 72, 3.2, p.thin)
    p.circle(56, 70, 2.4, p.thin)
    p.circle(50, 54, 2.4, p.thin)
    p.circle(70, 22, 3.6, p.thin)
    p.circle(76, 10, 2.4, p.thin)


def g_fitting(p: Pen):
    # An open-end spanner across the cell: the slots are fitted by hand at home.
    p.c.save()
    p.c.translate(50, 50)
    p.c.rotate(45)
    p.arc(0, -24, 17, 300, 300)
    p.path([(-8.5, -38.7), (-5, -26), (5, -26), (8.5, -38.7)])
    p.rect(-6, -8, 6, 30, 5)
    p.circle(0, 38, 9)
    p.circle(0, 38, 3.5, p.thin)
    p.c.restore()


GLYPHS = {n: globals()[f"g_{n}"] for n in NAMES}


def render(sheet: bool):
    w, h = CELL * len(NAMES), CELL
    surface = skia.Surface(w, h)
    canvas = surface.getCanvas()
    canvas.clear(skia.ColorTRANSPARENT)
    for col, name in enumerate(NAMES):
        canvas.save()
        canvas.translate(col * CELL, 0)
        canvas.clipRect(skia.Rect.MakeWH(CELL, CELL))
        canvas.scale(CELL / 100, CELL / 100)
        canvas.saveLayer()
        GLYPHS[name](Pen(canvas))
        canvas.restore()
        canvas.restore()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    surface.makeImageSnapshot().save(str(OUT), skia.kPNG)
    print(f"wrote {OUT} ({w}x{h})")
    if sheet:
        label = 22
        s2 = skia.Surface(w, h + label)
        c2 = s2.getCanvas()
        c2.clear(skia.Color(8, 14, 22))
        tint = skia.Paint(ColorFilter=skia.ColorFilters.Blend(skia.Color(140, 220, 255), skia.BlendMode.kSrcIn))
        c2.drawImage(surface.makeImageSnapshot(), 0, 0, skia.SamplingOptions(), tint)
        font = skia.Font(skia.Typeface("Helvetica Neue"), 13)
        for col, name in enumerate(NAMES):
            c2.drawString(name, col * CELL + 6, CELL + 15, font, skia.Paint(Color=skia.Color(160, 175, 190)))
        i = sys.argv.index("--sheet")
        path = Path(sys.argv[i + 1]) if len(sys.argv) > i + 1 else OUT.with_name("module_icons_sheet.png")
        s2.makeImageSnapshot().save(str(path), skia.kPNG)
        print(f"wrote {path}")


if __name__ == "__main__":
    render("--sheet" in sys.argv)
