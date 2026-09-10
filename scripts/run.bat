@echo off
setlocal

rem ===========================================================================
rem  yialite - build and run the sandbox
rem
rem      scripts\run.bat [preset] [config] [sandbox args...]
rem
rem      preset   configure preset from CMakePresets.json    (default: msvc)
rem      config   Debug or Release                           (default: Debug)
rem
rem  Examples
rem      scripts\run.bat
rem      scripts\run.bat msvc Release
rem      scripts\run.bat msvc Debug --headless --frames=120
rem
rem  The build runs first: launching a stale executable and debugging code that
rem  is not in it wastes more time than the build costs.
rem ===========================================================================

set "HERE=%~dp0"
cd /d "%HERE%.."

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=msvc"

set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "EXTRA=%~3 %~4 %~5 %~6 %~7 %~8 %~9"

call "%HERE%build_windows.bat" "%PRESET%" "%CONFIG%" yialite_sandbox
if errorlevel 1 exit /b 1

set "EXE=%CD%\build\%PRESET%\sandbox\%CONFIG%\yialite_sandbox.exe"
if not exist "%EXE%" set "EXE=%CD%\build\%PRESET%\sandbox\yialite_sandbox.exe"
if not exist "%EXE%" (
    echo [failed] yialite_sandbox.exe not found under build\%PRESET%\sandbox
    exit /b 1
)

echo [run] %EXE%
"%EXE%" %EXTRA%
exit /b %ERRORLEVEL%
