@echo off
setlocal

set "_CONFIG=Release"
set "_PLATFORM=x64"
set "_NO_LAUNCH="

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
if /I "%~1"=="no_launch" (
    set "_NO_LAUNCH=-NoLaunch"
    shift
    goto :ARGS_LOOP
)

echo Unknown argument: %~1
echo Usage: tools\wtd.cmd [rel^|dbg] [x64^|x86^|arm64^|arm] [no_launch]
exit /b 2

:RUN
set "_SCRIPT=%~dp0RegisterAndLaunchWtd.ps1"
if not exist "%_SCRIPT%" set "_SCRIPT=%~dp0tools\RegisterAndLaunchWtd.ps1"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%_SCRIPT%" -Configuration "%_CONFIG%" -Platform "%_PLATFORM%" %_NO_LAUNCH%
exit /b %ERRORLEVEL%
