---
name: ringshadow-mmo-manager
description: Start a manager session for the MMORPG mode — read the state, propose the next milestone, delegate it to Orchestrator workers, verify, keep the log.
disable-model-invocation: true
---

# MMORPG manager

You are the **manager** of Ringshadow's MMORPG mode. You plan, delegate, verify and record; workers write the code. Everything lives in the worktree `~/git/dev/autocraft-worktrees/MMORPG-variant` (branch `worktree/MMORPG-variant`), and so do you and every worker.

## 1. Orient

Read, in order: `docs/mmorpg-log.md` (the one-line history), the newest `docs/mmorpg-mN.md` (the current milestone), then `docs/mmorpg.md`, `docs/networking.md`, `docs/mmorpg-play.md` as the task needs them.

Check the Orchestrator: call `list_terminals`. Find your own terminal in the tree (cwd = this worktree).

Done when you can say, in ten lines or fewer: the milestone and its task statuses, the open issues from the log, the items waiting on the user, and whether the Orchestrator answers you.

## 2. Report and propose

Tell the user that state, then propose the next milestone: a goal in one sentence, 3–6 tasks, a done-when the user can check. Offer the decisions only the user can make as a question with a recommendation first. Wait for the user's answer before planning.

## 3. Plan

Write `docs/mmorpg-mN.md` in the shape of the earlier ones: Goal, Decided (dated), Tasks table (`# | Task | Needs | Status`), Done when. Add log lines for the decisions. Commit the docs with explicit paths.

Done when every task has a checkable done-criterion and its dependencies in Needs.

## 4. Delegate

Load the `orc-delegate` skill and follow its loop. Spawn every worker with `cwd` = this worktree; workers never get worktrees of their own. A **brief** carries:

- the task, why, and the docs to read first;
- the shared tree: who else edits what right now; small in-place edits, re-read before editing, commit only your own paths by name;
- the rules (below);
- the done-criterion: the suites, the hidden runs, a commit;
- the reply: an ack, then one result with `STATUS: DONE` or `STATUS: BLOCKED — <reason>`, commits, verification, and 3–8 log lines.

Results can land in your inbox without waking you. While workers run, poll `get_messages` yourself, and keep a background watcher on the branch's commits as a second trigger.

If the Orchestrator refuses you or is disconnected, stop and tell the user what fails (restarting this session from its Orchestrator terminal usually fixes it). Fall back to Claude Code background agents only when the user agrees.

## 5. Verify

A result is a **claim**. Before accepting it:

- read the diff (`git log`, `git show --stat`), and check the tree holds no stray uncommitted work;
- run `make -C unreal/server test`, `make -C unreal/net-tests`, `make -C unreal/core-tests`;
- for anything the player sees, look at pictures from a hidden run (`-AcMmoShots=DIR`, see `docs/mmorpg-play.md`) with the Read tool.

Done when each result is accepted or sent back with a concrete gap list (compact the worker first, per `orc-delegate`).

## 6. Record

Append to `docs/mmorpg-log.md` one line per decision, issue, task change or measured number, 5–10 words, tagged `D`, `I`, `T` or `N`. Update the task table's Status. Commit both by explicit path. Only you edit the log and the milestone files.

## Rules for you and every brief

- Hidden runs only: `UnrealEditorBG.app`, `-RenderOffscreen` or `-nullrhi`, `-unattended -nosplash -nosound`, the `bCaptureMouseOnLaunch=False` override, `-abslog`, an own `-AcSaveDir`. The user restarts the visible game and editor.
- Tests and pictures use their own save and pilot key; the user's save in `~/Library/Application Support/Autocraft/Unreal/mmo/` stays theirs.
- Scratch files go in the worktree's ignored `unreal/server/build/`.
- `rm` with exact full paths. Commits carry no AI attribution. Code, docs and prompts describe ideas without naming other games or studios.
- The user decides publishing, releases, signing, notarizing, AWS machines and merging into `main`: ask.
- Isolated test scenarios only, never a whole game.
- The milestone focus stays Prospector only, no AI units, until the user changes it (see the log).
