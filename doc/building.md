
# How to build OpenConsole

## Prerequisites

Make sure your machine matches the [Developer Guidance in the README](../README.md#developer-guidance). The most common source of mysterious build failures is an out-of-date toolchain — as of mid-2026 this repository requires **Visual Studio 2026 (18.6 or later)**, which provides the v145 platform toolset, and the **Windows 11 SDK 10.0.26100**. Visual Studio 2022 can no longer build `main`; see [Troubleshooting](#troubleshooting-command-line-builds) below for what that failure looks like.

The repository does not use git submodules; a plain `git clone` gives you everything. NuGet packages are restored automatically by the build scripts (using the `nuget.exe` bundled at `dep/nuget/nuget.exe`) or by Visual Studio.

OpenConsole.slnx may be built from within Visual Studio or from the command-line using a set of convenience scripts & tools in the **/tools** directory:

When using Visual Studio, be sure to set up the path for code formatting. To download the required clang-format.exe file, follow one of the building instructions below and run:
```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Get-Format
```
After, go to Tools > Options > Text Editor > C++ > Formatting and check "Use custom clang-format.exe file" in Visual Studio and choose the clang-format.exe in the repository at /packages/clang-format.win-x86.10.0.0/tools/clang-format.exe by clicking "browse" right under the check box.

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

### Troubleshooting command-line builds

* **`Invalid input 'OpenConsole.slnx'. The file type was not recognized.`** — your checkout has an old `dep/nuget/nuget.exe` that predates `.slnx` solution support. Pull the latest `main` (the bundled nuget.exe understands `.slnx`), or download the [latest nuget.exe](https://dist.nuget.org/win-x86-commandline/latest/nuget.exe) over `dep/nuget/nuget.exe`.
* **vcpkg: `Unable to find a valid Visual Studio instance` ... `with toolset version v145`** — you are building with Visual Studio 2022 (or older). This repository requires Visual Studio 2026; the v145 platform toolset does not exist in VS 2022.
* **`Xaml Internal Error error WMC9999`** on sources you haven't touched — another symptom of building with a pre-2026 Visual Studio; the XAML compiler crashes rather than reporting the real problem. Install VS 2026 and rebuild.
* **`C3859: Failed to create virtual memory for PCH` / `C1076: internal heap limit reached` / MSBuild hanging after an `MSB8084 ... OutOfMemoryException`** — the default fully-parallel build can exhaust commit memory; several projects here compile multi-GB precompiled headers concurrently. Rerun with reduced parallelism, e.g.:

  ```powershell
  Invoke-OpenConsoleBuild /m:2 /p:CL_MPCount=2
  ```

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

## Updating Nuget package references - Globally versioned
Most Nuget package references in this project are centralized in a single configuration so that there is a single canonical version for everything.  This canonical version is restored before builds by the build pipeline, environment initialization scripts, or Visual Studio (as appropriate).

The canonical version numbers are defined in dep/nuget/packages.config.  That defines what will be downloaded by nuget.exe.  Most Nuget packages also have a .props and/or .targets file that must be imported by every project that consumes it.  Those import statements are consolidated in:
- src/common.nugetversions.props
- src/common.nugetversions.targets

When a globally managed version changes all three of those files must be changed in unison.

## Updating Nuget package references - Locally versioned
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


## Building the Terminal package from the commandline

The Terminal is bundled as an `.msix`, which is produced by the `CascadiaPackage.wapproj` project. To build that project from the commandline, you can run the following (from a window you've already run `tools\razzle.cmd` in):

```cmd
"%msbuild%" "%OPENCON%\OpenConsole.slnx" /p:Configuration=%_LAST_BUILD_CONF% /p:Platform=%ARCH% /p:AppxSymbolPackageEnabled=false /t:Terminal\CascadiaPackage /m
```

This takes quite some time, and only generates an `msix`. It does not install the msix. To deploy the package (requires [Developer Mode](https://docs.microsoft.com/en-us/windows/uwp/get-started/enable-your-device-for-development) to register an unsigned loose layout):

```powershell
# If you haven't already:
Import-Module .\tools\OpenConsole.psm1;
Set-MsBuildDevEnvironment;

# The Set-MsBuildDevEnvironment call is needed for finding the path to
# makeappx. It also takes a little longer to run. If you're sticking in powershell, best to do that.

# The AppPackages folder name and msix name include the build configuration for
# Debug builds (CascadiaPackage_0.0.1.0_x64_Debug_Test / ..._Debug.msix), but
# NOT for Release builds (CascadiaPackage_0.0.1.0_x64_Test / ..._x64.msix).
Set-Location -Path src\cascadia\CascadiaPackage\AppPackages\CascadiaPackage_0.0.1.0_x64_Debug_Test;
if ((Get-AppxPackage -Name 'WindowsTerminalDev*') -ne $null) {
Remove-AppxPackage 'WindowsTerminalDev_0.0.1.0_x64__8wekyb3d8bbwe'
};
New-Item ..\loose -Type Directory -Force;
makeappx unpack /v /o /p .\CascadiaPackage_0.0.1.0_x64_Debug.msix /d ..\loose\;
Add-AppxPackage -Path ..\loose\AppxManifest.xml -Register -ForceUpdateFromAnyVersion -ForceApplicationShutdown
```

Once registered, the dev build is launchable as `wtd.exe` (or "Windows Terminal Dev" in Start).

Building the package from VS generates the loose layout to begin with, and then registers the loose manifest, skipping the msix step. It's a lot faster than the commandline inner loop here, unfortunately.

### Deploying with DeployAppRecipe

The following command can be used to build the terminal package, and then deploy it — locate `DeployAppRecipe.exe` under your Visual Studio installation's `Common7\IDE` directory (e.g. `%VSINSTALLDIR%Common7\IDE\DeployAppRecipe.exe` from a developer prompt):

```cmd
pushd %OPENCON%\src\cascadia\CascadiaPackage
bx
"%VSINSTALLDIR%\Common7\IDE\DeployAppRecipe.exe" bin\%ARCH%\%_LAST_BUILD_CONF%\CascadiaPackage.build.appxrecipe
popd
```

The `bx` will build just the Terminal package, critically, populating the `CascadiaPackage.build.appxrecipe` file. Once that's been built, then the `DeployAppRecipe.exe` command can be used to deploy a loose layout in the same way that Visual Studio does.

Notably, this method of building the Terminal package can't leverage the FastUpToDate check in Visual Studio, so the builds end up being considerably slower for the whole package, as cppwinrt does a lot of work before confirming that it's up to date and doing nothing.


### Are you seeing `DEP0700: Registration of the app failed`?

Once in a blue moon, I get a `DEP0700: Registration of the app failed.
[0x80073CF6] error 0x80070020: Windows cannot register the package because of an
internal error or low memory.` when trying to deploy in VS. For us, that can
happen if the `OpenConsoleProxy.dll` gets locked up, in use by some other
terminal package.

Doing the equivalent command in powershell can give us more info:

```pwsh
Add-AppxPackage -register "Z:\dev\public\OpenConsole\src\cascadia\CascadiaPackage\bin\x64\Debug\AppX\AppxManifest.xml"
```

That'll suggest `NOTE: For additional information, look for [ActivityId]
dbf551f1-83d0-0007-43e7-9cded083da01 in the Event Log or use the command line
Get-AppPackageLog -ActivityID dbf551f1-83d0-0007-43e7-9cded083da01`. So do that:

```pwsh
Get-AppPackageLog -ActivityID dbf551f1-83d0-0007-43e7-9cded083da01
```

which will give you a lot of info. In my case, that revealed that the platform
couldn't delete the packaged com entries. The key line was: `AppX Deployment
operation failed with error 0x0 from API Logging data because access was denied
for file:
C:\ProgramData\Microsoft\Windows\AppRepository\Packages\WindowsTerminalDev_0.0.1.0_x64__8wekyb3d8bbwe,
user SID: S-1-5-18`

Take that path, and
```pwsh
sudo start C:\ProgramData\Microsoft\Windows\AppRepository\Packages\WindowsTerminalDev_0.0.1.0_x64__8wekyb3d8bbwe
```

(use `sudo`, since the path is otherwise locked down). From there, go into the
`PackagedCom` folder, and open [File
Locksmith](https://learn.microsoft.com/en-us/windows/powertoys/file-locksmith)
(or Process Explorer, if you're more familiar with that) on
`OpenConsoleProxy.dll`. Just go ahead and immediately re-launch it as admin,
too. That should list off a couple terminal processes that are just hanging
around. Go ahead and end them all. You should be good to deploy again after
that.
