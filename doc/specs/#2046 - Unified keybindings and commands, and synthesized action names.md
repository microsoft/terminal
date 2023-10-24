---
author: Mike Griese @zadjii-msft
created on: 2020-06-15
last updated: 2020-06-19
issue id: 2046
---

# Command Palette, Addendum 1 -  Unified keybindings and commands, and synthesized action names

## Abstract

This document is intended to serve as an addition to the [Command Palette Spec].
While that spec is complete in it's own right, subsequent discussion revealed
additional ways to improve the functionality and usability of the command
palette. This document builds largely on the topics already introduced in the
original spec, so readers should first familiarize themselves with that
document.

One point of note from the original document was that the original specification
was entirely too verbose when defining both keybindings and commands for
actions. Consider, for instance, a user that wants to bind the action "duplicate
the current pane". In that spec, they need to add both a keybinding and a
command:

```json
{
    "keybindings": [
        { "keys": [ "ctrl+alt+t" ], "command": { "action": "splitPane", "split":"auto", "splitMode": "duplicate" } },
    ],
    "commands": [
        { "name": "Duplicate Pane", "action": { "action": "splitPane", "split":"auto", "splitMode": "duplicate" }, "icon": null },
    ]
}
```

These two entries are practically the same, except for two key differentiators:
* the keybinding has a `keys` property, indicating which key chord activates the
  action.
* The command has a `name` property, indicating what name to display for the
  command in the Command Palette.

What if the user didn't have to duplicate this action? What if the user could
just add this action once, in their `keybindings` or `commands`, and have it
work both as a keybinding AND a command?


## Solution Design

This spec will outline two primary changes to keybindings and commands.
1. Unify keybindings and commands, so both `keybindings` and `commands` can
   specify either actions bound to keys, and/or actions bound to entries in the
   Command Palette.
2. Propose a mechanism by which actions do not _require_ a `name` to appear in
   the Command Palette.

These proposals are two atomic units - either could be approved or rejected
independently of one another. They're presented together here in the same doc
because together, they present a compelling story.

### Proposal 1: Unify Keybindings and Commands

As noted above, keybindings and commands have nearly the exact same syntax, save
for a couple properties. To make things easier for the user, I'm proposing
treating everything in _both_ the `keybindings` _and_ the `commands` arrays as
**BOTH** a keybinding and a command.

Furthermore, as a change from the previous spec, we'll be using `bindings` from
here on as the unified `keybindings` and `commands` lists. This is considering
that we'll currently be using `bindings` for both commands and keybindings, but
we'll potentially also have mouse & touch bindings in this array in the future.
We'll "deprecate" the existing `keybindings` property, and begin to exclusively
use `bindings` as the new property name. For compatibility reasons, we'll
continue to parse `keybindings` in the same way we parse `bindings`. We'll
simply layer `bindings` on top of the legacy `keybindings`.

* Anything entry that has a `keys` value will be added to the keybindings.
  Pressing that keybinding will activate the action defined in `command`.
* Anything with a `name`<sup>[1]</sup> will be added as an entry (using that
  name) to the Command Palette's Action Mode.

###### Caveats

* **Nested commands** (commands with other nested commands). If a command has
  nested commands in the `commands` property, AND a `keys` property, then
  pressing that keybinding should open the Command Palette directly to that
  level of nesting of commands.
* **"Iterable" commands** (with an `iterateOn` property): These are commands
  that are expanded into one command per profile. These cannot really be bound
  as keybindings - which action should be bound to the key? They can't all be
  bound to the same key. If a KeyBinding/Command json blob has a valid
  `iterateOn` property, then we'll ignore it as a keybinding. This includes any
  commands that are nested as children of this command - we won't be able to
  know which of the expanded children will be the one to bind the keys to.

<sup>[1]</sup>: This requirement will be relaxed given **Proposal 2**, below,
but ignored for the remainder of this section, for illustrative purposes.

#### Example

Consider the following settings:

```json
"bindings": [
  { "name": "Duplicate Tab", "command": "duplicateTab", "keys": "ctrl+alt+a" },
  { "command": "nextTab", "keys": "ctrl+alt+b" },
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
        "keys": "ctrl+alt+c",
        "icon": "...",
        "name": "first.com",
        "command": { "action": "newTab", "commandline": "ssh me@first.com" }
      },
      {
        "keys": "ctrl+alt+d",
        "icon": "...",
        "name": "second.com",
        "command": { "action": "newTab", "commandline": "ssh me@second.com" }
      }
    ]
  }
  {
    "keys": "ctrl+alt+e",
    "icon": "...",
    "name": { "key": "SplitPaneWithProfileRootCommandName" },
    "commands": [
      {
        "iterateOn": "profiles",
        "icon": "${profile.icon}",
        "name": { "key": "SplitPaneWithProfileCommandName" },
        "commands": [
          {
            "keys": "ctrl+alt+f",
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

This will generate a tree of commands as follows:

```
<Command Palette>
├─ Duplicate tab { ctrl+alt+a }
├─ New Tab With Profile...
│  ├─ Profile 1
│  ├─ Profile 2
│  └─ Profile 3
├─ Connect to ssh...
│  ├─ first.com { ctrl+alt+c }
│  └─ second.com { ctrl+alt+d }
└─ New Pane... { ctrl+alt+e }
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

Note also the keybindings in the above example:
* <kbd>ctrl+alt+a</kbd>: This key chord is bound to the "Duplicate tab"
  (`duplicateTab`) action, which is also bound to the command with the same
  name.
* <kbd>ctrl+alt+b</kbd>: This key chord is bound to the `nextTab` action, which
  doesn't have an associated command.
* <kbd>ctrl+alt+c</kbd>: This key chord is bound to the "Connect to
  ssh../first.com" action, which will open a new tab with the `commandline`
  `"ssh me@first.com"`. When the user presses this keybinding, the action will
  be executed immediately, without the Command Palette appearing.
* <kbd>ctrl+alt+d</kbd>: This is the same as the above, but with the "Connect to
  ssh../second.com" action.
* <kbd>ctrl+alt+e</kbd>: This key chord is bound to opening the Command Palette
  to the "New Pane..." command's menu. When the user presses this keybinding,
  they'll be prompted with this command's sub-commands:
  ```
  Profile 1...
  Profile 2...
  Profile 3...
  ```
* <kbd>ctrl+alt+f</kbd>: This key will _not_ be bound to any action. The parent
  action is iterable, which means that the `SplitPaneName` command is going to
  get turned into one command for each and every profile, and therefore cannot
  be bound to just a single action.

### Proposal 2: Automatically synthesize action names

Previously, all Commands were required to have a `name`. This name was used as
the text for the action in the Action Mode of the Command Palette. However, this
is a little tedious for users who already have lots of keys bound. They'll need
to go through and add names to each of their existing keybindings to ensure that
the actions appear in the palette. Could we instead synthesize the names for the
commands ourselves? This  would enable users to automatically get each of their
existing keybindings to appear in the palette without any extra work.

To support this, the following changes will be made:
* `ActionAndArgs` will get a `GenerateName()` method defined. This will create a
  string describing the `ShortcutAction` and it's associated `ActionArgs`.
  - Not EVERY action _needs_ to define a result for `GenerateName`. Actions that
    don't _won't_ be automatically added to the Command Palette.
  - Each of the strings used in `GenerateName` will need to come from our
    resources, so they can be localized appropriately.
* When we're parsing commands, if a command doesn't have a `name`, we'll instead
  attempt to use `GenerateName` to create the unique string for the action
  associated with this command. If the command does have a `name` set, we'll use
  that string instead, allowing the user to override the default name.
  - If a command has it's name set to `null`, then we'll ignore the command
    entirely, not just use the generated name.

[**Appendix 1**](#appendix-1-name-generation-samples-for-ShortcutActions) below
shows a complete sample of the strings that will be generated for each of the existing
`ShortcutActions`, and many of the actions that have been proposed, but not yet
implemented.

These strings should be human-friendly versions of the actions and their
associated args. For some of these actions, with very few arguments, the strings
can be relatively simple. Take for example, `CopyText`:

JSON | Generated String
-- | --
`{ "action":"copyText" }` | "Copy text"
`{ "action":"copyText", "singleLine": true }` | "Copy text as a single line"
`{ "action":"copyText", "singleLine": false, "copyFormatting": false }` | "Copy text without formatting"
`{ "action":"copyText", "singleLine": true, "copyFormatting": true }` | "Copy text as a single line without formatting"

CopyText is a bit of a simplistic case however, with very few args or
permutations of argument values. For things like `newTab`, `splitPane`, where
there are many possible arguments and values, it will be acceptable to simply
append `", property:value"` strings to the generated names for each of the set
values.

For example:

JSON | Generated String
-- | --
`{ "action":"newTab", "profile": "Hello" }` | "Open a new tab, profile:Hello"
`{ "action":"newTab", "profile": "Hello", "directory":"C:\\", "commandline": "wsl.exe", title": "Foo" }` | "Open a new tab, profile:Hello, directory:C:\\, commandline:wsl.exe, title:Foo"


This is being chosen in favor of something that might be more human-friendly,
like "Open a new tab with profile {profile name} in {directory} with
{commandline} and a title of {title}". This string would be much harder to
synthesize, especially considering localization concerns.

#### Remove the resource key notation

Since we'll be using localized names for each of the actions in `GenerateName`,
we no longer _need_ to provide the `{ "name":{ "key": "SomeResourceKey" } }`
syntax introduced in the original spec. This functionality was used to allow us
to define localizable names for the default commands.

However, I think we should keep this functionality, to allow us additional
flexibility when defining default commands.

### Complete Defaults

Considering both of the above proposals, the default keybindings and commands
will be defined as follows:

* The current default keybindings will be untouched. These actions will
  automatically be added to the Command Palette, using their names generated
  from `GenerateName`.
  - **TODO: FOR DISCUSSION**: Should we manually set the names for the default
    "New Tab, profile index: 0" keybindings to `null`? This seems like a not
    terribly helpful name for the Command Palette, especially considering the
    iterable commands listed below.
* We'll add a few new commands:
  - A nested, iterable command for "Open new tab with
    profile..."/"Profile:{profile name}"
  - A nested, iterable command for "Select color scheme..."/"{scheme name}"
  - A nested, iterable command for "New Pane..."/"Profile:{profile
    name}..."/["Automatic", "Horizontal", "Vertical"]
  > 👉 NOTE: These default nested commands can be removed by the user defining
  > `{ "name": "Open new tab with profile...", "action":null }` (et al) in their
  > settings.
  - If we so chose, in the future we can add further commands that we think are
    helpful to `defaults.json`, without needing to give them keys. For example,
    we could add
    ```json
    { "command": { "action": "copy", "singleLine": true } }
    ```
    to `bindings`, to add a "copy text as a single line" command, without
    necessarily binding it to a keystroke.


These changes to the `defaults.json` are represented in json as the following:

```json
"bindings": [
  {
    "icon": null,
    "name": { "key": "NewTabWithProfileRootCommandName" },
    "commands": [
      {
        "iterateOn": "profiles",
        "icon": "${profile.icon}",
        "name": "${profile.name}",
        "command": { "action": "newTab", "profile": "${profile.name}" }
      }
    ]
  },
  {
    "icon": null,
    "name": { "key": "SelectColorSchemeRootCommandName" },
    "commands": [
      {
        "iterateOn": "schemes",
        "icon": null,
        "name": "${scheme.name}",
        "command": { "action": "selectColorScheme", "scheme": "${scheme.name}" }
      }
    ]
  },
  {
    "icon": null,
    "name": { "key": "SplitPaneWithProfileRootCommandName" },
    "commands": [
      {
        "iterateOn": "profiles",
        "icon": "${profile.icon}",
        "name": { "key": "SplitPaneWithProfileCommandName" },
        "commands": [
          {
            "icon": null,
            "name": { "key": "SplitPaneName" },
            "command": { "action": "splitPane", "profile": "${profile.name}", "split": "automatic" }
          },
          {
            "icon": null,
            "name": { "key": "SplitPaneVerticalName" },
            "command": { "action": "splitPane", "profile": "${profile.name}", "split": "vertical" }
          },
          {
            "icon": null,
            "name": { "key": "SplitPaneHorizontalName" },
            "command": { "action": "splitPane", "profile": "${profile.name}", "split": "horizontal" }
          }
        ]
      }
    ]
  }
]
```

A complete diagram of what the default Command Palette will look like given the
default keybindings and these changes is given in [**Appendix
2**](#appendix-2-complete-default-command-palette).

## Concerns

**DISCUSSION**: "New tab with index {index}". How does this play with
the new tab dropdown customizations in [#5888]? In recent iterations of that
spec, we changed the meaning of `{ "action": "newTab", "index": 1 }` to mean
"open the first entry in the new tab menu". If that's a profile, then we'll open
a new tab with it. If it's an action, we'll perform that action. If it's a
nested menu, then we'll open the menu to that entry.

Additionally, how exactly does that play with something like `{ "action":
"newTab", "index": 1, "commandline": "wsl.exe" }`? This is really a discussion
for that spec, but is an issue highlighted by this spec. If the first entry is
anything other than a `profile`, then the `commandline` parameter doesn't really
mean anything anymore. I'm tempted to revert this particular portion of the new
tab menu customization spec over this.

We could instead add an `index` to `openNewTabDropdown`, and have that string
instead be "Open new tab dropdown, index:1". That would help disambiguate the
two.

Following discussion, it was decided that this was in fact the cleanest
solution, when accounting for both the needs of the new tab dropdown and the
command palette. The [#5888] spec has been updated to reflect this.

## Future considerations

* Some of these command names are starting to get _very_ long. Perhaps we need a
  netting to display Command Palette entries on two lines (or multiple, as
  necessary).
* When displaying the entries of a nested command to the user, should we display
  a small label showing the name of the previous command? My gut says _yes_. In
  the Proposal 1 example, pressing `ctrl+alt+e` to jump to "Split Pane..."
  should probably show a small label that displays "Split Pane..." above the
  list of nested commands.
* It wouldn't be totally impossible to allow keys to be bound to an iterable
  command, and then simply have the key work as "open the command palette with
  only the commands generated by this iterable command". This is left as a
  future option, as it might require some additional technical plumbing.

## Appendix 1: Name generation samples for `ShortcutAction`s

### Current `ShortcutActions`

* `CopyText`
  - "Copy text"
  - "Copy text as a single line"
  - "Copy text without formatting"
  - "Copy text as a single line without formatting"
* `PasteText`
  - "Paste text"
* `OpenNewTabDropdown`
  - "Open new tab dropdown"
* `DuplicateTab`
  - "Duplicate tab"
* `NewTab`
  - "Open a new tab, profile:{profile name}, directory:{directory}, commandline:{commandline}, title:{title}"
* `NewWindow`
  - "Open a new window"
  - "Open a new window, profile:{profile name}, directory:{directory}, commandline:{commandline}, title:{title}"
* `CloseWindow`
  - "Close window"
* `CloseTab`
  - "Close tab"
* `ClosePane`
  - "Close pane"
* `NextTab`
  - "Switch to the next tab"
* `PrevTab`
  - "Switch to the previous tab"
* `SplitPane`
  - "Open a new pane, profile:{profile name}, split direction:{direction}, split size:{X%/Y chars}, resize parents, directory:{directory}, commandline:{commandline}, title:{title}"
  - "Duplicate the current pane, split direction:{direction}, split size:{X%/Y chars}, resize parents, directory:{directory}, commandline:{commandline}, title:{title}"
* `SwitchToTab`
  - "Switch to tab {index}"
* `AdjustFontSize`
  - "Increase the font size"
  - "Decrease the font size"
* `ResetFontSize`
  - "Reset the font size"
* `ScrollUp`
  - "Scroll up a line"
  - "Scroll up {amount} lines"
* `ScrollDown`
  - "Scroll down a line"
  - "Scroll down {amount} lines"
* `ScrollUpPage`
  - "Scroll up a page"
  - "Scroll up {amount} pages"
* `ScrollDownPage`
  - "Scroll down a page"
  - "Scroll down {amount} pages"
* `ResizePane`
  - "Resize pane {direction}"
  - "Resize pane {direction} {percent}%"
* `MoveFocus`
  - "Move focus {direction}"
* `Find`
  - "Toggle the search box"
* `ToggleFullscreen`
  - "Toggle fullscreen mode"
* `OpenSettings`
  - "Open settings"
  - "Open settings file"
  - "Open default settings file"
* `ToggleCommandPalette`
  - "Toggle the Command Palette"
  - "Toggle the Command Palette in commandline mode"

### Other yet unimplemented actions:
* `SwitchColorScheme`
  - "Select color scheme {name}"
* `ToggleRetroEffect`
  - "Toggle the retro terminal effect"
* `ExecuteCommandline`
  - "Run a wt commandline: {cmdline}"
* `ExecuteActions`
  - OPINION: THIS ONE SHOULDN'T HAVE A NAME. We're not including any of these by
    default. The user knows what they're putting in the settings by adding this
    action, let them name it.
  - Alternatively: "Run actions: {action.ToName() for action in actions}"
* `SendInput`
  - OPINION: THIS ONE SHOULDN'T HAVE A NAME. We're not including any of these by
    default. The user knows what they're putting in the settings by adding this
    action, let them name it.
* `ToggleMarkMode`
  - "Toggle Mark Mode"
* `NextTab`
  - "Switch to the next most-recent tab"
* `SetTabColor`
  - "Set the color of the current tab to {#color}"
    * It would be _really_ cool if we could display a sample of the color
      inline, but that's left as a future consideration.
  - "Set the color for this tab..."
    * this command isn't nested, but hitting enter immediately does something
      with the UI, so that's _fine_
* `RenameTab`
  - "Rename this tab to {name}"
  - "Rename this tab..."
    * this command isn't nested, but hitting enter immediately does something
      with the UI, so that's _fine_


## Appendix 2: Complete Default Command Palette

This diagram shows what the default value of the Command Palette would be. This
assumes that the user has 3 profiles, "Profile 1", "Profile 2", and "Profile 3",
as well as 3 schemes: "Scheme 1", "Scheme 2", and "Scheme 3".

```
<Command Palette>
├─ Close Window
├─ Toggle fullscreen mode
├─ Open new tab dropdown
├─ Open settings
├─ Open default settings file
├─ Toggle the search box
├─ New Tab
├─ New Tab, profile index: 0
├─ New Tab, profile index: 1
├─ New Tab, profile index: 2
├─ New Tab, profile index: 3
├─ New Tab, profile index: 4
├─ New Tab, profile index: 5
├─ New Tab, profile index: 6
├─ New Tab, profile index: 7
├─ New Tab, profile index: 8
├─ Duplicate tab
├─ Switch to the next tab
├─ Switch to the previous tab
├─ Switch to tab 0
├─ Switch to tab 1
├─ Switch to tab 2
├─ Switch to tab 3
├─ Switch to tab 4
├─ Switch to tab 5
├─ Switch to tab 6
├─ Switch to tab 7
├─ Switch to tab 8
├─ Close pane
├─ Open a new pane, split: horizontal
├─ Open a new pane, split: vertical
├─ Duplicate the current pane
├─ Resize pane down
├─ Resize pane left
├─ Resize pane right
├─ Resize pane up
├─ Move focus down
├─ Move focus left
├─ Move focus right
├─ Move focus up
├─ Copy Text
├─ Paste Text
├─ Scroll down a line
├─ Scroll down a page
├─ Scroll up a line
├─ Scroll up a page
├─ Increase the font size
├─ Decrease the font size
├─ Reset the font size
├─ New Tab With Profile...
│  ├─ Profile 1
│  ├─ Profile 2
│  └─ Profile 3
├─ Select Color Scheme...
│  ├─ Scheme 1
│  ├─ Scheme 2
│  └─ Scheme 3
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


<!-- Footnotes -->
[Command Palette Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%232046%20-%20Command%20Palette.md
