@echo off

@rem This script can be used for running the unit tests just the same way
@rem    they'll run on VSO.

setlocal
set VSTEST_PATH="C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe"

set test_cmd=%VSTEST_PATH% ^
 "%OPENCON%\bin\%ARCH%\%_LAST_BUILD_CONF%\ConAdapter.Unit.Tests.dll" ^
 "%OPENCON%\bin\%ARCH%\%_LAST_BUILD_CONF%\Conhost.Interactivity.Win32.Unit.Tests.dll" ^
 "%OPENCON%\bin\%ARCH%\%_LAST_BUILD_CONF%\Conhost.Unit.Tests.dll" ^
 "%OPENCON%\bin\%ARCH%\%_LAST_BUILD_CONF%\ConParser.Unit.Tests.dll" ^
 /Settings:"%OPENCON%\src\unit.tests.%ARCH%.runsettings" ^
 /EnableCodeCoverage ^
 /logger:trx ^
 /TestAdapterPath:"%OPENCON%" ^
 %*

@rem Note: You can't use the same /name*test* parameters to regex find tests with this tester.
@rem you instead need to use /Tests:[<test name>]
@rem ex: vso_ut /Tests:DtorTest
@rem will match any test with "DtorTest" in the name. Note that Test Discovery will take FOREVER.

echo Starting tests with the following commandline in a new window:
echo ```
echo %test_cmd%
echo ```
start cmd /k "%test_cmd%"
