# Runtime Title Variables

`tabTitleFormat` and `tabTitleSeparator` are profile settings and inherit through
`profiles.defaults`. `windowTitleFormat` and `windowTitleSeparator` are global
settings. Existing `tabTitle`, `--title`, application titles, and runtime renames
remain literal strings.

```json
{
    "windowTitleFormat": "${workspaceName}${separator}${tabTitle}",
    "windowTitleSeparator": " - ",
    "profiles": {
        "defaults": {
            "tabTitleFormat": "${paneTitle}${separator}${otherPanes}",
            "tabTitleSeparator": " "
        }
    }
}
```

The default tab format is `null`; the default window format is shown above.
Both separators default to `" - "`. Explicit `null` selects the existing title
path; on a profile it also clears an inherited format. Empty or invalid results
fall back without evaluating another template. Runtime tab renames override tab
formatting until reset.

A non-null window format owns the entire caption and takes precedence over
`showTerminalTitleInTitlebar`. Existing settings that disable that legacy option
without specifying a window format migrate to `windowTitleFormat: null`.

## Syntax

Use `${name}` for a case-sensitive variable. Expansion is single-pass: variable
values are never parsed. `$${` emits literal `${`; backslashes are ordinary text
after JSON decoding. Expressions, nested references, and unclosed references
invalidate a format. Unknown variables are empty and produce a settings warning.

`${separator}` separates nonempty segments. Leading/trailing boundaries and
boundaries around empty segments disappear. Literal punctuation is not removed:
`(+${numOtherPanes})` renders `(+)` with a single pane. Use `${otherPanes}` for an
optional complete `[+N]` decoration. Expanded captions normalize control
characters and trim outer whitespace.

## Variables

| Variable | Value |
| --- | --- |
| `paneTitle` | Accepted unformatted pane title, or the existing selected-group label. |
| `tabTitle` | Effective tab title including rename; window templates only. |
| `profileName` | Active terminal pane's profile display name. |
| `workspaceName`, `windowName` | Current named window/workspace; empty when unnamed. |
| `appName` | Product display name from application metadata. |
| `tabIndex` | One-based visual tab position, including non-terminal tabs. |
| `numPanes` | Terminal leaf count across the entire tab, including zoom-hidden panes. |
| `numOtherPanes` | Count minus one, empty when zero. |
| `otherPanes` | Complete `[+N]` decoration, empty for one pane. |
| `elevated`, `elevatedShield` | Localized administrator label or U+1F6E1 for known elevated local connections. |
| `shellName`, `shellPid` | Launched local executable basename and PID when known to ConPTY. |
| `processName` | Reserved for a reliable active-local-process provider; currently unavailable. |
| `cwd`, `cwdFolder` | Existing working directory and its final component, initially seeded from the configured starting directory and updated by supported directory reports; roots remain roots. |
| `env:NAME` | Effective ConPTY launch environment value after profile overrides. |

Leaf-specific values are empty for a selected pane group; its most recently
focused terminal profile selects the tab format. Non-terminal tabs retain their
own title logic and expose only tab/window identity to the window formatter.

Directory variables use the same working directory as other terminal operations.
Without directory reports, this value does not track shell directory changes.
Environment lookups use Windows case-insensitive rules and are
not live shell state. Inbound console handoffs have no launch-environment snapshot.
No process-tree scraping, remote-state inference, shell execution, or refresh
timer is used.

Putting an environment value in a title exposes it in tabs, window captions,
task switching, taskbar previews, and screen sharing. Do not use secrets.

## Provider Follow-Up

`processName` needs a reliable foreground-process provider before that feature
is complete. Connections without metadata support return empty values; a local
SSH client does not reveal a remote process or its privilege level. Existing
elevation chrome is unchanged. The formatter does not strip application-provided
administrator prefixes.