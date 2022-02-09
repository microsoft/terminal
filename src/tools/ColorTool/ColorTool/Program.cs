//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using ColorTool.ConsoleTargets;
using static ColorTool.ConsoleAPI;
using ColorTool.SchemeWriters;
using System;
using System.Collections.Generic;
using System.Linq;

namespace ColorTool
{
    static class Program
    {
        private static bool quietMode = false;
        private static bool reportErrors = false;
        private static bool setDefaults = false;
        private static bool setProperties = true;
        private static bool setUnixStyle = false;
        private static bool setTerminalStyle = false;
        private static bool compactTableStyle = true;
        private static bool printCurrent = false;

        public static void Main(string[] args)
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
                    case "-?":
                    case "--help":
                        Usage();
                        return;
                    case "-c":
                    case "--current":
                        printCurrent = true;
                        break;
                    case "-l":
                    case "--location":
                        SchemeManager.PrintSchemesDirectory();
                        return;
                    case "-o":
                    case "--output":
                        if (i + 1 < args.Length)
                        {
                            new IniSchemeWriter().ExportCurrentAsIni(args[i + 1]);
                        }
                        else
                        {
                            OutputUsage();
                        }
                        return;
                    case "-s":
                    case "--schemes":
                        SchemeManager.PrintSchemes();
                        return;
                    case "-v":
                    case "--version":
                        Version();
                        return;
                    case "-e":
                    case "--errors":
                        reportErrors = true;
                        break;
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
                    case "-x":
                    case "--xterm":
                        setUnixStyle = true;
                        setProperties = true;
                        break;
                    case "-a":
                    case "--allcolors":
                        compactTableStyle = false;
                        break;
                    case "-t":
                    case "--terminal":
                        setTerminalStyle = true;
                        setProperties = true;
                        break;
                    default:
                        break;
                }
            }
            if (printCurrent)
            {
                if (setUnixStyle) DoInVTMode(() => ColorTable.PrintTableWithVt(compactTableStyle));
                else ColorTable.PrintTable(compactTableStyle);
                return;
            }

            string schemeName = args[args.Length - 1];

            ColorScheme colorScheme = SchemeManager.GetScheme(schemeName, reportErrors);

            if (colorScheme == null)
            {
                Console.WriteLine(string.Format(Resources.SchemeNotFound, schemeName));
                return;
            }

            foreach (var target in GetConsoleTargets())
            {
                target.ApplyColorScheme(colorScheme, quietMode, compactTableStyle);
            }
        }

        private static void Usage()
        {
            Console.WriteLine(Resources.Usage,
                string.Join($"{Environment.NewLine}  ", SchemeManager.GetParsers().Select(p => p.Name)));
        }

        private static void OutputUsage()
        {
            Console.WriteLine(Resources.OutputUsage);
        }

        public static bool DoInVTMode(Action VTAction)
        {
            IntPtr hOut = GetStdOutputHandle();
            uint requestedMode;
            uint originalConsoleMode;
            bool succeeded = GetConsoleMode(hOut, out originalConsoleMode);
            if (succeeded)
            {
                requestedMode = originalConsoleMode | (uint)ConsoleAPI.ConsoleOutputModes.ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, requestedMode);
            }

            VTAction();

            if (succeeded) SetConsoleMode(hOut, originalConsoleMode);
            return succeeded;
        }

        private static void Version()
        {
            var assembly = System.Reflection.Assembly.GetExecutingAssembly();
            var info = System.Diagnostics.FileVersionInfo.GetVersionInfo(assembly.Location);
            Console.WriteLine($"ColorTool v{info.FileVersion}");
        }

        /// <summary>
        /// Returns an enumerable of consoles that we want to apply the color scheme to.
        /// The contents of this enumerable depends on the user's provided command line flags.
        /// </summary>
        private static IEnumerable<IConsoleTarget> GetConsoleTargets()
        {
            if (setDefaults)
            {
                yield return new DefaultConsoleTarget();
            }
            if (setProperties)
            {
                if (setUnixStyle)
                {
                    yield return new VirtualTerminalConsoleTarget();
                }
                else if (setTerminalStyle)
                {
                    yield return new TerminalSchemeConsoleTarget();
                }
                else
                {
                    yield return new CurrentConsoleTarget();
                }
            }
        }
    }
}
