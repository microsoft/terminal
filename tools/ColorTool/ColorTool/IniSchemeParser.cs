//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Text;
using System.Runtime.InteropServices;
using static ColorTool.ConsoleAPI;
using System.IO;

namespace ColorTool
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

        // TODO: Abstract the locating of a scheme into a function the implementation can call into
        //      Both parsers duplicate the searching, they should just pass in their extension and
        //      a callback for initally validating the file
        static string FindIniScheme(string schemeName)
        {
            string exeDir = System.IO.Directory.GetParent(System.Reflection.Assembly.GetEntryAssembly().Location).FullName;
            string filename = schemeName + ".ini";
            string exeSchemes = exeDir + "/schemes/";
            string cwd = "./";
            string cwdSchemes = "./schemes/";
            // Search order, for argument "name", where 'exe' is the dir of the exe.
            //  1. ./name
            //  2. ./name.ini
            //  3. ./schemes/name
            //  4. ./schemes/name.ini
            //  5. exe/schemes/name
            //  6. exe/schemes/name.ini
            //  7. name (as an absolute path)
            string[] paths = {
                cwd + schemeName,
                cwd + filename,
                cwdSchemes + schemeName,
                cwdSchemes + filename,
                exeSchemes + schemeName,
                exeSchemes + filename,
                schemeName,
            };
            foreach (string path in paths)
            {
                if (File.Exists(path))
                {
                    return path;
                }
            }
            return null;
        }

        public ColorScheme ParseScheme(string schemeName, bool reportErrors = true)
        {
            bool success = true;

            string filename = FindIniScheme(schemeName);
            if (filename == null) return null;

            string[] tableStrings = new string[COLOR_TABLE_SIZE];
            uint[] colorTable = null;

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
                return new ColorScheme { colorTable = colorTable };
            }
            else
            {
                return null;
            }
        }
    }
}
