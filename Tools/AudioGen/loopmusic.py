# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy"]
# ///
"""Make a music take into a seamless loop, keeping its channels: drop its
first `head_s` and its last `tail_s` (a model's intro swell and its ending),
then lay the last `xfade_s` of what is left over its first `xfade_s` with an
equal-power cross-fade, so the end runs into the start with no click, no
dip and no fade (the siege layers, docs/music.md "Siege layers").

    uv run Tools/AudioGen/loopmusic.py <in.wav> <out.wav> [head_s=0.5] [tail_s=3] [xfade_s=2]

16-bit WAV in and out, the input's rate and channels. Deterministic.
"""
import sys
import wave

import numpy as np


def main():
    src, dst = sys.argv[1], sys.argv[2]
    head = float(sys.argv[3]) if len(sys.argv) > 3 else 0.5
    tail = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0
    xfade = float(sys.argv[5]) if len(sys.argv) > 5 else 2.0
    with wave.open(src) as w:
        rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        assert width == 2, "16-bit WAV only"
        x = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, ch).astype(np.float64)
    a, b, n = int(rate * head), int(rate * tail), int(rate * xfade)
    x = x[a: len(x) - b]
    assert len(x) > 2 * n, "too short for that cut and cross-fade"
    t = np.linspace(0, np.pi / 2, n)[:, None]
    start = x[:n] * np.sin(t) + x[-n:] * np.cos(t)
    y = np.concatenate([start, x[n: len(x) - n]])
    y = np.clip(y, -32768, 32767).astype(np.int16)
    with wave.open(dst, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(y.tobytes())
    print(f"{dst}: {len(y) / rate:.2f} s, {ch} channels, {rate} Hz")


if __name__ == "__main__":
    main()
