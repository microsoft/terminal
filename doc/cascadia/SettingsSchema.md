# Profiles.json Documentation

## Globals
Properties listed below affect the entire window, regardless of the profile settings.

| Property | Necessity | Type | Default | Description |
| -------- | --------- | ---- | ------- | ----------- |
| `alwaysShowTabs` | _Required_ | Boolean | `true` | When set to `true`, tabs are always displayed. When set to `false` and `showTabsInTitlebar` is set to `false`, tabs only appear after typing <kbd>Ctrl</kbd> + <kbd>T</kbd>. |
| `copyOnSelect` | Optional | Boolean | `false` | When set to `true`, a selection is immediately copied to your clipboard upon creation. When set to `false`, the selection persists and awaits further action. |
| `defaultProfile` | _Required_ | String | PowerShell guid | Sets the default profile. Opens by typing <kbd>Ctrl</kbd> + <kbd>T</kbd> or by clicking the '+' icon. The guid of the desired default profile is used as the value. |
| `initialCols` | _Required_ | Integer | `120` | The number of columns displayed in the window upon first load. |
| `initialRows` | _Required_ | Integer | `30` | The number of rows displayed in the window upon first load. |
| `requestedTheme` | _Required_ | String | `system` | Sets the theme of the application. Possible values: `"light"`, `"dark"`, `"system"` |
| `showTerminalTitleInTitlebar` | _Required_ | Boolean | `true` | When set to `true`, titlebar displays the title of the selected tab. When set to `false`, titlebar displays "Windows Terminal". |
| `showTabsInTitlebar` | Optional | Boolean | `true` | When set to `true`, the tabs are moved into the titlebar and the titlebar disappears. When set to `false`, the titlebar sits above the tabs. |
| `wordDelimiters` | Optional | String | <code>&nbsp;&#x2f;&#x5c;&#x28;&#x29;&#x22;&#x27;&#x2d;&#x3a;&#x2c;&#x2e;&#x3b;&#x3c;&#x3e;&#x7e;&#x21;&#x40;&#x23;&#x24;&#x25;&#x5e;&#x26;&#x2a;&#x7c;&#x2b;&#x3d;&#x5b;&#x5d;&#x7b;&#x7d;&#x7e;&#x3f;│</code><br>_(`│` is `U+2502 BOX DRAWINGS LIGHT VERTICAL`)_ | Determines the delimiters used in a double click selection. |

## Profiles
Properties listed below are specific to each unique profile.

| Property | Necessity | Type | Default | Description |
| -------- | --------- | ---- | ------- | ----------- |
| `guid` | _Required_ | String | | Unique identifier of the profile. Written in registry format: `"{00000000-0000-0000-0000-000000000000}"`. |
| `name` | _Required_ | String | | Name of the profile. Displays in the dropdown menu. <br>Additionally, this value will be used as the "title" to pass to the shell on startup. Some shells (like `bash`) may choose to ignore this initial value, while others (`cmd`, `powershell`) may use this value over the lifetime of the application. This "title" behavior can be overriden by using `tabTitle`. |
| `acrylicOpacity` | Optional | Number | `0.5` | When `useAcrylic` is set to `true`, it sets the transparency of the window for the profile. Accepts floating point values from 0-1. |
| `background` | Optional | String | | Sets the background color of the profile. Overrides `background` set in color scheme if `colorscheme` is set. Uses hex color format: `"#rrggbb"`. |
| `backgroundImage` | Optional | String | | Sets the file location of the Image to draw over the window background. |
| `backgroundImageAlignment` | Optional | String | `center` | Sets how the background image aligns to the boundaries of the window. Possible values: `"center"`, `"left"`, `"top"`, `"right"`, `"bottom"`, `"topLeft"`, `"topRight"`, `"bottomLeft"`, `"bottomRight"` |
| `backgroundImageOpacity` | Optional | Number | `1.0` | Sets the transparency of the background image. Accepts floating point values from 0-1. |
| `backgroundImageStretchMode` | Optional | String | `uniformToFill` | Sets how the background image is resized to fill the window. Possible values: `"none"`, `"fill"`, `"uniform"`, `"uniformToFill"` |
| `closeOnExit` | Optional | Boolean | `true` | When set to `true`, the selected tab closes when `exit` is typed. When set to `false`, the tab will remain open when `exit` is typed. |
| `colorScheme` | Optional | String | `Campbell` | Name of the terminal color scheme to use. Color schemes are defined under `schemes`. |
| `colorTable` | Optional | Array[String] | | Array of colors used in the profile if `colorscheme` is not set. Array follows the format defined in `schemes`. |
| `commandline` | Optional | String | | Executable used in the profile. |
| `cursorColor` | Optional | String | `#FFFFFF` | Sets the cursor color for the profile. Uses hex color format: `"#rrggbb"`. |
| `cursorHeight` | Optional | Integer | | Sets the percentage height of the cursor starting from the bottom. Only works when `cursorShape` is set to `"vintage"`. Accepts values from 25-100. |
| `cursorShape` | Optional | String | `bar` | Sets the cursor shape for the profile. Possible values: `"vintage"` ( &#x2583; ), `"bar"` ( &#x2503; ), `"underscore"` ( &#x2581; ), `"filledBox"` ( &#x2588; ), `"emptyBox"` ( &#x25AF; ) |
| `fontFace` | Optional | String | `Consolas` | Name of the font face used in the profile. We will try to fallback to Consolas if this can't be found or is invalid. |
| `fontSize` | Optional | Integer | `12` | Sets the font size. |
| `foreground` | Optional | String | | Sets the foreground color of the profile. Overrides `foreground` set in color scheme if `colorscheme` is set. Uses hex color format: `#rgb` or `"#rrggbb"`. |
| `hidden` | Optional | Boolean | `false` | If set to true, the profile will not appear in the list of profiles. This can be used to hide default profiles and dynamicially generated profiles, while leaving them in your settings file. |
| `historySize` | Optional | Integer | `9001` | The number of lines above the ones displayed in the window you can scroll back to. |
| `icon` | Optional | String | | Image file location of the icon used in the profile. Displays within the tab and the dropdown menu. |
| `padding` | Optional | String | `8, 8, 8, 8` | Sets the padding around the text within the window. Can have three different formats: `"#"` sets the same padding for all sides, `"#, #"` sets the same padding for left-right and top-bottom, and `"#, #, #, #"` sets the padding individually for left, top, right, and bottom. |
| `scrollbarState` | Optional | String | | Defines the visibility of the scrollbar. Possible values: `"visible"`, `"hidden"` |
| `selectionBackground` | Optional | String | Sets the selection background color of the profile. Overrides `selectionBackground` set in color scheme if `colorscheme` is set. Uses hex color format: `"#rrggbb"`. |
| `snapOnInput` | Optional | Boolean | `true` | When set to `true`, the window will scroll to the command input line when typing. When set to `false`, the window will not scroll when you start typing. |
| `source` | Optional | String | | Stores the name of the profile generator that originated this profile. _There are no discoverable values for this field._ |
| `startingDirectory` | Optional | String | `%USERPROFILE%` | The directory the shell starts in when it is loaded. |
| `suppressApplicationTitle` | Optional | Boolean | | When set to `true`, `tabTitle` overrides the default title of the tab and any title change messages from the application will be suppressed. When set to `false`, `tabTitle` behaves as normal. |
| `tabTitle` | Optional | String | | If set, will replace the `name` as the title to pass to the shell on startup. Some shells (like `bash`) may choose to ignore this initial value, while others (`cmd`, `powershell`) may use this value over the lifetime of the application.  |
| `useAcrylic` | Optional | Boolean | `false` | When set to `true`, the window will have an acrylic background. When set to `false`, the window will have a plain, untextured background. |

## Schemes
Properties listed below are specific to each color scheme. [ColorTool](https://github.com/microsoft/terminal/tree/master/src/tools/ColorTool) is a great tool you can use to create and explore new color schemes. All colors use hex color format.

| Property | Necessity | Type | Description |
| -------- | ---- | ----------- | ----------- |
| `name` | _Required_ | String | Name of the color scheme. |
| `foreground` | _Required_ | String | Sets the foreground color of the color scheme. |
| `background` | _Required_ | String | Sets the background color of the color scheme. |
| `selectionBackground` | Optional | String | Sets the selection background color of the color scheme. |
| `black` | _Required_ | String | Sets the color used as ANSI black. |
| `blue` | _Required_ | String | Sets the color used as ANSI blue. |
| `brightBlack` | _Required_ | String | Sets the color used as ANSI bright black. |
| `brightBlue` | _Required_ | String | Sets the color used as ANSI bright blue. |
| `brightCyan` | _Required_ | String | Sets the color used as ANSI bright cyan. |
| `brightGreen` | _Required_ | String | Sets the color used as ANSI bright green. |
| `brightPurple` | _Required_ | String | Sets the color used as ANSI bright purple. |
| `brightRed` | _Required_ | String | Sets the color used as ANSI bright red. |
| `brightWhite` | _Required_ | String | Sets the color used as ANSI bright white. |
| `brightYellow` | _Required_ | String | Sets the color used as ANSI bright yellow. |
| `cyan` | _Required_ | String | Sets the color used as ANSI cyan. |
| `green` | _Required_ | String | Sets the color used as ANSI green. |
| `purple` | _Required_ | String | Sets the color used as ANSI purple. |
| `red` | _Required_ | String | Sets the color used as ANSI red. |
| `white` | _Required_ | String | Sets the color used as ANSI white. |
| `yellow` | _Required_ | String | Sets the color used as ANSI yellow. |

## Keybindings
Properties listed below are specific to each custom key binding.

| Property | Necessity | Type | Description |
| -------- | ---- | ----------- | ----------- |
| `command` | _Required_ | String | The command executed when the associated key bindings are pressed. |
| `keys` | _Required_ | Array[String] | Defines the key combinations used to call the command. |

### Implemented Keybindings

Bindings listed below are per the implementation in `src/cascadia/TerminalApp/AppKeyBindingsSerialization.cpp`

- copy
- copyTextWithoutNewlines
- paste
- newTab
- openNewTabDropdown
- duplicateTab
- newTabProfile0
- newTabProfile1
- newTabProfile2
- newTabProfile3
- newTabProfile4
- newTabProfile5
- newTabProfile6
- newTabProfile7
- newTabProfile8
- closeWindow
- closeTab
- closePane
- switchToTab
- nextTab
- prevTab
- increaseFontSize
- decreaseFontSize
- scrollUp
- scrollDown
- scrollUpPage
- scrollDownPage
- switchToTab0
- switchToTab1
- switchToTab2
- switchToTab3
- switchToTab4
- switchToTab5
- switchToTab6
- switchToTab7
- switchToTab8
- openSettings
- splitHorizontal
- splitVertical
- resizePaneLeft
- resizePaneRight
- resizePaneUp
- resizePaneDown
- moveFocusLeft
- moveFocusRight
- moveFocusUp
- moveFocusDown
- toggleFullscreen

## Background Images and Icons
Some Terminal settings allow you to specify custom background images and icons. It is recommended that custom images and icons are stored in system-provided folders and are referred to using the correct [URI Schemes](https://docs.microsoft.com/en-us/windows/uwp/app-resources/uri-schemes). URI Schemes provide a way to reference files independent of their physical paths (which may change in the future).

The most useful URI schemes to remember when customizing background images and icons are:

| URI Scheme | Corresponding Physical Path | Use / description |
| --- | --- | ---|
| `ms-appdata:///Local/` | `%localappdata%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\` | Per-machine files |
| `ms-appdata:///Roaming/` | `%localappdata%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\RoamingState\` | Common files |

> ⚠ Note: Do not rely on file references using the `ms-appx` URI Scheme (i.e. icons). These files are considered an internal implementation detail and may change name/location or may be omitted in the future.

### Icons
Terminal displays icons for each of your profiles which Terminal generates for any built-in shells - PowerShell Core, PowerShell, and any installed Linux/WSL distros. Each profile refers to a stock icon via the `ms-appx` URI Scheme.

> ⚠ Note: Do not rely on the files referenced by the `ms-appx` URI Scheme - they are considered an internal implementation detail and may change name/location or may be omitted in the future.

You can refer to you own icons if you wish, e.g.:

```json
    "icon" : "C:\\Users\\richturn\\OneDrive\\WindowsTerminal\\icon-ubuntu-32.png",
```

> 👉 Tip: Icons should be sized to 32x32px in an appropriate raster image format (e.g. .PNG, .GIF, or .ICO) to avoid having to scale your icons during runtime (causing a noticeable delay and loss of quality.)

### Custom Background Images
You can apply a background image to each of your profiles, allowing you to configure/brand/style each of your profiles independently from one another if you wish.

To do so, specify your preferred `backgroundImage`, position it using `backgroundImageAlignment`, set its opacity with `backgroundImageOpacity`, and/or specify how your image fill the available space using `backgroundImageStretchMode`.

For example:
```json
    "backgroundImage": "C:\\Users\\richturn\\OneDrive\\WindowsTerminal\\bg-ubuntu-256.png",
    "backgroundImageAlignment": "bottomRight",
    "backgroundImageOpacity": 0.1,
    "backgroundImageStretchMode": "none"
```

> 👉 Tip: You can easily roam your collection of images and icons across all your machines by storing your icons and images in OneDrive (as shown above).

With these settings, your Terminal's Ubuntu profile would look similar to this:

![Custom icon and background image](../images/custom-icon-and-background-image.jpg)
