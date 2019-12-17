# Windows Terminal User Documentation

NOTE: At the time of writing Windows Terminal is still under active development and many things will
change. If you notice an error in the docs, please raise an issue. Or better yet, please file a PR with an appropriate update!

## Installing Windows Terminal

### From Source Code

To compile Windows Terminal yourself using the source code, follow the instructions in the [README](/README.md#developer-guidance).

### From the Microsoft Store

1. Make sure you have upgraded to the current Windows 10 release (at least build `1903`). To determine your build number, see [winver](https://docs.microsoft.com/en-us/windows/client-management/windows-version-search).
2. Open the Windows Terminal listing in the [Microsoft Store](https://aka.ms/install-terminal).
3. Review the minimum system requirements to confirm you can successfully install Windows Terminal.
4. Click `Get` to begin the installation process.

## Starting Windows Terminal

1. Locate the _Windows Terminal_ app in your Start menu.
2. Click _Windows Terminal_ to launch the app. If you need administrative privileges, right-click the entry and click `Run as administrator`. Alternatively, you can highlight the app and press `Ctrl`+`Shift`+`Enter`.

NOTE: The default shell is PowerShell; you can change this using the _Running a Different Shell_ procedure.

### Command line options

None at this time. See issue [#607](https://github.com/microsoft/terminal/issues/607)

## Multiple Tabs

Additional shells can be started by hitting the `+` button from the tab bar -- a new instance of the
default shell is displayed (default shortcut: <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>1</kbd>).

## Running a Different Shell

Note: This section assumes you already have _Windows Subsystem for Linux_ (WSL) installed. For more information, see [the installation guide](https://docs.microsoft.com/en-us/windows/wsl/install-win10).

Windows Terminal uses PowerShell as its default shell. You can also use Windows Terminal to launch other shells, such as `cmd.exe` or WSL's `bash`:

1. In the tab bar, click the `⌵` button to view the available shells.
2. Choose your shell from the dropdown list. The new shell session will open in a new tab.

To customize the shell list, see the _Configuring Windows Terminal_ section below.

## Starting a new PowerShell tab with admin privilege

There is no current plan to support this feature for security reasons. See issue [#623](https://github.com/microsoft/terminal/issues/632)

## Selecting and Copying Text in Windows Terminal

As in ConHost, a selection can be made by left-clicking and dragging the mouse across the terminal. This is a line selection by default, meaning that the selection will wrap to the end of the line and the beginning of the next one. You can select in block mode by holding down the <kbd>Alt</kbd> key when starting a selection.

To copy the text to your clipboard, you can right-click the terminal when a selection is active. As of [#1224](https://github.com/microsoft/terminal/pull/1224) (first available in Windows Terminal v0.4), the Windows Terminal now supports HTML copy. The HTML is automatically copied to your clipboard along with the regular text in any copy operation.

If there is not an active selection, a right-click will paste the text content from your clipboard to the terminal.

Copy and paste operations can also be keybound. For more information on how to bind keys, see [Using Json Settings](UsingJsonSettings.md#adding-copy-and-paste-keybindings).

> 👉 **Note**: If you have the `copyOnSelect` global setting enabled, a selection will persist and immediately copy the selected text to your clipboard. Right-clicking will always paste your clipboard data.

## Add a "Open Windows Terminal Here" to File Explorer

Not currently supported "out of the box". See issue [#1060](https://github.com/microsoft/terminal/issues/1060)

## Configuring Windows Terminal

All Windows Terminal settings are currently managed using the `profiles.json` file, located within `$env:LocalAppData\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe/LocalState`.

To open the settings file from Windows Terminal:

1. Click the `⌵` button in the top bar.
2. From the dropdown list, click `Settings`. You can also use a shortcut: <kbd>Ctrl</kbd>+<kbd>,</kbd>.
3. Your default `json` editor will open the settings file.

For an introduction to the various settings, see [Using Json Settings](UsingJsonSettings.md). The list of valid settings can be found in the [profiles.json documentation](../cascadia/SettingsSchema.md) section.

## Tips and Tricks:

1. In PowerShell you can discover if the Windows Terminal is being used by checking for the existence of the environment variable `WT_SESSION`.

    Under pwsh you can also use
`(Get-Process -Id $pid).Parent.Parent.ProcessName -eq 'WindowsTerminal'`

    (ref https://twitter.com/r_keith_hill/status/1142871145852440576)

2. Terminal zoom can be changed by holding <kbd>Ctrl</kbd> and scrolling with mouse.
3. If `useAcrylic` is enabled in profiles.json, background opacity can be changed by holding <kbd>Ctrl</kbd>+<kbd>Shift</kbd> and scrolling with mouse.
4. Please add more Tips and Tricks
