---
author: Mike Griese @zadjii-msft
created on: 2020-5-13
last updated: 2020-6-02
issue id: 1571
---

# New Tab Menu Customization

## Abstract

Many users have lots and _lots_ of profiles that they use. Some of these
profiles the user might not use that frequently. When that happens, the new tab
dropdown can become quite cluttered.

A common ask is for the ability to reorder and reorganize this dropdown. This
spec provides a design for how the user might be able to specify the
customization in their settings.

## Inspiration

Largely, this spec was inspired by discussion in
[#1571](https://github.com/microsoft/terminal/issues/1571#issuecomment-519504048)
and the _many_ linked threads.

## Solution Design

This design proposes adding a new setting `"newTabMenu"`. When unset, (the
default), the new tab menu is populated with all the profiles, in the order they
appear in the users settings file. When set, this enables the user to control
the appearance of the new tab dropdown. Let's take a look at an example:

```json
{
    "profiles":{ ... },
    "newTabMenu": [
        { "type":"profile", "profile": "cmd" },
        { "type":"profile", "profile": "Windows PowerShell" },
        { "type":"separator" },
        {
            "type":"folder",
            "name": "ssh",
            "icon": "C:\\path\\to\\icon.png",
            "entries":[
                { "type":"profile", "profile": "Host 1" },
                { "type":"profile", "profile": "8.8.8.8" },
                { "type":"profile", "profile": "Host 2" }
            ]
        },
        { "type":"separator" },
        { "type":"profile", "profile": "Ubuntu-18.04" },
        { "type":"profile", "profile": "Fedora" }
    ]
}
```

If a user were to use this as their new tab menu, that they would get is a menu
that looks like this:

![fig 1](Menu-Customization-000.png)

_fig 1_: A _very rough_ mockup of what this feature might look like

There are three `type`s of objects in this menu:
* `"type":"profile"`: This is a profile. Clicking on this entry will open a new
  tab, with that profile. The profile is identified with the `"profile"`
  parameter, which accepts either a profile `name` or GUID.
* `"type":"separator"`: This represents a XAML `MenuFlyoutSeparator`, enabling
  the user to visually space out entries.
* `"type":"folder"`: This represents a nested menu of entries.
  - The `"name"` property provides a string of text to display for the group.
  - The `"icon"` property provides a path to a image to use as the icon. This
    property is optional.
  - The `"entries"` property specifies a list of menu entries that will appear
    nested under this entry. This can contain other `"type":"folder"` groups as
    well!
* `"type":"action"`: This represents a menu entry that should execute a specific
  `ShortcutAction`.
  - The `"name"` property provides a string of text to display for the action.
  - The `"icon"` property provides a path to a image to use as the icon. This
    property is optional.
  - The `"name"` property specifies a shortcut action similar to the "command"
    property in keybindings. If this is a string, we'll treat it as a name of a
    `ShortcutAction`. If it's an object, we'll treat it like an `ActionAndArgs`

## UI/UX Design

See the above _figure 1_.

The profile's `icon` will also appear as the icon on `profile` entries. If
there's a keybinding bound to open a new tab with that profile, then that will
also be added to the `MenuFlyoutItem` as the accelerator text, similar to the
text we have nowadays.

Beneath the list of profiles will _always_ be the same "Settings", "Feedback"
and "About" entries, separated by a `MenuFlyoutSeparator`. This is consistent
with the UI as it exists with no customization. These entries cannot be removed
with this feature, only the list of profiles customized.

## Capabilities

### Accessibility

This menu will be added to the XAML tree in the same fashion as the current new
tab flyout, so there should be no dramatic change here.

### Security

_(no change expected)_

### Reliability

_(no change expected)_

### Compatibility

_(no change expected)_

### Performance, Power, and Efficiency

## Potential Issues

Currently, the `openTab` and `splitPane` keybindings will accept a `index`
parameter to say "create a new tab/pane with the profile at index N in the new
tab dropdown". With this change, the N'th entry in the menu won't always be a
profile. It might be a folder with more options, or it might be an action (that
might not be opening a new tab/pane at all).

Given all the above scenarios, I'm proposing the following behavior:

For a `newTab` or `splitPane` action with an `"index":N` parameter, if the Nth
top-level entry in the new tab menu is a:
* `"type":"profile"`: perform the `newTab` or `splitPane` action with that profile.
* `"type":"folder"`: Focus the first element in the sub menu, so the user could
  navigate it with the keyboard.
* `"type":"separator"`: Ignore these when counting top-level entries.
* `"type":"action"`: Do nothing. During settings validation, display a warning
  to the user that the keybinding in question won't do anything.

So for example:

```
New Tab Button
├─ Folder 1
│  └─ Profile A
│  └─ Action B
├─ Separator
├─ Folder 2
│  └─ Profile C
│  └─ Profile D
├─ Action E
└─ Profile F
```

* <kbd>ctrl+shift+1</kbd> focuses "Profile A", but the user needs to press enter/space to creates a new tab/split
* <kbd>ctrl+shift+2</kbd> focuses "Profile C", but the user needs to press enter/space to creates a new tab/split
* <kbd>ctrl+shift+3</kbd> performs Action E
* <kbd>ctrl+shift+4</kbd> Creates a new tab/split with Profile F

> 👉 NOTE: A previous version of this spec considered leaving the `index`
> parameter to work with  profiles _in the order specified in the user's
> settings_, not the order they appear in this dropdown. Since a user might have
> nested profiles in this dropdown, picking either a depth-first or
> breadth-first ordering would be confusing.

## Future considerations

* The user could set a `"name"`/`"text"`, or `"icon"` property to these menu
  items manually, to override the value from the profile
    - This would be especially useful for the `"folder"` or aforementioned
      `"action"` menu entry
* [#2046] covers the addition of a "Command Palette", and commands that the user
  can run using that UI. Once that's added, we should make sure that we also
  support either:
  - A `ShortcutAction` for running a `command` from the command palette
  - A `{ "type": "command", "command": "<command name>" }` type of entry, that
    automatically uses the command's name and icon for the entry in the entry.


<!-- Footnotes -->
[#2046]: https://github.com/microsoft/terminal/issues/2046
