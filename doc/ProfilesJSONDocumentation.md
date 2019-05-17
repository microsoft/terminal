# Profiles.json Documentation

| Property | Type | Description |
| -------- | ---- | ----------- |
| **Global** |  | **Properties listed below affect the entire window, regardless of the property settings.** |
| alwaysShowTabs | Boolean | When set to `true`, tabs are always displayed. When set to `false` and `experimental_showTabsInTitlebar` is set to `false`, tabs only appear after typing <kbd>Ctrl</kbd> + <kbd>T</kbd>. |
| defaultProfile | String | Sets the default profile. Opens by typing <kbd>Ctrl</kbd> + <kbd>T</kbd> or by clicking the '+' icon. The guid of the desired default profile is used as the value. |
| experimental_showTabsInTitlebar | Boolean | When set to `true`, the tabs are moved into the titlebar and the titlebar disappears. When set to `false`, the titlebar sits above the tabs. |
| initalCols | Integer | The number of columns displayed in the window upon first load. |
| initialRows | Integer | The number of rows displayed in the window upon first load. |
| requestedTheme | String | Sets the theme of the tab bar. Possible values: `"light"`, `"dark"`, `"system"` |
| showTerminalTitleInTitlebar | Boolean | When set to `true`, titlebar displays the title of the selected tab. When set to `false`, titlebar displays "Windows Terminal". |
| **profiles** | **Array[Object]** | **Properties listed below are specific to each unique profile.** |
| acrylicOpacity | Integer | Sets the transparency of the window for the profile. Accepts values from 0-1. |
| background | String | Sets the background color of the profile. Overrides `background` set in color scheme if `colorscheme` is set. |
| closeOnExit | Boolean | When set to `true`, the selected tab closes when `exit` is typed. When set to `false`, the tab will remain open when `exit` is typed. |
| colorscheme | String | Name of the color scheme used in the profile that is defined under `schemes`. |
| colorTable | Array[String] | Array of colors used in the profile if `colorscheme` is not set. Ordering is as follows: `[black, red, green, yellow, blue, magenta, cyan, white, bright black, bright red, bright green, bright yellow, bright blue, bright magenta, bright cyan, bright white]` |
| commandline | String | Executable used in the profile. |
| cursorColor | String | Sets the cursor color for the profile. |
| cursorHeight | Integer | Sets the height of the cursor. Only works when `cursorShape` is set to `"vintage"`. Accepts values from 25-100. |
| cursorShape | String | Sets the cursor shape for the profile. Possible values: `"vintage"`, `"bar"`, `"underscore"`, `"filledBox"`, `"emptyBox"` |
| fontFace | String | Name of the font face used in the profile. |
| fontSize | Integer | Sets the font size. |
| foreground | String | Sets the foreground color of the profile. Overrides `foreground` set in color scheme if `colorscheme` is set. |
| guid | String | Unique identifier of the profile. |
| historySize | Integer | The number of lines above the ones displayed in the window you can scroll back to. |
| icon | String | Image file location of the icon used in the profile. Displays within the tab and the dropdown menu. |
| name | String | Name of the profile. Displays in the dropdown menu. |
| padding | String | Sets the padding around the text within the window. Can have three different formats: `"#"` sets the same padding for all sides, `"#, #"` sets the same padding for top-bottom and right-left, and `"#, #, #, #"` sets the padding individually for top, right, bottom, and left. |
| scrollbarState | String | Defines the visibility of the scrollbar. Possible values: `"visible"`, `"hidden"` |
| snapOnInput | Boolean | When set to `true`, the window will scroll to the command input line when typing. When set to `false`, the window will not scroll when you start typing. |
| startingDirectory | String | The directory the shell starts in when it is loaded. |
| useAcrylic | Boolean | When set to `true`, the window will have an acrylic background. When set to `false`, the window will have a plain, untextured background. |
| **schemes** | **Array[Object]** | **Properties listed below are specific to each color theme.** |
| background | String | Sets the background color of the color scheme. |
| colors | Array[String] | Array of colors used in the color scheme. Ordering is as follows: `[black, red, green, yellow, blue, magenta, cyan, white, bright black, bright red, bright green, bright yellow, bright blue, bright magenta, bright cyan, bright white]` |
| foreground | String | Sets the foreground color of the color scheme. |
| name | String | Name of the color scheme. |