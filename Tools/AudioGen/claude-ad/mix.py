# /// script
# dependencies = ["numpy"]
# ///
import sys, wave, numpy as np
def read(p):
    w = wave.open(p); r = w.getframerate(); c = w.getnchannels()
    x = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, c).astype(float) / 32768
    return (np.repeat(x, 2, 1) if c == 1 else x), r
v, r = read("voice.wav"); b, rb = read("bed.wav"); assert r == rb
def rms_db(x): 
    n=int(r*.4); k=len(x)//n; l=np.sqrt((x[:k*n].mean(1).reshape(k,n)**2).mean(1)); l=np.sort(l)[k//2:]; return 20*np.log10(l.mean())
v *= 10**((-17 - rms_db(v))/20)
intro, tail = 3.5, 3.0
L = int((intro + len(v)/r + tail) * r); b = b[:L]
if len(b) < L: b = np.vstack([b, np.zeros((L-len(b), 2))])
b *= 10**((-23 - rms_db(b[:int(r*3)]) )/20)  # intro 6 dB under the voices
# voice activity envelope -> duck bed 15 dB under talk, smooth
vo = np.zeros((L, 2)); s = int(intro*r); vo[s:s+len(v)] = v
n = int(r*.02); k = L//n; act = np.sqrt((vo[:k*n].mean(1).reshape(k,n)**2).mean(1)) > 10**(-40/20)
act = np.convolve(act.astype(float), np.ones(40), "same") > 0  # hold 0.8 s
g = np.where(act, 10**(-26/20), 1.0)
# smooth gain: 300 ms
g = np.convolve(g, np.ones(15)/15, "same"); g = np.repeat(g, n); g = np.concatenate([g, np.full(L-len(g), g[-1])])
fade = np.ones(L); fl = int(r*2.5); fade[-fl:] = np.linspace(1, 0, fl)
m = vo + b * (g * fade)[:, None]
m /= max(1, np.abs(m).max() / 10**(-1/20))
w = wave.open("ad_studio.wav", "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(r)
w.writeframes((np.clip(m,-1,1)*32767).astype(np.int16).tobytes()); w.close()
print(f"{L/r:.1f}s, peak {20*np.log10(np.abs(m).max()):.1f} dBFS")
