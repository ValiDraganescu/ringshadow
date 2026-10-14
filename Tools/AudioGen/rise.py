# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy"]
# ///
"""Turn a sound that hits and dies away into one that rises to a hard stop:
reversed, cut where its hit was (its sharpest rise in level, now the
sharpest fall), the start faded in.

    uv run Tools/AudioGen/rise.py <in.wav> <out.wav> [fade_in_ms=300]

The starfall's streak (moments.manifest.json, sfstreak): the sound model
would not crescendo, so a roar that starts at full and fades is played
backwards, a rising roar that stops dead where the impact's boom takes
over. Channels, rate and sample width stay as they were. Deterministic.
"""
import sys
import wave

import numpy as np


def main():
    src, dst = sys.argv[1], sys.argv[2]
    fade_ms = float(sys.argv[3]) if len(sys.argv) > 3 else 300
    with wave.open(src) as w:
        rate, ch = w.getframerate(), w.getnchannels()
        x = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, ch).astype(np.float64)
    # The hit: the sharpest rise in level (dB, 50 ms windows) over the
    # whole sound. Reversed, the sound ends there; before it was the lead-in.
    k = int(0.05 * rate)
    e = 10 * np.log10(np.convolve((x ** 2).mean(axis=1), np.ones(k) / k, mode="same") + 1e-3)
    hit = int(np.argmax(e[k:] - e[:-k])) + k // 2
    x = x[::-1][: len(x) - hit]
    n = min(len(x), int(rate * fade_ms / 1000))
    x[:n] *= (0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n)))[:, None]
    # 5 ms off at the stop, so it makes no click.
    m = int(0.005 * rate)
    x[len(x) - m:] *= np.linspace(1, 0, m)[:, None]
    with wave.open(dst, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(np.clip(x, -32768, 32767).astype("<i2").tobytes())


if __name__ == "__main__":
    main()
