# Windows Terminal User Documentation

NOTE: At the time of writing Windows Terminal is still under active development and many things will
change. If you notice an error in the docs, please raise an issue. Or better yet, please file a PR with an appropriate update!

## Installing Windows Terminal 

### From Source Code

Follow the instructions in this repo's [README](/README.md#developer-guidance).

### From the Microsoft Store

1. Make sure you have upgraded to the current Windows 10 release (at least 1903)
2. Search for Windows Terminal in the Store
3. Review the minimum system settings to ensure you can successfully install Windows Terminal
4. Install in the normal fashion

## Starting Windows Terminal

From the Windows Start menu, select Windows Terminal and run the application.

Note: You can right click on the application item and run with Windows Administrator privilege if required.

The default shell is PowerShell.


### Command line options

None at this time. See issue [#607](https://github.com/microsoft/terminal/issues/607)

## Multiple Tabs

Additional shells can be started by hitting the `+` button from the tab bar -- a new instance of the
default shell is displayed (default shortcut `Ctrl+Shift+1`).

## Running a Different Shell

Note: This section assumes you already have _Windows Subsystem for Linux_ (WSL) installed. For more information, see [the installation guide](https://docs.microsoft.com/en-us/windows/wsl/install-win10).

Windows Terminal uses PowerShell as its default shell. You can also use Windows Terminal to launch other shells, such as `cmd.exe` or WSL's `bash`:

1. In the tab bar, click the `⌵` button to view the available shells.
2. Choose your shell from the dropdown list. The new shell session will open in a new tab.

To customize the shell list, see the _Configuring Windows Terminal_ section below.

## Starting a new PowerShell tab with admin privilege

There is no current plan to support this feature for security reasons. See issue [#623](https://github.com/microsoft/terminal/issues/632)

## Using cut and paste in the Terminal window

### With PowerShell

* Copy - Select the text with mouse (default left button), then right click with mouse
* Paste - by default use `<ctrl>+v`>, or right click with mouse

### With Bash

* Copy - Select the text with mouse (default left button), then right click with mouse
* Paste - Right click with mouse

## Add a "Open Windows Terminal Here" to File Explorer

Not currently supported "out of the box". See issue [#1060](https://github.com/microsoft/terminal/issues/1060)

## Configuring Windows Terminal

All Windows Terminal settings are currently managed using the `profiles.json` file, located within `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_<randomString>/RoamingState`.

To open the settings file from Windows Terminal:

1. Click the `⌵` button in the top bar.
2. From the dropdown list, click `Settings`. You can also use a shortcut: `Ctrl+,`.
3. Your default `json` editor will open the settings file.

For an introduction to the various settings, see [Using Json Settings](UsingJsonSettings.md). The list of valid settings can be found in the [profiles.json documentation](../cascadia/SettingsSchema.md) section.

## Tips and Tricks:

1. In PowerShell you can discover if the Windows Terminal is being used by checking for the existence of the environment variable `WT_SESSION`.

    Under pwsh you can also use
`(Get-Process -Id $pid).Parent.Parent.ProcessName -eq 'WindowsTerminal'`

    (ref https://twitter.com/r_keith_hill/status/1142871145852440576)

2. Terminal zoom can be changed by holding `Ctrl` and scrolling with mouse.
3. If `useAcrylic` is enabled in profiles.json, background opacity can be changed by holding `Ctrl+Shift` and scrolling with mouse.
4. Please add more Tips and Tricks
