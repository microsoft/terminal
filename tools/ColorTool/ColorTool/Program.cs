//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using static ColorTool.ConsoleAPI;

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

        // Use a Console index in to get a VT index out.
        static int[] VT_INDICIES = {
            0, // DARK_BLACK
            4, // DARK_BLUE
            2, // DARK_GREEN
            6, // DARK_CYAN
            1, // DARK_RED
            5, // DARK_MAGENTA
            3, // DARK_YELLOW
            7, // DARK_WHITE
            8+0, // BRIGHT_BLACK
            8+4, // BRIGHT_BLUE
            8+2, // BRIGHT_GREEN
            8+6, // BRIGHT_CYAN
            8+1, // BRIGHT_RED
            8+5, // BRIGHT_MAGENTA
            8+3, // BRIGHT_YELLOW
            8+7,// BRIGHT_WHITE
        };

        static bool quietMode = false;
        static bool setDefaults = false;
        static bool setProperties = true;
        static bool setUnixStyle = false;

        static void Usage()
        {
            Console.WriteLine(Resources.Usage,
                string.Join($"{Environment.NewLine}  ", GetParsers().Select(p => p.Name)));
        }

        static void OutputUsage()
        {
            Console.WriteLine(Resources.OutputUsage);
        }

        static void Version()
        {
            var assembly = System.Reflection.Assembly.GetExecutingAssembly();
            var info = System.Diagnostics.FileVersionInfo.GetVersionInfo(assembly.Location);
            Console.WriteLine($"colortool v{info.FileVersion}");
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

            // Reset foreground and background colors
            Console.ForegroundColor = currentForeground;
            Console.BackgroundColor = currentBackground;
        }
        
        static void PrintTableWithVt()
        {
            // Save the current background and foreground colors.
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
                Console.Write("\x1b[m");

                if (fg >= 0)
                {
                    Console.Write(FGs[fg] + "\t");
                }

                if (fg == 0)
                {
                    Console.Write("\x1b[39m");
                }
                else
                {
                    Console.Write("\x1b[" + FGs[fg]);
                }

                for (int bg = 0; bg < BGs.Length; bg++)
                {
                    if (bg > 0)
                    {
                        Console.Write(" ");
                    }
                    if (bg == 0)
                    {
                        Console.Write("\x1b[49m");
                    }
                    else
                    {
                        Console.Write("\x1b[" +BGs[bg]);
                    }

                    Console.Write(test);
                    Console.Write("\x1b[49m");
                }
                Console.Write("\n");

            }
            Console.Write("\n");

            // Reset foreground and background colors
            Console.Write("\x1b[m");
        }

        private static IntPtr GetStdOutputHandle()
        {
            return GetStdHandle(STD_OUTPUT_HANDLE);
        }

        static void PrintSchemes()
        {
            var schemeDirectory = new FileInfo(new Uri(Assembly.GetEntryAssembly().GetName().CodeBase).AbsolutePath).Directory.FullName + "/schemes"; 

            if (Directory.Exists(schemeDirectory))
            {
                IntPtr handle = GetStdOutputHandle();
                GetConsoleMode(handle, out var mode);
                SetConsoleMode(handle, mode | 0x4);

                int consoleWidth = Console.WindowWidth;
                string fgText = " gYw ";
                foreach (string schemeName in Directory.GetFiles(schemeDirectory).Select(Path.GetFileName))
                {
                    ColorScheme colorScheme = GetScheme(schemeName, false);
                    if (colorScheme != null)
                    {
                        string colors = string.Empty;
                        for (var index = 0; index < 8; index++)
                        {
                            uint t = colorScheme.colorTable[index];
                            var color = UIntToColor(t);
                            // Set the background color to the color in the scheme, plus some text to show how it looks
                            colors += $"\x1b[48;2;{color.R};{color.G};{color.B}m{fgText}";
                        }
                        // Align scheme colors right, or on newline if it doesn't fit
                        int schemeTextLength = fgText.Length * 8;
                        int bufferLength = consoleWidth - (schemeName.Length + schemeTextLength);

                        string bufferString = bufferLength >= 0
                            ? new string(' ', bufferLength)
                            : "\n" + new string(' ', consoleWidth - schemeTextLength);

                        string outputString = schemeName + bufferString + colors;
                        Console.WriteLine(outputString);
                        Console.ResetColor();
                    }
                }
            }
        }

        private static Color UIntToColor(uint color)
        {
            byte r = (byte)(color >> 0);
            byte g = (byte)(color >> 8);
            byte b = (byte)(color >> 16);
            return Color.FromArgb(r, g, b);
        }

        static bool SetProperties(ColorScheme colorScheme)
        {
            CONSOLE_SCREEN_BUFFER_INFO_EX csbiex = CONSOLE_SCREEN_BUFFER_INFO_EX.Create();
            IntPtr hOut = GetStdOutputHandle();
            bool success = GetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            if (success)
            {
                csbiex.srWindow.Bottom++;
                for (int i = 0; i < 16; i++)
                {
                    csbiex.ColorTable[i] = colorScheme.colorTable[i];
                }
                if(colorScheme.background != null && colorScheme.foreground != null)
                {
                    int fgidx = colorScheme.CalculateIndex(colorScheme.foreground.Value);
                    int bgidx = colorScheme.CalculateIndex(colorScheme.background.Value);
                    csbiex.wAttributes = (ushort)(fgidx | (bgidx << 4));
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

        static bool SetPropertiesWithVt(ColorScheme colorScheme)
        {
            IntPtr hOut = GetStdOutputHandle();
            uint originalMode;
            uint requestedMode;
            bool succeeded = GetConsoleMode(hOut, out originalMode);
            if (succeeded)
            {
                requestedMode = originalMode | (uint)ConsoleAPI.ConsoleOutputModes.ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, requestedMode);

            }

            for (int i = 0; i < colorScheme.colorTable.Length; i++)
            {
                int vtIndex = VT_INDICIES[i];
                Color color = UIntToColor(colorScheme.colorTable[i]);
                string s = $"\x1b]4;{vtIndex};rgb:{color.R:X}/{color.G:X}/{color.B:X}\x7";
                Console.Write(s);
            }
            if (!quietMode)
            {
                PrintTableWithVt();
            }

            if (succeeded)
            {
                SetConsoleMode(hOut, originalMode);
            }

            return true;
        }
        static bool SetDefaults(ColorScheme colorScheme)
        {
            //TODO
            RegistryKey consoleKey = Registry.CurrentUser.OpenSubKey("Console", true);
            for (int i = 0; i < colorScheme.colorTable.Length; i++)
            {
                string valueName = "ColorTable" + (i < 10 ? "0" : "") + i;
                consoleKey.SetValue(valueName, colorScheme.colorTable[i], RegistryValueKind.DWord);
            }
            Console.WriteLine(Resources.WroteToDefaults);
            return true;
        }

        static bool ExportCurrentAsIni(string outputPath)
        {
            CONSOLE_SCREEN_BUFFER_INFO_EX csbiex = CONSOLE_SCREEN_BUFFER_INFO_EX.Create();
            IntPtr hOut = GetStdOutputHandle();
            bool success = GetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            if (success)
            {
                try
                {
                    // StreamWriter can fail for a variety of file system reasons so catch exceptions and print message.
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
                catch (Exception ex)
                {
                    Console.WriteLine(ex.Message);
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
                    case "-x":
                    case "--xterm":
                        setUnixStyle = true;
                        setProperties = true;
                        break;
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
                    case "-s":
                    case "--schemes":
                        PrintSchemes();
                        return;
                    default:
                        break;
                }
            }

            string schemeName = args[args.Length - 1];

            ColorScheme colorScheme = GetScheme(schemeName);

            if (colorScheme == null)
            {
                Console.WriteLine(string.Format(Resources.SchemeNotFound, schemeName));
                return;
            }

            if (setDefaults)
            {
                SetDefaults(colorScheme);
            }
            if (setProperties)
            {
                if (setUnixStyle)
                {
                    SetPropertiesWithVt(colorScheme);
                }
                else
                {
                    SetProperties(colorScheme);
                }
            }
        }

        private static IEnumerable<ISchemeParser> GetParsers()
        {
            return typeof(Program).Assembly.GetTypes()
                .Where(t => !t.IsAbstract && typeof(ISchemeParser).IsAssignableFrom(t))
                .Select(t => (ISchemeParser)Activator.CreateInstance(t));
        }
                
        private static ColorScheme GetScheme(string schemeName, bool reportErrors = true)
        {
            foreach (var parser in GetParsers())
            {
                ColorScheme scheme = parser.ParseScheme(schemeName, reportErrors);
                if (scheme != null)
                {
                    return scheme;
                }
            }
            return null;
       }
    }
}
