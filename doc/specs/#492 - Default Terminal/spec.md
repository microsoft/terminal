---
author: Michael Niksa @miniksa
created on: 2020-08-14
last updated: 2020-08-24
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

#### Updated console

The updated replacement console will have the same console API server capabilities as the inbox console, but will be a later, updated, or customized-to-the-scenario version of the API server generally revolving around improving ConPTY support for a Terminal application.

#### Terminal UX

The terminal will be its own complete presentation and input solution on top of a ConPTY connection, separating the concerns between API servicing and the user experience.

## UI/UX Design

[comment]: # What will this fix/feature look like? How will it affect the end user?

## Capabilities

### Accessibility

Accessibility applications are the most likely to resort to a method of spelunking the process tree or window handles to attempt to find content to read out. Presuming they have hardcoded rules for console-type applications, these algorithms could be surprised by the substitution of another terminal environment.

The major players here that I am considering are NVDA, JAWS, and Narrator. As far as I am aware, all of these applications attempt to drive their interactivity through UI Automation where possible. And we have worked with all of these applications in the past in improving their support for both `conhost.exe` and the Windows Terminal product. I have relatively high confidence that we will be able to work with them again to help update these assistive products to understand the new UI delegation, if necessary.

### Security

Let's hit the elephant in the room. "You plan on pulling a completely different binary inside the `conhost.exe` process and just... delegating all activity to it?" Yes. 

As far as I'm concerned, the `conhost.exe` that is started to host the command-line client application is running at the same integrity level as the client binary that is partially started and waiting for its server to be ready. This is the long-standing existing protection that we have from the Windows operating system. Anything running in the same integrity level is already expected to be able to tamper with anything else at the same integrity level. The delegated binary that we would be loading into our process space will also be at the same integrity level. Nothing really stops a malicious actor from launching that binary in any other way in the same integrity level as a part of the command-line client application's startup. 

The mitigation here, if necessary, would be to use `WinVerifyTrust` to validate the certification path of the `OpenConsole.exe` binary to ensure that only one that is signed by Microsoft can be the substitute server host for the application. This doesn't stop third parties from redistributing our `OpenConsole.exe` off of GitHub if necessary with their products, but it would stop someone from introducing any random binary that met the signature interface of the delegation methods into `conhost.exe`. The only value I see this providing is stopping someone from being "tricked" into delegating their `conhost.exe` to another binary through the configuration methods we provide. It doesn't really stop someone (or an attacker) from taking ownership of the `conhost.exe` in System32 and replacing it directly. So this point might be moot. (It is expected that replacement of the System32 one is already protected, to some degree, by being owned by the SYSTEM account and requiring some measure of authority to replace.)

### Reliability

The change on its own may honestly improve reliability of the hosting system. The existing just-in-time startup of the console host application only had a single chance at initializing a user experience before it would give up and return that the command-line application could not be started.

However, there are now several phases in the startup process that will have the opportunity to make multiple attempts at multiple versions or applications to find a suitable host for the starting application before giving up.

One layer of this is where the `conhost.exe` baked into the operating system will be on the lookout for an `OpenConsole.exe` that will replace its server activities. The delegation binary loses a bit of reliability, theoretically, by the fact that loading another process during launch could have versioning/resolution/path/dependency issues, but it simultaneously offers us the opportunity for improved reliability by being able to service that binary quickly outside the Windows OS release cycle. Fixes can arrive in days instead of months to years.

Another layer of this is where either `conhost.exe` or the delegated `OpenConsole.exe` server will search for a terminal user experience host, like `WindowsTerminal.exe` or another registered first or third party host, and split the responsibility of hosting the session with that binary. Again, there's a theoretical reliability loss with the additional process launch/load, but there's much to be gained by reducing the scope of what each binary must accomplish. Removing the need to handle user interaction from `conhost.exe` or `OpenConsole.exe` and delegating those activities means there is less surface area running and less chance for a UX interaction to interfere with API call servicing and vice versa. And again, having the delegation to external components means that they can be fixed on a timeline of days instead of months or years as when baked into the operating system.

### Compatibility

One particular scenario that this could break is an application that makes use of spelunking the process tree when a command-line application starts to identify the hosting terminal application window by HWND to inject input, extract output, or otherwise hook and bind to hosting services. As the default application UI that will launch may not have the `conhost.exe` name (for spelunking via searching processes) and the HWND located may either be the ConPTY fakeout HWND or an HWND belonging to a completely different UI, these applications might not work.

Two considerations here:

1. At a minimum, we must offer an opt-out of the delegation to another terminal for the default application.
1. We may also want to offer a process-name, policy, manifest, or other per client application opt-out mechanism.

### Performance, Power, and Efficiency

I expect to take some degree of performance, power, and efficiency hit by implementing this replacement default app scenario just by it's nature. We will be loading multiple processes, performing tests and branches during startup, and we will likely need to load COM/WinRT and packaging data that was not loaded prior to resolve the final state of default application load. I would expect this to accrue to some failures in the performance and power gates inside the Windows product. Additionally, the efficiency of running pretty much everything through the ConPTY is lower than just rendering it directly to `conhost.exe`'s embedded GDI-powered UI itself thanks to the multiple levels of translation and parsing that occur in this scenario.

The mitigations to these losses are as follows:

1. We will delay load any of the interface load and packaging data lookup libraries to only be pulled into process space should we determine that the application is non-interactive. 
    1. That should save us some of the commit and power costs for the sorts of non-interactive scripts and applications that typically run early in OS startup (and leverage `conhost.exe` as their host environment). 
    1. We will still likely get hit with the on-disk commit cost for the additional export libraries linked as well as additional code. That would be a by-design change.

1. We plan to begin Profile Guided Optimization across our `OpenConsole.exe` and `WindowsTerminal.exe` binaries. This should allow us to optimize the startup paths for this scenario and bias the `OpenConsole.exe` binary that we redistribute to focus its efforts and efficiency on the ConPTY role specifically, ignoring all of the interactive Win32/GDI portions that aren't typically used.
    1. We may need to add a PGO scenario inside Windows to tune the optimization of `conhost.exe` especially if we're going to go full on Windows Terminal in the box default application. The existing PGO that occurs in the optimization branches is running on several `conhost.exe` interactive scenarios, none of which will be relevant here. We would probably want to update it to focus on the default app delegation routine AND on the non-interactive scenario for hosted applications (where delegation will not occur but Win32/GDI will still not be involved).

## Potential Issues

[comment]: # What are some of the things that might cause problems with the fixes/features proposed? Consider how the user might be negatively impacted.

## Future considerations

[comment]: # What are some of the things that the fixes/features might unlock in the future? Does the implementation of this spec enable scenarios?

* We additionally would like to leave the door open to distributing updated `OpenConsole.exe`s in their own app package as a dependency that others could rely on.
    * This was one of the original management requests when we were open sourcing the console product as well as the Terminal back in spring of 2019. For the sake of ongoing servicing and maintainability, it was requested that we reach a point where our dependencies could be serviced potentially independently of the product as a whole static unit. We didn't achieve that goal initially, but this design would enable us to do something like this.
    * One negative to this scenario is that dependency resolution and the installation of dependent packages through APPX is currently lacking in several ways. It's difficult/impossible to do in environments where the store or the internet is unavailable. And it's a problem often enough that the Windows Terminal package embeds the VC runtimes inside itself instead of relying on the dependency resolution of the app platform.

## Resources

(link here to default app spec)
