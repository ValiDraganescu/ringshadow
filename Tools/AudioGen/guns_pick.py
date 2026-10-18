# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "pyloudnorm"]
# ///
"""The minigun class's picks (docs/guns-and-practice.md §6, C2): every take
from Tools/AudioGen/guns.manifest.json is kept in art/models/guns/sounds/, the
picked ones are levelled (gain to a target loudness, never past -1 dBFS peak)
and written to unreal/Resources/Sounds/guns/<stem>_<n>.wav, numbered from 1.

Run from the repo root:
    uv run Tools/AudioGen/guns_pick.py <takes dir>
"""
import os, shutil, sys, wave
import numpy as np, pyloudnorm

# stem: (picked takes, target LUFS). Left out (2026-10-18, by their
# envelopes): rotaryfire_2 (jumps 10 dB half way, no loop), roundground_1
# (-38 dBFS peak, near silence), roundmetal_1 (-32 dBFS peak, faint),
# roundchitin_2 (a second hit 0.25 s in).
PICKS = {
    "rotaryfire": ([1], -16),
    "rotaryspinup": ([1, 2], -18),
    "rotaryspindown": ([1, 2], -18),
    "roundground": ([2, 3], -20),
    "roundmetal": ([2, 3], -20),
    "roundchitin": ([1, 3], -20),
}


def read(p):
    with wave.open(p) as w:
        ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64) / 32768.0
    return x.reshape(-1, ch), rate


def write(p, x, rate):
    with wave.open(p, "wb") as w:
        w.setnchannels(x.shape[1]); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def loudness(x, rate):
    # The impacts are under half a second: a block that fits them.
    return pyloudnorm.Meter(rate, block_size=min(0.4, 0.9 * len(x) / rate)).integrated_loudness(x)


takes = sys.argv[1]
os.makedirs("art/models/guns/sounds", exist_ok=True)
os.makedirs("unreal/Resources/Sounds/guns", exist_ok=True)
for f in sorted(os.listdir(takes)):
    if f.endswith(".wav"):
        shutil.copyfile(os.path.join(takes, f), f"art/models/guns/sounds/{f}")
for stem, (picks, target) in PICKS.items():
    for n, t in enumerate(picks, 1):
        x, rate = read(os.path.join(takes, f"{stem}_{t}.wav"))
        gain = target - loudness(x, rate)
        gain = min(gain, -1 - 20 * np.log10(np.abs(x).max()))
        y = x * 10 ** (gain / 20)
        out = f"unreal/Resources/Sounds/guns/{stem}_{n}.wav"
        write(out, y, rate)
        print(f"{out:52} take {t}  {gain:+5.1f} dB  {loudness(y, rate):6.1f} LUFS")
