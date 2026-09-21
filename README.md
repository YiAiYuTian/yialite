# yialite

A small 2D game engine in C++23, written as a personal learning project.

It is very immature — expect rough edges, missing features, and interfaces that
change without warning. Suggestions are welcome.

It is 2D for now and may or may not grow into 3D later; either way it is still
at the learning stage.

## Building

Needs CMake 3.21+ and a C++23 compiler (MSVC, GCC 13+ or Clang). Presets for
`msvc`, `gcc` and `clang`, in both debug and release, live in
`CMakePresets.json`.

Windows:

```
scripts\build_windows.bat    # build
scripts\run.bat              # build, then run the sandbox
scripts\test.bat             # build, then run the tests
```

Linux / macOS:

```
scripts/build_linux.sh
scripts/run.sh
scripts/test.sh
```

Each accepts `[preset] [config] [target]` — for example
`scripts\build_windows.bat gcc` or `scripts/build_linux.sh gcc-release`.

Or straight through CMake:

```
cmake --preset msvc
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

## Third-Party Libraries

* [imgui](https://github.com/ocornut/imgui)
* [SDL3](https://github.com/libsdl-org/SDL)
* [spdlog](https://github.com/gabime/spdlog)
* [vorbis](https://github.com/xiph/vorbis)
* [miniaudio](https://github.com/mackron/miniaudio)
* [ogg](https://github.com/xiph/ogg)
* [stb](https://github.com/nothings/stb)
* [fmt](https://github.com/fmtlib/fmt)
