---
author: Michael Niksa @miniksa/miniksa@microsoft.com
created on: 2019-07-24
last updated: 2019-07-24
issue id: #1256
---

# Tab Tearoff/Merge & Default App IPC

## Abstract

This spec describes the sort of interprocess communications that will be required to support features like tab tearoff and merge. It goes through some of the considerations that became apparent when I tried to prototype passing connections between `conhost` and `wt`.

## Inspiration

Two main drivers:
1. We want the ability to tear off a tab from one Windows Terminal instance and send it to another Windows Terminal instance
2. We want the ability for a launch of a command-line application to trigger a hosting environment that isn't the stock in-box `conhost.exe`.

Both of these concerns will require there to exist some sort of interprocess communication manager that can send/receive the system handles representing connections between client applications and the hosting environment.

I spent some time during the Microsoft Hackathon in July 2019 investigating these avenues with a branch I pushed and linked at the bottom. The work resulted in me finding more questions than answers and ultimately deciding that a Hackathon is good enough for exploration of the mechanisms and ideas behind this, but not a good time for a full implementation.

## Solution Design

### Common Pieces

There are several common pieces needed for both the tab tear-off scenario and the default application scenario.

#### Manager

We need some sort of server/manager code that sits there waiting for connections from `wt.exe` processes and potentially `conhost.exe` processes such that it can broker a connection between the processes. It either needs to run in its own process or it needs to run in one of the existing `wt.exe`s that is chosen as the primary manager at the time. It should create communication channels and a global mutex at the time of creation.

All other `wt.exe` processes starting after the primary should detect the existence of the server manager process and wait on the mutex handle. When the primary disappears, the OS scheduler should choose one of the others to wake up first on the mutex. It can take the lock and then set up the primary management channel. 

Alternatively, if the manager process is completely isolated and we expect all `wt.exe`s to have to remain connected at all times, we can make it such that when the connections are broken between the individual processes and the manager that they all shut down. I would prefer that it is resilient (the previous option) over this one, but browsers must have a good reason for preferring this way.

I attempted one particular way in a prototype of communicating between processes by setting up a Multithreaded Pipe Server using a Message-type configuration. This is visible in the branch I linked at the bottom. However, ultimately I think we would want to formalize around something more structured, tested, and inherently secured like a COM server interface.

#### Connection details

There are several parameters to a connection and several different modes. In short, they summarize to the ability to pass kernel handles between two processes and/or the ability to pass arbitrary length structured information about paths and settings. Both tab tear off and default application will likely need both functionalities.

##### Fresh Start
For an application that is being freshly started, the information required to begin the session is one of three things:
1. A server (and maybe reference) handle that describes the driver connection between the console server and the command-line client process. A `conhost.exe` can wrap this and turn it into a PTY. This may also contain LNK file (shortcut file) preferences for the running session.
1. A command-line string and working directory that describes which command-line client process we want to start. A `conhost.exe` can start this up and create the server and reference handles along the way and then turn it into a PTY.
1. A PTY session with its read, write, and signal handles.

When transiting a connection, we need to be aware of all three of these modes and relay them to the destination `wt.exe`.

For system handles, we can use the manager to broker a request to the destination process to find its `PID` and tell the source process. We can then use the `PID` with the `OpenProcess` method and the `PROCESS_DUP_HANDLE` right to get a handle to `DuplicateHandle` any of the above handle types into the destination process. The act of opening and duplicating the handles already requires the OS to check our access tokens and rights to interfere with another process, so that should automatically handle some level of the security checking for us.

For command-line string and working directory, we can pass all of this information along to the destination `wt.exe` and let it attempt to start a new ConPTY normally as if someone had chosen to start an option from the dropdown menu. A minor trick here is that we may need to attempt to match the command-line string with one of the user profiles to line up the icon and user-preferences for how the session should launch.

Lastly, for things started from an LNK, a user might expect that a window launched inside `wt.exe` from an old shortcut that they had would still apply even if that shortcut's properties technically apply to `conhost.exe` preferences and not to `wt.exe` preferences. The behavior here would likely to be to transit the LNK file information along to the `wt.exe` process by the same mechanism as a command-line string or working directory and let `wt.exe` use the shortcut parsing shared libraries to extract this information and migrate it into a `Settings` preference. Whether we would store that `Settings` preference or not for future use in the drop down might be an option or a prompt.

##### Already Running
For an application that is already running, we will need to send several pieces of information to successfully migrate to a new tab location:
1. The ConPTY handles for read, write, and signal
1. The scroll-back history that is stored inside `wt.exe` but isn't actually a part of what the underlying PTY-mode `conhost.exe` re-renders at any given time
1. The user preferences and session information related to `Settings`.

We would send all of this over to the destination by whatever IPC mechanism and then let it stand up a new tab with all of the same parameters as the tab on the other end.

**ALTERNATIVELY**

If we move everything to an isolated process model where the individual tabs/panes have a process and their UI is hosted in another frame/shell process and then there's a manager process, we will presumably already have to architect a solution that allows the UIs to be remoted onto other interfaces (Component UI?). If this is true, then all we need to relay for an active session is the information required to redirect the drawing/input targets for a given tab/pane to a different shell. This may ultimately be easier and more reliable than moving and rebuilding all the pieces of what fundamentally makes a session to the other side.

### Separate Pieces

#### For Tab Tear-off

We add a handler to the on-drag for the tab bar. We also likely need to implement a drag and drop handler. Drag and drop handlers use OLE (COM) so this might be another reason why we should implement the entire manager as COM. Note, I have never used this before so this is a theoretical low-knowledge design that would have to be explored...

Presumably the tab control from WinUI will update to support reordering the tabs through its own drag/drop. But we would likely want to create some sort of drag source with the session GUID when a drag operation starts.

Then we can let the OS handle the drop operation with the session GUID information. If the drop handler drops onto another wt.exe, it can use the session GUID in the drop payload in order to convey connection information between the processes. If it drops somewhere else, presumably we can be made aware of that in the source of the drag/drop operation and instead spawn a new `wt.exe` with arguments that specify that it should start up doing the "drop" portion of the operation to the session GUID with the manager instead of launching the default tab.

#### For Default Application

For default application launches, `conhost.exe` will have to attempt to transfer the incoming connection to the registered terminal handler instead of launching its own UI window.

If the registered handler fails to start the connection, there is no registered handler, or any part of this mechanism fails. The `conhost.exe` needs to fall back to doing whatever it would have done prior to this development (launching a window if necessary, being hidden, etc.)

##### Interactive vs. Not

We would have to be able to detect the difference between an interactive and non-interactive mode here. 
- Interactive is defined as the end-user is attempting to launch a command-line application with a visible window to see the output and enter input.
- Non-interactive is defined as tools, utilities, and services attempting to launch a command-line application with no visible window (and possibly some redirected handles).

We do not want to capture non-interactive sessions as compilers, scripts, and utilities run command-line tools all the time. These should not trigger the overhead of being transitioned into the terminal as they will not need output or display.

Additionally, we may need to identify ConPTYs being started and ensure that they don't accidentally attempt to hand off in an infinite loop.

The biggest trick here is that we don't know whether it is going to be interactive or not until we begin to accept the connection from the server handle. We have two choices here:

##### Inbox conhost handles it
The inbox `conhost.exe` can just accept the connection from the server handle, assure itself that a `wt.exe` could take over the UI hosting of the session, and then switch itself into `ConPTY` mode and give those handles over to `wt.exe` and remain invisible in the background in PTY mode (much the same as if `wt.exe` had started the connection itself).

The upside here is that most of the startup connection flow happens normally, the `conhost.exe` that was given the server handle is the one that will continue to service it for the lifetime of the command-line application session. I can then discard any concerns about how the driver reacts and how the applications grovel for the relationship between processes as it will be normal.

The downside here is that launching command-line applications from shortcuts, the shell, or the run box (as is what triggers the default application scenario) will be using an old version of the PTY. It is possible and even probable that we will make improvements to the PTY that we would want to leverage if they're on the system already inside the app package. However, if we try to transit the server connection to the PTY in the package, we will have to deal with:
1. Potentially leaving the original conhost.exe open until the other one exits in case someone is waiting on the process
1. Coming up with some sort of dance to have the delegated PTY conhost inside the package determine the interactivity on starting the connection **OR** having the outside conhost start the connection and passing the connection off part way through if it's interactive **OR** something of that ilk.

##### Conhost in the Terminal package handles it
We could just send the server connection from the `conhost.exe` in System32 into the one inside the package and let it deal with it. We can connect to the broker and pass along the server handle and let `wt.exe` create a `conhost.exe` in PTY mode with that specific server handle.

The upsides/downsides here are exactly opposite of those above, so I won't restate.

##### Making default app work on current and downlevel OS
There's a few areas to study here.

1. Replacing conhost.exe in system32 at install time
- The OS (via the code for `ConsoleInitialize` inside `kernelbase.dll`) will launch `C:\windows\system32\conhost.exe` to start a default application session with the server handle. We can technically replace this binary in `system32` with an `OpenConsole.exe` named `conhost.exe` to make newer code run on older OS (presuming that we have the CRTs installed, build against the in-OS-CRT, and otherwise have conditional feature detection properly performed for all APIs/references not accessible downlevel). This is how we test/develop locally inside Windows without a full nightly build, so we know it works to some degree. Replacing a binary in `system32` is a bit of a problem, though, because the OS actively works to defend against this through ACLs (Windows File Protection which detected and restored changes here is gone, I believe). Additionally, it works for us because we're using internal builds and signing our binaries with test certificates for which our machines have the root certificate installed. Not going to cut it outside. We probably also can't sign it officially with the app signing mechanism and have it work because I'm not sure the root certificates for app signing will be trusted the same way as the certificates for OS signing. Also, we can't build outside of Windows against the in-box CRT. So we'd have to have the MSVCRT redist, which is also gross.

2. Updating kernelbase.dll to look up the launch preference and/or to launch a console host via a protocol handler
- To make this work anywhere but the most recent OS build, we'd have to service downlevel. Given `kernelbase.dll` is fundamental to literally everything, there's virtually no chance that we would be allowed to service it backwards in time for the sake of adding a feature. It's too risky by any stretch of the imagination. It's even risky to change `kernelbase.dll` for an upcoming release edition given how fundamental it is. End of thought experiment.

3. Updating conhost.exe to look up the launch preference and/or to launch another console host via a protocol handler
- This would allow the `C:\windows\system32\conhost.exe` to effectively delegate the session to another `conhost.exe` that is hopefully newer than the inbox one. Given that the driver protocol in the box doesn't change and hasn't changed and we don't intend to change it, the forward/backward compatibility story is great here. Additionally, if for whatever reason the delegated `conhost.exe` fails to launch, we can just fall back and launch the old one like we would have prior to the change. It is significantly more likely, but still challenging, to argue for servicing `conhost.exe` back several versions in Windows to make this light up better for all folks. It might be especially more possible if it is a very targeted code snippet that can drop in to all the old versions of the `conhost.exe` code. We would still have the argument about spending resources developing for OS versions that are supposed to be dropped in favor of latest, but it's still a lesser argument than upending all of `kernelbase.dll`.
- A protocol handler is also well understood and relatively well handled/tested in Windows. Old apps can handle protocols. New apps can handle protocols. Protocol handlers can take arguments. We don't have to lean on any other team to get them to help change the way the rest of the OS  works. 

#### Communicating the launch
For the parameters passing, I see a few options:
1. `conhost.exe` can look up the package registration for `wt.exe` and call an entrypoint with arguments. This could be adapted to instead look up which package is registered as the default one instead of `wt.exe` for third party hosts. We would have to build provisions into the OS to select this, or use some sort of publically documented registry key mechanism. Somewhat gross.
1. `conhost.exe` can call the execution alias with parameters. WSL distro launchers use this.
1. We can define a protocol handler for these sorts of connections and let `wt.exe` register for it. Protocol handlers are already well supported and understood both by classic applications and by packaged/modern applications on Windows. They must have provisions to communicate at least some semblance of argument data as well. This is the route I'd probably prefer. `ms-term://incoming/<session-id>` or something like that. The receiving `wt.exe` can contact the manager process (or set one up if it is the first) and negotiate receiving the session that was specified into a new tab.

## UI/UX Design

### For Tab Tear-off

#### Ideal World
The UX would be just as one might expect from a browser application.

- Mouse down and drag on a tab should provide some visual indication that it is being dragged.
- Dragging left/right should provide a visual indicator of the tabs reordering on the bar and otherwise not involve the IPC manager service.
- Dragging up/down to break free from the tab bar should launch a new instance of `wt.exe` passing in the state of the dragging tab as the initial launch point (ignoring other default launch aspects). The drag/mouse-down would be passed to that new instance which would chase the mouse.
- Continuing to drag the loose tab onto the tab bar of another running instance of `wt.exe` would merge the tab with that copy of the application. The interim new/loose frame instance of `wt.exe` would close when it transferred out the last tab to the drop location.

#### Simplified V1
To simplify this for a first iteration, we could just make it so the transfer does not happen live.
- Mouse down and drag on a tab should provide a visual indication that it is being dragged by changing the cursor (or something of that ilk)
- Nothing would actually happen in terms of transitioning the tab until it is released
- If released onto the same `wt.exe` instance in a different spot on the tab bar, we reorder the tabs in the tab control
- If released onto a different `wt.exe` instance, we relay the communications channel and details through the IPC manager to the other instance. It opens the tab on the destination instance; we close the tab on the source instance.
- If released onto anything that isn't a `wt.exe` instance, we create a new `wt.exe` instance and send in the connection as the default startup parameter.

#### Component UI
It is also theoretically possible that if we could find a Component UI style solution (where the tab/panes live in their own process and just remote the UI/input into the shell) that it would be easy and even trivial to change out which shell/frame host is holding that element at any given time. 

### For Default Application
The UX would make it look exactly like the user had started `wt.exe` from a shortcut or launch tile, but would launch the first tab differently than the defaults.

#### No WT already started
If no `wt.exe` is already started, the `conhost.exe` triggered by the system to host the client application would find the installed `wt.exe` package and launch it with parameters to use as its first connection (in lieu of launching the default tab). `conhost.exe` wouldn't show a window, it would drop into ConPTY mode and only the new `wt.exe` and its tab would be visible.

#### WT already started
If a `wt.exe` is already started, `conhost.exe` would find the running instance and just add a new tab at the end of the tab bar by the same mechanism.

#### Multiple WTs already started
If multiple `wt.exe`s are already started, `conhost.exe` would have to find the foreground one, the active one, or the primary/manager one and send the tab there. I'm not sure how other tabbing things to do this. We could research/study.

## Capabilities

### Accessibility

I don't believe it changes anything for accessibility. The only concern I'd have to call out is the knowledge I have that the UIA framework makes its connections and some of its logic/reasoning based on PIDs, HWNDs, and the hierarchy thereof. Playing with these might impact the ability of screen reading applications to get the UIA tree when tabs have been shuffled around.

### Security

This particular feature will have to go through a security review/audit. It is unclear what level of control we will need over the IPC communication channels. A few things come to mind:
1. We need to ensure that the mutexes/pipes/communications are restricted inside of one particular session to one particular user. If another user is also running WT in their session, it should involve a completely different manager process and system objects.
1. We MAY have to enforce a scenario where we inhibit cross-integrity-level connections from being passed around. Generally speaking, processes at a higher integrity level have the authority to perform actions on those with a lower integrity level. This means that an elevated `wt.exe` could theoretically send a tab to a standard level `wt.exe`. We may be required to inhibit/prohibit this. We may also need to have one manager per integrity level. 
1. I'm not sure what sorts of ACL/DACL/SACLs we would need to apply to all the kernel objects involved.
1. My initial prototype here used message-passing type pipes with a custom rolled protocol. If I make my own protocol, it needs to be fuzzed. And I'm probably missing something. Many/most of these concerns for security are probably eliminated if we use a well-known mechanism for this sort of IPC. My thoughts go to a COM server. More complicated to implement than message pipes, but probably brings a lot of security benefits and eliminates the need to fuzz the protocol (probably).

### Reliability

In the simple implementation, it will decrease reliability. We'll be shuffling connections back and forth between application instances. By default, that's more risky than leaving things alone. The only reason it is worth it is the user experience.

We might be able to mitigate some of the reliability concerns here or even improve reliability by going a step further with the process/containerization model like browsers do and standing up each individual tab as its own process host.

```
wt.exe - Manager Mode 
|- wt.exe - Frame Host Mode 
|   |- wt.exe - Tab Host Mode
|   |  |- conhost.exe - ConPTY mode
|   |     |- pwsh.exe - Client application 
|   |- wt.exe - Tab Host Mode
|      |- conhost.exe - ConPTY mode
|         |- cmd.exe - Client application 
|- wt.exe - Frame Host Mode 
    |- wt.exe - Tab Host Mode
       |- conhost.exe - ConPTY mode
          |- pwsh.exe - Client application 
```

The current structure of `wt.exe` has everything hosted within the one process. To improve reliability, we would likely have to make `wt.exe` run in three modes.
1. Manager Mode - no UI, just sits there as a broker to hold the kernel objects for a given window station/session and integrity level, accepts protocol handler routines, helps relay connections between various frame hosts when tabs move and determines where to instantiate new default-app tabs
1. Frame Host Mode - The complete outer shell of the application outside of an individual tab. Hosts the tab bar, settings drop downs, title bar, etc.
1. Tab Host Mode - The inner shell of an individual tab including the rendering area, scroll bar, inputs, etc.
1. Pane Host Mode - Now that panes are a thing, we might need to go even one level deeper. Or maybe it's just a recursion on Tab Host mode.

How these connect to each other is unexplored at this time.

### Compatibility

There are a few compatibility concerns here, primarily related to how client applications or outside utilities detect the relationship between a command-line client application and its console hosting environment.

We're well aware that the process tree/hierarchy is one of the major methods used for understanding the relationship between the client and server application. However, in order to accomplish our goals here, it is inevitable that the original hosting `conhost.exe` (either started in ConPTY mode by a `wt.exe` or started by the operating system in response to an otherwise unhosted command-line application) will become orphaned or otherwise disassociated with the UI that is actually presenting it.

It is possible (but would need to be explored) that the APIs available to us to reorder the parenting of the processes to put the `conhost.exe` as the parent of the `cmd.exe` (despite the fact that `cmd.exe` usually starts first as the default application and the `ConsoleInitialize` routines inside `kernelbase.dll` create the `conhost.exe`) could be reused here to shuffle around the parent/child relationships. However, it could also introduce new problems. One prior example was that the UIA trees for accessibility do **NOT** tolerate the shuffling of the parent child relationship because their communication channel sessions are often tied to the relationships of HWNDs and PIDs.

#### Hierarchy Example between two Terminals (tab tearoff/merge)

In the one instance, we have this process hierarchy. Two instances of Windows Terminal exist. In Terminal A, the user has started a `cmd.exe` and a `pwsh.exe` tab. In the second instance, the user has started just one `cmd.exe` tab.

```
- wt.exe (Terminal Instance A) 
  |- conhost.exe (in PTY mode) - Hosted to A
  |  |- cmd.exe
  |- conhost.exe (in PTY mode) - Hosted to A
     |- pwsh.exe <-- I will be dragged out

- wt.exe (Terminal Instance B)
  |- conhost.exe (in PTY mode) - Hosted to B 
     |- cmd.exe 
```

When the `pwsh.exe` tab is torn off from Instance A and is dropped onto Instance B, the process hierarchy doesn't actually change. The connection details, preferences, and session metadata are passed via the IPC management channels, but to an outside observer, nothing has actually changed.

```
- wt.exe (Terminal Instance A) 
  |- conhost.exe (in PTY mode) - Hosted to A
  |  |- cmd.exe
  |- conhost.exe (in PTY mode) - Hosted to B
     |- pwsh.exe <-- I am hosted in B but I'm parented to A

- wt.exe (Terminal Instance B)
  |- conhost.exe (in PTY mode) - Hosted to B 
     |- cmd.exe 
```

I don't believe there are provisions in the Windows OS to reparent applications to a different process.

Additionally, this becomes more interesting when Terminal Instance A dies and B is still running:

```
- conhost.exe (in PTY mode) - Hosted to B
  |- pwsh.exe <-- I am hosted in B but I'm parented to A

- wt.exe (Terminal Instance B)
  |- conhost.exe (in PTY mode) - Hosted to B 
     |- cmd.exe 
```

When instance A dies, the `conhost.exe` that was reparented keeps running and now just appears orphaned within the process hierarchy, reporting to the top level under utilities like Process Explorer.

I believe the action plan here would be to implement what we can, observe the state of the world, and correct going forward. We don't have a solid understanding of how many client applications might be impacted by this apparent change. It also might be perfectly OK because the client applications will always remain parented to the same `conhost.exe` even if those `conhost.exe`s don't report up to the correct `wt.exe`.

It is also unclear whether someone might want to write a utility from the outside to discover this hierarchy. I would be inclined to not provide a way to do this without a strong case otherwise because attempting to understand the local machine process hierarchy is a great way to box yourself in when attempting to expand later to encompass remote connections.

#### Hierarchy Example between Conhost and a Terminal (default application)

This looks very much like the previous section where Terminal Instance B died.

```
- conhost.exe (in PTY mode) - Hosted to A
  |- pwsh.exe

- wt.exe (Terminal Instance A)
```

The `conhost.exe` was started in response to a `pwsh.exe` being started with no host. It then put itself into PTY mode and launched into a connection of `wt.exe` instance A.

**ALTERNATIVELY**

```
- conhost.exe - idling
  
- wt.exe (Terminal Instance A)
  |- conhost.exe (in PTY mode)
     |- pwsh.exe
```

The `conhost.exe` at the top was launched in response to `pwsh.exe` being started with no host. It identified that `wt.exe` was running and instead shuttled the incoming connection into that `wt.exe`. `wt.exe` stood up the `conhost.exe` in PTY mode beneath itself and the client `pwsh.exe` call below that. The PTY mode `conhost.exe` uses its reparenting commands on startup to make the tree look like the above. The orphaned (originally started) `conhost.exe` waits until the connection exits before exiting itself in case someone was waiting on it.

### Performance, Power, and Efficiency

This is obviously less efficient than not doing it as we have to stand up servers and protocols and handlers for shuffling things about.

But as long as we're creating threads and services that sleep most of the time and are only awakened on some kernel/system event, we shouldn't be wasting too much in terms of power and background resources.

Additionally, `wt.exe` is worse than `conhost.exe` alone in all efficiency categories simply because it not only requires more resources to display in a "pretty" manner, but it also requires a `conhost.exe` under it in PTY mode to adapt the API calls. This is generally acceptable for end users who care more about the experience than the total performance. 

It is, however, not likely to be much if any worse than just choosing to use `wt.exe` anyway over `conhost.exe`.

## Potential Issues

I've listed most of the issues above in their individual sections. The primary highlights are:
1. Process tree layout - The processes in hierarchy may not make sense to someone inspecting them either visually with a tool or programmatically
1. Process and kernel object lifetime - Applications may be counting on a specific process or object lifetime in regards to their hosting window and we might be tampering with that in how we apply job objects or shuffle around ownership to make tabs happen
1. Default launch expectations - It is possible that test utilities or automation are counting on `conhost.exe` being the host application or that they're not ready to tolerate the potential for other applications to start. I think the interactive/non-interactive check mitigates this, but we'd have to remain concerned here.
1. `AttachConsole` and `DetachConsole` and `AllocConsole` - I don't have the slightest idea what happens for these APIs. We would have to explore. `AttachConsole` has restrictions based on the process hierarchy. It would likely behave in interesting ways with the strange parenting order and might be a driver to why we would have to adjust the parenting of the processes (or change the API under the hood). `DetachConsole` might create an issue where a tab disappears out of the terminal and the job object causes everything to die. `AttachConsole` wouldn't necessarily be guaranteed to go back into the same `wt.exe` or a `wt.exe` at all. 

## Future considerations

This might unlock some sort of isolation for extensions as well. Extensions of some sort our on our own long term roadmap, but they're inherently risky to the stability and integrity of the application. If we have to go through a lot of gyrations to enable process containerization and an interprocess communication model for tab tear off and default application work, we might also be able to contain extensions the same way. This derives further from the idea of what browsers do.

## Resources

- [Manager Prototype](https://github.com/microsoft/terminal/blob/dev/miniksa/manager/src/types/Manager.cpp)
- [Pipe Server documentation](https://docs.microsoft.com/en-us/windows/win32/ipc/multithreaded-pipe-server)
- [OLE Drag and Drop](https://docs.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-registerdragdrop)
- [OpenProcess](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocess)
- [DuplicateHandle](https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle)
