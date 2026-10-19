#!/usr/bin/env bash
# The Mule's review renders (models/mule.py) from the model row, hidden, one run a picture:
#   art/models/mule/shots.sh [name ...]      (no names: all of them)
# Writes art/models/mule/previews/<name>.png. A load is the readouts' numbers (the parts' emission), the heap's and
# the sight glasses' scale, as PoseMule sets them.
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$PWD/art/models/mule/previews"
RUNS="$PWD/unreal/Saved/MuleShots"
mkdir -p "$OUT" "$RUNS"

R() { uv run unreal/Tools/hidden_run.py "$@"; }

# load ORE MH: the flags for a Mule holding ORE ore and MH MH (capacity 1000)
load() {
  local ore=$1 mh=$2 room heap bar glass
  room=$(( 1000 - ore - mh )); (( room < 0 )) && room=0
  bar=$(( (room + 99) / 100 ))
  heap=$(python3 -c "print(max(0.08, min(1.0, ($ore + $mh) / 1000)))")
  glass=$(python3 -c "print(max(0.02, min(1.0, $mh / 1000)))")
  local e="screens:1;headlamps:1"
  for k in 0 1; do e="$e;roomDigits_$k:$room;roomBar_$k:$bar;oreDigits_$k:$ore;mhDigits_$k:$mh"; done
  local s="heap:1,1,$heap;mhLevel_0:1,1,$glass;mhLevel_1:1,1,$glass"
  local show=""
  (( ore + mh > 0 )) && show="-AcModelShow=heap"
  echo "-AcModelEmission=$e" "-AcModelScale=$s" $show
}

shot() {
  local name=$1; shift
  echo "== $name"
  R --out "$RUNS/$name" --level /Game/Maps/ModelRow --res 1600x900 -- -AcNoSave -AcNoAI -AcPaused \
    -AcModelFocus=mule_blue "-AcShot=$OUT/$name.png" "$@" || echo "!! $name failed"
}

want() { [[ $# -eq 0 ]] && return 0; local n; for n in "${NAMES[@]}"; do [[ $n == "$1" ]] && return 0; done; return 1; }
NAMES=("$@")
all() { [[ ${#NAMES[@]} -eq 0 ]]; }

# shellcheck disable=SC2046
{
  (all || want 3q) && shot 3q $(load 420 140)
  (all || want 3q-red) && shot 3q-red -AcModelTeam=1 $(load 420 140)
  (all || want side-empty) && shot side-empty -AcModelEye=-40,560,120 -AcModelLook=-40,0,80 -AcModelFov=28 $(load 0 0)
  (all || want side-half) && shot side-half -AcModelEye=-40,560,120 -AcModelLook=-40,0,80 -AcModelFov=28 $(load 380 120)
  (all || want side-full) && shot side-full -AcModelEye=-40,560,120 -AcModelLook=-40,0,80 -AcModelFov=28 $(load 860 170)
  (all || want screen) && shot screen -AcModelEye=-84,190,100 -AcModelLook=-84,0,99 -AcModelFov=20 $(load 380 120)
  (all || want screen-left) && shot screen-left -AcModelEye=-84,-190,100 -AcModelLook=-84,0,99 -AcModelFov=20 $(load 380 120)
  (all || want front) && shot front -AcModelEye=520,140,150 -AcModelLook=0,0,70 -AcModelFov=30 $(load 860 170)
  (all || want top) && shot top -AcModelEye=-200,260,520 -AcModelLook=-30,0,60 -AcModelFov=30 $(load 860 170)
}
