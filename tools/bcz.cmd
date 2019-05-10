@echo off

rem bcz - Clean and build the project
rem This is another script to help Microsoft developers feel at home working on the openconsole project.

if (%_LAST_BUILD_CONF%)==() (
    set _LAST_BUILD_CONF=%DEFAULT_CONFIGURATION%
)

set _MSBUILD_TARGET=Clean,Build

:ARGS_LOOP
if (%1) == () goto :POST_ARGS_LOOP
if (%1) == (dbg) (
    echo Manually building debug
    set _LAST_BUILD_CONF=Debug
)
if (%1) == (rel) (
    echo Manually building release
    set _LAST_BUILD_CONF=Release
)
if (%1) == (no_clean) (
    set _MSBUILD_TARGET=Build
)
shift
goto :ARGS_LOOP

:POST_ARGS_LOOP
echo Starting build...

nuget.exe restore %OPENCON%\OpenConsole.sln

rem /p:AppxBundle=Never prevents us from building the appxbundle from the commandline.
rem We don't want to do this from a debug build, because it takes ages, so disable it.
rem if you want the appx, build release

set _APPX_ARGS=

if (%_LAST_BUILD_CONF%) == (Debug) (
    echo Skipping building appx...
    set _APPX_ARGS=/p:AppxBundle=false
) else (
    echo Building Appx...
)

set _BUILD_CMDLINE="%MSBUILD%" %OPENCON%\OpenConsole.sln /t:%_MSBUILD_TARGET% /m /p:Configuration=%_LAST_BUILD_CONF% /p:Platform=%ARCH% %_APPX_ARGS%

echo %_BUILD_CMDLINE%
%_BUILD_CMDLINE%

rem Cleanup unused variables here. Note we cannot use setlocal because we need to pass modified
rem _LAST_BUILD_CONF out to OpenCon.cmd later.
rem
set _MSBUILD_TARGET=
set _BIN_=%~dp0\bin\%PLATFORM%\%_LAST_BUILD_CONF%
