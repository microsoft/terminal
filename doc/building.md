
# How to build OpenConsole

This repository uses [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) for some of its dependencies. To make sure submodules are restored or updated, be sure to run the following prior to building:

```shell
git submodule update --init --recursive
```

OpenConsole.sln may be built from within Visual Studio or from the command-line using a set of convenience scripts & tools in the **/tools** directory:

When using Visual Studio, be sure to set up the path for code formatting. To download the required clang-format.exe file, follow one of the building instructions below and run:
```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Get-Format
```
After, go to Tools > Options > Text Editor > C++ > Formatting and checking "Use custom clang-format.exe file" in Visual Studio and choose the clang-format.exe in the repository at /packages/clang-format.win-x86.10.0.0/tools/clang-format.exe by clicking "browse" right under the check box.

### Building in PowerShell

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

There are a few additional exported functions (look at their documentation for further details):

- `Invoke-OpenConsoleBuild` - builds the solution. Can be passed msbuild arguments.
- `Invoke-OpenConsoleTests` - runs the various tests. Will run the unit tests by default.
- `Start-OpenConsole` - starts Openconsole.exe from the output directory. x64 is run by default.
- `Debug-OpenConsole` - starts Openconsole.exe and attaches it to the default debugger. x64 is run by default.
- `Invoke-CodeFormat` - uses clang-format to format all c++ files to match our coding style.

### Building in Cmd

```shell
.\tools\razzle.cmd
bcz
```

There are also scripts for running the tests:
- `runut.cmd` - run the unit tests
- `runft.cmd` - run the feature tests
- `runuia.cmd` - run the UIA tests
- `runformat` - uses clang-format to format all c++ files to match our coding style.

## Running & Debugging

To debug the Windows Terminal in VS, right click on `CascadiaPackage` (in the Solution Explorer) and go to properties. In the Debug menu, change "Application process" and "Background task process" to "Native Only".

You should then be able to build & debug the Terminal project by hitting <kbd>F5</kbd>.

> 👉 You will _not_ be able to launch the Terminal directly by running the WindowsTerminal.exe. For more details on why, see [#926](https://github.com/microsoft/terminal/issues/926), [#4043](https://github.com/microsoft/terminal/issues/4043)

## Configuration Types

Openconsole has three configuration types:

- Debug
- Release
- AuditMode

AuditMode is an experimental mode that enables some additional static analysis from CppCoreCheck.

## Updating Nuget package references
Certain Nuget package references in this project, like `Microsoft.UI.Xaml`, must be updated outside of the Visual Studio NuGet package manager. This can be done using the snippet below.
> Note that to run this snippet, you need to use WSL as the command uses `sed`.
To update the version of a given package, use the following snippet

`git grep -z -l $PackageName | xargs -0 sed -i -e 's/$OldVersionNumber/$NewVersionNumber/g'`

where:
- `$PackageName` is the name of the package, e.g. Microsoft.UI.Xaml
- `$OldVersionNumber` is the version number currently used, e.g. 2.4.0-prerelease.200506002
- `$NewVersionNumber` is the version number you want to migrate to, e.g. 2.5.0-prerelease.200812002

Example usage:

`git grep -z -l Microsoft.UI.Xaml | xargs -0 sed -i -e 's/2.4.0-prerelease.200506002/2.5.0-prerelease.200812002/g'`

## Using .nupkg files instead of downloaded Nuget packages
If you want to use .nupkg files instead of the downloaded Nuget package, you can do this with the following steps:

1. Open the Nuget.config file and uncomment line 8 ("Static Package Dependencies")
2. Create the folder /dep/packages
3. Put your .nupkg files in /dep/packages
4. If you are using different versions than those already being used, you need to update the references as well. How to do that is explained under "Updating Nuget package references".
