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
        private static readonly IReadOnlyList<int> TableColors = new[]
        {
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

        private static readonly IReadOnlyList<string> AnsiForegroundSequences = new[]
        {
            "30m",
            "90m",
            "31m",
            "91m",
            "32m",
            "92m",
            "33m",
            "93m",
            "34m",
            "94m",
            "35m",
            "95m",
            "36m",
            "96m",
            "37m",
            "97m"
        };

        private static readonly IReadOnlyList<string> AnsiBackgroundSequences = new[]
        {
            "40m",
            "100m",
            "41m",
            "101m",
            "42m",
            "102m",
            "43m",
            "103m",
            "44m",
            "104m",
            "45m",
            "105m",
            "46m",
            "106m",
            "47m",
            "107m"
        };

        private const string AnsiDefaultFg = "39m";
        private const string AnsiDefaultBg = "49m";

        public static void PrintTable(bool compact)
        {
            string TestText = compact ? "  gYw  " : " gYw ";
            ConsoleColor[] colors = (ConsoleColor[])ConsoleColor.GetValues(typeof(ConsoleColor));
            // Save the current background and foreground colors.
            ConsoleColor currentBackground = Console.BackgroundColor;
            ConsoleColor currentForeground = Console.ForegroundColor;

            // first column
            Console.Write("\t ");
            if (compact) Console.Write(" ");
            Console.Write(AnsiDefaultBg);
            if (compact) Console.Write(" ");
            Console.Write(" ");

            for (int bg = 0; bg < AnsiBackgroundSequences.Count; bg += 1 + Convert.ToUInt16(compact))
            {
                Console.Write("  ");
                if (compact) Console.Write(" ");
                Console.Write(AnsiBackgroundSequences[bg]);
                if (compact) Console.Write(" ");
                if (AnsiBackgroundSequences[bg].Length == 3) Console.Write(" ");
            }
            Console.WriteLine();

            for (int fg = 0; fg <= TableColors.Count && fg <= AnsiForegroundSequences.Count; fg++)
            {
                Console.ForegroundColor = currentForeground;
                Console.BackgroundColor = currentBackground;

                Console.Write(fg == 0 ? AnsiDefaultFg : AnsiForegroundSequences[fg - 1]);
                Console.Write("\t");

                if (fg == 0) Console.ForegroundColor = currentForeground;
                else Console.ForegroundColor = colors[TableColors[fg - 1]];

                for (int bg = 0; bg <= TableColors.Count; bg += 1 + Convert.ToUInt16(compact))
                {
                    if (bg > 0) Console.Write(" ");
                    if (bg == 0)
                        Console.BackgroundColor = currentBackground;
                    else Console.BackgroundColor = colors[TableColors[bg - (1 + Convert.ToUInt16(compact))]];
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

        public static void PrintTableWithVt(bool compact)
        {
            string TestText = compact ? "  gYw  " : " gYw ";
            // first column
            Console.Write("\t ");
            if (compact) Console.Write(" ");
            Console.Write(AnsiDefaultBg);
            if (compact) Console.Write(" ");
            Console.Write(" ");

            for (int bg = 0; bg < AnsiBackgroundSequences.Count; bg += 1 + Convert.ToUInt16(compact))
            {
                Console.Write("  ");
                if (compact) Console.Write(" ");
                Console.Write(AnsiBackgroundSequences[bg]);
                if (compact) Console.Write(" ");
                if (AnsiBackgroundSequences[bg].Length == 3) Console.Write(" ");
            }
            Console.WriteLine();

            for (int fg = 0; fg <= AnsiForegroundSequences.Count; fg++)
            {
                Console.Write("\x1b[m");

                Console.Write(fg == 0 ? AnsiDefaultFg : AnsiForegroundSequences[fg - 1]);
                Console.Write("\t");

                if (fg == 0) Console.Write("\x1b[" + AnsiDefaultFg);
                else Console.Write("\x1b[" + AnsiForegroundSequences[fg - 1]);

                for (int bg = 0; bg <= AnsiBackgroundSequences.Count; bg += 1 + Convert.ToUInt16(compact))
                {
                    if (bg != 0) Console.Write(" ");
                    if (bg == 0) Console.Write("\x1b[" + AnsiDefaultBg);
                    else Console.Write("\x1b[" + AnsiBackgroundSequences[bg - (1 + Convert.ToUInt16(compact))]);

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
