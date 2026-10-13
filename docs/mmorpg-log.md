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
- T T2 done, merged: unreal/server, `make -C unreal/server test` 13 pass.
- N Region API: ac::server::Region in unreal/server/world/World.h.
- D Snapshot entities are ac::Unit/ac::Structure; enemy plans scrubbed.
- D Remembered enemy buildings sent, flagged `remembered`.
- D Hostile players: only upgrades and team sent.
- D Server event gate stricter: Missed, Landed fog-gated too.
- I Core limit: one sim.pilot, Pilot::player=0 → one driver, side 0.
- I Not yet: PathAhead threads (≥300 units), Intel.units memory.
- N Bench Badlands Large 1480 things: step 1.68 ms mean, p95 5.1, max 46.
- N Snapshot build 0.33 ms mean at 256 entities.
