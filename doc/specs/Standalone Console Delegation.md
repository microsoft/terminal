---
author: leecher1337 @leecher1337
created on: 2026-09-05
last updated: 2026-09-05
issue id: n/a
---

# Standalone Console Delegation

## Abstract

This adds a way for a third-party `DelegationConsole` implementation
(registered the same way Windows Terminal registers itself as the delegated
console/terminal handler, per [`doc/specs/#492 - Default Terminal/spec.md`])
to handle a handed-off console session as a genuine, native window - the same
codepath a plain, non-delegated `OpenConsole.exe`/`conhost.exe` launch already
uses - instead of being required to hand off again to a `DelegationTerminal`
over ConPTY.

Concretely: a new sentinel CLSID, `DelegationConfig::CLSID_Standalone`, can be
used as the `DelegationTerminal` value (never `DelegationConsole`) in
`HKCU\Console\%%Startup`. When `ConsoleEstablishHandoff` (`srvinit.cpp`) sees
this as the resolved terminal, it stays a plain windowed host instead of
`CoCreateInstance`-ing a Terminal.

## Inspiration

The existing delegation mechanism (`DelegationConsole`/`DelegationTerminal`,
`CConsoleHandoff`, `ConsoleEstablishHandoff`) was designed for exactly one
purpose: routing a console session to a Terminal UI over ConPTY. That's the
right choice for the overwhelming majority of console applications, which
only need a VT text stream.

Some applications need more than that: direct, pixel-level control of their
own console window (GDI/Direct2D/Direct3D), not just text over a pty. A
ConPTY connection cannot carry that - there is no VT representation for raw
pixel data. Today, any such application is locked out of delegation entirely;
its only option is to be handed a legacy, non-delegated window, which means a
third-party console host can never legitimately take over these sessions the
way it can for ordinary text-mode ones.

A concrete example driving this: a companion change resurrects
`CONSOLE_GRAPHICS_BUFFER` support (see microsoft/terminal#246, "Bringing back
the console graphics screen-buffers?"), which lets a client render a
pixel-addressable buffer directly to its console window - historically used by
NTVDM to display DOS graphics-mode output, but usable by any client
(#246 itself notes Far Manager plugins wanting to show image thumbnails
in-console as another example). Once graphics buffers exist at all, a
third-party console host that implements them needs a way to actually be the
one hosting a session where they're used, without an intervening ConPTY hop
that would silently discard the pixel data. But the mechanism this spec adds
is independent of that one use case: any application needing a real,
natively-drawable console window - not just graphics buffers - can use it.

## Solution Design

- `DelegationConfig::CLSID_Standalone` (`propslib/DelegationConfig.hpp`): a
  fixed, well-known sentinel CLSID, analogous to `CLSID_Default`/
  `CLSID_Conhost`. It is never resolved via `CoCreateInstance` - the in-box
  console host's own `attemptHandoff` (`server/IoDispatchers.cpp`) only ever
  activates the `DelegationConsole` side, so a `DelegationTerminal` value
  never needs to be a real, activatable class.
- `ConsoleEstablishHandoff` (`host/srvinit.cpp`): once activated as
  `DelegationConsole` and handed a session, checks whether the resolved
  `DelegationTerminal` is `CLSID_Standalone`. If so, it skips the
  `CoCreateInstance`/`EstablishPtyHandoff` sequence entirely and calls
  `ConsoleCreateIoThread` directly, reusing the `Server`/`driverInputEvent`/
  `connectMessage` the in-box host already established - identical to how a
  normal, non-handoff launch reaches the same function
  (`ConsoleCreateIoThreadLegacy`), just with those already provided instead of
  freshly created.

No changes are needed to `CConsoleHandoff`/the COM activation path itself -
this only affects what happens once a session has already been handed off.

### Registration

Enabling this for a given console host build is two per-user registry writes
(no elevation, no protected files):

- `HKCU\Software\Classes\CLSID\{<the host's own CConsoleHandoff CLSID>}\LocalServer32`
  → path to the host's `.exe`.
- `HKCU\Console\%%Startup`:
  - `DelegationConsole` = that CLSID
  - `DelegationTerminal` = `{5C2A1F8E-8E3B-4A2B-9B4C-2F6A1D7E3C0A}` (`CLSID_Standalone`)

An `AppID` association for the console's CLSID is also recommended (see
Potential Issues below).

**Important**: a build's `CConsoleHandoff` CLSID must not collide with an
existing installed console/terminal's own CLSID. An installed Windows
Terminal package can register COM servers for all four `WT_BRANDING_*`
CLSIDs via registration-free MSIX COM, which takes precedence over a plain
per-user `HKCU\Software\Classes` registration for the same CLSID - any
third-party build sharing one of those four will silently resolve to
Microsoft's own `OpenConsole.exe` instead. Third-party builds should use a
CLSID of their own.

## UI/UX Design

None directly - this has no user-facing UI of its own. It's consumed the same
way `DelegationConsole`/`DelegationTerminal` already are: a value in the
registry, currently set by hand or by whatever installer a third-party
console host provides. (Whether/how this should ever be exposed in Windows
Terminal's own "Default Terminal Application" picker, alongside the existing
"Let Windows decide"/"Windows Terminal"/"Windows Console Host" options, is an
open question - see Future considerations.)

## Capabilities

### Accessibility

No impact - the resulting window goes through the exact same
`InitWindowsSubsystem`/`AccessibilityNotifier` path a normal, non-delegated
launch already uses.

### Security

No new attack surface: this only changes what an *already-activated*
`DelegationConsole` does with a session it has already been handed. The
`CoCreateInstance`/`LocalServer32`/`AppID` security model that gates
activation in the first place is completely unchanged.

### Reliability

One real gap was found and needs a documented workaround: `EndTask()`
(`ConsoleControl(ConsoleEndTask, ...)`, called from `Ctrl+Close` handling in
`input.cpp`'s `ProcessCtrlEvents`) relies on CSRSS recognizing the calling
process as the legitimate console host for its target process. That
recognition is never established for a handoff-received session - every
existing handoff target is headless ConPTY, which never exercises this
native-window `Ctrl+Close` path at all, so this gap was never hit before.
`EndTask()` reports success but has no actual effect: the attached client is
never signaled, and the window would never close on its own.

Since this is undocumented CSRSS behavior with no public API to fix
correctly, `ProcessCtrlEvents` falls back to terminating the attached
process(es) directly via `TerminateProcess()` when handling
`CTRL_CLOSE_EVENT` for a handoff-received session specifically
(`ServiceLocator::LocateGlobals().handoffTarget`). This is scoped tightly
enough that it cannot affect a normal, non-handoff launch, which continues to
close via `EndTask()` exactly as before.

### Compatibility

Purely additive. `DelegationTerminal` values other than `CLSID_Standalone`
behave identically to today; nothing changes for the existing
"Let Windows decide"/`ConhostDelegationPair`/Terminal-delegation paths.

### Performance, Power, and Efficiency

No measurable impact - this is a one-time branch taken during session
handoff, not a hot path.

## Potential Issues

- The `EndTask()`/CSRSS gap above is a real, if narrow, correctness gap in
  the existing (unmodified) delegation machinery, only now visible because
  this is the first scenario to combine a handoff-received session with a
  real native window. The `TerminateProcess()` fallback means an attached
  client never gets a chance to react to `CTRL_CLOSE_EVENT` gracefully in
  this specific scenario (unlike a normal launch) - acceptable given the
  alternative is a console window that can never be closed at all, but worth
  a proper fix if CSRSS's ownership-recognition behavior for handoff-received
  sessions is ever better understood or documented.
- Per-user `LocalServer32` COM registrations without an explicit `AppID`
  can see inconsistent/high-latency `CoCreateInstance(CLSCTX_LOCAL_SERVER)`
  activation on some systems. Registering an `AppID` for the console's CLSID
  and granting explicit Local Launch/Local Activation permission via
  Component Services is recommended.

## Future considerations

- A well-known, standalone-capable third-party console host could
  eventually be surfaced in Windows Terminal's own "Default Terminal
  Application" settings UI, the same way installed Terminal-family packages
  already are (`DelegationConfig::s_GetAvailablePackages`), rather than
  requiring hand-written registry entries.
- Any application needing genuine pixel/window-level console access - not
  just the graphics-buffer use case that motivated this - can build on this
  mechanism.

## Resources

- microsoft/terminal#246 - "Bringing back the console graphics
  screen-buffers?" (the companion change this spec was motivated by)
- `src/host/exe/CConsoleHandoff.cpp`/`.h`
- `src/host/srvinit.cpp` (`ConsoleEstablishHandoff`)
- `src/server/IoDispatchers.cpp` (`attemptHandoff`)
- `src/propslib/DelegationConfig.hpp`/`.cpp`
