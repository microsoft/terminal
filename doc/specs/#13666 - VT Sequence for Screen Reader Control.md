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

In 2019, Daniel Imms proposed a VT sequence to control screen readers directly from the command-line application[^2]. In summary, this VT sequence allows command-line applications to (1) stop reading new output temporarily, (2) override what text is read by the screen reader, and (3) supply additional text to be ready by the screen reader. As presented in the spec from the Terminal working group:
> `OSC Ps ; Pt ST`
> - `Ps = 2 0 0` -> Stop announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately. The screen reader will resume announcing incoming data if any key is pressed.
> - `Ps = 2 0 1` -> Resume announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately.
> - `Ps = 2 0 2` -> Announce `Pt` immediately to the screen reader.
> Note that the reason any key press will force the screen reader to announce again is to prevent situations where applications are terminated while the screen reader is not announcing or where applications are misbehaving.

## Potential Solutions

### Option 1: Override Notifications

#### Option 1A: Override Notifications
This option is the one presented by Daniel Imms in the Terminal working group:
> `OSC Ps ; Pt ST`
> - `Ps = 2 0 0` -> Stop announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately. The screen reader will resume announcing incoming data if any key is pressed.
> - `Ps = 2 0 1` -> Resume announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately.
> - `Ps = 2 0 2` -> Announce `Pt` immediately to the screen reader.
> Note that the reason any key press will force the screen reader to announce again is to prevent situations where applications are terminated while the screen reader is not announcing or where applications are misbehaving.

Upon further discussion with Daniel Imms and the Terminal team, several concerns were raised. Such concerns include the following:
- adoption by command-line applications may be difficult due to the extra overhead and un-intuitiveness of this spec
- the story for embedding this into the buffer is unclear as an application may maliciously (or accidentally) forget to resume announcing data, providing an inconsistent experience between output text and scan mode
- since OSC numbers are a finite resource, we should consolidate the three `Ps` values

Though this is a very versatile option, for the reasons mentioned above, we have decided to no longer move forward with this option.

#### Option 1B: Override Notifications - Consolidated Version
This is the same solution as option 1A, however, the OSC numbers have been consolidated:
> `OSC 200; Ps ; Pt ST`
> - `Ps = 0` -> Stop announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately. The screen reader will resume announcing incoming data if any key is pressed.
> - `Ps = 1` -> Resume announcing incoming data to screen reader, `Pt` is an optional string that will be announced immediately.
> - `Ps = 2` -> Announce `Pt` immediately to the screen reader.
> Note that the reason any key press will force the screen reader to announce again is to prevent situations where applications are terminated while the screen reader is not announcing or where applications are misbehaving.

However, this still does not address the other concerns, so it has also been decided against for that reason.

### Option 2: Decorative Tags
This is a more fine-tuned solution designed to hide data from the screen reader.
> `OSC 200; Ps ST`
> - `Ps = 0` -> Stop announcing incoming data to screen reader. The screen reader will resume announcing incoming data if any key is pressed.
> - `Ps = 1` -> Resume announcing incoming data to screen reader.
> Note that the reason any key press will force the screen reader to announce again is to prevent situations where applications are terminated while the screen reader is not announcing or where applications are misbehaving.

This solution is a simplified version of option 1, without the ability to provide text to be read out by the screen reader. Though it is less versatile, most scenarios generally already provide some kind of text to the user. For example, consider a progress bar that is being redrawn to illustrate a bar filling up. Generally, these progress bars have the percentage number displayed beside it and being updated. Decorative tags would mark the progress bar as decorative, resulting in the screen reader only reading the percentage number drawn beside it.

The main concern with this solution is versatility. Though it is really good at solving one problem, it doesn't necessarily solve all the problems we need it to solve. Particularly, the `gh pr create` experience mentioned earlier in the spec would not be improved given this option.

### Option 3: Semantic Embedding
This builds off the fine-tuned aspect of option 2. This option is also inspired by HTML's Accessible Rich Internet Applications (ARIA) which are designed to provide hints to the screen reader. The idea is essentially to create tags for different interaction models.
> `OSC 200; Ps; Pt; Pu; Pv ST`
> - `Ps = presentation` -> The range of text is decorative and should not be announced to the screen reader.
> - `Ps = option` -> The range of text represents an option the user can choose from.
>    - `Pu = 0` -> The option is marked as unselected.
>    - `Pu = 1` -> The option is marked as selected.
> - `Ps = table` -> The range of text belongs to a table.
>    - `Pu` -> (optional) string representing the column header. Empty string is acceptable.
>    - `Pv` -> (optional) string representing the row header. Empty string is acceptable.
> 
> - `Pt = 0` -> End tagging the range of text.
> - `Pt = 1` -> Begin tagging the range of text.

This solution builds on the the positive takeaways from option 2 by providing a more fine-tuned solution. Command-line application developers can specifically use the `Ps` subset that is relevant to their interaction model. From the terminal emulator side, implementation can be done one interaction model at a time, which may reduce the overhead burden of making changes to support this feature.

### Option 4: Screen Reader Flag
This is a simple solution that allows the command-line application to know if a screen reader is attached.
> `DSR` - Screen Reader
> - command-line application query: `DSR ? 2575 n`
> - terminal emulator response: `DSR ? Ps n`
>    - `Ps = 2570` -> Screen reader is not attached
>    - `Ps = 2571` -> Screen reader is attached
`DSR` is already a standard method for command-line applications to query the capabilities of the attached terminal emulator. By claiming a value, the terminal can easily respond to let the command-line application know if a screen reader is attached or not. In the event the terminal emulator does not support this feature, no response is given, which is common practice.

Introducing a formal `DSR` query can be used by command-line applications to make changes on their end if they decide there is a more accessible way to represent their content. PowerShell, for example, disables PSReadline if they detect a screen reader is attached. However, PowerShell's method of doing so can be inconsistent and is a Windows-specific approach. By defining this method via control sequences, other command-line applications can achieve a similar benefit across platforms outside of Windows.

### Option 5: Synchronized Updates
This solution was proposed by Martin Hostettler in the Terminal working group in response to option 1 [4].  The idea is essentially to use a control sequence to denote the beginning and end of a synchronized update:
> `ESC P = 1 s ESC \` -> Begin synchronized update
> `ESC P = 2 s ESC \` -> End synchronized update

Screen readers could then look at the changed zones and deduce the structure of the screens content to identify what useful information should be read. Alternatively, the terminal emulator itself can deduce what content is useful and can then tell the attached screen reader exactly what to read.

The main concern with this solution is engineering workload. The amount of work necessary to support this feature is fairly large and ambiguous. As a result, it may result in different implementations across terminal emulators, if the developers for those terminal emulators can even be motivated to implement it. With this solution requiring changes on the command-line application side and major work on the terminal emulator side, it seems unlikely that this solution could be common practice when implementing command-line apps or terminal emulators.

## Committed Solutions

### Semantic Embedding (Option 3)
The general structure of this control sequence is as follows:
> `OSC 200; Ps; params; Pu ST`
> - `Ps`: The semantic role that is being used
> - `params`: The optional list of parameters represented as `key=value` assignments, separated by the `:` character. This is inspired by OSC 8 [^5].
> - `Pu`: A value representing that the range of text is beginning or ending.
>    - `Pu = 0`: The beginning of the range of text.
>    - `Pu = 1`: The end of the range of text.

Similar to OSC 8, this design provides significant versatility that can easily be expanded on with other roles. Additionally, as Egmont Koblinger stated in his OSC 8 spec, this design permits switching from one role to another without explicitly closing the first one. 

As mentioned earlier, this solution takes inspiration from HTML's ARIA roles. The role itself will be defined using `Ps`, and any additional properties will be defined using `params`.

#### "Presentation" Role
This prevents the range of text from being exposed to the accessibility tree. The ARIA documentation can be found [here](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/presentation_role).
> - `Ps`: `presentation` or `none`
> - `params`: none

#### "Option" Role
This is used for selectable items. The ARIA documentation can be found [here](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/option_role);
> - `Ps`: `option`
> - `params`:
>    - `selected`: `true` if the option is currently selected. `false` otherwise. (default: `false`)
>    - `checked`: describes the checked state when options are used in a multiple selection fashion. Supports `true`, `false`, and `mixed`. (default: `null`)
>    - `posinset`: defines the element's number or position in the current set of options. Particularly useful if only a subset of items are visible. (default: `-1`, which represents unknown)
>    - `setsize`: defines the number of items in the current set of options. (default: `-1`, which represents unknown)

#### "Suggestion" Role
This is used for proposed changes to an editable area. The ARIA documentation can be found [here](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/suggestion_role).
> - `Ps`: `suggestion`
> - `params`: none

<!--
>    - `role`: `insertion` if the provided text will be inserted into the text input area. `deletion` if the provided text will be removed from the text input area. (default: `insertion`)
-->

The ARIA spec has `insertion` and `deletion` as child elements for a `suggestion` tag. However, this spec has been simplified for the terminal so that terminals don't need to keep track of nested roles.

#### "Cell" Role
This is used for cells in a tabular container. The ARIA documentation can be found [here](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/cell_role).
> - `Ps`: `cell`
> - `params`:
>    - `rowheader`: a header for the corresponding row of the current cell. (default: `""`)
>    - `columnheader`: a header for the corresponding column of the current cell. (default: `""`)
>    - `rowindex`: the index for the corresponding row of the current cell. (default: `null`)
>    - `colindex`: the index for the corresponding column of the current cell. (default: `null`)
>    - `rowsize`: the number of rows in the table. (default: `null`)
>    - `colsize`: the number of columns in the table. (default: `null`)

The ARIA spec relies on nested roles to provide header context to cells. this spec has been simplified for the terminal so that terminals don't need to keep track of nested roles.

### Screen Reader Flag (Option 4)

As mentioned earlier, `DSR` is already a standard method for command-line applications to query the capabilities of the attached terminal emulator. By claiming a value, the terminal can easily respond to let the command-line application know if a screen reader is attached or not. In the event the terminal emulator does not support this feature, no response is given, which is common practice.
> `DSR` - Screen Reader
> - command-line application query: `DSR ? 2575 n`
> - terminal emulator response: `DSR ? Ps n`
>    - `Ps = 2570` -> Screen reader is not attached
>    - `Ps = 2571` -> Screen reader is attached

Introducing a formal `DSR` query can be used by command-line applications to make changes on their end if they decide there is a more accessible way to represent their content. PowerShell, for example, disables PSReadline if they detect a screen reader is attached. However, PowerShell's method of doing so can be inconsistent and is a Windows-specific approach. By defining this method via control sequences, other command-line applications can achieve a similar benefit across platforms outside of Windows.

## Solution Design

### 1. VT Sequence Parsing
The `OutputStateMachineEngine` would need to be updated to include support for the new `OSC 200` sequence. These would call an API out of `ITermDispatch`, which would need to be updated to extract the semantic role `Ps`, the optional string parameter `params`, and the range starter/terminator `Pu`. Since the new sequence is structured fairly similarly to `OSC 8` (which uses `OutputStateMachineEngine::_ParseHyperlink()` to parse the sequence), a function very similar to `_ParseHyperlink()` will be introduced.

For the screen reader flag, a new `DispatchTypes::StatusType` will need to be introduced and added to `AdaptDispatch::DeviceStatusReport()`. This will generate a response based on the existence of a `TermControlAutomationPeer`.

### 2.A. Windows Terminal Support

#### Announcing Incoming Text
The `Terminal` layer has access to the `UiaRenderer` which is responsible for batching UIA events and notifications at the renderer's refresh rate. `Terminal` already sends new output to the `UiaRenderer` in preparation for it to dispatch a UIA notification; this method is called `TriggerNewTextNotification()`. This same function can be used to explicitly have the screen reader read the given string. The string of interest will be constructed based on the associated role for the semantic embedding sequence:
- `presentation`
   - do not forward the range of text to the automation peer
- `option`:
   - read the text range (text for the option)
   - if `posinset` and `setsize` are defined, read `"<posinset> of <setsize>`
   - if `selected` is `true`, read `"option selected"`. Otherwise, read `"option unselected"`
   - if `checked` is defined, ignore `selected` property and readings. Instead read `checkbox checked` if "true", `checkbox unchecked` if "false", or `checkbox indeterminate` if "mixed".
- `suggestion`:
   - read `"suggested text"`
   - read the text range
- `cell`:
   - if `rowindex` and `rowsize` are defined, read `"row <rowindex> of <rowsize>"`
   - if `colindex` and `colsize` are defined, read `"column <colindex> of <colsize>"`
   - read the text range (cell contents)

More explicitly, as text is written to the screen and detected by the `UiaRenderer`, we must check if the text belongs to a range above. If so, suppress announcing that text and store it. Once the range ends, synthesize the announcement using the heuristic above and the stored text.

The synthesized text is then sent to the `TermControlAutomationPeer::NotifyNewOutput()` function as normal, which dispatches a UIA notification for the screen reader to immediately read.

#### Handling Scan Mode
Scan mode is a mode where the screen reader can navigate the text buffer and read its contents. The user may navigate the text region by different units such as character, word, and line. This means that the screen reader may navigate over partial or multiple ranges when in scan mode.

Generally, this functionality consists of a `UIA::Move()` call to update an existing text range, followed by a `UIA::GetText()` call to extract and read the text of interest. In our implementation of `UiaTextRange`, `GetText()` will need to be modified to check if the extracted text belongs to a semantically embedded range. The process will look something like the following:
0. `range` is provided as a set of two coordinates in the buffer that needs to have text extracted
1. iterate over the `range`, checking if the cell has an embedded semantic control sequence
   - if a semantic range is started (i.e. `OSC 200; option; ; 1 ST`), all future text is assumed to belong to this semantic range and is stored separately.
   - if a semantic range is concluded (i.e. `OSC 200; option; ; 0 ST`), all stored text is assumed to belong to this semantic range. A new string is synthesized using the heuristic above and inserted into the text to be returned. The stored text is then cleared.
2. if `range` has no more text left, but a semantic range was started but not concluded, the remaining text will be used to synthesize a new string as in the step above.

In the event that a semantic range is concluded without having been started, any text received up until that point is assumed to have belonged to the semantic range.

As a result, partial ranges will be announced to the user as a semantic range. Though it may not be clear that this is a partial range, the user should be able to know that the range is a part of an `option` or `suggestion`. Multiple ranges will be announced correctly and fully using the synthesis process above.

The only exception for this heuristic is the `presentation` role. If the user is in scan mode, any `presentation` text will still be presented to the user.

### 2.B. ConHost Support

#### Announcing Incoming Text

ConHost does not support UIA notifications in the same way Windows Terminal does. Though it can dispatch UIA notifications, a `UiaRenderer` doesn't exist, which means that it cannot record new output to then add as a payload to the UIA notification. It only supports UIA text changed events, which operate by notifying the screen reader that the output text has changed, and leaving the work of figuring out what changed (and, thus, what to present) to the screen reader.

As such, there is no way to inject the semantic metadata into the announced string because the screen reader is the one determining what text has changed and what is relevant to be announced.

There is a desire from the screen reader community to add UIA Notifications to ConHost, and doing so would allow semantic embedded text ranges to be announced properly. It may be worth prototyping UIA Notifications behind a setting or a feature flag, and disabling text changed events altogether. However, this is a risky change that requires significant testing and buy-in from other screen readers, especially since ConHost is not updated as easily as Windows Terminal.

#### Handling Scan Mode
The same behavior and changes can be expected from the Windows Terminal implementation as `UiaTextRange` is a shared component between ConHost and Windows Terminal.

## UI/UX Design

### Screen Reader Support
Narrator and JAWS automatically read out UIA notifications from Windows Terminal and ConHost so the experience should be as expected. NVDA does the same for ConHost. However, NVDA currently ignores all UIA notifications unless a setting is enabled. Since UIA notifications can have an associated ID (think of it more like a class id for the notification), that may need to be set differently from the current `"TerminalTextOutput"`. NVDA would then need changes on their end to either allow the new notification event, or specifically ignore `"TerminalTextOutput"` notifications. Regardless, long-term, the setting that enables listening to UIA notifications for Windows Terminal will eventually be enabled by default as it provides major performance enhancements.

### Command-line Application Support
From the command-line application side, there are a number of internal partners that we could reach out to to start adoption of this new control sequences:
- Winget has expressed interest to use this for progress bars[^3]
- PowerShell could use this for...
	- first-party progress bars
	- suggestions
	- tab completion (specifically menu complete)
- GitHub CLI could use this for commands that prompt the user (i.e. `gh pr create`)

Third-party applications would also need updates on their end to adopt the new control sequences or take advantage of them.

## Capabilities

### Accessibility

The whole feature is an accessibility feature that will make terminal content and command-line applications more accessible for screen reader users.

### Security

Theoretically, someone could use `OSC 200; presentation` maliciously by disabling UIA notifications. Thankfully, scan mode does not adhere to `OSC 200; presentation`, so any output text can still be manually scanned by the user. Additionally, the various roles introduced in the `OSC 200` sequence are expected to be disjoint. Thus, if another `OSC 200` sequence is introduced, it will be assumed that the former semantically embedded range is now concluded. Handling this scenario is an implementation detail excluded in the solution design above to maintain a more simple solution.

### Reliability

N/A

### Compatibility

As is standard with control sequences, if a terminal emulator that does not support these control sequences receives them, the control sequence will be ignored, and the output text will remain the same. As a result, the experience should be no different than it would have been before this spec was even introduced.

That said, there are many other terminal emulators out there. The Windows ecosystem uses UI Automation to enable accessible experiences, so any other terminal emulators on Windows may opt-in to support these control sequences by using the UIA Notification heuristic explained in the solution design above. However, command-line applications are also cross-platform, and thus may run on MacOS or other Linux distributions (i.e. Ubuntu). In order to encourage the use of these control sequences by command-line applications, there is a goal and consideration for support for these sequences to be relatively straightforward and inexpensive.

MacOS' AppKit enables accessible experiences using the `NSAccessibility` namespace. MacOS Terminal and iTerm2 maintain a text area to expose the terminal's contents, as expected. `NSAccessibility` supports an `announcementRequested` of the type `NSAccessibility.Notification` which is used to make an announcement to the user, similar to UIA Notifications on Windows. For newly output text, MacOS terminal emulators may be able to leverage this to announce the semantically embedded output upon writing the output text.

On the other hand, Linux enables accessible experiences via the `IAccessible2` API. This API complements Microsoft's work on MSAA, the predecessor to UI Automation [^6]. This API doesn't seem to have support for an immediate notification system like UIA Notifications where content is immediately presented by a screen reader. However, an `IAccessibleText` interface exists to provide read-only access to text [^7]. This means that Linux screen readers (i.e. Orca) are forced to rely on this interface to extract text from the text buffer and present it to the user. Thus, if the implementation is modified as explained in the "Handling Scan Mode" section above, the terminal emulator can represent semantically embedded text ranges properly.

### Performance, Power, and Efficiency

N/A

## Potential Issues

- Adoption:
   - This is probably the main concern with introducing a new control sequence as adoption requires changes on the command-line application side and the terminal emulator side across multiple OS platforms. The research outlined in the "Compatibility" section above shows that adoption on the terminal emulator side is feasible at a relatively inexpensive cost. With Windows Terminal now being the default terminal on Windows 11, this will hopefully encourage more adoption on the command-line application side.
- Semantic embedding: forgetting to close a `presentation` range
   - Since `presentation` ranges are not announced to the screen reader, it could be a problem that these are used maliciously to hide data from the user. However, two mechanisms are in place to circumvent this issue. First, `presentation` ranges are hidden from output announcements, not scan mode, so the user can still use scan mode to read over the desired text. Second, if another `role` is used, the `presentation` range is assumed to have ended, thus making the screen reader resume announcing newly output text.

## Future considerations

- Additional support for semantic roles
   - More roles and properties can be added to the semantic embedding spec as desired. Theoretically, a completely separate terminal emulator may introduce their own roles or properties that provide helpful metadata for accessibility (or non-accessibility) needs.
- Semantic embedding by the terminal emulator
   - Since the terminal emulator owns the text buffer, theoretically, there is nothing that prevents the terminal itself from parsing the text buffer and inferring what interaction models are being used. An AI could possibly be trained to detect these scenarios and embed semantic control sequences into the buffer. This would work well for scan mode, however, it would probably not work for output notifications without some form of synchronizing or batching output notifications.
- Rich control placement in terminal area
   - With specific interaction models being described in the text buffer, these text areas could be represented with rich UI controls. Consider an `option` role being represented with a combo box, for example.

## Resources

[1]: [Unable to use applications that hook the arrow keys using Windows Console Host. · Issue #13666 · microsoft/terminal (github.com)](https://github.com/microsoft/terminal/issues/13666)
[2]: [Control Screen Reader from Applications (#18) · Issues · terminal-wg / specifications · GitLab](https://gitlab.freedesktop.org/terminal-wg/specifications/-/issues/18)
[3]: [Client Verbosity Settings · Issue #161 · microsoft/winget-cli (github.com)](https://github.com/microsoft/winget-cli/issues/161)
[4]: [Synchronized Updates Spec](https://gitlab.com/gnachman/iterm2/-/wikis/synchronized-updates-spec)
[5]: [Hyperlinks in Terminal Emulators spec](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda)
[6]: [IAccessible2 documentation](https://wiki.linuxfoundation.org/accessibility/iaccessible2/start)
[7]: [IAccessible2 IAccessibleText Interface Reference](https://accessibility.linuxfoundation.org/a11yspecs/ia2/docs/html/interface_i_accessible_text.html)