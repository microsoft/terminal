---
author: Mike Griese @zadjii-msft
created on: 2019-08-01
last updated: 2019-12-11
issue id: 2046
---

# Spec Title

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

## Solution Design

First off, for the sake of clarity, we'll rename the `command` of a keybinding
to `action`. This will help keep the mental model between commands and actions
clearer. When deserializing keybindings, we'll include a check for the old
`command` key to migrate it.

We'll introduce a new top-level array to the user settings, under the key
`commands`. `commands` will contain an array of commands, each with the
following schema:

```js
{
    "name": string,
    "action": string,
    "icon": string
    "args": object?,
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
`PerformAction(ActionAndArgs)` method (and events for each action).
`AppKeyBindings` will be initialized with a reference to the
`ShortcutActionDispatch` object, so that it can call `PerformAction` on it.
Additionally, by having a singular `ShortcutActionDispatch` instance, we won't
need to re-hook up the ShortcutAction keybindings each time we re-load the
settings.

In `App`, when someone clicks on an item in the list, we'll get the
`ActionAndArgs` associated with that list element, and call PerformAction on
the app's `ShortcutActionDispatch`. This will trigger the event handler just the
same as pressing the keybinding.

### Commands for each profile?

[#3879] Is a request for being able to launch a profile directly, via the
command pallete. Esentially, the user will type the name of a profile, and hit
enter to launch that profile. I quite like this idea, but with the current spec,
this won't work great. We'd need to manually have one entry in the command
palette for each profile, and every time the user adds a profile, they'd need to
update the list of commands to add a new entry for that profile as well.

This is a fairly complicated addition to this feature, so I'd hold it for
"Command Palette v2", thoguh I believe it's solution deserves special
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

In this example, we'll look up the `NewTabWithProfileCommandName` resource when we're first parsing the command, to find a string similar to `"New Tab with ${profile.name}"`. When we then later expand the command, we'll see the `${profile.name}` bit from the resource, and expand that like we normally would.

Trickily, we'll need to make sure to have a helper for replacing strings like
this that can be used for general purpose arg parsing. As you can see, the
`profile` property of the `newTab` command also needs the name of the profile.
Either the command validator will need to go through and update these strings
manually, or we'll need another of enabling these `IActionArgs` classes to fill
those parameters in based on the profile being used. Perhaps the command
pre-expansion could just stash the json for the action, then expand it later?
This implementation detail is why this particualr feature is not slated for
inclusion in an initial Command Palette implementation.

## UI/UX Design

We'll add another action that can be used to toggle the visibility of the
command palette. Pressing that keybinding will bring up the command palette.

When the command palette appears, we'll want it to appear as a single overlay
over all of the panes of the Terminal. The drop-down will be centered
horizontally, dropping down from the top (from the tab row). When commands are
entered, it will be implied that they are delivered to the focused terminal
pane. This will help avoid two problematic scenarios that could arise from
having the command palette attache to a single pane:
  * When attached to a single pane, it might be very easy for the UI to quickly
    become cluttered, especially at smaller pane sizes.
  * This avoids the "find the overlay problem" which is common in editors like
    VS where the dialog appears attached to the active editor pane.

The palette will consist of two main UI elements: a text box for searching for
commands, and a list of commands.

The list of commands will be populated with all the commands by default. Each
command will appear like a `MenuFlyoutItem`, with an icon at the left (if it has
one) and the name visible. When opened, the palette will automatically highlight
the first entry in the list.

The user can navigate the list of entries with the arrow keys. Hitting enter
will close the palette and execute the action that's highlighted. Hitting escape
will dismiss the pallete, returning control to the terminal. When the palette is
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
        { "icon": null, "name": "[-] Split Horizontal", "action": "splitHorizontal" },
        { "icon": null, "name": "[ | ] Split Vertical", "action": "splitVertical" },
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
* Even more powerfully, "sv" would return "[ | ] **S**plit **V**ertical". This
  is a great example of how a user could execute a command with very few
  keystrokes.

As the user types, we should **bold** each matching character in the command
name, to show how their input correlates to the results on screen.

## Capabilities

### Accessibility

As the entire command palette will be a native XAML element, it'll automatically
be hooked up to the UIA tree, allowing for screen readers to naturally find it.
 * When the palette is opened, it will automatically recieve focus.
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

We'll aditionally be introducing a few extra json values to parse, so that could
increase our load times (though this will likely be negligible).

## Potential Issues

This will first require the work in [#1205] to work properly. Right now we
heavily lean on the "focused" element to determine which terminal is "active".
However, when the command palette is opened, focus will move out of the terminal
control into the command palette, which leads to some hard to debug crashes.

Additionally, we'll need to ensure that the "fuzzy search" algorithm proposed
above will work for non-english languages, where a single charater might be
multiple `char`s long. As we'll be using a standard XAML text box for input, we
won't need to worry about handling the input ourselves.

### Localization

Because we'll be shipping a set of default commands with the terminal, we should
make sure that list of commands can be localizable. Each of the names we'll give
to the commands should be locale-specific. This would require a change to the
generation of the `defaults.json` file.

1. Should we ship one defaults.json per locale (that we support), and use the
   system locale at runtime to load the one we want? This would mean that when
   the user opens the `defaults.json` file, they'll open a file like
   `defaults.en-us.json`.
2. Should we somehow build the Terminal for each locale separately? This doesn't
   seem like a feasible choice.
3. Use strings straight from the resources file somehow?

Option 1 seems more similar to how XAML resources work today, where all of the
resources are compiled into the app, but only the appropriate one is used at
runtime. We would need to be able to generate a variety of `defaults.LANG.json`
files based upon entries in another file, with a list of locales we support, and
a map of ShortcutAction->name pairings for each language. This set of json files
should be generated at build time.

We'll need to modify `GenerateHeaderForJson.ps1` to generate headers for _all_
these files, and we'll additionally need to create a mechanism by which all
these generated headers are included, not just the singular `defaults.json`. We
could combine all the per-language defaults strings into globals within a single
header, so that we don't need to worry about including lots of files.

If at runtime we _don't_ find a `defaults.json` for the current locale, we'll
fall back to the en-us variant.

Overall, this seems wildly overcomplicated.

Option 3 was suggested in passing one of the few times I was in the office.
We'll use a syntax like the following to suggest that we should load a string
from our resources, as opposed to using the value from the file:

```json
    "commands": [
        { "icon": null, "name": { "key": "NewTabCommandName" }, "action": "newTab" },
        { "icon": null, "name": { "key": "CloseTabCommandKey" }, "action": "closeTab" },
        { "icon": null, "name": { "key": "ClosePaneCommandKey" }, "action": "closePane" },
        { "icon": null, "name": { "key": "SplitHorizontalCommandKey" }, "action": "splitHorizontal" },
        { "icon": null, "name": { "key": "SplitVerticalCommandKey" }, "action": "splitVertical" },
        { "icon": null, "name": { "key": "NextTabCommandKey" }, "action": "nextTab" },
        { "icon": null, "name": { "key": "PrevTabCommandKey" }, "action": "prevTab" },
        { "icon": null, "name": { "key": "OpenSettingsCommandKey" }, "action": "openSettings" },
    ],
```

We'll check at parse time if the `name` property is a string or an object. If
it's a string, we'll treat that string as the literal text. Otherwise, if it's
an object, we'll attempt to use the `key` property of that object to look up a
string from our `ResourceDictionary`. This way, we'll be able to ship localized
strings for all the built-in commands, while aslo allowing the user to easily
add their own commands.


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
  commadline to the buffer of vim. It would be weird, but not unexpected.
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
* We might want to alter the fuzzy-search algorithim, to give higher precedence
  in the results list to commands with more consecutive matching characters.
  Alternatively we could give more weight to commands where the search matched
  the initial character of words in the command.
  - For example: `ot` would give more weight to "**O**pen **T**ab" than
    "**O**pen Se**t**tings").
* Another idea for a future spec is the concept of "nested commands", where a
  single command has many sub-commands. This would hide the children commands
  from the entire list of commands, allowing for much more succinct top-level
  list of commands, and allowing related commands to be grouped together.
  - For example, I have a text editor plugin that enables rendering markdown to
    a number of different styles. To use that command in my text editor, first I
    hit enter on the "Render Markdown..." command, then I select which style I
    want to render to, in another list of options. This way, I don't need to
    have three options for "Render Markdown to github", "Render Markdown to
    gitlab", all in the top-level list.
  - We probably also want to allow a nested command set to be evaluated at
    runtime somehow. Like if we had a "Open New Tab..." command that then had a
    nested menu with the list of profiles.
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

## Resources
Initial post that inspired this spec: #[2046](https://github.com/microsoft/terminal/issues/2046)

Keybindings args: #[1349](https://github.com/microsoft/terminal/pull/1349)

Cascading User & Default Settings: #[754](https://github.com/microsoft/terminal/issues/754)

Untie "active control" from "currently XAML-focused control" #[1205](https://github.com/microsoft/terminal/issues/1205)

Allow dropdown menu customization in profiles.json [#1571](https://github.com/microsoft/terminal/issues/1571)

Search or run a command in Dropdown menu [#3879]

<!-- Footnotes -->
[#754]: https://github.com/microsoft/terminal/issues/754
[#1205]: https://github.com/microsoft/terminal/issues/1205
[#1142]: https://github.com/microsoft/terminal/pull/1349
[#2046]: https://github.com/microsoft/terminal/issues/2046
[#1571]: https://github.com/microsoft/terminal/issues/1571
[#3879]: https://github.com/microsoft/terminal/issues/3879
