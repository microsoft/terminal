@echo off
setlocal

set "_CONFIG=Release"
set "_PLATFORM=x64"
set "_VERBOSITY=normal"
set "_REGISTER="
set "_LAUNCH="

:ARGS_LOOP
if "%~1"=="" goto :RUN
if /I "%~1"=="dbg" (
    set "_CONFIG=Debug"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="debug" (
    set "_CONFIG=Debug"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="rel" (
    set "_CONFIG=Release"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="release" (
    set "_CONFIG=Release"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="x86" (
    set "_PLATFORM=x86"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="x64" (
    set "_PLATFORM=x64"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="arm64" (
    set "_PLATFORM=arm64"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="arm" (
    set "_PLATFORM=arm"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="register" (
    set "_REGISTER=-Register"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="launch" (
    set "_REGISTER=-Register"
    set "_LAUNCH=-Launch"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="quiet" (
    set "_VERBOSITY=quiet"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="minimal" (
    set "_VERBOSITY=minimal"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="normal" (
    set "_VERBOSITY=normal"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="verbose" (
    set "_VERBOSITY=detailed"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="detailed" (
    set "_VERBOSITY=detailed"
    shift
    goto :ARGS_LOOP
)
if /I "%~1"=="diagnostic" (
    set "_VERBOSITY=diagnostic"
    shift
    goto :ARGS_LOOP
)

echo Unknown argument: %~1
echo Usage: tools\build-wtd.cmd [rel^|dbg] [x64^|x86^|arm64^|arm] [quiet^|minimal^|normal^|verbose^|detailed^|diagnostic] [register^|launch]
exit /b 2

:RUN
set "_SCRIPT=%~dp0BuildWtd.ps1"
if not exist "%_SCRIPT%" set "_SCRIPT=%~dp0tools\BuildWtd.ps1"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%_SCRIPT%" -Configuration "%_CONFIG%" -Platform "%_PLATFORM%" -Verbosity "%_VERBOSITY%" %_REGISTER% %_LAUNCH%
exit /b %ERRORLEVEL%
