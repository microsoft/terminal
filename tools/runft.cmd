@echo off

rem Run the console feature tests.

call %TAEF% ^
    %OPENCON%\bin\%ARCH%\Debug\ConHost.Feature.Tests.dll ^
    %*
