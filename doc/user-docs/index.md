# Windows Terminal User Documentation

NOTE: At the time of writing Windows Terminal is still under active development and many things will
change. If you notice an error in the docs, please raise an issue. Or better yet, please file a PR with an appropriate update!

## Installing Windows Terminal 

### From Source Code

Follow the instructions in this repo's [README](/README.md#developer-guidance)

### From the Microsoft Store

1. Make sure you have upgraded to the current Windows 10 release (at least 1903)
2. Search for Windows Terminal in the Store
3. Review the minimum system settings to ensure you can successfully install Windows Terminal
4. Install in the normal fashion

## Starting Windows Terminal

From the Windows Start menu, select Windows Terminal and run the application.

Note: You can right click on the application item and run with administrator privilege if required.

The default shell is PowerShell.

#TODO Are they any command line arguments or switches?

## Multiple Tabs

Additional shells can be started by hitting the `+` button from the tab bar -- a new instance of the
default shell is displayed (default shortcut `Ctrl+Shift+1`).

## Running a Different Shell

Note: The following text assumes you have WSL installed.

To choose a different shell (e.g. `cmd.exe` or WSL `bash`) then

1. Select the `down` button next to the `+` in the tab bar
2. Choose your new shell from the list (more on how to extend the list in the config section)

## Starting a new tab Powershell with admin privilege

#TODO

## Using cut and paste in the Terminal window

#TODO  For Powershell and Bash (and cmd.exe?)

## Add a "Open Windows Terminal Here" to File Explorer

#TODO

## Configuring Windows Terminal

At the time of writing all Windows Terminal settings are managed via a json file.

From the `down` button in the top bar select Settings (default shortcut `Ctrl+,`).

Your default json editor will open up the Terminal settings file. The file can be found
at `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_<randomString>/RoamingState`

#TODO -- is this path correct for **users** ?

An introduction to the the various settings can be found [here](UsingJsonSettings.md).

The list of valid settings can be found in the [Profiles.json Documentation](../cascadia/SettingsSchema.md) doc.

#TODO Add new page with detailed information on settings. E.g Adding new shells, keybindings, background Gif etc.

## Tips and Tricks:

1. In PowerShell you can discover if the Windows Terminal is being used by checking for the existence of the environment variable `WT_SESSION`.

    Under pwsh you can also use
`(Get-Process -Id $pid).Parent.Parent.ProcessName -eq 'WindowsTerminal'`

    (ref https://twitter.com/r_keith_hill/status/1142871145852440576)

2. #TODO  More Tips and Tricks
