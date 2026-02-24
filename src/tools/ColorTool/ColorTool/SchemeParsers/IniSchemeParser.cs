//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using static ColorTool.ConsoleAPI;

namespace ColorTool.SchemeParsers
{
    class IniSchemeParser : SchemeParserBase
    {
        [DllImport("kernel32")]
        private static extern int GetPrivateProfileString(string section, string key, string def, StringBuilder retVal, int size, string filePath);

        public override string FileExtension { get; } = ".ini";

        // These are in Windows Color table order - BRG, not RGB.
        internal static readonly IReadOnlyList<string> ColorNames = new[]
        {
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

        public override string Name { get; } = "INI File Parser";

        public override ColorScheme ParseScheme(string schemeName, bool reportErrors = false)
        {
            bool success = true;

            string filename = FindIniScheme(schemeName);
            if (filename == null) return null;

            string[] tableStrings = new string[ColorTableSize];
            uint[] colorTable = null;
            uint? foregroundColor = null;
            uint? backgroundColor = null;
            uint? popupForegroundColor = null;
            uint? popupBackgroundColor = null;
            uint? cursorColor = null;

            for (int i = 0; i < ColorTableSize; i++)
            {
                string name = ColorNames[i];
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
                    colorTable = new uint[ColorTableSize];
                    for (int i = 0; i < ColorTableSize; i++)
                    {
                        colorTable[i] = ParseColor(tableStrings[i]);
                    }

                    if (ReadAttributes("popup", out var foreground, out var background, out var cursor))
                    {
                        var foregroundIndex = (ColorNames as IList<string>).IndexOf(foreground);
                        var backgroundIndex = (ColorNames as IList<string>).IndexOf(background);
                        if (foregroundIndex != -1 && backgroundIndex != -1)
                        {
                            popupForegroundColor = colorTable[foregroundIndex];
                            popupBackgroundColor = colorTable[backgroundIndex];
                        }
                    }

                    if (ReadAttributes("screen", out foreground, out background, out cursor))
                    {
                        var foregroundIndex = (ColorNames as IList<string>).IndexOf(foreground);
                        var backgroundIndex = (ColorNames as IList<string>).IndexOf(background);
                        if (foregroundIndex != -1)
                        {
                            foregroundColor = colorTable[foregroundIndex];
                        }
                        else
                        {
                            foregroundColor = ParseColor(foreground);
                        }
                        if (backgroundIndex != -1)
                        {
                            backgroundColor = colorTable[backgroundIndex];
                        }
                        else
                        {
                            backgroundColor = ParseColor(background);
                        }
                        if (cursor.Length > 1)
                        {
                            cursorColor = ParseColor(cursor);
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
                var consoleAttributes = new ConsoleAttributes(backgroundColor, foregroundColor, popupBackgroundColor, popupForegroundColor, cursorColor);
                return new ColorScheme(ExtractSchemeName(schemeName), colorTable, consoleAttributes);
            }
            else
            {
                return null;
            }

            bool ReadAttributes(string section, out string foreground, out string background, out string cursor)
            {
                foreground = null;
                background = null;
                cursor = null;

                StringBuilder buffer = new StringBuilder(512);
                GetPrivateProfileString(section, "FOREGROUND", null, buffer, 512, filename);
                foreground = buffer.ToString();
                if (foreground.Length <= 1)
                    return false;


                buffer = new StringBuilder(512);
                GetPrivateProfileString(section, "BACKGROUND", null, buffer, 512, filename);
                background = buffer.ToString();
                if (background.Length <= 1)
                    return false;

                buffer = new StringBuilder(512);
                GetPrivateProfileString(section, "CURSOR", null, buffer, 512, filename);
                cursor = buffer.ToString();

                return true;
            }
        }

        private static uint ParseHex(string arg)
        {
            System.Drawing.Color col = System.Drawing.ColorTranslator.FromHtml(arg);
            return RGB(col.R, col.G, col.B);
        }

        private static uint ParseRgb(string arg)
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

        private static uint ParseColor(string arg)
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

        private string FindIniScheme(string schemeName)
        {
            return SchemeManager.GetSearchPaths(schemeName, FileExtension).FirstOrDefault(File.Exists);
        }
    }
}
