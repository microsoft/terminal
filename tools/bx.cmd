@echo off

@rem TODO Find a way to only rebuild the metaproj if the sln changed

setlocal
rem set MSBuildEmitSolution=1
rem "%msbuild%" %OPENCON%\OpenConsole.sln /t:ValidateSolutionConfiguration /m > NUL
rem set MSBuildEmitSolution=0

set _METAPROJ=%OPENCON%\OpenConsole.sln.metaproj

set _BX_SCRIPT=powershell bx.ps1

rem @echo on
FOR /F "tokens=* USEBACKQ" %%F IN (`%_BX_SCRIPT% 2^> NUL`) DO (
    echo %%F
    set _OUTPUT=%%F
)
if (%_OUTPUT%) == () (
    echo Could not find a .vcxproj file in this directory.
    echo `bx.cmd` only works in directories with a vcxproj file.
    echo Please navigate to directory with a project file in it, or try `bz` to build the entire solution.
    goto :eof
)

rem echo Found %_OUTPUT%
set PROJECT_NAME=%_OUTPUT%

set MSBUILD_COMMAND="%msbuild%" %OPENCON%\Openconsole.sln /t:%PROJECT_NAME% /m /p:Configuration=Debug /p:Platform=%ARCH%

echo Building only %PROJECT_NAME% Using commandline:
echo %MSBUILD_COMMAND%

%MSBUILD_COMMAND%

endlocal
:eof
