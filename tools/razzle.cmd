@echo off

echo Setting up dev environment...

rem Open Console build environment setup
rem Adds msbuild to your path, and adds the open\tools directory as well
rem This recreates what it's like to be an actual windows developer!

rem skip the setup if we're already ready.
if not "%OpenConBuild%" == "" goto :END

rem Add Opencon build scripts to path
set PATH=%PATH%;%~dp0;

rem add some helper envvars - The Opencon root, and also the processor arch, for output paths
set OPENCON_TOOLS=%~dp0
rem The opencon root is at ...\open\tools\, without the last 7 chars ('\tools\')
set OPENCON=%OPENCON_TOOLS:~0,-7%

rem Add nuget to PATH
set PATH=%PATH%%OPENCON%\dep\nuget;

rem Run nuget restore so you can use vswhere
nuget restore %OPENCON% -Verbosity quiet

rem Find vswhere
rem from https://github.com/microsoft/vs-setup-samples/blob/master/tools/vswhere.cmd
for /f "usebackq delims=" %%I in (`dir /b /aD /o-N /s "%~dp0..\packages\vswhere*" 2^>nul`) do (
    for /f "usebackq delims=" %%J in (`where /r "%%I" vswhere.exe 2^>nul`) do (
        set VSWHERE=%%J
    )
)

if not defined VSWHERE (
    echo Could not find vswhere on your machine. Please set the VSWHERE variable to the location of vswhere.exe and run razzle again.
    goto :EXIT
)

rem Add path to MSBuild Binaries
for /f "usebackq tokens=*" %%B in (`%VSWHERE% -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set MSBUILD=%%B
)

rem Try to find MSBuild in prerelease versions of MSVS
if not defined MSBUILD (
    for /f "usebackq tokens=*" %%B in (`%VSWHERE% -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
        set MSBUILD=%%B
    )
)

if not defined MSBUILD (
    echo Could not find MsBuild on your machine. Please set the MSBUILD variable to the location of MSBuild.exe and run razzle again.
    goto :EXIT
)

set PATH=%PATH%%MSBUILD%\..;

if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
    set ARCH=x64
    set PLATFORM=x64
) else (
    set ARCH=x86
    set PLATFORM=Win32
)
set DEFAULT_CONFIGURATION=Debug

rem call .razzlerc - for your generic razzle environment stuff
if exist "%OPENCON_TOOLS%\.razzlerc.cmd" (
    call %OPENCON_TOOLS%\.razzlerc.cmd
)   else (
    (
        echo @echo off
        echo.
        echo rem This is your razzlerc file. It can be used for default dev environment setup.
    ) > %OPENCON_TOOLS%\.razzlerc.cmd
)

rem if there are args, run them. This can be used for additional env. customization,
rem    especially on a per shortcut basis.
:ARGS_LOOP
if (%1) == () goto :POST_ARGS_LOOP
if (%1) == (dbg) (
    set DEFAULT_CONFIGURATION=Debug
    shift
    goto :ARGS_LOOP
)
if (%1) == (rel) (
    set DEFAULT_CONFIGURATION=Release
    shift
    goto :ARGS_LOOP
)
if (%1) == (x86) (
    set ARCH=x86
    set PLATFORM=Win32
    shift
    goto :ARGS_LOOP
)
if exist %1 (
    call %1
) else (
    echo Could not locate "%1"
)
shift
goto :ARGS_LOOP

:POST_ARGS_LOOP
set TAEF=%OPENCON%\packages\Microsoft.Taef.10.58.210305002\build\Binaries\%ARCH%\TE.exe
rem Set this envvar so setup won't repeat itself
set OpenConBuild=true

:END
echo The dev environment is ready to go!

:EXIT
