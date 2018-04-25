//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using static ColorTool.ConsoleAPI;
using Microsoft.Win32;
using System.Reflection;

namespace ColorTool
{
    class Program
    {
        static int DARK_BLACK = 0;
        static int DARK_BLUE = 1;
        static int DARK_GREEN = 2;
        static int DARK_CYAN = 3;
        static int DARK_RED = 4;
        static int DARK_MAGENTA = 5;
        static int DARK_YELLOW = 6;
        static int DARK_WHITE = 7;
        static int BRIGHT_BLACK = 8;
        static int BRIGHT_BLUE = 9;
        static int BRIGHT_GREEN = 10;
        static int BRIGHT_CYAN = 11;
        static int BRIGHT_RED = 12;
        static int BRIGHT_MAGENTA = 13;
        static int BRIGHT_YELLOW = 14;
        static int BRIGHT_WHITE = 15;

        static int[] saneFgs = {
            DARK_BLACK     ,
            DARK_RED       ,
            DARK_GREEN     ,
            DARK_YELLOW    ,
            DARK_BLUE      ,
            DARK_MAGENTA   ,
            DARK_CYAN      ,
            DARK_WHITE     ,
            BRIGHT_BLACK   ,
            BRIGHT_RED     ,
            BRIGHT_GREEN   ,
            BRIGHT_YELLOW  ,
            BRIGHT_MAGENTA ,
            BRIGHT_BLUE    ,
            BRIGHT_CYAN    ,
            BRIGHT_WHITE
        };

        // This is the order of colors when output by the table.
        static int[] outputFgs = {
            BRIGHT_WHITE   ,
            DARK_BLACK     ,
            BRIGHT_BLACK   ,
            DARK_RED       ,
            BRIGHT_RED     ,
            DARK_GREEN     ,
            BRIGHT_GREEN   ,
            DARK_YELLOW    ,
            BRIGHT_YELLOW  ,
            DARK_BLUE      ,
            BRIGHT_BLUE    ,
            DARK_MAGENTA   ,
            BRIGHT_MAGENTA ,
            DARK_CYAN      ,
            BRIGHT_CYAN    ,
            DARK_WHITE     ,
            BRIGHT_WHITE
        };

        static int[] saneBgs = {
            DARK_BLACK     ,
            DARK_RED       ,
            DARK_GREEN     ,
            DARK_YELLOW    ,
            DARK_BLUE      ,
            DARK_MAGENTA   ,
            DARK_CYAN      ,
            DARK_WHITE
        };

        static bool quietMode = false;
        static bool setDefaults = false;
        static bool setProperties = true;

        static void Usage()
        {
            Console.WriteLine(Resources.Usage);
        }

        static void OutputUsage()
        {
            Console.WriteLine(Resources.OutputUsage);
        }

        static void Version()
        {
            //System.Reflection.Assembly.GetEntryAssembly();
            //AssemblyName.GetAssemblyName(@"c:\path\to\file.dll").Version;
            string exePath = System.Reflection.Assembly.GetEntryAssembly().Location;
            Version ver = AssemblyName.GetAssemblyName(exePath).Version;
            Console.WriteLine("colortool v" + ver);
        }

        static void PrintTable()
        {
            ConsoleColor[] colors = (ConsoleColor[])ConsoleColor.GetValues(typeof(ConsoleColor));
            // Save the current background and foreground colors.
            ConsoleColor currentBackground = Console.BackgroundColor;
            ConsoleColor currentForeground = Console.ForegroundColor;
            string test = "  gYw  ";
            string[] FGs = {
                "m",
                "1m",
                "30m",
                "1;30m",
                "31m",
                "1;31m",
                "32m",
                "1;32m",
                "33m",
                "1;33m",
                "34m",
                "1;34m",
                "35m",
                "1;35m",
                "36m",
                "1;36m",
                "37m",
                "1;37m"
            };
            string[] BGs = {
                "m",
                "40m",
                "41m",
                "42m",
                "43m",
                "44m",
                "45m",
                "46m",
                "47m"
            };
            
            Console.Write("\t");
            for (int bg = 0; bg < BGs.Length; bg++)
            {
                if (bg > 0) Console.Write(" ");
                Console.Write("  ");
                Console.Write(bg == 0 ? "   " : BGs[bg]);
                Console.Write("  ");
            }
            Console.WriteLine();

            for (int fg = 0; fg < FGs.Length; fg++)
            {
                Console.ForegroundColor = currentForeground;
                Console.BackgroundColor = currentBackground;

                if (fg >= 0) Console.Write(FGs[fg] + "\t");

                if (fg == 0) Console.ForegroundColor = currentForeground;
                else Console.ForegroundColor = colors[outputFgs[fg - 1]];

                for (int bg = 0; bg < BGs.Length; bg++)
                {
                    if (bg > 0) Console.Write(" ");
                    if (bg == 0)
                        Console.BackgroundColor = currentBackground;
                    else Console.BackgroundColor = colors[saneBgs[bg - 1]];
                    Console.Write(test);
                    Console.BackgroundColor = currentBackground;
                }
                Console.Write("\n");

            }
            Console.Write("\n");
        }

        static bool SetProperties(uint[] colorTable)
        {
            CONSOLE_SCREEN_BUFFER_INFO_EX csbiex = CONSOLE_SCREEN_BUFFER_INFO_EX.Create();
            int STD_OUTPUT_HANDLE = -11;
            IntPtr hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            bool success = GetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            if (success)
            {
                csbiex.srWindow.Bottom++;
                for (int i = 0; i < 16; i++)
                {
                    csbiex.ColorTable[i] = colorTable[i];
                }
                SetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            }
            if (success)
            {
                if (!quietMode)
                {
                    PrintTable();
                }
            }
            return success;
        }

        static bool SetDefaults(uint[] colorTable)
        {
            RegistryKey consoleKey = Registry.CurrentUser.OpenSubKey("Console", true);
            for (int i = 0; i < colorTable.Length; i++)
            {
                string valueName = "ColorTable" + (i < 10 ? "0" : "") + i;
                consoleKey.SetValue(valueName, colorTable[i], RegistryValueKind.DWord);
            }
            Console.WriteLine(Resources.WroteToDefaults);
            return true;
        }

        static bool ExportCurrentAsIni(string outputPath)
        {
            CONSOLE_SCREEN_BUFFER_INFO_EX csbiex = CONSOLE_SCREEN_BUFFER_INFO_EX.Create();
            int STD_OUTPUT_HANDLE = -11;
            IntPtr hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            bool success = GetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            if (success)
            {
                using (System.IO.StreamWriter file = new System.IO.StreamWriter(outputPath))
                {
                    file.WriteLine("[table]");
                    for (int i = 0; i < 16; i++)
                    {
                        string line = IniSchemeParser.COLOR_NAMES[i];
                        line += " = ";
                        uint color = csbiex.ColorTable[i];
                        uint r = color & (0x000000ff);
                        uint g = (color & (0x0000ff00)) >> 8;
                        uint b = (color & (0x00ff0000)) >> 16;
                        line += r + "," + g + "," + b;
                        file.WriteLine(line);
                    }
                }
            }
            else
            {
                Console.WriteLine("Failed to get console information.");
            }
            return success;
        }


        static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                Usage();
                return;
            }
            for (int i = 0; i < args.Length; i++)
            {
                string arg = args[i];
                switch (arg)
                {
                    case "-c":
                    case "--current":
                        PrintTable();
                        return;
                    case "-q":
                    case "--quiet":
                        quietMode = true;
                        break;
                    case "-d":
                    case "--defaults":
                        setDefaults = true;
                        setProperties = false;
                        break;
                    case "-b":
                    case "--both":
                        setDefaults = true;
                        setProperties = true;
                        break;
                    case "-?":
                    case "--help":
                        Usage();
                        return;
                    case "-v":
                    case "--version":
                        Version();
                        return;
                    case "-o":
                    case "--output":
                        if (i+1 < args.Length)
                        {
                            ExportCurrentAsIni(args[i + 1]);
                        }
                        else
                        {
                            OutputUsage();
                        }
                        return;
                    default:
                        break;
                }
            }

            string schemeName = args[args.Length - 1];

            uint[] colorTable = null;
            ISchemeParser[] parsers = { new XmlSchemeParser(), new IniSchemeParser() };
            foreach (var parser in parsers)
            {
                uint[] table = parser.ParseScheme(schemeName);
                if (table != null)
                {
                    colorTable = table;
                    break;
                }
            }

            if (colorTable == null)
            {
                Console.WriteLine(string.Format(Resources.SchemeNotFound, schemeName));
                return;
            }

            if (setDefaults)
            {
                SetDefaults(colorTable);
            }
            if (setProperties)
            {
                SetProperties(colorTable);
            }
        }

    }
}
