# /// script
# dependencies = ["numpy"]
# ///
"""Run from the folder with part1.wav/part2.wav (afconvert of the mp3s): uv run tighten.py voice.wav 0.45 part1.wav part2.wav.
Join parts, shorten every silence longer than MAXGAP to MAXGAP (cut inside the silence), write mono 16-bit wav."""
import sys, wave, numpy as np
out, maxgap, ins = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
def read(p):
    w = wave.open(p); r = w.getframerate()
    return np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, w.getnchannels()).mean(1) / 32768, r
xs = []; 
for p in ins:
    x, r = read(p); xs += [x, np.zeros(int(r * maxgap))]
x = np.concatenate(xs)
n = int(r * .02); k = len(x)//n; lv = 20*np.log10(np.sqrt((x[:k*n].reshape(k,n)**2).mean(1))+1e-9)
quiet = lv < -42; keep = np.ones(len(x), bool); run = 0; mg = int(maxgap/.02)
segs = []; s = None
for i, q in enumerate(list(quiet) + [False]):
    if q and s is None: s = i
    if not q and s is not None:
        if i - s > mg:
            a = (s + mg//2) * n; b = (i - mg//2) * n; keep[a:b] = False
        s = None
y = x[keep]
# trim lead silence
lead = np.argmax(np.abs(y) > 10**(-42/20)); y = y[max(0, lead - int(r*.1)):]
w = wave.open(out, "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(r)
w.writeframes((np.clip(y, -1, 1) * 32767).astype(np.int16).tobytes()); w.close()
print(f"{len(x)/r:.1f}s -> {len(y)/r:.1f}s")
