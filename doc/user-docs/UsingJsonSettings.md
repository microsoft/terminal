# Editing Windows Terminal Json settings.

One way to configure Windows Terminal is by editing the json settings. At
the time of writing you can open the settings file in your default editor by selecting
`Settings` from he WT pull down menu.

The settings are stored in the file `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_<randomString>/RoamingState/profiles.json`

Details of specific settings cane be found [here](../cascadia/SettingsSchema.md). This document is an introduction.

The settings are grouped under four headings:

1. Global: Settings that apply to the whole application e.g. Default profile, initial size etc.
2. Key Bindings: Actually a sub field of globals, buy worth discussing seperatly
3. Profiles: A group of settings to be applied to a tab when it opens using that profile. E.g. shell to use, cursor shape etc.
4. Schemes: Sets of colour for background, text etc that can be used by profiles

## Global Settings:

Theese settings define startup defaults

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

This is an array of commands and key choards to invoke the command. Each command can have more that one
one key binding.

NOTE: Key bindings is a subfield of the global settings and key bindings apply to all profiles

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

A profile are the settings appliced when a new WT tab is opended. Each profile is ientifed by a GUID and contains
a number of other fields

* Which command line to execute. This can include arguments #TODO Test this assumption, I only read the code
* Starting directory
* Which colour scheme to use (see Schemes below)
* Font face and size
* Various settings to control appearence. E.g. Opacity, icon, cursor apperance, display name etc
* Other behaviourl settings. E.g. Close on exit, snap on input, .....

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
    "name" : "Debian",
    "padding" : "0, 0, 0, 0",
    "snapOnInput" : true,
    "useAcrylic" : false,
    "startingDirectory" : "%USERPROFILE%/wslhome"
}
```

##  Colour Schemes

Each colour scheme defines the colour values to be used for various terminal escape sequences.
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

#TODO Add some useful examples
