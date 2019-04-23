//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using ColorTool.ConsoleTargets;
using ColorTool.SchemeWriters;
using System;
using System.Collections.Generic;
using System.Linq;

namespace ColorTool
{
    class Program
    {
        static bool quietMode = false;
        static bool reportErrors = false;
        static bool setDefaults = false;
        static bool setProperties = true;
        static bool setUnixStyle = false;

        static void Usage()
        {
            Console.WriteLine(Resources.Usage,
                string.Join($"{Environment.NewLine}  ", SchemeManager.GetParsers().Select(p => p.Name)));
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
                        ColorTable.PrintTable();
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
                    case "-?":
                    case "--help":
                        Usage();
                        return;
                    case "-v":
                    case "--version":
                        Version();
                        return;
                    case "-l":
                    case "--location":
                        SchemeManager.PrintSchemesDirectory();
                        return;
                    case "-x":
                    case "--xterm":
                        setUnixStyle = true;
                        setProperties = true;
                        break;
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
                    default:
                        break;
                }
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
                target.ApplyColorScheme(colorScheme, quietMode);
            }
        }

        /// <summary>
        /// Returns an enumerable of consoles that we want to apply the colorscheme to.
        /// The contents of this enumerable depends on the user's provided command line flags.
        /// </summary>
        public static IEnumerable<IConsoleTarget> GetConsoleTargets()
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
                else
                {
                    yield return new CurrentConsoleTarget();
                }
            }
        }
    }
}
