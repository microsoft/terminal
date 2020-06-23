---
author: Mike Griese @zadjii-msft
created on: 2019-08-01
last updated: 2020-06-10
issue id: 2046
---

# Command Palette

## Abstract

This spec covers the addition of a "command palette" to the Windows Terminal.
The Command Palette is a GUI that the user can activate to search for and
execute commands. Beneficially, the command palette allows the user to execute
commands _even if they aren't bound to a keybinding_.

## Inspiration

This feature is largely inspired by the "Command Palette" in text editors like
VsCode, Sublime Text and others.

This spec was initially drafted in [a
comment](https://github.com/microsoft/terminal/issues/2046#issuecomment-514219791)
in [#2046]. That was authored during the annual Microsoft Hackathon, where I
proceeded to prototype the solution. This spec is influenced by things I learned
prototyping.

Initially, the command palette was designed simply as a method for executing
certain actions that the user pre-defined. With the addition of [commandline
arguments](https://github.com/microsoft/terminal/issues/4632) to the Windows
Terminal in v0.9, we also considered what it might mean to be able to have the
command palette work as an effective UI not only for dispatching pre-defined
commands, but also `wt.exe` commandlines to the current terminal instance.

## Solution Design

Fundamentally, we need to address two different modes of using the command palette:
* In the first mode, the command palette can be used to quickly look up
  pre-defined actions and dispatch them. We'll refer to this as "Action Mode".
* The second mode allows the user to run `wt` commandline commands and have them
  apply immediately to the current Terminal window. We'll refer to this as
  "commandline mode".

Both these options will be discussed in detail below.

### Action Mode

We'll introduce a new top-level array to the user settings, under the key
`commands`. `commands` will contain an array of commands, each with the
following schema:

```js
{
    "name": string|object,
    "action": string|object,
    "icon": string
}
```

Command names should be human-friendly names of actions, though they don't need
to necessarily be related to the action that it fires. For example, a command
with `newTab` as the action could have `"Open New Tab"` as the name.

The command will be parsed into a new class, `Command`:

```c++
class Command
{
    winrt::hstring Name();
    winrt::TerminalApp::ActionAndArgs ActionAndArgs();
    winrt::hstring IconSource();
}
```

We'll add another structure in GlobalAppSettings to hold all these actions. It
will just be a `std::vector<Command>` in `GlobalAppSettings`.

We'll need app to be able to turn this vector into a `ListView`, or similar, so
that we can display this list of actions. Each element in the view will be
intrinsically associated with the `Command` object it's associated with. In
order to support this, we'll make `Command` a winrt type that implements
`Windows.UI.Xaml.Data.INotifyPropertyChanged`. This will let us bind the XAML
element to the winrt type.

When an element is clicked on in the list of commands, we'll raise the event
corresponding to that `ShortcutAction`. `AppKeyBindings` already does a great
job of dispatching `ShortcutActions` (and their associated arguments), so we'll
re-use that. We'll pull the basic parts of dispatching `ActionAndArgs`
callbacks into another class, `ShortcutActionDispatch`, with a single
`DoAction(ActionAndArgs)` method (and events for each action).
`AppKeyBindings` will be initialized with a reference to the
`ShortcutActionDispatch` object, so that it can call `DoAction` on it.
Additionally, by having a singular `ShortcutActionDispatch` instance, we won't
need to re-hook up the ShortcutAction keybindings each time we re-load the
settings.

In `TerminalPage`, when someone clicks on an item in the list, we'll get the
`ActionAndArgs` associated with that list element, and call `DoAction` on
the app's `ShortcutActionDispatch`. This will trigger the event handler just the
same as pressing the keybinding.

#### Commands for each profile?

[#3879] Is a request for being able to launch a profile directly, via the
command palette. Essentially, the user will type the name of a profile, and hit
enter to launch that profile. I quite like this idea, but with the current spec,
this won't work great. We'd need to manually have one entry in the command
palette for each profile, and every time the user adds a profile, they'd need to
update the list of commands to add a new entry for that profile as well.

This is a fairly complicated addition to this feature, so I'd hold it for
"Command Palette v2", though I believe it's solution deserves special
consideration from the outset.

I suggest that we need a mechanism by which the user can specify a single
command that would be expanded to one command for every profile in the list of
profiles. Consider the following sample:

```json
    "commands": [
        {
            "expandOn": "profiles",
            "icon": "${profile.icon}",
            "name": "New Tab with ${profile.name}",
            "command": { "action": "newTab", "profile": "${profile.name}" }
        },
        {
            "expandOn": "profiles",
            "icon": "${profile.icon}",
            "name": "New Vertical Split with ${profile.name}",
            "command": { "action": "splitPane", "split":"vertical", "profile": "${profile.name}" }
        }
    ],
```

In this example:
* The `"expandOn": "profiles"` property indicates that each command should be
  repeated for each individual profile.
* The `${profile.name}` value is treated as "when expanded, use the given
  profile's name". This allows each command to use the `name` and `icon`
  properties of a `Profile` to customize the text of the command.

To ensure that this works correctly, we'll need to make sure to expand these
commands after all the other settings have been parsed, presumably in the
`Validate` phase. If we do it earlier, it's possible that not all the profiles
from various sources will have been added yet, which would lead to an incomplete
command list.

We'll need to have a placeholder property to indicate that a command should be
expanded for each `Profile`. When the command is first parsed, we'll leave the
format strings `${...}` unexpanded at this time. Then, in the validate phase,
when we encounter a `"expandOn": "profiles"` command, we'll remove it from the
list, and use it as a prototype to generate commands for every `Profile` in our
profiles list. We'll do a string find-and-replace on the format strings to
replace them with the values from the profile, before adding the completed
command to the list of commands.

Of course, how does this work with localization? Considering the [section
below](#localization), we'd update the built-in commands to the following:

```json
    "commands": [
        {
            "iterateOn": "profiles",
            "icon": "${profile.icon}",
            "name": { "key": "NewTabWithProfileCommandName" },
            "command": { "action": "newTab", "profile": "${profile.name}" }
        },
        {
            "iterateOn": "profiles",
            "icon": "${profile.icon}",
            "name": { "key": "NewVerticalSplitWithProfileCommandName" },
            "command": { "action": "splitPane", "split":"vertical", "profile": "${profile.name}" }
        }
    ],
```

In this example, we'll look up the `NewTabWithProfileCommandName` resource when
we're first parsing the command, to find a string similar to `"New Tab with
${profile.name}"`. When we then later expand the command, we'll see the
`${profile.name}` bit from the resource, and expand that like we normally would.

Trickily, we'll need to make sure to have a helper for replacing strings like
this that can be used for general purpose arg parsing. As you can see, the
`profile` property of the `newTab` command also needs the name of the profile.
Either the command validation will need to go through and update these strings
manually, or we'll need another of enabling these `IActionArgs` classes to fill
those parameters in based on the profile being used. Perhaps the command
pre-expansion could just stash the json for the action, then expand it later?
This implementation detail is why this particular feature is not slated for
inclusion in an initial Command Palette implementation.

From initial prototyping, it seems like the best solution will be to stash the
command's original json around when parsing an expandable command like the above
examples. Then, we'll handle the expansion in the settings validation phase,
after all the profiles and color schemes have been loaded.

For each profile, we'll need to replace all the instances in the original json
of strings like `${profile.name}` with the profile's name to create a new json
string. We'll attempt to parse that new string into a new command to add to the
list of commands.

### Commandline Mode

One of our more highly requested features is the ability to run a `wt.exe`
commandline in the current WT window (see [#4472]). Typically, users want the
ability to do this straight from whatever shell they're currently running.
However, we don't really have an effective way currently to know if WT is itself
being called from another WT instance, and passing those arguments to the
hosting WT. Furthermore, in the long term, we see that feature as needing the
ability to not only run commands in the current WT window, but an _arbitrary_ WT
window.

The Command Palette seems like a natural fit for a stopgap measure while we
design the correct way to have a `wt` commandline apply to the window it's
running in.

In Commandline Mode, the user can simply type a `wt.exe` commandline, and when
they hit enter, we'll parse the commandline and dispatch it _to the current
window_. So if the user wants to open a new tab, they could type `new-tab` in
Commandline Mode, and it would open a new tab in the current window. They're
also free to chain multiple commands like they can with `wt` from a shell - by
entering something like `split-pane -p "Windows PowerShell" ; split-pane -H
wsl.exe`, the terminal would execute two `SplitPane` actions in the currently
focused pane, creating one with the "Windows PowerShell" profile and another
with the default profile running `wsl` in it.

## UI/UX Design

We'll add another action that can be used to toggle the visibility of the
command palette. Pressing that keybinding will bring up the command palette. We
should make sure to add a argument to this action that specifies whether the
palette should be opened directly in Action Mode or Commandline Mode.

When the command palette appears, we'll want it to appear as a single overlay
over all of the panes of the Terminal. The drop-down will be centered
horizontally, dropping down from the top (from the tab row). When commands are
entered, it will be implied that they are delivered to the focused terminal
pane. This will help avoid two problematic scenarios that could arise from
having the command palette attached to a single pane:
  * When attached to a single pane, it might be very easy for the UI to quickly
    become cluttered, especially at smaller pane sizes.
  * This avoids the "find the overlay problem" which is common in editors like
    VS where the dialog appears attached to the active editor pane.

The palette will consist of two main UI elements: a text box for
entering/searching for commands, and in action mode, a list of commands.

### Action Mode

The list of commands will be populated with all the commands by default. Each
command will appear like a `MenuFlyoutItem`, with an icon at the left (if it has
one) and the name visible. When opened, the palette will automatically highlight
the first entry in the list.

The user can navigate the list of entries with the arrow keys. Hitting enter
will close the palette and execute the action that's highlighted. Hitting escape
will dismiss the palette, returning control to the terminal. When the palette is
closed for any reason (executing a command, dismissing with either escape or the
`toggleCommandPalette` keybinding), we'll clear out any search text from the
palette, so the user can start fresh again.

We'll also want to enable the command palette to be filterable, so that the user
can type the name of a command, and the command palette will automatically
filter the list of commands. This should be more powerful then just a simple
string compare - the user should be able to type a search string, and get all
the commands that match a "fuzzy search" for that string. This will allow users
to find the command they're looking for without needing to type the entire
command.

For example, consider the following list of commands:

```json
    "commands": [
        { "icon": null, "name": "New Tab", "action": "newTab" },
        { "icon": null, "name": "Close Tab", "action": "closeTab" },
        { "icon": null, "name": "Close Pane", "action": "closePane" },
        { "icon": null, "name": "[-] Split Horizontal", "action": { "action": "splitPane", "split": "horizontal" } },
        { "icon": null, "name": "[ | ] Split Vertical", "action": { "action": "splitPane", "split": "vertical" } },
        { "icon": null, "name": "Next Tab", "action": "nextTab" },
        { "icon": null, "name": "Prev Tab", "action": "prevTab" },
        { "icon": null, "name": "Open Settings", "action": "openSettings" },
        { "icon": null, "name": "Open Media Controls", "action": "openTestPane" }
    ],
```

* "open" should return both "**Open** Settings" and "**Open** Media Controls".
* "Tab" would return "New **Tab**", "Close **Tab**", "Next **Tab**" and "Prev
  **Tab**".
* "P" would return "Close **P**ane", "[-] S**p**lit Horizontal", "[ | ]
  S**p**lit Vertical", "**P**rev Tab", "O**p**en Settings" and "O**p**en Media
  Controls".
* Even more powerfully, "sv" would return "[ | ] Split Vertical" (by matching
  the **S** in "Split", then the **V** in "Vertical"). This is a great example
  of how a user could execute a command with very few keystrokes.

As the user types, we should **bold** each matching character in the command
name, to show how their input correlates to the results on screen.

Additionally, it will be important for commands in the action list to display
the keybinding that's bound to them, if there is one.

### Commandline Mode

Commandline mode is much simpler. In this mode, we'll simply display a text input,
similar to the search box that's rendered for Action Mode. In this box, the
user will be able to type a `wt.exe` style commandline. The user does not need
to start this commandline with `wt` (or `wtd`, etc) - since we're already
running in WT, the user shouldn't really need to repeat themselves.

When the user hits <kbd>enter</kbd>, we'll attempt to parse the commandline. If
we're successful in parsing the commandline, we can close the palette and
dispatch the commandline. If the commandline had errors, we should reveal a text
box with an error message below the text input. We'll leave the palette open
with their entered command, so they can edit the commandline and try again. We
should _probably_ leave the message up for a few seconds once they've begun
editing the commandline, but eventually hide the message (ideally with a motion
animation).

### Switching Between Modes

**TODO**: This is a topic for _discussion_.

How do we differentiate Action Mode from Commandline Mode?

I think there should be a character that the user types that switches the mode.
This is reminiscent of how the command palette works in applications like VsCode
and Sublime Text. The same UI is used for a number of functions. In the case of
VsCode, when the user opens the palette, it's initially in a "navigate to file"
mode. When the user types the prefix character `@`, the menu seamlessly switches
to a "navigate to symbol mode". Similarly, users can use `:` for "go to line"
and `>` enters an "editor command" mode.

I believe we should use a similarly implemented UI. The UI would be in one of
the two modes by default, and typing the prefix character would enter the other
mode. If the user deletes the prefix character, then we'd switch back into the
default mode.

When the user is in Action Mode vs Commandline mode, if the input is empty
(besides potentially the prefix character), we should probably have some sort of
placeholder text visible to indicate which mode the user is in. Something like
_"Enter a command name..."_ for action mode, or _"Type a wt commandline..."_ for
commandline mode.

Initially, I favored having the palette in Action Mode by default, and typing a
`:` prefix to enter Commandline Mode. This is fairly similar to how tmux's
internal command prompt works, which is bound to `<prefix>-:` by default.

If we wanted to remain _similar_ to VsCode, we'd have no prefix character be the
Commandline Mode, and `>` would enter the Action mode. I'd think that might
actually be _backwards_ from what I'd expect, with `>` being the default
character for the end of the default `cmd` `%PROMPT%`.

**FOR DISCUSSION** What option makes the most sense to the team? I'm leaning
towards the VsCode style (where Action='>', Commandline='') currently.

Enabling the user to configure this prefix is discussed below in "[Future
Considerations](#Configuring-The-ActionCommandline-Mode-Prefix)".

### Layering and "Unbinding" Commands

As we'll be providing a list of default commands, the user will inevitably want
to change or remove some of these default commands.

Commands should be layered based upon the _evaluated_ value of the "name"
property. Since the default commands will all use localized strings in the
`"name": { "key": "KeyName" }` format, the user should be able to override the
command based on the localized string for that command.

So, assuming that `NewTabCommandName` is evaluated as "Open New Tab", the
following command
```json
{ "icon": null, "name": { "key": "NewTabCommandName" }, "action": "newTab" },
```

Could be overridden with the command:
```json
{ "icon": null, "name": "Open New Tab", "action": "splitPane" },
```

Similarly, if the user wants to remove that command from the command palette,
they could set the action to `null`:

```json
{ "icon": null, "name": "Open New Tab", "action": null },
```

This will remove the command from the command list.

## Capabilities

### Accessibility

As the entire command palette will be a native XAML element, it'll automatically
be hooked up to the UIA tree, allowing for screen readers to naturally find it.
 * When the palette is opened, it will automatically receive focus.
 * The terminal panes will not be able to be interacted with while the palette
   is open, which will help keep the UIA tree simple while the palette is open.

### Security

This should not introduce any _new_ security concerns. We're relying on the
security of jsoncpp for parsing json. Adding new keys to the settings file
will rely on jsoncpp's ability to securely parse those json values.

### Reliability

We'll need to make sure that invalid commands are ignored. A command could be
invalid because:
* it has a null `name`, or a name with the empty string for a value.
* it has a null `action`, or an action specified that's not an actual
  `ShortcutAction`.

We'll ignore invalid commands from the user's settings, instead of hard
crashing. I don't believe this is a scenario that warrants an error dialog to
indicate to the user that there's a problem with the json.

### Compatibility

We will need to define default _commands_ for all the existing keybinding
commands. With #754, we could add all the actions (that make sense) as commands
to the commands list, so that everyone wouldn't need to define them manually.

### Performance, Power, and Efficiency

We'll be adding a few extra XAML elements to our tree which will certainly
increase our runtime memory footprint while the palette is open.

We'll additionally be introducing a few extra json values to parse, so that could
increase our load times (though this will likely be negligible).

## Potential Issues

This will first require the work in [#1205] to work properly. Right now we
heavily lean on the "focused" element to determine which terminal is "active".
However, when the command palette is opened, focus will move out of the terminal
control into the command palette, which leads to some hard to debug crashes.

Additionally, we'll need to ensure that the "fuzzy search" algorithm proposed
above will work for non-english languages, where a single character might be
multiple `char`s long. As we'll be using a standard XAML text box for input, we
won't need to worry about handling the input ourselves.

### Localization

Because we'll be shipping a set of default commands with the terminal, we should
make sure that list of commands can be localizable. Each of the names we'll give
to the commands should be locale-specific.

To facilitate this, we'll use a special type of object in JSON that will let us
specify a resource name in JSON. We'll use a syntax like the following to
suggest that we should load a string from our resources, as opposed to using the
value from the file:

```json
    "commands": [
        { "icon": null, "name": { "key": "NewTabCommandName" }, "action": "newTab" },
        { "icon": null, "name": { "key": "CloseTabCommandKey" }, "action": "closeTab" },
        { "icon": null, "name": { "key": "ClosePaneCommandKey" }, "action": "closePane" },
        { "icon": null, "name": { "key": "SplitHorizontalCommandKey" }, "action": { "action": "splitPane", "split": "horizontal" } },
        { "icon": null, "name": { "key": "SplitVerticalCommandKey" }, "action": { "action": "splitPane", "split": "vertical" } },
        { "icon": null, "name": { "key": "NextTabCommandKey" }, "action": "nextTab" },
        { "icon": null, "name": { "key": "PrevTabCommandKey" }, "action": "prevTab" },
        { "icon": null, "name": { "key": "OpenSettingsCommandKey" }, "action": "openSettings" },
    ],
```

We'll check at parse time if the `name` property is a string or an object. If
it's a string, we'll treat that string as the literal text. Otherwise, if it's
an object, we'll attempt to use the `key` property of that object to look up a
string from our `ResourceDictionary`. This way, we'll be able to ship localized
strings for all the built-in commands, while also allowing the user to easily
add their own commands.

During the spec review process, we considered other options for localization as
well. The original proposal included options such as having one `defaults.json`
file per-locale, and building the Terminal independently for each locale. Those
were not really feasible options, so we instead settled on this solution, as it
allowed us to leverage the existing localization support provided to us by the
platform.

The `{ "key": "resourceName" }` solution proposed here was also touched on in
[#5280].

### Proposed Defaults

These are the following commands I'm proposing adding to the command palette by
default. These are largely the actions that are bound by default.

```json
"commands": [
    { "icon": null, "name": { "key": "NewTabCommandKey" }, "action": "newTab" },
    { "icon": null, "name": { "key": "DuplicateTabCommandKey" }, "action": "duplicateTab" },
    { "icon": null, "name": { "key": "DuplicatePaneCommandKey" }, "action": { "action": "splitPane", "split":"auto", "splitMode": "duplicate" } },
    { "icon": null, "name": { "key": "SplitHorizontalCommandKey" }, "action": { "action": "splitPane", "split": "horizontal" } },
    { "icon": null, "name": { "key": "SplitVerticalCommandKey" }, "action": { "action": "splitPane", "split": "vertical" } },

    { "icon": null, "name": { "key": "CloseWindowCommandKey" }, "action": "closeWindow" },
    { "icon": null, "name": { "key": "ClosePaneCommandKey" }, "action": "closePane" },

    { "icon": null, "name": { "key": "OpenNewTabDropdownCommandKey" }, "action": "openNewTabDropdown" },
    { "icon": null, "name": { "key": "OpenSettingsCommandKey" }, "action": "openSettings" },

    { "icon": null, "name": { "key": "FindCommandKey" }, "action": "find" },

    { "icon": null, "name": { "key": "NextTabCommandKey" }, "action": "nextTab" },
    { "icon": null, "name": { "key": "PrevTabCommandKey" }, "action": "prevTab" },

    { "icon": null, "name": { "key": "ToggleFullscreenCommandKey" }, "action": "toggleFullscreen" },

    { "icon": null, "name": { "key": "CopyTextCommandKey" }, "action": { "action": "copy", "singleLine": false } },
    { "icon": null, "name": { "key": "PasteCommandKey" }, "action": "paste" },

    { "icon": null, "name": { "key": "IncreaseFontSizeCommandKey" }, "action": { "action": "adjustFontSize", "delta": 1 } },
    { "icon": null, "name": { "key": "DecreaseFontSizeCommandKey" }, "action": { "action": "adjustFontSize", "delta": -1 } },
    { "icon": null, "name": { "key": "ResetFontSizeCommandKey" }, "action": "resetFontSize"  },

    { "icon": null, "name": { "key": "ScrollDownCommandKey" }, "action": "scrollDown" },
    { "icon": null, "name": { "key": "ScrollDownPageCommandKey" }, "action": "scrollDownPage" },
    { "icon": null, "name": { "key": "ScrollUpCommandKey" }, "action": "scrollUp" },
    { "icon": null, "name": { "key": "ScrollUpPageCommandKey" }, "action": "scrollUpPage" }
]
```

## Future considerations

* Commands will provide an easy point for allowing an extension to add its
  actions to the UI, without forcing the user to bind the extension's actions to
  a keybinding
* Also discussed in [#2046] was the potential for adding a command that inputs a
  certain commandline to be run by the shell. I felt that was out of scope for
  this spec, so I'm not including it in detail. I believe that would be
  accomplished by adding a `inputCommand` action, with two args: `commandline`,
  a string, and `suppressNewline`, an optional bool, defaulted to false. The
  `inputCommand` action would deliver the given `commandline` as input to the
  connection, followed by a newline (as to execute the command).
  `suppressNewline` would prevent the newline from being added. This would work
  relatively well, so long as you're sitting at a shell prompt. If you were in
  an application like `vim`, this might be handy for executing a sequence of
  vim-specific keybindings. Otherwise, you're just going to end up writing a
  commandline to the buffer of vim. It would be weird, but not unexpected.
* Additionally mentioned in [#2046] was the potential for profile-scoped
  commands. While that's a great idea, I believe it's out of scope for this
  spec.
* Once [#754] lands, we'll need to make sure to include commands for each action
  manually in the default settings. This will add some overhead that the
  developer will need to do whenever they add an action. That's unfortunate, but
  will be largely beneficial to the end user.
* We could theoretically also display the keybinding for a certain command in
  the `ListViewItem` for the command. We'd need some way to correlate a
  command's action to a keybinding's action. This could be done in a follow-up
  task.
* We might want to alter the fuzzy-search algorithm, to give higher precedence
  in the results list to commands with more consecutive matching characters.
  Alternatively we could give more weight to commands where the search matched
  the initial character of words in the command.
  - For example: `ot` would give more weight to "**O**pen **T**ab" than
    "**O**pen Se**t**tings").
* We may want to add a button to the New Tab Button's dropdown to "Show Command
  Palette". I'm hesitant to keep adding new buttons to that UI, but the command
  palette is otherwise not highly discoverable.
  - We could add another button to the UI to toggle the visibility of the
    command palette. This was the idea initially proposed in [#2046].
  - For both these options, we may want a global setting to hide that button, to
    keep the UI as minimal as possible.
* [#1571] is a request for customizing the "new tab dropdown" menu. When we get
  to discussing that design, we should consider also enabling users to add
  commands from their list of commands to that menu as well.
  - This is included in the spec in [#5888].
* I think it would be cool if there was a small timeout as the user was typing
  in commandline mode before we try to auto-parse their commandline, to check
  for errors. Might be useful to help sanity check users. We can always parse
  their `wt` commandlines safely without having to execute them.
* It would be cool if the commands the user typed in Commandline Mode could be
  saved to a history of some sort, so they could easily be re-entered.
  - It would be especially cool if it could do this across launches.
    - We don't really have any way of storing transient data like that in the
      Terminal, so that would need to be figured out first.
  - Typically the Command Palette is at the top of the view, with the
    suggestions below it, so navigating through the history would be _backwards_
    relative to a normal shell.
* Perhaps users will want the ability to configure which side of the window the
  palette appears on?
  - This might fit in better with [#3327].
* [#3753] is a pull request that covers the addition of an "Advanced Tab
  Switcher". In an application like VsCode, their advanced tab switcher UI is
  similar to their command palette UI. It might make sense that the user could
  use the command palette UI to also navigate to active tabs or panes within the
  terminal, by control name. We've already outlined how the Command Palette
  could operate in "Action Mode" or "Commandline Mode" - we could also add
  "Navigate Mode" on `@`, for navigating between tabs or panes.
  - The tab switcher could probably largely re-use the command palette UI, but
    maybe hide the input box by default.
* We should make sure to add a setting in the future that lets the user opt-in
  to showing most-recently used commands _first_ in the search order, and
  possibly even pre-populating the search box with whatever their last entry
  was.
  - I'm thinking these are two _separate_ settings.

### Nested Commands

Another idea for a future spec is the concept of "nested commands", where a
single command has many sub-commands. This would hide the children commands from
the entire list of commands, allowing for much more succinct top-level list of
commands, and allowing related commands to be grouped together.
- For example, I have a text editor plugin that enables rendering markdown to a
  number of different styles. To use that command in my text editor, first I hit
  enter on the "Render Markdown..." command, then I select which style I want to
  render to, in another list of options. This way, I don't need to have three
  options for "Render Markdown to github", "Render Markdown to gitlab", all in
  the top-level list.
- We probably also want to allow a nested command set to be evaluated at runtime
  somehow. Like if we had a "Open New Tab..." command that then had a nested
  menu with the list of profiles.

The above might be able to be expressed through some JSON like the following:
```json
"commands": [
  {
    "icon": "...",
    "name": { "key": "NewTabWithProfileRootCommandName" },
    "commands": [
      {
        "iterateOn": "profiles",
        "icon": "${profile.icon}",
        "name": { "key": "NewTabWithProfileCommandName" },
        "command": { "action": "newTab", "profile": "${profile.name}" }
      }
    ]
  },
  {
    "icon": "...",
    "name": "Connect to ssh...",
    "commands": [
      {
        "icon": "...",
        "name": "first.com",
        "command": { "action": "newTab", "commandline": "ssh me@first.com" }
      },
      {
        "icon": "...",
        "name": "second.com",
        "command": { "action": "newTab", "commandline": "ssh me@second.com" }
      }
    ]
  }
  {
    "icon": "...",
    "name": { "key": "SplitPaneWithProfileRootCommandName" },
    "commands": [
      {
        "iterateOn": "profiles",
        "icon": "${profile.icon}",
        "name": { "key": "SplitPaneWithProfileCommandName" },
        "commands": [
          {
            "icon": "...",
            "name": { "key": "SplitPaneName" },
            "command": { "action": "splitPane", "profile": "${profile.name}", "split": "automatic" }
          },
          {
            "icon": "...",
            "name": { "key": "SplitPaneVerticalName" },
            "command": { "action": "splitPane", "profile": "${profile.name}", "split": "vertical" }
          },
          {
            "icon": "...",
            "name": { "key": "SplitPaneHorizontalName" },
            "command": { "action": "splitPane", "profile": "${profile.name}", "split": "horizontal" }
          }
        ]
      }
    ]
  }
]
```

This would define three commands, each with a number of nested commands underneath it:
* For the first command:
  - It uses the XAML resource `NewTabWithProfileRootCommandName` as it's name.
  - Activating this command would cause us to remove all the other commands from
    the command palette, and only show the nested commands.
  - It contains nested commands, one for each profile.
    - Each nested command would use the XAML resource
      `NewTabWithProfileCommandName`, which then would also contain the string
      `${profile.name}`, to be filled with the profile's name in the command's
      name.
    - It would also use the profile's icon as the command icon.
    - Activating any of the nested commands would dispatch an action to create a
      new tab with that profile
* The second command:
  - It uses the string literal `"Connect to ssh..."` as it's name
  - It contains two nested commands:
    - Each nested command has it's own literal name
    - Activating these commands would cause us to open a new tab with the
      provided `commandline` instead of the default profile's `commandline`
* The third command:
  - It uses the XAML resource `NewTabWithProfileRootCommandName` as it's name.
  - It contains nested commands, one for each profile.
    - Each one of these sub-commands each contains 3 subcommands - one that will
      create a new split pane automatically, one vertically, and one
      horizontally, each using the given profile.

So, you could imagine the entire tree as follows:

```
<Command Palette>
├─ New Tab With Profile...
│  ├─ Profile 1
│  ├─ Profile 2
│  └─ Profile 3
├─ Connect to ssh...
│  ├─ first.com
│  └─ second.com
└─ New Pane...
   ├─ Profile 1...
   |  ├─ Split Automatically
   |  ├─ Split Vertically
   |  └─ Split Horizontally
   ├─ Profile 2...
   |  ├─ Split Automatically
   |  ├─ Split Vertically
   |  └─ Split Horizontally
   └─ Profile 3...
      ├─ Split Automatically
      ├─ Split Vertically
      └─ Split Horizontally
```

Note that the palette isn't displayed like a tree - it only ever displays the
commands from one single level at a time. So at first, only:

* New Tab With Profile...
* Connect to ssh...
* New Pane...

are visible. Then, when the user <kbd>enter</kbd>'s on one of these (like "New
Pane"), the UI will change to display:

* Profile 1...
* Profile 2...
* Profile 3...

### Configuring the Action/Commandline Mode prefix

As always, I'm also on board with the "this should be configurable by the user"
route, so they can change what mode the command palette is in by default, and
what the prefixes for different modes are, but I'm not sure how we'd define that
cleanly in the settings.

```json
{
  "commandPaletteActionModePrefix": "", // or null, for no prefix
  "commandPaletteCommandlineModePrefix": ">"
}
```

We'd need to have validation on that though, what if both of them were set to
`null`? One of them would _need_ to be `null`, so if both have a character, do
we just assume one is the default?

## Resources
Initial post that inspired this spec: #[2046](https://github.com/microsoft/terminal/issues/2046)

Keybindings args: #[1349](https://github.com/microsoft/terminal/pull/1349)

Cascading User & Default Settings: #[754](https://github.com/microsoft/terminal/issues/754)

Untie "active control" from "currently XAML-focused control" #[1205](https://github.com/microsoft/terminal/issues/1205)

Allow dropdown menu customization in profiles.json [#1571](https://github.com/microsoft/terminal/issues/1571)

Search or run a command in Dropdown menu [#3879]

Spec: Introduce a mini-specification for localized resource use from JSON [#5280]

<!-- Footnotes -->
[#754]: https://github.com/microsoft/terminal/issues/754
[#1205]: https://github.com/microsoft/terminal/issues/1205
[#1142]: https://github.com/microsoft/terminal/pull/1349
[#2046]: https://github.com/microsoft/terminal/issues/2046
[#1571]: https://github.com/microsoft/terminal/issues/1571
[#3879]: https://github.com/microsoft/terminal/issues/3879
[#5280]: https://github.com/microsoft/terminal/pull/5280
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#3327]: https://github.com/microsoft/terminal/issues/3327
[#3753]: https://github.com/microsoft/terminal/pulls/3753
[#5888]: https://github.com/microsoft/terminal/pulls/5888
