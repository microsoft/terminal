# Welcome\! 
#### This repository contains the source code for:

  * Windows Terminal
  * The Windows console host (`conhost.exe`)
  * Components shared between the two projects
  * [ColorTool](https://github.com/Microsoft/Terminal/tree/master/src/tools/ColorTool)
  * [Sample projects](https://github.com/Microsoft/Terminal/tree/master/samples) that show how to consume the Windows Console APIs
  
#### Other related repositories include:
  * [Console API Documentation](https://github.com/MicrosoftDocs/Console-Docs)

### Build Status

Project|Build Status
---|---
Terminal|[![Build Status](https://dev.azure.com/ms/Terminal/_apis/build/status/Terminal%20CI?branchName=master)](https://dev.azure.com/ms/Terminal/_build?definitionId=136)
ColorTool|![](https://microsoft.visualstudio.com/_apis/public/build/definitions/c93e867a-8815-43c1-92c4-e7dd5404f1e1/17023/badge)

# Terminal & Console Overview

Please take a few minutes to review the overview below before diving into the code:

## Windows Terminal

Windows Terminal is a new, modern, feature-rich, productive terminal application for command-line users. It includes many of the features most frequently requested by the Windows command-line community including support for tabs, rich text, globalization, configurability, theming & styling, and more.

The Terminal will also need to meet our goals and measures to ensure it remains fast, and efficient, and doesn't consume vast amounts of memory or power.

## The Windows console host

The Windows console host, `conhost.exe`, is Windows' original command-line user experience. It implements Windows' command-line infrastructure, and is responsible for hosting the Windows Console API, input engine, rendering engine, and user preferences. The console host code in this repository is the actual source from which the `conhost.exe` in Windows itself is built.

Console's primary goal is to remain backwards-compatible with existing console subsystem applications.

Since assuming ownership of the Windows command-line in 2014, the team has added several new features to the Console, including window transparency, line-based selection, support for [ANSI / Virtual Terminal sequences](https://en.wikipedia.org/wiki/ANSI_escape_code), [24-bit color](https://devblogs.microsoft.com/commandline/24-bit-color-in-the-windows-console/), a [Pseudoconsole ("ConPTY")](https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/), and more.

However, because the Console's primary goal is to maintain backward compatibility, we've been unable to add many of the features the community has been asking for, and which we've been wanting to add for the last several years--like tabs!

These limitations led us to create the new Windows Terminal.

## Shared Components

While overhauling the Console, we've modernized its codebase considerably. We've cleanly separated logical entities into modules and classes, introduced some key extensibility points, replaced several old, home-grown collections and containers with safer, more efficient [STL containers](https://docs.microsoft.com/en-us/cpp/standard-library/stl-containers?view=vs-2019), and made the code simpler and safer by using Microsoft's [WIL](https://github.com/Microsoft/wil) header library.

This overhaul work resulted in the creation of several key components that would be useful for any terminal implementation on Windows, including a new DirectWrite-based text layout and rendering engine, a text buffer capable of storing both UTF-16 and UTF-8, and a VT parser/emitter.

## Building a new terminal

When we started building the new terminal application, we explored and evaluated several approaches and technology stacks. We ultimately decided that our goals would be best met by sticking with C++ and sharing the aforementioned modernized components, placing them atop the modern Windows application platform and UI framework.

Further, we realized that this would allow us to build the terminal's renderer and input stack as a reusable Windows UI control that others can incorporate into their applications.

# FAQ

## Where can I download Windows Terminal?

### The Windows Terminal preview can be downloaded from the Microsoft Store.

[https://www.microsoft.com/en-us/p/windows-terminal-preview/9n0dx20hk701](https://www.microsoft.com/en-us/p/windows-terminal-preview/9n0dx20hk701)

## I built and ran the new Terminal, but I just get a blank window app!

Make sure you are building for your computer's architecture. If your box has a 64-bit Windows, change your Solution Platform to x64.  
To check your OS architecture go to Settings -> System -> About (or Win+X -> System) and under `Device specifications` check for the  `System type`.

## I built and ran the new Terminal, but it looks just like the old console! What gives?

Firstly, make sure you're building & deploying `CascadiaPackage` in Visual Studio, _NOT_ `Host.EXE`. `OpenConsole.exe` is just `conhost.exe`, the same old console you know and love. `opencon.cmd` will launch `openconsole.exe`, and unfortunately, `openterm.cmd` is currently broken.

Secondly, try pressing <kbd>Ctrl</kbd> + <kbd>T</kbd>. The tabs are hidden when you only have one tab by default. In the future, the UI will be dramatically different, but for now, the defaults are _supposed_ to look like the console defaults.

## I tried running WindowsTerminal.exe and it crashes!

* Don't try to run it unpackaged. Make sure to build & deploy `CascadiaPackage` from Visual Studio, and run the Windows Terminal (Dev Build) app.
* Make sure you're on the right version of Windows. You'll need to be on Insider's builds, or wait for the 1903 release, as the Windows Terminal **REQUIRES** features from the latest Windows release.

# Getting Started

## Debugging

* To debug in VS, right click on CascadiaPackage (from VS Solution Explorer) and go to properties, in the Debug menu, change "Application process" and "Background task process" to "Native Only".

## Contributing

We are excited to work alongside you, our amazing community, to build and enhance Windows Terminal\!

We ask that **before you start work on a feature that you would like to contribute**, please read our [Contributor's Guide](https://github.com/microsoft/terminal/blob/master/doc/contributing.md). We will be happy to work with you to figure out the best approach, provide guidance and mentorship throughout feature development, and help avoid any wasted or duplicate effort.

> 👉 **Remember\!** Your contributions may be incorporated into future versions of Windows\! Because of this, all pull requests will be subject to the same level of scrutiny for quality, coding standards, performance, globalization, accessibility, and compatibility as those of our internal contributors.

> ⚠ **Note**: The Command-Line Team is actively working out of this repository and will be periodically re-structuring the code to make it easier to comprehend, navigate, build, test, and contribute to, so **DO expect significant changes to code layout on a regular basis**.

## Documentation

All documentation is located in the `./doc` folder. If you would like to contribute to the documentation, please submit a pull request.

## Communicating with the Team

The easiest way to communicate with the team is via GitHub issues. Please file new issues, feature requests and suggestions, but **DO search for similar open/closed pre-existing issues before you do**.

Please help us keep this repository clean, inclusive, and fun\! We will not tolerate any abusive, rude, disrespectful or inappropriate behavior. Read our [Code of Conduct](https://opensource.microsoft.com/codeofconduct/) for more details.

If you would like to ask a question that you feel doesn't warrant an issue (yet), please reach out to us via Twitter:

  * Rich Turner, Program Manager: [@richturn\_ms](https://twitter.com/richturn_ms)

  * Dustin Howett, Engineering Lead: [@dhowett](https://twitter.com/DHowett)
  
  * Michael Niksa, Senior Developer: [@michaelniksa](https://twitter.com/MichaelNiksa)

  * Kayla Cinnamon, Program Manager (especially for UX issues): [@cinnamon\_msft](https://twitter.com/cinnamon_msft)

# Developer Guidance

## Build Prerequisites

* You must be running Windows 1903 (build >= 10.0.18362.0) or above in order to run Windows Terminal.
* You must have the [1903 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk) (build 10.0.18362.0) installed.
* You must have at least [VS 2017](https://visualstudio.microsoft.com/downloads/) installed.
* You must install the following Workloads via the VS Installer. If you're running VS 2019, opening the solution will [prompt you to install missing components automatically](https://devblogs.microsoft.com/setup/configure-visual-studio-across-your-organization-with-vsconfig/).
  - Desktop Development with C++
  - Universal Windows Platform Development
    - Also install the following Individual Component:
      - C++ (v141) Universal Windows Platform Tools

* You must also [enable Developer Mode in the Windows Settings app](https://docs.microsoft.com/en-us/windows/uwp/get-started/enable-your-device-for-development) to locally install and run the Terminal app.


## Building the Code

This repository uses [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) for some of its dependencies. To make sure submodules are restored or updated, be sure to run the following prior to building:

```shell
git submodule update --init --recursive
```

OpenConsole.sln may be built from within Visual Studio or from the command-line using MSBuild. To build from the command line, find your shell below.

### PowerShell

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

### CMD

```shell
.\tools\razzle.cmd
bcz
```

We've provided a set of convenience scripts as well as [README](./tools/README.md) in the **/tools** directory to help automate the process of building and running tests.

## Coding Guidance

Please review these brief docs below relating to our coding standards etc.

> 👉 If you find something missing from these docs, feel free to contribute to any of our documentation files anywhere in the repository (or make some new ones\!)

This is a work in progress as we learn what we'll need to provide people in order to be effective contributors to our project.

  - [Coding Style](https://github.com/Microsoft/Terminal/blob/master/doc/STYLE.md)
  - [Code Organization](https://github.com/Microsoft/Terminal/blob/master/doc/ORGANIZATION.md)
  - [Exceptions in our legacy codebase](https://github.com/Microsoft/Terminal/blob/master/doc/EXCEPTIONS.md)
  - [Helpful smart pointers and macros for interfacing with Windows in WIL](https://github.com/Microsoft/Terminal/blob/master/doc/WIL.md)

# Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct][conduct-code].
For more information see the [Code of Conduct FAQ][conduct-FAQ] or contact [opencode@microsoft.com][conduct-email] with any additional questions or comments.

[conduct-code]: https://opensource.microsoft.com/codeofconduct/
[conduct-FAQ]: https://opensource.microsoft.com/codeofconduct/faq/
[conduct-email]: mailto:opencode@microsoft.com
