# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "pyloudnorm"]
# ///
"""The Mule's picks (C4, docs/haul-and-ferry.md section 4): every take from
Tools/AudioGen/mule.manifest.json is kept in art/models/mule/sounds/, the picked
ones are levelled (gain to a target loudness, never past -1 dBFS peak) and
written to unreal/Resources/Sounds/sfx/<stem>_<n>.wav, numbered from 1. The
driver's lines (vmule, muledeath) went through the helmet comm and keep their
level, as the Prospector's own.

Run from the repo root:
    uv run Tools/AudioGen/mule_pick.py unreal/server/build/mule/takes
"""
import os, shutil, sys, wave
import numpy as np, pyloudnorm

# stem: (picked takes, target LUFS; None: as made). The drive loop sits under the rest (the drill's is -13).
# Left out: mulemove_3 and mulefill_1 (their peaks held them at -20 and -23 LUFS).
PICKS = {"vmule": ([1, 2, 3, 4], None), "muledeath": ([1, 2, 3], None), "mulemove": ([1, 2], -11),
         "muleengine": ([1, 2], -16), "mulegear": ([1, 2, 3], -14), "mulefill": ([2], -14)}


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
os.makedirs("art/models/mule/sounds", exist_ok=True)
for f in sorted(os.listdir(takes)):
    shutil.copyfile(os.path.join(takes, f), f"art/models/mule/sounds/{f}")
for stem, (picks, target) in PICKS.items():
    for n, t in enumerate(picks, 1):
        x, rate = read(os.path.join(takes, f"{stem}_{t}.wav"))
        gain = 0.0 if target is None else target - pyloudnorm.Meter(rate).integrated_loudness(x)
        gain = min(gain, -1 - 20 * np.log10(np.abs(x).max()))
        y = x * 10 ** (gain / 20)
        out = f"unreal/Resources/Sounds/sfx/{stem}_{n}.wav"
        write(out, y, rate)
        print(f"{out:52} take {t}  {gain:+5.1f} dB  {pyloudnorm.Meter(rate).integrated_loudness(y):6.1f} LUFS")
