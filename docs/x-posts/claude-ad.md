---
status: posted
date: 2026-10-12
angle: build
video: video/renders/claude-ad-x.mp4
posted: https://x.com/escu__valentin/status/2109594076634890705
xdraft: xd_cc67fc82
---

## Post

Ringshadow is a real-time strategy game whose code Claude Code writes, and its in-game radio station, KSTR 88.7 Stardust, just ran its first real ad: two morning-show hosts argue about whether Claude counts as a person, then walk through how Claude built the Peregrine, the Atlas and the Scorpion.

Anthropic didn't ask for it and nobody paid for it. It's a test of how a real ad sits between the songs, and it plays twice as often as the joke ads (Quicksilver Cola: not a beverage).

Claude wrote the script, ElevenLabs voiced both hosts, and every shot is the game running with the ad on its radio. The Ranger sheet that came back 8% too tall really happened.

## Reply

KSTR 88.7 Stardust, its ads and the prompts behind the songs: github.com/ValiDraganescu/ringshadow/blob/main/docs/music.md

## Notes

The developer asked for this post and allowed posting it directly (2026-10-12, 01:15 EEST, so 2026-10-11 18:15 US Eastern; that day had no posts). The ad is unofficial: Anthropic did not ask for it or pay for it.

Video (3:36, 1280x720, 9.5 MB for the X upload; the full render is video/renders/claude-ad.mp4, 1080p): fourteen sims in .claude/skills/ringshadow-video/sims.json (`claude-ad-a` to `claude-ad-p`), each with `-AcMusicAd=claude -AcMusicAdAt=S`, so the game's own radio plays the ad from the second that shot covers; cut to the ad's time by cross-correlating each clip's sound with unreal/Resources/Sounds/ads/claude.wav. 129 to 164 s are stills of the model pipeline (the Atlas concepts, blockout, parts, atlas.py, clay, painting; the Ranger's compare sheet). The clean ad file is laid over the whole video with the clips' game sound at 0.3 (0.15 under "Claude, by Anthropic"), because the units' voice lines covered the hosts. Subtitles burned in (X autoplays muted), timed by Whisper word timestamps. Opening and end cards from giantcut.py. The compositor was a one-off script in the session scratchpad.

Posted 2026-10-12 at 10:36 UTC from Orchestrator draft xd_cc67fc82 (thread: the post and the reply). The posted video has an echo under the gameplay: the clips' game sound held the in-game radio's own copy of the ad, and the cut snaps each clip to a frame, so that copy sat 33 or 67 ms behind the clean file (correlation peaks at 0.4 of the main one; none on the stills). Fixed after posting: `unad.py` subtracts the radio's copy from each clip's sound with a least-squares filter at its lag (30 to 51 dB down), and the compositor takes the clips' sound from those stems. The fixed renders replace video/renders/claude-ad.mp4 and claude-ad-x.mp4 (9.6 MB, -16.1 LUFS); the scripts are kept in video/renders/claude-ad-work/scripts/. Any later cut of a clip whose sound has the radio on it, under the radio's clean file, needs the same.
