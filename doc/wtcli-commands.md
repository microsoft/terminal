# wtcli Command Reference

`wtcli` is the CLI client for the Windows Terminal Protocol. It calls
`CoCreateInstance` using the per-brand `CLSID` and exposes a tmux-style command surface over its IDL methods.

- Source: `src/tools/wtcli/main.cpp`
- IDL: `src/host/proxy/ITerminalProtocol.idl`

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
| `list-windows` | `lsw` | List all Terminal windows. | `wtcli --json list-windows` | — |
| `list-tabs` | `lst` | List tabs in a window. `-w` defaults to the first window. | `wtcli --json list-tabs -w 1` | — |
| `list-panes` | `lsp` | List panes in a tab. `-t`/`-w` default to the first tab of the first window. | `wtcli --json list-panes -t 2` | — |
| `active-pane` | — | Return metadata for the currently focused pane. Used by other subcommands as the default `-t` target. | `wtcli --json active-pane` | — |
| `capture-pane` | `capturep` | Read pane scrollback as text. `-l` caps line count. `--last-prompt` returns only the most recent completed shell prompt (requires OSC 133 shell integration). | `wtcli --json capture-pane -t 3 --last-prompt` | — |
| `pane-status` | — | Report pane process state: `pid`, `state` (`running`/`exited`), and `exit_code` when applicable. | `wtcli --json pane-status -t 3` | — |
| `new-tab` | `neww` | Create a new tab. `-c` command, `-n` title, `-d` cwd. | `wtcli --json new-tab -c "pwsh" -n "build" -d C:\src` | — |
| `split-pane` | `splitw` | Split a pane. `-d right\|left\|up\|down\|auto` (default `automatic`). `-H`/`-v` are legacy aliases for `down`/`right`. `-s` is size fraction; `-c` is the command to run. | `wtcli --json split-pane -t 3 -d right -s 0.4 -c "tail -f log"` | — |
| `kill-pane` | `killp` | Close a pane. | `wtcli kill-pane -t 4` | — |
| `focus-pane` | `focusp` | Move focus to the given pane. | `wtcli focus-pane -t 3` | — |
