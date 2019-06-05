//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Collections.Generic;

namespace ColorTool
{
    /// <summary>
    /// Displays the color table that demonstrates the current color scheme.
    /// </summary>
    static class ColorTable
    {
        private const int DarkBlack = 0;
        private const int DarkBlue = 1;
        private const int DarkGreen = 2;
        private const int DarkCyan = 3;
        private const int DarkRed = 4;
        private const int DarkMagenta = 5;
        private const int DarkYellow = 6;
        private const int DarkWhite = 7;
        private const int BrightBlack = 8;
        private const int BrightBlue = 9;
        private const int BrightGreen = 10;
        private const int BrightCyan = 11;
        private const int BrightRed = 12;
        private const int BrightMagenta = 13;
        private const int BrightYellow = 14;
        private const int BrightWhite = 15;

        // This is the order of colors when output by the table.
        private static readonly IReadOnlyList<int> Foregrounds = new[]
        {
            BrightWhite,
            DarkBlack,
            BrightBlack,
            DarkRed,
            BrightRed,
            DarkGreen,
            BrightGreen,
            DarkYellow,
            BrightYellow,
            DarkBlue,
            BrightBlue,
            DarkMagenta,
            BrightMagenta,
            DarkCyan,
            BrightCyan,
            DarkWhite,
            BrightWhite
        };

        private static readonly IReadOnlyList<int> Backgrounds = new[]
        {
            DarkBlack,
            DarkRed,
            DarkGreen,
            DarkYellow,
            DarkBlue,
            DarkMagenta,
            DarkCyan,
            DarkWhite
        };

        private const string TestText = "  gYw  ";

        private static readonly IReadOnlyList<string> AnsiForegroundSequences = new[]
        {
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

        private static readonly IReadOnlyList<string> AnsiBackgroundSequences = new[]
        {
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

        public static void PrintTable()
        {
            ConsoleColor[] colors = (ConsoleColor[])ConsoleColor.GetValues(typeof(ConsoleColor));
            // Save the current background and foreground colors.
            ConsoleColor currentBackground = Console.BackgroundColor;
            ConsoleColor currentForeground = Console.ForegroundColor;

            Console.Write("\t");
            for (int bg = 0; bg < AnsiBackgroundSequences.Count; bg++)
            {
                if (bg > 0) Console.Write(" ");
                Console.Write("  ");
                Console.Write(bg == 0 ? "   " : AnsiBackgroundSequences[bg]);
                Console.Write("  ");
            }
            Console.WriteLine();

            for (int fg = 0; fg < AnsiForegroundSequences.Count; fg++)
            {
                Console.ForegroundColor = currentForeground;
                Console.BackgroundColor = currentBackground;

                if (fg >= 0) Console.Write(AnsiForegroundSequences[fg] + "\t");

                if (fg == 0) Console.ForegroundColor = currentForeground;
                else Console.ForegroundColor = colors[Foregrounds[fg - 1]];

                for (int bg = 0; bg < AnsiBackgroundSequences.Count; bg++)
                {
                    if (bg > 0) Console.Write(" ");
                    if (bg == 0)
                        Console.BackgroundColor = currentBackground;
                    else Console.BackgroundColor = colors[Backgrounds[bg - 1]];
                    Console.Write(TestText);
                    Console.BackgroundColor = currentBackground;
                }
                Console.Write("\n");
            }
            Console.Write("\n");

            // Reset foreground and background colors
            Console.ForegroundColor = currentForeground;
            Console.BackgroundColor = currentBackground;
        }

        public static void PrintTableWithVt()
        {
            Console.Write("\t");
            for (int bg = 0; bg < AnsiBackgroundSequences.Count; bg++)
            {
                if (bg > 0) Console.Write(" ");
                Console.Write("  ");
                Console.Write(bg == 0 ? "   " : AnsiBackgroundSequences[bg]);
                Console.Write("  ");
            }
            Console.WriteLine();

            for (int fg = 0; fg < AnsiForegroundSequences.Count; fg++)
            {
                Console.Write("\x1b[m");

                if (fg >= 0)
                {
                    Console.Write(AnsiForegroundSequences[fg] + "\t");
                }

                if (fg == 0)
                {
                    Console.Write("\x1b[39m");
                }
                else
                {
                    Console.Write("\x1b[" + AnsiForegroundSequences[fg]);
                }

                for (int bg = 0; bg < AnsiBackgroundSequences.Count; bg++)
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
                        Console.Write("\x1b[" + AnsiBackgroundSequences[bg]);
                    }

                    Console.Write(TestText);
                    Console.Write("\x1b[49m");
                }
                Console.Write("\n");
            }
            Console.Write("\n");

            // Reset foreground and background colors
            Console.Write("\x1b[m");
        }
    }
}
