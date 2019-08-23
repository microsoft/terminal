@echo off

rem Run the console unit tests.
rem Keep this file in sync with tests.xml

call %TAEF% ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Conhost.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\TextBuffer.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Terminal.Core.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Conhost.Interactivity.Win32.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\ConParser.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\ConAdapter.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Types.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_TerminalApp\Terminal.App.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\LocalTests_TerminalApp\TerminalApp.LocalTests.dll ^
    %*
