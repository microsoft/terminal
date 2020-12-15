---
author: David Goodenough dgnuff/dgnuff@gmail.com
created on: 2020-04-18
last updated: 2020-04-18
issue id: 4183
---

# Independent Tab and Window Titles 

## Abstract

Currently the tab title and main window title are always the same, either the tab title from settings or the application title, based on the "suppressApplicationTitle" setting.

This change allows those two to differ, in particular it allows the tab to use the tab title, while the main window uses the application title.

This will always be reflected in the taskbar button for Terminal, and if "showTabsInTitleBar" is false, it will also be reflected in the main window titlebar.

## Inspiration

With "suppressApplicationTitle" disabled, it allows consoles to do things like show the running program, or the current directory as part of the title.  However letting this leak through to the tabs makes them very cluttered.

By using this option it allows short tab titles (e.g. "PS", "CMD", "bash" etc.), thus keeping the tabs tidy, while still making the application title available.

## Solution Design

Add a setting that's a peer to the current "suppressApplicationTitle", called "suppressApplicationTitleInTab".

Also add a \_windowTitle member to class Microsoft::Terminal::Core::Terminal as a peer to the current \_title member.

Then modify the flow for setting the window title to use \_windowTitle rather than \_title.

Finally modify Terminal::SetWindowTitle to respect the "suppressApplicationTitleInTab" so that when it's true and "suppressApplicationTitle" is false, the tab title is sent to \_title for display in the tab, while the application title is sent to \_windowTitle for display in the main window.

## UI/UX Design

[comment]: # What will this fix/feature look like? How will it affect the end user?

The effect is as outlined above: setting the new flag causes the window title and tab titles to display different information.

## Capabilities

[comment]: # Discuss how the proposed fixes/features impact the following key considerations:

### Accessibility

[comment]: # How will the proposed change impact accessibility for users of screen readers, assistive input devices, etc.

This should not alter the ability of screen readers to access program information, it will just have the effect of changing what the user hears if they screen read the window title with this setting turned on.

### Security

[comment]: # How will the proposed change impact security?
It should not have any effect on security.

### Reliability

[comment]: # Will the proposed change improve reliability? If not, why make the change?
There will be no increase in reliability.  the reason to do this is to improve the UX.

### Compatibility

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?
This should not break any current code flow.  In particular if the flag is not set, the user should not see any change in behavior at all.

### Performance, Power, and Efficiency

This should have no effect on performance, power usage or efficiency.

## Potential Issues

[comment]: # What are some of the things that might cause problems with the fixes/features proposed? Consider how the user might be negatively impacted.

If the code flow for setting tab titles is ever refactored or redesigned, it will almost certainly require this feature to be included as part of that refactor / redesign.

## Future considerations

[comment]: # What are some of the things that the fixes/features might unlock in the future? Does the implementation of this spec enable scenarios?
If we ever get the ability to "tear" a tab out of one window and either use it to create a new window, or drag it into an already existing window, the behavior of this feature will need to be thoroughly checked.

## Resources

[comment]: # Be sure to add links to references, resources, footnotes, etc.
