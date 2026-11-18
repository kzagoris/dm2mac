#!/bin/bash
# Build a phase movie from 320x200 stills: 4x nearest-neighbour on a 1280x960 canvas, a 160 px caption strip,
# 2.5 s per phase (4 s for the last), 0.3 s crossfades, H.264 yuv420p 30 fps, no audio.
# Runs in Git Bash with the Windows ffmpeg:
#   bash .claude/skills/phase-movie/scripts/phase_movie.sh OUT.mov "still.png|Caption" ...
# The first still also gets the "dm2mac · Advanced" tag. Captions go through text files, so any character is safe.
set -e
out=$1; shift
n=$#
font="C\\:/Windows/Fonts/segoeuib.ttf"
fontr="C\\:/Windows/Fonts/segoeui.ttf"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
esc() { cygpath -m "$1" | sed 's/:/\\:/'; }  # a Windows path that drawtext accepts
printf '%s' 'dm2mac · Advanced' >"$tmp/tag.txt"
inputs=(); filters=""; i=0
for spec in "$@"; do
    still=${spec%%|*}; caption=${spec#*|}
    [ -f "$still" ] || { echo "missing still: $still" >&2; exit 1; }
    # the first caption shares the strip with the dm2mac tag: longer ones run into it
    if [ $i -eq 0 ] && [ ${#caption} -gt 26 ]; then
        echo "first caption has ${#caption} characters (at most 26 fit beside the tag): $caption" >&2; exit 1
    fi
    [ ${#caption} -gt 40 ] && echo "warning: caption of still $((i + 1)) has ${#caption} characters" >&2
    dur=2.5; [ $i -eq $((n - 1)) ] && dur=4
    inputs+=(-loop 1 -framerate 30 -t $dur -i "$still")
    printf '%s' "$((i + 1))/$n" >"$tmp/num$i.txt"
    printf '%s' "$caption" >"$tmp/cap$i.txt"
    f="[$i:v]scale=1280:800:flags=neighbor,pad=1280:960:0:0:black"
    f+=",drawtext=fontfile='$font':textfile='$(esc "$tmp/num$i.txt")':expansion=none:fontcolor=0xF2B63C:fontsize=52:x=48:y=800+(160-th)/2"
    f+=",drawtext=fontfile='$font':textfile='$(esc "$tmp/cap$i.txt")':expansion=none:fontcolor=white:fontsize=52:x=170:y=800+(160-th)/2"
    if [ $i -eq 0 ]; then
        f+=",drawtext=fontfile='$fontr':textfile='$(esc "$tmp/tag.txt")':expansion=none:fontcolor=0xB0B0B0:fontsize=36:x=w-tw-48:y=800+(160-th)/2"
    fi
    filters+="$f,format=yuv420p,setsar=1[v$i];"
    i=$((i + 1))
done
prev="v0"; offset=0
for ((j = 1; j < n; j++)); do
    offset=$(awk "BEGIN{print $offset + 2.2}")
    filters+="[$prev][v$j]xfade=transition=fade:duration=0.3:offset=$offset[x$j];"
    prev="x$j"
done
ffmpeg -v error -y "${inputs[@]}" -filter_complex "${filters%;}" -map "[$prev]" -r 30 -c:v libx264 -preset slow \
    -crf 16 -pix_fmt yuv420p -movflags +faststart -an "$out"
ffprobe -v error -show_entries format=duration -of csv=p=0 "$out" | sed "s|^|$out: |;s|$| s|"
