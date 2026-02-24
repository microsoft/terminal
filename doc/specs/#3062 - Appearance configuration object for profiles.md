---
author: Pankaj Bhojwani, pabhojwa@microsoft.com
created on: 2020-11-20
last updated: 2021-2-5
issue id: #8345
---

# Appearance configuration objects for profiles

## Abstract

This spec outlines how we can support 'configuration objects' in our profiles, which
will allow us to render differently depending on the state of the control. For example, a
control can be rendered differently if it's focused as compared to when it's unfocused.

## Inspiration

Reference: [#3062](https://github.com/microsoft/terminal/issues/3062)

Users want there to be a more visible indicator than the one we have currently for which
pane is focused and which panes are unfocused. This change would grant us that feature.

## Solution Design

The implementation design for appearance config objects centers around the recent change where inheritance was added to the
`TerminalSettings` class in the Terminal Settings Model - i.e. different `TerminalSettings` objects can inherit from each other.
The reason for this change was that we did not want a settings reload to erase any overrides `TermControl` may have made
to the settings during runtime. By instead passing a child of the `TerminalSettings` object to the control, we can change
the parent of the child during a settings reload without the overrides being erased (since those overrides live in the child).

The idea behind unfocused appearance configurations is similar. We will pass in another `TerminalSettings` object to the control,
which is simply a child that already has some overrides in it. When the control gains or loses focus, it simply switches between
the two settings objects appropriately.

### Allowed parameters

For now, these states are meant to be entirely appearance-based. So, not all parameters which can be
defined in a `Profile` can be defined in this new object (for example, we do not want parameters which
would cause a resize in this object.) Here is the list of parameters we will allow:

- Anything regarding colors: `colorScheme`, `foreground`, `background`, `cursorColor` etc
- Anything regarding background image: `path`, `opacity`, `alignment`, `stretchMode`
- `cursorShape`

We may wish to allow further parameters in these objects in the future (like `bellStyle`?). The addition
of further parameters can be discussed in the future and is out of scope for this spec.

### Inheritance

The inheritance model can be thought of as an 'all-or-nothing' approach in the sense that the `unfocusedAppearance` object
is considered as a *single* setting instead of an object with many settings. We have chosen this model because it is cleaner
and easier to understand than the alternative, where each setting within an `unfocusedAppearance` object has a parent from which
it obtains its value.

Note that when `TerminalApp` initializes a control, it creates a `TerminalSettings` object for that profile and passes the
control a child of that object (so that the control can store overrides in the child, as described earlier). If an unfocused
config is defined in the profile (or in globals/profile defaults), then `TerminalApp` will create a *child of that child*,
put the relevant overrides in it, and pass that into the control as well. Thus, the inheritance of any undefined parameters
in the unfocused config will be as follows:

1. The unfocused config specified in the profile (or in globals/profile defaults)
2. Overrides made by the terminal control
3. The parent profile

## UI/UX Design

Users will be able to add a new setting to their profiles that will look like this:

```
"unfocusedAppearance":
{
    "colorScheme": "Campbell",
    "cursorColor": "#888",
    "cursorShape": "emptyBox",
    "foreground": "#C0C0C0",
    "background": "#000000"
}
```

When certain appearance settings are changed via OSC sequences (such as the background color), only the focused/regular
appearance will change and the unfocused one will remain unchanged. However, since the unfocused settings object inherits
from the regular one, it will still apply the change (provided it does not define its own value for that setting).

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

Rapidly switching between many panes, causing several successive appearance changes in a short period of time, could
potentially impact performance. However, regular/reasonable pane switching should not have a noticeable effect.

## Potential Issues

Inactive tabs will be 'rendered' in the background with the `UnfocusedRenderingParams` object, we need to make
sure that switching to an inactive tab (and so causing the renderer to update with the 'normal' parameters)
does not cause the window to flash/show a jarring indicator that the rendering values changed.

## Future considerations

We will need to decide how this will look in the settings UI.

We may wish to add more states in the future (like 'elevated'). When that happens, we will need to deal with how
these appearance objects can scale/layer over each other. We had a lot of discussion about this and could not find
a suitable solution to the problem of multiple states being valid at the same time (like unfocused and elevated).
This, along with the fact that it is uncertain if there even will be more states we would want to add led us to
the conclusion that we should only support the unfocused state for now, and come back to this issue later. If there
are no more states other than unfocused and elevated, we could allow combining them (like having an 'unfocused elevated' state).
If there are more states, we could do the implementation as an extension rather than inherently supporting it.

## Resources


