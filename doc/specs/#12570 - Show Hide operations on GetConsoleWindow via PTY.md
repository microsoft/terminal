---
author: Michael Niksa @miniksa
created on: 2022-02-24
last updated: 2022-02-24
issue id: 12570
---

# Show Hide operations on GetConsoleWindow via PTY

## Abstract

To maintain compatibility with command-line tools, utilities, and tests that desire to
manipulate the final presentation window of their output through retrieving the raw
console window handle and performing `user32` operations against it like `::ShowWindow()`,
we will create a compatibility layer that captures this intent and translates it into
the nearest equivalent in the cross-platform virtual terminal language and implement the
understanding of these sequences in our own Windows Terminal.

## Inspiration

When attempting to enable the Windows Terminal as the default terminal application on Windows
(to supersede the execution of command-line utilities inside the classic console host window),
we discovered that there were a bunch of automated tests, tools, and utilities that relied on
showing and hiding the console window using the `::GetConsoleWindow()` API in conjunction with
`::ShowWindow()`.

When we initially invented the ConPTY, we worked to ensure that we built to the common
denominator that would work cross-platform in all scenarios, avoiding situations that were
dependent on Windows-isms like `user32k` including the full knowledge of how windowing occurs
specific to the Windows platform.

We also understood that on Windows, the `::CreateProcess` API provides ample flags specifically
for command-line applications to command the need for (or lack thereof) a window on startup
such as `CREATE_NEW_CONSOLE`, `CREATE_NO_WINDOW`, and `DETACHED_PROCESS`. The understanding
was that people who didn't need or want a window, or otherwise needed to manipulate the
console session, would use those flags on process creation to dictate the session. Additionally,
the `::CreateProcess` call will accept information in `STARTUPINFO` or `STARTUPINFOEX` that
can dictate the placement, size, and visibility of a window... including some fields specific
to console sessions. We had accepted those as ways applications would specify their intent.

Those assumptions have proven incorrect. Because it was too easy to just `::CreateProcess` in
the default manner and then get access to the session after-the-fact and manipulate it with
APIs like `::GetConsoleWindow()`, tooling and tests organically grew to make use of this process.
Instead of requesting up front that they didn't need a window or the overhead of a console session,
they would create one anyway by default and then manipulate it afterward to hide it, move it off-
screen, or otherwise push it around. Overall, this is terrible for their performance and overall
reliability because they've obscured their intent by not asking for it upfront and impacted their
performance by having the entire subsystem spin up interactive work when they intend to not use it.
But Windows is the place for compatibility, so we must react and compensate for the existing
non-ideal situation.

We will implement a mechanism to compensate for these that attempts to capture the intent of the
requests from the calling applications against the ConPTY and translates them into the "universal"
Virtual Terminal language to the best of its ability to make the same effects as prior to the
change to the new PTY + Terminal platform.

## Solution Design

[comment]: # Outline the design of the solution. Feel free to include ASCII-art diagrams, etc.

## UI/UX Design

The visible change in behavior is that a call to `::ShowWindow()` against the `::GetConsoleWindow()`
handle that is returned by the ConPTY will be propagated to the attached Terminal. As such, a
user will see the entire window be shown or hidden if one of the underlying attached
command-line applications requests a show or hide.

At the initial moment, the fact that the Terminal contains tabbed and/or paned sessions and
therefore multiple command-line clients on "different sessions" are attached to the same window
is partially ignored. If one attached client calls "show", the entire window will be shown with
all tabs. If another calls "hide", the entire window will be hidden including the other tab
that just requested a show. In the opposite direction, when the window is shown, all attached
PTYs for all tabs/panes will be alerted that they're now shown at once.

## Capabilities

### Accessibility

Users of assistive devices will have the same experience that they did with the legacy Windows
Console after this change. If a command-line application decides to show or hide the window
through the API without their consent, they will receive notification of the showing/hiding
window through our UIA framework.

Prior to this change, the window would have always remained visible and there would be no
action.

Overall, the experience will be consistent between what is happening on-screen and what is
presented through the UIA framework to assistive tools.

For third party terminals, it will be up to them to decide what their reaction and experience is.

### Security

We will maintain the security and integrity of the Terminal application chosen for presentation
by not revealing its true window handle information to the client process through the existing
`::GetConsoleWindow()` API. Through our design for default terminal applications, the final
presentation terminal could be Windows Terminal or it could be any third-party terminal that
meets the same specifications for communication. Giving raw access to its `HWND` to a client
application could disrupt its security.

By maintaining a level of separation with this feature by generating a "fake window" in the
ConPTY layer and only forwarding events, the attached terminal (whether ours or a 3rd party)
maintains the final level of control on whether or not it processes the message. This is
improved security over the legacy console host where the tool had full backdoor style access
to all `user32` based window APIs.

### Reliability

This test doesn't improve overall reliability in the system because utilities that are relying
on the behavior that this compatibility shim will restore are already introducing additional
layers of complexity and additional processes into their operation than were strictly necessary
simply by not stating their desires upfront at creation time.

In some capacity, you could argue it increases reliability of the existing tests that were
using this complex behavior in that they didn't work before and they will work now, but
the entire process is fragile. We're just restoring the fragile process instead of having
it not work at all.

### Compatibility

This change restores compatibility with existing applications that were relying on the behavior
we had excluded from our initial designs.

### Performance, Power, and Efficiency

The performance of tooling that is leveraging this process to create a console and then hide
or manipulate the session after the fact will be significantly worse when we enable the
default Windows Terminal than it was with the old Windows Console. This is because the
Terminal is significantly heavier weight (with its modern technologies like WinUI) and
will take more time to start and more committed memory. Additionally, more processes
will be in use because there will be the `conhost.exe` doing the ConPTY translation
and then the `windowsterminal.exe` doing the presentation.

However, this particular feature doesn't do anything to make that better or worse.

The appropriate solution for any tooling, test, or scenario that has a need for
performance and efficiency is to use the flags to `::CreateProcess` in the first place
to specify that they did not need a console window session at all, or to direct its
placement and visibility as a part of the creation call. We are working with
Microsoft's test automation tooling (TAEF) as well as the Windows performance
fundamentals (FUN) team to ensure that the test automation supports creating sessions
without a console window and that our internal performance test suite uses those
specifications on creation so we have accurate performance testing of the operating
system.

## Potential Issues

### Multiple clients sharing the same window host

With the initial design, multiple clients sharing the same window host will effectively
share the window state. Two different tabs or panes with two different client applications
could fight over the show/hide state of the window. In the initial revision, this is
ignored because this feature is being driven by a narrow failure scenario in the test gates.
In the reported scenario, a singular application is default-launched into a singular tab
in a terminal window and then the application expects to be able to hide it after the creation.

In the future, we may have to implement a conflict resolution or a graphical variance to
compensate for multiple tabs.

### Other verbs against the console window handle

This scenario initially focuses on just the `::ShowWindow()` call against the window handle
from `::GetConsoleWindow()`. Other functions from `user32` against the `HWND` will not
necessarily be captured and forwarded to the attached terminal application. And even more
specifically, we're focusing only on the Show and Hide state. Other state modifications that
are subtle related to z-ordering, activation, maximizing, snapping, and so on are not considered.

## Future considerations

### Multiple clients

If the multiple clients problem becomes more widespread, we may need to change the graphical
behavior of the Windows Terminal window to only hide certain tabs or panes when a command
comes in instead of hiding the entire window (unless of course there is only one tab/pane).

We may also need to adjust that once consensus is reached among tabs/panes that it can then
and only then propagate up to the entire window.

We will decide on this after we receive feedback that it is a necessary scenario. Otherwise,
we will hold for now.

### Other verbs

If it turns out that we discover tests/scenarios that need maximizing, activation, or other
properties of the `::ShowWindow()` call to be propagated to maintain compatibility, we will
be able to carry those through on the same channel and command. Most of them have an existing
equivalent in `XTWINOPS`. Those that do not, we would want to probably avoid as they will not
be implemented in any other terminal. We would extend the protocol as an absolute last resort
and only after receiving feedback from the greater worldwide terminal community.

### Z-ordering

The channel we're establishing here to communicate information about the window and its
placement may be useful for the z-ordering issues we have in #2988. In those scenarios,
a console client application is attempting to launch and position a window on top of the
terminal, wherever it is. Further synchronizing the state of the new fake-window in the
ConPTY with the real window on the terminal side may enable those tools to function as
they expect.

This is another circumstance we didn't expect: having command-line applications create windows
with a need for complex layout and ordering. These sorts of behaviors cannot be translated
to a universal language and will not be available off the singular machine, so encouraged
alternative methods like command-line based UI. However, for single-box scenarios, this
behavior is engrained in some Windows tooling due to its ease of use.

## Resources

[Default Terminal spec](https://github.com/microsoft/terminal/pull/7414)
[Z-ordering issue](https://github.com/microsoft/terminal/issues/2988)
