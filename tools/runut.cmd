@echo off

rem Run the console unit tests.
rem Keep this file in sync with tests.xml

rem The TerminalApp.LocalTests are actually run from the TestHostApp path.
rem That's a cppwinrt project, that doesn't use %PLATFORM% in its path when the
rem platform is Win32/x86.
rem set a helper for us to find that test

set _TestHostAppPath=%OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\TestHostApp
if "%PLATFORM%" == "Win32" (
    set _TestHostAppPath=%OPENCON%\bin\%_LAST_BUILD_CONF%\TestHostApp
)

%TAEF% ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Conhost.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\TextBuffer.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_TerminalCore\Terminal.Core.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Conhost.Interactivity.Win32.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\ConParser.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\ConAdapter.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\Types.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\til.unit.tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_TerminalApp\Terminal.App.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_Remoting\Remoting.Unit.Tests.dll ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_Control\Control.Unit.Tests.dll ^
    %_TestHostAppPath%\TerminalApp.LocalTests.dll ^
    %*

set _EarlyTestFail=%ERRORLEVEL%

%OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_SettingsModel\te.exe ^
    %OPENCON%\bin\%PLATFORM%\%_LAST_BUILD_CONF%\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll ^
    %*

if %_EarlyTestFail% NEQ 0 (
    exit /b %_EarlyTestFail%
)

exit /b %ERRORLEVEL%
