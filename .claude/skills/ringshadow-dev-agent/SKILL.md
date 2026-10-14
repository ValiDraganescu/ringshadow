---
name: ringshadow-dev-agent
description: How a developer agent works on Ringshadow beside other agents — its own worktree, building with unreal/Tools/build.sh, hidden runs through unreal/Tools/hidden_run.py, merging back through the manager. Load it before your first edit, build or hidden run when a brief says you are a worker.
---

# Ringshadow developer agent

Other agents build other features at the same time, each in a worktree of its own. Your edits reach nobody's build until you merge, and theirs reach yours only when you rebase. The Mac is shared, so you check your work in the cheapest way that answers the question.

## 1. Your worktree

The brief names it, with its branch (`~/git/dev/autocraft-worktrees/MMORPG-wt-2`, branch `wt-2`). Edit, build and run only there. At the start of a task, with a clean tree: `git rebase worktree/MMORPG-variant`.

## 2. Check, cheapest first

1. **The make suites**, in seconds: `make -C unreal/core-tests`, `make -C unreal/server test`, `make -C unreal/net-tests`. They cover the rules, the server and the wire without the editor.
2. **The editor build**, only when you need the editor next (a hidden run, automation tests): `unreal/Tools/build.sh`. It runs at low priority, and builds from every worktree queue behind each other on their own. It prints the errors and `Result:`.
3. **A hidden run**, only for what the suites can't show (pictures, the real game flow): `uv run unreal/Tools/hidden_run.py --out DIR [--headless | --tests PREFIX | --python SCRIPT] -- FLAGS`. One run at a time on the whole Mac: it waits for the slot by itself. `{out}` in the flags becomes `DIR`. It prints the flow steps that failed, the pictures, the tests, and exits 0 ok, 1 failed, 2 timeout, 3 build first. A warning that `unreal/Source` is newer than the module means you skipped step 2.

```sh
R="uv run unreal/Tools/hidden_run.py"
$R --out unreal/server/build/g2 -- -AcMap=highlands-small -AcFlowTest=mmoshots -AcMmoShots={out} -AcMmoShotsG2
$R --out unreal/server/build/hud --headless -- -AcFlowTest=mmohud,mmocross
$R --out unreal/server/build/pick --tests Autocraft.Pick
$R --out unreal/server/build/mat --python unreal/Tools/Editor/make_resource_materials.py
$R --out unreal/Saved/Shots --raw -- uv run unreal/Tools/shots/compare.py top
```

`--raw` runs a tool that starts the editor itself (the shots compare, a video recording) under the same slot.

Run builds and hidden runs with `run_in_background` and take the next step while they go. Launch the editor no other way.

**Keep runs cheap.** `--headless` when you need the flow and not the pictures. Re-shoot only the moments you changed when the picture set can filter them (`-AcMmoShotsOnly=`). A harness step that waits for the game clock (nightfall, a timed event) jumps the clock instead. Look at the pictures with Read.

## 3. Merge back, small and often

1. Commit on your branch, by explicit paths, with no AI attribution.
2. `git rebase worktree/MMORPG-variant`. Resolve conflicts here, in your worktree, keeping both sides' intent. Run the suites again.
3. Send the manager `merge wt-N <sha>`. The manager fast-forwards the integration branch to it; when someone merged first, it asks you to rebase again.

Merge each finished step, not the whole task at the end: small merges rarely conflict.

## 4. Talk to the others

`list_agents` and `send_message` reach the manager and the other workers by name. Before you change a header or an interface another worker's feature uses, tell that worker. When the manager says others merged, rebase before your next build.
