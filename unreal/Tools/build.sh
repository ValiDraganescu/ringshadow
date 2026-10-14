#!/bin/sh
# Builds this worktree's editor module at low priority.
#
#   unreal/Tools/build.sh
#
# UE's build tool keys its -WaitMutex lock on the engine's path, so builds from
# every worktree on this Mac queue behind each other: one compiles at a time.
# nice keeps the user's own apps responsive while it does. The full output goes
# to unreal/Saved/Logs/build.log; this prints the errors and the result.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LOG="$ROOT/unreal/Saved/Logs/build.log"
mkdir -p "$(dirname "$LOG")"
nice -n 10 "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" \
  AutocraftEditor Mac Development -Project="$ROOT/unreal/Autocraft.uproject" -WaitMutex >"$LOG" 2>&1
code=$?
grep -E ": (fatal )?error:|Result: |Total execution time|Waiting for" "$LOG" | head -40
[ $code -ne 0 ] && echo "build failed (exit $code); full log: $LOG"
exit $code
