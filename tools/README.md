# OpenConsole Tools

These are a collection of tools and scripts to make your life building the
OpenConsole project easier. Many of them are designed to be functional clones of
tools that we used to use when developing inside the Windows build system.

## Razzle

This is a script that quickly sets up your environment variables so that these
tools can run easily. It's named after another script used by Windows developers
to similar effect.
 - It adds msbuild to your path.
 - It adds the tools directory to your path as well, so all these scripts are
 easily available.
 - It executes `\tools\.razzlerc.cmd` to add any other personal configuration to
 your environment as well, or creates one if it doesn't exist.
 - It sets up the default build configuration to be 'Debug'. If you'd like to
 manually specify a build configuration, pass the parameter `dbg` for Debug, and
 `rel` for Release.

## bcz

`bcz` can quick be used to clean and build the project. By default, it builds
the `%DEFAULT_CONFIGURATION%` configuration, which is `Debug` if you use `razzle.cmd`.

 - `bcz dbg` can be used to manually build the Debug configuration.
 - `bcz rel` can be used to manually build the Release configuration.


## opencon (and openbash, openps)

`opencon` can be used to launch the **last built** OpenConsole binary. If given an
argument, it will try and run that program in the launched window. Otherwise it
will default to cmd.exe.

`openbash` is similar, it immediately launches bash.exe (the Windows Subsystem
for Linux entrypoint) in your `~` directory.

Likewise, `openps` launches powershell.

## runformat

`runformat` will format the c++ code to match our coding style.

## testcon, runut, runft
`runut` will automatically run all of the unit tests through TAEF. `runft` will
run the feature tests, and `testcon` runs all of them. They'll pass any
arguments through to TAEF, so you can more finely control the testing.

A recommended workflow is the following command:
```
bcz dbg && runut /name:*<name of test>*
```
Where `<name of test>` is the name of the test testing the relevant feature area
you're working on. For example, if I was working on the VT Mouse input support,
I would use `MouseInputTest` as that string, to isolate the mouse input tests.
If you'd like to run all the tests, just ignore the `/name` param:
`bcz dbg && runut`

To make sure your code is ready for a pull request, run the build, then launch
the built console, then run the tests in it. The built console will inherit all
of the razzle environment, so you can immediately start using the macros:
 1. `bcz`
 2. `opencon`
 3. `testcon` (in the new console window)
 4. `runformat`

If they all come out green, then you're ready for a pull request!
