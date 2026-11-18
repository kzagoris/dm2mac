#!/bin/bash
# ctrlshot.sh NAME SAVE : start SAVE with a window, shoot NAME-0 (no Ctrl) and NAME-1 (Ctrl held), stop.
# Run inside WSL: MSYS_NO_PATHCONV=1 wsl -e bash .claude/skills/phase-movie/scripts/ctrlshot.sh NAME SAVE
K=$(dirname "$0"); W=captures/feature-tweets/work; n=$1
DM2_VIDEO=x11 bash $K/game.sh start $2 >/dev/null; sleep 1
bash $K/game.sh req '{"cmd":"move","x":150,"y":150}' '{"cmd":"wait","ticks":20}' >/dev/null
bash $K/game.sh shot $n-0 >/dev/null
python3 $K/xkey.py down Control_L; sleep 0.3
bash $K/game.sh req '{"cmd":"wait","ticks":15}' >/dev/null
bash $K/game.sh shot $n-1 >/dev/null
python3 $K/xkey.py up Control_L
bash $K/game.sh stop >/dev/null
