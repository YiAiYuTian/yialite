#!/usr/bin/env bash
#
# ===========================================================================
#  yialite - build and run the sandbox
#
#      scripts/run.sh [preset] [config] [sandbox args...]
#
#      preset   configure preset from CMakePresets.json    (default: gcc)
#      config   Debug or Release                           (default: Debug)
#
#  Examples
#      scripts/run.sh
#      scripts/run.sh gcc-release
#      scripts/run.sh gcc Debug --headless --frames=120
#
#  The build runs first: launching a stale executable and debugging code that
#  is not in it wastes more time than the build costs.
# ===========================================================================

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/.."

preset="${1:-gcc}"
config="${2:-Debug}"
extra=("${@:3}")

"$here/build_linux.sh" "$preset" "$config" yialite_sandbox

exe="build/$preset/sandbox/yialite_sandbox"
if [ ! -x "$exe" ]; then
    exe="build/$preset/sandbox/$config/yialite_sandbox"
fi
if [ ! -x "$exe" ]; then
    echo "[failed] yialite_sandbox not found under build/$preset/sandbox" >&2
    exit 1
fi

echo "[run] $exe"
if [ "${#extra[@]}" -gt 0 ]; then
    exec "$exe" "${extra[@]}"
else
    exec "$exe"
fi
