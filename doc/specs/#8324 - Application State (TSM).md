---
author: Dustin Howett @DHowett
created on: 2020-10-19
last updated: 2020-11-18
issue id: #8324
---

# Application State (in the Terminal Settings Model)

## Abstract

Terminal is going to need a place to store application "state", including:
* Dialogs the user has chosen to hide with a `[ ] Do not ask again` checkbox, as proposed in [issue 6641]
* Which dynamic profiles have been generated, as a way to resolve [user dissatisfaction] around profiles "coming back"
* On-screen position of the window, active session state, layout, etc. for [eventual restoration]

This specification provides for a place to store these things.

## Inspiration

Many other applications store cross-session state somewhere.

## Solution Design

It is the author's opinion that the above-mentioned settings are inappropriate for storage in the user's settings.json;
they do not need to be propagated immediately to other instances of Windows Terminal, they are not intended to be
edited, and storing them outside of settings.json defers the risk inherent in patching the user's settings file.

Therefore, this document proposes that a separate application state archive (`state.json`) be stored next to
`settings.json`.

It would be accessed by way of an API hosted in the `Microsoft.Terminal.Settings` namespace.

```idl
namespace Microsoft.Terminal.Settings {
    [default_interface]
    runtimeclass ApplicationState {
        // GetForCurrentApplication will return an object deserialized from state.json.
        static ApplicationState GetForCurrentApplication();

        void Clear();

        IVector<guid> GeneratedProfiles;
        Boolean ShowCloseOnExitWarning;
        // ... further settings ...
    }
}
```

> 📝 The sole motivation for exposing this from Microsoft.Terminal.Settings is to concentrate JSON de-/serialization in one
place.

Of note: there is no explicit `Save` or `Commit` mechanism. Changes to the application state are committed durably a
short duration after they're made.

## UI/UX Design

This has no direct impact on the UI/UX; however, we may want to add a button to general settings page titled "reset all
dialogs" or "don't not ask me again about those things that, some time ago, I told you to not ask me about".

We do not intend this file to be edited by hand, so it does not have to be user-friendly or serialized with indentation.

## Capabilities

### Accessibility

It is not expected that this proposal will impact accessibility.

### Security

It is not expected that this proposal will impact security, as it uses our _existing_ JSON parser in a new place.

### Reliability

[comment]: # Will the proposed change improve reliability? If not, why make the change?

The comment in this section heavily suggests that we should only make changes that improve reliability, not ones that
maintain it.

### Compatibility

Some users who were expecting profiles to keep coming back after they delete them will need to adjust their behavior.

### Performance, Power, and Efficiency

## Potential Issues

Another file on disk is another file users might edit. We'll need to throw out the entire application state payload if
it's been damaged, instead of trying to salvage it, so that we more often "do the right thing."

## Future considerations

This will allow us to implement a number of the features mentioned at the head of this document.

## Resources

[issue 6641]: https://github.com/microsoft/terminal/issues/6641
[user dissatisfaction]: https://github.com/microsoft/terminal/issues/3231
[eventual restoration]: https://github.com/microsoft/terminal/issues/961
