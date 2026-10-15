# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""Cuts a showreel from recorded clips, offline: an edit list in, one MP4 out.

The edit list (JSON) names the shots in order, each a piece of a clip
(`raw2mp4.py` makes the clips from hidden recordings), with an optional slow
push-in and a caption; the shots cross-fade into each other; an end card
closes it. The sound is mixed here, never played: a music bed from the
game's own tracks (unreal/Resources/Sounds/music, docs/music.md) and the
game's sounds (unreal/Content-src/sounds/sfx) at times on the reel's clock,
the whole at -16 LUFS. The game runs that made the clips were silent.

    uv run unreal/Tools/showreel/reel.py unreal/server/build/v3/edit.json

The edit list:

    {
      "out": "unreal/server/build/v3/ringshadow-showreel.mp4",
      "fade": 0.6,                               cross-fade between shots, s
      "music": {"track": "ashfall_reach_1", "start": 12, "volume": 0.8},
      "shots": [
        {"src": "runs/home/home.mp4", "in": 0, "out": 6,
         "zoom": [1.0, 1.12], "focus": [0.3, 0.4],  push-in toward a point (0..1)
         "focus_to": [0.6, 0.4],                  and a pan to another (eased)
         "rate": 1.0,                             playback speed
         "kicker": "01", "caption": "The home screen",
         "subtitle": "A line spoken", "subtitle_at": [0.5, 4.0],  a subtitle, bottom middle
         "sfx": [{"file": "jetup_1", "at": 0.9}]}  a sound on the shot's clock
      ],
      "card": {"seconds": 4.5, "line": "A lone prospector under a ringed giant",
               "image": "key-art.jpg", "url": "ringshadow.com"},
      "sfx": [{"file": "rsrelease_1", "at": 4.2, "gain": 0.5}]
    }

Paths are relative to the edit list. Writes the MP4, OUT-contact.jpg (one
frame a shot) and OUT-stills/ (a PNG from the middle of each shot). The card
holds over the last shot's last frame, or over `image` (the key art) with a
slow push-in; `url` goes under the line. `"web": {"mb": 14}` also writes
OUT-web.mp4, two-pass H.264 sized under that many MB.
"""
import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = Path(__file__).resolve().parents[3]
FONTS = REPO / "unreal/Content-src/fonts"
MUSIC = REPO / "unreal/Resources/Sounds/music"
SFX = REPO / "unreal/Content-src/sounds/sfx"
W, H, FPS = 1920, 1080, 30
ICE, CYAN, SOFT = (235, 247, 255, 255), (89, 217, 255, 255), (140, 217, 242, 255)


def tool(name: str) -> str:
    """FFmpeg from the video workspace, the PATH, or nixpkgs."""
    local = REPO / "video/.tools/ffmpeg-bin/bin" / name
    if local.exists():
        return str(local)
    if found := shutil.which(name):
        return found
    out = subprocess.run(["nix", "--extra-experimental-features", "nix-command flakes", "build", "nixpkgs#ffmpeg.bin",
                          "--no-link", "--print-out-paths"], capture_output=True, text=True, check=True).stdout.strip()
    return f"{out}/bin/{name}"


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def duration(path: Path) -> float:
    out = subprocess.run([tool("ffprobe"), "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
                         capture_output=True, text=True, check=True).stdout
    return float(out.strip())


def font(name: str, size: int, variation: bytes | None = None) -> ImageFont.FreeTypeFont:
    f = ImageFont.truetype(str(FONTS / name), size)
    if variation:
        f.set_variation_by_name(variation)
    return f


def shadowed(img: Image.Image, xy: tuple[float, float], s: str, f: ImageFont.FreeTypeFont, fill, spacing: float = 0) -> float:
    """Text with a soft drop shadow; `spacing` tracks the letters. Returns its width."""
    sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ds, d = ImageDraw.Draw(sh), ImageDraw.Draw(img)
    x = xy[0]
    for ch in s if spacing else [s]:
        ds.text((x + 3, xy[1] + 4), ch, font=f, fill=(0, 0, 0, 200))
        x += d.textlength(ch, font=f) + spacing
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(5)))
    x = xy[0]
    for ch in s if spacing else [s]:
        d.text((x, xy[1]), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + spacing
    return x - xy[0] - (spacing if spacing else 0)


def caption(kicker: str, title: str) -> Image.Image:
    """The shot's caption at the left above the console: a cyan kicker and hairline, the title in ice, over a soft scrim."""
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g = Image.new("L", (W, H), 0)
    px = g.load()
    # Above the pilot's console (its top is about 790 px down): the game's own HUD stays clear.
    top, bottom = int(H * 0.46), int(H * 0.73)
    for y in range(top, bottom):
        v = min(1.0, (y - top) / ((bottom - top) * 0.7)) * min(1.0, (bottom - y) / 24)
        for x in range(0, int(W * 0.55)):
            px[x, y] = int(255 * 0.55 * v * (1 - x / (W * 0.55)) ** 0.9)
    scrim = Image.new("RGBA", (W, H), (3, 9, 18, 0))
    scrim.putalpha(g)
    img.alpha_composite(scrim)
    x0, base = 96, H - 318
    kf, tf = font("BarlowCondensed-Bold.ttf", 30), font("BarlowCondensed-SemiBold.ttf", 54)
    kw = shadowed(img, (x0, base - 112), kicker.upper(), kf, CYAN, spacing=5)
    ImageDraw.Draw(img).line([(x0 + kw + 18, base - 92), (x0 + kw + 18 + 120, base - 92)], fill=(89, 217, 255, 150), width=2)
    shadowed(img, (x0, base - 72), title, tf, ICE, spacing=1.2)
    return img


def card(line: str, url: str = "", dim: int = 150) -> Image.Image:
    """The end card: RINGSHADOW in the wordmark's face, shaded white to ice, a cyan glow, the line (and url) under it."""
    img = Image.new("RGBA", (W, H), (2, 6, 12, dim))
    mf = font("Exo2-ExtraBold.ttf", 168)
    word, spacing = "RINGSHADOW", 0.1 * 168
    d = ImageDraw.Draw(img)
    width = sum(d.textlength(c, font=mf) for c in word) + spacing * (len(word) - 1)
    x0, y0 = (W - width) / 2, H / 2 - 150
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    x = x0
    for c in word:
        md.text((x, y0), c, font=mf, fill=255)
        x += md.textlength(c, font=mf) + spacing
    glow = Image.new("RGBA", (W, H), (89, 217, 255, 0))
    glow.putalpha(mask.filter(ImageFilter.GaussianBlur(26)).point(lambda v: int(v * 0.55)))
    img.alpha_composite(glow)
    l, t, r, b = mask.getbbox()
    shade = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sp = shade.load()
    for y in range(t, b):
        v = (y - t) / max(1, b - t)
        c0, c1, k = ((255, 255, 255), (204, 242, 255), v / 0.45) if v < 0.45 else ((204, 242, 255), (140, 217, 242), (v - 0.45) / 0.55)
        col = tuple(int(c0[i] + (c1[i] - c0[i]) * k) for i in range(3)) + (255,)
        for xx in range(l, r):
            sp[xx, y] = col
    shade.putalpha(mask)
    img.alpha_composite(shade)
    lf = font("BarlowCondensed-SemiBold.ttf", 52)
    lw = sum(d.textlength(c, font=lf) for c in line) + 2.2 * (len(line) - 1)
    shadowed(img, ((W - lw) / 2, b + 46), line, lf, ICE, spacing=2.2)
    ImageDraw.Draw(img).line([((W - 220) / 2, b + 30), ((W + 220) / 2, b + 30)], fill=(89, 217, 255, 170), width=2)
    if url:
        uf = font("BarlowCondensed-Bold.ttf", 40)
        uw = sum(d.textlength(c, font=uf) for c in url) + 4 * (len(url) - 1)
        shadowed(img, ((W - uw) / 2, b + 124), url, uf, CYAN, spacing=4)
    return img


def subtitle(text: str) -> Image.Image:
    """A spoken line at the bottom middle, ice on a dark band, as subtitles sit."""
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    f = font("BarlowCondensed-SemiBold.ttf", 46)
    d = ImageDraw.Draw(img)
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = f"{cur} {w}".strip()
        if d.textlength(t, font=f) > W * 0.62 and cur:
            lines.append(cur)
            cur = w
        else:
            cur = t
    lines.append(cur)
    y = H - 120 - 58 * len(lines)
    for ln in lines:
        lw = d.textlength(ln, font=f)
        d.rounded_rectangle([(W - lw) / 2 - 18, y - 4, (W + lw) / 2 + 18, y + 54], 6, fill=(3, 9, 18, 170))
        shadowed(img, ((W - lw) / 2, y), ln, f, ICE)
        y += 58
    return img


def shot_video(s: dict, base: Path, tmp: Path, i: int) -> tuple[Path, float]:
    """One shot: the piece of its clip at its rate, the push-in, the caption fading in and out."""
    src = (base / s["src"]).resolve()
    a, b, rate = float(s.get("in", 0)), float(s["out"]), float(s.get("rate", 1.0))
    length = (b - a) / rate
    z0, z1 = s.get("zoom", [1.0, 1.0])
    fx, fy = s.get("focus", [0.5, 0.5])
    tx, ty = s.get("focus_to", [fx, fy])
    # The push-in: scale up per frame, then a 1920x1080 window kept around the
    # focus point, which eases to focus_to (a pan) on the same curve.
    e = f"(0.5-0.5*cos(PI*min(t/{length:.4f},1)))"
    z = f"({z0}+({z1}-{z0})*{e})"
    vf = [f"trim={a:.4f}:{b:.4f}", "setpts=PTS-STARTPTS", f"setpts=PTS/{rate}", f"fps={FPS}",
          f"scale=w='2*trunc({W}*{z}/2)':h='2*trunc({H}*{z}/2)':eval=frame:flags=bicubic",
          f"crop={W}:{H}:x='(iw-{W})*({fx}+({tx}-{fx})*{e})':y='(ih-{H})*({fy}+({ty}-{fy})*{e})'"]
    out = tmp / f"shot{i:02d}.mp4"
    cmd = [tool("ffmpeg"), "-v", "error", "-y", "-i", str(src)]
    # Overlays (the caption, a subtitle), each a still faded in and out.
    overlays = []
    if s.get("caption"):
        png = tmp / f"cap{i:02d}.png"
        caption(s.get("kicker", ""), s["caption"]).save(png)
        overlays.append((png, 0.5, max(1.0, length - 0.9)))
    if s.get("subtitle"):
        png = tmp / f"sub{i:02d}.png"
        subtitle(s["subtitle"]).save(png)
        sa, sb = s.get("subtitle_at", [0.4, length - 0.4])
        overlays.append((png, float(sa), float(sb)))
    filt = f"[0:v]{','.join(vf)}[s0]"
    for k, (png, cin, cout) in enumerate(overlays):
        cmd += ["-loop", "1", "-framerate", str(FPS), "-i", str(png)]
        filt += (f";[{k + 1}:v]format=rgba,fade=in:st={cin}:d=0.4:alpha=1,"
                 f"fade=out:st={cout - 0.4:.3f}:d=0.4:alpha=1[c{k}];[s{k}][c{k}]overlay=shortest=1[s{k + 1}]")
    filt += f";[s{len(overlays)}]format=yuv420p[v]"
    cmd += ["-filter_complex", filt, "-map", "[v]", "-t", f"{length:.4f}", "-an",
            "-c:v", "libx264", "-crf", "12", "-preset", "fast", str(out)]
    run(cmd)
    return out, duration(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("edit", type=Path)
    a = ap.parse_args()
    edit = json.loads(a.edit.read_text())
    base = a.edit.resolve().parent
    out = (base / edit["out"]).resolve()
    fade = float(edit.get("fade", 0.6))
    with tempfile.TemporaryDirectory(dir=base) as t:
        tmp = Path(t)
        pieces = [shot_video(s, base, tmp, i) for i, s in enumerate(edit["shots"])]
        # The end card over the last shot's last frame, held.
        c = edit.get("card")
        if c:
            png = tmp / "card.png"
            card(c["line"], c.get("url", ""), 110 if c.get("image") else 150).save(png)
            last, _ = pieces[-1]
            held = tmp / "card.mp4"
            secs = float(c["seconds"])
            if c.get("image"):
                # The key art, filling the frame, pushed in slowly under the card.
                art = (base / c["image"]).resolve()
                bg = (f"[0:v]scale={W * 2}:{H * 2}:force_original_aspect_ratio=increase,crop={W * 2}:{H * 2},"
                      f"zoompan=z='1+0.06*on/({secs}*{FPS})':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':d=1:s={W}x{H}:fps={FPS},"
                      f"setpts=N/{FPS}/TB[b]")
                src_in = ["-loop", "1", "-framerate", str(FPS), "-i", str(art)]
            else:
                bg = f"[0:v]trim=end_frame=1,loop=loop=-1:size=1,setpts=N/{FPS}/TB,eq=brightness=-0.04[b]"
                src_in = ["-sseof", "-0.1", "-i", str(last)]
            run([tool("ffmpeg"), "-v", "error", "-y", *src_in, "-loop", "1", "-framerate", str(FPS),
                 "-i", str(png), "-filter_complex",
                 f"{bg};[1:v]format=rgba,fade=in:st=0.4:d=1.0:alpha=1[c];[b][c]overlay,fade=out:st={secs - 0.8}:d=0.8,format=yuv420p[v]",
                 "-map", "[v]", "-t", str(secs), "-c:v", "libx264", "-crf", "12", "-preset", "fast", str(held)])
            pieces.append((held, secs))
        # Cross-fade the pieces in order.
        cmd = [tool("ffmpeg"), "-v", "error", "-y"]
        for p, _ in pieces:
            cmd += ["-i", str(p)]
        chain, label, at = [], "[0:v]", 0.0
        starts = [0.0]
        for k in range(1, len(pieces)):
            at += pieces[k - 1][1] - fade
            starts.append(at)
            nxt = f"[x{k}]"
            chain.append(f"{label}[{k}:v]xfade=transition=fade:duration={fade}:offset={at:.4f}{nxt}")
            label = nxt
        total = at + pieces[-1][1]
        video = tmp / "video.mp4"
        cmd += ["-filter_complex", ";".join(chain) if chain else "[0:v]null[x0]", "-map", label if chain else "[x0]",
                "-c:v", "libx264", "-crf", "16", "-preset", "slow", "-pix_fmt", "yuv420p", str(video)]
        run(cmd)
        # The sound: the music bed and the game's sounds, mixed offline, -16 LUFS.
        m = edit.get("music")
        acmd = [tool("ffmpeg"), "-v", "error", "-y"]
        parts, n = [], 0
        if m:
            acmd += ["-ss", str(m.get("start", 0)), "-t", f"{total:.3f}", "-i", str(MUSIC / f"{m['track']}.mp3")]
            parts.append(f"[{n}:a]aformat=sample_rates=48000:channel_layouts=stereo,volume={m.get('volume', 0.8)},"
                         f"afade=in:d={m.get('fade_in', 1.5)},afade=out:st={total - m.get('fade_out', 3.0):.3f}:d={m.get('fade_out', 3.0)}[a{n}]")
            n += 1
        # A shot's own sounds sit on its clock: `at` from the shot's start.
        cues = list(edit.get("sfx", []))
        for k, sh in enumerate(edit["shots"]):
            cues += [dict(fx, at=starts[k] + float(fx["at"])) for fx in sh.get("sfx", [])]
        for fx in cues:
            acmd += ["-i", str(SFX / f"{fx['file']}.wav")]
            ms = int(float(fx["at"]) * 1000)
            parts.append(f"[{n}:a]aformat=sample_rates=48000:channel_layouts=stereo,volume={fx.get('gain', 0.5)},adelay={ms}|{ms}[a{n}]")
            n += 1
        audio = None
        if n:
            mix = "".join(f"[a{k}]" for k in range(n))
            parts.append(f"{mix}amix=inputs={n}:normalize=0:duration=longest,atrim=0:{total:.3f},loudnorm=I=-16:TP=-1.5:LRA=11[mix]")
            audio = tmp / "audio.m4a"
            run(acmd + ["-filter_complex", ";".join(parts), "-map", "[mix]", "-ar", "48000", "-c:a", "aac", "-b:a", "256k", str(audio)])
        out.parent.mkdir(parents=True, exist_ok=True)
        mux = [tool("ffmpeg"), "-v", "error", "-y", "-i", str(video)]
        if audio:
            mux += ["-i", str(audio), "-map", "0:v", "-map", "1:a", "-c:a", "copy", "-shortest"]
        run(mux + ["-c:v", "copy", "-movflags", "+faststart", str(out)])
        # A frame from the middle of each shot: the contact sheet and the stills.
        stills = out.parent / f"{out.stem}-stills"
        stills.mkdir(exist_ok=True)
        mids = [starts[k] + pieces[k][1] / 2 for k in range(len(pieces))]
        for k, tt in enumerate(mids):
            run([tool("ffmpeg"), "-v", "error", "-y", "-ss", f"{tt:.3f}", "-i", str(out), "-frames:v", "1", str(stills / f"{k + 1:02d}.png")])
        cols = 4
        rows = (len(mids) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * 480, rows * 270), (3, 9, 18))
        for k in range(len(mids)):
            im = Image.open(stills / f"{k + 1:02d}.png").convert("RGB").resize((480, 270))
            sheet.paste(im, ((k % cols) * 480, (k // cols) * 270))
        sheet.save(out.with_name(f"{out.stem}-contact.jpg"), quality=88)
        print(f"{out}: {duration(out):.2f} s, {out.stat().st_size / 1e6:.1f} MB, {len(pieces)} pieces")
        if wb := edit.get("web"):
            # Two-pass H.264 under the size asked, the sound at 128k.
            web = out.with_name(f"{out.stem}-web.mp4")
            kbps = int(float(wb.get("mb", 14)) * 8e3 / duration(out)) - 128
            log = tmp / "pass"
            enc = [tool("ffmpeg"), "-v", "error", "-y", "-i", str(out), "-c:v", "libx264", "-preset", "slow",
                   "-b:v", f"{kbps}k", "-pix_fmt", "yuv420p", "-passlogfile", str(log)]
            run(enc + ["-pass", "1", "-an", "-f", "mp4", "/dev/null"])
            run(enc + ["-pass", "2", "-c:a", "aac", "-b:a", "128k", "-movflags", "+faststart", str(web)])
            print(f"{web}: {web.stat().st_size / 1e6:.1f} MB")
        for k, (p, d) in enumerate(pieces):
            print(f"  {k + 1:02d} at {starts[k]:6.2f} s, {d:5.2f} s")


if __name__ == "__main__":
    main()
