@echo off
setlocal

rem ===========================================================================
rem  yialite - Windows build
rem
rem      scripts\build_windows.bat [preset] [config] [target]
rem
rem      preset   configure preset from CMakePresets.json      (default: msvc)
rem      config   Debug or Release - only meaningful for a multi-configuration
rem               generator such as the Visual Studio one. The Ninja presets
rem               fix the build type when they are configured, which is why
rem               they come in pairs (mingw / mingw-release).  (default: Debug)
rem      target   a single CMake target                        (default: all)
rem
rem  Examples
rem      scripts\build_windows.bat
rem      scripts\build_windows.bat msvc Release
rem      scripts\build_windows.bat msvc Debug yialite_core
rem      scripts\build_windows.bat mingw
rem ===========================================================================

set "HERE=%~dp0"
cd /d "%HERE%.."

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc"

set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "TARGET=%~3"
set "BUILD_DIR=%CD%\build\%PRESET%"

if exist "%BUILD_DIR%\CMakeCache.txt" goto build

echo [configure] %PRESET%
cmake --preset "%PRESET%"
if errorlevel 1 goto fail

:build
set "MULTI=1"
findstr /b /c:"CMAKE_CONFIGURATION_TYPES:" "%BUILD_DIR%\CMakeCache.txt" >nul 2>&1
if errorlevel 1 set "MULTI="
if not defined MULTI if /i not "%CONFIG%"=="Debug" (
    echo [warn] preset "%PRESET%" is single-config: --config %CONFIG% has no effect.
    echo        Use a Release preset ^(e.g. mingw-release^) instead.
)

if "%TARGET%"=="" (
    echo [build] %PRESET% ^| %CONFIG%
    cmake --build "%BUILD_DIR%" --config "%CONFIG%"
) else (
    echo [build] %PRESET% ^| %CONFIG% ^| target %TARGET%
    cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target "%TARGET%"
)
if errorlevel 1 goto fail

echo.
echo [ok] %BUILD_DIR%
exit /b 0

:fail
echo.
echo [failed] see the output above
exit /b 1
