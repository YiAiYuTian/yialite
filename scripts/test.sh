#!/usr/bin/env bash
#
# ===========================================================================
#  yialite - build and run the automated tests
#
#      scripts/test.sh [preset] [config] [ctest args...]
#
#      preset   configure preset from CMakePresets.json    (default: mingw)
#      config   Debug or Release                           (default: Debug)
#
#  Examples
#      scripts/test.sh
#      scripts/test.sh mingw-release
#      scripts/test.sh mingw Debug -R core_smoke
# ===========================================================================

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/.."

preset="${1:-mingw}"
config="${2:-Debug}"
extra=("${@:3}")

"$here/build_linux.sh" "$preset" "$config" yialite_tests

echo "[test] $preset | $config"
if [ "${#extra[@]}" -gt 0 ]; then
    ctest --test-dir "build/$preset" -C "$config" --output-on-failure "${extra[@]}"
else
    ctest --test-dir "build/$preset" -C "$config" --output-on-failure
fi
