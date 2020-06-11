---
author: Carlos Zamora @carlos-zamora
created on: 2020-05-14
last updated: 2020-05-14
issue id: #2557
---

# Open Settings Keybinding

## Abstract

This spec outlines an expansion to the existing `openSettings` keybinding.

## Inspiration

As a Settings UI becomes more of a reality, the behavior of this keybinding will be expanded on to better interact with the UI. Prior to a Settings UI, there was only one concept of the modifiable user settings: settings.json.

Once the Settings UI is created, we can expect users to want to access the following scenarios:
- Settings UI: globals page
- Settings UI: profiles page
- Settings UI: color schemes page
- Settings UI: keybindings page
- settings.json
- defaults.json
These are provided as non-comprehensive examples of pages that might be in a future Settings UI. The rest of the doc assumes these are the pages in the Settings UI.


## Solution Design
Originally, #2557 was intended to allow for a keybinding arg to access defaults.json. I imagined a keybinding arg such as "openDefaults: true/false" to accomplish this. However, this is not expandable in the following scenarios:
- what if we decide to create more settings files in the future? (i.e. themes.json, extensions.json, etc...)
- when the Settings UI comes in, there is ambiguity as to what `openSettings` does (json? UI? Which page?)

### Proposition 1.1: the minimal `target` arg
Instead, what if we introduced a new `target` keybinding argument, that could be used as follows:
| Keybinding Command | Behavior |
|--|--|
| `"command": { "action": "openSettings", "target": "settingsFile" }`       | opens "settings.json" in your default text editor |
| `"command": { "action": "openSettings", "target": "defaultsFile" }`       | opens "defaults.json" in your default text editor |
| `"command": { "action": "openSettings", "target": "allSettingsFiles" }`   | opens all of settings files in your default text editor |
| `"command": { "action": "openSettings", "target": "settingsUI" }`         | opens the Settings UI |

This was based on Proposition 1 below, but reduced the overhead of people able to define specific pages to go to.

### Other options we considered were...

#### Proposition 1: the `target` arg
We considered making target be more specific like this:
| Keybinding Command | Behavior |
|--|--|
| `"command": { "action": "openSettings", "target": "settingsFile" }`   | opens "settings.json" in your default text editor |
| `"command": { "action": "openSettings", "target": "defaultsFile" }`   | opens "defaults.json" in your default text editor |
| `"command": { "action": "openSettings", "target": "uiSettings" }`     | opens the Settings UI |
| `"command": { "action": "openSettings", "target": "uiGlobals" }`      | opens the Settings UI to the Globals page |
| `"command": { "action": "openSettings", "target": "uiProfiles" }`     | opens the Settings UI to the Profiles page |
| `"command": { "action": "openSettings", "target": "uiColorSchemes" }` | opens the Settings UI to the Color Schemes page |

If the Settings UI does not have a home page, `uiGlobals` and `uiSettings` will do the same thing.

This provides the user with more flexibility to decide what settings page to open and how to access it.

#### Proposition 2: the `format` and `page` args
Another approach would be to break up `target` into `format` and `page`.

`format` would be either `json` or `ui`, dictating how you can access the setting.
`page` would be any of the categories we have for settings: `settings`, `defaults`, `globals`, `profiles`, etc...

This could look like this:
| Keybinding Command | Behavior |
|--|--|
| `"command": { "action": "openSettings", "format": "json", "page": "settings" }`     | opens "settings.json" in your default text editor |
| `"command": { "action": "openSettings", "format": "json", "page": "defaults" }`     | opens "defaults.json" in your default text editor |
| `"command": { "action": "openSettings", "format": "ui",   "page": "settings" }`     | opens the Settings UI |
| `"command": { "action": "openSettings", "format": "ui",   "page": "globals" }`      | opens the Settings UI to the Globals page |
| `"command": { "action": "openSettings", "format": "ui",   "page": "profiles" }`     | opens the Settings UI to the Profiles page |
| `"command": { "action": "openSettings", "format": "ui",   "page": "colorSchemes" }` | opens the Settings UI to the Color Schemes page |

The tricky thing for this approach is, what do we do in the following scenario:
```js
{ "command": { "action": "openSettings", "format": "json", "page": "colorSchemes" } }
```
In situations like this, where the user wants a `json` format, but chooses a `page` that is a part of a larger settings file, I propose we simply open `settings.json` (or whichever file contains the settings for the desired feature).

#### Proposition 3: minimal approach
What if we don't need to care about the page, and we really just cared about the format: UI vs json? Then, we still need a way to represent opening defaults.json. We could simplify Proposition 2 to be as follows:
- `format`: `json`, `ui`
- ~`page`~ `openDefaults`: `true`, `false`

Here, we take away the ability to specifically choose which page the user wants to open, but the result looks much cleaner.

If there are concerns about adding more settings files in the future, `openDefaults` could be renamed to be `target`, and this would still serve as a hybrid of Proposition 1 and 2, with less possible options.

## UI/UX Design

The user has full control over modifying and adding these keybindings.

However, the question arises for what the default experience should be. I propose the following:
| Keychord | Behavior |
| <kbd>ctrl+,</kbd> | Open settings.json |
| <kbd>ctrl+alt+,</kbd> | Open defaults.json |

When the Settings UI gets added in, they will be updated to open their respective pages in the Settings UI.

## Capabilities

### Accessibility

None.

### Security

None.

### Reliability

None.

### Compatibility

Users that expect a json file to open would have to update their keybinding to do so.

### Performance, Power, and Efficiency

## Potential Issues

None.

## Future considerations

When the Settings UI becomes available, a new value for `target` of `settingsUI` will be added and it will become the default target.

If the community finds value in opening to a specific page of the Settings UI, `target` will be responsible for providing that functionality.

## Resources

None.
