# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "pyloudnorm", "matplotlib"]
# ///
"""Check sound files offline, without playing them (docs/mmorpg-m7.md J1).

For each WAV: its length, channels, rate, sample peak (dBFS), integrated
loudness (LUFS, ITU-R BS.1770) and, with --loop, the seam: the jump in level
between its last and first 50 ms (a loop that clicks or swells at the wrap
shows it). With --png DIR it also draws a spectrogram per file (DIR/<name>.png)
to listen by eye.

Run:
    uv run unreal/Tools/Audio/check_sounds.py unreal/Resources/Sounds/sfx/rsdrone_1.wav ... [--loop NAME,NAME] [--png DIR]
"""
import argparse
import os
import wave

import numpy as np
import pyloudnorm


def read(path):
    with wave.open(path) as w:
        ch, rate, n, width = w.getnchannels(), w.getframerate(), w.getnframes(), w.getsampwidth()
        raw = w.readframes(n)
    if width != 2:
        raise SystemExit(f"{path}: {8 * width}-bit, want 16")
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    return x.reshape(-1, ch), rate


def db(v):
    return 20 * np.log10(max(v, 1e-9))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--loop", default="", help="names (file stems before _<n>) that loop")
    ap.add_argument("--png", help="draw spectrograms here")
    a = ap.parse_args()
    loops = set(filter(None, a.loop.split(",")))
    if a.png:
        os.makedirs(a.png, exist_ok=True)
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    print(f"{'file':<16} {'sec':>6} {'ch':>2} {'rate':>6} {'peak':>7} {'LUFS':>7} {'seam':>7}")
    for f in a.files:
        x, rate = read(f)
        stem = os.path.basename(f)[:-4]
        mono = x.mean(axis=1)
        peak = db(np.abs(x).max())
        try:
            lufs = pyloudnorm.Meter(rate).integrated_loudness(x)
        except ValueError:  # shorter than one 400 ms block
            lufs = float("nan")
        seam = ""
        if stem.split("_")[0] in loops:
            k = int(0.05 * rate)
            seam = f"{db(np.sqrt(np.mean(mono[-k:] ** 2))) - db(np.sqrt(np.mean(mono[:k] ** 2))):+.1f}"
        print(f"{stem:<16} {len(x) / rate:6.2f} {x.shape[1]:2d} {rate:6d} {peak:7.1f} {lufs:7.1f} {seam:>7}")
        if a.png:
            fig, ax = plt.subplots(figsize=(8, 3), dpi=100)
            ax.specgram(mono, NFFT=2048, Fs=rate, noverlap=1536, cmap="magma", vmin=-120)
            ax.set_ylim(0, 12000)
            ax.set_title(f"{stem}: {len(x) / rate:.1f} s, peak {peak:.1f} dBFS, {lufs:.1f} LUFS")
            ax.set_xlabel("s")
            ax.set_ylabel("Hz")
            fig.tight_layout()
            fig.savefig(os.path.join(a.png, stem + ".png"))
            plt.close(fig)


if __name__ == "__main__":
    main()
