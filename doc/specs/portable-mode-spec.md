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
