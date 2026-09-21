#!/usr/bin/env bash
#
# ===========================================================================
#  yialite - Linux / macOS build
#
#      scripts/build_linux.sh [preset] [config] [target]
#
#      preset   configure preset from CMakePresets.json      (default: gcc)
#      config   Debug or Release - only meaningful for a multi-configuration
#               generator such as the Visual Studio one. The Ninja presets fix
#               the build type when they are configured, which is why they
#               come in pairs (gcc / gcc-release).           (default: Debug)
#      target   a single CMake target                        (default: all)
#
#  Examples
#      scripts/build_linux.sh
#      scripts/build_linux.sh gcc-release
#      scripts/build_linux.sh gcc Debug yialite_core
# ===========================================================================

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/.."

preset="${1:-gcc}"
config="${2:-Debug}"
target="${3:-}"

build_dir="build/$preset"

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    echo "[configure] $preset"
    cmake --preset "$preset"
fi

if ! grep -q '^CMAKE_CONFIGURATION_TYPES:' "$build_dir/CMakeCache.txt" \
        && [ "$config" != "Debug" ]; then
    echo "[warn] preset '$preset' is single-config: --config $config has no effect." >&2
    echo "       Use a Release preset (e.g. gcc-release) instead." >&2
fi

if [ -z "$target" ]; then
    echo "[build] $preset | $config"
    cmake --build "$build_dir" --config "$config"
else
    echo "[build] $preset | $config | target $target"
    cmake --build "$build_dir" --config "$config" --target "$target"
fi

echo
echo "[ok] $build_dir"
