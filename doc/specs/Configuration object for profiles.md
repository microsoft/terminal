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
control can be rendered differently if its focused as compared to when its unfocused. Another
example is that an elevated state control can be rendered differently as compared to a
non-elevated one.

## Inspiration

Reference: [#3062](https://github.com/microsoft/terminal/issues/3062)

Users want there to be a more visible indicator than the one we have currently for which
pane is focused and which panes are unfocused. This change would grant us that feature.

## Solution Design

A new class in the `TerminalControl` namespace, called `CustomRenderConfig`, that will contain rendering
parameters. `TermControl` and `TerminalCore` will use objects of this class to pass along rendering parameters
to the renderer.

Our `TerminalSettingsModel` will parse out that object from the settings json file and pipe it over to
`TermControl`/`TerminalCore` to use. This will be done through `IControlSettings` and `ICoreSettings`, which
is the way we already pipe over information that these interfaces need to know regarding rendering (such as
`CursorStyle` and `FontFace`).

### Allowed parameters

Not all parameters which can be defined in a `Profile` can be defined in this new object (for example, we
do not want parameters which would cause a resize in this object.) Here are the list of parameters we
will allow:

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


