---
author: Carlos Zamora @carlos-zamora
created on: 2019-08-22
last updated: 2019-08-22
issue id: 980
---

# Snap On Output

## Abstract

The goal of this change is to introduce a new profile-level setting `snapOnOutput` that allows users to control the Terminal's scroll response to newly generated output.

Currently, new output causes the Terminal to always scroll to it. Some users want to be able to scroll through the buffer without interruptions. This setting allows the Terminal to conditionally or never scroll to the new output.

## Inspiration

Creating a selection when the terminal is generating more output is difficult. This is because the terminal scrolls to the new output.

In CMD, a selection causes any processes to run in the background, but prevents more output from being displayed. When the selection is removed, the output is flushed to the display.

Typical Unix terminals work differently. Rather than disabling the output, they disable the automatic scrolling. This allows the user to continue to see more output by choice.

## Solution Design

`snapOnOutput` will be a profile-level `ICoreSettings` setting of type enum. It can be set to the following values:
- `never`: new output does not cause the viewport to update to the bottom of the scroll region
- `noSelection`: (**default**) new output causes the viewport to update to the bottom of the scroll region **IF** no selection is active
- `always`: new output causes the viewport to update to the bottom of the scroll region

The `TerminalCore` is responsible for moving the viewport on a scroll event. All of the logic for this feature should be handled here.

A new private field `std::function<bool(void)> _snapOnOutput` will be introduced. This field will be set when the settings are loaded, with the following logic:
- `never`       --> `return false`
- `noSelection` --> `return !isSelectionActive()`
- `always`      --> `return true`
`_snapOnOutput` can then be used to determine if the viewport should update given a specific situation.

## UI/UX Design

`snapOnOutput` will be a profile-level `ICoreSettings` setting of type enum. It can be set to the following values:
- `never`: new output does not cause the viewport to update to the bottom of the scroll region
- `noSelection`: (**default**) new output causes the viewport to update to the bottom of the scroll region **IF** no selection is active
- `always`: new output causes the viewport to update to the bottom of the scroll region

This is done at a profile-level to be near `snapOnInput`. Additionally, the `never` value seems more valuable when the user can dedicate a specific task to the profile. Such a scenario would be a shell that frequently generates new output (i.e.: a live-generating log), but the user is not necessarily interested in what the latest output is.

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

See **Future considerations** > **Infinite Scrollback**.

## Future considerations

### Extensibility
The introduction of `std::function<bool(void)> _snapOnOutput` allows for this feature to be enabled/disabled in more complex scenarios. A potential extension would be to introduce a new UI element or keybinding to toggle this feature.

Another would be to introduce a more complex boolean function dependent on the state of the app or the contents of the new output. Consider the output from a large testing suite. A user could create an extension that snaps on output for failing tests or when the testing has concluded.

### Infinite Scrollback
At the time of introducing this, the infinite scrollback feature is not supported. This means that the buffer saves the history up to the `historySize` amount of lines. When infinite scrollback is introduced, the buffer needs to change its own contents to allow the user to scroll beyond the `historySize`. With infinite scrollback enabled and the mutable viewport **NOT** snapping to new output, the `TerminalCore` needs to keep track of...
- what contents are currently visible to the user (in the current location of the mutable viewport)
- how to respond to a user's action of changing the location of the mutable viewport (i.e.: snapOnInput, scroll up/down)

## Resources

[#980]: # https://github.com/microsoft/terminal/issues/980
