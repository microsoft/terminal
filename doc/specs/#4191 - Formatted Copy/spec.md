---
author: Kayla Cinnamon @cinnamon-msft
created on: <2020-04-01>
last updated: <2020-04-01>
issue id: <#4191>
---

# Formatted Copy

## Abstract

When copying text, the Terminal should provide the option of including formatting. Not all apps that receive text allow for picking which format you want when pasting. The default should be to only copy plain text, based on the response from this poll on Twitter.

![Twitter poll](twitter-poll.png)

## Solution Design

Solutions for the right click behavior and user settings are described below.

### Right click behavior

Be default, right clicking to copy would only copy the plain text. If the user would like to copy the formatting, they can hold `alt` and right click (this behavior is the same in conhost).

### Settings option 1 - global setting

We could have a global setting that adds the formatting when the user copies. When set, this would change the right click behavior.

### Settings option 2 - key binding argument

We could add an argument to the `copy` key binding to allow for formatted copying when the user chooses to do so.

## UI/UX Design

### Settings option 1 - global setting

a. The user could list which kinds of formats they want included when they copy. When right clicking, they would copy with these formats. If this is set to something other than `plain`, holding `alt` and right clicking would copy plain text.

`"copyFormats": "html,rtf,plain"`

b. We could also just combine html and rtf into a single boolean. Users would either get plain text only (`false`) or all formatting (`true`) onto their clipboard. If this is set to `true`, the default right click behavior is reversed: right clicking copies the formatting and holding `alt` copies only the plain text.

`"copyFormatting": true`

### Settings option 2 - key binding argument

a. Just like the the `trimWhitespace` argument you can add to the `copy` key binding, we could add one for text formatting.

`{"command": {"action": "copy", "keepFormatting": true}, "keys": "ctrl+a"}`

b. We could also split out the html and rtf formats.

`{"command": {"action": "copy", "formats": "rtf,html,plain"}, "keys": "ctrl+a"}`

## Capabilities

### Accessibility

This shouldn't affect accessibility.

### Security

This does not affect security.

### Reliability

This does not affect reliability.

### Compatibility

This breaks the existing behavior of always copying the formatting. The justification for breaking this default behavior is in the response from the community saying the default should be plain text only.

### Performance, Power, and Efficiency

## Potential Issues

One possible issue is that discovering how to copy the formatting might be difficult to find. We could mitigate this by adding it into the settings.json file and commenting it out.

## Future considerations

## Resources
