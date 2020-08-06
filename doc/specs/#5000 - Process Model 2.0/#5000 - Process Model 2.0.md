---
author: Mike Griese @zadjii-msft
created on: 2020-07-31
last updated: 2020-08-06
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

## Background

In order to better understand some of the following technical solutions, it's
important to understand some of the technical hurdles that need to be overcome.

### Mixed admin and unelevated clients in a single window

Let's presume that you're a user who wants to be able to open an elevated tab
within an otherwise unelevated Terminal window. We call this scenario "mixed
elevation" - the tabs within the Terminal can be running either unelevated _or_
elevated client applications.

It wouldn't be terribly difficult for the unelevated Terminal to request the
permission of the user to spawn an elevated client application. The user would
see a UAC prompt, they'd accept, and then they'd be able to have an elevated
shell alongside their unelevated tabs.

However, this creates an escalation of priviledge vector. Now, there's an
unelevated window which is connected directly to an elevated process. At this
point, any other unelevated application could send input to the Terminal's
`HWND`, making it possible for another unelevated process to "drive" the
Terminal window and send commands to the elevated client application.

### Drag and drop tabs to create new windows

Another important scenario we're targeting is the ability to drag a tab out of
the Terminal window and create a new Terminal window. Obviously, when we do
this, we want the newly created window to be able to persist all the same state
that the original window had for that tab. For us, that primarily means the
buffer contents and connection state.

However, _how_ do we move the terminal state to another window? The terminal
state is all located in-memory of the thread that created the `TermControl`
hosting the terminal buffer. It's fairly challenging to re-create this state in
another process.

Take a look at [this
document](https://github.com/windows-toolkit/Sample-TabView-TearOff) for more
background on how the scenario might work.

There's really only a limited selection of things that a process could transfer
to another with a drag and drop operation. If we wanted to use a string to
transfer the data, we'd somehow need to serialize then entire state of the tab,
it's tree of panes, and the state of each of the buffers in the tab, into some
sort of string, and then have the new window deserialize that string to
re-create the tab state. Consider that each bufer might include 32000 rows of
80+ characters each, each with possibly RGB attributes, and you're looking at
30MB+ of raw data to serialize and de-serialize per buffer, minimum. This sounds
extremely fragile, if not impossible to do robustly.

What we need is a more effective way for seperate Terminal windows to to be able
to connect to and display content that's being hosted in another process.

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
window process may be in communication with multiple content processes - one per
terminal instance. That means that each and every `TermControl` in a window
process will be hosted in a seperate process.

The window process will be full of "thin" `TermControl`s - controls which are
only the XAML layer and a WinRT object which is hosted by the content process.
These thin `TermControl`s will recieve input, and have all the UI elements
(including the `SwapChainPanel`) of the `TermControl` today, but the actual
_rendering_ to the swap chain, and the handling of those inputs will be done by
the content process.

As a broad outline, whenever the window wants to create a terminal, the flow
will be something like the following:

1. A window process will spawn a new content process, with a unique ID.
2. The window process will attach itself to the content process, indicating that
   it (the window process) is the content process's hosting window.
3. When the content process creates it's swap chain, it will raise an event
   which the window process will use to connect that swap chain to the window
   process's `SwapChainPanel`.
4. The will process output from the connection and draw to the swap chain. The
   contents that are rendered to the swap chain will be visible in the window
   process's `SwapChainPanel`, because they share the same underlying kernel
   object.

The content process will be responsible for the terminal buffer and other
terminal state (the core `Terminal` object), as well as the `Renderer`,
`DxEngine`, and `TerminalConnection`. These are all being combined in the
content process, as to maximize performance. We don't want to have to hop across
the process boundary multiple times per frame, so the renderer must be in the
same process as the buffer. Similarly, we want to be able to read data off of
the connection as quickly as possible, and the best way to do this will be to
have the connection in the same process as the `Terminal` core.


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
identify _content processes_. Each content process will recieve a unique GUID
when it is created, and it will register as the server for that GUID. Then, any
window process will be able to connect to that specific content process strictly
by GUID. Because each GUID is unique to each content process, any time any
client calls `create_instance<>(theGuid, ...)`, it will uniquely attempt to
connect to the content process hosting `theGuid`.

This means that if we wanted to have a second window process connect to the
_same_ content process as another window, all it needs is the content process's
GUID.

#### Scenario: Tab Tearoff/ Reattach

Because all that's needed to uniquely identify an individual terminal instance
is the GUID of the content process, it becomes fairly trivial to be able to
"move" a terminal instance from one window to another.

When a drag/drop operation happens, the payload of the event will simply contain
the structure of the tree of panes, and the GUIDs of the content processes that
make up the leaf nodes of the tree. The recieving window process will then be
able to use that tree to be able to re-create a similar pane structure in its
window, and use the GUIDs to be able to connect to the content processes for the
terminals in that tab.

The terminal buffer never needs to move from one process to another - it always
stays in just a single content process. The only thing that changes is the
window process which is rendering the content process's swapchain.

#### Scenario: Mixed Elevation

With the ability for window processes to connect to other pre-existing content
processes, we can now also support mixed elevation scenarios as well.

If a user requests a new elevated tab from an otherwise unelevated window, we
can use UAC to create a new, elevated WP, and "move" all the current tabs to
that WP, as well as the new elevated client. Now, the WP is elevated, preventing
it from input injection, but still contains all the previously existing tabs.
The original WP can now be discarded, as the new elevated WP will pretend to be
the original window.

We would probably want to provide some additional configuration options here as
well:
* Users will most likely want to be able to specify that a given profile should
  always run elevated.
  - We should also provide some argument to the `NewTerminalArgs` (used by the
    `new-tab` and `split-pane` actions) to allow a user to elevate an otherwise
    unelevated profile.
* We'll probably want to create new CPs in a medium-IL context by default,
  unless the profile or launch args otherwise indicates launching elevated. So a
  WP running elevated would still create unelevated profiles by default.
* Currently, if a user launches the terminal as an administrator, then _all_ of
  their tabs run elevated. We'll probably want to provide a similar
  configuration as well. Maybe if a WP is initially launched as elevated (rather
  than inheriting from an existing session), then it could either:
  - default all future connections it creates to elevated connections
  - assume the _first_ tab (or all the terminals created by the startup
    commandline) should be elevated, but then subsequent connections would
    default to unelevated processes.

It's a long-standing platform bug that you cannot use drag-and-drop in elevated
scenarios. This unfortunately means that elevated WPs will _not_ be able to tear
tabs out into their own windows. Even tabs that only contain unelevated CPs in
them will not be able to be torn out, because the drag-and-drop service is
fundamentally incapable of connecting to elevated windows.

We should probably have a discussion about what happens when the last elevated
CP in an elevated WP is closed. If an elevated WP doesn't have any remaining
elevated CPs, then it should be able to revert to an unelevated WP. Should we do
this? There's likely some visual flickering that will happen as we re-create the
new window process, so maybe it would be unappealing to users. However, there's
also the negative side effect that come from elevated windows (the aformentioned
lack of drag/drop), so maybe users will want to return to the more permissive
unelevated WP.

This is one section where unfortunately I don't have a working prototype yet.
From my research, an elevated process cannot simply `create_instance` a class
that's hosted by an unelevated process. However, after proposing this idea to
the broad C++/WinRT discussion alias, no one on that thread immediately shot
down the idea as being impossible or terrible from a security perspective, so I
believe that it _will_ be possible. We still need to do some research on the
actual mechanisms for some token impersonation.

### Monarch and Servant Processes

With the current design, it's easy to connect many content processes to a window
process, and move those content processes easily between windows. However, we
want to make sure that it's also possible to communicate between these
processes. What if we want to have only a single WT instance, so that whenever
Wt is run, it creates a tab in the existing window? What if you want to run a
commandline in a given WT window?

In addition to the concept of Window and Content Processes, we'll also be
introducing another type of categorization for window processes. These are
"Monarch" and "Servant" processes. This will allow for the coordination across
the various windows by the Monarch.






#### Scenario: Single Instance Mode

In Single Instance mode, the monarch _is_ the single instance. When `wt` is run,
and it determines that it is not the monarch, it'll as the monarch if it's in
single instance mode. If it is, then the servant that's starting up will instead
pass it's commandline arguments to the monarch process, and let the monarch
handle them, then exit.

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

TODO: Concerns with the Universal App version of the Terminal?
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


<hr>

1. Add Monarch/Servant capabilities to `wt` window processes.
   - This does not need to involve any actual UX functionality, simply have the
    WT instances communicate with one another to see who is the Monarch.
   - The monarch will track and assign IDs to servants.
2. (Dependent on 1): Add support for running a `wt` commandline in an existing
   window
   - Monarch should assign simple IDs for use with the `wt` commandline
   - `0` should be reserved as an alias for "the current window"
3. (Dependent on 1, maybe on 2): Add support for "single instance mode". New
   servant windows will first ask the monarch if it's in single instance mode,
   and pass the servant's commandline to the monarch if it is.
4. (Dependent on 1): Monarch registers for the global quake hotkey, and uses
   that to activate _the monarch_.
   - Pressing the key when a window is currently focused should minimize? Do nothing?
5. (Dependent on 4): any othe quake mode improvements:
   - Summon the nearest window
   - make the window "drop down" from the top
   - Summon the MRU window
     - It would need to track a the MRU for windows, so pressing the shortcut when
       no window is active summons the MRU one.

<hr>

6. Change `TermControl`/`DxEngine` to create its own swap chain using
   `DCompositionCreateSurfaceHandle` and
   `CreateSwapChainForCompositionSurfaceHandle`. This is something that's
   already done in commit TODO:`ffffff`.

7. (Dependent on 6) Refactor `TermControl` to allow it to have a WinUI layer and
   a Core layer. The WinUI layer will be only responsible for interacting with
   XAML, processing input, etc, and sending it to the core layer, which handles
   all the logic concerning input. The core will expose these methods that the
   UI layer calls as projected methods

8. (Dependent on 7): Expose the methods the UI layer calls on the core as
   projected methods. The control is still fundamentally in-proc, and the UI
   layer calls directly into the implementation of the control core, but the
   methods _could_ be used x-proc.

9. (Dependent on 8): Enable a `TermControl` to have an out-of proc core.
   - The core will need to be able to accept a PID using some method, and be able
    to `DuplicateHandle` the swapchain to that PID. It'll also need to raise the
    `SwapChainHandleChanged` event.
   - The Control UI layer would need to recieve a GUID in the ctor, and use that
     connect to the out-of-proc core. It'll pass the UI's PID to the core, and
     attach to the core's swap chain handle.
   - The UI layer will be smart enough to call either the implementation method
     (if it's hosting in-proc) or the projected method (if it's out-of-proc).

10. (Dependent on 9?) The core layer needs to be able to construct connections
    itself, rather than have one passed in to it.
   - In the future we'll probably want this to be more extensible, but for now
     we can probably just pass an enum for connection type, and an
     `IConnectionSettings` object to the core, and use the `ConnectionType` and
     setting to build the limited types of connection we currently have.

11. (Dependent on 9, 10) `wt.exe` needs to be able to spawn as a content
    process, accepting a GUID for its ID, and spawning a single control core.
   - When the content process is first spawned, it won't create the core or
     connection, nor will it have any settings. The first client to connect to
     the content process should make sure to set up the settings before
     initializing the control.
   - A scratch XAML Island application might be a useful tool at this point, to
     test hosting the content in another process (that's not a full-blown
     terminal instance).

12. (Dependent on 11) TerminalApp creates new content processes for each and
    every terminal instance it spawns. At this point, there's no tearout, but
    the terminal instances are all out-of-proc from the window process.

13. (Dependent on 12) Terminal can drop tabs onto another WT window process by
    communicating the structure of the tab's panes and their content GUIDs.
   - At this point, the tabs can't be torn out to create new windows, only move
     between existing windows
   - It might be hard to have the tab "attach" to the tab row of the other window.
   - It might be easiest to do this after 1, and communicate to the new WP the
     GUID of the old WP and the tab within the original WP, and just have the
     new WP ask the old WP what the structure of the tab is
     - Though, the tab won't be in the original process's list of tabs anymore,
       so that might not be helpful.

14. `wt` accepts an initial position, size on the commandline.

15. (Dependent on 13, 14) Tearing out a tab and dropping it _not_ on another WT
    window creates a new window for the tab.
   - This will require and additional argument to `wt` for it to be able to
     inherit the structure of a given set of tabs. Either this will need to be
     passed on the cmdline, or the newly spawned process will need to be able to
     communicate with the original process to ask it what structure it should be
     building.
   - Doing this after 1 might be helpful.
   - Needs 14 to be able to specify the location of the new window.

<hr>

16. Add support for a `NewWindow` action
17. `wt` accepts a commandline arg to force it to auto-elevate itself if it
    isn't already elevated.
18. (Dependent on 16, 17) Users can mark a profile as `"elevated": true`, to
    always force a new elevated window to be created for elevated profiles.
19. (Dependent on 18, 13) When a user creates an elevated profile, we'll
    automatically create an elevated version of the current window, and move all
    the current tabs to that new window.

## Footnotes

<a id="footnote-1"><a>[1]:

## Future considerations

* Yea add these

* Non-terminal content in the Terminal. We've now created a _very_
  terminal-specific IPC mechanism for transfering _terminal_ state from one
  thread to another. When we start to plan on having panes that contain
  non-terminal content in them, they won't be able to move across-process like
  this. Options available here include:
  - Passing some well-defined string / JSON blob from the source process to the
    target process, such that the extension could use that JSON to recreate
    whatever state the pane was last in. The extension would be able to control
    this content entirely.
      - Maybe they want to pass a GUID that the new process will be able to user
        to `CreateInstance` the singleton for their UI and then dupe their swap
        chain to the new thread...
  - Preventing tabs with non-terminal content open in them from being dragged
    out entirely. They're stuck in their window until the non-terminal content
    is removed.
* A previous discussion with someone on the input team brought up the following
  idea: When a tab is being torn out, move all the _other_ tabs to a new window,
  and continue dragging the current window. This should make the drag/drop
  experience feel more seamless, and might be able to allow us to render the tab
  content as we're drag/dropping.
* We could definitely do a `NewWindowFromTab(index:int?)` action that creates a
  new window from a current tab once this all lands.
  - Or it could be `NewWindowFromTab(index:union(int,int[])?)` to specify the
    current tab, a specific tab, or many tabs.


## TODOs

* [ ] Prove that a elevated window can host an unelevated content
* [ ] Prove that I can toss content process IDs from one window to another
* [ ] come up with a commandline mechanism for starting one `wt.exe` process either
  as a window process, or as a content process
* [ ] What about handling XAML input events? The "thin" term control will need to do
  that in-proc, so it can reply on the UI thread if a particular keystroke was
  handled or not.
  - I'm almost certain that the `Terminal::HandleKey()` stuff is going to need
    to be handled in the thin control, and the core will need to raise events to
    the control? oof


## Resources

* [Tab Tearout in the community toolkit] - this document proved invaluable to
  the background of tearing a tab out of an application to create a new window.

<!-- Footnotes -->

[#5000]: https://github.com/microsoft/terminal/issues/5000
[#1256]: https://github.com/microsoft/terminal/issues/1256
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#2227]: https://github.com/microsoft/terminal/issues/2227
[#653]: https://github.com/microsoft/terminal/issues/653
[#1032]: https://github.com/microsoft/terminal/issues/1032
[#632]: https://github.com/microsoft/terminal/issues/632

[Tab Tearout in the community toolkit]: https://github.com/windows-toolkit/Sample-TabView-TearOff
