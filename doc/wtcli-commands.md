# wtcli Command Reference

`wtcli` is the CLI client for the Windows Terminal Protocol. It looks up the
running Terminal via the `WT_COM_CLSID` environment variable, calls
`CoCreateInstance(CLSCTX_LOCAL_SERVER)` to obtain `IProtocolServer`, and
exposes a tmux-style command surface over its IDL methods.

- Source: `src/tools/wtcli/main.cpp`
- IDL: `src/cascadia/TerminalProtocol/TerminalProtocol.idl`
- Primary in-tree caller: `tools/wta/src/shell/wt_channel/cli_channel.rs` (and
  `tools/wta/src/app.rs` for `publish`).

## Global flags

| Flag | Effect |
|------|--------|
| `--json` | Emit machine-readable JSON. Required for any caller that parses output. |

## Commands

The "Used in repo" column reflects whether some other component in this
repository actually shells out to that subcommand today (not whether the
subcommand is reachable). External callers (third-party agents, ad-hoc
scripts) are not counted.

| Command | Alias | What it does | Example | Used in repo |
|---------|-------|--------------|---------|--------------|
| `list-windows` | `lsw` | List all Terminal windows. | `wtcli --json list-windows` | ✅ `cli_channel.rs` (`list_windows`) |
| `list-tabs` | `lst` | List tabs in a window. `-w` defaults to the first window. | `wtcli --json list-tabs -w 1` | ✅ `cli_channel.rs` (`list_tabs`) |
| `list-panes` | `lsp` | List panes in a tab. `-t`/`-w` default to the first tab of the first window. | `wtcli --json list-panes -t 2` | ✅ `cli_channel.rs` (`list_panes`) |
| `active-pane` | — | Return metadata for the currently focused pane. Used by other subcommands as the default `-t` target. | `wtcli --json active-pane` | ✅ `cli_channel.rs` (`get_active_pane`) |
| `capture-pane` | `capturep` | Read pane scrollback as text. `-l` caps line count. `--last-prompt` returns only the most recent completed shell prompt (requires OSC 133 shell integration). | `wtcli --json capture-pane -t 3 --last-prompt` | ✅ `cli_channel.rs` (`read_pane_output`) |
| `pane-status` | — | Report pane process state: `pid`, `state` (`running`/`exited`), and `exit_code` when applicable. | `wtcli --json pane-status -t 3` | ✅ `cli_channel.rs` (`get_process_status`) |
| `new-tab` | `neww` | Create a new tab. `-c` command, `-n` title, `-d` cwd. | `wtcli --json new-tab -c "pwsh" -n "build" -d C:\src` | ✅ `cli_channel.rs` (`create_tab`) |
| `split-pane` | `splitw` | Split a pane. `-d right\|left\|up\|down\|auto` (default `automatic`). `-H`/`-v` are legacy aliases for `down`/`right`. `-s` is size fraction; `-c` is the command to run. | `wtcli --json split-pane -t 3 -d right -s 0.4 -c "tail -f log"` | ✅ `cli_channel.rs` (`split_pane`) |
| `kill-pane` | `killp` | Close a pane. | `wtcli kill-pane -t 4` | ✅ `cli_channel.rs` (`close_pane`) |
| `focus-pane` | `focusp` | Move focus to the given pane. | `wtcli focus-pane -t 3` | ✅ `cli_channel.rs` (`focus_pane`) |
| `wait-for` | — | Block (poll `pane-status`) until the pane process exits. `--interval` is poll period in ms; `--timeout` is seconds (`0` = forever). | `wtcli wait-for -t 3 --timeout 60` | ❌ Not called. (`wta` exposes its own `wait-for` subcommand at `tools/wta/src/main.rs:209`, but its handler polls by shelling out to `wtcli pane-status` in a Rust loop — it does **not** invoke `wtcli wait-for`.) |
| `listen` | — | Long-running. Subscribe to `IProtocolServer` and stream every event JSON line to stdout until Ctrl-C. `-t` filters by pane id; `--event` filters by type and supports a trailing `*` wildcard. | `wtcli --json listen --event "agent.*"` | ✅ `cli_channel.rs` (background listener task) |
| `send-event` | `se` | Publish an event using the `agent_event` envelope: sets `type=event`, `method=agent_event`, fills `params.event` from `-e` and `params.pane_id` from `-p` (or the active pane). Extra params come from the trailing JSON object. | `wtcli send-event -p 3 -e agent.task.completed '{"exit_code":0}'` | ❌ Not called from in-tree code. Documented as the public CLI surface for external agents in `doc/specs/llm-agent-event-integration.md`. |
| `publish` | — | Low-level escape hatch: forwards a raw JSON string straight to `IProtocolServer::SendEvent` with no envelope. Used for events that don't fit the `agent_event` shape (e.g. `autofix_state` routed directly to `TerminalPage`). | `wtcli publish '{"method":"autofix_state","params":{"state":"ready"}}'` | ✅ `tools/wta/src/app.rs` (`publish_event_blocking`) |
| `info` | — | Print `WT_COM_CLSID`, connection status, protocol version, and the server's `GetCapabilities()` method list. | `wtcli --json info` | ✅ `cli_channel.rs` maps `get_capabilities` → `wtcli info` |
| `test-pipe` | — | Smoke test: connect, run `list-windows` + `get_capabilities`, print results. Diagnostic only. | `wtcli test-pipe` | ❌ Not called. Manual diagnostic. |
| `set-env` | `setenv` | Print shell-specific export statements for `WT_COM_CLSID` (`-s powershell\|bash\|cmd`). Output is meant to be `eval`'d / `Invoke-Expression`'d by the caller; it does not modify the current process. | `wtcli set-env -s powershell \| Invoke-Expression` | ❌ Not called. Manual recovery for child shells that didn't inherit `WT_COM_CLSID`. |

## Summary

- **Wired into `wta` runtime (13):** `list-windows`, `list-tabs`,
  `list-panes`, `active-pane`, `capture-pane`, `pane-status`,
  `new-tab`, `split-pane`, `kill-pane`, `focus-pane`,
  `listen`, `info`, `publish`.
- **Defined but not invoked from in-tree code (4):** `wait-for`,
  `send-event`, `test-pipe`, `set-env`. These remain as public surface for
  external agents / shell scripts and for manual debugging.
