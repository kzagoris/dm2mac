#!/bin/bash
# Build build/DM2Mac-linux-x64.tar.gz: the Linux game (x86-64) and SDL3. The player adds the game files.
# SDL_PREFIX (environment) names the SDL3 installation; it must be set for a build outside the system folders.
set -euo pipefail
cd "$(dirname "$0")/../.."
# Link with $ORIGIN so the binary finds the SDL3 that ships beside it. Remove the binary first: a new rpath needs a new link.
rm -f build/dm2
make build/dm2 LDFLAGS="-Wl,-rpath,'\$\$ORIGIN'"
out=build/dist-linux/DM2Mac
rm -rf build/dist-linux build/DM2Mac-linux-x64.tar.gz
mkdir -p "$out"
sdl="$(ldd build/dm2 | awk '/libSDL3\.so/ && $3 != "not" {print $3; exit}')"
if [ -z "$sdl" ] || [ ! -f "$sdl" ]; then
    sdl="$(pkg-config --variable=libdir sdl3)/libSDL3.so.0"
fi
test -f "$sdl" || { echo "libSDL3.so.0 is not found; set SDL_PREFIX or install SDL3" >&2; exit 1; }
cp -L "$sdl" "$out/libSDL3.so.0"
strip build/dm2 -o "$out/dm2"
chmod +x "$out/dm2"
cat > "$out/README.txt" <<'EOF'
DM2Mac: Dungeon Master II for Linux (64-bit).

The game files are not included. You need your own copy of the PC DOS version of
Dungeon Master II (English, 1995). Copy its folder, its ZIP archive (.zip), or its
CD image (.iso, .img, .bin) into this folder. At the first start the game checks
these 10 files and copies them into game/:
  INTRPLAY.PCX, FTL, INTRO, END, CREDITS, DRUM.BNK, MELODIC.BNK,
  DATA/SONGLIST.DAT, DATA/GRAPHICS.DAT, DATA/DUNGEON.DAT
Other versions of the game do not work: the game names the wrong file.

Start the game from this folder:
  ./dm2

The messages go to the terminal. SDL3 is included (libSDL3.so.0), so no system
SDL3 is needed. The game files, the saved games, and the settings are kept in
game/ in this folder. To remove the game, delete this folder.
EOF
(cd build/dist-linux && tar -czf ../DM2Mac-linux-x64.tar.gz DM2Mac)
rm -rf build/dist-linux
ls -lh build/DM2Mac-linux-x64.tar.gz
