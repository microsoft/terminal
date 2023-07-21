@echo off

rem bcz - Clean and build the solution.
rem This is another script to help Microsoft developers feel at home working on the Terminal project.

rem Args:
rem     dbg: manually build the solution in the Debug configuration. If omitted,
rem          we'll use whatever the last configuration build was.
rem     rel: manually build the solution in the Release configuration. If
rem          omitted, we'll use whatever the last configuration build was.
rem     no_clean: Don't clean before building. This is a much faster build
rem          typically, but leaves artifacts from previous builds around, which
rem          can lead to unexpected build failures.
rem     exclusive: Only build the project in the cwd. If omitted, we'll try
rem          building the entire solution instead.

if (%_LAST_BUILD_CONF%)==() (
    set _LAST_BUILD_CONF=%DEFAULT_CONFIGURATION%
)

set _MSBUILD_TARGET=Clean;Build
set _EXCLUSIVE=
set _APPX_ARGS=

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
if (%1) == (audit) (
    echo Manually building audit mode
    set _LAST_BUILD_CONF=AuditMode
)
if (%1) == (no_clean) (
    set _MSBUILD_TARGET=Build
)
if (%1) == (exclusive) (
    set _EXCLUSIVE=1
)
shift
goto :ARGS_LOOP
:POST_ARGS_LOOP


if "%_EXCLUSIVE%" == "1" (
    set "PROJECT_NAME="
    call :get_project
) else if (%_LAST_BUILD_CONF%) == (Debug) (

    rem /p:AppxBundle=Never prevents us from building the appxbundle from the
    rem commandline. We don't want to do this from a debug build, because it
    rem takes ages, so disable it. if you want the appx, build release

    rem Only do this check if we're doing a full solution build. If we're only
    rem trying to build the appx, then we obviously want to build the appx.

    echo Skipping building appx...
    set _APPX_ARGS=/p:AppxBundle=false
) else (
    echo Building Appx...
)

if "%_EXCLUSIVE%" == "1" (
    if "%PROJECT_NAME%" == "" ( goto :eof ) else echo Building only %PROJECT_NAME%
)

if "%_SKIP_NUGET_RESTORE%" == "1" (
    echo Skipped nuget restore
) else (
    echo Performing nuget restore...
    nuget.exe restore %OPENCON%\OpenConsole.sln
)

@rem /p:GenerateAppxPackageOnBuild=false will prevent us from building the whole .msix package when building the wapproj project.
set _BUILD_CMDLINE="%MSBUILD%" %OPENCON%\OpenConsole.sln /t:"%_MSBUILD_TARGET%" /m /p:Configuration=%_LAST_BUILD_CONF% /p:GenerateAppxPackageOnBuild=false /p:Platform=%ARCH% %_APPX_ARGS%

echo %_BUILD_CMDLINE%
echo Starting build...

@rem start indeterminate progress in the taskbar
@rem this `<NUL set /p =` magic will output the text _without a newline_
<NUL set /p =]9;4;3

%_BUILD_CMDLINE%

@rem capture the return value of msbuild, so we can return that as our return value.
set _build_result=%errorlevel%
if (%_build_result%) == (0) (

@rem clear the progress
<NUL set /p =]9;4

) else (

@rem set the taskbar to the error state, then sleep for 500ms, before clearing
@rem the progress state. This will "blink" the error state into the taskbar

<NUL set /p =]9;4;2;100

@rem this works to "sleep" the console for 500ms. `ping` can't wait for less
@rem than 500ms, and it will only wait if the target address _doesn't_ respond,
@rem hence why we're using 128., not 127.

ping 128.0.0.1 -n 1 -w 500 > nul

<NUL set /p =]9;4

)

rem Cleanup unused variables here. Note we cannot use setlocal because we need to pass modified
rem _LAST_BUILD_CONF out to OpenCon.cmd later.
rem
set _MSBUILD_TARGET=
set _BIN_=%~dp0\bin\%PLATFORM%\%_LAST_BUILD_CONF%

@rem Exit with the value from msbuild. If msbuild is unsuccessful in building, this will be 1
EXIT /b %_build_result%

goto :eof

rem ############################################################################
rem The code to figure out what project we're building needs to be in its own
rem function. Otherwise, when cmd evaluates the if statement above `if
rem "%_EXCLUSIVE%" == "1"`, it'll evaluate the entire block with the value of
rem the variables at the time the if was executed. So instead, make a function
rem here with `enabledelayedexpansion` set.
:get_project
setlocal enabledelayedexpansion

rem TODO:GH#2172 Find a way to only rebuild the metaproj if the sln changed
rem First generate the metaproj file
set MSBuildEmitSolution=1
"%msbuild%" %OPENCON%\OpenConsole.sln /t:ValidateSolutionConfiguration /m > NUL
set MSBuildEmitSolution=

rem Use bx.ps1 to figure out which target we're looking at
set _OUTPUT=
FOR /F "tokens=* USEBACKQ" %%F IN (`powershell -NoProfile -ExecutionPolicy Bypass -NonInteractive bx.ps1 2^> NUL`) DO (
    set _OUTPUT=%%F
)
if "!_OUTPUT!" == "" (
    echo Could not find a .vcxproj file in this directory.
    echo `bx.cmd` only works in directories with a vcxproj file.
    echo Please navigate to directory with a project file in it, or try `bz` to build the entire solution.
    goto :eof
)
set "__PROJECT_NAME=!_OUTPUT!"

rem If we're trying to clean build, make sure to update the target here.
if "%_MSBUILD_TARGET%" == "Build" (
    set __MSBUILD_TARGET=%__PROJECT_NAME%
) else if "%_MSBUILD_TARGET%" == "Clean;Build" (
    set __MSBUILD_TARGET=%__PROJECT_NAME%:Rebuild
) else (
    echo.
    echo Oops... build bug in the neighborhood of configuring a build target.
    echo.
)
rem This statement will propagate our internal variables up to the calling
rem scope. Because they're all on one line, the value of our local variables
rem will be evaluated before we endlocal
endlocal & set "PROJECT_NAME=%__PROJECT_NAME%" & set "_MSBUILD_TARGET=%__MSBUILD_TARGET%"
rem ############################################################################


:eof
