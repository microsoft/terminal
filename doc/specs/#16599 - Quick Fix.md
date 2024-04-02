---
author: Carlos Zamora @carlos-zamora
created on: 2024-04-01
last updated: 2024-04-02
issue id: "#16599"
---
# Quick Fix
## Solution Design
Quick Fix will be a new UI surface that allows the user to interact with the terminal and leverage the context of a specific command being executed. The UI is discussed in the [UI/UX Design section](#ui/ux-design).

The UI itself will live in the TerminalApp project, whereas any logic populating the UI will live in the TerminalControl project. This allows for any consumer of the TerminalControl project to still have access to these features, but have the ability to represent them in whatever UI they please. To accomplish this, the TerminalControl layer must expose a `IVector<String> GetQuickFixContext(UInt32 Row)` which will retrieve all of the quick fix suggestions at the given `Row`. The returned suggestions will be consumed by the TerminalApp layer, where the `IVector<String>` will be converted to "sendInput" actions and displayed in the quick fix UI.

If and when the buffer resizes and reflows, the TerminalApp layer will be notified that the "contexts" are now invalidated. At that point, TerminalApp will call `GetQuickFixContext()` again to retrieve the updated information.

The actual suggestions will be populated from various sources such as the following:
- WinGet Command Not Found OSC Sequence:
	- `OSC 9001; CmdNotFound; <missingCmd>` will be used by the attached CLI app to notify the terminal that a command `missingCmd` wasn't found. The TermControl layer will use WinGet's API to search for any packages that would remediate this error and construct a list of suggested packages to be installed.
`OSC 9001` can be expanded on to add more suggestions via VT sequences from the underlying CLI app; for more on this, read the [Future considerations section](#future-considerations).
## UI/UX Design
A Quick Fix icon will be displayed immediately left of the prompt. If the icon fits in the left-gutter region, it will be drawn there. Otherwise, a cropped and transparent version will be displayed overlapping the text region; when the icon is hovered over, the icon will appear more clear by becoming more opaque and enlarging to become uncropped.

When the icon (exposed as a button) is invoked, the Quick Fix menu (exposed as a flyout menu) will be displayed. This menu will include the following:
- WinGet Command Not Found:
	- If a command wasn't found in the immediately previous output (between prompts), this entry will be enabled. It includes a nested menu of all the WinGet suggestions. If no suggestions were found or the previous output didn't have a missing command, this entry will be disabled.
- App Suggestions:
	- The underlying CLI app could specifically recommend a command using a custom VT Sequence. For more information, read the [Future considerations section](#future-considerations).
These suggestions will be displayed as "Run: \<command\>", which aligns with the design used in VS Code's Quick Fix menu.
## Accessibility
When a Quick Fix is available, an accessibility notification should be dispatched to alert the user that it is available. Aside from that, Quick Fix must be keyboard accessible via a default key binding. The UI will be a WinUI flyout menu, so it will be accessible by default once it is displayed and under focus.
## Future considerations
The Quick Fix menu can also be a home for future extensions to live. We can and should take inspiration from VS Code's implementation[^2][^3].

Other possible sources of suggestions in the quick fix menu include the following:
- App Suggestions:
	- `OSC 9001; SuggestInput; <cmd>; <description>` will be used by the attached CLI app to directly make suggestions to the terminal. The TermControl layer will stuff the `cmd` parameter into a new entry in `Suggestions`. An optional `description` can be provided to display in the UI for additional information.
- Similar to VS Code's Terminal[^1], when `git push` fails due to an upstream not being set, suggest to push with the upstream set.
- Similar to VS Code's Terminal[^1], when a `git` subcommand fails with a similar command error, suggest to use the similar command(s).
- Similar to VS Code's Terminal[^1], when `git push` results in a suggestion to create a GitHub PR, suggest to open the link.
As more sources of suggestions are added, `IVector<String> GetQuickFixContext(UInt32 Row)` will be modified to return `IVector<QuickFixSuggestion>` instead, where the `QuickFixSuggestion` will be able to include additional metadata like the optional description above, an icon, or anything we feel is necessary in the future.
## Resources
[^1]: [VS Code: Terminal Quick Fix Documentation](https://code.visualstudio.com/docs/terminal/shell-integration#_quick-fixes)
[^2]: [VS Code: Expose shell integration command knowledge to extensions](https://github.com/microsoft/vscode/issues/145234)
[^3]: [VS Code: Terminal quick fix API](https://github.com/microsoft/vscode/issues/162950)