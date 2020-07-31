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

### Window and Content Processes

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
4. The will process output from the connection and draw to the swap chain. The
   contents that are rendered to the swap chain will be visible in the WP's
   `SwapChainPanel`, because they share the same underlying kernel object.

The CP will be responsible for the terminal buffer and other terminal state (the
core `Terminal` object), as well as the `Renderer`, `DxEngine`, and
`TerminalConnection`. These are all being combined in the CP, as to maximize
performance. We don't want to have to hop across the process boundary multiple
times per frame, so the renderer must be in the same process as the buffer.
Similarly, we want to be able to read data off of the connection as quickly as
possible, and the best way to do this will be to have the connection in the same
process as the `Terminal` core.


#### Technical Details

Much of the above is powered by the magic of WinRT (which is powered by the
magic of COM). Whenever we create WinRT types, WinRT provides metadata about
these types that not only enables us to use them in-proc (via the
implementation), but also enables using these types _across process boundaries_.

Typically, these classes are given a unique GUID for the class. A process that
wants to implement one of these out-of-proc WinRT objects can register with the
system to say "I make `MyClass`'s, and their GUID is `{foo}`". these are called
WinRT _servers_. Then, consumers (_clients_) of that class can ask the system
"I'd like to instantiate a `{foo}` please", which will cause the server to
create a new instance of that object (in the server process), and the client
will be given a winrt object which can interact with the classes WinRT
projection.

A server can register the types it produces with `CoRegisterClassObject`, like so:

```c++
CoRegisterClassObject(MyClassGUID,
                      winrt::make<MyClassFactory>().get(),
                      CLSCTX_LOCAL_SERVER,
                      REGCLS_MULTIPLEUSE,
                      &dwRegistration);

```

And a client can attempt to get an instance of `MyClass` with

```c++
auto myClass = create_instance<winrt::MyClass>(MyClassGUID, CLSCTX_LOCAL_SERVER);
```

We're going to be using that system a little differently here. Instead of using
a GUID to represent a single _Class_, we're going to use the GUID to uniquely
identify _CPs_. Each CP will recieve a unique GUID when it is created, and it
will register as the server for that GUID. Then, any WP will be able to connect
to that specific CP strictly by GUID. Because each GUID is unique to each
content process, any time any client calls `create_instance<>(theGuid, ...)`, it
will uniquely attempt to connect to the content process hosting `theGuid`.

This means that if we wanted to have a second WP connect to the _same_ CP as
another window, all it needs is the CP's GUID.

#### Scenario: Tab Tearoff/ Reattach

Oh the flow between these sections is less than stellar. I need to introduce why moving object is hard first, then talk about just moving guids

#### Scenario: Mixed Elevation

### Monarch and Servant Processes

With the current design, it's easy to connect many CPs to a WP, and move those
CPs easily between windows. However, we want to make sure that it's also
possible to communicate between these processes. What if we want to have only a
single WT instance, so that whenever Wt is run, it creates a tab in the existing
window? What if you want to run a commandline in a given WT window?

In addition to the concept of Window and Content Processes, we'll also be
introducing another type of categorization for WPs. These are "Monarch" and
"Servant" processes. This will allow for the coordination across the various windows by the Monarch.






#### Scenario: Single Instance Mode

In Single Instance mode, the monarch _is_ the single instance. When `wt` is run, and it determines that it is not the monarch, it'll as the monarch if it's in single instance mode. If it is, then the servant that's starting up will instead pass it's commandline arguments to the monarch process, and let the monarch handle them, then exit.

#### Scenario: Run `wt` in the current window

We should reserve the session id `0` to always refer to "The current window", if
there is one. So `wt -s 0 new-tab` will run `new-tab` in the current window (if
we're being run from WT), otherwise it will create a new window.

In Single-Instance mode, running `wt -s 0` outside a WT window will still cause
the commandline to glom to the existing single terminal instance, if there is
one.

#### Scenario: Quake Mode


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

Extensions & non-terminal content.

## Implementation Plan

Obviously, everything that's discussed here represents an _enormous_ amount of
work. It's important that as we move towards this new model, that we do so in
safe, incremental chunks, such that each step is still a viable Terminal
application, but no single step is too large to review. As such, I'll attempt to
break down the work required into atomic pieces, and provide a relative ordering
for the work described above.


1. Add Monarch/Servant capabilities to `wt` WPs.
  - This does not need to involve any actual UX functionality, simply have the
    WT instances communicate with one another to see who is the Monarch.
  - The monarch will track and assign IDs to servants.
2. (Dependant on 1): Add support for running a `wt` commandline in an existing window
3. (Dependant on 1, maybe on 2): Add support for "single instance mode"

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


## Resources

* [Tab Tearout in the community toolkit]

<!-- Footnotes -->

[#5000]: https://github.com/microsoft/terminal/issues/5000
[#1256]: https://github.com/microsoft/terminal/issues/1256
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#2227]: https://github.com/microsoft/terminal/issues/2227
[#653]: https://github.com/microsoft/terminal/issues/653
[#1032]: https://github.com/microsoft/terminal/issues/1032
[#632]: https://github.com/microsoft/terminal/issues/632

[Tab Tearout in the community toolkit]: https://github.com/windows-toolkit/Sample-TabView-TearOff
