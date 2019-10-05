# Welcome

This repository contains the source code for:

* [Windows Terminal](https://www.microsoft.com/en-us/p/windows-terminal-preview/9n0dx20hk701)
* The Windows console host (`conhost.exe`)
* Components shared between the two projects
* [ColorTool](https://github.com/Microsoft/Terminal/tree/master/src/tools/ColorTool)
* [Sample projects](https://github.com/Microsoft/Terminal/tree/master/samples) that show how to consume the Windows Console APIs

Related repositories include:

* [Console API Documentation](https://github.com/MicrosoftDocs/Console-Docs)

## Installing & running Windows Terminal

> 👉 Note: To run the Windows Terminal you'll need to be running Windows 10 1903 (build 18362) or later

### From the Microsoft Store

Install the Microsoft Terminal free from the Microsoft Store and it'll be continuously updated:

<a href='//www.microsoft.com/store/apps/9n0dx20hk701?cid=storebadge&ocid=badge'>
<img src='https://assets.windowsphone.com/85864462-9c82-451e-9355-a3d5f874397a/English_get-it-from-MS_InvariantCulture_Default.png' alt='English badge' width="284" height="104" />
</a>

### From this repository

Every public release of Windows Terminal is avaiable for manual download from [this repo's Releases page](https://github.com/microsoft/terminal/releases)

> ⚠ Note: If you manually install Terminal from [releases](https://github.com/microsoft/terminal/releases), Terminal will not auto-update when newer builds are published. Be sure to regularly check & install the latest Terminal release to receive all the latest fixes and improvements!

### Via Chocolatey (unofficial)

[Chocolatey](https://chocolatey.org) users can download and install the latest Terminal release by installing the `microsoft-windows-terminal` package:

```powershell
choco install microsoft-windows-terminal
```

To upgrade Windows Terminal using Chocolatey, run the following:

```powershell
choco upgrade microsoft-windows-terminal
```

> If you have any issues when installing/upgrading the package please go to the [package page](https://chocolatey.org/packages/microsoft-windows-terminal) and follow the [Chocolatey triage process](https://chocolatey.org/docs/package-triage-process)

---

## Project Build Status

Project|Build Status
---|---
Terminal|[![Build Status](https://dev.azure.com/ms/Terminal/_apis/build/status/Terminal%20CI?branchName=master)](https://dev.azure.com/ms/Terminal/_build?definitionId=136)
ColorTool|![](https://microsoft.visualstudio.com/_apis/public/build/definitions/c93e867a-8815-43c1-92c4-e7dd5404f1e1/17023/badge)

---

## Windows Terminal v1.0 Roadmap

The plan for delivering Windows Terminal v1.0 [is described here](/doc/terminal-v1-roadmap.md), and will be updated as the project proceeds.

---

## Terminal & Console Overview

Please take a few minutes to review the overview below before diving into the code:

### Windows Terminal

Windows Terminal is a new, modern, feature-rich, productive terminal application for command-line users. It includes many of the features most frequently requested by the Windows command-line community including support for tabs, rich text, globalization, configurability, theming & styling, and more.

The Terminal will also need to meet our goals and measures to ensure it remains fast and efficient, and doesn't consume vast amounts of memory or power.

### The Windows Console host

The Windows Console host, `conhost.exe`, is Windows' original command-line user experience.

The Console is the app you see and interact with when you run Cmd, PowerShell, wsl, or any other command-line application on Windows.

Console also implements Windows' command-line infrastructure, hosting the Windows Console API, input engine, rendering engine, and user preferences. 

> The Console's code in this repository is the actual source from which the `conhost.exe` in Windows itself is built.

Since taking ownership of the Windows command-line in 2014, the team has added several new features to the Console, including window transparency, line-based selection, support for [ANSI / Virtual Terminal sequences](https://en.wikipedia.org/wiki/ANSI_escape_code), [24-bit color](https://devblogs.microsoft.com/commandline/24-bit-color-in-the-windows-console/), a [Pseudoconsole ("ConPTY")](https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/), and more.

However, because Windows Console's primary goal is to maintain backward compatibility, we've been unable to add many of the features the community has been asking for (and which we've been wanting to add) for the last several years including tabs, unicode text, and emoji.

These limitations led us to create the new Windows Terminal.

### Shared Components

While overhauling Windows Console, we modernized its codebase considerably: We've cleanly separated logical entities into modules and classes, introduced some key extensibility points, replaced several old, home-grown collections and containers with safer, more efficient [STL containers](https://docs.microsoft.com/en-us/cpp/standard-library/stl-containers?view=vs-2019), and made the code simpler and safer by using Microsoft's [WIL](https://github.com/Microsoft/wil).

This overhaul resulted in several of Console's key components being available for re-use in any terminal implementation on Windows. These components include a new DirectWrite-based text layout and rendering engine, a text buffer capable of storing both UTF-16 and UTF-8, and a VT parser/emitter, and more.

## Building the new terminal

When we started building the new Windows Terminal application, we explored and evaluated several approaches and technology stacks. We ultimately decided that our goals would be best met by sticking with C++ and reusing several of the aforementioned modernized components. Further, we realized that this would allow us to build much of the Terminal's core itself as a reusable Windows UI control that others can incorporate into their applications.

The result of this work is contained within this repo and delivered as the Windwos Terminal application you can dowload from the Microsoft Store, or directly from this repo.

---

## Resources
For more information about Windows Terminal, you may find some of these resources useful and interesting:
* [Command-Line Blog](https://devblogs.microsoft.com/commandline)
* [Command-Line Backgrounder Blog Series]()
* Windows Terminal Launch: [Terminal "Sizzle Video"](https://www.youtube.com/watch?v=8gw0rXPMMPE&list=PLEHMQNlPj-Jzh9DkNpqipDGCZZuOwrQwR&index=2&t=0s)
* Windows Terminal Launch: [Build 2019 Session](https://www.youtube.com/watch?v=KMudkRcwjCw)
* Run As Radio: [Show 645 - Windows Terminal with Richard Turner](http://www.runasradio.com/Shows/Show/645)
* Azure Devops Podcast: [Episode 54 - Kayla Cinnamon and Rich Turner on DevOps on the Windows Terminal](http://azuredevopspodcast.clear-measure.com/kayla-cinnamon-and-rich-turner-on-devops-on-the-windows-terminal-team-episode-54)
---

## FAQ

## I built and ran the new Terminal, but I just see a blank window app

Make sure your Terminal us built for your PC's processor architecture:

For example, if your PC runs 64-bit Windows, set your Solution Platform to x64: In Visual Studio, go to Settings -> System -> About (or Win+X -> System) and under `Device specifications` check for the  `System type`.

## I built and ran the new Terminal, but it looks just like the old console

Make sure you're building & running the `CascadiaPackage` project in Visual Studio.

## I tried running WindowsTerminal.exe and it crashes

* Don't try to run `WindowsTerminal.exe` directly. Make sure to build & deploy `CascadiaPackage` from Visual Studio, and run the Windows Terminal (Dev Build) app.
* Make sure you're running Windows 10 1903 or later - the Windows Terminal **REQUIRES** features from the latest Windows release.

---

## Documentation

All project documentation is located in the `./doc` folder. If you would like to contribute to the documentation, please submit a pull request.

## Contributing

We are excited to work alongside you, our amazing community, to build and enhance Windows Terminal\!

***BEFORE you start work on a feature/fix***, please read & follow our [Contributor's Guide](https://github.com/microsoft/terminal/blob/master/doc/contributing.md) to help avoid any wasted or duplicate effort.

## Communicating with the Team

The easiest way to communicate with the team is via GitHub issues.

Please file new issues, feature requests and suggestions, but **DO search for similar open/closed pre-existing issues before creating a new issue.**

If you would like to ask a question that you feel doesn't warrant an issue (yet), please reach out to us via Twitter:

* Kayla Cinnamon, Program Manager: [@cinnamon\_msft](https://twitter.com/cinnamon_msft)
* Rich Turner, Program Manager: [@richturn\_ms](https://twitter.com/richturn_ms)
* Dustin Howett, Engineering Lead: [@dhowett](https://twitter.com/DHowett)
* Michael Niksa, Senior Developer: [@michaelniksa](https://twitter.com/MichaelNiksa)

## Developer Guidance

## Build Prerequisites

* You must be running Windows 1903 (build >= 10.0.18362.0) or later in order to run Windows Terminal
* You must [enable Developer Mode in the Windows Settings app](https://docs.microsoft.com/en-us/windows/uwp/get-started/enable-your-device-for-development) to locally install and run Windows Terminal
* You must have the [Windows 10 1903 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk) installed.
* You must have at least [VS 2019](https://visualstudio.microsoft.com/downloads/) installed
* You must install the following Workloads via the VS Installer. Note: Opening the solution in VS 2019 will [prompt you to install missing components automatically](https://devblogs.microsoft.com/setup/configure-visual-studio-across-your-organization-with-vsconfig/):
  * Desktop Development with C++
  * Universal Windows Platform Development
  * **The following Individual Components**
    * C++ (v142) Universal Windows Platform Tools

## Building the Code

This repository uses [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) for some of its dependencies. To make sure submodules are restored or updated, be sure to run the following prior to building:

```shell
git submodule update --init --recursive
```

OpenConsole.sln may be built from within Visual Studio or from the command-line using a set of convenience scripts & tools in the **/tools** directory.:

### Building in PowerShell

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

### Building in Cmd

```shell
.\tools\razzle.cmd
bcz
```

### Debugging Terminal

To debug Terminal in VS, right click on `CascadiaPackage` (in the Solution Explorer) and go to properties, in the Debug menu, change "Application process" and "Background task process" to "Native Only".

You should then be able to build & debug the Terminal project by hitting <kbd>F5</kbd>

### Coding Guidance

Please review these brief docs below about our coding practices.

> 👉 If you find something missing from these docs, feel free to contribute to any of our documentation files anywhere in the repository (or write some new ones!)

This is a work in progress as we learn what we'll need to provide people in order to be effective contributors to our project.

* [Coding Style](https://github.com/Microsoft/Terminal/blob/master/doc/STYLE.md)
* [Code Organization](https://github.com/Microsoft/Terminal/blob/master/doc/ORGANIZATION.md)
* [Exceptions in our legacy codebase](https://github.com/Microsoft/Terminal/blob/master/doc/EXCEPTIONS.md)
* [Helpful smart pointers and macros for interfacing with Windows in WIL](https://github.com/Microsoft/Terminal/blob/master/doc/WIL.md)

---

## Code of Conduct

Please help us keep this repository clean, inclusive, and fun!

This project has adopted the [Microsoft Open Source Code of Conduct][conduct-code].
For more information see the [Code of Conduct FAQ][conduct-FAQ] or contact [opencode@microsoft.com][conduct-email] with any additional questions or comments.

[conduct-code]: https://opensource.microsoft.com/codeofconduct/
[conduct-FAQ]: https://opensource.microsoft.com/codeofconduct/faq/
[conduct-email]: mailto:opencode@microsoft.com
