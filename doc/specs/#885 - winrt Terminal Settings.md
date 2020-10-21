---
author: Carlos Zamora @carlos-zamora
created on: 2020-07-10
last updated: 2020-07-10
issue id: [#885](https://github.com/microsoft/terminal/issues/885)
---

# Terminal Settings Model

## Abstract

This spec proposes a major refactor and repurposing of the TerminalSettings project as the TerminalSettingsModel.
 TerminalSettingsModel would be responsible for exposing, serializing, and deserializing settings as WinRT objects
 for Windows Terminal. In doing so, Terminal's settings model is accessible as WinRT objects to existing components
 like TerminalApp, TerminalControl, and TerminalCore. Additionally, Terminal Settings can be used by the Settings UI or
 Shell Extensions to modify or reference Terminal's settings respectively.

## Inspiration

The main driver for this change is the Settings UI. The Settings UI will need to read and modify Terminal's settings
 objects. At the time of writing this spec, the Terminal's settings are serialized as objects in the TerminalApp project.
 To access these objects via XAML, the Settings UI needs them to be WinRT objects. Additional features that need the
 settings objects to be WinRT objects include future shell extensions, like jumplist.

## Solution Design

### Terminal Settings Model: Objects and Projections

The following TerminalApp objects will become WinRT objects and will be moved to the TerminalSettingsModel project
(formerly TerminalSettings):
- ColorScheme
- Profile
- GlobalAppSettings
- CascadiaSettings

The TerminalSettingsModel project will have a root namespace of `Microsoft.Terminal.Settings.Model`.

Adjacent to the introduction of these settings objects, `IControlSettings` and `ICoreSettings` will be moved
 to the `Microsoft.Terminal.TerminalControl` namespace. This allows for a better consumption of the
 settings model that is covered later in the (Consumption section)[#terminal-settings-model:-consumption].

#### Moving/Splitting the Action Model

Windows Terminal represents actions via several objects:
- `AppKeyBindings`: a map of all the defined keybindings and their corresponding actions
- `ActionAndArgs`: a (de)serializable action (this holds more objects inside of it, but we won't focus on that for now)
- `ShortcutActionDispatch`: responsible for dispatching events pertinent to a given ActionAndArgs object
`TerminalApp`'s `TerminalPage` handles any events dispatched by the `ShortcutActionDispatch`.

With the introduction of the TerminalSettingsModel, we will split `AppKeyBindings` using a `KeyMapping` class.
 This separation will look something like the following:
```c++
namespace TerminalApp
{
    [default_interface] runtimeclass AppKeyBindings : Microsoft.Terminal.TerminalControl.IKeyBindings
    {
        AppKeyBindings();

        // NOTE: It may be possible to move both of these to the constructor instead
        void SetDispatch(ShortcutActionDispatch dispatch);
        void SetKeyMap(KeyMapping keymap);
    }
}

namespace TerminalSettingsModel
{
    [default_interface] runtimeclass KeyMapping
    {
        void SetKeyBinding(ActionAndArgs actionAndArgs, Microsoft.Terminal.TerminalControl.KeyChord chord);
        void ClearKeyBinding(Microsoft.Terminal.TerminalControl.KeyChord chord);

        Microsoft.Terminal.TerminalControl.KeyChord GetKeyBindingForAction(ShortcutAction action);
        Microsoft.Terminal.TerminalControl.KeyChord GetKeyBindingForActionWithArgs(ActionAndArgs actionAndArgs);
    }
}
```
This separation leaves `AppKeyBindings` with the responsibility of detecting and dispatching actions, whereas
 `KeyMapping` handles the (de)serialization and navigation of the key bindings.


### Terminal Settings Model: Serialization and Deserialization

Introducing these `Microsoft.Terminal.Settings.Model` WinRT objects also allow the serialization and deserialization
 logic from TerminalApp to be moved to TerminalSettings. `JsonUtils` introduces several quick and easy methods
 for setting serialization. This will be moved into the `Microsoft.Terminal.Settings.Model` namespace too.

Deserialization will be an extension of the existing `JsonUtils` `ConversionTrait` struct template. `ConversionTrait`
 already includes `FromJson` and `CanConvert`. Serialization would be handled by a `ToJson` function.


### Terminal Settings Model: Warnings and Serialization Errors

Today, if the deserialization of `CascadiaSettings` encounters any errors, an exception is thrown and caught/handled
 by falling back to a simple `CascadiaSettings` object. However, WinRT does not support exceptions.

To get around this issue, when `CascadiaSettings` encounters a serialization error, it must internally record
 any pertinent information for that error, and return the simple `CascadiaSettings` as if nothing happened.
 The consumer must then call `CascadiaSettings::GetErrors()` and `CascadiaSettings::GetWarnings()` to properly
 understand whether an error ocurred and how to present that to the user.


#### TerminalApp: Loading and Reloading Changes

TerminalApp will construct and reference a `CascadiaSettings settings` as follows:
- TerminalApp will have a global reference to the "settings.json" filepath
- construct an `CascadiaSettings` using `CascadiaSettings("settings.json")`. This builds an `CascadiaSettings`
   from the "defaults.json" file data (which is already compiled as a string literal)
   and layers the settings.json data on top of it.
- check for errors/warnings, and handle them appropriately

This will be different from the current model which has the settings.json path hardcoded, and is simplified
 to a `LoadAll()` call wrapped in error handlers.

**NOTE:** This model allows us to layer even more settings files on top of the existing Terminal Settings
 Model, if so desired. This could be helpful when importing additional settings files from an external location
 such as a marketplace.

When TerminalApp detects a change to settings.json, it'll repeat the steps above. We could cache the result from
 constructing an `CascadiaSettings` from "defaults.json" data to improve performance.


#### TerminalControl: Acquiring and Applying the Settings

At the time of writing this spec, TerminalApp constructs `TerminalControl.TerminalSettings` WinRT objects
 to expose `IControlSettings` and `ICoreSettings` to any hosted terminals. In moving `IControlSettings`
 and `ICoreSettings` down to the TerminalControl layer, TerminalApp can now have better control over
 how to expose relevant settings to a TerminalControl instance.

`TerminalSettings` (which implements `IControlSettings` and `ICoreSettings`) will be moved to
 TerminalApp and act as a bridge connecting `CascadiaSettings` to the TermControl. It will operate
 very similarly as it does today. On construction of the TermControl or hot-reload,
 `TerminalSettings` will be constructed by copying the relevant values of `CascadiaSettings`.
 Then, it will be passed to TermControl (and TermCore by extension).


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

N/A

## Future considerations

### TerminalSettings: passing by reference

`TermApp` synthesizes a `TerminalSettings` by copying the relevant values of `CascadiaSettings`,
 then giving it to a Terminal Control. Some visual keybindings and interactions like ctrl+scroll
 and ctrl+shift+scroll to change the font size and acrylic opacity operate by directly modifying
 the value of the instantiated `TerminalSettings`. However, when a settings reload occurs,
 these instanced changes are lost.

`TerminalSettings` can be used as a WinRT object that references (instead of copies) the relevant
 values of `CascadiaSettings`. This would prevent those instanced changes from being lost on a settings
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
 onto the existing `CascadiaSettings`. This is already done internally, we would just expose
 it via C++/WinRT.

```c++
runtimeclass CascadiaSettings
{
    // Load a settings file, and layer those changes on top of the existing CascadiaSettings
    void LayerSettings(String path);
}
```

### Settings UI: Modifying and Applying the Settings (DRAFT)

```c++
runtimeclass CascadiaSettings
{
    // Create a copy of the existing CascadiaSettings
    CascadiaSettings Clone();

    // Compares object to "source" and applies changes to
    // the settings file at "outPath"
    void Save(String outPath);
}
```

The Settings UI will also have a reference to the `CascadiaSettings settings` from TerminalApp
 as `settingsSource`. When the Settings UI is opened up, the Settings UI will also have its own `CascadiaSettings settingsClone`
 that is a clone of TerminalApp's `CascadiaSettings`.
```c++
settingsClone = settingsSource.Clone()
```

As the user navigates the Settings UI, the relevant contents of `settingsClone` will be retrieved and presented.
 As the user makes changes to the Settings UI, XAML will update `settingsClone` using XAML data binding.
 When the user saves/applies the changes in the XAML, `settingsClone.Save("settings.json")` is called;
 this compares the changes between `settingsClone` and `settingsSource`, then injects the changes (if any) to `settings.json`.

As mentioned earlier, TerminalApp detects a change to "settings.json" to update its `CascadiaSettings`.
 Since the above triggers a change to `settings.json`, TerminalApp will also update itself. When
 something like this occurs, `settingsSource` will automatically be updated too.

In the case that a user is simultaneously updating the settings file directly and the Settings UI,
 `settingsSource` and `settingsClone` can be compared to ensure that the Settings UI, the TerminalApp,
 and the settings files are all in sync.

 **NOTE:** In the event that the user would want to export their current configuration, `Save`
 can be used to export the changes to a new file.

 ### Reserialization (DRAFT)

After deserializing the settings, injecting the new json into settings.json
 should not remove the existing comments or formatting.

The reserialization process takes place right after comparing the `settingsSource` and `settingsClone` objects.
 For each setting found in the diff, we go to the relevant part of the JSON and see if the key is already there.
 If it is, we update the value to be the one from `settingsClone`. Otherwise, we append the key/value pair
 at the end of the section (much like we do with dynamic profiles in `profiles`).

## Resources

- [Preview Commands](https://github.com/microsoft/terminal/issues/6689)
- [New JSON Utils](https://github.com/microsoft/terminal/pull/6590)
- [Spec: Settings UI](https://github.com/microsoft/terminal/pull/6720)
