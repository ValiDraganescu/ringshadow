# /// script
# dependencies = ["numpy","soundfile","scipy"]
# ///
"""Soften a rain bed: a 2nd-order low-pass at CUT Hz (filtered round the loop,
so the seam stays seamless), peak at -3 dBFS.

    uv run Tools/AudioGen/soften.py <in.wav> <out.wav> <cut_hz>

The weather beds: raw/rain_1.mp3 and raw/downpour_1.mp3 (decoded to WAV),
cut 7000, into unreal/Resources/Sounds/sfx/.
"""
import sys, numpy as np, soundfile as sf
from scipy.signal import butter, sosfiltfilt
src, dst, cut = sys.argv[1], sys.argv[2], float(sys.argv[3])
x, sr = sf.read(src)
sos = butter(2, cut, 'low', fs=sr, output='sos')
pad = sr  # wrap a second each side: the loop's end runs into its start
y = sosfiltfilt(sos, np.concatenate([x[-pad:], x, x[:pad]]), axis=0)[pad:-pad]
y *= 10 ** (-3 / 20) / np.abs(y).max()
sf.write(dst, y, sr, subtype='PCM_16')
