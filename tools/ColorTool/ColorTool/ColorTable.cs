//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Collections.Generic;

namespace ColorTool
{
    /// <summary>
    /// Displays the color table that demonstrates the current colorscheme.
    /// </summary>
    class ColorTable
    {
        const int DARK_BLACK = 0;
        const int DARK_BLUE = 1;
        const int DARK_GREEN = 2;
        const int DARK_CYAN = 3;
        const int DARK_RED = 4;
        const int DARK_MAGENTA = 5;
        const int DARK_YELLOW = 6;
        const int DARK_WHITE = 7;
        const int BRIGHT_BLACK = 8;
        const int BRIGHT_BLUE = 9;
        const int BRIGHT_GREEN = 10;
        const int BRIGHT_CYAN = 11;
        const int BRIGHT_RED = 12;
        const int BRIGHT_MAGENTA = 13;
        const int BRIGHT_YELLOW = 14;
        const int BRIGHT_WHITE = 15;

        // This is the order of colors when output by the table.
        private static readonly IReadOnlyList<int> outputFgs = new[]
        {
            BRIGHT_WHITE,
            DARK_BLACK,
            BRIGHT_BLACK,
            DARK_RED,
            BRIGHT_RED,
            DARK_GREEN,
            BRIGHT_GREEN,
            DARK_YELLOW,
            BRIGHT_YELLOW,
            DARK_BLUE,
            BRIGHT_BLUE,
            DARK_MAGENTA,
            BRIGHT_MAGENTA,
            DARK_CYAN,
            BRIGHT_CYAN,
            DARK_WHITE,
            BRIGHT_WHITE
        };

        private const string TestText = "  gYw  ";

        private static readonly IReadOnlyList<string> FGs = new[]
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

        private static readonly IReadOnlyList<string> BGs = new[]
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

        private static readonly IReadOnlyList<int> saneBgs = new[]
        {
            DARK_BLACK,
            DARK_RED,
            DARK_GREEN,
            DARK_YELLOW,
            DARK_BLUE,
            DARK_MAGENTA,
            DARK_CYAN,
            DARK_WHITE
        };

        public static void PrintTable()
        {
            ConsoleColor[] colors = (ConsoleColor[])ConsoleColor.GetValues(typeof(ConsoleColor));
            // Save the current background and foreground colors.
            ConsoleColor currentBackground = Console.BackgroundColor;
            ConsoleColor currentForeground = Console.ForegroundColor;

            Console.Write("\t");
            for (int bg = 0; bg < BGs.Count; bg++)
            {
                if (bg > 0) Console.Write(" ");
                Console.Write("  ");
                Console.Write(bg == 0 ? "   " : BGs[bg]);
                Console.Write("  ");
            }
            Console.WriteLine();

            for (int fg = 0; fg < FGs.Count; fg++)
            {
                Console.ForegroundColor = currentForeground;
                Console.BackgroundColor = currentBackground;

                if (fg >= 0) Console.Write(FGs[fg] + "\t");

                if (fg == 0) Console.ForegroundColor = currentForeground;
                else Console.ForegroundColor = colors[outputFgs[fg - 1]];

                for (int bg = 0; bg < BGs.Count; bg++)
                {
                    if (bg > 0) Console.Write(" ");
                    if (bg == 0)
                        Console.BackgroundColor = currentBackground;
                    else Console.BackgroundColor = colors[saneBgs[bg - 1]];
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
            for (int bg = 0; bg < BGs.Count; bg++)
            {
                if (bg > 0) Console.Write(" ");
                Console.Write("  ");
                Console.Write(bg == 0 ? "   " : BGs[bg]);
                Console.Write("  ");
            }
            Console.WriteLine();

            for (int fg = 0; fg < FGs.Count; fg++)
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

                for (int bg = 0; bg < BGs.Count; bg++)
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
                        Console.Write("\x1b[" + BGs[bg]);
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
