---
author: Mike Griese @zadjii-msft
created on: 2019-11-08
last updated: 2020-01-15
issue id: #607
---

# Commandline Arguments for the Windows Terminal

## Abstract

This spec outlines the changes necessary for Windows Terminal to support
commandline arguments. These arguments can be used to enable customized launch
scenarios for the Terminal, such as booting directly into a specific profile or
directory.

## Inspiration

Since the addition of the "execution alias" `wt.exe` which enables launching the
Windows Terminal from the commandline, we've always wanted to support arguments
to enable custom launch scenarios. This need was amplified by requests like:
* [#576], which wanted to add jumplist entries for the Windows Terminal, but was
  blocked because there was no way of communicating to the Terminal _which_
  profile it wanted to launch
* [#1060] - being able to right-click in explorer to "open a Windows Terminal
  Here" is great, but would be more powerful if it could also provide options to
  open specific profiles in that directory.
* [#2068] - We want the user to be able to (from inside the Terminal) not only
  open a new window with the default profile, but also open the new window with
  a specific profile.

Additionally, the final design for the arguments was heavily inspired by the
arguments available to `tmux`, which also enables robust startup configuration
through commandline arguments.

## User Stories

Lets consider some different ways that a user or developer might want to
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

### Proposal 1 - Parameters

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

### Proposal 2 - Commands and Parameters

Instead, we'll try to separate these arguments by their responsibilities. Some
of these arguments cause something to happen, like `help`, `version`, or
`open-settings`. Other arguments act more like modifiers, like for example
`--profile` or `--startingDirectory`, which provide additional information to
the action of _opening a new tab_. Lets try and define these concepts more
clearly.

**Commands** are arguments that cause something to happen. They're provided in
`kebab-case`, and can have some number of optional or required "parameters".

**Parameters** are arguments that provide additional information to "commands".
They can be provided in either a long form or a short form. In the long form,
they're provided in `--camelCase`, with two hyphens preceding the argument
name. In short form, they're provided as just a single character preceded by a
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
wt /?

# output the list of profiles (user story 5)
wt list-profiles

# open the settings file, without opening the Terminal window (user story 6)
wt open-settings

# Display version info for the Windows Terminal (user story 7)
wt version
wt --version
wt -v

# Start the default profile in directory "c:/Users/Foo/dev/MyProject" (user story 9)
wt new-tab --startingDirectory "c:/Users/Foo/dev/MyProject"
wt --startingDirectory "c:/Users/Foo/dev/MyProject"
wt -d "c:/Users/Foo/dev/MyProject"
# Windows-style paths work too
wt -d "c:\Users\Foo\dev\MyProject"

# Runs the user's "Windows Powershell" profile in a new tab in directory
#  "c:/Users/Foo/dev/MyProject" (user story 2, 9)
wt new-tab --profile "Windows Powershell" --startingDirectory "c:/Users/Foo/dev/MyProject"
wt --profile "Windows Powershell" --startingDirectory "c:/Users/Foo/dev/MyProject"
wt -p "Windows Powershell" -d "c:/Users/Foo/dev/MyProject"

# open a new tab with the "Windows Powershell" profile, and another with the
#  "cmd" profile (user story 12)
wt new-tab --profile "Windows Powershell" ; new-tab --profile "cmd"
wt --profile "Windows Powershell" ; new-tab --profile "cmd"
wt --profile "Windows Powershell" ; --profile "cmd"
wt --p "Windows Powershell" ; --p "cmd"

# run "my-commandline.exe with some args" in a new tab
wt new-tab my-commandline.exe with some args
wt my-commandline.exe with some args

# run "my-commandline.exe with some args and a ; literal semicolon" in a new
#  tab, and in another tab, run "another.exe running in a second tab"
wt my-commandline.exe with some args and a \; literal semicolon ; new-tab another.exe running in a second tab

# Start cmd.exe, then split it vertically (with the first taking 70% of it's
#  space, and the new pane taking 30%), and run wsl.exe in that pane (user story 13)
wt cmd.exe ; split-pane --target 0 -V -% 30 wsl.exe
wt cmd.exe ; split-pane -% 30 wsl.exe

# Create a new window with the default profile, create a vertical split with the
#  default profile, then create a horizontal split in the second pane and run
#  "media.exe" (user story 13)
wt new-tab ; split-pane -V ; split-pane --target 1 -H media.exe
wt new-tab ; split-pane -V ; split-pane -t 1 -H media.exe

```

## `wt` Syntax

The `wt` commandline is divided into two main sections: "Options", and "Commands":

`wt [options] [command ; ]...`

Options are a list of flags and other parameters that can control the behavior
of the `wt` commandline as a whole. Commands are a semicolon-delimited list of
commands and arguments for those commands.

If no command is specified in a `command`, then the command is assumed to be a
`new-tab` command by default. So, for example, `wt cmd.exe` is interpreted the
same as `wt new-tab cmd.exe`.

To take this a step further, empty commands surrounded by semicolons will also
be interpreted as `new-tab` commands with the default parameters, so `wt ; ; ;`
can be used to open the windows terminal with **4** new tabs. Effectively, that
commandline expands to `wt new-tab ; new-tab ; new-tab ; new-tab`.

<!--
### Aside: What should the default command be?

These are notes from my draft intentionally left here to help understand the
conclusion that new-tab should be the default command.

Should the default command be `new-window` or `new-tab`?

`new-window` makes sense to take params like `--initialPosition`,
`--initialRows`/`--initialCols`, and _implies_ `new-tab`. However, chained
commands that want to open in the same window _need_ to specify `new-tab`,
otherwise they'll all appear in new windows.

If it's `new-tab`, then how do `--initialRows` (etc) work? `new-tab` generally
_doesn't_ accept those parameters, because it's going to be inheriting the
parent's window size. Do we just ignore them for subsequent invocations? I
suppose that makes sense, once the first tab has set those, then the other tabs
can't really change them.

When dealing with a file full of startup commands, we'll assume all of them are
intended for the given window. So the first `new-tab` in the file will create
the window, and all subsequent `new-tab` commands will create tabs in that same
window.
 -->

### Options

#### `--help,-h,-?,/?,`
Runs the `help` command.

#### `--version,-v`
Runs the `version` command.

#### `--session,-s session-id`
Run these commands in the given Windows Terminal session. Enables opening new
tabs in already running Windows Terminal windows. This feature is dependent upon
other planned work landing, so is only provided as an example, of what it might
look like. See [Future Considerations](#Future-Considerations) for more details.

#### `--file,-f configuration-file`
Run these commands in the given Windows Terminal session. Enables opening new
tabs in already running Windows Terminal windows. See [Future
Considerations](#Future-Considerations) for more details.

### Commands

#### `help`

`help`

Display the help message.

#### `version`

`version`

Display version info for the Windows Terminal.

#### `open-settings`

`open-settings [--defaults,-d]`

Open the settings file. If this command is provided alone, it does not open the
terminal window.

**Parameters**:
* `--defaults,-d`: Open the `defaults.json` file instead of the `profiles.json`
  file.

#### `list-profiles`

`list-profiles [--all,-A] [--showGuids,-g]`

Displays a list of each of the available profiles. Each profile displays it's
name, separated by newlines.

**Parameters**:
* `--all,-A`: Show all profiles, including profiles marked `"hidden": true`.
* `--showGuids,-g`:  In addition to showing names, also list each profile's
  guid. These GUIDs should probably be listed _first_ on each line, to make
  parsing output easier.

#### `new-tab`

`new-tab [--initialPosition x,y]|[--maximized]|[--fullscreen] [--initialRows rows] [--initialCols cols] [terminal_parameters]`

Opens a new tab with the given customizations. On its _first_ invocation, also
opens a new window. Subsequent `new-tab` commands will all open new tabs in the
same window.

**Parameters**:
* `--initialPosition x,y`: Create the new Windows Terminal window at the given
  location on the screen in pixels. This parameter is only used when initially
  creating the window, and ignored for subsequent `new-tab` commands. When
  combined with any of `--maximized` or `--fullscreen`, an error message will be
  displayed to the user, indicating that an invalid combination of arguments was
  provided.
* `--initialRows rows`: Create the terminal window with `rows` rows (in
  characters). If omitted, uses the value from the user's settings. This
  parameter is only used when initially creating the window, and ignored for
  subsequent `new-tab` commands. When combined with any of `--maximized` or
  `--fullscreen`, an error message will be displayed to the user, indicating
  that an invalid combination of arguments was provided.
* `--initialCols cols`: Create the terminal window with `cols` cols (in
  characters). If omitted, uses the value from the user's settings. This
  parameter is only used when initially creating the window, and ignored for
  subsequent `new-tab` commands. When combined with any of `--maximized` or
  `--fullscreen`, an error message will be displayed to the user, indicating
  that an invalid combination of arguments was provided.
* `[terminal_parameters]`: See [[terminal_parameters]](#terminal_parameters).


#### `split-pane`

`split-pane [--target,-t target-pane] [-H]|[-V] [--percent,-% split-percentage] [terminal_parameters]`

Creates a new pane in the currently focused tab by splitting the given pane
vertically or horizontally.

**Parameters**:
* `--target,-t target-pane`: Creates a new split in the given `target-pane`.
  Each pane has a unique index (per-tab) which can be used to identify them.
  These indices are assigned in the order the panes were created. If omitted,
  defaults to the index of the currently focused pane.
* `-H`, `-V`: Used to indicate which direction to split the pane. `-V` is
  "vertically" (think `[|]`), and `-H` is "horizontally" (think `[-]`). If
  omitted, defaults to "auto", which splits the current pane in whatever the
  larger dimension is. If both `-H` and `-V` are provided, defaults to vertical.
* `--percent,-% split-percentage`: Designates the amount of space that the new
  pane should take as a percentage of the parent's space. If omitted, the pane
  will take 50% by default.
* `[terminal_parameters]`: See [[terminal_parameters]](#terminal_parameters).

#### `focus-tab`

`focus-tab [--target,-t tab-index]`

Moves focus to a given tab.

**Parameters**:

* `--target,-t tab-index`: moves focus to the tab at index `tab-index`. If omitted,
  defaults to `0` (the first tab).

#### `focus-pane`

`focus-pane [--target,-t target-pane]`

Moves focus within the currently focused tab to a given pane.

**Parameters**:

* `--target,-t target-pane`: moves focus to the given `target-pane`. Each pane
  has a unique index (per-tab) which can be used to identify them. These
  indices are assigned in the order the panes were created. If omitted,
  defaults to the index of the currently focused pane (which is effectively a
  no-op).

#### `move-focus`

`move-focus [--direction,-d direction]`

Moves focus within the currently focused tab in the given direction.

**Parameters**:

* `--direction,-d direction`: moves focus in the given `direction`. `direction`
  should be one of [`left`, `right`, `up`, `down`]. If omitted, does not move
  the focus at all (resulting in a no-op).

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

Fundamentally, there's no reason that _all_ the current profile settings
couldn't be overridden by commandline arguments. Practically, it might be
unreasonable to create short form arguments for each and every Profile
property, but the long form would certainly be reasonable.

The arguments listed above represent both special cases of the profile settings
like `guid` and `name`, as well as high priority properties to add as arguments.
* It doesn't really make sense to override `name` or `guid`, so those have been
  repurposed as arguments for selecting a profile.
* `commandline` is a bit of a unique case - we're not explicitly using an
  argument to identify the start of the commandline here. This is to help avoid
  the need to parse and escape arguments to the client commandline.
* `startingDirectory` is a _highly_ requested commandline argument, so that's
  been given priority in this spec.

## Implementation Details

Following an investigation performed the week of Nov 18th, 2019, I've determined
that we should be able to use the [CLI11] open-source library to parse
our arguments. We'll need to add some additional logic on top of CLI11 in order
to properly separate commands with `;`, but that's not impossible to achieve.

CLI11 will allow us to parse commandlines as a series of options, with a
possible sub-command that takes its own set of parameters. This functionality
will be used to enable our options & commands style of parameters.

When commands are parsed, each command will build an `ActionAndArgs` that can be
used to tell the terminal what steps to perform on startup. The Terminal already
uses these `ActionAndArgs` to perform actions like opening new tabs, panes,
moving focus, etc.

In my initial investigation, it seemed as though the Terminal did not initialize
the size of child controls initially. This meant that it wasn't possible to
immediately create all the splits and tabs for the Terminal as passed on the
commandline, because they'd open at a size of 0x0. To mitigate this, we'll
handle dispatching these startup actions one at a time, waiting until the
Terminal for an action is initialized or the command is otherwise completed
before dispatching the next one.

This is a perhaps fragile way of handling the initialization. Ideally, there
should be a way to dispatch all the commands _immediately_, before the Terminal
fully initializes, so that the UI pops up in the state as specified in the
commandline. This will be an area of active investigation as implementation is
developed, to make the initialization of many commands as seamless as possible.

### Implementation plan

As this is a very complex feature, there will need to be a number of steps taken
in the codebase to enable this functionality in a way that users are expecting.
The following is a suggestion of the individual changelists that could be made
to iteratively work towards fulling implementing this functionality.

* [x] Refactor `ShortcutAction` dispatching into its own class
  - Right now, the `AppKeyBindings` is responsible for triggering all
    `ActionAndArgs` events, but only based upon keystrokes while the Terminal is
    running. As we'll be re-using `ActionAndArgs` for handling startup events,
    we'll need a more generic way of dispatching those events.
* [x] Add a `SplitPane` `ShortcutAction`, with a single parameter `split`,
  which accepts either `vertical`, `horizontal`, or `auto`.
  - Make sure to convert the legacy `SplitVertical` and `SplitHorizontal` to use
    `SplitPane` with that arg set appropriately.
* [x] Add a `TerminalParameters` winrt object to `NewTabArgs` and `SplitPane`
  args. `TerminalParameters` will include the following properties:

```c#
runtimeclass TerminalParameters {
    String ProfileName;
    String ProfileGuid;
    String StartingDirectory;
    String Commandline;
}
```
  - These represent the arguments in `[terminal_parameters]`. When set, they'll
    both `newTab` and `splitPane` will accept [`profile`, `guid`, `commandline`,
    `startingDirectory`] as optional parameters, and when they're set, they'll
    override the default values used when creating a new terminal instance.
    - `profile` and `guid` will be used to look up the profile to create by
      `name`, `guid`, respectively, as opposed to the default profile.
    - The others will override their respective properties from the
      `TerminalSettings` created for that profile.
* [x] Add an optional `"percent"` argument to `SplitPane`, that enables a pane
  to be split with a specified percent of the parent pane.
* [x] Add support to `TerminalApp` for parsing commandline arguments, and
  constructing a list of `ActionAndArgs` based on those commands.
  - This will include adding tests that validate a particular commandline
    generates the given sequence of `ActionAndArgs`.
  - This will _not_ include _performing_ those actions, or passing the
    commandline from the `WindowsTerminal` executable to the `TerminalApp`
    library for parsing. This change does not add any user-facing functional
    behavior, but is self-contained enough that it can be its own changelist,
    without depending upon other functionality.
* [ ] When parsing a `new-tab` command, configure the `TerminalApp::AppLogic` to
  set some initial state about itself, to handle the `new-tab` arguments
  [`--initialPosition`, `--maximized`, `--initialRows`, `--initialCols`]. Only
  set this state for the first `new-tab` parsed. These settings will overwrite
  the corresponding global properties on launch.
* [ ] When parsing a `help` command or a `list-profiles` command, trigger a
  event on `AppLogic`. This event should be able to be handled by
  WindowsTerminal (`AppHost`), and used to display a `MessageBox` with the given
  text. (see [Potential Issues](##subsystemwindows-or-subsystemconsole) for a
  discussion on this).
* [ ] Add support for performing actions passed on the commandline. This
  includes:
  - Passing the commandline into the `TerminalApp` for parsing.
  - Performing `ActionAndArgs` that are parsed by the Terminal.
  - At this point, the user should be able to pass the following commands to the
    Terminal:
    - `new-tab`
    - `split-pane`
    - `move-focus`
    - `focus-tab`
    - `open-settings`
    - `help`
    - `list-profiles`
* [ ] Add a `ShortcutAction` for `FocusPane`, which accepts a single parameter
  `index`.
  - We'll need to track each `Pane`'s ID as `Pane`s are created, so that we can
    quickly switch to the nth `Pane`.
  - This is in order to support the `-t,--target` parameter of `split-pane`.

## Capabilities

### Accessibility

As a commandline feature, the accessibility of this feature will largely be tied
to the ability of the commandline environment to expose accessibility
notifications. Both `conhost.exe` and the Windows Terminal already support
basic accessibility patterns, so users using this feature from either of those
terminals will be reliant upon their accessibility implementations.

### Security

As we'll be parsing user input, that's always subject to worries about buffer
length, input values, etc. Fortunately, most of this should be handled for us by
the operating system, and passed to us as a commandline via `winMain` and
`CommandLineToArgvW`. We should still take extra care in parsing these args.

### Reliability

This change should not have any particular reliability concerns.

### Compatibility

This change should not regress any existing behaviors.

### Performance, Power, and Efficiency

This change should not particularly impact startup time or any of these other categories.

## Potential Issues

### Commandline escaping

Escaping commandlines is notoriously tricky to do correctly. Since we're using
`;` to delimit commands, which might want to also use `;` in the commandline
itself, we'll use `\;` as an escaped `;` within the commandline. This is an area
we've been caught in before, so extensive testing will be necessary to make sure
this works as expected.

Painfully, powershell uses `;` as a separator between commands as well. So, if
someone wanted to call a `wt` commandline in powershell with multiple commands,
the user would need to also escape those semicolons for powershell first. That
means a command like ```wt new-tab ; split-pane``` would need to be ```wt new-tab
`; split-pane``` in powershell, and ```wt new-tab ; split-pane commandline \; with
\; semicolons``` would need to become ```wt new-tab `; split-pane commandline \`;
with \`; semicolons```, using ```\`;``` to first escape the semicolon for
powershell, then the backslash to escape it for `wt`.

Alternatively, the user could choose to escape the semicolons with quotes
(either single or double), like so: ```wt new-tab ';' split-pane "commandline \;
with \; semicolons"```.

This would get a little ridiculous when using powershell commands that also have
semicolons possible escaped within them:

```powershell
wt.exe ";" split-pane "powershell Write-Output 'Hello World' > foo.txt; type foo.txt"
```

We've decided that although this behavior is uncomfortable in powershell, there
doesn't seem to be any option out there that's _less_ painful. This is a
reasonable option that makes enough logical sense. Users familiar with
powershell will understand the need to escape commandlines like this.

As noted by @jantari:
> PowerShell has the --% (stop parsing) operator, which instructs it to stop
> interpreting anything after it and just pass it on verbatim. So, the
> semicolon-problem could also be addressed by the following syntax:
> ```sh
> # wt.exe still needs to be interpreted by PowerShell as it's a command in PATH, but nothing after it
> wt.exe --% cmd.exe ; split-pane --target-pane 0 -V -% 30 wsl.exe
> ```

### `/SUBSYSTEM:Windows` or `/SUBSYSTEM:Console`?

When you create an application on Windows, you must link it as either a Windows
or a Console application. When the application is launched from a commandline
shell as a Windows application, the shell will immediately return to the
foreground of the console, which means that any console output emitted by the
process will be intermixed with the shell. However, if an application is linked
as a Console application, and it's launched from the Start Menu, Run dialog, or
any other context that's _not_ a console, then the OS will _automatically_
create a console to host the commandline application. That means that briefly, a
console window will appear on the screen, even if we decide that we just want to
launch our application's window.

This basically leaves us with two bad scenarios. Either we're a Console
application, and a console window always flashes on screen for every
non-commandline invocation of the Terminal, or we're a Windows application, and
console output we log (including help messages) can get mixed with shell output.
Neither of these are particularly good.

`python` et. al. often ship with _two_ executables, a `python.exe` which is a
Console application, and a `pythonw.exe`, which is a Windows application. This
however has led to [loads of confusion](https://stackoverflow.com/a/30313091),
and even with plentiful documentation, would likely result in users being
confused about what does what. For situations like launching the Terminal in the
CWD of `explorer.exe`, users would need to use `wtw.exe -d .` to prevent the
console window from appearing. However, when calling Windows Terminal from a
commandline environment, users who call `wtw.exe /?` would likely get unexpected
behavior, because they should have instead called `wt.exe /?`.

To avoid this confusion, I propose we follow the example of `msiexec /?`. This
is a Windows application that uses a `MessageBox` to display its help text.
While this is less convenient for users coming exclusively from a commandline
environment, it's also the least bad option available to us.
* It's less confusing than having control returned to the shell
* It's not as bad as forcing the creation of a console window for
  non-commandline launches.
* There's precedent for this kind of dialog (we're not inventing a new pattern
  here).

### What happens if `new-tab` isn't the first command?

Consider the following commandline:

```sh
wt.exe split-pane -V ; new-tab
```

In the future, maybe we could presume in this case that the commands are
intended for the current Windows Terminal window, though that's not
functionality that will arrive in 1.0. Even when sessions are supported like
that, I'm not sure that when we're parsing a commandline, we'll be able to
know what session we're currently running in. That might make it challenging to
dispatch this kind of command to "the current WT window".

Additionally, what would happen if this was run in a `conhost` window, that
wasn't attached to a Terminal session? We wouldn't be able to tell _the current
session_ to `split-pane`, since there wouldn't be one. What would we do then?
Display an error message somehow?

I don't believe that implying the _current Windows Terminal session_ is the
correct behavior here. Instead we should either:
* Assume that there's an implicit `new-tab` command that's run first, to create
  the window, _then_ run `split-pane` in that tab.
* Immediately display an error that the commandline is invalid, and that a
  commandline should start with a `new-tab ; `?

In my initial implementation, I resolved this by assuming there was an implicit
`new-tab` command, and that felt right. The team has discussed this, and
concluded that's the correct behavior. In the words of @DHowett-MSFT:

> In favor of "implicit `new-tab`": `wt.exe` without any arguments is _already_
> an implicit `new-window` or `new-tab`; we can't claw back the implicitness and
> ease of use in that one, so I think in the spirit of keeping that going WT
> should automatically do anything necessary to service a command (`wt
> split-pane` should operate in a new tab or new window, etc.)

We should also make sure that when we add support for the `open-settings`
command, that command by itself should not imply a `new-tab`. `wt open-settings`
should simply open the settings in the user's chosen `.json` editor, without
needing to open a terminal window.

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
* In the past we've had requests (like [#756]) for having the terminal start
  with multiple tabs/panes by default. This might be a path to enabling that
  scenario. One could imagine the `profiles.json` file including a
  `defaultConfiguration` property, with a path to a .conf file filled with
  commands. We'd parse that file on window creation just the same as if it was
  parsed on the commandline. If the user provides a file on the commandline,
  we'll just ignore that value from `profiles.json`.
* When working on "New Window", we'll want the user to be able to open a new
  window with not only the default profile, but also a specific profile. This
  will help us enable that scenario.
* We might want to look into `Register‑ArgumentCompleter` in powershell to
  enable letting the user auto-complete our args in powershell.
* If we're careful, we could maybe create short form aliases for all the
  commands, so the user wouldn't need to type them all out every time. `new-tab`
  could become `nt`, `split-pane` becomes `sp`, etc. A commandline could look
  like `wt ; sp less some-log.txt ; fp -t 0` then.


## Resources

Feature Request: wt.exe supports command line arguments (profile, command, directory, etc.) [#607]
Add "open Windows terminal here" into right-click context menu [#1060]

Feature Request: Task Bar jumplist should show items from profile [#576]
Draft spec for adding profiles to the Windows jumplist [#1357]

Spec for tab tear off and default app [#2080]

[Question] Configuring Windows Terminal profile to always launch elevated [#632]

New window key binding not working [#2068]

Feature Request: Start with multiple tabs open [#756]

<!-- Footnotes -->

[#756]: https://github.com/microsoft/terminal/issues/756
[#576]: https://github.com/microsoft/terminal/issues/576
[#607]: https://github.com/microsoft/terminal/issues/607
[#632]: https://github.com/microsoft/terminal/issues/632
[#1060]: https://github.com/microsoft/terminal/issues/1060
[#1357]: https://github.com/microsoft/terminal/pull/1357
[#2068]: https://github.com/microsoft/terminal/issues/2068
[#2080]: https://github.com/microsoft/terminal/pull/2080
[CLI11]: https://github.com/CLIUtils/CLI11
