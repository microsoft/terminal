---
author: Carlos Zamora @carlos-zamora
created on: 2022-11-03
last updated: 2022-11-04
issue id: 13666
---

# VT Sequence for Screen Reader Control

## Abstract

Now that Windows Terminal has a UI Automation implementation that can be leveraged by screen readers, the content of the buffer can be explored and engaged with via screen readers. Additionally, the introduction of UIA notifications opened the possibility for screen readers to directly speak content output from the terminal application. However, not all command-line application scenarios are necessarily accessible as they are generally limited to moving the cursor and writing output to the cursor's position. As such, command-line applications require a way to directly interact with the screen reader as they know the best way to represent their content.

## Inspiration

In 2022, a community member reported a bug on Windows Terminal that the GitHub CLI command `gh pr create` would not be read out properly[^1]. The problem was caused by GitHub CLI rewriting a prompt to highlight a selected option using a '>' prefix. This resulted in screen readers re-reading the entire prompt instead of only the selected option because Windows Terminal dispatches UIA notifications for output text.

In 2019, Daniel Imms proposed a VT sequence to control screen readers directly from the command-line application[^2]. In summary, this VT sequence allows command-line applications to (1) stop reading new output temporarily, (2) override what text is read by the screen reader, and (3) supply additional text to be ready by the screen reader. As presented in the spec from ther Terminal working group:
> `OSC Ps ; Pt ST`
> - `Ps = 2 0 0` -> Stop announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately. The screen reader will resume announcing incoming data if any key is pressed.
> - `Ps = 2 0 1` -> Resume announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately.
> - `Ps = 2 0 2` -> Announce `Pt` immediately to the screen reader.
> Note that the reason any key press will force the screen reader to announce again is to prevent situations where applications are terminated while the screen reader is not announcing or where applications are misbehaving.

## Solution Design

### 1. VT Sequence Parsing
The `OutputStateMachineEngine` would need to be updated to include support for the new `OSC 200`, `OSC 201`, and `OSC 202` sequences. These would call an API out of `ITermDispatch`, which would need to be updated to extract the optional string parameter. This string represents text that will be read by the screen reader. From here, the associated terminal (in our case, either ConHost or Windows Terminal) will expose an API that will be called to notify the screen reader and suppress/resume UIA output events/notifications.

### 2.A. Windows Terminal Support
The `Terminal` layer has access to the `UiaRenderer` which is responsible for batching UIA events and notifications at the renderer's refresh rate. `Terminal` already sends new output to the `UiaRenderer` in preparation for it to dispatch a UIA notification; this method is called `TriggerNewTextNotification()`. This same function can be used to forward the optional string from the new VT sequence. The text is then sent to the `TermControlAutomationPeer::NotifyNewOutput()` function as normal, which dispatchees a UIA notification for the screen reader to immediately read.

`OSC 200` and `OSC 201` will respectively suppress and resume notifications from `TermControlAutomationPeer`. Upon handling these sequences in the `Terminal` layer, the `UiaRenderer` will be instructed to enable or disable a "suppress UIA text events" flag that it owns. Since `UiaRenderer` is a central point to handle batching UIA notifications for new output, any additional output will then be added or suppressed before forwarding it to the `TermControlAutomationPeer`. If "suppress UIA text events" is enabled, UIA text changed events will also be suppressed. Additionally, if a key stroke is detected via the `TermControl` layer, the `UiaRenderer` will need to disable the "suppress UIA text events" flag in order to resume sending notifications to the screen reader.

### 2.B. ConHost Support
ConHost does not support UIA notifications in the same way Windows Terminal does. Though it can dispatch UIA notifications, a `UiaRenderer` doesn't exist, which means that it cannot record new output to then add as a payload to the UIA notification. It only supports UIA text changed events, which operate by notifying the screen reader that the output text has changed, and leaving the work of figuring out what changed (and, thus, what to present) to the screen reader.

As a prototype, it is worth validating the experience of suppressing/resuming UIA text changed events upon receiving `OSC 200` and `OSC 201` respectively. `WindowUiaProvider` is the central location for dispatching text changed events. Upon parsing the VT sequence in `AdaptDispatch`, a flag will be set on `WindowUiaProvider` to suppress/resume UIA text changed events. `HandleKeyEvent()` in windowio.cpp will need to be updated to resume UIA text changed events via the `WindowUiaProvider`.

The optional string parameter can still be dispatched via a UIA notification using the `UIAutomationCore` API immediately.

## UI/UX Design

### Screen Reader Support
Narrator and JAWS automatically read out UIA notifications from Windows Terminal and ConHost so the experience should be as expected. NVDA does the same for ConHost. However, NVDA currently ignores all UIA notifications unless a setting is enabled. Since UIA notifications can have an associated ID (think of it more like a class id for the notification), that may need to be set differently from the current `"TerminalTextOutput"`. NVDA would then need changes on their end to either whitelist the new notification event, or specifically blacklist `"TerminalTextOutput"` notifications. Regardless, long-term, the setting that enables listening to UIA notifications for Windows Terminal will eventually be enabled by default as it provides major performance enhancements.

### Command-line Application Support
From the command-line application side, there are a number of internal partners that we could reach out to to start adoption of this new VT sequence:
- Winget has expressed interest to use this for progress bars[^3]
- PowerShell could use this for...
	- first-party progress bars
	- suggestions
	- tab completion
- GitHub CLI could use this for commands that prompt the user (i.e. `gh pr create`)

Third-party applications would also need updates on their end to adopt the new VT sequence.

### Scan Mode Experience
Three scenarios this VT sequence would make more accessible include:
1. text is being redrawn on top of existing text (i.e. progress bars)
2. prompts where the user must select an option using the arrow keys (i.e. `gh pr create`)
3. supplementary content is displayed with different visual characteristics (i.e. PowerShell suggestions)

Scan mode is a mode where the user can use the screen reader to navigate the text manually. In the scenarios listed above, the user should expect the following experiences when in scan mode:
1. The progress bar should be read in the way it is drawn. The VT sequence data should not be embedded into the terminal because it would be more confusing to read out "10%" and "20%" depending on where the user is scanning the progress bar. Instead, the progress bar should be displayed as "historical" content that had already occurred.
2. The output text should be read in the way it is drawn. Alt text doesn't make sense for this scenario.
3. The output text should be read in the way it is drawn.

In the future, if alt text is functionality the community is interested in, a separate VT sequence should probably be introduced to provide that functionality.

## Capabilities

### Accessibility

The whole feature is an accessibility feature that will make terminal content and command-line applications more accessible for screen reader users.

### Security

Theoretically, someone could use `OSC 200` maliciously by disabling UIA text changed events and notifications. However, UIA events and notifications will be re-enabled upon any key press so the user is able to overcome this issue very easily.

### Reliability

N/A

### Compatibility

N/A

### Performance, Power, and Efficiency

N/A

## Potential Issues

Screen reader users may experience some confusion if they hear a notification say "foo", but then enter scan mode and cannot find the "foo" they heard.

## Future considerations

In the future, if alt text is functionality the community is interested in, a separate VT sequence should probably be introduced to provide that functionality.

## Resources

[1]: [Unable to use applications that hook the arrow keys using Windows Console Host. · Issue #13666 · microsoft/terminal (github.com)](https://github.com/microsoft/terminal/issues/13666)
[2]: [Control Screen Reader from Applications (#18) · Issues · terminal-wg / specifications · GitLab](https://gitlab.freedesktop.org/terminal-wg/specifications/-/issues/18)
[3]: [Client Verbosity Settings · Issue #161 · microsoft/winget-cli (github.com)](https://github.com/microsoft/winget-cli/issues/161)