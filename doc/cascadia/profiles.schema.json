{
  "$id": "https://github.com/microsoft/terminal/blob/master/doc/cascadia/profiles.schema.json",
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Microsoft's Windows Terminal Settings Profile Schema'",
  "definitions": {
    "Color": {
      "default": "#",
      "pattern": "^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$",
      "type": "string"
    },
    "ProfileGuid": {
      "default": "{}",
      "pattern": "^\\{[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}\\}$",
      "type": "string"
    },
    "Keybinding": {
      "additionalProperties": false,
      "properties": {
        "command": {
          "description": "The command executed when the associated key bindings are pressed.",
          "enum": [
            "closePane",
            "closeTab",
            "closeWindow",
            "copy",
            "copyTextWithoutNewlines",
            "decreaseFontSize",
            "duplicateTab",
            "increaseFontSize",
            "moveFocusDown",
            "moveFocusLeft",
            "moveFocusRight",
            "moveFocusUp",
            "newTab",
            "newTabProfile0",
            "newTabProfile1",
            "newTabProfile2",
            "newTabProfile3",
            "newTabProfile4",
            "newTabProfile5",
            "newTabProfile6",
            "newTabProfile7",
            "newTabProfile8",
            "nextTab",
            "openNewTabDropdown",
            "openSettings",
            "paste",
            "prevTab",
            "resizePaneDown",
            "resizePaneLeft",
            "resizePaneRight",
            "resizePaneUp",
            "scrollDown",
            "scrollDownPage",
            "scrollUp",
            "scrollUpPage",
            "splitHorizontal",
            "splitVertical",
            "switchToTab",
            "switchToTab0",
            "switchToTab1",
            "switchToTab2",
            "switchToTab3",
            "switchToTab4",
            "switchToTab5",
            "switchToTab6",
            "switchToTab7",
            "switchToTab8",
            "toggleFullscreen"
          ],
          "type": "string"
        },
        "keys": {
          "description": "Defines the key combinations used to call the command.",
          "items": {
            "pattern": "^(?<modifier>(ctrl|alt|shift)\\+?((ctrl|alt|shift)(?<!\\2)\\+?)?((ctrl|alt|shift)(?<!\\2|\\4))?\\+?)?(?<key>[^+\\s]+?)?(?<=[^+\\s])$",
            "type": "string"
          },
          "minItems": 1,
          "type": "array"
        }
      },
      "required": [
        "command",
        "keys"
      ],
      "type": "object"
    },
    "Globals": {
      "additionalProperties": true,
      "description": "Properties that affect the entire window, regardless of the profile settings.",
      "properties": {
        "alwaysShowTabs": {
          "default": true,
          "description": "When set to true, tabs are always displayed. When set to false and showTabsInTitlebar is set to false, tabs only appear after opening a new tab.",
          "type": "boolean"
        },
        "copyOnSelect": {
          "default": false,
          "description": "When set to true, a selection is immediately copied to your clipboard upon creation. When set to false, the selection persists and awaits further action.",
          "type": "boolean"
        },
        "defaultProfile": {
          "$ref": "#/definitions/ProfileGuid",
          "description": "Sets the default profile. Opens by clicking the '+' icon or typing the key binding assigned to 'newTab'. The guid of the desired default profile is used as the value."
        },
        "initialCols": {
          "default": 120,
          "description": "The number of columns displayed in the window upon first load.",
          "maximum": 999,
          "minimum": 1,
          "type": "integer"
        },
        "initialRows": {
          "default": 30,
          "description": "The number of rows displayed in the window upon first load.",
          "maximum": 999,
          "minimum": 1,
          "type": "integer"
        },
        "keybindings": {
          "description": "Properties are specific to each custom key binding.",
          "items": {
            "$ref": "#/definitions/Keybinding"
          },
          "type": "array"
        },
        "requestedTheme": {
          "default": "system",
          "description": "Sets the theme of the application.",
          "enum": [
            "light",
            "dark",
            "system"
          ],
          "type": "string"
        },
        "showTabsInTitlebar": {
          "default": true,
          "description": "When set to true, the tabs are moved into the titlebar and the titlebar disappears. When set to false, the titlebar sits above the tabs.",
          "type": "boolean"
        },
        "showTerminalTitleInTitlebar": {
          "default": true,
          "description": "When set to true, titlebar displays the title of the selected tab. When set to false, titlebar displays 'Windows Terminal'.",
          "type": "boolean"
        },
        "wordDelimiters": {
          "default": " ./\\()\"'-:,.;<>~!@#$%^&*|+=[]{}~?│",
          "description": "Determines the delimiters used in a double click selection.",
          "type": "string"
        }
      },
      "required": [
        "defaultProfile"
      ],
      "type": "object"
    },
    "ProfileList": {
      "description": "Properties are specific to each unique profile.",
      "items": {
        "additionalProperties": false,
        "properties": {
          "acrylicOpacity": {
            "default": 0.5,
            "description": "When useAcrylic is set to true, it sets the transparency of the window for the profile. Accepts floating point values from 0-1 (default 0.5).",
            "maximum": 1,
            "minimum": 0,
            "type": "number"
          },
          "background": {
            "$ref": "#/definitions/Color",
            "description": "Sets the background color of the profile. Overrides background set in color scheme if colorscheme is set. Uses hex color format: \"#rrggbb\". Default \"#000000\" (black)."
          },
          "backgroundImage": {
            "description": "Sets the file location of the Image to draw over the window background.",
            "type": "string"
          },
          "backgroundImageAlignment": {
            "default": "center",
            "enum": [
              "bottom",
              "bottomLeft",
              "bottomRight",
              "center",
              "left",
              "right",
              "top",
              "topLeft",
              "topRight"
            ],
            "type": "string"
          },
          "backgroundImageOpacity": {
            "description": "(Not in SettingsSchema.md)",
            "maximum": 1,
            "minimum": 0,
            "type": "number"
          },
          "backgroundImageStretchMode": {
            "default": "uniformToFill",
            "description": "Sets how the background image is resized to fill the window.",
            "enum": [
              "fill",
              "none",
              "uniform",
              "uniformToFill"
            ],
            "type": "string"
          },
          "closeOnExit": {
            "default": true,
            "description": "When set to true (default), the selected tab closes when the connected application exits. When set to false, the tab will remain open when the connected application exits.",
            "type": "boolean"
          },
          "colorScheme": {
            "default": "Campbell",
            "description": "Name of the terminal color scheme to use. Color schemes are defined under \"schemes\".",
            "type": "string"
          },
          "colorTable": {
            "description": "Array of colors used in the profile if colorscheme is not set. Colors use hex color format: \"#rrggbb\". Ordering is as follows: [black, red, green, yellow, blue, magenta, cyan, white, bright black, bright red, bright green, bright yellow, bright blue, bright magenta, bright cyan, bright white]",
            "items": {
              "additionalProperties": false,
              "properties": {
                "background": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the background color of the color table."
                },
                "black": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI black."
                },
                "blue": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI blue."
                },
                "brightBlack": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright black."
                },
                "brightBlue": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright blue."
                },
                "brightCyan": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright cyan."
                },
                "brightGreen": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright green."
                },
                "brightPurple": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright purple."
                },
                "brightRed": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright red."
                },
                "brightWhite": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright white."
                },
                "brightYellow": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI bright yellow."
                },
                "cyan": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI cyan."
                },
                "foreground": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the foreground color of the color table."
                },
                "green": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI green."
                },
                "purple": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI purple."
                },
                "red": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI red."
                },
                "white": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI white."
                },
                "yellow": {
                  "$ref": "#/definitions/Color",
                  "description": "Sets the color used as ANSI yellow."
                }
              },
              "type": "object"
            },
            "type": "array"
          },
          "commandline": {
            "description": "Executable used in the profile.",
            "type": "string"
          },
          "connectionType": {
            "$ref": "#/definitions/ProfileGuid",
            "description": "A GUID reference to a connection type. Currently undocumented as of 0.3, this is used for Azure Cloud Shell"
          },
          "cursorColor": {
            "$ref": "#/definitions/Color",
            "default": "#FFFFFF",
            "description": "Sets the cursor color for the profile. Uses hex color format: \"#rrggbb\"."
          },
          "cursorHeight": {
            "description": "Sets the percentage height of the cursor starting from the bottom. Only works when cursorShape is set to \"vintage\". Accepts values from 25-100.",
            "maximum": 100,
            "minimum": 25,
            "type": "integer"
          },
          "cursorShape": {
            "default": "bar",
            "description": "Sets the cursor shape for the profile. Possible values: \"vintage\" ( ▃ ), \"bar\" ( ┃, default ), \"underscore\" ( ▁ ), \"filledBox\" ( █ ), \"emptyBox\" ( ▯ )",
            "enum": [
              "bar",
              "emptyBox",
              "filledBox",
              "underscore",
              "vintage"
            ],
            "type": "string"
          },
          "fontFace": {
            "default": "Consolas",
            "description": "Name of the font face used in the profile.",
            "type": "string"
          },
          "fontSize": {
            "default": 12,
            "description": "Sets the font size.",
            "minimum": 1,
            "type": "integer"
          },
          "foreground": {
            "$ref": "#/definitions/Color",
            "description": "Sets the foreground color of the profile. Overrides foreground set in color scheme if colorscheme is set. Uses hex color format: \"#rrggbb\". Default \"#ffffff\" (white)."
          },
          "guid": {
            "$ref": "#/definitions/ProfileGuid",
            "description": "Unique identifier of the profile. Written in registry format: \"{00000000-0000-0000-0000-000000000000}\"."
          },
          "hidden": {
            "default": false,
            "description": "If set to true, the profile will not appear in the list of profiles. This can be used to hide default profiles and dynamicially generated profiles, while leaving them in your settings file.",
            "type": "boolean"
          },
          "historySize": {
            "default": 9001,
            "description": "The number of lines above the ones displayed in the window you can scroll back to.",
            "minimum": -1,
            "type": "integer"
          },
          "icon": {
            "description": "Image file location of the icon used in the profile. Displays within the tab and the dropdown menu.",
            "type": "string"
          },
          "name": {
            "description": "Name of the profile. Displays in the dropdown menu.",
            "minLength": 1,
            "type": "string"
          },
          "padding": {
            "default": "8, 8, 8, 8",
            "description": "Sets the padding around the text within the window. Can have three different formats: \"#\" sets the same padding for all sides, \"#, #\" sets the same padding for left-right and top-bottom, and \"#, #, #, #\" sets the padding individually for left, top, right, and bottom.",
            "pattern": "^-?[0-9]+(\\.[0-9]+)?( *, *-?[0-9]+(\\.[0-9]+)?|( *, *-?[0-9]+(\\.[0-9]+)?){3})?$",
            "type": "string"
          },
          "scrollbarState": {
            "default": "visible",
            "description": "Defines the visibility of the scrollbar.",
            "enum": [
              "visible",
              "hidden"
            ],
            "type": "string"
          },
          "snapOnInput": {
            "default": true,
            "description": "When set to true, the window will scroll to the command input line when typing. When set to false, the window will not scroll when you start typing.",
            "type": "boolean"
          },
          "source": {
            "description": "Stores the name of the profile generator that originated this profile.",
            "type": "string"
          },
          "startingDirectory": {
            "description": "The directory the shell starts in when it is loaded.",
            "type": "string"
          },
          "tabTitle": {
            "description": "If set, will replace the name as the title to pass to the shell on startup. Some shells (like bash) may choose to ignore this initial value, while others (cmd, powershell) may use this value over the lifetime of the application.",
            "type": "string"
          },
          "useAcrylic": {
            "default": false,
            "description": "When set to true, the window will have an acrylic background. When set to false, the window will have a plain, untextured background.",
            "type": "boolean"
          }
        },
        "required": [
          "guid",
          "name"
        ],
        "type": "object"
      },
      "type": "array"
    },
    "SchemeList": {
      "description": "Properties are specific to each color scheme. ColorTool is a great tool you can use to create and explore new color schemes. All colors use hex color format.",
      "items": {
        "additionalProperties": false,
        "properties": {
          "name": {
            "description": "Name of the color scheme.",
            "minLength": 1,
            "type": "string"
          },
          "background": {
            "$ref": "#/definitions/Color",
            "description": "Sets the background color of the color scheme."
          },
          "black": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI black."
          },
          "blue": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI blue."
          },
          "brightBlack": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright black."
          },
          "brightBlue": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright blue."
          },
          "brightCyan": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright cyan."
          },
          "brightGreen": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright green."
          },
          "brightPurple": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright purple."
          },
          "brightRed": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright red."
          },
          "brightWhite": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright white."
          },
          "brightYellow": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI bright yellow."
          },
          "cyan": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI cyan."
          },
          "foreground": {
            "$ref": "#/definitions/Color",
            "description": "Sets the foreground color of the color scheme."
          },
          "green": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI green."
          },
          "purple": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI purple."
          },
          "red": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI red."
          },
          "white": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI white."
          },
          "yellow": {
            "$ref": "#/definitions/Color",
            "description": "Sets the color used as ANSI yellow."
          }
        },
        "type": "object"
      },
      "type": "array"
    }
  },
  "oneOf": [
    {
      "allOf": [
        { "$ref": "#/definitions/Globals" },
        {
          "additionalItems": true,
          "properties": {
            "profiles": { "$ref": "#/definitions/ProfileList" },
            "schemes": { "$ref": "#/definitions/SchemeList" }
          },
          "required": [
            "profiles",
            "schemes",
            "defaultProfile"
          ]
        }
      ]
    },
    {
      "additionalItems": false,
      "properties": {
        "globals": { "$ref": "#/definitions/Globals" },
        "profiles": { "$ref": "#/definitions/ProfileList" },
        "schemes": { "$ref": "#/definitions/SchemeList" }
      },
      "required": [
        "profiles",
        "schemes",
        "globals"
      ]
    }
  ]
}
