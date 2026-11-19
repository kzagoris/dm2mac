#!/bin/bash
# check.sh MOVIE OUT.png : one frame from the middle of every phase (phase k starts at k*2.2 s), in one contact sheet
m=$1; out=$2
dur=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$m")
n=$(awk "BEGIN{n=0; while (n*2.2+2.5 <= $dur + 0.05) n++; print n}")
tmp=$(mktemp -d -p "$(pwd)/captures/feature-tweets/work"); trap 'rm -rf "$tmp"' EXIT
for ((i=0;i<n;i++)); do t=$(awk "BEGIN{print $i*2.2+1.2}"); ffmpeg -v error -y -ss $t -i "$(cygpath -m "$m")" -frames:v 1 -vf scale=480:360 "$(cygpath -m "$tmp")/f$(printf %02d $i).png"; done
abs=$(cygpath -m "$(pwd)/$out"); files=$(ls "$tmp"/f*.png | xargs -I{} cygpath -m {}); magick montage $files -tile 3x -geometry +2+2 "$abs"
echo "$n phases -> $out"
