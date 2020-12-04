---
author: <Pankaj> <Bhojwani> <pabhojwa@microsoft.com>
created on: <2020-11-20>
last updated: <2020-12-4>
issue id: <#8345>
---

# Appearance configuration objects for profiles

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

## Note: this is going to be an experimental change

Upon discussion we realized that this feature might cause scaling problems in the future. Thus, we
have decided to make this an experimental change for now - meaning that we are not (at this point) committed
to supporting this feature in the long term. We will decide on the longevity of this feature after obtaining
user feedback from the initial experimental phase.

## Solution Design

We will add new interfaces in the `TerminalControl` namespace, called `IControlAppearance` and `ICoreAppearance`,
which defines how `TerminalControl` and `TerminalCore` will ask for the rendering settings they need to know about
(such as `CursorShape`). `TerminalApp` will implement this interface through a class called `AppAppearanceConfig`.

We will also have `IControlSettings` require `IControlAppearance`, and `ICoreSettings` will require `ICoreAppearance`.
That way, the control's `settings` object can itself also be used as an object that implements both appearance interfaces. We do this so we
do not need a separate 'regular' configuration object when we wish to switch back to the 'regular' appearance from the unfocused
appearance - we can simply pass in the settings.

On the Settings Model side, there will be a new interface called `IAppearanceConfig`, which is essentially a
combination/mirror of the appearance interfaces described earlier. A new class, `AppearanceConfig`, will implement this
interface and so will `Profile` itself (for the same reason as earlier - so that no new configuration object is
needed for the regular appearance). We are choosing to have a separate interface on the settings model side to maintain
its separation from the rest of the terminal.

When we parse out the settings json file, each state-appearance will be stored in an object of the `AppearanceConfig`
class. Later on, these values get piped over to the `AppAppearanceConfig` objects in `TerminalApp`. This is the
similar to the way we already pipe over information such as `FontFace` and `CursorStyle` from the settings
model to the app.

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

In the case that not all of the allowed parameters are defined in an appearance object, we will obtain the
values for those parameters in the following matter:

If the profile defines an `unfocusedState`, any parameters not explicitly defined within it will adopt
the values from the profile itself. If the profile does not define an `unfocusedState`, then the global/default `unfocusedState` is used
for this profile.

Thus, if a user wishes for the unfocused state to look the same as the focused state for a particular profile,
while still having a global/default unfocused state appearance, they simply need to define an empty `unfocusedState`
for that profile (similarly, they could define just 1 or 2 parameters if they wish for minimal changes between the focused
and unfocused states for that profile). If they do not define any `unfocusedState` for the profile, then
the global/default one will be used.

### Multiple states

At the time of writing this, we have noted two possible 'appearance states' that we should allow: unfocused and elevated.
The question then is, in the case of an unfocused and elevated control, which appearance should we use?

For now, we will solve this problem by allowing a third state "unfocused elevated". This solution style of combining states
is of course not going to be feasible if we add more states in the future - hence why this feature is going to remain
experimental for now.

## UI/UX Design

Users will be able to add a new setting to their profiles that will look like this:

```
"experimental.state.unfocused": 
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


