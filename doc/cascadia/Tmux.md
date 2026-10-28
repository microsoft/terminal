# tmux control mode

This experimental integration uses tmux's control protocol to display
remote windows as Terminal tabs and remote panes as native TermControls. A single
SSH connection carries all the panes. Live output goes through Terminal's normal
VT parser and renderer, not a periodically captured screen.

## Connecting and reconnecting

In **Settings > Profiles > [SSH profile] > General**, enable **Use tmux control
mode** to connect through the persistent `windows-terminal` session. The checkbox
is off by default and changes apply to new connections. The saved SSH command
line is left unchanged, so switching the option off restores plain SSH.

In an existing SSH or WSL shell, run:

```sh
tmux -u -CC new-session -A -s windows-terminal
```

Alternatively, enable the option in an SSH profile's JSON:

```json
{
    "name": "Remote SSH",
    "commandline": "ssh.exe remote-server",
    "ssh.useTmux": true,
    "historySize": 9001
}
```

The option applies to direct `ssh`/`ssh.exe` connection profiles, including
generated SSH profiles and manually configured ones. Keep positional remote
commands out of the base SSH command line when enabling it. An explicit command
line override takes precedence over the profile's launch mode.
Manual `tmux -CC` detection remains available even when the profile option is off.

The original tab becomes a gateway. Its Escape key detaches the client without
killing remote programs; `n` creates a remote window. Default new-tab actions
from an active tmux tab also create remote windows. Selecting an explicit profile
still opens that profile.
If an interactive SSH client dies without sending the control-stream terminator,
`X` leaves the broken gateway so you can reconnect from the outer shell.

Opening this profile again reattaches to the same named session. The integration
loads its windows, layouts, scrollback, visible screen, alternate screen, cursor,
scroll region, and terminal modes exposed by tmux. Closing Terminal or losing SSH
does not kill the server or its processes. Explicitly closing a native tmux pane
or tab kills the corresponding remote panes.

This is reattachment, not a server-restart recovery service. It cannot resurrect
processes after the remote machine or tmux server has stopped.

## tmux compatibility

Basic control mode is not restricted to a particular tmux 3.x release. The
implementation has been exercised against tmux 2.9a and 3.4. It probes for
per-window client sizing and falls back to legacy global sizing, including the
status-line height, when that feature is unavailable. Splits use portable integer
sizes. Input uses Unicode hexadecimal key values accepted by older servers,
including control characters and NUL, rather than requiring `send-keys -H`.
Neither capability negotiation nor command acknowledgments block typing.

Optional state restoration depends on the formats exported by the server.
This is also true of iTerm2: its current documentation describes tmux 3.5+ key-mode
exports and tmux 3.6+ clipboard query support. Missing optional formats do not
prevent attaching, native scrolling, colors, or session reattachment.

## Native terminal behavior

Scrollbars, search, selection, copy, paste, fonts, and rendering are provided by
Terminal. History is restored up to the profile's `historySize` and the history
actually retained by the server. Captures preserve ANSI and truecolor attributes,
Unicode, literal backslashes, and wrapped lines. Subsequent output streams
directly into each pane.

Native split, directional pane resize, and zoom actions issue the corresponding
tmux commands. Window resizes use per-window sizes where supported, otherwise the
smallest requested global client size. Changes made
by other tmux clients are reflected through control notifications.

For applications to *choose* 256-color/truecolor output, their remote environment
and terminfo must advertise it. For example, configure tmux's `default-terminal`
as `tmux-256color` where that terminfo is installed, and set `COLORTERM=truecolor`
in the remote shell. The integration does not silently change server-wide tmux
options or existing processes' environments.

Tmux, rather than both tmux and Terminal, answers application terminal queries.
The connection distinguishes terminal-generated responses from keyboard input.
Existing ConPTY-specific window ownership and lifecycle operations continue to
reach the underlying connection.

This implementation does not yet support moving tmux panes/tabs between Terminal
windows or mixing remote tmux panes with unrelated local panes. It does not claim
to restore state that the installed tmux version cannot export, such as previously
drawn image protocols or newer key-reporting modes absent from tmux 3.4.
Tmux 3.4 also does not export bracketed-paste state. Use a newer server exposing
`bracket_paste_flag` for faithful bracketed-paste restoration. Split-orientation
changes must currently be made with the remote `tmux select-layout` command.
Generated tmux tabs are excluded from saved Terminal workspace layouts: the
gateway profile reconnects once and reconstructs them from the server.

## Input and output

Input is encoded as Unicode hexadecimal `send-keys` values, queued to a transport
writer, and sent without waiting for command acknowledgments. Captures are
restricted to attach, new panes, and flow-control recovery. Inventory refreshes
are notification-driven and coalesced; neither screen captures nor UI-thread
dispatches are on the per-keystroke path.

Control records are parsed in spans, and consecutive output for the same pane
is combined within the received batch before entering the native terminal.
This adds no timer or wait for more output and preserves ordering across panes,
snapshots, and session lifecycle events.
