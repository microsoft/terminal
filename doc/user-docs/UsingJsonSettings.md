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
4. Schemes: Sets of colour for background, text etc. that can be used by profiles

## Global Settings

These settings define startup defaults.

* Theme
* Title Bar options
* Initial size
* Default profile used when WT is started

For example

```json
{
    "alwaysShowTabs" : true,
    "defaultProfile" : "{58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530}",
    "initialCols" : 120,
    "initialRows" : 50,
    "requestedTheme" : "system",
    "showTabsInTitlebar" : true,
    "showTerminalTitleInTitlebar" : true,
    "keybinding" : []
}
```

## Key Bindings

This is an array of key chords and shortcuts to invoke various commands.
Each command can have more than one key binding.

NOTE: Key bindings is a subfield of the global settings and
key bindings apply to all profiles.

For example

```json
{
    "command" : "scrollDown",
    "keys" : 
    [
        "ctrl+shift+down"
    ]
},
```

## Profiles

A profile contains the settings applied when a new WT tab is opened. Each profile is identified by a GUID and contains
a number of other fields.

* Which command to execute on startup - this can include arguments.
* Starting directory
* Which colour scheme to use (see Schemes below)
* Font face and size
* Various settings to control appearance. E.g. Opacity, icon, cursor appearance, display name etc.
* Other behavioural settings. E.g. Close on exit, snap on input, .....

For example 

```json
{
    "acrylicOpacity" : 0.5,
    "closeOnExit" : true,
    "colorScheme" : "Campbell",
    "commandline" : "wsl.exe -d Debian",
    "cursorColor" : "#FFFFFF",
    "cursorShape" : "bar",
    "fontFace" : "Hack",
    "fontSize" : 9,
    "guid" : "{58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530}",
    "historySize" : 9001,
    "icon" : "ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png",
    "backgroundImage": "ms-appdata://Roaming/Linus.jpg",
    "backgroundImageOpacity": 0.3,
    "backgroundImageStretchMode":  "Fill",
    "name" : "Debian",
    "padding" : "0, 0, 0, 0",
    "snapOnInput" : true,
    "useAcrylic" : false,
    "startingDirectory" : "%USERPROFILE%/wslhome"
}
```

The values for background image stretch mode are documented [here](https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.media.stretch)

##  Colour Schemes

Each scheme defines the colour values to be used for various terminal escape sequences.
Each one is identified by the name field. For example

```json
{
    "background" : "#0C0C0C",
    "black" : "#0C0C0C",
    "blue" : "#0037DA",
    "brightBlack" : "#767676",
    "brightBlue" : "#3B78FF",
    "brightCyan" : "#61D6D6",
    "brightGreen" : "#16C60C",
    "brightPurple" : "#B4009E",
    "brightRed" : "#E74856",
    "brightWhite" : "#F2F2F2",
    "brightYellow" : "#F9F1A5",
    "cyan" : "#3A96DD",
    "foreground" : "#F2F2F2",
    "green" : "#13A10E",
    "name" : "Campbell",
    "purple" : "#881798",
    "red" : "#C50F1F",
    "white" : "#CCCCCC",
    "yellow" : "#C19C00"
},
```
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
    "backgroundImage": "ms-appdata://Roaming/openlogo.jpg",
    "backgroundImageOpacity": 0.3,
    "backgroundImageStretchMode":  "Fill",
```
5. Make sure that `useAcrylic` is `false`.
6. Save the file.
7. Jump over to WT and verify your changes.
Note: You will need to experiment with different colour settings
and schemes to make your terminal text visible on top of your image
