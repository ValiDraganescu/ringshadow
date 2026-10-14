# /// script
# requires-python = ">=3.11"
# dependencies = ["skia-python==144.0.post2"]
# ///
"""The skill screen's node icons (docs/prospector-progression.md, "On screen").

One atlas, white line glyphs on clear, tinted by the screen (grey locked,
cyan open, the branch colour learned): a row a branch (Deep Vein,
Quicksilver, Torchwright, Wayfinder, Hardshell), a column a node in the
catalogue's order (`Progression.cpp`), 13 to a row, 128 px a cell. Every
glyph is drawn on a 100-unit grid with the same stroke, round caps and
joins, so they read as one set.

    uv run unreal/Tools/hud/skill_icons.py            # writes the atlas
    uv run unreal/Tools/hud/skill_icons.py --sheet    # and a labelled sheet to look at

Out: unreal/Resources/Textures/Skills/skill_icons.png (read at run time by
SAcSkillScreen.cpp, staged loose by Autocraft.Build.cs).
"""

import math
import sys
from pathlib import Path

import skia

CELL = 128
COLS = 13
ROWS = 5
STROKE = 6.5  # in grid units (100 to a cell)
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Resources/Textures/Skills/skill_icons.png"

# The catalogue's order (Progression.h `Skill`), a row a branch.
NAMES = [
    ["quickDrill", "bigHaul", "cleanCut", "richVein", "coreSampler", "overcharge", "deepBore", "oreSled",
     "quickRegrowth", "motherlode", "seismicCharge", "densePack", "stardustHeart"],
    ["rigMaster", "bigTank", "coldSeal", "derrickTender", "twinCanister", "quickfill", "pressureValve",
     "quicksilverStride", "wellspring", "mercuryLungs", "hydrogenBoost", "fieldPump", "silverTide"],
    ["fastWelder", "patchwork", "prefab", "reinforced", "longArm", "arcSurge", "siteBoss", "blueprintSavant",
     "overbuild", "hardenedWorks", "emergencyWeld", "efficientPlans", "headStart"],
    ["lightFrame", "surveyor", "longSight", "dustRunner", "scannerPulse", "cliffHop", "jumpJets", "padLink",
     "homingBeacon", "luckyStrike", "sprint", "shortHop", "ringwalker"],
    ["plating", "hardHat", "cuttingTorch", "shield", "selfMend", "pulseMine", "longTorch", "bulwark", "secondWind",
     "rigArmor", "aegis", "torchRhythm", "ironclad"],
]


# MARK: - The pen


class Pen:
    def __init__(self, canvas: skia.Canvas):
        self.c = canvas
        self.line = skia.Paint(AntiAlias=True, Style=skia.Paint.kStroke_Style, StrokeWidth=STROKE,
                               StrokeCap=skia.Paint.kRound_Cap, StrokeJoin=skia.Paint.kRound_Join,
                               Color=skia.ColorWHITE)
        self.thin = skia.Paint(self.line)
        self.thin.setStrokeWidth(STROKE * 0.6)
        self.fill = skia.Paint(AntiAlias=True, Style=skia.Paint.kFill_Style, Color=skia.ColorWHITE)
        self.soft = skia.Paint(AntiAlias=True, Style=skia.Paint.kFill_Style, Color=skia.Color4f(1, 1, 1, 0.35))
        self.clear = skia.Paint(AntiAlias=True, Style=skia.Paint.kFill_Style, BlendMode=skia.BlendMode.kClear)
        self.clearLine = skia.Paint(self.line)
        self.clearLine.setBlendMode(skia.BlendMode.kClear)
        self.clearLine.setStrokeWidth(STROKE * 2.2)
        self.cut = skia.Paint(self.line)
        self.cut.setBlendMode(skia.BlendMode.kClear)
        self.cut.setStrokeWidth(STROKE * 0.8)

    def path(self, pts, closed=False, paint=None):
        p = skia.Path()
        p.moveTo(*pts[0])
        for q in pts[1:]:
            p.lineTo(*q)
        if closed:
            p.close()
        self.c.drawPath(p, paint or self.line)
        return p

    def poly(self, pts, paint=None):
        return self.path(pts, True, paint or self.fill)

    def circle(self, x, y, r, paint=None):
        self.c.drawCircle(x, y, r, paint or self.line)

    def dot(self, x, y, r=4.5):
        self.c.drawCircle(x, y, r, self.fill)

    def arc(self, x, y, r, start, sweep, paint=None):
        self.c.drawArc(skia.Rect.MakeLTRB(x - r, y - r, x + r, y + r), start, sweep, False, paint or self.line)

    def rect(self, l, t, r, b, radius=3, paint=None):
        self.c.drawRoundRect(skia.Rect.MakeLTRB(l, t, r, b), radius, radius, paint or self.line)

    def curve(self, pts, paint=None):
        """A quadratic chain: p0, c1, p1, c2, p2..."""
        p = skia.Path()
        p.moveTo(*pts[0])
        for i in range(1, len(pts) - 1, 2):
            p.quadTo(*pts[i], *pts[i + 1])
        self.c.drawPath(p, paint or self.line)

    def text(self, s, x, y, size=26):
        font = skia.Font(skia.Typeface("Helvetica Neue", skia.FontStyle.Bold()), size)
        w = font.measureText(s)
        self.c.drawString(s, x - w / 2, y + size * 0.36, font, self.fill)


# MARK: - Motifs shared by the set


def drill(p: Pen, x, y, s=1.0, angle=0):
    """The drill bit: a tapering spiral cone, point down."""
    p.c.save()
    p.c.translate(x, y)
    p.c.rotate(angle)
    p.c.scale(s, s)
    p.path([(-14, -22), (14, -22), (0, 24)], closed=True)
    p.path([(-9, -8), (9, -14)], paint=p.thin)
    p.path([(-5, 4), (6, 0)], paint=p.thin)
    p.path([(-6, -34), (6, -34), (6, -22), (-6, -22)], closed=True)
    p.c.restore()


def ore(p: Pen, x, y, s=1.0, filled=False):
    """A chunk of Stardust Ore: a faceted gem."""
    pts = [(x - 14 * s, y - 2 * s), (x - 6 * s, y - 14 * s), (x + 8 * s, y - 14 * s), (x + 15 * s, y - 2 * s),
           (x + 1 * s, y + 15 * s)]
    if filled:
        p.poly(pts)
    else:
        p.path(pts, closed=True)
        p.path([(x - 14 * s, y - 2 * s), (x + 15 * s, y - 2 * s)], paint=p.thin)
        p.path([(x - 6 * s, y - 14 * s), (x - 2 * s, y - 2 * s), (x + 1 * s, y + 15 * s)], paint=p.thin)
        p.path([(x + 8 * s, y - 14 * s), (x + 5 * s, y - 2 * s)], paint=p.thin)


def drop(p: Pen, x, y, s=1.0, filled=False):
    """A drop of Metallic Hydrogen."""
    path = skia.Path()
    path.moveTo(x, y - 20 * s)
    path.cubicTo(x + 4 * s, y - 10 * s, x + 14 * s, y - 2 * s, x + 14 * s, y + 7 * s)
    path.cubicTo(x + 14 * s, y + 15 * s, x + 7 * s, y + 21 * s, x, y + 21 * s)
    path.cubicTo(x - 7 * s, y + 21 * s, x - 14 * s, y + 15 * s, x - 14 * s, y + 7 * s)
    path.cubicTo(x - 14 * s, y - 2 * s, x - 4 * s, y - 10 * s, x, y - 20 * s)
    path.close()
    p.c.drawPath(path, p.fill if filled else p.line)
    if not filled:
        p.arc(x, y + 7 * s, 7 * s, 100, 70, p.thin)


def canister(p: Pen, x, y, s=1.0):
    p.rect(x - 11 * s, y - 16 * s, x + 11 * s, y + 20 * s, 6 * s)
    p.path([(x - 5 * s, y - 22 * s), (x + 5 * s, y - 22 * s)])
    p.path([(x - 11 * s, y - 2 * s), (x + 11 * s, y - 2 * s)], paint=p.thin)


def flame(p: Pen, x, y, s=1.0, filled=False, down=False):
    if down:
        p.c.save()
        p.c.translate(x, y)
        p.c.rotate(180)
        flame(p, 0, 0, s, filled)
        p.c.restore()
        return
    path = skia.Path()
    path.moveTo(x, y - 22 * s)
    path.cubicTo(x + 6 * s, y - 10 * s, x + 16 * s, y - 4 * s, x + 14 * s, y + 8 * s)
    path.cubicTo(x + 12 * s, y + 18 * s, x + 4 * s, y + 22 * s, x, y + 22 * s)
    path.cubicTo(x - 6 * s, y + 22 * s, x - 14 * s, y + 16 * s, x - 14 * s, y + 6 * s)
    path.cubicTo(x - 14 * s, y - 2 * s, x - 6 * s, y - 4 * s, x - 6 * s, y - 12 * s)
    path.cubicTo(x - 2 * s, y - 8 * s, x - 2 * s, y - 16 * s, x, y - 22 * s)
    path.close()
    p.c.drawPath(path, p.fill if filled else p.line)


def torch(p: Pen, x, y, s=1.0, angle=-40):
    """The welding torch: a handle, a nozzle, a spark at the tip."""
    p.c.save()
    p.c.translate(x, y)
    p.c.rotate(angle)
    p.c.scale(s, s)
    p.rect(-6, 4, 6, 30, 3)
    p.path([(-4, 4), (-3, -14), (3, -14), (4, 4)], closed=True)
    p.path([(0, -14), (0, -22)])
    p.c.restore()


def spark(p: Pen, x, y, r=9):
    for a in range(0, 360, 45):
        rr = r if a % 90 == 0 else r * 0.6
        dx, dy = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.path([(x + dx * rr * 0.35, y + dy * rr * 0.35), (x + dx * rr, y + dy * rr)], paint=p.thin)


def house(p: Pen, x, y, s=1.0):
    """A building: a dome on a block."""
    p.path([(x - 20 * s, y + 16 * s), (x - 20 * s, y), (x + 20 * s, y), (x + 20 * s, y + 16 * s)], closed=True)
    p.arc(x, y, 14 * s, 180, 180)
    p.path([(x - 5 * s, y + 16 * s), (x - 5 * s, y + 7 * s), (x + 5 * s, y + 7 * s), (x + 5 * s, y + 16 * s)])


def boot(p: Pen, x, y, s=1.0):
    p.path([(x - 10 * s, y - 20 * s), (x - 10 * s, y + 10 * s), (x - 12 * s, y + 18 * s), (x + 18 * s, y + 18 * s),
            (x + 18 * s, y + 10 * s), (x + 4 * s, y + 4 * s), (x + 4 * s, y - 20 * s)], closed=True)
    p.path([(x - 12 * s, y + 11 * s), (x + 18 * s, y + 11 * s)], paint=p.thin)


def speedLines(p: Pen, x, y, n=3, length=18, gap=9):
    for i in range(n):
        yy = y + (i - (n - 1) / 2) * gap
        off = abs(i - (n - 1) / 2) * 5
        p.path([(x - length + off, yy), (x + off * 0.2, yy)], paint=p.thin)


def shieldShape(p: Pen, x, y, s=1.0, filled=False, paint=None):
    path = skia.Path()
    path.moveTo(x, y - 24 * s)
    path.lineTo(x + 20 * s, y - 16 * s)
    path.cubicTo(x + 20 * s, y + 4 * s, x + 12 * s, y + 18 * s, x, y + 26 * s)
    path.cubicTo(x - 12 * s, y + 18 * s, x - 20 * s, y + 4 * s, x - 20 * s, y - 16 * s)
    path.close()
    p.c.drawPath(path, paint or (p.fill if filled else p.line))
    return path


def plus(p: Pen, x, y, r=7, paint=None):
    p.path([(x - r, y), (x + r, y)], paint=paint)
    p.path([(x, y - r), (x, y + r)], paint=paint)


def arrowUp(p: Pen, x, y, h=16, paint=None):
    p.path([(x, y + h / 2), (x, y - h / 2)], paint=paint)
    p.path([(x - h * 0.35, y - h / 2 + h * 0.35), (x, y - h / 2), (x + h * 0.35, y - h / 2 + h * 0.35)], paint=paint)


def faded(paint, alpha):
    out = skia.Paint(paint)
    out.setColor4f(skia.Color4f(1, 1, 1, alpha))
    return out


def badge(p: Pen, x, y, r=11):
    """A small solid disc with a hole knocked out round it, for a corner mark."""
    p.c.drawCircle(x, y, r + 4, p.clear)
    p.c.drawCircle(x, y, r, p.fill)


def knock(p: Pen, x, y, r):
    p.c.drawCircle(x, y, r, p.clear)


def pips(p: Pen, x, y, n, gap=9):
    for i in range(n):
        p.dot(x + (i - (n - 1) / 2) * gap, y, 3.6)


def lightning(p: Pen, x, y, s=1.0, filled=True):
    pts = [(x + 4 * s, y - 22 * s), (x - 10 * s, y + 2 * s), (x - 1 * s, y + 2 * s), (x - 5 * s, y + 22 * s),
           (x + 10 * s, y - 4 * s), (x + 1 * s, y - 4 * s)]
    if filled:
        p.poly(pts)
    else:
        p.path(pts, closed=True)


def gauge(p: Pen, x, y, r, needle):
    p.arc(x, y, r, 150, 240)
    a = math.radians(150 + 240 * needle)
    p.path([(x, y), (x + math.cos(a) * r * 0.75, y + math.sin(a) * r * 0.75)])
    p.dot(x, y, 4)


def ring(p: Pen, x, y, rx, ry, angle=-18, paint=None):
    p.c.save()
    p.c.translate(x, y)
    p.c.rotate(angle)
    p.c.drawOval(skia.Rect.MakeLTRB(-rx, -ry, rx, ry), paint or p.line)
    p.c.restore()


# MARK: - The glyphs (centre 50, 50; keep within 12..88)


def g_quickDrill(p):
    drill(p, 44, 54, 1.05)
    speedLines(p, 84, 40, 3, 16, 9)


def g_bigHaul(p):
    # A loaded ore sack-pod: a tub with gems heaped over it.
    p.path([(18, 52), (24, 84), (76, 84), (82, 52)], closed=True)
    ore(p, 36, 40, 0.75, True)
    ore(p, 62, 38, 0.8, True)
    ore(p, 50, 26, 0.6, True)
    plus(p, 50, 68, 8, p.thin)


def g_cleanCut(p):
    ore(p, 50, 56, 1.55)
    # A clean slice off its top corner.
    p.c.drawLine(18, 26, 82, 46, p.clearLine)
    p.path([(18, 25), (82, 45)], paint=p.thin)


def g_richVein(p):
    # A seam through rock with two gems on it.
    p.curve([(14, 76), (36, 66), (48, 52), (60, 38), (86, 26)])
    ore(p, 34, 66, 0.62, True)
    ore(p, 66, 36, 0.62, True)
    p.path([(14, 88), (28, 80)], paint=p.thin)
    p.path([(72, 16), (86, 12)], paint=p.thin)


def g_coreSampler(p):
    # A core tube with banded strata.
    p.rect(36, 14, 64, 86, 12)
    for yy in (32, 48, 64):
        p.path([(36, yy), (64, yy)], paint=p.thin)
    p.dot(50, 40, 4)
    p.dot(50, 72, 4)


def g_overcharge(p):
    drill(p, 40, 56, 0.95)
    lightning(p, 72, 36, 0.95)


def g_deepBore(p):
    drill(p, 50, 44, 0.85)
    for i, yy in enumerate((74, 82, 90)):
        w = 30 - i * 9
        p.path([(50 - w, yy - 4), (50 + w, yy - 4)], paint=p.thin)


def g_oreSled(p):
    # A sled with runners, piled with ore.
    p.path([(14, 64), (78, 64), (86, 56)])
    p.path([(18, 76), (82, 76), (90, 66)])
    p.path([(30, 64), (30, 76)], paint=p.thin)
    p.path([(66, 64), (66, 76)], paint=p.thin)
    ore(p, 36, 46, 0.72, True)
    ore(p, 60, 46, 0.72, True)


def g_quickRegrowth(p):
    # A gem sprouting from the ground, a clock arc round it.
    ore(p, 50, 56, 0.9)
    p.arc(50, 52, 34, 200, 260)
    p.path([(80, 34), (84, 22), (72, 26)], closed=False)
    p.path([(22, 84), (78, 84)], paint=p.thin)


def g_motherlode(p):
    # Three gems stacked: triple.
    ore(p, 30, 64, 0.85, True)
    ore(p, 70, 64, 0.85, True)
    ore(p, 50, 34, 0.95, True)


def g_seismicCharge(p):
    # Shock rings from a charge in the ground.
    p.path([(14, 66), (86, 66)])
    p.arc(50, 66, 14, 180, 180)
    p.arc(50, 66, 26, 195, 150, p.thin)
    p.arc(50, 66, 38, 205, 130, p.thin)
    p.rect(44, 70, 56, 86, 2, p.fill)


def g_densePack(p):
    # A crate pressed by arrows from both sides.
    p.rect(32, 30, 68, 70, 4)
    ore(p, 50, 50, 0.55, True)
    p.path([(12, 50), (26, 50)])
    p.path([(20, 43), (27, 50), (20, 57)])
    p.path([(88, 50), (74, 50)])
    p.path([(80, 43), (73, 50), (80, 57)])


def g_stardustHeart(p):
    # A star in a gem: the capstone.
    ore(p, 50, 54, 2.1)
    star(p, 50, 44, 13, 6)


def star(p, x, y, r, ri, n=4, paint=None):
    pts = []
    for i in range(n * 2):
        a = math.radians(-90 + i * 180 / n)
        rr = r if i % 2 == 0 else ri
        pts.append((x + math.cos(a) * rr, y + math.sin(a) * rr))
    p.poly(pts, paint)


def g_rigMaster(p):
    # A pump gauge running fast.
    gauge(p, 50, 56, 30, 0.82)
    p.path([(32, 86), (68, 86)], paint=p.thin)
    drop(p, 50, 24, 0.45, True)


def g_bigTank(p):
    canister(p, 50, 54, 1.45)
    plus(p, 50, 64, 8, p.thin)


def g_coldSeal(p):
    # A drop under a snowflake seal.
    drop(p, 42, 58, 1.2)
    for a in range(0, 180, 60):
        dx, dy = math.cos(math.radians(a)) * 12, math.sin(math.radians(a)) * 12
        p.path([(74 - dx, 28 - dy), (74 + dx, 28 + dy)], paint=p.thin)


def g_derrickTender(p):
    # A derrick tower with a tank filling beside it.
    p.path([(22, 86), (36, 18), (50, 86)])
    p.path([(26, 66), (46, 66)], paint=p.thin)
    p.path([(30, 44), (42, 44)], paint=p.thin)
    p.rect(58, 42, 84, 86, 6)
    p.rect(62, 64, 80, 82, 3, p.fill)
    drop(p, 71, 24, 0.4, True)


def g_twinCanister(p):
    canister(p, 34, 54, 1.1)
    canister(p, 66, 54, 1.1)


def g_quickfill(p):
    canister(p, 40, 54, 1.25)
    p.rect(32, 54, 48, 74, 3, p.fill)
    lightning(p, 74, 34, 0.8)


def g_pressureValve(p):
    # A valve wheel on a pipe.
    p.path([(12, 70), (88, 70)])
    p.path([(50, 70), (50, 46)])
    p.circle(50, 32, 16)
    p.path([(34, 32), (66, 32)], paint=p.thin)
    p.path([(50, 16), (50, 48)], paint=p.thin)
    p.dot(50, 32, 5)


def g_quicksilverStride(p):
    boot(p, 52, 50, 1.2)
    drop(p, 82, 26, 0.38, True)
    speedLines(p, 30, 62, 3, 14, 8)


def g_wellspring(p):
    # A spring welling up from a well's lip.
    p.path([(18, 84), (18, 70), (82, 70), (82, 84)])
    p.curve([(50, 70), (50, 46), (34, 34)])
    p.curve([(50, 70), (50, 46), (66, 34)])
    drop(p, 50, 26, 0.45, True)
    drop(p, 26, 46, 0.3, True)
    drop(p, 74, 46, 0.3, True)


def g_mercuryLungs(p):
    # Two lung-shaped tanks on a pipe.
    p.path([(50, 14), (50, 40)])
    p.path([(50, 40), (40, 46)])
    p.path([(50, 40), (60, 46)])
    p.c.drawRoundRect(skia.Rect.MakeLTRB(16, 38, 44, 86), 14, 14, p.line)
    p.c.drawRoundRect(skia.Rect.MakeLTRB(56, 38, 84, 86), 14, 14, p.line)
    p.rect(22, 62, 38, 80, 6, p.fill)
    p.rect(62, 62, 78, 80, 6, p.fill)


def g_hydrogenBoost(p):
    # A drop with a jet flame under it.
    drop(p, 50, 34, 1.0, True)
    flame(p, 50, 74, 0.6, False, True)
    speedLines(p, 26, 30, 3, 12, 9)


def g_fieldPump(p):
    # A hand pump on bare ground: a lever over a spout.
    p.path([(14, 86), (86, 86)], paint=p.thin)
    p.rect(40, 40, 54, 86, 3)
    p.path([(54, 50), (72, 50), (72, 60)])
    p.path([(36, 36), (72, 18)])
    p.dot(47, 34, 5)
    drop(p, 72, 72, 0.32, True)


def g_silverTide(p):
    # A wave of quicksilver, a star in it: the capstone.
    p.curve([(12, 62), (24, 44), (38, 56), (52, 68), (64, 52), (76, 36), (88, 50)])
    p.curve([(12, 80), (24, 62), (38, 74), (52, 86), (64, 70), (76, 54), (88, 68)], p.thin)
    star(p, 50, 30, 14, 6)


def g_fastWelder(p):
    torch(p, 40, 56, 1.05)
    spark(p, 60, 28, 14)
    speedLines(p, 86, 66, 3, 14, 8)


def g_patchwork(p):
    # A plate with a patch welded on, stitches round it.
    p.rect(16, 20, 84, 80, 6)
    p.rect(40, 38, 70, 64, 2)
    for xx in range(42, 72, 8):
        p.path([(xx, 34), (xx + 3, 36)], paint=p.thin)
        p.path([(xx, 68), (xx + 3, 66)], paint=p.thin)


def g_prefab(p):
    # Panels stacked, a price tag cut.
    p.path([(18, 40), (50, 26), (82, 40), (50, 54)], closed=True)
    p.path([(18, 40), (18, 62), (50, 76), (82, 62), (82, 40)])
    p.path([(50, 54), (50, 76)])


def g_reinforced(p):
    house(p, 44, 50, 1.25)
    badge(p, 76, 30, 13)
    plus(p, 76, 30, 7, p.cut)


def g_longArm(p):
    # The torch on a reach arrow.
    torch(p, 26, 70, 0.75, -90)
    p.path([(42, 70), (84, 70)])
    p.path([(74, 60), (84, 70), (74, 80)])
    spark(p, 70, 34, 12)


def g_arcSurge(p):
    # Two arcs of lightning off the torch.
    torch(p, 34, 64, 0.95, -35)
    lightning(p, 70, 30, 0.75)
    p.curve([(56, 52), (66, 58), (74, 54), (82, 50), (86, 60)], p.thin)


def g_siteBoss(p):
    # A hard-hat over a building.
    house(p, 50, 62, 1.0)
    p.arc(50, 34, 16, 180, 180)
    p.path([(28, 34), (72, 34)])
    p.path([(50, 18), (50, 26)], paint=p.thin)


def g_blueprintSavant(p):
    # A blueprint sheet with a grid and a dome drawn on it.
    p.rect(14, 18, 86, 82, 4)
    for xx in (32, 50, 68):
        p.path([(xx, 18), (xx, 82)], paint=faded(p.thin, 0.45))
    for yy in (34, 50, 66):
        p.path([(14, yy), (86, yy)], paint=faded(p.thin, 0.45))
    p.arc(50, 62, 18, 180, 180)
    p.path([(26, 62), (74, 62)])


def g_overbuild(p):
    house(p, 28, 64, 0.6)
    house(p, 72, 64, 0.6)
    house(p, 50, 36, 0.6)
    plus(p, 50, 72, 7, p.thin)


def g_hardenedWorks(p):
    house(p, 50, 64, 1.0)
    shieldShape(p, 50, 30, 0.6, True)


def g_emergencyWeld(p):
    # A cross on a building: first aid for walls.
    house(p, 44, 58, 1.15)
    badge(p, 74, 30, 14)
    plus(p, 74, 30, 8, p.cut)


def g_efficientPlans(p):
    # A rolled plan with a drop crossed down: less MH.
    p.rect(14, 30, 64, 76, 4)
    p.circle(64, 38, 8, p.thin)
    p.path([(24, 44), (52, 44)], paint=p.thin)
    p.path([(24, 56), (46, 56)], paint=p.thin)
    p.path([(24, 66), (52, 66)], paint=p.thin)
    drop(p, 76, 64, 0.55, True)
    p.path([(76, 84), (76, 90)], paint=p.thin)


def g_headStart(p):
    # A building already a quarter up, a star: the capstone.
    p.rect(20, 40, 80, 84, 4)
    p.rect(20, 66, 80, 84, 4, p.fill)
    p.path([(40, 66), (40, 40)], paint=p.thin)
    p.path([(60, 66), (60, 40)], paint=p.thin)
    star(p, 50, 22, 13, 6)


def g_lightFrame(p):
    # A runner's frame: a chevron stride with speed lines.
    p.path([(26, 30), (46, 50), (26, 70)])
    p.path([(50, 30), (70, 50), (50, 70)])
    speedLines(p, 86, 50, 3, 10, 10)


def g_surveyor(p):
    # A compass rose with the needle on a site.
    p.circle(50, 50, 32)
    p.poly([(50, 22), (57, 50), (50, 56), (43, 50)])
    p.path([(50, 56), (50, 76)], paint=p.thin)
    for a in (0, 90, 180, 270):
        dx, dy = math.cos(math.radians(a)), math.sin(math.radians(a))
        p.path([(50 + dx * 32, 50 + dy * 32), (50 + dx * 40, 50 + dy * 40)], paint=p.thin)


def g_longSight(p):
    # An eye with a long gaze line.
    p.curve([(12, 50), (34, 26), (56, 50)])
    p.curve([(12, 50), (34, 74), (56, 50)])
    p.circle(34, 50, 8, p.fill)
    p.path([(62, 50), (88, 50)], paint=p.thin)
    p.path([(80, 43), (88, 50), (80, 57)], paint=p.thin)


def g_dustRunner(p):
    boot(p, 56, 46, 1.15)
    for i, (x, y, r) in enumerate([(26, 78, 7), (16, 70, 5), (34, 86, 4)]):
        p.circle(x, y, r, p.thin)


def g_scannerPulse(p):
    # A dish sending rings.
    p.arc(36, 64, 22, 135, 180)
    p.path([(20, 48), (52, 80)])
    p.path([(36, 64), (36, 86)], paint=p.thin)
    p.arc(46, 54, 22, -80, 70, p.thin)
    p.arc(46, 54, 34, -80, 70, p.thin)
    p.arc(46, 54, 46, -78, 66, p.thin)


def g_cliffHop(p):
    # A ledge, a jump arc up onto it.
    p.path([(12, 84), (46, 84), (46, 40), (88, 40)])
    p.curve([(24, 76), (34, 18), (70, 32)])
    p.path([(60, 24), (70, 32), (60, 38)])


def g_jumpJets(p):
    # A boot with two flames under it.
    boot(p, 50, 32, 0.9)
    flame(p, 40, 72, 0.5, True, True)
    flame(p, 62, 74, 0.5, True, True)


def g_padLink(p):
    # A landing pad with a reach ring round it.
    ring(p, 50, 60, 18, 8, 0, p.fill)
    ring(p, 50, 60, 34, 15, 0, p.thin)
    p.arc(50, 60, 40, 200, 140, p.thin)
    arrowUp(p, 50, 30, 22)


def g_homingBeacon(p):
    # A beacon mast with signal arcs.
    p.path([(36, 86), (50, 40), (64, 86)])
    p.path([(42, 66), (58, 66)], paint=p.thin)
    p.dot(50, 34, 6)
    p.arc(50, 34, 16, -150, 120, p.thin)
    p.arc(50, 34, 16, 30, 120, p.thin)
    p.arc(50, 34, 28, -145, 110, p.thin)
    p.arc(50, 34, 28, 35, 110, p.thin)


def g_luckyStrike(p):
    # A chip with a sparkle: a survey chip found.
    p.rect(26, 34, 70, 78, 6)
    for xx in (36, 48, 60):
        p.path([(xx, 78), (xx, 86)], paint=p.thin)
        p.path([(xx, 26), (xx, 34)], paint=p.thin)
    p.rect(38, 46, 58, 66, 2, p.fill)
    star(p, 78, 24, 11, 4)


def g_sprint(p):
    # Double chevron with a long streak.
    p.path([(36, 24), (62, 50), (36, 76)])
    p.path([(58, 24), (84, 50), (58, 76)])
    speedLines(p, 28, 50, 3, 16, 12)


def g_shortHop(p):
    # A lander over a short dashed arc between two pads.
    p.path([(14, 80), (30, 80)])
    p.path([(70, 80), (86, 80)])
    for t0 in range(0, 6):
        a0 = math.radians(180 + t0 * 30 + 4)
        a1 = math.radians(180 + t0 * 30 + 20)
        p.path([(50 + math.cos(a0) * 28, 80 + math.sin(a0) * 36), (50 + math.cos(a1) * 28, 80 + math.sin(a1) * 36)],
               paint=p.thin)
    p.poly([(50, 30), (60, 50), (50, 46), (40, 50)])


def g_ringwalker(p):
    # The gas giant's ring with a path walking across it: the capstone.
    p.circle(50, 50, 18)
    ring(p, 50, 50, 40, 12, -18)
    knock(p, 50, 50, 0)
    star(p, 78, 22, 11, 5)


def g_plating(p):
    # Riveted plates overlapping.
    p.rect(16, 20, 64, 64, 4)
    p.rect(36, 40, 84, 84, 4)
    for x, y in [(24, 28), (56, 28), (24, 56), (44, 76), (76, 48), (76, 76)]:
        p.dot(x, y, 3.6)


def g_hardHat(p):
    p.arc(50, 60, 28, 180, 180)
    p.path([(14, 62), (86, 62)])
    p.path([(50, 32), (50, 48)], paint=p.thin)
    p.rect(40, 66, 60, 76, 3, p.thin)


def g_cuttingTorch(p):
    torch(p, 40, 60, 1.1, 40)
    flame(p, 70, 26, 0.5, True)


def g_shield(p):
    shieldShape(p, 50, 50, 1.45)


def g_selfMend(p):
    shieldShape(p, 50, 50, 1.45)
    plus(p, 50, 50, 11)


def g_pulseMine(p):
    p.circle(50, 56, 14, p.fill)
    p.circle(50, 56, 26, p.thin)
    p.circle(50, 56, 38, faded(p.thin, 0.5))
    p.path([(50, 42), (50, 22)], paint=p.thin)
    spark(p, 50, 16, 9)


def g_longTorch(p):
    torch(p, 24, 70, 0.7, 50)
    flame(p, 50, 44, 0.42, True)
    p.path([(60, 38), (86, 22)])
    p.path([(76, 20), (86, 22), (82, 32)])


def g_bulwark(p):
    # A wall of blocks.
    for row, y in enumerate((30, 50, 70)):
        off = 0 if row % 2 == 0 else 12
        for x in range(14 + off, 86, 24):
            p.rect(x, y - 8, min(x + 22, 86), y + 8, 2)


def g_secondWind(p):
    # A heart with a circular arrow.
    path = skia.Path()
    path.moveTo(50, 70)
    path.cubicTo(26, 54, 26, 32, 40, 32)
    path.cubicTo(46, 32, 50, 38, 50, 40)
    path.cubicTo(50, 38, 54, 32, 60, 32)
    path.cubicTo(74, 32, 74, 54, 50, 70)
    path.close()
    p.c.drawPath(path, p.fill)
    p.arc(50, 50, 36, -60, 290, p.thin)
    p.path([(66, 12), (69, 19), (61, 22)], paint=p.thin)


def g_rigArmor(p):
    # A chest plate.
    p.path([(22, 22), (40, 18), (50, 26), (60, 18), (78, 22), (82, 52), (66, 84), (34, 84), (18, 52)],
           closed=True)
    p.path([(50, 26), (50, 84)], paint=p.thin)
    p.path([(26, 50), (74, 50)], paint=p.thin)


def g_aegis(p):
    shieldShape(p, 50, 50, 1.45, True)
    knock(p, 50, 48, 0)
    lightning(p, 50, 50, 0.7, True)
    p.c.save()
    p.c.translate(0, 0)
    p.c.restore()


def g_torchRhythm(p):
    # Three strikes, the third heavy.
    # Three strikes, the third a heavy one.
    p.path([(16, 40), (30, 66)], paint=p.thin)
    p.path([(32, 40), (46, 66)], paint=p.thin)
    p.path([(52, 22), (74, 74)])
    p.path([(62, 20), (84, 72)], paint=p.thin)
    spark(p, 80, 26, 13)
    pips(p, 32, 82, 3, 12)


def g_ironclad(p):
    # A double shield, a star: the capstone.
    shieldShape(p, 50, 52, 1.45)
    shieldShape(p, 50, 52, 0.95, False, p.thin)
    star(p, 50, 50, 12, 5)


GLYPHS = {name[2:]: fn for name, fn in globals().items() if name.startswith("g_")}


def aegisFix(p):
    # Aegis: a solid shield with the bolt knocked out of it.
    path = shieldShape(p, 50, 50, 1.45, True)
    del path
    pts = [(54, 24), (36, 54), (48, 54), (44, 78), (64, 44), (52, 44)]
    q = skia.Path()
    q.moveTo(*pts[0])
    for pt in pts[1:]:
        q.lineTo(*pt)
    q.close()
    p.c.drawPath(q, p.clear)


GLYPHS["aegis"] = aegisFix


def ringwalkerFix(p):
    # The ringed giant with its ring passing in front: the capstone.
    p.circle(50, 52, 20)
    ring(p, 50, 52, 42, 12, -18, p.clearLine)
    ring(p, 50, 52, 42, 12, -18)
    star(p, 80, 20, 11, 5)


GLYPHS["ringwalker"] = ringwalkerFix


def render(sheet: bool):
    w, h = CELL * COLS, CELL * ROWS
    surface = skia.Surface(w, h)
    canvas = surface.getCanvas()
    canvas.clear(skia.ColorTRANSPARENT)
    for row, names in enumerate(NAMES):
        for col, name in enumerate(names):
            canvas.save()
            canvas.translate(col * CELL, row * CELL)
            canvas.clipRect(skia.Rect.MakeWH(CELL, CELL))
            canvas.scale(CELL / 100, CELL / 100)
            # Each glyph on its own layer, so a knock-out clears only itself.
            canvas.saveLayer()
            GLYPHS[name](Pen(canvas))
            canvas.restore()
            canvas.restore()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    surface.makeImageSnapshot().save(str(OUT), skia.kPNG)
    print(f"wrote {OUT} ({w}x{h})")
    if sheet:
        # A dark sheet with names, to look at.
        label = 22
        s2 = skia.Surface(w, h + ROWS * label)
        c2 = s2.getCanvas()
        c2.clear(skia.Color(8, 14, 22))
        atlas = surface.makeImageSnapshot()
        font = skia.Font(skia.Typeface("Helvetica Neue"), 13)
        tint = skia.Paint(ColorFilter=skia.ColorFilters.Blend(skia.Color(140, 220, 255), skia.BlendMode.kSrcIn))
        for row, names in enumerate(NAMES):
            y0 = row * (CELL + label)
            c2.drawImageRect(atlas, skia.Rect.MakeXYWH(0, row * CELL, w, CELL), skia.Rect.MakeXYWH(0, y0, w, CELL),
                             skia.SamplingOptions(), tint)
            for col, name in enumerate(names):
                c2.drawString(name, col * CELL + 6, y0 + CELL + 15, font, skia.Paint(Color=skia.Color(160, 175, 190)))
        path = Path(sys.argv[sys.argv.index("--sheet") + 1]) if len(sys.argv) > sys.argv.index("--sheet") + 1 else OUT.with_name("skill_icons_sheet.png")
        s2.makeImageSnapshot().save(str(path), skia.kPNG)
        print(f"wrote {path}")


if __name__ == "__main__":
    render("--sheet" in sys.argv)
