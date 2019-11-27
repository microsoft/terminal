---
author: Dustin Howett @DHowett-MSFT
created on: 2019-07-19
last updated: 2019-11-05
issue id: "#2563"
---

# Improvements to CloseOnExit

## Abstract

This specification describes an improvement to the `closeOnExit` profile feature and the `ITerminalConnection` interface that will offer greater flexibility and allow us to provide saner defaults in the face of unreliable software.

### Conventions and Terminology

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and "OPTIONAL" in this document are to be interpreted as described in [RFC 2119](https://tools.ietf.org/html/rfc2119).

## Inspiration

Other terminal emulators like ConEmu have a similar feature.

## Solution Design

### `ITerminalConnection` Changes

* The `TerminalConnection` interface will be augmented with an enumerator and a set of events regarding connection state transitions.
    * enum `TerminalConnection::ConnectionState`
        * This enum attempts to encompass all potential connection states, even those which do not make sense for a local terminal.
        * The wide variety of values will be useful to indicate state changes in a user interface.
        * `NotConnected`: All new connections will start out in this state
        * `Connecting`: The connection has been initated, but has not yet completed connecting.
        * `Connected`: The connection is active.
        * `Closing`: The connection is being closed (usually by request).
        * `Closed`: The connection has been closed, either by request or from the remote end terminating successfully.
        * `Failed`: The connection was unexpectedly terminated.
    * event `StateChanged(ITerminalConnection, IInspectable)`
        * (the `IInspectable` argument is recommended and required for a typed event handler, but it will bear no payload.)
    * event `TerminalDisconnected` will be removed, as it is replaced by `StateChanged`
    * **NOTE**: A conforming implementation MUST treat states as a directed acyclic graph. States MUST NOT be transitioned in reverse.
* A helper class may be provided for managing state transitions.

### `TerminalControl` Changes

* As the decision as to whether to close a terminal control hosting a connection that has transitioned into a terminal state will be made by the application, the unexpressive `Close` event will be removed and replaced with a `ConnectionStateChanged` event.
* `event ConnectionStateChanged(TerminalControl, IInspectable)` event will project its connection's `StateChanged` event.
* TerminalControl's new `ConnectionState` will project its connection's `State`.
    * (this is indicated for an eventual data binding; see Future Considerations.)

### Application and Settings

1. The existing `closeOnExit` profile key will be replaced with an enumerated string key supporting the following values (behaviours):
    * `always` - a tab or pane hosting this profile will always be closed when the launched connection reaches a terminal state.
    * `graceful` - a tab or pane hosting this profile will be closed if and only if the launched connection reaches the `Closed` terminal state.
    * `never` - a tab or pane hosting this profile will not automatically close.
    * See the Compatibility section for information on the legacy settings transition. 
    * **The new default value for `closeOnExit` will be `graceful`.**
2. `Pane` will remain responsible for making the final determination as to whether it is closed based on the settings of the profile it is hosting.

## UI/UX Design

* The existing `ITerminalConnection` implementations will be augmented to print out interesting and useful status information when they transition into a `Closed` or `Failed` state.
    * Example (ConPTY connection)
        * The pseudoconsole cannot be opened, or the process fails to launch.<br>`[failed to spawn 'thing': 0x80070002]`, transition to `Failed`.
        * The process exited unexpectedly.<br>`[process exited with code 300]`, transition to `Failed`.
        * The process exited normally.<br>`[process exited with code 0]`, transition to `Closed`.
* _The final message will always be printed_ regardless of user configuration.
* If the user's settings specify `closeOnExit: never/false`, the terminal hosting the connection will never be automatically closed. The message will remain on-screen.
* If the user's settings specify `closeOnExit: graceful/true`, the terminal hosting the connection _will_ automatically be closed if the connection's state is `Closed`. A connection in the `Failed` state will not be closed, and the message will remain on-screen.
* If the user's settings specify `closeOnExit: always`, the terminal hosting the connection will be closed. The message will not be seen.

## Capabilities

### Accessibility

This will give users of all technologies a way to know when their shell has failed to launch or has exited with an unexpected status code.

### Security

There will be no impact to security.

### Reliability

Windows Terminal will no longer immediately terminate on startup if the user's shell doesn't exist.

### Compatibility

There is an existing `closeOnExit` _boolean_ key that a user may have configured in profiles.json. The boolean values should map as follows:

* `true` -> `graceful`
* `false` -> `never`

This will make for a clean transition to Windows Terminal's sane new defaults.

### Performance, Power, and Efficiency

## Potential Issues

There will be no impact to Performance, Power or Efficiency.

## Future considerations

* Eventually, we may want to implement a feature like "only close on graceful exit if the shell was running for more than X seconds". This puts us in a better position to do that, as we can detect graceful and clumsy exits more readily.
    * (potential suggestion: `{ "closeOnExit": "10s" }`
* The enumerator values for transitioning connection states will be useful for connections that require internet access.
* Since the connection states are exposed through `TerminalControl`, they should be able to be data-bound to other Xaml elements. This can be used to provide discrete UI states for terminal controls, panes or tabs _hosting_ terminal controls.
    * Example: a tab hosting a terminal control whose connection has been broken MAY display a red border.
    * Example: an inactive tab that reaches the `Connected` state MAY flash to indicate that it is ready.
