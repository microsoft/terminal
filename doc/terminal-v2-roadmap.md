# Terminal 2.0 Roadmap

## Overview

This document outlines the roadmap towards delivering Windows Terminal 2.0 by Winter 2021.

## Milestones

Windows Terminal is engineered and delivered as a set of 6-week milestones. New features will go into [Windows Terminal Preview](https://aka.ms/terminal-preview) first, then a month after they've been in Preview, those features will move into [Windows Terminal](https://aka.ms/terminal).

| Duration | Activity | Releases |
| --- | --- | --- |
| 4 weeks | Dev Work<br/> <ul><li>Fixes / Features for future Windows Releases</li><li>Fixes / Features for Windows Terminal</li></ul> |  Release to Internal Selfhosters at end of week 4 |
| 1 week | Quality & Stability<br/> <ul><li>Bug Fixes</li><li>Perf & Stability</li><li>UI Polish</li><li>Tests</li><li>etc.</li></ul>| Push to Microsoft Store at end of week 5 |
| 1 week | Release <br/> <ul><li>Available from [Microsoft Store](https://aka.ms/terminal) & [GitHub Releases](https://github.com/microsoft/terminal/releases)</li><li>Release Notes & Announcement Blog published</li><li>Engineering System Maintenance</li><li>Community Engagement</li><li>Docs</li><li>Future Milestone Planning</li></ul> | Release available from Microsoft Store & GitHub Releases |

## Terminal Roadmap / Timeline

Below is the schedule for when milestones will be included in release builds of Windows Terminal and Windows Terminal Preview. The dates are rough estimates and are subject to change.

| Milestone End Date | Milestone Name | Preview Release Blog Post |
| ------------------ | -------------- | ------------------------- |
| 2020-06-18 | [1.1] in Windows Terminal Preview | [Windows Terminal Preview 1.1 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-1-release/) |
| 2020-07-31 | [1.2] in Windows Terminal Preview<br>[1.1] in Windows Terminal | [Windows Terminal Preview 1.2 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-2-release/) |
| 2020-08-31 | [1.3] in Windows Terminal Preview<br>[1.2] in Windows Terminal | [Windows Terminal Preview 1.3 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-3-release/) |
| 2020-09-30 | [1.4] in Windows Terminal Preview<br>[1.3] in Windows Terminal | [Windows Terminal Preview 1.4 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-4-release/) |
| 2020-11-30 | [1.5] in Windows Terminal Preview<br>[1.4] in Windows Terminal | [Windows Terminal Preview 1.5 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-5-release/) |
| 2021-01-31 | [1.6] in Windows Terminal Preview<br>[1.5] in Windows Terminal | [Windows Terminal Preview 1.6 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-6-release/) |
| 2021-03-01 | [1.7] in Windows Terminal Preview<br>[1.6] in Windows Terminal | [Windows Terminal Preview 1.7 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-7-release/) |
| 2021-04-14 | [1.8] in Windows Terminal Preview<br>[1.7] in Windows Terminal | [Windows Terminal Preview 1.8 Release](https://devblogs.microsoft.com/commandline/windows-terminal-preview-1-8-release/) |
| 2021-05-31 | [1.9] in Windows Terminal Preview<br>[1.8] in Windows Terminal | |
| 2021-07-31 | 1.10 in Windows Terminal Preview<br>[1.9] in Windows Terminal | |
| 2021-08-30 | 1.11 in Windows Terminal Preview<br>1.10 in Windows Terminal | |
| 2021-10-31 | 1.12 in Windows Terminal Preview<br>1.11 in Windows Terminal | |
| 2021-11-30 | 2.0 RC in Windows Terminal Preview<br>2.0 RC in Windows Terminal | |
| 2021-12-31 | [2.0] in Windows Terminal Preview<br>[2.0] in Windows Terminal | |

## Issue Triage & Prioritization

Incoming issues/asks/etc. are triaged several times a week, labeled appropriately, and assigned to a milestone in priority order:

* P0 (serious crashes, data loss, etc.) issues are scheduled to be dealt with ASAP
* P1/2 issues/features/asks  assigned to the current or future milestone, or to the [Terminal 2.0 milestone](https://github.com/microsoft/terminal/milestone/22) for future assignment, if required to deliver a 2.0 feature
* Issues/features/asks not on our list of 2.0 features are assigned to the [Terminal Backlog](https://github.com/microsoft/terminal/milestone/7) for subsequent triage, prioritization & scheduling.

## 2.0 Scenarios

The following are a list of the key scenarios we're aiming to deliver for Terminal 2.0.

> 👉 Note: There are many other features that don't fit within 2.0, but will be re-assessed and prioritized for 3.0, the plan for which will be published in 2021.

| Priority\* | Scenario | Description/Notes |
| ---------- | -------- | ----------------- |
| 0 | Settings UI | A user interface that connects to settings.json. This provides a way for people to edit their settings without having to edit a JSON file.<br><br>Issue: [#1564]<br>Specs: [#6720], [#6904]<br>Implementation: [#7283], [#7370], [#8048] |
| 0 | Command palette | A popup menu to list possible actions and commands.<br><br>Issues: [#5400], [#2046]<br>Spec: [#2193]<br>Implementation: [#6635] |
| 1 | Tab tear-off | The ability to tear a tab out of the current window and spawn a new window or attach it to a separate window.<br><br>Issue: [#1256], [#5000]<br>Spec: [#2080], [#7240] |
| 1 | Clickable links | Hyperlinking any links that appear in the text buffer. When clicking on the link, the link will open in your default browser.<br><br>Issue: [#574]<br>Implementation: [#7251] |
| 1 | Default terminal | If a command-line application is spawned, it should open in Windows Terminal (if installed) or your preferred terminal<br><br>Issue: [#492]<br>Spec: [#2080], [#7414] |
| 1 | Overall theme support | Tab coloring, title bar coloring, pane border coloring, pane border width, definition of what makes a theme<br><br>Issue: [#3327]<br>Spec: [#5772] |
| 1 | Open profile elevated | Configure profiles to always open elevated (if Terminal was run unelevated)<br><br>Issue: [#5000], [#632]<br>Spec: [#8455] |
| 1 | Open tab in existing window | Open new tabs in existing Terminal windows<br><br>Issue: [#5000], [#4472]<br>Spec: [#8135] |
| 1 | Traditional opacity | Have a transparent background without the acrylic blur.<br><br>Issue: [#603] <br>**Current State**: Blocked on WinUI 3.0 |
| 2 | SnapOnOutput, scroll lock | Pause output or scrolling on click.<br><br>Issue: [#980]<br>Spec: [#2529]<br>Implementation: [#6062] |
| 2 | Infinite scrollback | Have an infinite history for the text buffer.<br><br>Issue: [#1410] |
| 2 | Pane management | All issues listed out in the original issue. Some features include pane resizing with mouse, pane zooming, and opening a pane by prompting which profile to use.<br><br>Issue: [#1000] |
| 2 | Theme marketplace | Marketplace for creation and distribution of themes.<br>Dependent on overall theming |
| 2 | Jump list | Show profiles from task bar (on right click)/start menu.<br><br>Issue: [#576]<br>Implementation: [#7515] |
| 2 | Open with multiple tabs | A setting that allows Windows Terminal to launch with a specific tab configuration (not using only command line arguments).<br><br>Issue: [#756] |
| 3 | Open in Windows Terminal | Functionality to right click on a file or folder and select Open in Windows Terminal.<br><br>Issue: [#1060]<br>Implementation: [#6100] |
| 3 | Session restoration | Launch Windows Terminal and the previous session is restored with the proper tab and pane configuration and starting directories.<br><br>Issues: [#961], [#960], [#766] |
| 3 | Quake mode | Provide a quick launch terminal that appears and disappears when a hotkey is pressed.<br><br>Issue: [#653] |
| 3 | Settings migration infrastructure | Migrate people's settings without breaking them. Hand-in-hand with settings UI. |
| 3 | Pointer bindings | Provide settings that can be bound to the mouse.<br><br>Issue: [#1553] |

Feature Notes:

\* Feature Priorities:

0. Mandatory <br/>
1. Optimal <br/>
2. Optional / Stretch-goal <br/>

[1.1]: https://github.com/microsoft/terminal/milestone/24
[1.2]: https://github.com/microsoft/terminal/milestone/25
[1.3]: https://github.com/microsoft/terminal/milestone/26
[1.4]: https://github.com/microsoft/terminal/milestone/28
[1.5]: https://github.com/microsoft/terminal/milestone/30
[1.6]: https://github.com/microsoft/terminal/milestone/31
[1.7]: https://github.com/microsoft/terminal/milestone/32
[1.8]: https://github.com/microsoft/terminal/milestone/33
[1.9]: https://github.com/microsoft/terminal/milestone/34
[2.0]: https://github.com/microsoft/terminal/milestone/22
[#1564]: https://github.com/microsoft/terminal/issues/1564
[#6720]: https://github.com/microsoft/terminal/pull/6720
[#6904]: https://github.com/microsoft/terminal/pull/6904
[#7283]: https://github.com/microsoft/terminal/pull/7283
[#7370]: https://github.com/microsoft/terminal/pull/7370
[#5400]: https://github.com/microsoft/terminal/issues/5400
[#2046]: https://github.com/microsoft/terminal/issues/2046
[#2193]: https://github.com/microsoft/terminal/pull/2193
[#6635]: https://github.com/microsoft/terminal/pull/6635
[#1256]: https://github.com/microsoft/terminal/issues/1256
[#2080]: https://github.com/microsoft/terminal/pull/2080
[#574]: https://github.com/microsoft/terminal/issues/574
[#7251]: https://github.com/microsoft/terminal/pull/7251
[#492]: https://github.com/microsoft/terminal/issues/492
[#2080]: https://github.com/microsoft/terminal/pull/2080
[#7414]: https://github.com/microsoft/terminal/pull/7414
[#3327]: https://github.com/microsoft/terminal/issues/3327
[#5772]: https://github.com/microsoft/terminal/pull/5772
[#5000]: https://github.com/microsoft/terminal/issues/5000
[#603]: https://github.com/microsoft/terminal/issues/603
[#980]: https://github.com/microsoft/terminal/issues/980
[#2529]: https://github.com/microsoft/terminal/pull/2529
[#6062]: https://github.com/microsoft/terminal/pull/6062
[#1410]: https://github.com/microsoft/terminal/issues/1410
[#1000]: https://github.com/microsoft/terminal/issues/1000
[#576]: https://github.com/microsoft/terminal/issues/576
[#7515]: https://github.com/microsoft/terminal/pull/7515
[#756]: https://github.com/microsoft/terminal/issues/756
[#1060]: https://github.com/microsoft/terminal/issues/1060
[#6100]: https://github.com/microsoft/terminal/pull/6100
[#961]: https://github.com/microsoft/terminal/issues/961
[#960]: https://github.com/microsoft/terminal/issues/960
[#766]: https://github.com/microsoft/terminal/issues/766
[#653]: https://github.com/microsoft/terminal/issues/653
[#1553]: https://github.com/microsoft/terminal/issues/1553
[#7240]: https://github.com/microsoft/terminal/pull/7240
[#8135]: https://github.com/microsoft/terminal/pull/8135
[#8455]: https://github.com/microsoft/terminal/pull/8455
[#632]: https://github.com/microsoft/terminal/issues/632
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#8048]: https://github.com/microsoft/terminal/pull/8048
