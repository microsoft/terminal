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

| Command | Alias | What it does | Example |
|---------|-------|--------------|---------|
| `list-windows` | `lsw` | List all Terminal windows, with each window's `name` and `title`. | `wtcli --json list-windows` |
| `list-tabs` | `lst` | List tabs in a window. `-w` picks the window (default: current); `--all` lists every window's tabs. | `wtcli --json list-tabs -w build` |
| `list-panes` | `lsp` | List panes in a tab. `-w` picks the window (default: current), `-t` the tab (default: its first); `--all` lists every pane in every window. | `wtcli --json list-panes -t 2` |
| `active-pane` |  | Return metadata for the currently focused pane. Used by other subcommands as the default `-t` target. | `wtcli --json active-pane` |
| `pane-status` |  | Report pane process state: `pid`, `state` (`running`/`exited`), and `exit_code` when applicable. | `wtcli --json pane-status -t 3` |
| `new-tab` | `newt` | Create a new tab. `-c` command, `-n` title, `-d` cwd, `-w` window (default: current). Unlike the read-only commands, `-w` here may create the window. | `wtcli --json new-tab -c "pwsh" -n "build" -d C:\src` |
| `split-pane` | `splitp` | Split a pane. `-d right\|left\|up\|down\|auto` (default `automatic`). `-H`/`-v` are legacy aliases for `down`/`right`. `-s` is size fraction; `-c` is the command to run. | `wtcli --json split-pane -t 3 -d right -s 0.4 -c "tail -f log"` |
| `kill-pane` | `killp` | Close a pane. | `wtcli kill-pane -t 4` |
| `focus-pane` | `focusp` | Move focus to the given pane. | `wtcli focus-pane -t 3` |

## Targeting a window with `-w`

`-w,--window` uses the same grammar as `wt --window,-w` (they both go through the
same parser: `src/cascadia/WindowsTerminal/WindowTarget.h`)

| `-w` value | Means |
|------------|-------|
| *(omitted)* | The current window. |
| `0`, `last` | The current window. |
| a positive integer | The window with that `window_id`, as reported by `list-windows`. |
| `new`, or a negative integer | A new window. |
| anything else | The window with that **name**, as reported by `list-windows`. |


### Creating windows with `new-tab`

`wtcli new-tab -w new` and `wtcli new-tab -w <unused-name>` create the window
and make the requested tab its first tab.

A new window builds that tab asynchronously, so by default the server waits up
to 10s for it and then reports the usual `session_id`/`pid`. Pass `--no-wait` to
return as soon as the window exists. Either way, if the tab isn't ready the
result is still a success, flagged with `"pending": true`:

```console
$ wtcli --json new-tab -w new --no-wait
{"pending":true,"pid":0,"session_id":"{00000000-0000-0000-0000-000000000000}","tab_id":0,"window_id":4}
```

When `pending` is true, only `window_id` is meaningful. Follow up with
`wtcli --json list-panes -w <window_id>` once the window has started. Scripts
should branch on `pending` rather than testing `session_id` for emptiness.

Targeting a name that has a **saved workspace** restores that workspace, and the
requested tab is dropped, because a persisted layout takes precedence over requested
startup actions. This matches `wt -w <name>`.
