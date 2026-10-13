# MMORPG log

One line each. D decision, I issue, T task, N note. Newest last.
Design: [mmorpg.md](mmorpg.md), [networking.md](networking.md). Plan: [mmorpg-m1.md](mmorpg-m1.md).

## 2026-10-13

- D Player is one pilot; AI pilots replace AI commanders.
- D Goal thousands per world; players build cities together; PvP over no-man's land.
- D World map → regions → buildable lots.
- D Single player first on local server; releases continue.
- D Local server = production server program; only host changes.
- D Own networking on UDP, authoritative, not Unreal replication, not lockstep.
- D Transport written by us; encryption via a vetted library (later).
- D Server keeps 60 Hz step; snapshots 20 Hz to start.
- N M1 = drive one unit through local server under lag sim.
- D M1 excludes shots, encryption, saves, Win/Linux server.
- T M1 split into T1–T6, see mmorpg-m1.md.
