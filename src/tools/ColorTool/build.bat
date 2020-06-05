@echo off

rem Add path to MSBuild Binaries
set MSBUILD=()
for /f "usebackq tokens=*" %%f in (`where.exe msbuild.exe 2^>nul`) do (
    set MSBUILD="%%~ff"
    goto :FOUND_MSBUILD
)
set AppFiles=%ProgramFiles(x86)%
if NOT %PROCESSOR_ARCHITECTURE%==AMD64 set AppFiles=%ProgramFiles%
set MSBUILD="%AppFiles%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
if exist %MSBUILD% (
    goto :FOUND_MSBUILD
)
set MSBUILD="%AppFiles%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\msbuild.exe"
if exist %MSBUILD% (
    goto :FOUND_MSBUILD
)
set MSBUILD="%AppFiles%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\msbuild.exe"
if exist %MSBUILD% (
    goto :FOUND_MSBUILD
)
set MSBUILD="%AppFiles%\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\msbuild.exe"
if exist %MSBUILD% (
    goto :FOUND_MSBUILD
)
set MSBUILD="%AppFiles%\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe"
if exist %MSBUILD% (
    goto :FOUND_MSBUILD
)
set MSBUILD="%AppFiles%\MSBuild\14.0\bin\msbuild.exe"
if exist %MSBUILD% (
    goto :FOUND_MSBUILD
)
echo "MSBuild was not found. Make sure it is installed and included in the search path."
goto :EXIT

:FOUND_MSBUILD
echo MSBuild was found at %MSBUILD%
echo.

set _MSBUILD_TARGET=Build
set _MSBUILD_CONFIG=Debug

:ARGS_LOOP
if (%1) == () goto :POST_ARGS_LOOP
if (%1) == (clean) (
    set _MSBUILD_TARGET=Clean,Build
)
if (%1) == (rel) (
    set _MSBUILD_CONFIG=Release
)
shift
goto :ARGS_LOOP

:POST_ARGS_LOOP
%MSBUILD% %~dp0ColorTool.sln /t:%_MSBUILD_TARGET% /m /nr:true /p:Configuration=%_MSBUILD_CONFIG% /p:OutputPath=.\bin\%_MSBUILD_CONFIG%\

if (%ERRORLEVEL%) == (0) (
    echo.
    echo Created exe in:
    echo    %~dp0ColorTool\bin\%_MSBUILD_CONFIG%\ColorTool.exe
    echo.
    echo Copying exe to root of project...
    copy %~dp0ColorTool\bin\%_MSBUILD_CONFIG%\colortool.exe %~dp0ColorTool.exe
    echo.
)

:EXIT
Pause