
# How to build Openconsole

Openconsole can be built with Visual Studio or from the command line. There are build scripts for both cmd and PowerShell in /tools.

When using Visual Studio, be sure to set up the path for code formatting. This can be done in Visual Studio by going to Tools > Options > Text Editor > C++ > Formatting and checking "Use custom clang-format.exe file" and choosing the clang-format.exe in the repository at /dep/llvm/clang-format.exe by clicking "browse" right under the check box.

## Building with cmd

The cmd scripts are set up to emulate a portion of the OS razzle build environment. razzle.cmd is the first script that should be run. bcz.cmd will build clean and bz.cmd should build incrementally.

There are also scripts for running the tests:
- `runut.cmd` - run the unit tests
- `runft.cmd` - run the feature tests
- `runuia.cmd` - run the UIA tests
- `runformat` - uses clang-format to format all c++ files to match our coding style.

## Build with Powershell

Openconsole.psm1 should be loaded with `Import-Module`. From there `Set-MsbuildDevEnvironment` will set up environment variables required to build. There are a few exported functions (look at their documentation for further details):

- `Invoke-OpenConsolebuild` - builds the solution. Can be passed msbuild arguments.
- `Invoke-OpenConsoleTests` - runs the various tests. Will run the unit tests by default.
- `Start-OpenConsole` - starts Openconsole.exe from the output directory. x64 is run by default.
- `Debug-OpenConsole` - starts Openconsole.exe and attaches it to the default debugger. x64 is run by default.
- `Invoke-CodeFormat` - uses clang-format to format all c++ files to match our coding style.

## Configuration Types

Openconsole has three configuration types:

- Debug
- Release
- AuditMode

AuditMode is an experimental mode that enables some additional static analysis from CppCoreCheck.
