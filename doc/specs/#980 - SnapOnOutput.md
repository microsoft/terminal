---
author: Carlos Zamora @carlos-zamora
created on: 2019-08-22
last updated: 2020-07-06
issue id: 980
---

# Snap On Output

## Abstract

The goal of this change is to determine the Terminal's scroll response to newly generated output.

Currently, new output causes the Terminal to always scroll to it. Some users want to be able to scroll through the buffer without interruptions.

## Inspiration

In ConHost, a selection causes the active process to be completely paused. When the selection is removed, the process continues.

Typical Unix terminals work differently. Rather than disabling the output, they disable the automatic scrolling. This allows the user to continue to see more output by choice.

## Solution Design

By default, the viewport will scroll to new output if the following conditions are met:
- no selection is active
- the viewport is at the "virtual bottom" (the bottom of the scroll history)

This behavior will not be configurable. If the user wants the viewport to stop autoscrolling, the user will simply create a selection or scroll any distance above the virtual bottom. Conversely, if the user wants the viewport to automatically scroll, the user must scroll to the bottom. Scrolling to the bottom is most easily achieved using the `snapOnInput` functionality.

Alternative solutions were considered and are recorded below. These solutions may be revisited if users desire an additional level of configurability.

Researching other terminal emulators has shown that this behavior is not configurable.

## Alternative Solutions

### Solution 1: `snapOnOutput` profile setting - enum flags
`SnapOnOutput` will be a profile-level `ICoreSettings` setting of type enum or enum array. It can be set to one or multiple of the following values:
- `never`: new output does not cause the viewport to update to the bottom of the scroll region
- `noSelection`: new output causes the viewport to update to the bottom of the scroll region **IF** no selection is active
- `atBottom`: new output causes the viewport to update **IF** the viewport is already at the virtual bottom
- `always`: new output causes the viewport to update to the bottom of the scroll region

The `TerminalCore` is responsible for moving the viewport on a scroll event. All of the logic for this feature should be handled here.

A new private enum array `_snapOnOutput` will be introduced to save which of these settings are included. The `_NotifyScrollEvent()` calls (and nearby code) will be surrounded by conditional checks for the enums above. This allows it to be used to determine if the viewport should update given a specific situation.

The `snapOnOutput` setting is introduced as a profile setting to match `snapOnInput`.

The default `snapOnOutput` value will be `[ "noSelection", "atBottom" ]`.

When an enum array is defined in the settings, it will be interpreted using boolean logic. The following scenarios will be invalid using the FlagMapper:
- `[ "always", "atBottom" ]`
- `[ "never", "atBottom" ]`

### Solution 2: `scrollLock` keybinding action

A `scrollLock` keybinding action would toggle automatically scrolling to new output.

**NOTE**: This can be easily confused with the <kbd>ScrollLock</kbd> key. Researching the use of the <kbd>ScrollLock</kbd> key has shown that programs rarely use this key. In most apps, pressing the <kbd>ScrollLock</kbd> key does not actually prevent scrolling the application. Additionally, finding a way to bing the `scrollLock` action to the <kbd>ScrollLock</kbd> key would be difficult. A physical keyboard may not necessarily have a <kbd>ScrollLock</kbd> key. Also, we would have to poll for the internal state of "is the scroll lock key enabled", which may change while the user is not necessarily using Terminal.

The introduction of a `scrollLock` action would require a visual indicator for the user to know when scrolling has been disabled. However, this introduces a number of problems:
- if the indicator is persistent, it may block the view
- if the indicator is not persistent, the user may be unaware of being in a state where scrolling doesn't work properly

**Additionally relevant research**:
- In Unix consoles, <kbd>ctrl+s</kbd> and <kbd>ctrl+q</kbd> freeze and unfreeze output respectively. However, this is a feature that is implemented outside of the scope for Terminal. Other shells like PowerShell do not have this feature, for example. There, <kbd>ctrl+s</kbd> does a 'Forward Search History' instead.
- Additionally, there is a <kbd>Pause</kbd> key that pauses the output in the conhost console. Pressing any other key will resume scrolling.


## Capabilities

### Accessibility

N/A

### Security

N/A

### Reliability

N/A

### Compatibility

N/A

### Performance, Power, and Efficiency

N/A

## Potential Issues

### Circling the buffer
If the text buffer fills up, the text buffer begins 'circling'. This means that new output shifts lines of the buffer up to make space. In a case like this, if `snapOnOutput` is set to `never`, the viewport should actually scroll up to keep the same content on the viewport.

In the event that the buffer is circling and the viewport has been moved to the top of the buffer, that content of the buffer is now lost (as the 'Infinite Scrollback' feature does not exist or is disabled). At that point, the viewport will remain at the top of the buffer and the new output will push old output out of the buffer.

### Infinite Scrollback
See **Future considerations** > **Infinite Scrollback**.

## Future considerations

### Extensibility
The introduction of `enum SnapOnOutput` allows for this feature to be enabled/disabled in more complex scenarios. A potential extension would be to introduce a new UI element or keybinding to toggle this feature.

### Infinite Scrollback
At the time of introducing this, the infinite scrollback feature is not supported. This means that the buffer saves the history up to the `historySize` amount of lines. When infinite scrollback is introduced, the buffer needs to change its own contents to allow the user to scroll beyond the `historySize`. With infinite scrollback enabled and the mutable viewport **NOT** snapping to new output, the `TerminalCore` needs to keep track of...
- what contents are currently visible to the user (in the current location of the mutable viewport)
- how to respond to a user's action of changing the location of the mutable viewport (i.e.: snapOnInput, scroll up/down)

### Private Mode Escape Sequences
There are a couple of private mode escape sequences that some terminals use to control this kind of thing. DECSET 1010, for example, snaps the viewport to the bottom on output, whereas DECSET 1011 spans the viewport to the bottom on a keypress.

DECSET 1010 should set the `SnapOnOutput` value via a Terminal API.
DECSET 1011 should set the `SnapOnInput` value via a Terminal API.

## Resources

[GH#980](https://github.com/microsoft/terminal/issues/980)
[DECSET 1010](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-?-Pm-h:Ps-=-1-0-1-0.1F79)
[DECSET 1011](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-?-Pm-h:Ps-=-1-0-1-1.1F7A)
