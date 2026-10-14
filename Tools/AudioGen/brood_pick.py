# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "pyloudnorm"]
# ///
"""The Brood's picks (docs/creatures.md, "Sounds"): every take from
Tools/AudioGen/brood.manifest.json is kept in art/models/<kind>/sounds/, the
picked ones are levelled (gain to a target loudness, never past -1 dBFS peak)
and written to unreal/Resources/Sounds/sfx/<stem>_<n>.wav, numbered from 1.

Run from the repo root:
    uv run Tools/AudioGen/brood_pick.py unreal/server/build/n1/takes
"""
import os, shutil, sys, wave
import numpy as np, pyloudnorm

# stem: (picked takes, target LUFS). Left out: lodebackhurt_1/_3 (near silent),
# gloamwingspit_2 (-29 LUFS, smeared), matriarchidle_1 (+4.6 dB loop seam).
PICKS = {
    "shardlingidle": ([2, 3], -22), "shardlingbite": ([1, 2, 3], -14),
    "shardlinghurt": ([1, 2, 3], -15), "shardlingdeath": ([1, 2, 3], -15),
    "gloamwingidle": ([1, 2], -20), "gloamwingspit": ([1, 3], -14),
    "gloamwinghurt": ([1, 2, 3], -15), "gloamwingdeath": ([1, 2, 3], -15),
    "lodebackidle": ([1, 2, 3], -22), "lodebackchew": ([1, 2], -22),
    "lodebackbite": ([1, 2, 3], -12), "lodebackhurt": ([2], -15),
    "lodebackdeath": ([1, 2, 3], -14),
    "matriarchidle": ([2], -20), "matriarchroar": ([1, 2, 3], -9),
    "matriarchslam": ([1, 2, 3], -12), "matriarchspit": ([1, 2], -14),
    "matriarchhurt": ([1, 2, 3], -14), "matriarchdeath": ([1, 2], -10),
    "broodswarm": ([1, 3], -18),
}
KIND = {"broodswarm": "shardling"}


def read(p):
    with wave.open(p) as w:
        ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64) / 32768.0
    return x.reshape(-1, ch), rate


def write(p, x, rate):
    with wave.open(p, "wb") as w:
        w.setnchannels(x.shape[1]); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


takes = sys.argv[1]
for f in sorted(os.listdir(takes)):
    stem = f.rsplit("_", 1)[0]
    kind = KIND.get(stem) or next(k for k in ("shardling", "gloamwing", "lodeback", "matriarch") if stem.startswith(k))
    os.makedirs(f"art/models/{kind}/sounds", exist_ok=True)
    shutil.copyfile(os.path.join(takes, f), f"art/models/{kind}/sounds/{f}")
for stem, (picks, target) in PICKS.items():
    for n, t in enumerate(picks, 1):
        x, rate = read(os.path.join(takes, f"{stem}_{t}.wav"))
        gain = target - pyloudnorm.Meter(rate).integrated_loudness(x)
        gain = min(gain, -1 - 20 * np.log10(np.abs(x).max()))
        y = x * 10 ** (gain / 20)
        out = f"unreal/Resources/Sounds/sfx/{stem}_{n}.wav"
        write(out, y, rate)
        print(f"{out:52} take {t}  {gain:+5.1f} dB  {pyloudnorm.Meter(rate).integrated_loudness(y):6.1f} LUFS")
