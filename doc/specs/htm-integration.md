---
author: MisterTea
created on: 2026-08-30
last updated: 2026-08-30
issue id: n/a
---

# HTM (headless terminal multiplexer) integration

## Abstract

This spec describes how Windows Terminal detects an EternalTerminal `htm` session on an existing ConPTY connection, takes over tabs/panes so they map onto `htmd`, and tears the session down without a new `connectionType`. The work is gated by `Feature_HtmIntegration` (enabled in Dev, disabled in Release and WindowsInbox).

## Inspiration

[hyper-htm](https://github.com/MisterTea/hyper-htm) wraps Hyper so that running `htm` in a tab steals that PTY, creates follower panes with no local PTY, and maps split/new-tab/close onto the `htmd` daemon. Windows Terminal has no Hyper-style plugin API: JSON fragments can inject profiles and color schemes only. Shell integration (OSC 133) and `ShellExtension` do not intercept splits. Matching that UX requires TerminalApp to wrap ConPTY, the same pattern as `DebugTapConnection`.

EternalTerminal `htm`/`htmd` on Windows uses ConPTY for pane shells and AF_UNIX IPC at `%TEMP%\htm.<user>.ipc`. The wire protocol is unchanged so hyper-htm and Windows Terminal stay compatible.

## Solution Design

```
  Windows Terminal                          EternalTerminal
  ┌─────────────────────────────┐           ┌──────────────────────┐
  │ Leader pane                 │  PTY      │ htm.exe byte bridge  │
  │   ConPTY + HtmLeaderConnection ────────►│         │            │
  │ Follower panes              │  framed   │         ▼ AF_UNIX    │
  │   HtmFollowerConnection     │  packets  │ htmd.exe mux daemon  │
  │ HtmSession (per window)     │           │   ConPTY per pane    │
  └─────────────────────────────┘           └──────────────────────┘
```

### Why wrap ConPTY instead of a new `connectionType`

Users type `htm` in an existing profile (PowerShell, cmd, WSL). A dedicated `connectionType` would require a separate profile and would not take over a tab that is already running. Wrapping every ConPTY in `HtmLeaderConnection` (behind the feature flag) matches Hyper: pass-through until `ESC[###q`, then consume framed packets.

### Wire protocol

Compatible with EternalTerminal `HtmHeaderCodes.hpp` and `hyper-htm/htm-core.js`.

- Init: `ESC[###q` (pass through bytes before this; hold a partial match across chunks)
- Exit: `ESC[$$$q` (leave HTM mode; leader shows a normal shell again)
- Frame: `[1-byte header][8-char base64 of little-endian int32 length][payload]`
- `SESSION_END` (`D`) is a single byte with no length field

| Header | Payload | Direction |
|--------|---------|-----------|
| `1` INSERT_KEYS | 36-char pane UUID + base64 UTF-8 keys | client → server |
| `2` INIT_STATE | JSON multiplexer state | server → client |
| `3` CLIENT_CLOSE_PANE | pane UUID | client → server |
| `4` APPEND_TO_PANE | pane UUID + base64 output | server → client |
| `5` NEW_TAB | tab UUID + pane UUID | client → server |
| `8` SERVER_CLOSE_PANE | pane UUID | server → client |
| `9` NEW_SPLIT | source UUID + pane UUID + `'1'` vertical / `'0'` horizontal | client → server |
| `A` RESIZE_PANE | base64 int32 cols + base64 int32 rows + pane UUID | client → server |
| `B` DEBUG_LOG | base64 text | server → client |
| `C` INSERT_DEBUG_KEYS | raw keys (leader keystrokes, Escape disconnect) | client → server |
| `D` SESSION_END | none | either |

UUIDs are 36-character `GuidToPlainString` values (no braces). HTM `'1'` is a vertical divider (Windows Terminal left/right); `'0'` is horizontal (up/down).

### Types (`src/cascadia/TerminalApp/`)

| Type | Role |
|------|------|
| `HtmProtocol` | Framing, CSI consume, packet parse |
| `HtmLeaderConnection` | Wraps ConPTY; pass-through until init CSI; then consume packets and route leader keys as `INSERT_DEBUG_KEYS` |
| `HtmFollowerConnection` | No process. `WriteInput` → `INSERT_KEYS`; `Resize` → `RESIZE_PANE`; `Close` → `CLIENT_CLOSE_PANE` |
| `HtmSession` | Per-window on `TerminalPage`: UUID map, INIT_STATE layout, user split/tab intercept |

On `INIT_STATE`, the first pane of the first tab maps onto the existing leader (no second ConPTY). Remaining panes are created with `HtmFollowerConnection` via sequential binary splits (HTM n-way splits). `APPEND_TO_PANE` / `DEBUG_LOG` are injected into the mapped `TermControl`s.

While a session is active, split and new-tab on an HTM pane create a follower and send `NEW_SPLIT` / `NEW_TAB` instead of spawning ConPTY. Closing a follower sends `CLIENT_CLOSE_PANE`. Closing the leader or seeing `SESSION_END` / `ESC[$$$q` tears down followers and returns the leader to a normal shell; the Windows Terminal window stays open (Hyper closes the window; Windows Terminal only ends the HTM session).

### Settings

Put `htm.exe` / `htmd.exe` on `PATH`, or set a profile environment variable so a local EternalTerminal build is found:

```json
{
    "profiles": {
        "defaults": {
            "environment": {
                "HTM_BIN_DIR": "C:\\path\\to\\et\\build"
            }
        }
    }
}
```

When `HTM_BIN_DIR` is set, Terminal prepends it to `PATH` for that ConPTY. An optional profile `"commandline": "htm.exe"` is not required for takeover.

## UI/UX Design

1. Open Windows Terminal, run `htm` in any ConPTY profile.
2. Extra panes/tabs appear matching the multiplexer state (including restored scrollback).
3. Typing in a follower is injected into `htmd`; output streams back as `APPEND_TO_PANE`.
4. Split / new tab from an HTM pane create remote panes, not local shells.
5. Escape (via `htm` debug keys) or `ESC[$$$q` leaves HTM mode; `htm` again restores the session.

## Capabilities

### Accessibility

Follower panes are normal `TermControl` instances. Screen readers see the same text buffer as any other pane. No new UI chrome.

### Security

`htm`/`htmd` already run as the current user. AF_UNIX IPC is per-user under `%TEMP%`. This feature only interprets bytes already produced by a user-started process. `HTM_BIN_DIR` is an explicit profile setting.

### Reliability

Malformed frames drop HTM mode instead of wedging the connection. Leader close disconnects the session without closing the whole window. Unknown HTM headers cause `htmd` to disconnect that client.

### Compatibility

Disabled in Release and WindowsInbox via `Feature_HtmIntegration`. Dev builds wrap ConPTY; until `htm` prints `ESC[###q`, behavior is unchanged. The wire protocol is not versioned independently of EternalTerminal / hyper-htm.

### Performance, Power, and Efficiency

Pass-through copies ConPTY output until init. After takeover, framed packets replace raw PTY traffic for followers (no extra processes). Overhead is comparable to `DebugTapConnection`.

## Potential Issues

- This environment cannot compile Windows Terminal; first verification needs Windows 10 2004+ and Visual Studio.
- JSON fragment extensions cannot provide this behavior; an upstream plugin API (GH#4000) would be a larger design.
- Undo-close of an HTM follower may recreate a local ConPTY instead of a follower.
- n-way HTM splits are approximated with sequential 50/50 binary splits.

## Future considerations

A first-class `connectionType` or connection-wrapper extension point would let this live out-of-tree. Until then, a feature-flagged branch is the reviewable shape for an upstream PR.

## Resources

- EternalTerminal `src/htm/` (`HtmHeaderCodes.hpp`, `HtmClient`, `HtmServer`, `TerminalHandler`)
- [hyper-htm](https://github.com/MisterTea/hyper-htm) `htm-core.js`, `index.js`
- `DebugTapConnection` in TerminalApp
- Windows Terminal GH#4000 (extensibility)
