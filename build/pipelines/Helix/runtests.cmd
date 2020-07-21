setlocal ENABLEDELAYEDEXPANSION

echo %TIME%

robocopy %HELIX_CORRELATION_PAYLOAD% . /s /NP > NUL

echo %TIME%

reg add HKLM\Software\Policies\Microsoft\Windows\Appx /v AllowAllTrustedApps /t REG_DWORD /d 1 /f

rem enable dump collection for our test apps:
rem note, this script is run from a 32-bit cmd, but we need to set the native reg-key
FOR %%A IN (MUXControlsTestApp.exe,IXMPTestApp.exe,NugetPackageTestApp.exe,NugetPackageTestAppCX.exe,te.exe,te.processhost.exe,AppThatUsesMUXIndirectly.exe) DO (
  %systemroot%\sysnative\cmd.exe /c reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps\%%A" /v DumpFolder /t REG_EXPAND_SZ /d %HELIX_DUMP_FOLDER% /f
  %systemroot%\sysnative\cmd.exe /c reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps\%%A" /v DumpType /t REG_DWORD /d 2 /f
  %systemroot%\sysnative\cmd.exe /c reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps\%%A" /v DumpCount /t REG_DWORD /d 10 /f
)

echo %TIME%

:: kill dhandler, which is a tool designed to handle unexpected windows appearing. But since our tests are 
:: expected to show UI we don't want it running.
taskkill -f -im dhandler.exe

echo %TIME%
powershell -ExecutionPolicy Bypass .\EnsureMachineState.ps1
echo %TIME%
powershell -ExecutionPolicy Bypass .\InstallTestAppDependencies.ps1
echo %TIME%

set testBinaryCandidates=MUXControls.Test.dll MUXControlsTestApp.appx IXMPTestApp.appx MUXControls.ReleaseTest.dll
set testBinaries=
for %%B in (%testBinaryCandidates%) do (
    if exist %%B (
        set "testBinaries=!testBinaries! %%B"
    )
)

echo %TIME%
te.exe %testBinaries% /enablewttlogging /unicodeOutput:false /sessionTimeout:0:15 /testtimeout:0:10 /screenCaptureOnError %*
echo %TIME%

powershell -ExecutionPolicy Bypass Get-Process

move te.wtl te_original.wtl

copy /y te_original.wtl %HELIX_WORKITEM_UPLOAD_ROOT%
copy /y WexLogFileOutput\*.jpg %HELIX_WORKITEM_UPLOAD_ROOT%
for /f "tokens=* delims=" %%a in ('dir /b *.pgc') do ren "%%a" "%testnameprefix%.%%~na.pgc"
copy /y *.pgc %HELIX_WORKITEM_UPLOAD_ROOT%

set FailedTestQuery=
for /F "tokens=* usebackq" %%I IN (`powershell -ExecutionPolicy Bypass .\OutputFailedTestQuery.ps1 te_original.wtl`) DO (
  set FailedTestQuery=%%I
)

rem The first time, we'll just re-run failed tests once.  In many cases, tests fail very rarely, such that
rem a single re-run will be sufficient to detect many unreliable tests.
if "%FailedTestQuery%" == "" goto :SkipReruns

echo %TIME%
te.exe %testBinaries% /enablewttlogging /unicodeOutput:false /sessionTimeout:0:15 /testtimeout:0:10 /screenCaptureOnError /select:"%FailedTestQuery%"
echo %TIME%

move te.wtl te_rerun.wtl

copy /y te_rerun.wtl %HELIX_WORKITEM_UPLOAD_ROOT%
copy /y WexLogFileOutput\*.jpg %HELIX_WORKITEM_UPLOAD_ROOT%

rem If there are still failing tests remaining, we'll run them eight more times, so they'll have been run a total of ten times.
rem If any tests fail all ten times, we can be pretty confident that these are actual test failures rather than unreliable tests.
if not exist te_rerun.wtl goto :SkipReruns

set FailedTestQuery=
for /F "tokens=* usebackq" %%I IN (`powershell -ExecutionPolicy Bypass .\OutputFailedTestQuery.ps1 te_rerun.wtl`) DO (
  set FailedTestQuery=%%I
)

if "%FailedTestQuery%" == "" goto :SkipReruns

echo %TIME%
te.exe %testBinaries% /enablewttlogging /unicodeOutput:false /sessionTimeout:0:15 /testtimeout:0:10 /screenCaptureOnError /testmode:Loop /LoopTest:8 /select:"%FailedTestQuery%"
echo %TIME%

powershell -ExecutionPolicy Bypass Get-Process

move te.wtl te_rerun_multiple.wtl

copy /y te_rerun_multiple.wtl %HELIX_WORKITEM_UPLOAD_ROOT%
copy /y WexLogFileOutput\*.jpg %HELIX_WORKITEM_UPLOAD_ROOT%
powershell -ExecutionPolicy Bypass .\CopyVisualTreeVerificationFiles.ps1

:SkipReruns

powershell -ExecutionPolicy Bypass Get-Process

echo %TIME%
powershell -ExecutionPolicy Bypass .\OutputSubResultsJsonFiles.ps1 te_original.wtl te_rerun.wtl te_rerun_multiple.wtl %testnameprefix%
powershell -ExecutionPolicy Bypass .\ConvertWttLogToXUnit.ps1 te_original.wtl te_rerun.wtl te_rerun_multiple.wtl testResults.xml %testnameprefix%
echo %TIME%

copy /y *_subresults.json %HELIX_WORKITEM_UPLOAD_ROOT%

type testResults.xml

echo %TIME%