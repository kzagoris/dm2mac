#!/bin/bash
# mk.sh NN-name "still1|Caption" "still2|Caption" ... : move the old gameplay clip aside, copy the stills (given as shots/NAME.png
# paths) into NN-name/1-.., build NN-name.mov and the check sheet work/shots/chk.png
T=captures/feature-tweets; K=.claude/skills/phase-movie/scripts
N=$1; shift
if [ -f $T/$N.mov ] && [ ! -f $T/gameplay/$N.mov ]; then mv $T/$N.mov $T/gameplay/; fi
mkdir -p $T/$N; rm -f $T/$N/*.png
i=0; args=()
for spec in "$@"; do
  i=$((i+1)); src=${spec%%|*}; cap=${spec#*|}
  cp "$src" "$T/$N/$i-$(basename "$src")"
  args+=("$T/$N/$i-$(basename "$src")|$cap")
done
bash $K/phase_movie.sh $T/$N.mov "${args[@]}" && bash $K/check.sh $T/$N.mov $T/work/shots/chk.png
