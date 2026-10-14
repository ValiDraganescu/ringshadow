---
name: ringshadow-agent-manager
description: Running a team of developer agents on Ringshadow — a pool of warm worktrees, one per worker; which worker roles to spawn and which skills each loads; merging their branches; keeping builds and hidden runs from bogging down the Mac. Load it when you manage or spawn Orchestrator workers in this repo (the ringshadow-mmo-manager skill does).
---

# Ringshadow agent manager

Each worker builds its feature in a worktree of its own and follows the `ringshadow-dev-agent` skill. You keep the worktree pool, pick the workers, merge their branches, and watch the load. `orc-delegate` covers spawning, briefing and reviewing; this skill covers what is particular to this repo.

Why this shape (measured 2026-10-14): an incremental editor build takes 7–73 s, and UE's build tool already lets only one build run at a time on the whole Mac (its `-WaitMutex` lock is keyed on the engine's path). What bogs the Mac down is hidden runs side by side (one took 24 min instead of 4) and harness steps that wait for the game clock in real time. So builds are free to happen whenever a worker needs one, and hidden runs share one slot (`unreal/Tools/hidden_run.py`).

## 1. The worktree pool

Keep up to four worktrees and reuse them from task to task: a warm worktree builds in seconds, a new one pays a full build first (about 225 compile units, a few minutes) and 9–13 GB of disk.

- Name them `~/git/dev/autocraft-worktrees/MMORPG-wt-N` on branches `wt-N`, based on `worktree/MMORPG-variant`. Create one with the Orchestrator's `create_worktree` (base `worktree/MMORPG-variant`, hint `wt-N`): it seeds `.mcp.json` and `.claude/`, which git ignores. Use the path and branch it returns.
- A worker gets its worktree's path as `cwd` when you spawn it, and the path and branch in the brief.
- Between tasks the worktree stays; its next worker starts with `git rebase worktree/MMORPG-variant`.

Done when every worker has a worktree no other worker uses.

## 2. Pick the workers

Choose roles by the files the milestone touches, and give each worker files no other worker edits. Two to four workers at once.

| Role | Owns | Loads |
|---|---|---|
| core | `unreal/Source/AutocraftCore` (rules, sim, nav, terrain, sessions) | `ringshadow-testing` |
| net | `unreal/Source/AutocraftNet`, `unreal/net-tests` | `ringshadow-testing` |
| server | `unreal/server` (program, world) | `ringshadow-testing` |
| game | `unreal/Source/Autocraft` (HUD, pawns, camera, visuals) | `ringshadow-testing`, `ringshadow-building-visuals` for buildings |
| region | `unreal/server/regions/*` | `ringshadow-region-crafting` |
| model | models and catalog entries | `ringshadow-model-pipeline`, `ringshadow-building-visuals` |
| video | clips and posts | `ringshadow-video`, `ringshadow-x-posts` |

Every worker loads `ringshadow-dev-agent` first. A role not in the table: add its row once the work shows it.

## 3. Brief

On top of what `orc-delegate` puts in a brief: "Load `ringshadow-dev-agent` first. Your worktree is `<path>`, branch `wt-N`." Add who owns which files right now, and a done-criterion that names the cheapest check that proves it (a suite before a hidden run).

## 4. Merge

On a worker's `merge wt-N <sha>`: in the integration worktree, `git merge --ff-only <sha>`. When that fails because another branch landed first, tell the worker to rebase and send it again. After each merge, tell the other workers that `worktree/MMORPG-variant` moved, so they rebase before their next build.

Done when the integration branch holds every accepted step and no worker sits on more than one unmerged step.

## 5. Watch the load

- `cat ~/Library/Caches/Autocraft/hidden-run.holder` names the run holding the slot; a worker waiting long for it means runs are too long or too many. Look for runs that could be `--headless`, re-shoot every moment, or wait on the game clock, and give the fix to the worker that owns the harness.
- A worker that launches the editor without `hidden_run.py`, or runs `Build.sh` directly in a loop, gets the dev-agent skill again.
