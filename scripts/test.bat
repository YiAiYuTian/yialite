@echo off
setlocal

rem ===========================================================================
rem  yialite - build and run the automated tests
rem
rem      scripts\test.bat [preset] [config] [ctest args...]
rem
rem      preset   configure preset from CMakePresets.json    (default: msvc)
rem      config   Debug or Release                           (default: Debug)
rem
rem  Examples
rem      scripts\test.bat
rem      scripts\test.bat msvc Release
rem      scripts\test.bat msvc Debug -R core_smoke
rem ===========================================================================

set "HERE=%~dp0"
cd /d "%HERE%.."

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc"

set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "EXTRA=%~3 %~4 %~5 %~6 %~7 %~8 %~9"

call "%HERE%build_windows.bat" "%PRESET%" "%CONFIG%" yialite_tests
if errorlevel 1 exit /b 1

echo [test] %PRESET% ^| %CONFIG%
ctest --test-dir "%CD%\build\%PRESET%" -C "%CONFIG%" --output-on-failure %EXTRA%
exit /b %ERRORLEVEL%
