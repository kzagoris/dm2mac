#!/bin/bash
# winshot.sh NAME SAVE : start the WSLg window game on SAVE (SETTINGS.TXT already set), capture its window to shots/NAME.png, stop.
# Run inside WSL: MSYS_NO_PATHCONV=1 wsl -e bash .claude/skills/phase-movie/scripts/winshot.sh NAME SAVE
K=$(dirname "$0"); W=captures/feature-tweets/work; n=$1
DM2_VIDEO=x11 bash $K/game.sh start $2 >/dev/null; sleep 3
bash $K/game.sh req '{"cmd":"move","x":318,"y":198}' '{"cmd":"wait","ticks":30}' >/dev/null
id=$(DISPLAY=:0 xwininfo -root -tree | grep DM2Mac | awk '{print $1}')
DISPLAY=:0 import -window $id $W/shots/$n.png && identify $W/shots/$n.png
bash $K/game.sh stop >/dev/null
