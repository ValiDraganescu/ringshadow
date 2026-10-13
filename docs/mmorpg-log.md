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
- T T1 net → worker mmo-net, worktree mmo-net-b1457f45.
- T T2 server world → worker mmo-server-world, worktree mmo-server-world-8432175b.
- T T3 game-layer sim reads → one-shot research agent.
- N Workers report log lines; only manager edits this log.
- T T3 done; findings in mmorpg-m1.md.
- D Client mirror = unstepped ac::Simulation filled from snapshots.
- D Server sends pilot's fog bits; snapshots carry upgrades, levels, cargo.
- N Client crosshair is measured client-side; server must validate (M2).
