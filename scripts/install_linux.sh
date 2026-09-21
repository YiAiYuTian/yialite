#!/usr/bin/env bash
#
# ===========================================================================
#  yialite - build and install to install/<preset>
#
#      scripts/install_linux.sh [preset] [config]
#
#      preset   configure preset from CMakePresets.json    (default: gcc)
#      config   Debug or Release                           (default: Debug)
#
#  Examples
#      scripts/install_linux.sh
#      scripts/install_linux.sh gcc-release
#
#  Produces the same layout the presets point at, so a consumer project can
#  find it with:  cmake -DCMAKE_PREFIX_PATH=.../install/gcc ...
# ===========================================================================

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/.."

preset="${1:-gcc}"
config="${2:-Debug}"
prefix="$PWD/install/$preset"

"$here/build_linux.sh" "$preset" "$config"

echo "[install] $preset | $config -> $prefix"
cmake --install "build/$preset" --config "$config" --prefix "$prefix"

echo
echo "[ok] $prefix"
