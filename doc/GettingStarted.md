# Getting Started with *OpenConsole*

This guide serves as a walkthrough on how to get started contributing to the *OpenConsole* repository. We'll start by setting up the developer environment. Then, we'll provide a quick overview of the solution structure. Last, we'll list several online resources that may be helpful in developing for this project.

## Setting Up Your Developer Environment

First off, you must have Windows version 10.0.18362.9 or higher to actually run this project. If you don't have this version, feel free to download the source code and poke around. Just note, you won't be able to run and debug the project.

### Downloading the Source Code
Let's start off by downloading the source code. Open your favorite shell on your Windows device and `cd` to where you want to download the source code. Now, clone the repository:

```shell
git clone https://github.com/Console/WindowsTerminal.git
```

*OpenConsole* uses submodules for some of its dependencies. To make sure submodules are restored or updated, run the following command:

```shell
git submodule update --init --recursive
```

That's it! You now have a local copy of our code. Feel free to browse through it, but you won't be able to build it until you download a few dependencies.

### Dependencies

Using *Visual Studio Installer*, you'll be able to download any components necessary to build and modify *OpenConsole*. If you don't have *Visual Studio*, go to https://visualstudio.microsoft.com/ to download it. Within the installer, click on the *Modify* button. Select the following options:
 - In the **Workloads** Tab, under **Windows**, select **Desktop development with C++**
 - In the **Workloads** Tab, under **Windows**, select **Universal Windows Platform development**
   - In the column on the right-hand side, select **C++ Universal Windows Platform tools**
 - In the **Individual components** tab, under **SDKs, libraries, and frameworks**, select **Visual C++ ATL for x86 and x64**
 
 After selecting these options, click the *Modify* button on the bottom right corner to begin downloading these dependencies.

After these dependencies are downloaded and installed, open *OpenConsole.sln* using *Visual Studio*. Under *Solution Explorer*, ensure all of the Visual Studio Projects are loaded. If any are marked as unavailable, right-click on the project and select *Install Missing Features(s)* (or *Reload Project* if that option is not available).

## Exploring The *OpenConsole* Solution

Let's take a look at the structure for *OpenConsole*:
```shell
OpenConsole.sln
├───_Build Common
├───_Dependencies
├───_Tools
├───Conhost
├───Shared
|   ├───Buffer
|   ├───Rendering
|   └───Virtual Terminal
└───Terminal
```
 
 <!-- - `_Build Common`: -->
 <!-- - `_Dependencies`: -->
 <!-- - `_Tools`: -->
 - `Conhost`: All the components for the classic Windows Console
 - `Shared`: Components shared between classic Windows Console and Windows Terminal
   - `Buffer`: The text buffer used to store and navigate all the output data
   - `Rendering`: The renderer engine used to display portions of the buffer on the screen
   - `Virtual Terminal`: The PseudoTerminal used to parse and translate VT sequences
 - `Terminal`: All the components for the Windows Terminal

## Running the *OpenConsole* Projects

*OpenConsole* can be run on x64, x86, and ARM64 devices. Select whichever is compatible with your machine.

There are three projects you can build and run:
 - `VTApp`: test VT sequences
 - `OpenConsolePackage`: run ConHost in the classic Windows Console environment
 - `CascadiaPackage`: run ConHost in Windows Terminal

## Helpful Resources

|**Document in OpenConsole/doc**                            |**Description**                                                |
|-----------------------------------------------------------|---------------------------------------------------------------|
|**/cascadia/Windows-Terminal-Dev-Design.md**               |An overview of the architecture of Windows Terminal            |
|**/cascadia/19H1-Dev-Design-Spec.md**                      |A more detailed code-oriented description of various components|
|**/cascadia/Keybinding-spec.md**                           |How to introduce new keybindings                               |
|**/cascadia/TerminalSettings-spec.md**                     |How to interact with the settings                              |
|**building.md**                                            |How to build *OpenConsole*                                     |
|**Debugging.md**                                           |Debugging *OpenConsole*                                        |
|**STYLE.md**                                               |Our coding style                                               |

Additionally, if you are not familiar with some concepts used in this project, it might be worth taking a look at the following:
|**Concept**                                                |**Resource**                                                      |
|-----------------------------------------------------------|------------------------------------------------------------------|
|C++/WinRT                                                  |https://msdn.microsoft.com/en-us/magazine/mt745094.aspx           |
|XAML Islands                                               |https://channel9.msdn.com/Shows/Visual-Studio-Toolbox/XAML-Islands|
