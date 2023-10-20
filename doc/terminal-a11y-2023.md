# Windows Terminal Accessibility
## Abstract
The release of Windows Terminal has served as a way to reinvigorate the command-line ecosystem on Windows by providing a modern experience that is consistently updated. This experience has caused faster innovation in the Windows command-line ecosystem that can be seen across various areas like expanded virtual terminal sequence support and enhanced accessibility. Command-line apps can now leverage these innovations to create a better experience for the end-user.

Since accessibility is a very broad area, this document is intended to present recent innovations in the accessibility space for the command-line ecosystem. Furthermore, it will propose additional improvements that can be made alongside key stakeholders that could benefit from such changes.

## Windows Command-line Ecosystem
### First-party terminals
For many years, Console Host (Conhost) was the only first-party terminal on Windows. In 2019, Windows Terminal was released to the world as an open source first-party terminal. Windows Terminal was distributed through the Microsoft Store and received regular updates throughout the year, much more frequently than Conhost. In October 2022, Windows Terminal was enabled as the default terminal on Windows.

A significant amount of code is shared between Conhost and Windows Terminal to create the terminal area. To enable an accessible experience for this area, a shared UI Automation provider was introduced in 2019[^1], enabling accessibility tools to navigate and read contents from the terminal area. In 2020, Windows Terminal was updated to dispatch UIA events signaling when the cursor position, text output, or selection changed; this left the work of identifying what changed in the output to the attached screen reader application[^2]. In 2022, Windows Terminal was updated to dispatch UIA notifications with a payload of what text was written to the screen[^3]. 

### Internal Partners
There are many first-party command-line applications on Windows. The following are a few examples of those that are regularly updated:
- [**GitHub CLI**](https://cli.github.com/): a tool that can be used to query and interact with GitHub repos (open source)
- [**Winget**](https://github.com/microsoft/winget-cli): a tool to install applications and other packages
- [**PSReadLine**](https://github.com/PowerShell/PSReadLine): a PowerShell module that enhances the input line experience
- [**Windows Subsystem for Linux (WSL)**](https://learn.microsoft.com/en-us/windows/wsl/): a tool to manage and run GNU/Linux environments without a traditional virtual machine
- [**PowerShell**](https://github.com/PowerShell/PowerShell): a cross-platform command-line shell (open source)

Additionally, CMD is a Windows-specific command-line shell. Though it is no longer being updated, it is still widely used.

### External Command-line Applications
There are many third-party command-line applications on Windows that are regularly updated.
The following examples take over the entire viewport:
- [**Vim**](https://www.vim.org/): a modal-based text editor with an extensive plugin system
- **Emacs**: a shortcut-oriented text editor with an extensive plugin system
- [**Emacspeak**](https://emacspeak.sourceforge.net/): an Emacs subsystem that leverages Emacs' context-specific information to produce speech output
- **Midnight Commander**: a file manager with an extensive text user interface
- **Far Manager**: a file manager for Windows with an exclusive text user interface
- **tmux**: a terminal multiplexer
The following examples don't take over the entire viewport:
- [**Oh My Posh**](https://ohmyposh.dev/): a tool to customize shell prompts
- **git**: a tool for version control
The following examples operate as command-line shells:
- [**Bash**](https://www.gnu.org/software/bash/) is the default shell for most Linux distributions  
- [**Fish shell**](https://fishshell.com/) provides a rich shell experience with features like autosuggestion support and VGA colors
- [**Z shell**](https://zsh.sourceforge.io/) is an extended Bourne shell

## Accessibility tools and features
### Screen Readers
Windows is predominantly dominated by three major screen readers: Narrator, JAWS, and NVDA.
- **Narrator**: a free first-party screen reader that ships with Windows.
- **JAWS**:  a third-party screen reader that requires an annual license to use.
- **NVDA**: a free third-party screen reader that is open source.
It's important to note that most users generally use NVDA or JAWS[^4], so it's very important to consider the experience under both of those screen readers.

### Miscellaneous User Tools
Windows has many built-in features that enable a more accessible experiences. The following is a list of Windows accessibility settings and tools that may have an impact on apps:
- **Text size setting**: dictates the default size of text throughout all of Windows
- **Always show scrollbars setting**: forces the scrollbars to always be expanded and shown. Generally, this setting is automatically respected when using the Windows UI library.
- **Transparency effects setting**: allows transparency effects within the Windows shell
- **Animation effects setting**: allows animation effects
- **Text cursor indicator**: displays a colorful UI over the text cursor to make it stand out more and easier to find. Note, this is powered by UI Automation selection changed events because an empty selection is the cursor's position.
- **Magnifier**: zooms the monitor to provide a closer look at the current mouse and cursor position. Note, this is powered by UI Automation selection changed events because an empty selection is the cursor's position.
- **Color filters**: manipulates displayed colors to address color-blindness
- **High contrast themes**: ensures all displayed content has a high contrast ratio. Generally, this setting is automatically respected when using the Windows UI library. However, non-WinUI surfaces (like the terminal area) need to do additional work to respect this.
- **Voice access**: enables users to interact with their PC using their voice. This is powered by UI Automation to identify what controls can be interacted with and how.

### Verification Tools
[Accessibility Insights](https://accessibilityinsights.io/) is a modern tool that can be used to test and understand accessibility issues on Windows. It comes with a built-in color contrast analyzer, a UI Automation event listener, and a UI Automation tree explorer. Additionally, it can be used to explore and interact with different control patterns using the same API screen readers and other accessibility tools rely on. Accessibility Insights is also capable of running automated tests in CI and locally to detect common and simple issues.

## Proposed Accessibility Improvements

### Respect Text Size OS setting
Windows Terminal has a profile setting for the font size (`"font"."size": <integer>`) which is then used when instantiating new terminal sessions. There should be a way to make the terminal font size respect the text size setting in Windows found in the Settings App > Accessibility > Text Size.

A possible solution is to scale the session's text size at runtime based on the OS setting. Furthermore, the Windows Terminal's settings UI should warn the user when they are attempting to modify the font size profile setting.

This is tracked by [Windows accessibility "text size" parameter ignored by terminal · Issue #13600](https://github.com/microsoft/terminal/issues/13600)

### Respect High Contrast Mode
When high contrast mode is enabled, any WinUI surfaces are automatically changed to respect the newly desired contrast mode. However, the terminal area does not. In 2021, the Delta E algorithm was added to Windows Terminal as a tool to improve color contrast in the terminal area if desired[^5]. In 2022, it was exposed via an `adjustIndistinguishableColors` profile setting with a variety of possible configurations[^6].

There are several possible solutions to this issue, which all should be exposed to the user. Such solutions include the following:
- Enabling `adjustIndistinguishableColors` automatically when high contrast mode is enabled
	- This leverages the Delta E algorithm to force an adequate contrast ratio. The algorithm would need to be expanded to expose the threshold.
- Reducing the colors used to match the Windows Contrast Theme colors
	- Edge uses this heuristic and it has the added benefit that Windows Terminal is respecting the user's high contrast theme set in the OS.
- Implementing the `DECSTGLT` escape sequence which enables switching between different color modes (one of which is monochrome)
Additionally, we should automatically make the terminal fully opaque and ignore the acrylic or traditional transparency setting.

This is tracked by [[Epic] Improved High Contrast support · Issue #12999](https://github.com/microsoft/terminal/issues/12999)

### Enhanced UI Automation movement mechanisms
UI Automation has an `ITextRangeProvider` interface that can be used to represent a span of text. These text ranges can then be manipulated to explore the text area. Such manipulations include moving either (or both) endpoint(s) to encompass a text unit or move by unit. UI Automation supports the following text units:
- Character
- Format
- Word
- Line
- Paragraph
- Page
- Document
Windows Terminal and Conhost implement this `ITextRangeProvider` interface. However, this implementation doesn't support all of the available text units. Though this is standard across other implementations, this provides an opportunity for growth.

#### UI Automation: Movement by page
Movement by page is a relatively abstract concept in terminals. Arguably, the viewport could be leveraged to be considered a "page", which would provide users with a quick way to navigate the buffer.

This is tracked by [UIA: support movement by page · Issue #13756](https://github.com/microsoft/terminal/issues/13756)

#### UI Automation: Movement by format
The terminal area supports various text decorations including but not limited to underlined, bold, and italic text. These text attributes were exposed via the `ITextRangeProvider` interface in [PR #10366](https://github.com/microsoft/terminal/pull/10336). UI automation has support for navigating through contiguous spans adhering to a text attribute, which could be useful to expose to UIA clients.

This is tracked by [UIA Formatted Text Navigation · Issue #3567](https://github.com/microsoft/terminal/issues/3567).

#### Movement by prompt
[PR #12948](https://github.com/microsoft/terminal/pull/12948) added support for scrollbar marks in Windows Terminal. As a part of this, an `experimental.autoMarkPrompts` profile setting registers scrollbar marks with each prompt when enabled. This is a fantastic way to navigate through the scroll history quickly and easily.

Though UI automation doesn't have a text unit for prompts, we could define a "paragraph" as the space between two prompts.

This issue is tracked by [megathread: Scrollbar Marks · Issue #11000](https://github.com/microsoft/terminal/issues/11000).

### Mark Mode support for degenerate range
[PR #13053](https://github.com/microsoft/terminal/pull/13053) added support for mark mode in Windows Terminal. Mark mode allows users to create and modify selections by exclusively using the keyboard. However, screen reader users have reported it as a strange experience because it always has a cell of text selected; this results in the screen reader reading "x selected, y unselected" as opposed to the expected "x" when moving the cursor around.

Unfortunately, the changes required to fix this are very extensive because selections are stored as two inclusive terminal coordinates, which makes it impossible to represent an empty selection. 

This is tracked by [A11y: windows terminal emits selection/deselection events in mark mode when navigating with arrow keys · Issue #13447](https://github.com/microsoft/terminal/issues/13447).

### Search improvements
The search dialog can be used to perform a runtime query of the text buffer. However, in its current form, it does not count the total number of matches like other developer tools (i.e. Visual Studio Code). This work item tracks two things:
1. Support for counting the total number of matches
2. Displaying the number of results in the search box and announcing it via a UIA notification to the screen reader
Additional search features may fall into this including but not limited to:
- Highlight the search results in the scroll bar
- Successful searches should read the line where text was found (similar to VS Code experience)

It seems that UI Automation has no guidance for a consistent, good search experience on Windows. Noting that VS Code and Microsoft Word have two different search experiences, it may be valuable to create documentation and guidance for other developer tools on Windows to follow, thus helping to create a more consistent search experience.

This is tracked by [Search should display the number of results it finds · Issue #6319](https://github.com/microsoft/terminal/issues/6319) and [[JAWS] Search results read "results found" instead of something useful · Issue #14153](https://github.com/microsoft/terminal/issues/14153).

### Shell suggestions
Shells provide different forms of autocompletion. However, that autocompletion isn't necessarily an accessible experience. For example, CMD's standard autocompletion experience rewrites the "word". The screen reader rereads the new "word" properly, however, the user must cycle between the options until the desired one is shown. As another example,

PowerShell supports menu completion, where possible completions are displayed as a table of text and relevant help text is displayed. However, screen readers struggle with this because the entire menu is redrawn every time, making it harder to understand what exactly is "selected" (as the concept of selection in this instance is a shell-side concept represented by visual manipulation).

A possible solution is to introduce a new VT sequence to have the shell provide a payload. This payload can contain things like the suggested completion as well as associated help text. Such data can then be leveraged by Windows Terminal to create UI elements. Doing so leverages WinUI's accessible design. By designing a new VT sequence, other terminal emulators and CLI apps can opt-in to this functionality too.

This is tracked by [Enhance shell autocompletion with a cool new user interface and shell completion protocol · Issue #3121](https://github.com/microsoft/terminal/issues/3121).

### Scripts Panel
Common command-line experiences revolve around inputting a command into the shell and executing it. Expert command-line users can remember complex commands off the top of their heads. Windows Terminal could help users by storing complex commands. Furthermore, these commands should be able to be easily shared between users.

From an accessibility standpoint, this can be very useful for users with mobility issues, as it is particularly difficult to write complex commands quickly.

This is tracked by [Feature Request - Scripts Panel · Issue #1595](https://github.com/microsoft/terminal/issues/1595).

### Broadcast Input
Windows Terminal supports running multiple sessions at once across various panes, tabs, and windows. The broadcast input feature allows users to send input to multiple sessions at once.

From an accessibility standpoint, this can be very useful for users with mobility issues, as it is time consuming to write commands to multiple sessions quickly, particularly if those commands are complex.

This is tracked by [Support broadcast input? · Issue #2634](https://github.com/microsoft/terminal/issues/2634).

### UI Automation Notification Related Improvements
In 2022, Windows Terminal added UI Automation notifications that contained a payload of text output[^3]. This was done for various reasons. For one, Narrator was not reading new output by the terminal, and would require changes on their end to accomplish this. Another reason is that NVDA is unable to handle too many text changed events, which Conhost and Windows Terminal are both culprits of since they dispatch an event when text is written to the terminal output.

UIA notifications have provided many compatibility benefits since screen readers automatically read notifications they receive. Additionally, this has provided the possibility for major performance enhancements as screen readers may no longer be required to diff the text buffer and figure out what has changed. NVDA has prototyped listening to notifications and ignoring text changed events entirely[^7]. However, it reveals underlying challenges with this new model such as how to handle passwords. The proposals listed in this section are intended to have Windows Terminal achieve improved performance and accessibility quality.

#### VT Screen Reader Control
Some command-line applications are simply too difficult to create a consistent accessible experience. Applications that draw decorative content, for example, may have that content read by a screen reader. 

In 2019, Daniel Imms wrote a spec proposing a VT sequence that can partially control the attached screen reader[^8]. This VT sequence consists of three main formats:
1. Stop announcing incoming data to the screen reader. The screen reader will resume announcing incoming data if any key is pressed.
2. Resume announcing incoming data to the screen reader.
3. Announce the associated string payload immediately.
Additionally, all three formats include a string payload that will be announced immediately by the screen reader (as is done with the third format).

JAWS and Narrator both immediately read UIA notifications as that is how Windows Terminal presents newly output text. As described earlier, NVDA currently has notifications disabled, but is prototyping moving towards a world where they can drop text diffing entirely in favor of this.

With Windows Terminal now dispatching UIA notifications, it would be relatively trivial for Windows Terminal to extract the string payload from the relevant VT sequences and present it in a new UIA notification. Additionally, an internal flag would be enabled to suppress UIA notifications for text output until the flag is flipped via a key press or the relevant VT sequence is received.

This is tracked by [Unable to use applications that hook the arrow keys using Windows Console Host. · Issue #13666](https://github.com/microsoft/terminal/issues/13666)

#### Screen Reader Backpressure Control Proposal
NVDA has reported issues where Windows Terminal sends too may text changed events at once, causing NVDA to hang. Several considerations have been made in this area to address this issue:
- **Batch notifications**: unhelpful because Terminal doesn't know how much text is intended to be output
- **Diff text before sending a notification**: same problem as above
- **Provide an API to throttle notifications**: causes a security risk for denial of service attacks
Surprisingly, UI Automation doesn't have a built-in way to handle backpressure. However, the appropriate fix here seems to be either on the UI Automation side to fix this for all UIA clients, or on the NVDA side to improve event handling.

This is tracked by [UIA: reduce number of extraneous text change events in conhost · Issue #10822](https://github.com/microsoft/terminal/issues/10822)

## Proposed Priorities
The following table assigns priorities to the aforementioned work items. These priorities are solely based on accessibility impact to improve the Windows command-line ecosystem.

| Priority | Work Item | Reasoning |
|----------|-----------|-----------|
| 1 | VT Screen Reader Control | Several partners (both CLI apps and UIA clients) would benefit from this feature and have expressed interest in adopting it. |
| 1 | Shell suggestions | Several partners (both CLI apps and UIA clients) would benefit from this feature and have expressed interest in adopting it. |
| 1 | Search improvements | Search is very difficult to use in its current implementation. |
| 1 | Mark Mode support for degenerate range | Experience is barely usable and very unfriendly for non-sighted users. This is the only non-mouse method to select text. |
| 2 | Respect High Contrast Mode | Clear workarounds exist, but the golden path scenario fails where a user enables high contrast mode and content does not respect it. |
| 2 | Text Size OS Setting | Clear workarounds exist, but the golden path scenario fails where a user enables an OS setting and content does not respect it. |
| 2 | Broadcast Input | Users with mobility issues would greatly benefit from this. |
| 3 | Scripts Panel | Send-input commands currently exist. This would just expose it better. |
| 3 | UIA: Move to prompt | Additional UIA text unit support is good. |
| 3 | UIA: Move by page | Additional UIA text unit support is good. |
| 3 | UIA: Move by format | Additional UIA text unit support is good. |

Generally, the reasoning behind these priorities can be broken down as follows:
- Priority 1 bucket:
	- These work items provide access to features within the command-line ecosystem that are practically unusable for users with accessibility needs.
- Priority 2 bucket:
	- Active workarounds exist for these issues, but they could be cumbersome.
- Priority 3 bucket:
	- Purely innovative ideas that provide an exceptional experience for users.

## References
[^1]: [Accessibility: Set-up UIA Tree by carlos-zamora · Pull Request #1691](https://github.com/microsoft/terminal/pull/1691)
[^2]: [Fire UIA Events for Output and Cursor Movement by carlos-zamora · Pull Request #4826](https://github.com/microsoft/terminal/pull/4826)
[^3]: [Use UIA notifications for text output by carlos-zamora · Pull Request #12358](https://github.com/microsoft/terminal/pull/12358)
[^4]: [WebAIM: Screen Reader User Survey #8 Results](https://webaim.org/projects/screenreadersurvey8/#:~:text=NVDA%20is%20now%20the%20most%20commonly%20used%20screen,respondents%20use%20more%20than%20one%20desktop%2Flaptop%20screen%20reader.)
[^5]: [Implement the Delta E algorithm to improve color perception by PankajBhojwani · Pull Request #11095](https://github.com/microsoft/terminal/pull/11095)
[^6]: [Change AdjustIndistinguishableColors to an enum setting instead of a boolean setting by PankajBhojwani · Pull Request #13512](https://github.com/microsoft/terminal/pull/13512)
[^7]: [Prototype for Windows Terminal: Use notifications instead of monitoring for new text by leonardder · Pull Request #14047 · nvaccess/nvda (github.com)](https://github.com/nvaccess/nvda/pull/14047)
[^8]: [Control Screen Reader from Applications (#18) · Issues · terminal-wg / specifications · GitLab](https://gitlab.freedesktop.org/terminal-wg/specifications/-/issues/18)