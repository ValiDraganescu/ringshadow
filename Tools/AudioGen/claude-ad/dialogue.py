# /// script
# requires-python = ">=3.11"
# dependencies = ["requests"]
# ///
"""Render each part of a two-voice script with ElevenLabs text-to-dialogue (eleven_v3)."""
import json, os, sys, pathlib, requests
script, outdir, envfile = map(pathlib.Path, sys.argv[1:4])
key = next(l.split("=", 1)[1].strip().strip('"') for l in envfile.read_text().splitlines() if l.startswith("ELEVENLABS_API_KEY="))
d = json.loads(script.read_text())
for i, part in enumerate(d["parts"]):
    out = outdir / f"part{i+1}.mp3"
    if out.exists():
        print("skip", out); continue
    body = {"model_id": "eleven_v3", "inputs": [{"text": t, "voice_id": d["voices"][who]} for who, t in part]}
    r = requests.post("https://api.elevenlabs.io/v1/text-to-dialogue?output_format=mp3_44100_128",
                      headers={"xi-api-key": key}, json=body, timeout=600)
    if r.status_code != 200:
        print("HTTP", r.status_code, r.text[:500]); sys.exit(1)
    out.write_bytes(r.content); print("wrote", out, len(r.content))
