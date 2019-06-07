@echo off

rem Run the console UI Automation tests.
rem Keep this file in sync with tests.xml

rem Get AppDriver going first... You'll have to close it yourself.

start %OPENCON%\dep\WinAppDriver\WinAppDriver.exe

rem Then run the tests.

call %TAEF% ^
    %OPENCON%\bin\%ARCH%\Debug\Conhost.UIA.Tests.dll ^
    /p:VtApp=%OPENCON%\bin\%ARCH%\Debug\VtApp.exe ^
    %*
