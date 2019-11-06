---
author: Dustin Howett @DHowett-MSFT
created on: 2019-07-19
last updated: 2019-11-05
issue id: "#2563"
---

# Improvements to CloseOnExit

## Abstract

This spec describes an improvement to the `closeOnExit` profile feature and the `TerminalConnection` interface that will offer greater flexibility and allow us to provide saner defaults.

## Inspiration

Other terminal emulators like ConEmu have a similar feature.

## Solution Design

1. The existing `closeOnExit` profile key will become an enumerated string key with the following values/behaviours:
   * `always` - a tab or pane hosting this profile will always be closed when the launched connection terminates.
   * `graceful` - a tab or pane hosting this profile will be closed **iff** the launched connection closes with an exit code == 0.
   * `never` - a tab or pane hosting this profile will not automatically close.
2. The `TerminalConnection` interface will be augmented with an enumerator and a set of events regarding connection state transitions.
    * enum `TerminalConnection::ConnectionState`
        * This enum attempts to encompass all potential connection states, even those which do not make sense for a local terminal.
        * The wide variety of values will be useful to indicate state changes in a user interface.
        * `NotConnected`: All new connections will start out in this state
        * `Connecting`: The connection has been initated, but has not yet completed connecting.
        * `Connected`: The connection is active.
        * `Closing`: The connection is being closed (usually by request).
        * `Closed`: The connection has been closed, either by request or from the remote end terminating successfully.
        * `Failed`: The connection was unexpectedly terminated.
            * A connection entering the `Failed` state will usually produce a message containing a reason.
    * event arguments `StateChangedEventArgs(ConnectionState)`
    * typed event `StateChanged`, whose payload is a `StateChangedEventArgs`
    * event `TerminalDisconnected` will be removed, as it is replaced by `StateChanged`
3. As `TerminalApp` is responsible for producing connection instances, _it_ will subscribe to the new connection state event.

The new default value for `closeOnExit` will be `graceful`.

## UI/UX Design

As above. An affordance is provided for the application consuming a connection to determine what state the connection is in and provide sufficient UI. Terminal will not currently make use of any of the new states other than `Closed` and `Failed` to enact UI change.

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
* The enumerator values for transitioning connection sattes will be useful for connections that require internet access.