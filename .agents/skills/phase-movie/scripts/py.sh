#!/bin/bash
# Run a Python script inside WSL with the repository's dependencies, in a venv of its own outside the repository,
# so a staging run never changes the repository's .venv (the WSL venv). Never run uv from Windows in this repository.
# Run from Git Bash as:  MSYS_NO_PATHCONV=1 wsl -e bash .claude/skills/phase-movie/scripts/py.sh SCRIPT ARGS...
cd "$(dirname "$0")/../../../.." || exit 1
UV_PROJECT_ENVIRONMENT=$HOME/.cache/dm2mac-venv exec "$HOME/.local/bin/uv" run --frozen -q python "$@"
