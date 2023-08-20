---
author: Michael Niksa @miniksa
created on: 2020-08-14
last updated: 2022-01-13
issue id: #492
---

# Default Terminal Choice in Windows OS

## Abstract

Since the beginning, Windows has offered a single choice in default terminal hosting behavior. Specifically, the default terminal is defined as the one that the operating system will start on your behalf when a command-line application is started without a terminal attached. This specification intends to detail how we will offer customers the ultimate in choice among first and third party replacements for their default terminal experience.

## Inspiration

We've had a lot of success in the past several years on our terminal team journey. We updated the old console host user interface with long-desired features. We updated the console environment to bring Windows closer to Linux and Mac by implementing the client (receiving) end of Virtual Terminal sequences to unlock WSL, Docker, and other cross-platform command-line application compatibility. We then created the ConPTY to expose the server end of the console environment to first and third party applications to enable the hosting of any of those command-line clients within their own user interfaces by implementing the server (sending) end of Virtual Terminal sequences. And then we built Windows Terminal as our flagship implementation of the development environment on this model.

Through all of this, the entrypoint for alternatives to the console host UX continued to be "Start your alternative terminal implementation first, then start the command-line application inside." For those familiar with Linux and Mac or for those using the broad ecosystem of alternative Windows Terminals like ConEmu, Cmder, Console2, and the like... that was natural. But Windows did it differently a long time ago allowing the starting of a command-line application directly from the shell or kernel without a terminal specified. On noticing the missing terminal, the system would just-in-time start and attach the one terminal it could count on as always present, `conhost.exe`.

And so the inspiration of this is simple: We want to allow our customers to choose whichever terminal they want as the just-in-time terminal attached to an application without one present/specified on launch. This final move completes our journey to allow the ultimate in choice AND decouple the terminal experience from the operating system release schedule.

## Solution Design

There are three components to the proposed design:

1. **Inbox console**: This is the `conhost.exe` that is resident inside every Windows installation.
1. **Updated console**: This is the `openconsole.exe` that we ship with the Windows Terminal to provide a more up-to-date console server experience.
1. **Terminal UX**: This is `WindowsTerminal.exe`, the new Terminal user interface that runs on VT sequences.

And there are a few scenarios here to consider:

1. Replacement console API server and replacement terminal UX.
    1. This is the Windows Terminal scenario today. `OpenConsole.exe` is packed in the package to be the console API server and ConPTY environment for `WindowsTerminal.exe`.
1. Replacement console API server and legacy terminal UX.
    1. We don't explicitly distribute this today, but it's technically possible to just run `OpenConsole.exe` to accomplish this.
1. Inbox console API server and replacement terminal UX.
    1. The WSL environment does this when doing Windows interop and I believe VS Code does this too when told to use the ConPTY environment. (And since VS Code does it, anything using node-pty also does it, covering some 3rd party terminals as well).
1. Inbox console API server and inbox terminal UX.
    1. This is what we have today in `conhost.exe` running as the default application.

The goal is to offer the ultimate in choice here where any of the components can be replaced as necessary for a 1st or 3rd party scenario.

### Overview

#### Inbox console

The inbox console will be updated to support delegation of the incoming console client application connection to another console API server if one is registered and available.

We leave the inbox console in-place and always available on the operating system for these reasons:

1. A last chance fall-back should any of the delegation operations fail
1. An ongoing host for applications that aren't going to need a window at all
1. Continued support of our legacy `conhostv1.dll` environment, if chosen

The general operation is as follows:

- A command-line client application is started (from the start menu, run box, or any other `CreateProcess` or `ShellExecute` route) without an existing console server attached
- The inbox console is launched from `C:\windows\system32\conhost.exe` as always by the initialization routines inside `kernelbase.dll`.
- The inbox console accepts the incoming initial connection and looks for the `ShowWindow` information on the connection packet, as received from the kernel's process creation routines based on the parameters given to the `CreateProcess` call. (See [CREATE_NO_WINDOW](https://docs.microsoft.com/en-us/windows/win32/procthread/process-creation-flags) flag for details.)
- If the session is about to create a window, check for registration of a delegated/updated console and hand-off to it if it exists.
- Otherwise, start normally.

This workflow affords us several benefits:

- The only inbox component we have to change is `conhost.exe`, the one we already regularly update from open source on a regular basis. There is no change to the `kernelbase.dll` console initialization routines, `conclnt.lib` communication protocol, nor the `condrv.sys` driver.
- We should be able to make this change quickly, relatively easily, and the code delta should be relatively small
    - This makes it easy to squeeze in early in the development of the solution and get it into the Windows OS product as soon as possible for self-hosting, validation, and potentially shipping in the OS before the remainder of the solution has shaken out
    - This also makes it potentially possible to backport this portion of the code change to popular in-market versions of Windows 10. For instance, WSL2 has just backported to 1903 and 1909. The less churn and risk, the easier it is to sell a backport.

*Potential future:*
- ~~If no updated console exists, potentially check for registration of a terminal UX that is willing to use the inbox ConPTY bits, start it, and transition to being a PTY instead.~~ 
- **CUT FROM v1**: To simplify the story for end-users, we're offering this as a package deal in the first revision. Explaining the difference between consoles and terminals to end users is very difficult.

The registration would operate as follows:
- A registry key in `HKCU\Console\%%Startup` (format `REG_SZ`, name `DelegationConsole`) would specify ~~the path to ~~the replacement console that would be used to service the remainder of the connection process.
    - Alternatively or additionally, this same `REG_SZ` could list a COM server ID that could be looked up in the classes root and invoked. **V1 NOTE:** This was what was done.
        - Packaged applications and classic applications can easily register a COM server
        - WinRT libraries should be able to be easily registered as the COM server as well (given WinRT is COM underneath)
            - WinRT cannot be exposed outside of the package context itself, so the `conhost.exe` that is in the OS and is naturally outside the package cannot find it.
    - **V1 NOTE:** The subkey `%%Startup` was chosen to separate these keys (this one and the `DelegationTerminal` one below) in case we needed to ACL them or protect them in some way. We want a per-user choice of which Terminal/Console are used, but we might need to take action to prevent these keys from being slammed at some point in the future. Why `%%`? The subkeys are traditionally used to resolve paths to client binaries that have their own console preferences set. The `%%` should never be resolvable as it won't lead to a valid path or expanded path variable.

The delegation process would operate as follows:
- A method contract is established between the existing inbox console and any updated console (an interface). 
    - `HRESULT ConsoleEstablishHandoff(HANDLE server, HANDLE driverInputEvent, const PortableConnectMessage* const msg, HANDLE signalPipe, HANDLE inboxProcess, HANDLE* process)`
        - `HANDLE server`: This is the server side handle to the console driver, used with `DeviceIoControl` to receive/send messages with the client command-line application
        - `HANDLE driverInputEvent`: The input event is created and assigned to the driver immediately on first connection, before any messages are read from the driver, to ensure that it can track a blocking state should first message be an input request that we do not yet have data to fill. As such, the inbox console will have created this and assigned it to the driver before pulling off the connection packet and determining that it wants to delegate. Therefore, we will transfer ownership of this event to the updated console.
        - ~~`const PortableArguments* const args`: This contains the startup argument information that was passed in when the process was started including the original command line and the in/out handles.~~
            - ~~The `ConsoleArguments` structure could technically change between versions, so we will make a version agnostic portable structure that just carries the communication from the old one to the new one.~~
            - **CUT FROM V1**: The only arguments coming in from a default light-up are the server handle. Pretty much all the other arguments are related to the operation of the PTY. Since this feature is about "default application" launches where no arguments are specified, this was cut from the initial revision.
        - `const PortableConnectMessage* const msg`:
            - The `CONSOLE_API_MSG` structure contains both the actual packet data from the driver as well as some overhead/administration data related to the packet state, ordering, completion, errors, and buffers. It's a broad scope structure for every type of message we process and it can change over time as we improve the way the `conserver.lib` handles packets.
            - This represents a version agnostic variant for ONLY the connect message that can pass along the initial connect information structure, the packet sequencing information, and other relevant payload only to that one message type. It will purposefully discard references to things like a specific set of API servicing routines because the point of handing off is to get updated routines, if necessary.
            - **V1 NOTE:** This was named `CONSOLE_PORTABLE_ATTACH_MSG`
        - `HANDLE signalPipe`: During authoring, it was identified that <kbd>Ctrl+C</kbd> and other similar signals need to make it back to the original `conhost.exe` application as the Operating System grants it special privilege over the originally attached client application. This privilege cannot be transferred to the delegated console. So this channel remains for the delegated one to send its signals back through the original one for commanding the underlying client. (This also implies the original `conhost.exe` inbox cannot close and must remain a part of the process tree for the life of the session to maintain this control.)
        - `HANDLE inboxProcess`: Since we have to keep the inbox `conhost.exe` running for signal/ownership reasons, we also need to track its lifetime. If it disappears for whatever reason, we need to tear down the entire chain as part of our operation has been compromised.
        - `HANDLE* process`: On the contrary to `inboxProcess`, we need to give our process handle back so it can also be tracked. After the inbox console delegates, it remains in a very limited capacity. If the delegation one disappears, the session will no longer function and needs to be torn down (and the client closed).
        - *Return* `HRESULT`: This is one of the older style methods in the initialization. We moved them from mostly `NTSTATUS` to mostly `HRESULT` a while ago to take advantage of `wil`. This one will continue to follow the pattern and not move to exceptions. A return of `S_OK` will symbolize that the handoff worked and the inbox console can clean itself up and stop handling the session.
- When the connection packet is parsed for visibility information (see `srvinit.cpp`), we will attempt to resolve the registered handoff and call it.
    - ~~In the initial revision here, I have this as a `LoadLibrary`/`GetProcAddress` to the above exported contract method from the updated console. This maintains the server session in the same process space and avoids:~~
        1. ~~The issue of passing the server, event, and other handles into another process space. We're not entirely sure if the console driver will happily accept these things moving to a different process. It probably should, but unconfirmed.~~
        1. ~~Some command-line client applications rely on spelunking the process tree to figure out who is their servicing application. Maintaining the delegated/updated console inside the same process space maintains some level of continuity for these sorts of applications.~~
    - **Alternative:** We may make this just be a COM server/client contract. ~~An in-proc COM server should operate in much the same fashion here (loading the DLL into the process and running particular method) while being significantly more formal and customizable (version revisions, moving to out-of-proc, not really needing to know the binary path because the catalog knows).~~
        - **V1 NOTE:** We landed on an out-of-proc COM server/client here. This maintains the isolation of the newly running code from the old code. Since we're maintaining the original `conhost.exe` for signaling purposes, we're no longer worried about the spelunking the process tree and not having the relationship for clients to find.
    - **Not considering:** ~~WinRT. `conhost.exe` has no WinRT. Adding WinRT to it would significantly increase the complexity of compilation in the inbox and out of box code base. It would also significantly increase the compilation time, binary size, library link list, etc... unless we use just the ABI to access it. But I don't see an advantage to that over just using classic COM at that point. This is only one handoff method and a rather simplistic one at that. Every benefit WinRT provides is outweighed by the extra effort that would be required over just a classic COM server in this case.~~
- After delegation is complete, the inbox console will have to clean up any threads, handles, and state related to the session. We do a fairly good job with this normally, but some portions of the `conhost.exe` codebase are reliant on the process exiting for final cleanup. There may be a bit of extra effort to do some explicit cleanup here.
    - **V1 NOTE:** The inbox one cleans up everything it can and sits in a state waiting for the child/delegated process handle to exit. It also maintains a thread listening for the signals to come through in case it needs to send a command to the client application using the privilege granted to it by the driver.

#### Updated console

The updated replacement console will have the same console API server capabilities as the inbox console, but will be a later, updated, or customized-to-the-scenario version of the API server generally revolving around improving ConPTY support for a Terminal application.

On receiving the handoff from the method signature listed above, the updated console will:
- Establish its own set of IO threading, device communication infrastructure, and API messaging routines while storing the handles given
- ~~Re-parse the command line arguments, if necessary, and store them for guiding the remainder of launch~~
- Dispatch the attach message as if it were received normally
- Continue execution from there

There will then either be a registration for a Terminal UX to take over the session by using ConPTY, ~~or the updated console will choose to launch its potentially updated version of the `conhost` UX~~.

For registration, we repeat the dance above with another key:
- A registry key in `HKCU\Console\%%Startup` (format `REG_SZ`, name `DelegationTerminal`).

The delegation repeats the same dance as above as well:
- A contract (interface) is established between the updated console and the terminal
    - `HRESULT EstablishPtyHandoff(HANDLE in, HANDLE out, HANDLE signal, HANDLE ref, HANDLE server, HANDLE client)`
        - `HANDLE in`: The handle to read client application output from the ConPTY and display on the Terminal
        - `HANDLE out`: The handle to write user input from the Terminal to the ConPTY
        - `HANDLE signal`: The signal handle for the ConPTY for out-of-band communication between PTY server and Terminal application
        - ~~`COORD size`: The initial window size from the starting application, as it can be a preference in the connection structure. (A resize message may get sent back downward almost immediately from the Terminal as its dimensions could be different.)~~ **V1 NOTE:** This proved unnecessary as the resize operations sorted themselves out naturally.
        - `HANDLE ref`: This is a "client reference handle" to the console driver and session. We hold onto a copy of this in the Terminal so the session will stay alive until we let go. (The other console hosts in the chain also hold one of these, as should the client.)
        - `HANDLE server`: This is a process handle to the PTY we're attached to. We monitor this to know when the PTY is still alive from the Terminal side.
        - `HANDLE client`: This is a process handle to the underlying client application. The terminal tracks this for exit handling.
    - **Alternative:** This should likely just be a COM server/client contract as well. This would be consistent with the above and wouldn't require argument parsing or wink/nudge understanding of standard handle passing. It also conveys the same COM flexibility as described in the inbox console section. **V1 NOTE:** We used this alternative. We used COM, not a well-known exported function from the prototype.
- The contract is called and on success, responsibility of the UX is given over to the Terminal. The console sits in PTY mode.
    - On failure, the console launches interactive.

#### Terminal UX

The terminal will be its own complete presentation and input solution on top of a ConPTY connection, separating the concerns between API servicing and the user experience.

Today the Terminal knows how to start and then launches a ConPTY under it. The Terminal will need to be updated to accept a preexisting ConPTY connection on launch (or when the multi-process model arrives, as an inbound connection), and connect that to a new tab/pane instead of using the `winconpty.lib` libraries to make its own.

For now, I'm considering only the fresh-start scenario.
- The Terminal will have to detect the inbound connection through ~~its argument parsing (or through~~ a new entrypoint in the COM alternative ~~)~~ and store the PTY in/out/signal handles for that connection in the startup arguments information
- When the control is instantiated on a new tab, that initial creation where normally the "default profile" is launched will instead have to place the PTY in/out/signal handles already received into the `ConPtyConnection` object and use that as if it was already created.
- The Terminal can then let things run normally and the connection will come through and be hosted inside the session.

There are several issues/concerns:
- Which profile/settings get loaded? We don't really know anything about the client that is coming in already-established. That makes it difficult to know what user preferences to apply to the inbound tab. We could:
    - Use only the defaults for the incoming connection. Do not apply any profile-specific settings.
    - Use the profile information from the default profile to some degree. This could cause some weird scenarios/mismatches if that profile has a particular icon or a color scheme that makes it recognizable to the user.
    - Create some sort of "inbound profile" profile that is used for incoming connections
    - Add a heuristic that attempts to match the name/path of the connecting client binary to a profile that exists and use those settings, falling back if one is not found.
    - **Proposal:** Do the first one immediately for bootstrapping, then investigate the others as a revision going forward.
- The handles that are coming in are "raw" and "unpacked", not in the nice opaque `HPCON` structure that is usually provided to the `ConPtyConnection` object by the `winconpty.lib`.
    - Add methods to `winconpty.lib` that allow for the packing of incoming raw handles into the `HPCON` structure so the rest of the lifetime can be treated the same
    - Put the entrypoint for the COM server (or delegate the entrypoint for an argument) directly into this library so it can pack them up right away and hand of a ready-made `HPCON`.

## UI/UX Design

The user experience for this feature overall should be:

1. The user launches a command-line client application through the Start Menu, Win+X menu, the Windows Explorer, the Run Dialog box (WinKey+R), or through another existing Windows application.
1. Using the established settings, the console system transparently starts, delegates itself to the updated console, switches itself into ConPTY mode, and a copy of Windows Terminal launches with the first tab open to host the command-line client application.
    - **NOTE:** I'm not precluding 3rd party registrations of either the delegation updated console nor the delegation terminal. It is our intention to allow either or both of these pieces to be replaced to the user's desires. The example is for brevity of our golden path and motivation for this scenario.
1. The user is able to interact with the command-line client application as they would with the original console host.
    - The user receives the additional benefit that short-running executions of a command-line application may not "blink in and disappear" as they do today when a user runs something like `ipconfig` from the run dialog. The Terminal's default states tend to leave the tab open and say that the client has exited. This would allow a Run Dialog `ipconfig` user an improved experience over the default console host state of disappearing quickly.
1. If any portion of the delegation fails, we will progressively degrade back to a `conhost` style Win32+GDI UX and nothing will be different from before.

The settings experience includes:
- Configuration of the delegation operations:
    - Locations:
        - With the registry
            - This is what's going to be available first and will remain available. We will progress to some or all of the below after.
            - We will need to potentially add specifications to this to both the default profile (for new installations of Windows) or to upgrade/migration profiles (for users coming from previous editions of Windows) to enable the delegation process, especially if we put a copy of Windows Terminal directly into the box.
            - **V1 NOTE:** we didn't add additional migration logic here as `HKCU\Console*` and subkeys were already in the migration logic, so adding another should just carry along.
        - Inside Windows Terminal
            - Inside the new Settings UI, we will likely need a page that configures the delegation keys in `HKCU\Console\%%Startup` ~~or a link out to the Windows Settings panel, should we manage to get the settings configurable there~~.
        - Inside the console property sheet
            - Same as for Terminal but with `comctl` controls over XAML +/- a link to the Windows Settings panel
        - Inside the Settings panel for Windows (probably on the developer settings page)
            - The ultimate location for this is likely a panel directly inside Windows. This is the hardest one to accomplish because of the timelines of the Windows product. We may not get this in an initial revision, but it should likely be our ultimate goal. **V1 NOTE:** We did it!
    - Operation:
        - Specify paths/server IDs - This is the initial revision
        - Offer a list of registered servers or discovered manifests from the app catalog - This is the ideal scenario where we search the installed app catalog +/- the COM catalog and offer a list of apps that conform to the contract in a drop-down.
            - The final process was to use [App Extensions](https://docs.microsoft.com/windows/uwp/launch-resume/how-to-create-an-extension) inside the Terminal APPX package to declare the COM GUIDs that were available for the `DelegationConsole` and `DelegationTerminal` fields respectively. A configuration class `DelegationConfig` was added to `propslib.lib` that enables the lookup of these from the application state catalog and presents a list of them to choose from. It also manages reading and writing the registry keys.
            - **V1 NOTE:** Our configuration options currently allow pairings of replacement consoles and terminals to be adjusted in lock-step from the UI. That's not to say further combinations are not possible or even necessarily inhibited by the code. We just went for minimal confusion in our first round.
    
- Configuration of the legacy console state:
    - ~~Since we could end up in an experience where the default launch experience gets you directly into Windows Terminal, we believe that the Terminal will likely need an additional setting or settings in the new Settings UI that will allow the toggling of some of the `HKCU\Console` values to do things like set/remove the legacy console state.~~ **V1 NOTE:** Cut as low priority. Switch back to console and configure it that way or use the existing property sheet or tamper with registry keys.
    - We have left the per-launch debugging and advanced access hole of calling something like `conhost.exe cmd.exe` which will use the inbox conhost to launch `cmd.exe` even if there is a default specified.

Concerns:
- State separation policy for Windows. I believe `HKCU\Console` is already specified as a part of the "user state" that should be mutable and carried forward on OS Swap, especially as we have been improving the OS swap experience.
- Ability for installers/elevated scripts to stomp the Delegation keys
    - This was a long time problem for default app registrations and was limited in our OS. Are we about to run down the same path?
    - What is the alternative here? To use a protocol handler? To store this configuration state data with other protected state in a registry area that is mutable, but only ACL'd to the `SYSTEM` user like some other things in the Settings control panel?
        - **V1 NOTE:** We set ourselves up for some future ACL thing with the subkey, but we otherwise haven't enforced anything at this time.

## Capabilities

### Accessibility

Accessibility applications are the most likely to resort to a method of spelunking the process tree or window handles to attempt to find content to read out. Presuming they have hard-coded rules for console-type applications, these algorithms could be surprised by the substitution of another terminal environment.

The major players here that I am considering are NVDA, JAWS, and Narrator. As far as I am aware, all of these applications attempt to drive their interactivity through UI Automation where possible. And we have worked with all of these applications in the past in improving their support for both `conhost.exe` and the Windows Terminal product. I have relatively high confidence that we will be able to work with them again to help update these assistive products to understand the new UI delegation, if necessary.

### Security

Let's hit the elephant in the room. "You plan on pulling a completely different binary inside the `conhost.exe` process and just... delegating all activity to it?" Yes.

(**V1 NOTE:** Well, it's out of proc now. But it is at the same privilege level as the original one thanks to the mechanics of COM.) 

As far as I'm concerned, the `conhost.exe` that is started to host the command-line client application is running at the same integrity level as the client binary that is partially started and waiting for its server to be ready. This is the long-standing existing protection that we have from the Windows operating system. Anything running in the same integrity level is already expected to be able to tamper with anything else at the same integrity level. The delegated binary that we would be loading into our process space will also be at the same integrity level. Nothing really stops a malicious actor from launching that binary in any other way in the same integrity level as a part of the command-line client application's startup. 

The mitigation here, if necessary, would be to use `WinVerifyTrust` to validate the certification path of the `OpenConsole.exe` binary to ensure that only one that is signed by Microsoft can be the substitute server host for the application. This doesn't stop third parties from redistributing our `OpenConsole.exe` off of GitHub if necessary with their products, but it would stop someone from introducing any random binary that met the signature interface of the delegation methods into `conhost.exe`. The only value I see this providing is stopping someone from being "tricked" into delegating their `conhost.exe` to another binary through the configuration methods we provide. It doesn't really stop someone (or an attacker) from taking ownership of the `conhost.exe` in System32 and replacing it directly. So this point might be moot. (It is expected that replacement of the System32 one is already protected, to some degree, by being owned by the SYSTEM account and requiring some measure of authority to replace.)

### Reliability

The change on its own may honestly improve reliability of the hosting system. The existing just-in-time startup of the console host application only had a single chance at initializing a user experience before it would give up and return that the command-line application could not be started.

However, there are now several phases in the startup process that will have the opportunity to make multiple attempts at multiple versions or applications to find a suitable host for the starting application before giving up.

One layer of this is where the `conhost.exe` baked into the operating system will be on the lookout for an `OpenConsole.exe` that will replace its server activities. The delegation binary loses a bit of reliability, theoretically, by the fact that loading another process during launch could have versioning/resolution/path/dependency issues, but it simultaneously offers us the opportunity for improved reliability by being able to service that binary quickly outside the Windows OS release cycle. Fixes can arrive in days instead of months to years.

Another layer of this is where either `conhost.exe` or the delegated `OpenConsole.exe` server will search for a terminal user experience host, like `WindowsTerminal.exe` or another registered first or third party host, and split the responsibility of hosting the session with that binary. Again, there's a theoretical reliability loss with the additional process launch/load, but there's much to be gained by reducing the scope of what each binary must accomplish. Removing the need to handle user interaction from `conhost.exe` or `OpenConsole.exe` and delegating those activities means there is less surface area running and less chance for a UX interaction to interfere with API call servicing and vice versa. And again, having the delegation to external components means that they can be fixed on a timeline of days instead of months or years as when baked into the operating system.

### Compatibility

One particular scenario that this could break is an application that makes use of spelunking the process tree when a command-line application starts to identify the hosting terminal application window by HWND to inject input, extract output, or otherwise hook and bind to hosting services. As the default application UI that will launch may not have the `conhost.exe` name (for spelunking via searching processes) and the HWND located may either be the ConPTY fake HWND or an HWND belonging to a completely different UI, these applications might not work.

Two considerations here:

1. At a minimum, we must offer an opt-out of the delegation to another terminal for the default application.
1. We may also want to offer a process-name, policy, manifest, or other per client application opt-out mechanism.

**V1 NOTE:** There is no per-client specific way of doing this. The toggle is per-user and can be adjusted in 3 different places.

### Performance, Power, and Efficiency

I expect to take some degree of performance, power, and efficiency hit by implementing this replacement default app scenario just by it's nature. We will be loading multiple processes, performing tests and branches during startup, and we will likely need to load COM/WinRT and packaging data that was not loaded prior to resolve the final state of default application load. I would expect this to accrue to some failures in the performance and power gates inside the Windows product. Additionally, the efficiency of running pretty much everything through the ConPTY is lower than just rendering it directly to `conhost.exe`'s embedded GDI-powered UI itself thanks to the multiple levels of translation and parsing that occur in this scenario.

The mitigations to these losses are as follows:

1. We will delay load any of the interface load and packaging data lookup libraries to only be pulled into process space should we determine that the application is non-interactive. 
    1. That should save us some of the commit and power costs for the sorts of non-interactive scripts and applications that typically run early in OS startup (and leverage `conhost.exe` as their host environment). 
    1. We will still likely get hit with the on-disk commit cost for the additional export libraries linked as well as additional code. That would be a by-design change.

1. We plan to begin Profile Guided Optimization across our `OpenConsole.exe` and `WindowsTerminal.exe` binaries. This should allow us to optimize the startup paths for this scenario and bias the `OpenConsole.exe` binary that we redistribute to focus its efforts and efficiency on the ConPTY role specifically, ignoring all of the interactive Win32/GDI portions that aren't typically used.
    1. We may need to add a PGO scenario inside Windows to tune the optimization of `conhost.exe` especially if we're going to go full on Windows Terminal in the box default application. The existing PGO that occurs in the optimization branches is running on several `conhost.exe` interactive scenarios, none of which will be relevant here. We would probably want to update it to focus on the default app delegation routine AND on the non-interactive scenario for hosted applications (where delegation will not occur but Win32/GDI will still not be involved).

## Potential Issues

### Passing Handles with COM
COM doesn't inherently expose a way for us to pass handles directly between processes with the existing contracts. We know this is possible because Windows does it all the time, but it doesn't appear to be public. We believe the mission forward is to expose this functionality to the public as if it's good enough for us internally and it is a requirement to build complex functionality like this... then it should be good enough for the public.

**V1 NOTE:** We gained approval to open this up and documented it. [`system_handle` attribute](https://docs.microsoft.com/windows/win32/midl/system-handle). It didn't require any code changes because the public IDL compiler already recognized the existence of this attribute and did the correct thing. It just wasn't documented for use.

## Future considerations

* We additionally would like to leave the door open to distributing updated `OpenConsole.exe`s in their own app package as a dependency that others could rely on.
    * This was one of the original management requests when we were opening the source of the console product as well as the Terminal back in spring of 2019. For the sake of ongoing servicing and maintainability, it was requested that we reach a point where our dependencies could be serviced potentially independently of the product as a whole static unit. We didn't achieve that goal initially, but this design would enable us to do something like this.
    * One negative to this scenario is that dependency resolution and the installation of dependent packages through APPX is currently lacking in several ways. It's difficult/impossible to do in environments where the store or the internet is unavailable. And it's a problem often enough that the Windows Terminal package embeds the VC runtimes inside itself instead of relying on the dependency resolution of the app platform.

## Resources

- [Windows Terminal Process Model 2.0 spec](https://github.com/microsoft/terminal/pull/7240)
- [Windows Terminal 2.0 Process Model Improvements](https://github.com/microsoft/terminal/issues/5000)
- [Console allocation policy specifications](https://github.com/microsoft/terminal/pull/7337)
- [Fine-grained console allocation policy feature](https://github.com/microsoft/terminal/issues/7335)
