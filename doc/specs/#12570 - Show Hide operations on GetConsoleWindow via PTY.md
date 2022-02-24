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
console window handle and performing `user32` operations against it like [ShowWindow](https://docs.microsoft.com//windows/win32/api/winuser/nf-winuser-showwindow),
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

We also understood that on Windows, the [**CreateProcess**](https://docs.microsoft.com/windows/win32/procthread/process-creation-flags) API provides ample flags specifically
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

Overall, there are three processes involved in this situation:

1. The client command-line application utility, tool, or test that will manipulate the window.
1. The console host (`conhost.exe` or `openconsole.exe`) operating in PTY mode.
1. The terminal (`windowsterminal.exe` when it's Windows Terminal, but could be a third party).

The following diagram shows the components and how they will interact.

```txt
   ┌─────────────────┐                   ┌──────────────────┐                   ┌──────────────────────┐
   │                 │       1           │                  │                   │                      │
   │   Command-Line  ├─────────────────► │   Console Host   │                   │   Windows Terminal   │
   │   Tool or       │                   │   as ConPTY      │                   │   Backend            │
   │   Utility       │       2           │                  │        6          │                      │
   │                 │ ◄─────────────────┤                  ├─────────────────► │                      │
   │                 │                   │                  │                   │                      │
   │                 │                   │                  │                   │                      │
   │                 │                   │                  │         9         │                      │
   │                 │                   │                  │ ◄─────────────────┤                      │
   │                 │                   │                  │                   │                      │
   └─────┬───────────┘                   └───────────┬──────┘                   └─────────────────┬────┘
         │                                        ▲  │                                   ▲        │
         │                                        │  │                                   │        │
         │                                        │  │10                                 │        │7
         │3                                      5│  │                                   │8       │
         │                                        │  ▼                                   │        ▼
         │                                    ┌───┴────┐                              ┌──┴────┬───────┬─────────────────────────┐
         ▼                                    │ Hidden │                              │       │       │                      v^x│
   ┌─────────────────┐                        │ Fake   │                              ├───────┴───────┴─────────────────────────┤
   │                 │          4             │ PTY    │                              │                                         │
   │                 ├──────────────────────► │ Window │                              │                                         │
   │   user32.dll    │                        └────────┘                              │      Windows Terminal                   │
   │   Window APIs   │                                                                │      Displayed Window                   │
   │                 │                                                                │                                         │
   │                 │                                                                │                                         │
   └─────────────────┘                                                                │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      │                                         │
                                                                                      └─────────────────────────────────────────┘
```

1. The command-line tool calls `::GetConsoleWindow()` on the PTY host
2. The PTY host returns the raw `HWND` to the *Hidden Fake PTY Window* in its control
3. The command-line tool calls `::ShowWindow()` on the `user32.dll` API surface to manipulate that window.
4. `user32.dll` sends a message to the window message queue on the *Fake PTY Window*
5. The PTY host retrieves the message from the queue and translates it to a virtual terminal message
6. The Windows Terminal connection layer receives the virtual terminal message and decodes it into a window operation.
7. The true displayed *Windows Terminal Window* is told to change its status to show or hide.
8. The changed Show/Hide status is returned to the back-end on completion.
9. The Windows Terminal connection layer returns that information to the PTY host so it can remain in-the-know.
10. The PTY updates its *Fake PTY Window* status to match the real one so it continues to receive appropriate messages from `user32`.

This can be conceptually understood in a few phases:

- The client application grabs a handle and attempts to send a command via a back-channel through user32.
- User32 decides what message to send based on the window state of the handle.
- The message is translated by the PTY and propagated to the true visible window.
- The visible window state is returned back to the hidden/fake window to remain in synchronization so the next call to user32 can make the correct decision.

The communication between the PTY and the hosting terminal application occurs with a virtual terminal sequence.
Fortunately, *xterm* had already invented and implemented one for this behavior called **XTWINOPS** which means
we should be able to utilize that one and not worry about inventing our own Microsoft-specific thing. This ensures
that there is some precedence for what we're doing, guarantees a major third party terminal can support the same
sequence, and induces a high probability of other terminals already using it given *xterm* is the defacto standard
for terminal emulation.

Information about **XTWINOPS** can be found at [Xterm control sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html). Search for *XTWINOPS*.
The sequence is **CSI** *Ps*; *Ps*; *Ps* **t**. It starts with the common "control sequence initiator" of `ESC [` (`0x1B 0x5B`).
Then between 1 and 3 numerical parameters are given, separated by semicolons (`0x3B`).
And finally, the sequence is terminated with `t` (`0x74`).

Specifically, the two parameter commands of `1` for *De-iconify window* and `2` for *Iconify window* appear relevant to our interests.
In `user32` parlance, "iconify" traditionally corresponds to minimize/restore state and is a good proxy for overall visibility of the window.

The theory then is to detect when the assorted calls to `::ShowWindow()` against the *Fake PTY Window* are asking for a command that
maps to either "iconify" or "deiconify" and translate them into the corresponding message over the VT channel to the attached terminal.

To detect this, we need to use some heuristics inside the [window procedure](https://docs.microsoft.com/windows/win32/winmsg/window-procedures) for the window owned by the PTY.
Unfortunately, calls to `::ShowWindow()` on research with the team that owns `user32` do not go straight into the window message queue. Instead, they're dispatched straight into `win32k` to be analyzed and then trigger an array of follow on window messages into the queue depending on the `HWND`'s current state. Most specifically, they vary based on the `WS_VISIBLE` state of the `HWND`. (See [Window Styles](https://docs.microsoft.com/windows/win32/winmsg/window-styles) for details on the `WS_VISIBLE` flag.)

I evaluated a handful of messages with the help of the IXP Essentials team to see which ones would telegraph the changes from `::ShowWindow()` into our window procedure:

- [WM_QUERYOPEN](https://docs.microsoft.com/windows/win32/winmsg/wm-queryopen) - This one allows us to accept/reject a minimize/restore call. Not really useful for finding out current state
- [WM_SYSCOMMAND](https://docs.microsoft.com/windows/win32/menurc/wm-syscommand) - This one is what is called when the minimize, maximize/restore, and exit buttons are called in the window toolbar. But apparently it is not generated for these requests coming from outside the window itself through the `user32` APIs.
- [WM_SHOWWINDOW](https://docs.microsoft.com/windows/win32/winmsg/wm-showwindow) - This one provides some insight in certain transitions, specifically around force hiding and showing. When the `lParam` is `0`, we're supposed to know that someone explicitly called `::ShowWindow()` to show or hide with the `wParam` being a `BOOL` where `TRUE` is "show" and `FALSE` is "hide". We can translate that into *de-iconify* and *iconify* respectively.
- [WM_WINDOWPOSCHANGING](https://docs.microsoft.com/windows/win32/winmsg/wm-windowposchanging) - This one I evaluated extensively as it looked to provide us insight into how the window was about to change before it did so and offered us the opportunity to veto some of those changes (for instance, if we wanted to remain invisible while propagating a "show" message). I'll detail more about this one in a sub-heading below.
- [WM_SIZE](https://docs.microsoft.com/windows/win32/winmsg/wm-size) - This one has a `wParam` that specifically sends `SIZE_MINIMIZED` (`1`) and `SIZE_RESTORED` (`0`) that should translate into *iconify* and *de-iconify respectively.

#### WM_WINDOWPOSCHANGING data

In investigating `WM_WINDOWPOSCHANGING`, I built a table of some of the states I observed while receiving messages from an external caller that was using `::ShowWindow()`:

|integer|constant|flags|Should Hide?|minimizing|maximizing|showing|hiding|activating|`0x8000`|`SWP_NOCOPYBITS`|`SWP_SHOWWINDOW`|`SWP_FRAMECHANGED`|`SWP_NOACTIVATE`|`SWP_NOZORDER`|`SWP_NOMOVE`|`SWP_NOSIZE`|
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|0|`SW_HIDE`|?|YES|?|?|?|?|?|?|?|?|?|?|?|?|?|
|1|`SW_NORMAL`|`0x43`|NO|F|F|T|F|T|||X||||X|X|
|2|`SW_SHOWMINIMIZED`|`0x8160`|YES|T|F|T|F|T|X|X|X|X|||||
|3|`SW_SHOWMAXIMIZED`|`0x8160`|NO|F|T|T|F|T|X|X|X|X|||||
|4|`SW_SHOWNOACTIVATE`|`0x8070`|NO|F|F|T|F|F|X||X|X|X||||
|5|`SW_SHOW`|`0x43`|NO|F|F|T|F|T|||X||||X|X|
|6|`SW_MINIMIZE`|`0x8170`|YES|T|F|T|F|F|X|X|X|X|X||||
|7|`SW_SHOWMINNOACTIVE`|`0x57`|YES|T|F|T|F|F|||X||X|X|X|X|
|8|`SW_SHOWNA`|`0x53`|NO|F|F|T|F|F|||X||X||X|X|
|9|`SW_RESTORE`|`0x8160`|NO|F|F|T|F|T|||X|X|||||
|10|`SW_SHOWDEFAULT`|`0x43`|NO|F|F|T|F|T|||X||||X|X|
|11|`SW_FORCEMINIMIZE`|?|YES|?|?|?|?|?|?|?|?|?|?|?|?|?|

The headings are as follows:

- integer - The value of the Show Window constant `SW_*` (see [ShowWindow](https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-showwindow))
- constant - The name of the Show Window constant
- flags - The `lParam` field is a pointer to a [**WINDOWPOS**](https://docs.microsoft.com/windows/win32/api/winuser/ns-winuser-windowpos) structure during this message. This the `UINT flags` field of that structure.
- Should Hide? - Whether or not I believe that the window should hide if this constant is seen. (Conversely, should show on the opposite.)
- minimizing - This is the `BOOL` response from a call to [**IsIconic()**](https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-isiconic) during this message.
- maximizing - This is the `BOOL` response from a call to [**IsZoomed()**](https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-iszoomed) during this message.
- showing - This is whether `SWP_SHOWWINDOW` is set on the `WINDOWPOS.flags` field during this message.
- hiding - This is whether `SWP_HIDEWINDOW` is set on the `WINDOWPOS.flags` field during this message.
- activating - This is the inverse of whether `SWP_NOACTIVATE` is set on the `WINDOWPOS.flags` field during this message.
- Remaining headings are `flags` values expanded to `X` is set and blank is unset. See [**SetWindowPos()**](https://docs.microsoft.com/windows/win32/api/winuser/nf-winuser-setwindowpos) for the definitions of all the flags.

From this data collection, I noticed a few things:

- The data in this table was unstable. The fields varied depending on the order in which I called the various constants against `ShowWindow()`. This is just one particular capture.
- Some of the states, I wouldn't see any message data at all (`SW_HIDE` and `SW_FORCEMINIMIZE`).
- There didn't seem to be a definitive way to use this data to reliably decide when to show or hide the window. I didn't have a reliable way of pulling this together with my *Should Hide?* column.

On further investigation, it became apparent that the values received were sometimes not coming through or varying because the `WS_VISIBLE` state of the `HWND` affected how `win32k` decided to dispatch messages and what values they contained. This is where I determined that steps #8-10 in the diagram above were going to be necessary: to report the state of the real window back to the *fake window* so it could report status to `user32` and `win32k` and receive state-appropriate messages.

For reporting back #8-10, I initially was going to use the `XTWINOPS` call with parameter `11`. The PTY could ask the attached terminal for its state and expect to hear back an answer of either `1` or `2` in the same format message depending on the state. However, on further consideration, I realized that the real window could change at a moments notice without prompting from the PTY, so I instead wrote the PTY to always listen for this and had the Windows Terminal send this back down unprompted.

#### Refined WM_SHOWWINDOW and WM_SIZE data

Upon setting up the synchronization for #8-10, I then tried again to build the table using just the two window messages that were giving me reliable data: `WM_SHOWWINDOW` and `WM_SIZE`:

|integer|constant|Should Hide?|`WM_SHOWWINDOW` OR `WM_SIZE` reported hide?|
|---|---|---|---|
|0|`SW_HIDE`|YES|YES|
|1|`SW_NORMAL`|NO|NO|
|2|`SW_SHOWMINIMIZED`|YES|YES|
|3|`SW_SHOWMAXIMIZED`|NO|NO|
|4|`SW_SHOWNOACTIVATE`|NO|NO|
|5|`SW_SHOW`|NO|NO|
|6|`SW_MINIMIZE`|YES|YES|
|7|`SW_SHOWMINNOACTIVE`|YES|YES|
|8|`SW_SHOWNA`|NO|NO|
|9|`SW_RESTORE`|NO|NO|
|10|`SW_SHOWDEFAULT`|NO|NO|
|11|`SW_FORCEMINIMIZE`|YES|YES|

Since this now matched up perfectly with what I was suspecting should happen *and* it was easier to implement than picking apart the `WM_WINDOWPOSCHANGING` message, it is what I believe the design should be.

Finally, with the *fake window* changing state to and from `WS_VISIBLE`... it was appearing on the screen and showing up in the taskbar and alt-tab. To resolve this, I utilized [**DWMWA_CLOAK**](https://docs.microsoft.com//windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute) which makes the window completely invisible even when in a normally `WS_VISIBLE` state. I then added the [**WS_EX_TOOLWINDOW**](https://docs.microsoft.com/windows/win32/winmsg/extended-window-styles) extended window style to hide it from alt-tab and taskbar.

With this setup, the PTY now has a completely invisible window with a synchronized `WS_VISIBLE` state with the real terminal window, a bidirectional signal channel to adjust the state between the terminal and PTY, and the ability to catch `user32` calls being made against the *fake window* that the PTY stands up for the client command-line application.

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

- [Default Terminal spec](https://github.com/microsoft/terminal/pull/7414)
- [Z-ordering issue](https://github.com/microsoft/terminal/issues/2988)
- See all the embedded links in this document to Windows API resources
