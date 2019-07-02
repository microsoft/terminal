# Editing Windows Terminal JSON Settings

One way (currently the only way) to configure Windows Terminal is by editing the profiles.json settings file. At
the time of writing you can open the settings file in your default editor by selecting
`Settings` from the WT pull down menu.

The settings are stored in the file `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_<randomString>\RoamingState\profiles.json` 

Details of specific settings can be found [here](../cascadia/SettingsSchema.md). A general introduction is provided below.

The settings are grouped under four headings:

1. Global: Settings that apply to the whole application e.g. Default profile, initial size etc.
2. Key Bindings: Actually a sub field of the global settings, but worth discussing separately
3. Profiles: A group of settings to be applied to a tab when it is opened using that profile. E.g. shell to use, cursor shape etc.
4. Schemes: Sets of colors for background, text etc. that can be used by profiles

## Global Settings

These settings define startup defaults.

* Theme
* Title Bar options
* Initial size
* Default profile used when WT is started

Example settings include

```json
    "defaultProfile" : "{58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530}",
    "initialCols" : 120,
    "initialRows" : 50,
    "requestedTheme" : "system",
    "keybinding" : []
    ...
```

## Key Bindings

This is an array of key chords and shortcuts to invoke various commands.
Each command can have more than one key binding.

NOTE: Key bindings is a subfield of the global settings and
key bindings apply to all profiles in the same manner.

## Profiles

A profile contains the settings applied when a new WT tab is opened. Each profile is identified by a GUID and contains
a number of other fields.

* Which command to execute on startup - this can include arguments.
* Starting directory
* Which color scheme to use (see Schemes below)
* Font face and size
* Various settings to control appearance. E.g. Opacity, icon, cursor appearance, display name etc.
* Other behavioural settings. E.g. Close on exit, snap on input, .....

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
    "startingDirectory" : "%USERPROFILE%/wslhome"
    ....
```

The profile GUID is used to reference the default profile in the global settings.

The values for background image stretch mode are documented [here](https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.media.stretch)

##  color Schemes

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

## Configuration Examples:

### Add a custom background to the WSL Debian terminal profile

1. Download the Debian SVG logo https://www.debian.org/logos/openlogo.svg
2. Put the image in the
 `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_<randomString>\RoamingState\`
 directory (same directory as your `profiles.json` file).

    __NOTE__:  You can put the image anywhere you like, the above suggestion happens to be convenient.
3. Open your WT json properties file.
4. Under the Debian Linux profile, add the following fields:
```json
    "backgroundImage": "ms-appdata:///Roaming/openlogo.jpg",
    "backgroundImageOpacity": 0.3,
    "backgroundImageStretchMode":  "Fill",
```
5. Make sure that `useAcrylic` is `false`.
6. Save the file.
7. Jump over to WT and verify your changes.

Notes:
1. You will need to experiment with different color settings
and schemes to make your terminal text visible on top of your image
2. If you store the image in the UWP directory (the same directory as your profiles.json file),
then you should use the URI style path name given in the above example.
More information about UWP URI schemes [here](https://docs.microsoft.com/en-us/windows/uwp/app-resources/uri-schemes).
3. Instead of using a UWP URI you can use a:
    1. URL such as
`http://open.esa.int/files/2017/03/Mayer_and_Bond_craters_seen_by_SMART-1-350x346.jpg` 
    2. Local file location such as `C:\Users\Public\Pictures\openlogo.jpg`
