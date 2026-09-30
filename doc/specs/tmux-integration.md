---
author: MisterTea
created on: 2026-08-30
last updated: 2026-09-16
issue id: n/a
---

# tmux control-mode integration

## Abstract

This spec describes how Windows Terminal acts as a native UI for a tmux control-mode session. A gateway process runs on an existing ConPTY connection, while tmux windows and panes are represented by Windows Terminal windows, tabs, and panes. Terminal sends ordinary tmux commands and consumes ordinary newline-delimited tmux control-mode notifications; it does not define a separate HTM multiplexer protocol.

The Windows implementation is currently provided by EternalTerminal. Its executables retain the names `htm` and `htmd`, but `htm` exposes the same control protocol as `tmux -CC`. References to `htm` or `htmd` in this document therefore identify those programs or their Windows transport details, not a distinct Terminal-side protocol.

The work is gated by `Feature_TmuxIntegration`. It is enabled in non-Inbox builds and disabled for WindowsInbox branding.

## Background

iTerm2 introduced a native tmux integration based on `tmux -CC`. In control mode, the terminal is a tmux client: it receives structured notifications about output, layouts, and lifecycle events, and it sends tmux commands for input and UI operations. Other terminal emulators implement the same model.

EternalTerminal supplies a Windows tmux-compatible client and daemon through `htm.exe` and `htmd.exe`. On Windows, `htmd` owns the pane ConPTY processes and exposes tmux control mode to `htm`. Windows Terminal provides the native renderer and maps its UI actions back to tmux commands.

Windows Terminal has no Hyper-style plugin API. JSON fragments can add profiles and color schemes, but they cannot intercept split, tab, window, resize, or close operations. The integration therefore lives in TerminalApp and wraps the gateway ConPTY connection, following the same broad connection-wrapper pattern as `DebugTapConnection`.

## Solution design

```text
 Windows Terminal                              EternalTerminal
 ┌──────────────────────────────────┐          ┌──────────────────────┐
 │ Gateway tab                      │ ConPTY   │ htm.exe              │
 │   TmuxLeaderConnection ───────────────────► │ tmux control client  │
 │          │                       │          │          │           │
 │          │ commands/notifications│          │          ▼ AF_UNIX   │
 │          ▼                       │          │ htmd.exe mux server  │
 │ TmuxSession                      │          │   ConPTY per pane    │
 │   TmuxFollowerConnection per %pane          └──────────────────────┘
 │   native windows/tabs/panes      │
 └──────────────────────────────────┘
```

The gateway tab is the control plane. It displays a small command menu and does not directly render a tmux pane. Each real tmux pane is rendered by a `TmuxFollowerConnection`, which has no local child process of its own.

### Activation

Terminal wraps a ConPTY in `TmuxLeaderConnection` when all of the following are true:

- `Feature_TmuxIntegration` is enabled.
- The connection is a `ConptyConnection`.
- The configured command line launches `htm` or `htm.exe`.

This is established when the connection is created. Typing `htm` later inside an arbitrary shell tab does not retrofit that existing connection with the wrapper.

No new `connectionType` is required. The profile remains a normal local profile and the wrapper passes output through unchanged until it sees the control-mode start marker.

### Control-mode transport

The logical protocol is tmux control mode:

- Entry marker: DCS `1000p` (`ESC P 1000 p`).
- Exit marker: ST (`ESC \\`).
- Records between the markers are newline-delimited tmux control-mode records.
- Commands sent to the gateway are ordinary tmux commands terminated by carriage return.

ConPTY strips DCS sequences. To preserve arbitrary control bytes on Windows, EternalTerminal encodes them as one or more private CSI carrier sequences:

```text
ESC [ ? 777 ; <byte0> ; <byte1> ... q
```

Each carrier holds at most 15 decimal byte values. `TmuxLeaderConnection` incrementally decodes carriers, including carriers split across output chunks, before looking for the DCS/ST markers or parsing control lines. This carrier is a Windows transport adaptation; it does not change the tmux protocol visible above the transport.

### Consumed tmux records

Windows Terminal currently acts on these control-mode records:

| Record | Terminal behavior |
|--------|-------------------|
| `%begin`, `%end`, `%error` | Delimit and collect command replies. |
| `%output %pane ...` | Unescape tmux octal sequences and append output to the matching follower. |
| `%layout-change @window ...` | Reconcile live pane IDs and close stale follower UI. |
| `%window-pane-changed @window %pane` | Associate a pane with a tmux window and complete pending UI creation. |
| `%window-renamed @window name` | Update the corresponding Windows Terminal tab title. |
| `%session-window-changed ... @window` | Track the active tmux window for later UI actions. |
| `%exit` | Close follower UI and leave tmux mode cleanly. |

Unknown notifications are ignored. Command reply text is retained only while a `%begin`/`%end` or `%error` block is active.

### Commands emitted by Terminal

Terminal translates native actions into standard tmux commands:

| Windows Terminal action | tmux command |
|-------------------------|--------------|
| Type in a pane | `send-keys -H -t %pane 0x...` |
| Split a pane | `split-window -P -F '#{pane_id}' -t %pane -h/-v` |
| Open a tab or window | `new-window -P -F '#{pane_id}'` |
| Close a pane | `kill-pane -t %pane` |
| Resize a pane | `resize-pane -t %pane -x cols -y rows` |
| Resize the control client | `refresh-client -C <columns>x<rows>` |
| Detach | `detach-client` |

Keyboard input is converted to UTF-8 and sent with hexadecimal `send-keys -H` arguments so spaces, control characters, and Unicode are not reinterpreted by the command parser. Windows win32-input-mode records are decoded, and UTF-16 surrogate pairs are preserved across input callbacks.

Resize events are deduplicated and delayed by 75 ms. This avoids flooding the server with transient geometry while XAML animates a layout and prevents excessive blank lines from ConPTY resizes.

Writes to the leader are serialized and occur off the UI/output-callback path. This prevents commands from being interleaved and avoids reentrant ConPTY writes that can deadlock the window.

### Native UI mapping and affinities

The mapping is:

- tmux pane (`%N`) -> `TmuxFollowerConnection` and `TermControl`
- tmux window (`@N`) -> a Windows Terminal tab
- affinity group of tmux windows -> a Windows Terminal OS window

The first native tmux tab creates a host OS window. A Windows Terminal **New Tab** action creates a tmux window in the source window's affinity group. **New Window** creates a tmux window in a new native host. Splits stay within the source tab and target pane.

The grouping is stored in the tmux session option `@affinities`, using the iTerm2-compatible form:

```text
0,1,3 2,4
```

Each comma-separated group belongs in one native OS window. Because this is tmux session state, it survives a client detach and allows a replacement Windows Terminal client to reconstruct the same grouping.

On attach, Terminal first runs `show-options -qv @affinities`, then requests layout replay with `refresh-client -C 80x24`. This ordering is intentional: initial layout notifications contain pane/window relationships but not native-host ownership. Terminal also tolerates an empty command reply already in flight ahead of the affinity query. These rules prevent reattach races from overwriting persisted multi-window groupings.

### Gateway controls and teardown

While control mode is active, the gateway accepts the iTerm2-style command menu keys:

| Key | Action |
|-----|--------|
| `Esc` | Send `detach-client`, close all follower UI, and leave the tmux server running. |
| `X` | Force-close the control client and follower UI. |
| `L` | Toggle display of non-output protocol records in the gateway. |
| `C` | Prompt for and send an arbitrary tmux command. |

Receiving ST or `%exit`, closing the leader, or detaching closes all followers and removes their registrations. Followers are silenced before their `TermControl`s are closed so late `%output` records and resize callbacks cannot write through a torn-down session. Closing an individual native pane sends `kill-pane`; server-driven layout changes close the corresponding native UI without echoing another close command.

## Implementation types

The integration is implemented under `src/cascadia/TerminalApp/`:

| Type | Role |
|------|------|
| `TmuxProtocol` | Control markers, ConPTY carrier decoding, control-output unescaping, input decoding, and tmux layout parsing. |
| `TmuxLeaderConnection` | Wraps the gateway ConPTY, detects control mode, parses records, serializes commands, and owns client-size updates. |
| `TmuxFollowerConnection` | Virtual pane connection. Routes input and resize operations to tmux and receives `%output`. |
| `TmuxSession` | Maintains pane/window maps, affinity groups, pending command replies, UI creation, and teardown. |

TerminalPage action handlers recognize these connections and intercept new-tab, new-window, split, duplicate, resize, and close operations while the session is active.

## Configuration

Put EternalTerminal's `htm.exe` and `htmd.exe` on `PATH`, or set `HTM_BIN_DIR` in the profile environment. Terminal prepends that directory to `PATH` for the profile's ConPTY:

```json
{
    "profiles": {
        "list": [
            {
                "name": "tmux control mode",
                "commandline": "htm.exe",
                "environment": {
                    "HTM_BIN_DIR": "C:\\path\\to\\EternalTerminal\\build\\Release"
                }
            }
        ]
    }
}
```

The `commandline` must identify `htm` so Terminal installs the leader wrapper. `HTM_BIN_DIR` retains its name because it is part of EternalTerminal's executable discovery contract.

## UI/UX

1. Open a profile whose command line is `htm.exe`.
2. The gateway enters tmux control mode and displays its command menu.
3. Live tmux panes appear as native Windows Terminal windows, tabs, and panes.
4. Typing, splitting, opening tabs/windows, resizing, renaming, and closing are reflected in the tmux session.
5. Press `Esc` in the gateway to detach without killing the tmux session; reconnecting restores its panes and native-window affinity groups.

## Capabilities

### Accessibility

Follower panes are normal `TermControl` instances. Screen readers and other accessibility features observe the same text buffer and UI Automation surface as ordinary terminal panes. No new pane chrome is introduced.

### Security

`htm` and `htmd` run as the current user. EternalTerminal's AF_UNIX endpoint is per-user under `%TEMP%`. Windows Terminal interprets control records only on a connection explicitly launched as the tmux gateway. `HTM_BIN_DIR` is an explicit profile environment setting.

### Reliability

The implementation accounts for fragmented carriers and records, concurrent input/resize/action writes, command-reply versus notification ordering, pane-creation races, late output during teardown, and persisted-affinity replay. Malformed or incomplete input remains buffered only where it can form a valid carrier, marker, or newline-delimited record.

Unit tests cover transport decoding, control parsing, input conversion, and layouts. End-to-end tests in the EternalTerminal repository exercise Windows Terminal with `htm`/`htmd`, including layouts, stress, corner cases, affinity persistence, the control plane, races, and clean exit.

### Compatibility

The Terminal-side interface is tmux control mode and follows the `tmux -CC` model. The currently validated Windows provider is EternalTerminal's `htm`/`htmd`. Provider-specific names remain only where Terminal must launch or locate those binaries, or describe the ConPTY carrier used by that implementation.

### Performance, power, and efficiency

The gateway uses one wrapped ConPTY connection. Followers do not spawn local shell processes in Windows Terminal; `htmd` owns the real pane processes. Output is routed directly from `%output` records, and input, client writes, and resize updates are batched or serialized where necessary.

## Known limitations and future work

- Activation currently recognizes the EternalTerminal `htm` command line; it is not general discovery for every possible `tmux -CC` executable.
- The CSI `?777` carrier is specific to transporting control bytes through Windows ConPTY.
- Restoring a recently closed tmux pane through Windows Terminal's generic undo-close path is not supported as a tmux operation.
- A future connection-wrapper extension point could move provider-specific activation and transport adaptation out of TerminalApp.

## Resources

- tmux control mode (`tmux -CC`) and the tmux `CONTROL MODE` manual section
- EternalTerminal `src/htm/` and its `htm`/`htmd` executables
- [hyper-htm](https://github.com/MisterTea/hyper-htm)
- iTerm2 tmux integration and `@affinities` convention
- `DebugTapConnection` in TerminalApp
- Windows Terminal GH#4000 (extensibility)
