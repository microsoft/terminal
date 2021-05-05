@echo OFF
setlocal

:: From our friends at Microsoft-UI-XAML
:: https://github.com/microsoft/microsoft-ui-xaml/blob/main/tools/NugetWrapper.cmd

if "%VisualStudioVersion%" == "" set VisualStudioVersion=15.0

if defined NUGETEXETOOLPATH goto :AzurePipelines

if not exist %TEMP%\nuget.5.2.0.exe (
    echo Nuget.exe not found in the temp dir, downloading.
    powershell -Command "& { Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/v5.2.0/nuget.exe -outfile $env:TEMP\nuget.5.2.0.exe }"
)

%TEMP%\nuget.5.2.0.exe %*

exit /B %ERRORLEVEL%



:AzurePipelines
echo NUGETEXETOOLPATH = %NUGETEXETOOLPATH%

%NUGETEXETOOLPATH% %*
exit /B %ERRORLEVEL%