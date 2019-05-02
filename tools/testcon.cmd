@echo off

rem Run the console tests.

rem I couldn't come up with a good way to combine the ft and ut lists
rem    automatically, so this is manually updated for now :(

rem NOTE: UIA tests are still seperate because they takeover your mouse/keyboard. 
rem       Run them on your own.

call %TAEF% ^
    %OPENCON%\bin\%ARCH%\Debug\ConHost.Api.Tests.dll ^
    %OPENCON%\bin\%ARCH%\Debug\ConHost.Resize.Tests.dll ^
    %OPENCON%\bin\%ARCH%\Debug\Conhost.Unit.Tests.dll ^
    %OPENCON%\bin\%ARCH%\Debug\ConHost.CJK.Tests.dll ^
    %OPENCON%\bin\%ARCH%\Debug\ConParser.Unit.Tests.dll ^
    %OPENCON%\bin\%ARCH%\Debug\ConAdapter.Unit.Tests.dll ^
    %*
