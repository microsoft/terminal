---
author: Carlos Zamora @carlos-zamora
created on: 2020-07-10
last updated: 2020-07-10
issue id: [#885](https://github.com/microsoft/terminal/issues/885)
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
 At the time of writing this spec, the Terminal's settings are serialized as objects in the TerminalApp project.
 To access these objects, the Settings UI would need to be a part of TerminalApp too, making it more bloated.

## Solution Design

The existing TerminalSettings project will be repurposed and renamed to be TerminalSettingsModel.
 This project is responsible for the serialization, deserialization, and exposure of Windows Terminal's
 settings. It will host a variety of WinRT settings objects within the `Microsoft.Terminal.Settings.Model`
 namespace.

### Terminal Settings Model: Objects and Projections

The TerminalSettingsModel project will include the following WinRT objects (as well as various others):
- GlobalAppSettings: global settings that generally only affect TerminalApp
- Profile: profile-level settings
- ColorScheme: the color table used for a terminal instance
- AppKeyBindings: the key bindings to invoke actions
- Command: actions identified by a string (used in Command Palette)
- AppSettings: a top-level container and representation of Windows Terminal settings (formerly CascadiaSettings)

NOTE: All of these objects already exist in TerminalApp. The work to be done mainly
 includes transforming them into WinRT objects and moving them out of the TerminalApp project.

Additionally, this refactor will expose these settings objects as observable WinRT types.
 This allows for other components to subscribe to changes to the settings. A particular example of this
 being used will be an embedded TermControl in the upcoming Settings UI previewing changes to the color
 scheme.

Adjacent to the introduction of `AppSettings`, `IControlSettings` and `ICoreSettings` will be moved
 to the `Microsoft.Terminal.TerminalControl` namespace. This allows for a better consumption of the
 settings model that is covered later in the (Consumption section)[#terminal-settings-model:-consumption].


### Terminal Settings Model: Serialization and Deserialization

Introducing these `Microsoft.Terminal.Settings.Model` WinRT objects also allow the serialization and deserialization
 logic from TerminalApp to be moved to TerminalSettings. `JsonUtils` introduces several quick and easy methods
 for setting serialization. This will be moved into the `Microsoft.Terminal.Settings.Model` namespace too.

Deserialization will be an extension of the existing `JsonUtils` `ConversionTrait` struct template. `ConversionTrait`
 already includes `FromJson` and `CanConvert`. Deserialization would be handled via the introduction of a `ToJson` function.


### Interacting with the Terminal Settings Model

Separate projects can construct an `AppSettings` to extract data, subscribe to changes, and commit changes
 to the Terminal's settings. Today, Windows Terminal uses the settings.json file to load the user's preferences.
 This process is a part of the `LoadAll()` function call, which constructs and returns an `AppSettings`
 from defaults.json and settings.json.

Introducing the following `AppSettings` constructor will let users construct a settings object
 from a different file path.
```c++
runtimeclass AppSettings
{
    // Create an AppSettings based on the defaults.json file data
    // and layer the settings file at "path" onto it
    AppSettings(String path);
}
```
Today, the defaults.json text is injected directly as a string to improve performance. This will
 be unchanged. The `path` parameter simply dictates where settings.json is located.

#### TerminalApp: Loading and Reloading Changes

TerminalApp will construct and reference an `AppSettings settings` as follows:
- TerminalApp will have a global reference to the "settings.json" filepath
- construct an `AppSettings` using `AppSettings("C:\path\to\settings.json")`. This builds an `AppSettings`
   from the "defaults.json" file data (which is already compiled as a string literal)
   and layers the settings.json data on top of it.

When TerminalApp detects a change to "settings.json", it'll repeat the steps above. We could cache the result from
 constructing an `AppSettings` from "defaults.json" data to improve performance, if so desired.

#### TerminalControl: Acquiring and Applying the Settings

At the time of writing this spec, TerminalApp constructs `TerminalControl.TerminalSettings` WinRT objects
 to expose `IControlSettings` and `ICoreSettings` to any hosted terminals. In moving `IControlSettings`
 and `ICoreSettings` down to the TerminalControl layer, TerminalApp can now have better control over
 how to expose relevant settings to a TerminalControl instance.

`TerminalSettings` (which implements `IControlSettings` and `ICoreSettings`) will be moved to
 TerminalApp and act as a bridge connecting `AppSettings` to the TermControl. It will operate
 very similarly as it does today. On construction of the TermControl or hot-reload,
 `TerminalSettings` will be constructed by copying the relevant values of `AppSettings`.
 Then, it will be passed to TermControl (and TermCore by extension).

To accomplish this new distribution of responsibilities, `CascadiaSettings::BuildSettings()` will be
 moved to TerminalApp's AppLogic; it will use its reference of `AppSettings`


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

The deserialization process takes place right after comparing the `settingsSource` and `settingsClone` objects.
 For each setting found in the diff, we go to the relevant part of the JSON and see if the key is already there.
 If it is, we update the value to be the one from `settingsClone`. Otherwise, we append the key/value pair
 at the end of the section (much like we do with dynamic profiles in `profiles`).



## Future considerations

### TerminalSettings: passing by reference

`TermApp` synthesizes a `TerminalSettings` by copying the relevant values of `AppSettings`,
 then giving it to a Terminal Control. Some visual keybindings and interactions like ctrl+scroll
 and ctrl+shift+scroll to change the font size and acrylic opacity operate by directly modifying
 the value of the instantiated `TerminalSettings`. However, when a settings reload occurs,
 these instanced changes are lost.

`TerminalSettings` can be used as a WinRT object that references (instead of copies) the relevant
 values of `AppSettings`. This would prevent those instanced changes from being lost on a settings
 reload.

Since previewing commands like `setColorScheme` would require a clone of the existing `TerminalSettings`,
 a `Clone` API can be added on `TerminalSettings` to accomplish that. When passing by value,
 `TerminalSettings` can just overwrite the existing property (i.e.: color scheme). When passing
 by reference, a slightly more complex mechanism is required to override the value.

Now, instead of overwriting the value, we need to override the reference to a constant value
(i.e.: `snapOnInput=true`) or a referenced value (i.e.: `colorScheme`).

### Layering Additional Settings
As we begin to introduce more sources that affect the settings (via extensions or themes),
 we can introduce a `LayerSettings(String path)`. This layers the new settings file
 onto the existing `AppSettings`. This is already done internally, we would just expose
 it via C++/WinRT.

```c++
runtimeclass AppSettings
{
    // Load a settings file, and layer those changes on top of the existing AppSettings
    void LayerSettings(String path);
}
```

### Settings UI: Modifying and Applying the Settings (DRAFT)

```c++
runtimeclass AppSettings
{
    // Create a copy of the existing AppSettings
    AppSettings Clone();

    // Compares object to "source" and applies changes to
    // the settings file at "outPath"
    void Save(String outPath);
}
```

The Settings UI will also have a reference to the `AppSettings settings` from TerminalApp
 as `settingsSource`. When the Settings UI is opened up, the Settings UI will also have its own `AppSettings settingsClone`
 that is a clone of TerminalApp's `AppSettings`.
```c++
settingsClone = settingsSource.Clone()
```

As the user navigates the Settings UI, the relevant contents of `settingsClone` will be retrieved and presented.
 As the user makes changes to the Settings UI, XAML will update `settingsClone` using XAML data binding.
 When the user saves/applies the changes in the XAML, `settingsClone.Save("settings.json")` is called;
 this compares the changes between `settingsClone` and `settingsSource`, then injects the changes (if any) to `settings.json`.

As mentioned earlier, TerminalApp detects a change to "settings.json" to update its `AppSettings`.
 Since the above triggers a change to `settings.json`, TerminalApp will also update itself. When
 something like this occurs, `settingsSource` will automatically be updated too.

In the case that a user is simultaneously updating the settings file directly and the Settings UI,
 `settingsSource` and `settingsClone` can be compared to ensure that the Settings UI, the TerminalApp,
 and the settings files are all in sync.

 **NOTE:** In the event that the user would want to export their current configuration, `Save`
 can be used to export the changes to a new file.

## Resources

- [Preview Commands](https://github.com/microsoft/terminal/issues/6689)
- [New JSON Utils](https://github.com/microsoft/terminal/pull/6590)
- [Spec: Settings UI](https://github.com/microsoft/terminal/pull/6720)
