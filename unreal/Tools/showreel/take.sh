#!/bin/zsh
# One showreel take: a picture set's run recorded as motion from a moment on
# (-AcMmoShotsClip, AcMmoShots.cpp), hidden and silent through hidden_run.py
# (one run at a time on the Mac), then encoded by raw2mp4.py.
#
#   unreal/Tools/showreel/take.sh DIR SECONDS [--ui] -- FLAGS
#   unreal/Tools/showreel/take.sh unreal/server/build/v4/runs/m2 20 --ui -- -AcMmoShotsM2 -AcMmoShotsOnly=m2-01
#
# --ui keeps the HUD (the HUD's moments; the clean ones drop it anyway).
# The take lands in DIR/clip.mp4 with its contact sheet and the moments'
# times in the clip (DIR/clip-moments.txt). A DIR that holds a take already
# is refused: takes are kept.
set -e
cd "${0:A:h}/../../.."
dir=$1 secs=$2; shift 2
ui=()
[[ $1 == --ui ]] && { ui=(-AcShotUI); shift; }
[[ $1 == -- ]] && shift
[[ -e $dir/clip.mp4 ]] && { echo "$dir/clip.mp4 is a take already: pick another folder"; exit 1; }
mkdir -p "$dir"
uv run unreal/Tools/hidden_run.py --out "$dir" --res 1920x1080 --timeout 20 -- \
  -AcMap=highlands-small -AcFlowTest=mmoshots '-AcMmoShots={out}' -AcMmoShotsClip \
  '-AcShot={out}/clip.png' -AcShotRecord=$secs '-AcShotRaw={out}/clip.raw' -AcShotFrames=1 -AcNoPerfPanel \
  $ui -nosound "$@" || echo "hidden run exit $?"
uv run unreal/Tools/showreel/raw2mp4.py "$dir"
