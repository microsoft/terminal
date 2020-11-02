---
author: Mike Griese @zadjii-msft
created on: 2020-10-30
last updated: 2020-11-02
issue id: #4472
---

# Windows Terminal Session Management

## Abstract
This document is intended to serve as an addition to the [Windows Terminal
Process Model 2.0 Spec]. That document provides a big-picture overview of
changes to the entirety of the Windows Terminal process architecture, including
both the split of window/content processes, as well as the introduction of
monarch/peasant processes. The focus of that document was to identify solutions
to a set of scenarios that were closely intertwined, and establish these
solutions would work together, without preventing any one scenario from working.
What that docuement did not do was prescribe specific solutions to the given
scenarios.

This document offers a deeper dive on a subset of the issues in [#5000], to
describe specifics for managing multiple windows with the Windows Terminal. This
includes features such as:

* Run `wt` in the current window ([#4472])
* Single Instance Mode ([#2227])
* Quake Mode ([#653])

## Inspiration

[TODO]:  # TODO

## Background

[TODO]:  # TODO?

## Solution Design

### Monarch and Peasant Processes

This document assumes the reader is already familiar with the "Monarch and
Peasant" architecture as detailed in the [Windows Terminal Process Model 2.0
Spec]. As a quick summary:

* Every Windows Terminal window is a "Peasant" process.
* One of the Windows Terminal window processes is also the "Monarch" process.
  The Monarch is picked randomly from the Terminal windows, and there is only
  ever one Monarch process at a time.
* Peasants can communicate with the monarch when certain state changes (such as
  their window being activated), and the monarch can send commands to any of hte
  peasants.

This architecture will be used to enable each of the following scenarios.

### Scenario: Open new tabs in most recently used window

A common feature of many browsers is that when a web URL is clicked somewhere,
the web page is opened as a new tab in the most recently used window of the
browser. This functionality is often referred to as "glomming", as the new tab
"gloms" onto the existing window.

Currently, the terminal does not support such a feature - every `wt` invocation
creates a new window. With the monarch/peasant architecture, it'll now be
possible to enable such a scenario.

As each window is activated, it will call a method on the `Monarch` object
(hosted by the monarch process) which will indicate that "I am peasant N, and
I've been focused". The monarch will use those method calls to update its own
internal stack of the most recently used windows.

Whenever a new `wt.exe` process is launched, that process will _first_ ask the
monarch if it should run the commandline in an existing window, or create its
own window.

![auto-glom-wt-exe](auto-glom-wt-exe.png)

If glomming is disabled, then the Monarch will call back to the peasant and tell
it to run the provided commandline.

If glomming is enabled, the monarch will dispatch the
commandline to the appropriate window for them to handle instead. To the user,
it'll seem as if the tab just opened in the most recent window.

Users should certainly be able to specify if they want new instances to glom
onto the MRU window or not. You could imagine that currently, we default to the
hypothetical value `"glomToLastWindow": false`, meaning that each new wt gets
it's own new window.

#### Glomming within the same virtual desktop

When links are opened in the new Edge browser, they will only glom onto an
existing window if that window is open in the current virtual desktop. This
seems like a good idea of a feature for the Terminal to follow as well.

There must be some way for an application to determine which virtual desktop it
is open on. We could use that information to have the monarch track the last
active window _per-desktop_, and only glom when there's one on the current
desktop.

We could make the `glomToLastWindow` property even more powerful by accepting a
combined `bool`/enum value:
- `true` or `"always"`: always glom to the most recent window, regardless of
  desktop
- `"sameDesktop"`: Only glom if there's an existing window on this virtual
  desktop, otherwise create a new window
- `false` or `"never"`: Never glom, always create a new window.

### Handling the current working directory

Consider the following scenario: the user runs `wt -d .` in the address bar of
explorer, and the monarch determines that this new tab should be created in an
existing window. For clarity during this example, we will label the existing
window WT[1], and the second `wt.exe` process WT[2].

An example of this scenario is given in the following diagram:

![single-instance-mode-cwd](single-instance-mode-cwd.png)

In this scenario, we want the new tab to be spawned in the current working
directory of WT[2], not WT[1]. So when WT[1] is about to run the commands that
were passed to WT[2], WT[1] will need to:

* First, stash it's own CWD
* Change to the CWD of WT[2]
* Run the commands from WT[2]
* Then return to it's original CWD.

So, as a part of the interface that a peasant uses to communicate the startup
commandline to the monarch, we should also include the current working
directory.

### Scenario: Run `wt` in the current window

One often requested scenario is the ability to run a `wt.exe` commandline in the
current window, as opposed to always creating a new window. With the ability to
communicate between different window processes, one could imagine a logical
extension of this scenario being "run a `wt` commandline in _any_ given WT
window".

Since each window process will have it's own unique ID assigned to it by the
monarch, then running a command in a given window with ID `N` should be as easy
as something like:

```sh
wt.exe --session N new-tab ; split-pane
```

(or for shorthand, `wt -s N new-tab ; split-pane`).

> ❗ TODO for discussion: In the remainder of this spec, I'll continue to use the
> parameters `--session,-s session-id` as the parameter to identify the window
> that a command should be run in. These arguments could just as easily be
> `--window,-w window-id`, and mean the same thing. I leave it as an exercise
> for the reviewers to pick one naming that they like best.

More formally, we'll add the following parameter to the top-level `wt` command:

#### `--session,-s session-id`
Run these commands in the given Windows Terminal session. This enables opening
new tabs, splits, etc. in already running Windows Terminal windows.
* If `session-id` is `0`, run the given commands in _the  current window_
* If `session-id` is not the ID of any exsiting window, then run the commands in
  a _new_ Terminal window.
* If `session-id` is ommitted, then obey the value of `glomToLastWindow` when
  determining which window to run the command in.

_Whenever_ `wt.exe` is started, it must _always_ pass the provided commandline
first to the monarch process for handling. This is important for glomming
scenarios (as noted above). The monarch will parse the commandline, determine
which window the commandline is destined for, then call `ExecuteCommandline` on
that peasant, who will then run the command.

#### Running commands in the current window:`--session-id 0`

If `wt -s 0 <commands>` is run _outside_ a WT instance, it could attempt to glom
onto _the most recent WT window_ instead. This seems more logical than something
like `wt --session last` or some other special value indicating "run this in the
MRU window".

That might be a simple, but **wrong**, implementation for "the current window".
If the peasants always raise an event when their window is focused, and the
monarch keeps track of the MRU order for peasants, then one could naively assume
that the execution of `wt -s 0 <commands>` would always return the window the
user was typing in, the current one. However, if someone were to do something
like `sleep 10 ; wt -s 0 <commands>`, then the user could easily focus another
WT window during the sleep, which would cause the MRU window to not be the same
as the window executing the command.

I'm not sure that there is a better solution for the `-s 0` scenario other than
attempting to use the `WT_SESSION` environment variable. If a `wt.exe` process is
spawned and that's in it's environment variables, it could try and ask the
monarch for the peasant who's hosting the session corresponding to that GUID.
This is more of a theoretical solution than anything else.

In Single-Instance mode, running `wt -s 0` outside a WT window will still cause
the commandline to glom to the existing single terminal instance, if there is
one.

#### Running commands in a new window:`--session-id -1`

If the user passes an invalid ID to the `--session` parameter, then we'll always
create a new window for that commandline, regardless of the value of
`glomToLastWindow`. This will allow users to do something like `wt -s -1
new-tab` to _always_ create a new window. Since window IDs are only ever
positive integers, then `-1` would be a convenient value for something that's
_never_ an existing window ID.


#### `--session` in subcommands

Will we allow `wt -s 2 new-tab ; -s 3 split-pane`?



\<old text TODO>

#### Scenario: Single Instance Mode

"Single Instance Mode" is a scenario in which there is only ever one single WT
window. When Single Instance Mode is active, and the user runs a new `wt.exe`
commandline, it will always end up running in the existing window, if there is
one.

In "Single Instance Mode", the monarch _is_ the single instance. When `wt` is
run, and it determines that it is not the monarch, it'll pass the commandline to
the monarch. At this point, the monarch will determine that it is in Single
Instance Mode, and consume the commands itself.

One could imagine that single instance mode is the combination of two properties:
* The user wants new invocation of `wt` to "glom" onto the most recently used
  window (the aforementioned `"glomToLastWindow": true` setting)
* The user wants to prevent tab tear-out, or the ability for a `newWindow`
  action to work. This will be controlled by a proposed `"allowMultipleWindows"` setting, where:
  - `"allowMultipleWindows": false`: new windows are always created in the current window, and tab tear-out is disabled. This overrides any behavior from `glomToLastWindow`.
  - `"allowMultipleWindows": true`: `newWindow` actions will always create a new window, `-s ID`

[TODO]:  # TODO ################################################################
This is weird - why two settings? what value do we really get from that? Preventing people from tearing out tabs, but allowing `wt nt` to open in new windows?

Let's look at the old proposal, which had two properties:

* `"glomToLastWindow": true|false`
* `"allowTabTearOut": true|false`

Which gives us these cases:

* `"glomToLastWindow": true`, `"allowTabTearOut": true`:
  - New instances open in the current window by default.
  - `newWindow` opens a new window.
  - Tabs can be torn out to create new windows.
  - `wt -s -1` opens a new window.
* `"glomToLastWindow": true`, `"allowTabTearOut": false`:
  - New instances open in the current window by default.
  - `newWindow` opens in the existing window.
  - Tabs cannot be torn out to create new windows.
  - `wt -s -1` opens in the existing window.
* `"glomToLastWindow": false`, `"allowTabTearOut": true`:
  - New instances open in new windows by default
  - `newWindow` opens a new window
  - Tabs can be torn out to create new windows.
  - `wt -s -1` opens a new window.
* `"glomToLastWindow": false`, `"allowTabTearOut": false`:
  - New instances open in the current window by default. (unexpected?)
  - `newWindow` opens in the existing window.
  - Tabs cannot be torn out to create new windows.
  - `wt -s -1` opens in the existing window.

The fourth case is actually exactly the same as the second case. So that would
imply there's really only 3 values here, not a matrix of 2 options by 2 options.

Perhaps the  `glomToLastWindow` setting doesn't make exact sense? Maybe we need a few other values.

(for this mental experiment, `"glomToLastWindow"` and `"glomToLastWindowSameDesktop"` are being treated as one property)

What actual user stories do we have?


* Scenario 1: Browser-like glomming
  - <!-- Formerly, `"glomToLastWindow": true`, `"allowTabTearOut":
  true`:  -->
  - New instances open in the current window by default.
  - `newWindow` opens a new window.
  - Tabs can be torn out to create new windows.
  - `wt -s -1` opens a new window.
* Scenario 2: Single Instance Mode
  - <!-- Formerly, (`"glomToLastWindow": true`, `"allowTabTearOut":
    false`) and  (`"glomToLastWindow": false`, `"allowTabTearOut": false`)-->
  - New instances open in the current window by default.
  - `newWindow` opens in the existing window.
  - Tabs cannot be torn out to create new windows.
  - `wt -s -1` opens in the existing window.
* Scenario 3: No auto-glomming
  - <!-- Formerly, (`"glomToLastWindow": false`, `"allowTabTearOut":
  true`)-->
  - New instances open in new windows by default
  - `newWindow` opens a new window
  - Tabs can be torn out to create new windows.
  - `wt -s -1` opens a new window.

So this kinda reads as

`"glomTolastWindow": ("never"|false)|"sameDesktop"|("lastWindow"|true)|"always"`

[/TODO]:  # /TODO ##############################################################



While tear-out is a separate track of work from session management in general, this setting could be implemented along with this set of features, and later used to control tear out as well.

#### Scenario: Quake Mode

"Quake mode" has a variety of different scenarios<sup>[[1]](#footnote-1)</sup>
that have all been requested, more than what fits cleanly into this spec.
However, there's one key aspect of quake mode that is specifically relevant and
worth mentioning here.

One of the biggest challenges with registering a global shortcut handler is
_what happens when multiple windows try to register for the same shortcut at the
same time_? From my initial research, it seems that only the first process to
register for the shortcut will succeed. This makes it hard for multiple windows
to handle the global shortcut key gracefully. If a second window is created, and
it fails to register the global hotkey, then the first window is closed, there's
no way for the second process to track that and re-register as the handler for
that key.

With the addition of monarch/peasant processes, this problem becomes much easier
to solve. Now, the monarch process will _always_ be the process to register the
shortcut key, whenever it's elected. If it dies and another peasant is elected
monarch, then the new monarch will register as the global hotkey handler.

Then, the monarch can use it's pre-established channels of communication with
the other window processes to actually drive the response we're looking for.

**Alternatively**, we could use an entirely other process to be in charge of the
registration of the global keybinding. This process would be some sort of
long-running service that's started on boot. When it detects the global hotkey,
it could attempt to instantiate a `Monarch` object.

* If it can't make one, then it can simply run a new instance of `wt.exe`,
  because there's not yet a running Terminal window.
* Otherwise, it can communicate to the monarch that the global hotkey was
  pressed, and the monarch will take care of delegating the activation to the
  appropriate peasant window.

This would mitigate the need to have at least one copy of WT running already,
and the user could press that keybinding at any time to start the terminal.

\</old text /TODO >

## UI/UX Design

[TODO]:  # TODO

## Capabilities

<table>
<tr>
<td><strong>Accessibility</strong></td>
<td>

[TODO]:  # TODO

</td>
</tr>
<tr>
<td><strong>Security</strong></td>
<td>

[TODO]:  # TODO
</td>
</tr>
<tr>
<td><strong>Reliability</strong></td>
<td>

[TODO]:  # TODO

</td>
</tr>
<tr>
<td><strong>Compatibility</strong></td>
<td>

[TODO]:  # TODO

</td>
</tr>
<tr>
<td><strong>Performance, Power, and Efficiency</strong></td>
<td>

[TODO]:  # TODO

</td>
</tr>
</table>

## Potential Issues

[TODO]:  # TODO

## Implementation Plan

[TODO]:  # TODO? Or just leave this in the PM2.0 spec?

This is a list of actionable tasks generated as described by this spec:

* [ ] Add support for `wt.exe` processes to be Monarchs and Peasants, and communicate that state between themselves. This task does not otherwise add any user-facing features, merely an architectural update.
* [ ] Add support for the `glomToLastWindow` setting as a boolean. Opening new WT windows will conditionally glom to existing windows.
* [ ] Add support for per-desktop `glomToLastWindow`, by adding the support for the enum values `"always"`, `"sameDesktop"` and `"never"`.

## Footnotes

<a name="footnote-1"><a>[1]: TODO

## Future considerations


[TODO]:  # TODO

* During
  [review](https://github.com/microsoft/terminal/pull/7240#issuecomment-716022646),
  it was mentioned that when links are opened in the new Edge browser, they will
  only glom onto an existing window if that window is open in the current
  virtual desktop. This seems like a good idea of a feature for the Terminal to
  follow as well. Since Edge is able to do it, there must be some way for an
  application to determine which virtual desktop it is open on. We could use
  that information to have the monarch track the last active window per-desktop,
  and only glom when there's one on the current desktop.
  - We could even imagine changing the `glomToLastWindow` property to accept a
    combined `bool`/enum value:
    - `true` or `"always"`: always glom to the most recent window, regardless of
      desktop
    - `"sameDesktop"`: Only glom if there's an existing window on this virtual
      desktop, otherwise create a new window
    - `false` or `"never"`: Never glom, always create a new window.


## Resources

* [Tab Tear-out in the community toolkit] - this document proved invaluable to
  the background of tearing a tab out of an application to create a new window.

<!-- Footnotes -->

[#5000]: https://github.com/microsoft/terminal/issues/5000
[#1256]: https://github.com/microsoft/terminal/issues/1256
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#2227]: https://github.com/microsoft/terminal/issues/2227
[#653]: https://github.com/microsoft/terminal/issues/653
[#1032]: https://github.com/microsoft/terminal/issues/1032
[#632]: https://github.com/microsoft/terminal/issues/632
[#492]: https://github.com/microsoft/terminal/issues/492
[#4000]: https://github.com/microsoft/terminal/issues/4000
[#7972]: https://github.com/microsoft/terminal/pull/7972
[#961]: https://github.com/microsoft/terminal/issues/961
[`30b8335`]: https://github.com/microsoft/terminal/commit/30b833547928d6dcbf88d49df0dbd5b3f6a7c879

[Tab Tear-out in the community toolkit]: https://github.com/windows-toolkit/Sample-TabView-TearOff
[Quake mode scenarios]: https://github.com/microsoft/terminal/issues/653#issuecomment-661370107
[`ISwapChainPanelNative2::SetSwapChainHandle`]: https://docs.microsoft.com/en-us/windows/win32/api/windows.ui.xaml.media.dxinterop/nf-windows-ui-xaml-media-dxinterop-iswapchainpanelnative2-setswapchainhandle


[Windows Terminal Process Model 2.0 Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%235000%20-%20Process%20Model%202.0.md
