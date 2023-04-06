---
author: Dustin L. Howett @DHowett
created on: 2023-03-22
last updated: 2023-03-22
issue id: none
---

# Windows Terminal "Portable" Mode

## Abstract

Since we are planning on officially supporting unpackaged execution, I propose a special mode where Terminal stores its
settings in a `settings` folder next to `WindowsTerminal.exe`.

## Inspiration

- [PortableApps](https://portableapps.com)
- "Embeddable" Python, which relies on the deployment of a specific file to the Python root

## Solution Design

- _If running without package identity,_ `CascadiaSettings` will look for the presence of a file called `.portable` next
  to `Microsoft.Terminal.Settings.Model.dll`.
- If that file is present, it will change the settings and state paths to be rooted in a subfolder named `settings` next
  to `Microsoft.Terminal.Settings.Model.dll`.

Right now, _the only thing_ that makes Terminal not work in a "portable" manner is that it saves settings to
`%LOCALAPPDATA%`.

## UI/UX Design

_No UI/UX impact is expected._

## Capabilities

- Distributors could ship a self-contained and preconfigured Terminal installation.
- Users could archive fully-working preconfigured versions of Terminal.
- Developers (such as those on the team) could easily test multiple versions of Terminal without worrying about global
  settings pollution.

### Accessibility

_No change is expected._

### Security

_No change is expected._

### Reliability

More code always bears a risk.

### Compatibility

This is a net new feature, and it does not break any existing features. A distributor (or a user) can opt in (or out) by
adding (or removing) the `.portable` file.

The following features may be impacted.

- **Dynamic Profiles** and **Fragment Extensions**
  - _No impact expected._ Dynamic profiles will still be generated. If a portable installation is moved to a machine without the dynamic profile source, that profile will disappear.
- `firstWindowPreference` and `state.json`
  - _No impact expected._
  - State is stored next to settings, even for portable installations.
  - If a dynamic profile was saved in `state` and has been removed, Terminal will proceed as in non-portable mode.
- Moving an install from Windows 10 to Windows 11 and back
  - _No impact expected._
- "Machine-specific" settings, like those about rendering and repainting
  - _No impact expected._
  - Terminal does not distinguish settings that are specific to a machine. These settings will move along with the portable install.
- The shell extension
  - _No impact expected._
  - The shell extension will not be registered with Windows.
  - If we choose to register the shell extension, it is already prepared for running a version of WT from the same directory. Registering the portable shell extension will make it launch portable Terminal.

### Performance, Power, and Efficiency

_No change is expected._

## Potential Issues

- User confusion around where settings are stored.

## Future considerations

- In the future, perhaps `.portable` could itself contain a directory path into which we would store settings.
- We could consider adding an indicator in the Settings UI.
- Because we are using the module path of the Settings Model DLL, a future unpackaged version of the shell extension
  that supports profile loading would read the right settings file (assuming it used the settings model.)
- If we choose to store the shell extension cache in the registry, we would need to avoid doing so in portable mode.
