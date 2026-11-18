#!/bin/bash
# Headless dm2 (the Linux build/dm2, run inside WSL) driven through the control socket.
# Run from Git Bash as:  MSYS_NO_PATHCONV=1 wsl -e bash .claude/skills/phase-movie/scripts/game.sh <command> ...
#
#   start SAVE            copy SAVE to work/run.DAT and start the game on it (Advanced mode, saves in work/saves)
#   req JSON...           send requests, print each reply (cut to 300 characters)
#   shot NAME             screenshot to work/shots/NAME.png
#   burst PREFIX T...     screenshots PREFIX-T.png at T ticks after now (T ascending, e.g. 2 4 8 16 32)
#   stop                  quit the game and confirm no dm2 process is left
#
# WORK defaults to captures/feature-tweets/work. Never point it at game/DATA.
set -e
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
WORK=${WORK:-$ROOT/captures/feature-tweets/work}
SOCK=/tmp/dm2phase.sock
CTL="python3 $(dirname "$0")/ctl.py $SOCK"
mkdir -p "$WORK/shots" "$WORK/saves"
cmd=$1; shift
case $cmd in
start)
    if pgrep -x dm2 >/dev/null; then echo "a dm2 is already running in WSL: run stop first" >&2; exit 1; fi
    cp "$1" "$WORK/run.DAT"
    cd "$ROOT"
    SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=${DM2_VIDEO:-dummy} setsid nohup build/dm2 --mode advanced --saves "$WORK/saves" \
        --control $SOCK --save "$WORK/run.DAT" >"$WORK/game.log" 2>&1 &
    $CTL '{"cmd":"wait","ticks":30}' | cut -c1-300
    ;;
req)
    $CTL "$@" | cut -c1-300
    ;;
run)
    # a JSON list of requests in a file (the last line of route.py's output), then the party position
    mapfile -t reqs < <(python3 -c 'import json,sys; [print(json.dumps(r)) for r in json.loads(open(sys.argv[1]).read().splitlines()[-1])]' "$1")
    $CTL "${reqs[@]}" '{"cmd":"state"}' | tail -1 | grep -o '"party":{[^}]*}'
    ;;
shot)
    $CTL "{\"cmd\":\"screenshot\",\"path\":\"$WORK/shots/$1.png\"}" >/dev/null && echo "$WORK/shots/$1.png"
    ;;
burst)
    prefix=$1; shift; prev=0; args=()
    for t in "$@"; do
        [ $((t - prev)) -gt 0 ] && args+=("{\"cmd\":\"wait\",\"ticks\":$((t - prev))}")
        args+=("{\"cmd\":\"screenshot\",\"path\":\"$WORK/shots/$prefix-$t.png\"}")
        prev=$t
    done
    $CTL "${args[@]}" >/dev/null && echo "shots $prefix-{$*}"
    ;;
stop)
    $CTL '{"cmd":"quit"}' >/dev/null 2>&1 || true
    sleep 1
    if pgrep -x dm2 >/dev/null; then echo "dm2 still running: pids $(pgrep -x dm2)" >&2; exit 1; fi
    echo stopped
    ;;
*)
    echo "usage: game.sh start SAVE | req JSON... | shot NAME | burst PREFIX T... | stop" >&2; exit 2
    ;;
esac
