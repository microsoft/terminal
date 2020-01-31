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
same window.

**Parameters**:
* `[terminal_parameters]`: See [[terminal_parameters]](#terminal_parameters).


#### `split-pane`

`split-pane [--target,-t target-pane] [-H]|[-V] [terminal_parameters]`

Creates a new pane in the currently focused tab by splitting the given pane
vertically or horizontally.

**Parameters**:
* `--target,-t target-pane`: Creates a new split in the given `target-pane`.
  Each pane has a unique index (per-tab) which can be used to identify them.
  These indicies are assigned in the order the panes were created. If omitted,
  defaults to the index of the currently focused pane.
* `-H`, `-V`: Used to indicate which direction to split the pane. `-V` is
  "vertically" (think `[|]`), and `-H` is "horizontally" (think `[-]`). If
  omitted, defaults to "auto", which splits the current pane in whatever the
  larger dimension is. If both `-H` and `-V` are provided, defaults to vertical.
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


## Examples

### Open Windows Terminal in the current directory

```
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

```
wt ; split-pane -p "Windows PowerShell" ; split-pane -H wsl.exe
```

This creates a new Windows Terminal window with one tab, and 3 panes:
* `wt`: Creates the new tab with the default profile
* `split-pane -p "Windows PowerShell"`: This will create a new pane, split from
  the parent with the default profile. This pane will open with the "Windows
  PowerShell" profile
* `split-pane -H wsl.exe`: This will create a third pane, slit _horizontally_
  from the "Windows PowerShell" pane. It will be running the default profile,
  and will use `wsl.exe` as the commandline (instead of the default profile's
  `commandline`).

[#4023]: https://github.com/microsoft/terminal/pull/4023
