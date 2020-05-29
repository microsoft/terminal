---
author: Carlos Zamora @carlos-zamora
created on: 2019-08-22
last updated: 2020-05-13
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

`SnapOnOutput` will be a profile-level `ICoreSettings` setting of type enum. It can be set to one of the following values:
- `never`: new output does not cause the viewport to update to the bottom of the scroll region
- `noSelection`: new output causes the viewport to update to the bottom of the scroll region **IF** no selection is active
- `atBottom`: new output causes the viewport to update **IF** the viewport is already at the virtual bottom
- `noSelection|atBottom`: (**default**) new output causes the viewport to update **IF** no selection is active or the viewport is already at the virtual bottom
- `always`: new output causes the viewport to update to the bottom of the scroll region

The `TerminalCore` is responsible for moving the viewport on a scroll event. All of the logic for this feature should be handled here.

A new private enum array `_snapOnOutput` will be introduced to save which of these settings are included. The `_NotifyScrollEvent()` calls (and nearby code) will be surrounded by conditional checks for the enums above. This allows it to be used to determine if the viewport should update given a specific situation.

## UI/UX Design

The `snapOnOutput` setting is done at a profile-level to be near `snapOnInput`. Additionally, the `never` value seems more valuable when the user can dedicate a specific task to the profile. Such a scenario would be a shell that frequently generates new output (i.e.: a live-generating log), but the user is not necessarily interested in what the latest output is.

The default `snapOnOutput` value will be `"noSelection|atBottom"`.

## Capabilities

### Accessibility

The `UiaRenderer` exposes new content that is rendered to the screen upon a scroll. No additional changes are necessary here.

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

Another would be to introduce additional accepted values. Consider the output from a large testing suite. A user could create an extension that snaps on output for failing tests or when the testing has concluded.

### Infinite Scrollback
At the time of introducing this, the infinite scrollback feature is not supported. This means that the buffer saves the history up to the `historySize` amount of lines. When infinite scrollback is introduced, the buffer needs to change its own contents to allow the user to scroll beyond the `historySize`. With infinite scrollback enabled and the mutable viewport **NOT** snapping to new output, the `TerminalCore` needs to keep track of...
- what contents are currently visible to the user (in the current location of the mutable viewport)
- how to respond to a user's action of changing the location of the mutable viewport (i.e.: snapOnInput, scroll up/down)

The `TermControl`'s scrollbar will abstract this issue away. As the viewport moves, really we're just presenting content that was saved to the text buffer.

### Private Mode Escape Sequences
There are a couple of private mode escape sequences that some terminals use to control this kind of thing. Mode 1010, for example, snaps the viewport to the bottom on output, whereas Mode 1011 spans the viewport to the bottom on a keypress.

Mode 1010 should set the `SnapOnOutput` value via a Terminal API.
Mode 1011 should set the `SnapOnInput` value via a Terminal API.

## Resources

[#980]: # https://github.com/microsoft/terminal/issues/980
