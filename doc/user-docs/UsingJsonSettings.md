# Editing Windows Terminal JSON Settings

One way (currently the only way) to configure Windows Terminal is by editing the
`settings.json` settings file. At the time of writing you can open the settings
file in your default editor by selecting `Settings` from the WT pull down menu.

The settings are stored in the file `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json`.

As of [#2515](https://github.com/microsoft/terminal/pull/2515), the settings are
split into _two_ files: a hardcoded `defaults.json`, and `settings.json`, which
contains the user settings. Users should only be concerned with the contents of
the `settings.json`, which contains their customizations. The `defaults.json`
file is only provided as a reference of what the default settings are. For more
details on how these two files work, see [Settings
Layering](#settings-layering). To view the default settings file, click on the
"Settings" button while holding the <kbd>Alt</kbd> key.

Details of specific settings can be found [here](../cascadia/SettingsSchema.md).
A general introduction is provided below.

The settings are grouped under four headings:

1. Global: Settings that apply to the whole application e.g. Default profile, initial size etc.
2. Key Bindings: Actually a sub field of the global settings, but worth discussing separately
3. Profiles: A group of settings to be applied to a tab when it is opened using that profile. E.g. shell to use, cursor shape etc.
4. Schemes: Sets of colors for background, text etc. that can be used by profiles

## Global Settings

These settings define startup defaults, and application-wide settings that might
not affect a particular terminal instance.

* Theme
* Title Bar options
* Initial size
* Default profile used when the Windows Terminal is started

Example settings include

```json
{
    "defaultProfile" : "{58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530}",
    "initialCols" : 120,
    "initialRows" : 50,
    "theme" : "system",
    "keybindings" : []
    ...
}
```

These global properties should exist in the root json object.

## Key Bindings

This is an array of key chords and shortcuts to invoke various commands.
Each command can have more than one key binding.

> 👉 **Note**: Key bindings is a subfield of the global settings and
> key bindings apply to all profiles in the same manner.

For example, here's a sample of the default keybindings:

```json
{
    "keybindings":
    [
        { "command": "closePane", "keys": ["ctrl+shift+w"] },
        { "command": "copy", "keys": ["ctrl+shift+c"] },
        { "command": "newTab", "keys": ["ctrl+shift+t"] },
        // etc.
    ]
}
```

You can also use a single key chord string as the value of `"keys"`.
It will be treated as a chord of length one.
This will allow you to simplify the above snippet as follows:

```json
{
    "keybindings":
    [
        { "command": "closePane", "keys": "ctrl+shift+w" },
        { "command": "copy", "keys": "ctrl+shift+c" },
        { "command": "newTab", "keys": "ctrl+shift+t" },
        // etc.
    ]
}
```

A list of default key bindings is available [here](https://github.com/microsoft/terminal/blob/master/src/cascadia/TerminalApp/defaults.json#L204).

### Unbinding keys

If you ever come across a key binding that you're unhappy with, it's possible to
easily change the keybindings. For example, vim uses <kbd>Ctrl+^</kbd> as a
binding for "switch to previous buffer", which conflicts with the Terminal's
default keybinding for "open a new tab with the sixth profile". If you'd like to
unbind that keybinding, and allow the keystroke to fall through to vim, you can
add the following to your keybindings:

```json
{
    "command" : null, "keys" : ["ctrl+shift+6"]
},
```

This will _unbind_ <kbd>Ctrl+Shift+6</kbd>, allowing vim to use the keystroke
instead of the terminal.

### Binding multiple keys

You can have multiple key chords bound to the same action. To do this, simply
add multiple bindings for the same action. For example:

```json
    "keybindings" :
    [
        { "command": "copy", "keys": "ctrl+shift+c" },
        { "command": "copy", "keys": "ctrl+c" },
        { "command": "copy", "keys": "enter" }
    ]
```

In this snippet, all three of <kbd>ctrl+shift+c</kbd>, <kbd>ctrl+c</kbd> and <kbd>enter</kbd> are bound to `copy`.

## Profiles

A profile contains the settings applied when a new WT tab is opened. Each
profile is identified by a GUID and contains a number of other fields.

> 👉 **Note**: The `guid` property is the unique identifier for a profile. If
> multiple profiles all have the same `guid` value, you may see unexpected
> behavior.

* Which command to execute on startup - this can include arguments.
* Starting directory
* Which color scheme to use (see Schemes below)
* Font face and size
* Various settings to control appearance. E.g. Opacity, icon, cursor appearance, display name etc.
* Other behavioral settings. E.g. Close on exit, snap on input, .....

Example settings include

```json
    "closeOnExit" : true,
    "colorScheme" : "Campbell",
    "commandline" : "wsl.exe -d Debian",
    "cursorColor" : "#FFFFFF",
    "cursorShape" : "bar",
    "fontFace" : "Hack",
    "fontSize" : 9,
    "guid" : "{58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530}",
    "name" : "Debian",
    "startingDirectory" : "%USERPROFILE%\\wslhome"
    ....
```

> 👉 **Note**: To use backslashes in any path field, you'll need to escape them following JSON escaping rules (like shown above). As an alternative, you can use forward slashes ("%USERPROFILE%/wslhome").

The profile GUID is used to reference the default profile in the global settings.

The values for background image stretch mode are documented [here](https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.media.stretch).

### Hiding a profile

If you want to remove a profile from the list of profiles in the new tab
dropdown, but keep the profile around in your `settings.json` file, you can add
the property `"hidden": true` to the profile's json. This can also be used to
remove the default `cmd` and PowerShell profiles, if the user does not wish to
see them.

## Color Schemes

Each scheme defines the color values to be used for various terminal escape sequences.
Each schema is identified by the name field. Examples include

```json
    "name" : "Campbell",
    "background" : "#0C0C0C",
    "black" : "#0C0C0C",
    "blue" : "#0037DA",
    "foreground" : "#F2F2F2",
    "green" : "#13A10E",
    "red" : "#C50F1F",
    "white" : "#CCCCCC",
    "yellow" : "#C19C00"
    ...
```

The schema name can then be referenced in one or more profiles.

## Settings layering

The runtime settings are actually constructed from _three_ sources:

* The default settings, which are hardcoded into the application, and available
  in `defaults.json`. This includes the default keybindings, color schemes, and
  profiles for both Windows PowerShell and Command Prompt (`cmd.exe`).
* Dynamic Profiles, which are generated at runtime. These include Powershell
  Core, the Azure Cloud Shell connector, and profiles for and WSL distros.
* The user settings from `settings.json`.

Settings from each of these sources are "layered" upon the settings from
previous sources. In this manner, the user settings in `settings.json` can
contain _only the changes from the default settings_. For example, if a user
would like to only change the color scheme of the default `cmd` profile to
"Solarized Dark", you could change your cmd profile to the following:

```js
        {
            // Make changes here to the cmd.exe profile
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "colorScheme": "Solarized Dark"
        }
```

Here, we know we're changing the `cmd` profile, because the `guid`
`"{0caa0dad-35be-5f56-a8ff-afceeeaa6101}"` is `cmd`'s unique GUID. Any profiles
with that GUID will all be treated as the same object. Any changes in that
profile will overwrite those from the defaults.

Similarly, you can overwrite settings from a color scheme by defining a color
scheme in `settings.json` with the same name as a default color scheme.

If you'd like to unbind a keystroke that's bound to an action in the default
keybindings, you can set the `"command"` to `"unbound"` or `null`. This will
allow the keystroke to fallthrough to the commandline application instead of
performing the default action.

### Dynamic Profiles

When dynamic profiles are created at runtime, they'll be added to the
`settings.json` file. You can identify these profiles by the presence of a
`"source"` property. These profiles are tied to their source - if you uninstall
a linux distro, then the profile will remain in your `settings.json` file, but
the profile will be hidden.

The Windows Terminal uses the `guid` property of these dynamically-generated
profiles to uniquely identify them. If you try to change the `guid` of a
dynamically-generated profile, the Terminal will automatically recreate a new
entry for that profile.

If you'd like to disable a particular dynamic profile source, you can add that
`source` to the global `"disabledProfileSources"` array. For example, if you'd
like to hide all the WSL profiles, you could add the following setting:

```json

    "disabledProfileSources": ["Windows.Terminal.WSL"],
    ...

```

> 👉 **NOTE**: On launch, if a dynamic profile generator is enabled, it will
> always add new profiles it detects to your list of profiles. If you delete a
> dynamically generated profile from your list of profiles, it will just get
> re-added the next time the Terminal is launched! To remove a dynamic profile
> from your list of profiles, make sure to set `"hidden": true` in the profile.

### Default settings

In [#2325](https://github.com/microsoft/terminal/issues/2325), we introduced the
concept of "Default Profile Settings". These are settings that will apply to all
of your profiles by default. Profiles can still override these settings
individually. With default profile settings, you can easily make changes to all
your profiles at once. For example, given the following settings:

```json
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "profiles":
    [
        {
            "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "name": "Windows PowerShell",
            "commandline": "powershell.exe",
            "fontFace": "Cascadia Code",
            "fontSize": 14
        },
        {
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "name": "cmd",
            "commandline": "cmd.exe",
            "fontFace": "Cascadia Code",
            "fontSize": 14
        },
        {
            "commandline" : "cmd.exe /k %CMDER_ROOT%\\vendor\\init.bat",
            "name" : "cmder",
            "startingDirectory" : "%USERPROFILE%",
            "fontFace": "Cascadia Code",
            "fontSize": 14
        }
    ],
```

All three of these profiles are using "Cascadia Code" as their `"fontFace"`, and
14 as their `fontSize`. With default profile settings, you can easily set these
properties for all your profiles, like so:

```json
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "profiles": {
        "defaults":
        {
            "fontFace": "Cascadia Code",
            "fontSize": 14
        },
        "list": [
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "Windows PowerShell",
                "commandline": "powershell.exe"
            },
            {
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "name": "cmd",
                "commandline": "cmd.exe"
            },
            {
                "commandline" : "cmd.exe /k %CMDER_ROOT%\\vendor\\init.bat",
                "name" : "cmder",
                "startingDirectory" : "%USERPROFILE%"
            }
        ]
    },
```

Note that the `profiles` property has changed in this example from a _list_ of
profiles, to an _object_ with two properties:

* a `list` that contains the list of all the profiles
* the new `defaults` object, which contains all the settings that should apply to
  every profile.

What if I wanted a profile to have a different value for a property other than
the default? Simply set the property in the profile's entry to override the
value from `defaults`. Let's say you want the `cmd` profile to have _"Consolas"_
as the font, but the rest of your profiles to still have _"Cascadia Code"_. You
could achieve that with the following:

```json
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "profiles": {
        "defaults":
        {
            "fontFace": "Cascadia Code",
            "fontSize": 14
        },
        "list": [
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "Windows PowerShell",
                "commandline": "powershell.exe"
            },
            {
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "name": "cmd",
                "commandline": "cmd.exe",
                "fontFace": "Consolas"
            },
            {
                "commandline" : "cmd.exe /k %CMDER_ROOT%\\vendor\\init.bat",
                "name" : "cmder",
                "startingDirectory" : "%USERPROFILE%"
            }
        ]
    },
```

In the above settings, the `"fontFace"` in the `cmd.exe` profile overrides the
`"fontFace"` from the `defaults`.

## Configuration Examples

### Add a custom background to the WSL Debian terminal profile

1. Download the [Debian JPG logo](https://www.debian.org/logos/openlogo-100.jpg)
2. Put the image in the
 `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_<randomString>\LocalState\`
 directory (same directory as your `settings.json` file).

    __NOTE__:  You can put the image anywhere you like, the above suggestion happens to be convenient.
3. Open your WT json properties file.
4. Under the Debian Linux profile, add the following fields:

```json
    "backgroundImage": "ms-appdata:///Local/openlogo-100.jpg",
    "backgroundImageOpacity": 1,
    "backgroundImageStretchMode" : "none",
    "backgroundImageAlignment" : "topRight",
```

5. Make sure that `useAcrylic` is `false`.
6. Save the file.
7. Jump over to WT and verify your changes.

Notes:

1. You will need to experiment with different color settings
and schemes to make your terminal text visible on top of your image
2. If you store the image in the UWP directory (the same directory as your settings.json file),
then you should use the URI style path name given in the above example.
More information about UWP URI schemes [here](https://docs.microsoft.com/en-us/windows/uwp/app-resources/uri-schemes).
3. Instead of using a UWP URI you can use a:
    1. URL such as
`http://open.esa.int/files/2017/03/Mayer_and_Bond_craters_seen_by_SMART-1-350x346.jpg`
    2. Local file location such as `C:\Users\Public\Pictures\openlogo.jpg`

### Adding Copy and Paste Keybindings

As of [#1093](https://github.com/microsoft/terminal/pull/1093) (first available
in Windows Terminal v0.3), the Windows Terminal now supports copy and paste
keyboard shortcuts. However, if you installed and ran the terminal before that,
you won't automatically get the new keybindings added to your settings. If you'd
like to add shortcuts for copy and paste, you can do so by inserting the
following objects into your `globals.keybindings` array:

```json
{ "command": "copy", "keys": ["ctrl+shift+c"] },
{ "command": "paste", "keys": ["ctrl+shift+v"] }
```

> 👉 **Note**: you can also add a keybinding for the `copy` command with the argument `"trimWhitespace": true`. This removes newlines as the text is copied to your clipboard.

This will add copy and paste on <kbd>ctrl+shift+c</kbd>
and <kbd>ctrl+shift+v</kbd> respectively.

You can set the keybindings to whatever you'd like. If you prefer
<kbd>ctrl+c</kbd> to copy, then set the `keys` to `"ctrl+c"`.

You can even set multiple keybindings for a single action if you'd like. For example:

```json

            {
                "command" : "paste",
                "keys" :
                [
                    "ctrl+shift+v"
                ]
            },
            {
                "command" : "paste",
                "keys" :
                [
                    "shift+insert"
                ]
            }
```

will bind both <kbd>ctrl+shift+v</kbd> and
<kbd>shift+Insert</kbd> to `paste`.

> 👉 **Note**: If you set your copy keybinding to `"ctrl+c"`, you'll only be able to send
an interrupt to the commandline application using <kbd>Ctrl+C</kbd> when there's
no text selection. Additionally, if you set `paste` to `"ctrl+v"`, commandline
applications won't be able to read a ctrl+v from the input. For these reasons,
we suggest `"ctrl+shift+c"` and `"ctrl+shift+v"`

### Setting the `startingDirectory` of WSL Profiles to `~`

By default, the `startingDirectory` of a profile is `%USERPROFILE%`
(`C:\Users\<YourUsername>`). This is a Windows path. However, for WSL, you might
want to use the WSL home path instead. At the time of writing (26decf1 / Nov.
1st, 2019), `startingDirectory` only accepts a Windows-style path, so setting it
to start within the WSL distro can be a little tricky.

Fortunately, with Windows 1903, the filesystems of WSL distros can easily be
addressed using the `\\wsl$\` prefix. For any WSL distro whose name is
`DistroName`, you can use `\\wsl$\DistroName` as a Windows path that points to
the root of that distro's filesystem.

For example, the following works as a profile to launch the "Ubuntu-18.04"
distro in it's home path:

```json
{
    "name": "Ubuntu-18.04",
    "commandline" : "wsl -d Ubuntu-18.04",
    "startingDirectory" : "//wsl$/Ubuntu-18.04/home/<Your Ubuntu Username>",
}
```
