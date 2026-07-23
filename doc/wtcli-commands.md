# wtcli Command Reference

`wtcli` is the CLI client for the Windows Terminal Protocol. It calls
`CoCreateInstance` using the per-brand `CLSID` and exposes a tmux-style command surface over its IDL methods.

- Source: `src/tools/wtcli/main.cpp`
- IDL: `src/host/proxy/ITerminalProtocol.idl`
- Primary in-tree caller: `tools/wta/src/shell/wt_channel/cli_channel.rs`.

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

## Summary

- **Wired into `wta` runtime (10):** `list-windows`, `list-tabs`,
  `list-panes`, `active-pane`, `capture-pane`, `pane-status`,
  `new-tab`, `split-pane`, `kill-pane`, `focus-pane`.