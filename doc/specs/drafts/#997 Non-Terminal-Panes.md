---
author: Mike Griese @zadjii-msft
created on: 2019-05-31
last updated: 2019-05-31
issue id: #997
---

# Non-Terminal Panes

## Abstract

This spec hopes to cover the work necessary to enable panes to host non-terminal
content. It'll describe changes to the `Pane` class to support hosting arbitrary
controls in addition to terminals.

## Inspiration

The primary driver for this change is to enable testing of the pane code. If a
`Pane` can host an arbitrary class, then a use case for that would be the
hosting of a non-xaml test class that acts like a control. This test class could
be have its state queried, to make sure that the panes are properly delivering
focus to the correct pane content.

Additionally, non-terminal panes could be used to host a variety of other
content, such as browser panes, sticky notes, text editor scratch-pads, etc.
Some discussion of these ideas are in #644.

## Solution Design

We'll change the TermControl class to derive from the
`Windows.UI.Xaml.Controls.Control` runtime class.
* We may need to override its `FocusState` and `Focus` methods, and implement
  them by plumbing them straight through to the fake Control the `TermControl`
  hosts.
* Otherwise, it might be possible that we could just remove that fake control
  entirely.
* We'll remove the `GetControl` method from the `TermControl`, as the
  `TermControl` itself will now be used as the control.

We'll change the Pane class to accept a `Windows.UI.Xaml.Controls.Control`
instead of a `TermControl`.

We'll additionally change the `Pane` constructor to accept an `optional<GUID>`
as opposed to needing a GUID. For constructing panes with Terminals, we should
pass a GUID corresponding to that settings profile. For panes that aren't
hosting terminals however, we should pass `nullopt` as the GUID. For panes that
are leaf nodes (panes which are hosting a control, not another pair of panes),
if the pane has a GUID, we can assume that the control is a TermControl.

When we want to host other types of content, we'll simply pass any other control
to the Pane, and it'll render it just as it would the `TermControl`.

## UI/UX Design

Instead of a pane hosting a terminal, it could host _any arbitrary control_. The
control would still be subject to the sizing provided to it by the `Pane`, but
it could host any arbitrary content.

## Capabilities

### Security

I don't foresee this implementation by itself raising security concerns. This
feature is mostly concerned with adding support for arbitrary controls, not
actually implementing some arbitrary controls.

### Reliability

With more possible controls in a pane than just a terminal, it's possible that
crashes in those controls could impact the entire Terminal app's reliability.
This would largely be out of our control, as we only author the TermControl.

We may want to consider hosting each pane in it's own process, similar to how
moder browsers will host each tab in its own process, to help isolate tabs. This
is a bigger discussion than the feature at hand, however.

### Performance, Power, and Efficiency

decide to host a WebView in a pane, then it surely could impact these measures.
I don't believe this will have a noticeable impact _on its own_. Should the user
However, I leave that discussion to the implementation of the actual alternative
pane content itself.

### Accessibility

When implementing the accessibility tree for Panes, we'll need to make sure that
for panes with arbitrary content, we properly activate their accessibility,
should the control provide some sort of accessibility pattern.

## Potential Issues

* [ ] It's entirely possible that panes with non-terminal content will not be
  able to activate keybindings from the terminal application. For example, what
  if the hosted control wants to use Ctrl+T for its own shortcut? The current
  keybindings model has the `TermControl` call into the App layer to see if a
  keystroke should be handled by the app first. We may want to make sure that
  for non-terminal controls, we add a event handler to try and have the
  `AppKeyBindings` handle the keypress if the control doesn't. This won't solve
  the case where the control wants to use a keybinding that is mapped by the
  Terminal App. In that case, non-terminal controls will actually behave
  differently from the `TermControl`. The `TermControl` will give the app the
  first chance to handle a keybinding, while for other controls, the app will
  give the control the first chance to handle the keypress. This may be mildly
  confusing to end users.

## Future considerations

I expect this to be a major part of our (eventual) extensibility model. By
allowing arbitrary controls to be hosted in a Tab/Pane, this will allow
extension authors to embed their own UI experiences alongside the terminal.
See #555 for more discussion on the extensibility/plugin subject.

## Resources

N/A
