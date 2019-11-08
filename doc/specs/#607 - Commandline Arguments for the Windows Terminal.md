---
author: Mike Griese @zadjii-msft
created on: 2019-11-08
last updated: 2019-11-08
issue id: #607
---

# Commandline Arguments for the Windows Terminal

## Abstract

This spec outlines the changes necessary to add support to Windows Terminal to
support commandline arguments. These arguments can be used to enable customized
launch scenarios for the Terminal, such as booting directly into a specific
profile or directory.

## Inspiration

Since the addition of the "execution alias" `wt.exe` which enables launching the
Windows Terminal from the commandline, we've always wanted to support arguments
to enable custom launch scenarios. This need was amplified by requests like:
* [#576], which wanted to add jumplist entries for the Windows Terminal, but was
  blocked becaues there was no way of communicating to the Terminal _which_
  profile it wanted to launch
* [#1060] - being able to right-click in explorer to "open a Windows Terminal
  Here" is great, but would be more powerful if it could also provide options to
  open specific profiles in that directory.

Additionally, the final design for the arguments was heavily inspired by the
arguments available to `tmux`, which also enables robust startup configuration
through commandline arguments.

## User Stories

Lets consider some different ways that a user or developer might want want to
use commandline arguments, to help guide the design.

1. A user wants to open the Windows Terminal with their default profile.
  - This one is easy, it's already provided with simply `wt`.
2. A user wants to open the Windows Terminal with a specific profile from their
   list of profiles.
3. A user wants to open the Windows Terminal with their default profile, but
   running a different commandline than usual.
4. A user wants to know the list of arguments supported by `wt.exe`.
5. A user wants to see their list of profiles, so they can open one in
   particular
6. A user wants to open their settings file, without needing to open the
   Terminal window.
7. A user wants to know what version of the Windows Terminal they are running,
   without needing to open the Terminal window.
8. A user wants to open the Windows Terminal at a specific location on the
   screen
9. A user wants to open the Windows Terminal in a specific directory.
10. A user wants to open the Windows Terminal with a specific size
11. A user wants to open the Windows Terminal with only the default settings,
    ignoring their user settings.
12. A user wants to open the Windows Terminal with multiple tabs open
    simultaneously, each with different profiles, starting directories, even
    commandlines
13. A user wants to open the Windows Terminal with multiple tabs and panes open
    simultaneously, each with different profiles, starting directories, even
    commandlines, and specific split sizes
14. A user wants to use a file to provide a reusable startup configuration with
    many steps, to avoid needing to type the commandline each time.

## Solution Design

### Style 1 - Parameters

Initially, I had considered arguments in the following style:

* `--help`: Display the help message
* `--version`: Display version info for the Windows Terminal
* `--list-profiles`: Display a list of the available profiles
  - `--all` to also show "hidden" profiles
  - `--verbose`? To also display GUIDs?
* `--open-settings`: Open the settings file
* `--profile <profile name>`: Start with the given profile, by name
* `--guid <profile guid>`: Start with the given profile, by GUID
* `--startingDirectory <path>`: Start in the given directory
* `--initialRows <rows>`, `--initialCols <rows>`: Start with a specific size
* `--initialPosition <x,y>`: Start at an initial location on the screen
* `-- <commandline>`: Start with this commandline instead

However, this style of arguments makes it very challenging to start multiple
tabs or panes simultaneously. How would a user start multiple panes, each with a
different commandline? As configurations become more complex, these commandlines
would quickly become hard to parse and understand for the user.

### Style 2 - Commands and Parameters

Instead, we'll try to seperate these arguments by their responsibilities. Some
of these arguments cause something to happen, like `help`, `version`, or
`open-settings`. Other arguments act more like modifiers, like for example
`--profile` or `--startingDirectory`, which provide additional information to
the action of _opening a new tab_. Lets try and define these concepts more
clearly.

**Commands** are arguments that cause something to happen. They're provided in
`kebab-case`, and can have some number of optional or required "parameters".

**Parameters** are arguments that provide additional information to "commands".
They can be provided in either a long form or a short form. In the long form,
they're provided in `--camelCase`, with two hyphens preceeding the argument
name. In short form, they're provided as just a single character preceeded by a
hyphen, like so: `-c`.

Let's enumerate some possible example commandlines, with explanations, to
demonstrate:

### Sample Commandlines

```sh

# Runs the user's "Windows Powershell" profile in a new tab (user story 2)
wt new-tab --profile "Windows Powershell"
wt --profile "Windows Powershell"
wt -p "Windows Powershell"

# Runs the user's default profile in a new tab, running cmd.exe (user story 3)
wt cmd.exe

# display the help text (user story 4)
wt help
wt --help
wt -h
wt -?

# output the list of profiles (user story 5)
wt list-profiles

# open the settings file, without opening the Terminal window (user story 6)
wt open-settings

# Display version info for the Windows Terminal (user story 7)
wt version
wt --version
wt -v

# Start the default profile in directory "c:/Users/Foo/dev/MyProject" (user story 8)
wt new-tab --startingDirectory "c:/Users/Foo/dev/MyProject"
wt --startingDirectory "c:/Users/Foo/dev/MyProject"
wt -d "c:/Users/Foo/dev/MyProject"

# Runs the user's "Windows Powershell" profile in a new tab in directory
#  "c:/Users/Foo/dev/MyProject" (user story 2, 8)
wt new-tab --profile "Windows Powershell" --startingDirectory "c:/Users/Foo/dev/MyProject"
wt --profile "Windows Powershell" --startingDirectory "c:/Users/Foo/dev/MyProject"
wt -p "Windows Powershell" -d "c:/Users/Foo/dev/MyProject"

# open a new tab with the "Windows Powershell" profile, and another with the
#  "cmd" profile (user story 12)
wt new-window --profile "Windows Powershell" ; new-tab --profile "cmd"
wt --profile "Windows Powershell" ; new-tab --profile "cmd"
wt --p "Windows Powershell" ; new-tab --p "cmd"

# run "my-commandline.exe with some args" in a new tab
wt new-window my-commandline.exe with some args
wt my-commandline.exe with some args

# run "my-commandline.exe with some args and a ; literal semicolon" in a new
#  tab, and in another tab, run "another.exe running in a second tab"
wt my-commandline.exe with some args and a \; literal semicolon ; new-tab another.exe running in a second tab

# Start cmd.exe, then split it vertically (with the first taking 50% of it's
#  space), and run wsl.exe in that pane (user story 13)
wt cmd.exe ; split-pane -t 0 -v -p 50 wsl.exe
wt cmd.exe ; split-pane wsl.exe

# Create a new window with the default profile, create a vertical split with the
#  default profile, then create a horizontal split in the second pane and run
#  "media.exe" (user story 13)
wt new-window ; split-pane -v ; split-pane -t 1 -h media.exe

```

## `wt` Syntax

TODO: `wt [flags?] [commands]`
Describe this

### Commands:

#### `help`

`help` (aliased as `--help`,`-h`,`-?`)

Display the help message

#### `version`

`version` (aliased as `--version`,`-v`)

Display version info for the Windows Terminal

#### `open-settings`

`open-settings [--defaults,-d]`

Open the settings file.

**Parameters**:
* `--defaults,-d`: Open the `defaults.json` file instead of the `profiles.json`
  file.

#### `list-profiles`

`list-profiles [--all,-A] [--showGuids,-g]`

Displays a list of each of the available profiles. Each profile displays it's
name, seperated by newlines.

**Parameters**:
* `--all,-A`: Show all profiles, including profiles marked `"hidden": true`.
* `--showGuids,-g`:  In addition to showing names, also list each profile's
  guid. These GUIDs should probably be listed _first_ on each line, to make
  parsing output easier.

#### `new-tab`

`new-tab [--profile,-p profile-name]|[--guid,-g profile-guid] [--startingDirectory,-d starting-directory] [commandline]`

Opens a new tab with the given customizations.

* `--profile,-p profile-name`: Open a tab with the given profile, where
  `profile-name` is the `name` of a profile. If `name` does not match _any_
  profiles, uses the default. If both `--profile` and `--guid` are omitted, uses
  the default profile.
* `--guid,-g profile-guid`: Open a tab with the given profile, where
  `profile-guid` is the `guid` of a profile. If `guid` does not match _any_
  profiles, uses the default. If both `--profile` and `--guid` are omitted, uses
  the default profile. If both `--profile` and `--guid` are specified at the
  same time, `--guid` takes precedence.
* `--startingDirectory,-d starting-directory`: Overrides the value of `startingDirectory` of the specified profile, to start in `starting-directory` instead.
* `commandline`:

#### `split-pane`

`split-pane [--target,-t target-pane] [-h|v] [--percent,-p split-percentage] [--profile,-p profile-name]|[--guid,-g profile-guid] [--startingDirectory,-d starting-directory] [commandline]`

* `--all,-A`: Show all profiles, including profiles marked `"hidden": true`.
* `--showGuids,-g`:  In addition to showing names, also list each profile's


### TODO: Default command: `new-tab` vs `new-window`

What should the default command be?

TODO: The "default command" is `new-window`. Or should it be `new-tab`? We don't
currently support attaching, but when we do, how would that feel? For now, we'll
just assume that any command _doesn't_ attach by default, but there should
probably be a way.

If it's `new-window`, then chained commands that want to open in the same window
_need_ to specify `new-tab`, otherwise they'll all appear in new windows.

If it's `new-tab`, then how do `--initialRows` (etc) work? `new-tab` generally
_doesn't_ accept those parameters, because it's going to be inheriting the
parent's window size. Do we just ignore them for subsequent invocations?

Let's make `new-window` the default. `new-window` can take params like
`--initialPosition`, `--initialRows`/`--initialCols`, and _implies_ `new-tab`.

What if we assumed that new-window was the default for the _first_ command, and
subsequent commands defaulted to `new-tab`?

That seems sketchy, and then how would files full of commands work?

We could assume that the _first_ `new-tab` command _always_ creates a new window, and subsequent `new-tab` calls are all intended for the window of that created window.

When dealing with a file full of startup commands, we'll assume all of them are
intended for the given window. So the first `new-tab` in the file will create
the window, and all subsequent `new-tab` commands will create tabs in that same
window.

Okay I think I'm happy with `new-tab` creates a window if there isn't one by
default, and it accepts `--initialPosition`, `--initialRows`/`--initialCols`,
but those are ignored on subsequent launches. Let's clean this section up.


### Graceful Upgrading

The entire power of these commandline args is not feasible to accomplish within
the scope of v1.0 of the Windows Terminal. Core to this design is the idea of a
_graceful upgrade_ from 1.0 to some future version, where the full power of
these arguments can be expressed. For the sake of brevity, we'll assume that
future version is 2.0 for the remainder of the spec.

For 1.0, we're focused on primarily [user stories](#User-stories) 1-10, with
8-10 being a lower priority. During 1.0, we won't be focused on supporting
opening multiple tabs or panes straight from the commandline. We'll be focused
on a much simpler grammar of arguments, with the intention that commandlines
from 1.0 will work _without modification_ as 2.0.

For 1.0, we'll restrict ourselves in the following ways:
* We'll only support one command per commandline. This will be one of the
  following list:
  - `help`
  - `version`
  - `new-tab`
  - `open-settings`
* We'll need to make sure that we process commandlines with escaped semicolons
  in them the same as we will in 2.0. Users will need to escape `;` as `\;` in
  1.0, even if we don't support multiple commands in 1.0.
* If users don't provide a command, we'll assume the command was `new-tab`. This will be in-line with


#### Sample 1.0 Commandlines

```sh
# display the help message
wt help
wt --help
wt -h
wt -?

# Display version info for the Windows Terminal
wt version
wt --version
wt -v

# Runs the user's default profile in a new tab, running cmd.exe
wt cmd.exe

# Runs the user's "Windows Powershell" profile in a new tab
wt new-tab --profile "Windows Powershell"
wt --profile "Windows Powershell"

# run "my-commandline.exe with some args" in a new tab
wt new-tab my-commandline.exe with some args
wt my-commandline.exe with some args

# run "my-commandline.exe with some args and a ; literal semicolon" in a new
#  tab, and in another tab, run "another.exe running in a second tab"
wt my-commandline.exe with some args and a \; literal semicolon

# Start the default profile in directory "c:/Users/Foo/dev/MyProject" (user story 8)
wt new-tab --startingDirectory "c:/Users/Foo/dev/MyProject"
wt --startingDirectory "c:/Users/Foo/dev/MyProject"
wt -d "c:/Users/Foo/dev/MyProject"

# Runs the user's "Windows Powershell" profile in a new tab in directory
#  "c:/Users/Foo/dev/MyProject" (user story 2, 8)
wt new-tab --profile "Windows Powershell" --startingDirectory "c:/Users/Foo/dev/MyProject"
wt --profile "Windows Powershell" --startingDirectory "c:/Users/Foo/dev/MyProject"
wt -p "Windows Powershell" -d "c:/Users/Foo/dev/MyProject"

```



## UI/UX Design

## Capabilities

### Accessibility

As a commandline feature, the accessibility of this feature will largely be tied
to the ability of the commandline environment to expose accessibility
notifications. Both `conhost.exe` and the Windows Terminal already support
basica accessibility patterns, so users using this feature from either of those
terminals will be reliant upon their accessibility implementations.

### Security

[comment]: # How will the proposed change impact security?

### Reliability

[comment]: # Will the proposed change improve reliabilty? If not, why make the change?

### Compatibility

This change should not regress any existing behaviors.

### Performance, Power, and Efficiency

## Potential Issues

[comment]: # What are some of the things that might cause problems with the fixes/features proposed? Consider how the user might be negatively impacted.

## Future considerations

* These are some additional argument ideas which are dependent on other features
  that might not land for a long time. These features were still considered as a
  part of the design of this solution, though their implementation is purely
  hypothetical for the time being.
    * Instead of launching a new Windows Terminal window, attach this new
      terminal to an existing one. This would require the work outlined in
      [#2080], so support a "manager" process that could coordinate sessions
      like this.
        - This would be something like `wt --session [some-session-id]
          [commands]`, where `--session [some-session-id]` would tell us that
          `[more-commands]` are intended for the given other session/window.
          That way, you could open a new tab in another window with `wt --session
          0 cmd.exe` (for example).
    * `list-sessions`: A command to display all the active Windows terminal
      instances and their session ID's, in a way compatible with the above
      command. Again, heavily dependent upon the implementation of [#2080].
    * `--elevated`: Should it be possible for us to request an elevated session
      of ourselves, this argument could be used to indicate the process should
      launch in an _elevated_ context. This is considered in pursuit of [#632].
    * `--file,-f configuration-file`: Used for loading a configuration file to
      give a list of commands. This file can enable a user to have a re-usable
      configuration saved somewhere on their machine. When dealing with a file
      full of startup commands, we'll assume all of them are intended for the
      given window. So the first `new-tab` in the file will create the window,
      and all subsequent `new-tab` commands will create tabs in that same
      window.
* In the past we've had requests for having the terminal start with multiple
  tabs/panes by default. This might be a path to enabling that scenario. One
  could imagine the `profiles.json` file including a `defaultConfiguration`
  property, with a path to a .conf file filled with commands. We'd parse that
  file on window creation just the same as if it was parsed on the commandline.
  If the user provides a file on the commandline, we'll just ignore that value
  from `profiles.json`.

## Resources

Feature Request: wt.exe supports command line arguments (profile, command, directory, etc.) [#607]

Add "open Windows terminal here" into right-click context menu [#1060]

Feature Request: Task Bar jumplist should show items from profile [#576]
Draft spec for adding profiles to the Windows jumplist [#1357]

Spec for tab tear off and default app [#2080]

[Question] Configuring Windows Terminal profile to always launch elevated [#632]

<!-- Footnotes -->

[#576]: https://github.com/microsoft/terminal/issues/576
[#607]: https://github.com/microsoft/terminal/issues/607
[#632]: https://github.com/microsoft/terminal/issues/632
[#1060]: https://github.com/microsoft/terminal/issues/1060
[#1357]: https://github.com/microsoft/terminal/pull/1357
[#2080]: https://github.com/microsoft/terminal/pull/2080
