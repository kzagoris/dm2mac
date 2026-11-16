#!/bin/bash
# Decompile reference/SKULL.EXE (else game/SKULL.EXE) to reference/decomp/SKULL.c with Ghidra headless.
# docs/reference/skull-symbols.csv, if it exists, names functions and globals.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build reference/decomp
exe=reference/SKULL.EXE
[ -f "$exe" ] || exe=game/SKULL.EXE
info=$(uv run dm2-le "$exe" build/skull.bin)
base=$(echo "$info" | awk '/^object 1:/ {print $4}')
data=$(echo "$info" | awk '/^object 2:/ {print $4}')
entry=0x$(echo "$info" | awk -F: '/^entry:/ {print $3}')
SYMBOLS=docs/reference/skull-symbols.csv tools/ghidra/decompile.sh build/skull.bin "$base" "$data" reference/decomp/SKULL.c "$entry"
