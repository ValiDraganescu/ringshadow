"""Blueprint sites (docs/prospector-progression.md, "Blueprint sites"; docs/mmorpg-m7.md W2): one model per site kind,
placed by the client at every site of the region (`AAcSites`).

    UnrealEditorBG <abs>/unreal/Autocraft.uproject -run=pythonscript \
        -script=<abs>/unreal/Tools/Editor/models/sites.py -unattended -nullrhi -nosplash -nosound -abslog=<log>

Writes /Game/Models/site_<kind>/SM_<part>__<material> and the catalog entries `site_crate_blue`, `site_wreck_blue`,
`site_cache_blue`, `site_vault_blue` (category doodad: no team, no pose function; AAcSites shows and hides parts).

UE cm, the model's front (+Y) toward where the camera usually is, Z = 0 the ground at the site's point. A Prospector
is about 2 cells long; a site is reached within 2 cells of its point, so each model fits in about 3 cells.

The part names are the contract with AAcSites:
- `<name>_shut`: shown while the pilot has not opened the site (the lid on, the tarp over, the door closed);
- `<name>_open`: shown once it is opened (the lid off beside it, the tarp thrown back, the door slid apart);
- `lamp`: the blueprint's gold signal light, blinking while the site is shut, out once opened;
- `seam`: a steady gold glow while shut (the vault's lit seam), out once opened.
Everything else is the body, always shown.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from ac_modelkit import Model, box, cyl, cone, sphere, bar, prism, loft, ring_box  # noqa: E402

DRIVER = "AcSites.cpp:AAcSites::Tick"
# The blueprint's light: the HUD's level gold (the world map's site diamonds, the opening ring).
LAMP = (1.5, 0.55, 0.04)
SEAM = (1.1, 0.42, 0.03)


def materials(m):
    for name in ("hullLight", "hullMid", "hullDark", "hazard", "chrome", "soot", "rock_2", "rock_3"):
        m.material(name)
    m.material("siteLamp", parent="emissive", color=(1, 1, 1), emissive=LAMP)
    m.material("siteSeam", parent="emissive", color=(1, 1, 1), emissive=SEAM)


def lamp(m, at, r=7.0, what="the blueprint's signal light: blinks while shut, out once opened"):
    p = m.part("lamp", at=at, driver=dict(what=what, channels=["visibility"], functions=[DRIVER]))
    m.mesh(p, "siteLamp", sphere(r, at=at, steps=8), solid=False)
    return p


def shut(m, name, at=(0, 0, 0)):
    return m.part(name + "_shut", at=at, driver=dict(what="shown while the pilot has not opened the site", channels=["visibility"],
                                                     functions=[DRIVER]))


def opened(m, name, at=(0, 0, 0)):
    return m.part(name + "_open", at=at, driver=dict(what="shown once the pilot has opened the site", channels=["visibility"],
                                                     functions=[DRIVER]))


def body(m):
    return m.part("body", driver=dict(what="the site itself, always shown", channels=[], functions=[DRIVER]))


# --- crate: a dropped supply crate on open ground, a blinking beacon, a little scuffed -------------------------------
def crate():
    m = Model("site_crate", category="doodad", notes=[
        "A supply crate dropped on open ground: skids, hazard bands, a beacon mast. Lid on while shut; opened, the lid "
        "leans on its side and the dark hold shows."])
    materials(m)
    b = body(m)
    W, H = 128, 96
    # skids, then the box on them
    m.mesh(b, "hullDark", [box((W + 10, 16, 12), at=(0, y, 0), base=True) for y in (-46, 46)])
    m.mesh(b, "hullMid", box((W, W, H), at=(0, 0, 12), base=True))
    # corner posts and the rim the lid sits on
    m.mesh(b, "hullDark", [box((14, 14, H + 4), at=(sx * (W / 2 - 4), sy * (W / 2 - 4), 10), base=True)
                           for sx in (-1, 1) for sy in (-1, 1)])
    m.mesh(b, "hullDark", [box((W + 6, 8, 8), at=(0, sy * (W / 2 - 1), 12 + H - 4)) for sy in (-1, 1)] +
           [box((8, W + 6, 8), at=(sx * (W / 2 - 1), 0, 12 + H - 4)) for sx in (-1, 1)], solid=False)
    # hazard bands across the front and the sides, and the stencilled panel
    m.mesh(b, "hazard", [box((W - 20, 1.5, 12), at=(0, W / 2 + 0.6, 12 + 20)), box((1.5, W - 20, 12), at=(W / 2 + 0.6, 0, 12 + 20)),
                         box((1.5, W - 20, 12), at=(-W / 2 - 0.6, 0, 12 + 20))], solid=False)
    m.mesh(b, "hullLight", [box((54, 1.5, 34), at=(0, W / 2 + 0.6, 12 + 62)), box((1.5, 54, 34), at=(W / 2 + 0.6, 0, 12 + 62))],
           solid=False)
    # scuffs: soot where it hit the ground and scraped
    m.mesh(b, "soot", [box((40, 1.2, 16), at=(-34, W / 2 + 1.4, 18), rot=(0, 0, 12)), box((1.2, 30, 10), at=(-W / 2 - 1.4, 30, 16)),
                       box((26, 1.2, 8), at=(40, -W / 2 - 1.4, 22), rot=(0, 0, -8))], solid=False)
    # the beacon mast on the back left corner
    m.mesh(b, "hullDark", [cyl(3, 70, at=(-W / 2 + 4, -W / 2 + 4, 12 + H), base=True), cyl(7, 6, at=(-W / 2 + 4, -W / 2 + 4, 12 + H + 64),
                                                                                          base=True)])
    lamp(m, (-W / 2 + 4, -W / 2 + 4, 12 + H + 78), 8)
    # the lid, on
    lid = shut(m, "lid", (0, 0, 12 + H))
    m.mesh(lid, "hullLight", box((W + 8, W + 8, 12), at=(0, 0, 12 + H), base=True))
    m.mesh(lid, "hullDark", [box((W - 30, 8, 6), at=(0, 0, 12 + H + 12), base=True), box((8, W - 30, 6), at=(0, 0, 12 + H + 12), base=True)],
           solid=False)
    m.mesh(lid, "hazard", box((W + 9, W + 9, 3), at=(0, 0, 12 + H + 3)), solid=False)
    # opened: the hold dark, the lid leaning on the right side
    hold = opened(m, "lid", (0, 0, 12 + H))
    m.mesh(hold, "soot", box((W - 16, W - 16, 2), at=(0, 0, 12 + H - 0.4)), solid=False)
    # the lid flung down in front, to the left (-X), propped on a skid
    m.mesh(hold, "hullLight", box((W + 8, W + 8, 12), at=(-W / 2 - 52, W / 2 + 18, 6), rot=(0, 24, 7), base=True))
    m.mesh(hold, "hazard", box((W + 9, W + 9, 3), at=(-W / 2 - 52, W / 2 + 18, 15), rot=(0, 24, 7), base=True), solid=False)
    m.mesh(hold, "hullDark", box((W - 30, 8, 6), at=(-W / 2 - 52, W / 2 + 18, 19), rot=(0, 24, 7), base=True), solid=False)
    return m


# --- wreck: a crashed lander half-buried, torn panels, a cold engine --------------------------------------------------
def wreck():
    m = Model("site_wreck", category="doodad", notes=[
        "A small lander crashed nose first: hull half in the ground, torn panels, a snapped leg, a cold engine bell, a dirt "
        "berm at the nose. The side hatch is shut, or hanging open with the dark inside showing."])
    materials(m)
    b = body(m)

    # the hull along X, nose (+X) down into the ground, tail up
    def sec(x, w, h, cz):
        return ring_box(x, w, h, 0, cz, 14, 3.2)
    hull = [sec(150, 30, 26, -18), sec(110, 62, 54, 4), sec(40, 86, 72, 30), sec(-60, 90, 76, 50), sec(-140, 76, 64, 64),
            sec(-170, 60, 50, 70)]
    m.mesh(b, "hullMid", loft(hull))
    # bands and the torn skin
    for x, w, h, cz in ((40, 86, 72, 30), (-60, 90, 76, 50)):
        m.mesh(b, "hullDark", loft([sec(x - 4, w * 1.03, h * 1.03, cz), sec(x + 4, w * 1.03, h * 1.03, cz)]), solid=False)
    m.mesh(b, "hullLight", [
        prism([(0, 0), (70, 6), (64, 34), (10, 46)], 3, plane="xz", at=(-30, 78, 92), rot=(0, 0, 18)),
        prism([(0, 0), (52, -4), (40, 30)], 3, plane="xz", at=(-120, -70, 110), rot=(0, 0, -30)),
        prism([(0, 0), (46, 0), (30, 28), (6, 22)], 3, plane="xy", at=(-10, -20, 124), rot=(14, 0, 0)),
    ])
    m.mesh(b, "soot", [box((60, 2, 28), at=(70, 64, 40), rot=(0, 0, 8)), box((40, 2, 22), at=(-110, -68, 66)),
                       box((80, 50, 2), at=(-70, 0, 126.5))], solid=False)
    m.mesh(b, "hazard", [box((24, 2, 8), at=(-150, s * 66, 84)) for s in (-1, 1)], solid=False)
    # the cockpit glass, cracked dark
    m.mesh(b, "hullDark", box((30, 70, 14), at=(112, 0, 52), rot=(-30, 0, 0)), solid=False)
    # engine block and the cold bell at the tail, a stub fin
    m.mesh(b, "hullDark", [cyl(46, 26, at=(-172, 0, 72), axis="x"), cone(40, 58, 46, at=(-182, 0, 74), axis="x", rot=(0, 180, 0), base=True)])
    m.mesh(b, "soot", cone(36, 52, 2, at=(-226, 0, 74), axis="x", rot=(0, 180, 0), base=True), solid=False)
    m.mesh(b, "hullMid", prism([(0, 0), (60, 0), (40, 54), (14, 54)], 8, plane="xz", at=(-160, 0, 120)))
    # legs: one standing bent, one snapped off beside it
    m.mesh(b, "hullDark", [bar((-110, 70, 40), (-150, 130, -4), 12), bar((-100, -76, 34), (-118, -110, 10), 12),
                           box((60, 14, 12), at=(-60, -150, 0), rot=(0, 28, 0), base=True)])
    m.mesh(b, "hullMid", [cyl(22, 6, at=(-150, 130, -6)), cyl(22, 6, at=(-80, -170, 0), rot=(30, 0, 0))])
    # the berm the nose ploughed up, and spilled rock
    m.mesh(b, "rock_2", [sphere(70, at=(176, 0, -24), squash=(0.9, 1.5, 0.55), steps=10), sphere(44, at=(130, 80, -10), squash=(1.2, 0.9, 0.5), steps=8),
                         sphere(40, at=(126, -84, -12), squash=(1.1, 1, 0.5), steps=8), sphere(18, at=(210, 60, 0), steps=6),
                         sphere(14, at=(60, -110, 0), steps=6)])
    # the distress lamp on the spine
    m.mesh(b, "hullDark", cyl(4, 34, at=(-90, 0, 120), base=True))
    lamp(m, (-90, 0, 160), 8)
    # the side hatch (front, +Y): shut, or hanging open with the dark inside
    hatch = shut(m, "hatch", (-40, 90, 50))
    m.mesh(hatch, "hullLight", box((60, 6, 54), at=(-40, 90, 54)))
    m.mesh(hatch, "hazard", box((62, 7, 4), at=(-40, 90, 80)), solid=False)
    m.mesh(hatch, "chrome", box((10, 8, 4), at=(-22, 92, 54)), solid=False)
    gap = opened(m, "hatch", (-40, 90, 50))
    m.mesh(gap, "soot", box((54, 4, 48), at=(-40, 89, 54)), solid=False)
    m.mesh(gap, "hullLight", box((60, 54, 6), at=(-40, 118, 26), rot=(0, 0, 0)))
    m.mesh(gap, "hazard", box((62, 4, 7), at=(-40, 144, 27)), solid=False)
    return m


# --- cache: a buried prospector's stash under a tarp and rocks, a faint signal light ----------------------------------
def cache():
    m = Model("site_cache", category="doodad", notes=[
        "A prospector's stash: a low mound under a weighted tarp, a ring of rocks, a whip antenna with a faint light. "
        "Opened, the tarp is thrown back and the dug-out stash box shows, its lid off."])
    materials(m)
    m.material("tarp", parent="hull", color=(0.032, 0.036, 0.02), metallic=0.0, roughness=0.95)
    b = body(m)
    # the dug ring and the rocks that weigh the tarp
    m.mesh(b, "rock_2", [sphere(r, at=(math.cos(a) * 92, math.sin(a) * 74, r * 0.25), squash=(1.2, 1, 0.65), steps=8)
                         for r, a in ((26, 0.3), (20, 1.2), (30, 2.1), (18, 2.9), (24, 3.8), (22, 4.6), (28, 5.5))])
    m.mesh(b, "rock_3", [sphere(16, at=(-130, 30, 6), steps=6), sphere(12, at=(120, -60, 4), steps=6)])
    # the stash box (dug half out, always there: the tarp covers it while shut)
    m.mesh(b, "hullMid", box((84, 56, 40), at=(0, 0, -6), base=True))
    m.mesh(b, "hazard", box((86, 58, 6), at=(0, 0, 24)), solid=False)
    # the whip antenna and its faint light
    m.mesh(b, "hullDark", [cyl(5, 10, at=(70, -48, 0), base=True), cyl(1.6, 96, at=(70, -48, 10), base=True)])
    lamp(m, (70, -48, 108), 5)
    # shut: the tarp mound over the box, its edges pinned by the rocks
    tarp = shut(m, "tarp")
    m.mesh(tarp, "tarp", [sphere(80, at=(0, 0, 0), squash=(1.15, 0.92, 0.62), steps=12)])
    # two straps over the mound, pegged at the rocks
    m.mesh(tarp, "hullDark", [sphere(82, at=(x, 0, 0), squash=(0.045, 0.93, 0.63), steps=12) for x in (-34, 30)], solid=False)
    # opened: the tarp heaped to one side, the box's lid off and its hold dark
    gone = opened(m, "tarp")
    m.mesh(gone, "tarp", [sphere(46, at=(-104, -14, 6), squash=(0.7, 1.3, 0.4), steps=10), sphere(30, at=(-90, 40, 4), squash=(0.9, 0.8, 0.35), steps=8)])
    m.mesh(gone, "soot", box((76, 48, 2), at=(0, 0, 34.5)), solid=False)
    m.mesh(gone, "hullMid", box((84, 56, 6), at=(56, 50, 4), rot=(0, 24, 8), base=True))
    return m


# --- vault: a sealed armoured door set into the rock, ancient and heavy, a lit seam ------------------------------------
def vault():
    m = Model("site_vault", category="doodad", notes=[
        "An ancient vault: a heavy armoured door in a bevelled frame set into a rock outcrop, a lit gold seam round it "
        "and down its middle, two pylons with lamps. Opened, the two leaves stand slid apart into the frame and the dark "
        "passage shows; the seam and the lamps are out."])
    materials(m)
    m.material("vaultMetal", parent="hull", color=(0.045, 0.042, 0.038), metallic=0.85, roughness=0.55)
    b = body(m)
    # the outcrop: heaped boulders behind and round the door, the front (+Y) a flat face
    m.mesh(b, "rock_3", [sphere(150, at=(0, -110, 90), squash=(1.7, 0.9, 1.25), steps=12),
                         sphere(110, at=(-190, -40, 40), squash=(0.9, 1, 1), steps=10),
                         sphere(116, at=(190, -50, 46), squash=(0.95, 1, 1.05), steps=10),
                         sphere(60, at=(-250, 50, 0), squash=(1, 1, 0.8), steps=8),
                         sphere(52, at=(240, 60, 0), squash=(1, 1, 0.75), steps=8)])
    m.mesh(b, "rock_2", [sphere(90, at=(0, -60, 250), squash=(1.8, 1.1, 0.5), steps=10)])
    # the frame: a deep bevelled portal standing out of the rock
    D = 40   # depth of the frame
    m.mesh(b, "vaultMetal", [box((300, D, 34), at=(0, 20, 252)), box((300, D, 20), at=(0, 20, 0), base=True),
                             box((44, D, 260), at=(-128, 20, 0), base=True), box((44, D, 260), at=(128, 20, 0), base=True)])
    m.mesh(b, "hullDark", [box((24, D + 6, 274), at=(s * 160, 20, 0), base=True) for s in (-1, 1)] +
           [box((340, D + 6, 18), at=(0, 20, 270))])
    # rivets on the lintel, a worn glyph band
    m.mesh(b, "chrome", [sphere(4, at=(x, 42, 262), steps=5) for x in range(-130, 131, 26)], solid=False)
    m.mesh(b, "hazard", box((180, 2, 8), at=(0, 41, 238)), solid=False)
    m.mesh(b, "soot", [box((70, 2, 40), at=(-110, 41.5, 30)), box((40, 2, 70), at=(118, 41.5, 160))], solid=False)
    # two pylons beside the door with their lamps
    for s in (-1, 1):
        m.mesh(b, "vaultMetal", [box((26, 26, 150), at=(s * 196, 40, 0), base=True), box((34, 34, 10), at=(s * 196, 40, 150), base=True)])
    lp = m.part("lamp", at=(0, 40, 170), driver=dict(what="the pylons' gold lamps: blink while shut, out once opened",
                                                       channels=["visibility"], functions=[DRIVER]))
    m.mesh(lp, "siteLamp", [sphere(8, at=(s * 196, 40, 170), steps=8) for s in (-1, 1)], solid=False)
    # the passage behind the door: dark, seen only when open
    m.mesh(b, "soot", box((212, 4, 232), at=(0, -2, 20), base=True), solid=False)
    # shut: the two leaves closed, their bolts, the lit seam round and down the middle
    door = shut(m, "door", (0, 20, 136))
    for s in (-1, 1):
        m.mesh(door, "vaultMetal", box((105, 22, 232), at=(s * 53.5, 14, 20), base=True))
        m.mesh(door, "hullDark", [box((70, 3, 20), at=(s * 53.5, 26, 20 + z), base=True) for z in (40, 106, 172)], solid=False)
        m.mesh(door, "chrome", [cyl(9, 5, at=(s * 26, 27, z), axis="y") for z in (70, 136, 202)], solid=False)
    seam = m.part("seam", at=(0, 26, 136), driver=dict(what="the vault's lit seam: steady while shut, out once opened",
                                                         channels=["visibility"], functions=[DRIVER]))
    m.mesh(seam, "siteSeam", [box((3, 3, 232), at=(0, 26, 20), base=True), box((214, 3, 3), at=(0, 26, 253)),
                              box((214, 3, 3), at=(0, 26, 19)), box((3, 3, 232), at=(-107, 26, 20), base=True),
                              box((3, 3, 232), at=(107, 26, 20), base=True)], solid=False)
    # opened: the leaves slid sideways into the frame, the passage open
    gone = opened(m, "door", (0, 20, 136))
    for s in (-1, 1):
        m.mesh(gone, "vaultMetal", box((60, 22, 232), at=(s * 122, 32, 20), base=True))
        m.mesh(gone, "hullDark", [box((40, 3, 20), at=(s * 122, 44, 20 + z), base=True) for z in (40, 106, 172)], solid=False)
    return m


for make in (crate, wreck, cache, vault):
    make().write()
