---
author: Mike Griese @zadjii-msft
created on: 2020-01-16
last updated: 2020-01-17
---

# Using the `wt.exe` Commandline

As of [#4023], the Windows Terminal now supports accepting arguments on the
commandline, to enable launching the Terminal in a non-default configuration.
This document serves as a reference for all the parameters you can currently
pass, and gives some examples of how to use the `wt` commandline.

> NOTE: If you're running the Terminal built straight from the repo, you'll need
> to use `wtd.exe` and `wtd` instead of `wt.exe` and `wt`.

1. [Commandline Reference](#Reference)
1. [Commandline Examples](#Examples)

## Reference

### Options

#### `--help,-h,-?,/?,`

Display the help message.

## Subcommands

#### `new-tab`

`new-tab [terminal_parameters]`

Opens a new tab with the given customizations. On its _first_ invocation, also
opens a new window. Subsequent `new-tab` commands will all open new tabs in the
same window. <sup>[[1](#footnote-1)]</sup>

**Parameters**:

* `[terminal_parameters]`: See [[terminal_parameters]](#terminal_parameters).

#### `split-pane`

`split-pane [-H,--horizontal|-V,--vertical] [terminal_parameters]`

Creates a new pane in the currently focused tab by splitting the given pane
vertically or horizontally. <sup>[[1](#footnote-1)]</sup>

**Parameters**:

* `-H,--horizontal`, `-V,--vertical`: Used to indicate which direction to split
  the pane. `-V` is "vertically" (think `[|]`), and `-H` is "horizontally"
  (think `[-]`). If omitted, defaults to "auto", which splits the current pane
  in whatever the larger dimension is. If both `-H` and `-V` are provided,
  defaults to vertical.
* `[terminal_parameters]`: See [[terminal_parameters]](#terminal_parameters).

#### `focus-tab`

`focus-tab [--target,-t tab-index]|[--next,-n]|[--previous,-p]`

Moves focus to a given tab.

**Parameters**:

* `--target,-t tab-index`: moves focus to the tab at index `tab-index`. If
  omitted, defaults to `0` (the first tab). Will display an error if combined
  with either of `--next` or `--previous`.
* `-n,--next`: Move focus to the next tab. Will display an error if combined
  with either of `--previous` or `--target`.
* `-p,--previous`: Move focus to the previous tab. Will display an error if
  combined with either of `--next` or `--target`.

#### `[terminal_parameters]`

Some of the preceding commands are used to create a new terminal instance.
These commands are listed above as accepting `[terminal_parameters]` as a
parameter. For these commands, `[terminal_parameters]` can be any of the
following:

`[--profile,-p profile-name] [--startingDirectory,-d starting-directory] [commandline]`

* `--profile,-p profile-name`: Use the given profile to open the new tab/pane,
  where `profile-name` is the `name` or `guid` of a profile. If `profile-name`
  does not match _any_ profiles, uses the default.
* `--startingDirectory,-d starting-directory`: Overrides the value of
  `startingDirectory` of the specified profile, to start in `starting-directory`
  instead.
* `commandline`: A commandline to replace the default commandline of the
  selected profile. If the user wants to use a `;` in this commandline, it
  should be escaped as `\;`.

### Notes

* <span id="footnote-1"></span> [1]: If you try to run a `wt` commandline while running in a Windows Terminal window, the commandline will _always_ create a new window by default. Being able to run `wt` commandlines in the _current_ window is planned in the future - for more information, refer to [#4472].

## Examples

### Open Windows Terminal in the current directory

```powershell
wt -d .
```

This will launch a new Windows Terminal window in the current working directory.
It will use your default profile, but instead of using the `startingDirectory`
property from that it will use the current path. This is especially useful for
launching the Windows Terminal in a directory you currently have open in an
`explorer.exe` window.

### Opening with multiple panes

If you want to open with multiple panes in the same tab all at once, you can use
the `split-pane` command to create new panes.

Consider the following commandline:

```powershell
wt ; split-pane -p "Windows PowerShell" ; split-pane -H wsl.exe
```

This creates a new Windows Terminal window with one tab, and 3 panes:

* `wt`: Creates the new tab with the default profile
* `split-pane -p "Windows PowerShell"`: This will create a new pane, split from
  the parent with the default profile. This pane will open with the "Windows
  PowerShell" profile
* `split-pane -H wsl.exe`: This will create a third pane, split _horizontally_
  from the "Windows PowerShell" pane. It will be running the default profile,
  and will use `wsl.exe` as the commandline (instead of the default profile's
  `commandline`).


### Using multiple commands from PowerShell

The Windows Terminal uses the semicolon character `;` as a delimiter for
separating subcommands in the `wt` commandline. Unfortunately, `powershell` also
uses `;` as a command separator. To work around this you can use the following
tricks to help run multiple wt sub commands from powershell. In all the
following examples, we'll be creating a new Terminal window with three panes -
one running `cmd`, one with `powershell`, and a last one running `wsl`.

In each of the following examples, we're using the `Start-Process` command to
run `wt`. For more information on why we're using `Start-Process`, see ["Using
`start`"](#using-start) below.

#### Single quoted parameters (if you aren't calculating anything):

In this example, we'll wrap all the parameters to `wt` in single quotes (`'`)

```PowerShell
start wt 'new-tab "cmd"; split-pane -p "Windows PowerShell" ; split-pane -H wsl.exe'
```

#### Escaped quotes (if you need variables):

If you'd like to pass a value contained in a variable to the `wt` commandline,
instead use the following syntax:

```PowerShell
$ThirdPane = "wsl.exe"
start wt "new-tab cmd; split-pane -p `"Windows PowerShell`" ; split-pane -H $ThirdPane"
```

Note the usage of  `` ` `` to escape the double-quotes (`"`) around "Windows
Powershell" in the `-p` parameter to the `split-pane` sub-command.

#### Using `start`

All the above examples explicitly used `start` to launch the Terminal.

In the following examples, we're going to not use `start` to run the
commandline. Instead, we'll try two other methods of escaping the commandline:
* Only escaping the semicolons so that `powershell` will ignore them and pass
  them straight to `wt`.
* Using `--%`, so powershell will treat the rest of the commandline as arguments
  to the application.

```PowerShell
wt new-tab "cmd" `; split-pane -p "Windows PowerShell" `; split-pane -H wsl.exe
```

```Powershell
wt --% new-tab cmd ; split-pane -p "Windows PowerShell" ; split-pane -H wsl.exe
```

In both these examples, the newly created Windows Terminal window will create
the window by correctly parsing all the provided commandline arguments.

However, these methods are _not_ recommended currently, as Powershell will wait
for the newly-created Terminal window to be closed before returning control to
Powershell. By default, Powershell will always wait for Windows Store
applications (like the Windows Terminal) to close before returning to the
prompt. Note that this is different than the behavior of `cmd`, which will return
to the prompt immediately. See
[Powershell/PowerShell#9970](https://github.com/PowerShell/PowerShell/issues/9970)
for more details on this bug.


[#4023]: https://github.com/microsoft/terminal/pull/4023
[#4472]: https://github.com/microsoft/terminal/issues/4472
