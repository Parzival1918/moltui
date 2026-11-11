#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if command -v vhs >/dev/null 2>&1; then
    VHS=(vhs)
elif command -v pixi >/dev/null 2>&1; then
    VHS=(pixi run -e docs vhs)
else
    echo "Error: 'vhs' is not installed (and 'pixi' was not found). Install Charmbracelet VHS or run via 'pixi run -e docs vhs'." >&2
    exit 1
fi

if [[ ! -x ./build/moltui ]]; then
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
fi

"${VHS[@]}" docs/assets/demo.tape
"${VHS[@]}" docs/assets/styles.tape
"${VHS[@]}" docs/assets/snapshots.tape

echo "Generated assets in docs/assets/:"
ls -lh docs/assets/*.gif docs/assets/*.png
