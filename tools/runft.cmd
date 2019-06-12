@echo off

rem Run the console feature tests.
rem Keep this file in sync with tests.xml

call %TAEF% ^
    %OPENCON%\bin\%ARCH%\Debug\ConHost.Feature.Tests.dll ^
    %*
