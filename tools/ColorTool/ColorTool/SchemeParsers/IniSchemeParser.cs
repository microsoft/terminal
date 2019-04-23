//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.IO;
using System.Collections.Generic;
using static ColorTool.ConsoleAPI;

namespace ColorTool.SchemeParsers
{
    class IniSchemeParser : ISchemeParser
    {
        [DllImport("kernel32")]
        private static extern int GetPrivateProfileString(string section, string key, string def, StringBuilder retVal, int size, string filePath);

        // These are in Windows Color table order - BRG, not RGB.
        public static string[] COLOR_NAMES = {
            "DARK_BLACK",
            "DARK_BLUE",
            "DARK_GREEN",
            "DARK_CYAN",
            "DARK_RED",
            "DARK_MAGENTA",
            "DARK_YELLOW",
            "DARK_WHITE",
            "BRIGHT_BLACK",
            "BRIGHT_BLUE",
            "BRIGHT_GREEN",
            "BRIGHT_CYAN",
            "BRIGHT_RED",
            "BRIGHT_MAGENTA",
            "BRIGHT_YELLOW",
            "BRIGHT_WHITE"
        };

        public string Name => "INI File Parser";

        static uint ParseHex(string arg)
        {
            System.Drawing.Color col = System.Drawing.ColorTranslator.FromHtml(arg);
            return RGB(col.R, col.G, col.B);
        }

        static uint ParseRgb(string arg)
        {
            int[] components = { 0, 0, 0 };
            string[] args = arg.Split(',');
            if (args.Length != components.Length) throw new Exception("Invalid color format \"" + arg + "\"");
            if (args.Length != 3) throw new Exception("Invalid color format \"" + arg + "\"");
            for (int i = 0; i < args.Length; i++)
            {
                components[i] = Int32.Parse(args[i]);
            }

            return RGB(components[0], components[1], components[2]);
        }

        static uint ParseColor(string arg)
        {
            if (arg[0] == '#')
            {
                return ParseHex(arg.Substring(1));
            }
            else
            {
                return ParseRgb(arg);
            }
        }

        static string FindIniScheme(string schemeName)
        {
            return SchemeManager.GetSearchPaths(schemeName, ".ini").FirstOrDefault(File.Exists);
        }

        public ColorScheme ParseScheme(string schemeName, bool reportErrors = false)
        {
            bool success = true;

            string filename = FindIniScheme(schemeName);
            if (filename == null) return null;

            string[] tableStrings = new string[COLOR_TABLE_SIZE];
            uint[] colorTable = null;
            uint? foregroundColor = null;
            uint? backgroundColor = null;
            uint? popupForegroundColor = null;
            uint? popupBackgroundColor = null;

            for (int i = 0; i < COLOR_TABLE_SIZE; i++)
            {
                string name = COLOR_NAMES[i];
                StringBuilder buffer = new StringBuilder(512);
                GetPrivateProfileString("table", name, null, buffer, 512, filename);

                tableStrings[i] = buffer.ToString();
                if (tableStrings[i].Length <= 0)
                {
                    success = false;
                    if (reportErrors)
                    {
                        Console.WriteLine(string.Format(Resources.IniParseError, filename, name, tableStrings[i]));
                    }
                    break;
                }
            }

            if (success)
            {
                try
                {
                    colorTable = new uint[COLOR_TABLE_SIZE];
                    for (int i = 0; i < COLOR_TABLE_SIZE; i++)
                    {
                        colorTable[i] = ParseColor(tableStrings[i]);
                    }

                    if (ReadAttributes("popup", out var foreground, out var background))
                    {
                        var foregroundIndex = (COLOR_NAMES as IList<string>).IndexOf(foreground);
                        var backgroundIndex = (COLOR_NAMES as IList<string>).IndexOf(background);
                        if (foregroundIndex != -1 && backgroundIndex != -1)
                        {
                            popupForegroundColor = colorTable[foregroundIndex];
                            popupBackgroundColor = colorTable[backgroundIndex];
                        }
                    }

                    if (ReadAttributes("screen", out foreground, out background))
                    {
                        var foregroundIndex = (COLOR_NAMES as IList<string>).IndexOf(foreground);
                        var backgroundIndex = (COLOR_NAMES as IList<string>).IndexOf(background);
                        if (foregroundIndex != -1 && backgroundIndex != -1)
                        {
                            foregroundColor = colorTable[foregroundIndex];
                            backgroundColor = colorTable[backgroundIndex];
                        }
                    }
                }
                catch (Exception /*e*/)
                {
                    if (reportErrors)
                    {
                        Console.WriteLine(string.Format(Resources.IniLoadError, filename));
                    }

                    colorTable = null;
                }
            }

            if (colorTable != null)
            {
                var consoleAttributes = new ConsoleAttributes { background = backgroundColor, foreground = foregroundColor, popupBackground = popupBackgroundColor, popupForeground = popupForegroundColor };
                return new ColorScheme { colorTable = colorTable, consoleAttributes = consoleAttributes };
            }
            else
            {
                return null;
            }

            bool ReadAttributes(string section, out string foreground, out string background)
            {
                foreground = null;
                background = null;

                StringBuilder buffer = new StringBuilder(512);
                GetPrivateProfileString(section, "FOREGROUND", null, buffer, 512, filename);
                foreground = buffer.ToString();
                if (!COLOR_NAMES.Contains(foreground))
                    return false;


                buffer = new StringBuilder(512);
                GetPrivateProfileString(section, "BACKGROUND", null, buffer, 512, filename);
                background = buffer.ToString();
                if (!COLOR_NAMES.Contains(background))
                    return false;

                return true;
            }
        }
    }
}
