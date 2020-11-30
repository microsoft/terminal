---
author: <Pankaj> <Bhojwani> <pabhojwa@microsoft.com>
created on: <2020-11-20>
last updated: <2020-11-20>
issue id: <#8345>
---

# Configuration object for profiles

## Abstract

This spec outlines how we can support 'configuration objects' in our profiles, which
will allow us to render differently depending on the state of the control. For example, a
control can be rendered differently if it's focused as compared to when it's unfocused. Another
example is that an elevated state control can be rendered differently as compared to a
non-elevated one.

## Inspiration

Reference: [#3062](https://github.com/microsoft/terminal/issues/3062)

Users want there to be a more visible indicator than the one we have currently for which
pane is focused and which panes are unfocused. This change would grant us that feature.

## Solution Design

We will add an new interface in the `TerminalControl` namespace, called `IAppearance`, which defines how
`TerminalControl` and `TerminalCore` will ask for the rendering settings they need to know about (such as `FontFace`).
`TerminalApp` will implement this interface through a class called `AppAppearanceConfig`.

On the Settings Model side, there will be a new class called `AppearanceConfig`. When we parse out the
settings json file, each state-appearance will be stored in an object of this class. Later on, these values get
piped over to the `AppAppearanceConfig` objects in `TerminalApp`. This is the way we already pipe over information
such as `FontFace` and `CursorStyle` from the settings model to the app.

#### Need to talk about inheritance here

### Allowed parameters

For now, these states are meant to be entirely appearance-based. So, not all parameters which can be
defined in a `Profile` can be defined in this new object (for example, we do not want parameters which
would cause a resize in this object.) Here are the list of parameters we will allow:

- Anything regarding colors: `colorScheme`, `foreground`, `background`, `cursorColor` etc
- `cursorShape`
- `backgroundImage`

## UI/UX Design

Users will be able to add a new setting to their profiles that will look like this:

```
"unfocusedState": 
{
    "colorScheme": "Campbell",
    "cursorColor": "#888",
    "cursorShape": "emptyBox",
    "foreground": "#C0C0C0",
    "background": "#000000"
}
```

## Capabilities

### Accessibility

Does not affect accessibility.

### Security

Does not affect security.

### Reliability

This is another location in the settings where parsing/loading the settings may fail. However, this is the case
for any new setting we add so I would say that this is a reasonable cost for this feature.

### Compatibility

Should not affect compatibility.

### Performance, Power, and Efficiency

## Potential Issues

Inactive tabs will be 'rendered' in the background with the `UnfocusedRenderingParams` object, we need to make
sure that switching to an inactive tab (and so causing the renderer to update with the 'normal' parameters)
does not cause the window to flash/show a jarring indicator that the rendering values changed.

## Future considerations

We will need to decide how this will look in the settings UI.

## Resources


