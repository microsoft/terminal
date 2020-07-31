---
author: Mike Griese @zadjii-msft
created on: 2020-07-31
last updated: 2020-07-31
issue id: #5000
---

# Windows Terminal Process Model 2.0

## Abstract

The Windows Terminal currently exists as a single process per window, with one
connection per terminal pane (which could be an additional conpty process and
associated client processes). This model has proven effective for the simple
windowing we've done so far. However, in order to support scenarios like
dragging tabs into other windows, or having one top-level window with different
elevation levels within it, this single process model will not be sufficient.

This spec outlines changes to the Terminal process model in order to enable the
following scenarios:

* Tab Tearoff/ Reattach ([#1256])
* Run `wt` in the current window ([#4472])
* Single Instance Mode ([#2227])
* Quake Mode ([#653])
* Mixed Elevation ([#1032] & [#632])

## Inspiration

Much of the design for this feature was inspired by (what I believe is) the web
browser process model. For a web browser, there's often a seperate process for
each of the tabs within the browser, to help isolate the actual tabs from one
another.

## Solution Design

The primary concept introduced by this spec is the idea of two types of process,
which will work together to create a single Terminal window. These processes
will be referred to as the "Window Process" and the "Content Process".
* A **Window Process** is a process which is responsible for drawing a window to
  the desktop, and accepting input from the user. This is a window which hosts
  our XAML content, and the window which the user interacts with.
* A **Content Process** is a process which hosts a single terminal instance.
  This is the process that hosts the terminal buffer, state machine, connection,
  and is also responsible for the `Renderer` and `DxEngine`.

These two types of processes will work together to present both the UI of the
Windows Terminal app, as well as the contents of the terminal buffers. A single
WP may be in communication with multiple CPs - one per terminal instance. That
means that each and every `TermControl` in a WP will be hosted in a seperate
process.

The WP will be full of "thin" `TermControl`s - controls which are only the XAML
layer and a WinRT object which is hosted by the CP. These thin `TermControl`s
will recieve input, and have all the UI elements (including the
`SwapChainPanel`) of the `TermControl` today, but the actual _rendering_ to the
swap chain, and the handling of those inputs will be done by the content
process.

As a broad outline, whenever the window wants to create a terminal, the flow
will be something like the following:

1. A WP will spawn a new CP, with a unique ID.
2. The WP will attach itself to the CP, indicating that it (the WP) is the CP's
   hosting window.
3. When the CP creates it's swap chain, it will raise an event which the WP will
   use to connect that swap chain to the WP's `SwapChainPanel`.

### Tab Tearoff/ Reattach

### Single Instance Mode

Monarch/Servant architecture

### Run `wt` in the current window

We should reserve the session id `0` to always refer to "The current window", if
there is one. So `wt -s 0 new-tab` will run `new-tab` in the current window (if
we're being run from WT), otherwise it will create a new window.

In Single-Instance mode, running `wt -s 0` outside a WT window will still cause
the commandline to glom to the existing single terminal instance, if there is
one.

### Quake Mode
### Mixed Elevation


## UI/UX Design


## Capabilities

<table>
<tr>
<td><strong>Accessibility</strong></td>
<td>
TODO: This is _very_ applicable
</td>
</tr>
<tr>
<td><strong>Security</strong></td>
<td>
TODO: This is _very_ applicable
</td>
</tr>
<tr>
<td><strong>Reliability</strong></td>
<td>
TODO: This is _very_ applicable
</td>
</tr>
<tr>
<td><strong>Compatibility</strong></td>
<td>
TODO: This is _very_ applicable
</td>
</tr>
<tr>
<td><strong>Performance, Power, and Efficiency</strong></td>
<td>
TODO: This is _very_ applicable
</td>
</tr>
</table>

## Potential Issues

TODO: These are _very_ expected

## Footnotes

<a id="footnote-1"><a>[1]:

## Future considerations

* Yea add these



## TODOs

* Prove that a elevated window can host an unelevated content
* Prove that I can toss CP IDs from one window to another
* come up with a commandline mechanism for starting one `wt.exe` process either
  as a window process, or as a content process
* What about handling XAML input events? The "thin" term control will need to do
  that in-proc, so it can reply on the UI thread if a particular keystroke was
  handled or not.
  - I'm almost certain that the `Terminal::HandleKey()` stuff is going to need
    to be handled in the thin control, and the core will need to raise events to
    the control? oof

<!-- Footnotes -->

[#5000]: https://github.com/microsoft/terminal/issues/5000
[#1256]: https://github.com/microsoft/terminal/issues/1256
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#2227]: https://github.com/microsoft/terminal/issues/2227
[#653]: https://github.com/microsoft/terminal/issues/653
[#1032]: https://github.com/microsoft/terminal/issues/1032
[#632]: https://github.com/microsoft/terminal/issues/632
