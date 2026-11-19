#!/bin/bash
# Tile 320x200 screenshots into one contact sheet, 4 per row, each labelled with its file name, so one image read
# compares them. --grid enlarges 3x and draws a 20-pixel grid labelled in game coordinates, for finding click positions.
# Runs in Git Bash with the Windows ffmpeg:  bash .claude/skills/phase-movie/scripts/sheet.sh OUT.png [--grid] IN.png...
set -e
out=$1; shift
grid=0; [ "$1" = --grid ] && { grid=1; shift; }
n=$#; cols=$(( n < 4 ? n : 4 ))
font="C\\:/Windows/Fonts/consola.ttf"
if [ $grid = 1 ]; then w=960; h=600; else w=320; h=200; fi
inputs=(); f=""; ins=""; layout=""
i=0
for still in "$@"; do
    inputs+=(-i "$still")
    name=$(basename "$still" .png)
    if [ $grid = 1 ]; then
        g="scale=960:600:flags=neighbor,drawgrid=w=60:h=60:t=1:c=yellow@0.35"
        for v in 40 80 120 160 200 240 280; do g+=",drawtext=fontfile='$font':text='$v':x=$((v*3+2)):y=2:fontsize=16:fontcolor=yellow"; done
        for v in 40 80 120 160; do g+=",drawtext=fontfile='$font':text='$v':x=2:y=$((v*3+2)):fontsize=16:fontcolor=yellow"; done
        f+="[$i:v]$g,drawtext=fontfile='$font':text='$name':x=4:y=h-22:fontsize=18:fontcolor=white:box=1:boxcolor=black[t$i];"
    else
        f+="[$i:v]drawtext=fontfile='$font':text='$name':x=2:y=h-12:fontsize=11:fontcolor=white:box=1:boxcolor=black[t$i];"
    fi
    ins+="[t$i]"
    [ $i -gt 0 ] && layout+="|"
    layout+="$(( (i % cols) * w ))_$(( (i / cols) * h ))"
    i=$((i + 1))
done
if [ $n = 1 ]; then
    ffmpeg -v error -y "${inputs[@]}" -filter_complex "${f%;}" -map "[t0]" -frames:v 1 "$out"
else
    ffmpeg -v error -y "${inputs[@]}" -filter_complex "$f${ins}xstack=inputs=$n:layout=$layout:fill=black" -frames:v 1 "$out"
fi
echo "$out"
