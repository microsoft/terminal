@echo off

rem Open Console build environment setup
rem Adds msbuild to your path, and adds the open\tools directory as well
rem This recreates what it's like to be an actual windows developer!

rem skip the setup if we're already ready.
if not "%OpenConBuild%" == "" goto :END

rem Add path to MSBuild Binaries
set MSBUILD=()
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\msbuild.exe" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\msbuild.exe" (
    set MSBUILD="%ProgramFiles%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\msbuild.exe" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\msbuild.exe" (
    set MSBUILD="%ProgramFiles%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\msbuild.exe" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\msbuild.exe" (
    set MSBUILD="%ProgramFiles%\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles(x86)%\MSBuild\14.0\bin" (
    set MSBUILD="%ProgramFiles(x86)%\MSBuild\14.0\bin\msbuild.exe"
    goto :FOUND_MSBUILD
)
if exist "%ProgramFiles%\MSBuild\14.0\bin" (
    set MSBUILD="%ProgramFiles%\MSBuild\14.0\bin\msbuild.exe"
    goto :FOUND_MSBUILD
)

if %MSBUILD%==() (
    echo "Could not find MsBuild on your machine. It may be installed somewhere else."
    goto :EXIT
)

:FOUND_MSBUILD

rem Add Opencon build scripts to path
set PATH=%PATH%;%~dp0;

rem add some helper envvars - The Opencon root, and also the processor arch, for output paths
set OPENCON_TOOLS=%~dp0
rem The opencon root is at ...\open\tools\, without the last 7 chars ('\tools\')
set OPENCON=%OPENCON_TOOLS:~0,-7%

rem Add nuget to PATH
set PATH=%PATH%%OPENCON%\dep\nuget;

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
set TAEF=%OPENCON%\packages\Taef.Redist.Wlk.10.30.180808002\build\binaries\%ARCH%\TE.exe
rem Set this envvar so setup won't repeat itself
set OpenConBuild=true

:END
echo The dev environment is ready to go!

:EXIT
