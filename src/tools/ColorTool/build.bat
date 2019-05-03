@echo off

rem Add path to MSBuild Binaries
set MSBUILD=()
for /f "usebackq tokens=*" %%f in (`where.exe msbuild.exe 2^>nul`) do (
    set MSBUILD="%%~ff"
    goto :FOUND_MSBUILD
)
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
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe" (
    set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe"
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
    echo "I couldn't find MSBuild on your PC. Make sure it's installed somewhere, and if it's not in the above if statements (in build.bat), add it."
    goto :EXIT
)
:FOUND_MSBUILD
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
%MSBUILD% %~dp0ColorTool.sln /t:%_MSBUILD_TARGET% /m /nr:true /p:Configuration=%_MSBUILD_CONFIG%

if (%ERRORLEVEL%) == (0) (
    echo.
    echo Created exe in:
    echo    %~dp0ColorTool\bin\%_MSBUILD_CONFIG%\colortool.exe
    echo.
    echo Copying exe to root of project...
    copy %~dp0ColorTool\bin\%_MSBUILD_CONFIG%\colortool.exe %~dp0colortool.exe
    echo Done.
    echo.
)

:EXIT