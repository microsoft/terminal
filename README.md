# Welcome to the Console Project!

This project is currently controlled by the Windows Developer Platform Tools & Runtimes' Open Source Software team (*WDG > DEP > DART > OSS*).

Our team can be reached at `dartcon@microsoft.com`.

The code is stored at <https://microsoft.visualstudio.com/Dart/_git/OpenConsole>.

The area path within the Microsoft.VisualStudio.com database for our Work Items is `OS\CORE-OS Core\DEP-Developer Ecosystem Platform\DART-Developer Tools and Runtimes\Open Source Software\Console`.

## Jumping In

To get started, feel free to read up on some of our documentation on the way we get things done and hop in.

Make a branch off of `dev/main` for yourself of the pattern `dev/myalias/foo` and feel free to push it to the server to get automatic builds and unit test runs.

Choose a bit of code to clean up, try to add a new feature, or improve something that you try to use every day.

When you are ready, use the [web portal](https://microsoft.visualstudio.com/Dart/_git/OpenConsole/pullrequests) to send a pull request into our `dev/main` branch and we'll be happy to help you get your code in line with the rest of the console.

## Building

OpenConsole uses submodules for some of its dependencies. To make sure submodules are restored or updated:

```
git submodule update --init --recursive
```

OpenConsole.sln may be built from within Visual Studio or from the command line using msbuild. To build from the command line:

```
nuget.exe restore OpenConsole.sln
msbuild.exe OpenConsole.sln
```

We provide a set of convienence scripts in the /tools directory to help automate the process of building and running tests.

## Assorted Notes

Here's some assorted notes on the way we do things. If you learn something about how we do things, feel free to contribute to any of our documentation files anywhere in the repository (or make some new ones!) This is a work in progress as we try to learn what we'll need to train people on in order to be effective contributors to our project. We're pretty blind to these things after staring at this code for so long... so mind the gaps and ask us plenty of questions!

* [Coding Style](./doc/STYLE.md)
* [Code Organization](./doc/ORGANIZATION.md)
* [Exceptions in our legacy codebase](./doc/EXCEPTIONS.md)
* [Helpful smart pointers and macros for interfacing with Windows in WIL](./doc/WIL.md)
