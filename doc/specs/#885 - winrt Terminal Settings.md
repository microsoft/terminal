---
author: Carlos Zamora @carlos-zamora
created on: 2020-07-10
last updated: 2020-07-10
issue id: /#885
---

# Spec Title

## Abstract

This spec proposes a major refactor and repurposing of the TerminalSettings project. TerminalSettings would
 be responsible for exposing, serializing, and deserializing settings as WinRT objects for Windows Terminal. In
 doing so, Terminal's settings model is accessible as WinRT objects to existing components like TerminalApp,
 TerminalControl, and TerminalCore. Additionally, Terminal Settings can be used by the Settings UI or
 Shell Extensions to modify or reference Terminal's settings respectively.

## Inspiration

The main driver for this change is the Settings UI. The Settings UI will need to read and modify Terminal Settings.
 At the time of writing this spec, the Terminal Settings are serialized as objects in the TerminalApp project.
 To access these objects, the Settings UI would need to be a part of TerminalApp too, making it more bloated.

## Solution Design

### Terminal Settings Model: Objects and Projections

The objects/interfaces introduced in this section will be exposed as WinRT objects/interfaces
 within the `Microsoft.Terminal.Settings.Model` namespace. This allows them to be interacted with across
 different project layers.

Additionally, the top-level object holding all of the settings related to Windows Terminal will be
`AppSettings`. The runtime class will look something like this:
```c++
runtimeclass AppSettings
{
    // global settings that generally only affect TerminalApp
    GlobalSettings Globals { get; };

    // the list of profiles available to Windows Terminal (custom and dynamically generated)
    IObservableVector<Profile> Profiles { get; };

    // the list of key bindings indexed by their unique key chord
    IObservableMap<KeyChord, Action> Keybindings { get; };

    // the list of command palette commands indexed by their name (custom or auto-generated)
    IObservableMap<String, Action> Commands { get; };

    // the list of color schemes indexed by their unique names
    IObservableMap<String, ColorScheme> Schemes { get; };

    // a list of warnings encountered during the serialization of the JSON
    IVector<SerializationWarnings> Warnings { get; };
}
```

Introducing a number of observable WinRT types allows for other components to subscribe to changes
 to these collections. A particular example of this being used can be a preview of the TermControl in
 the upcoming Settings UI.

Adjacent to the introduction of `AppSettings`, `IControlSettings` and `ICoreSettings` will be moved
 to the `Microsoft.Terminal.TerminalControl` namespace. This allows for a better consumption of the
 settings model that is covered later in the (Consumption section)[#terminal-settings-model:-consumption].


### Terminal Settings Model: Serialization and Deserialization

Introducing these `Microsoft.Terminal.Settings` WinRT objects also allow the serialization and deserialization
 logic from TerminalApp to be moved to TerminalSettings. `JsonUtils` introduces several quick and easy methods
 for setting serialization. This will be moved into the `Microsoft.Terminal.Settings` namespace too.

Deserialization will be an extension of the existing `JsonUtils` `ConversionTrait` struct template. `ConversionTrait`
 already includes `FromJson` and `CanConvert`. Deserialization would be handled by a `ToJson` function.


### Terminal Settings Model: Consumption

Separate projects can consume the Terminal Settings objects as needed.

TerminalApp would reference `AppSettings` for the following functionality...
- `Profiles` to populate the dropdown menu
- `Keybindings` to detect active key bindings and which actions they run
- `Commands` to populate the Command Palette
- `Warnings` to show a serialization warning, if any

At the time of writing this spec, TerminalApp constructs `TerminalControl.TerminalSettings` WinRT objects
 to expose `IControlSettings` and `ICoreSettings` to any hosted terminals. In moving `IControlSettings`
 and `ICoreSettings` down to the TerminalControl layer, TerminalApp can now have better control over
 how to expose relevant settings to a TerminalControl instance.

`TerminalSettings` will be moved to TerminalApp and act as a bridge that queries `AppSettings`. Thus,
 when TermControl requests `IControlSettings` data, `TerminalSettings` responds by extracting the
 relevant information from `AppSettings`. TerminalCore operates the same way with `ICoreSettings`.


## UI/UX Design

N/A

## Capabilities

### Accessibility

N/A

### Security

N/A

### Reliability

N/A

### Compatibility

N/A

### Performance, Power, and Efficiency

## Potential Issues

### Deserialization

After deserializing the settings, injecting the new json into settings.json
 should not remove the existing comments or formatting.

## Future considerations

### Dropdown Customization

Profiles are proposed to be represented in an `IObservableVector`. They could be represented as an
 `IObservableMap` with the guid or profile name used as the key value. The main concern with representing
 profiles as a map is that this loses the order for the dropdown menu.

Once Dropdown Customization is introduced, `Settings` is expected to serialize the relevant JSON and expose
 it to TerminalApp (probably as an `IObservableVector<DropdownMenuItem>`). Since this new object would
 handle the ordering of profiles (if any), the `IObservableVector<Profile>` could be replaced with
 an observable map indexed by the profile name, thus even potentially providing an opportunity to
 remove guid as an identifier entirely.


## Resources

- [Spec: Dropdown Customization](https://github.com/microsoft/terminal/pull/5888)
- [New JSON Utils](https://github.com/microsoft/terminal/pull/6590)
- [Spec: Settings UI](https://github.com/microsoft/terminal/pull/6720)
