@echo off
setlocal

rem ===========================================================================
rem  yialite - build and install to install\<preset>
rem
rem      scripts\install_windows.bat [preset] [config]
rem
rem      preset   configure preset from CMakePresets.json    (default: msvc)
rem      config   Debug or Release                           (default: Debug)
rem
rem  Examples
rem      scripts\install_windows.bat
rem      scripts\install_windows.bat msvc Release
rem      scripts\install_windows.bat gcc
rem
rem  Produces the same layout the presets point at, so a consumer project can
rem  find it with:  set CMAKE_PREFIX_PATH=...\install\msvc
rem ===========================================================================

set "HERE=%~dp0"
cd /d "%HERE%.."

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc"

set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

call "%HERE%build_windows.bat" "%PRESET%" "%CONFIG%"
if errorlevel 1 exit /b 1

set "PREFIX=%CD%\install\%PRESET%"

echo [install] %PRESET% ^| %CONFIG% -^> %PREFIX%
cmake --install "%CD%\build\%PRESET%" --config "%CONFIG%" --prefix "%PREFIX%"
if errorlevel 1 goto fail

echo.
echo [ok] %PREFIX%
exit /b 0

:fail
echo.
echo [failed] see the output above
exit /b 1
