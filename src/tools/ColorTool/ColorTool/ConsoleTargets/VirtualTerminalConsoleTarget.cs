//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Collections.Generic;
using System.Drawing;
using static ColorTool.ConsoleAPI;

namespace ColorTool.ConsoleTargets
{
    /// <summary>
    /// A console target that writes to the currently open console, using VT sequences.
    /// </summary>
    class VirtualTerminalConsoleTarget : IConsoleTarget
    {
        // Use a Console index in to get a VT index out.
        public static readonly IReadOnlyList<int> VirtualTerminalIndices = new[]
        {
            0, // Dark Black
            4, // Dark Blue
            2, // Dark Green
            6, // Dark Cyan
            1, // Dark Red
            5, // Dark Magenta
            3, // Dark Yellow
            7, // Dark White
            8+0, // Bright Black
            8+4, // Bright Blue
            8+2, // Bright Green
            8+6, // Bright Cyan
            8+1, // Bright Red
            8+5, // Bright Magenta
            8+3, // Bright Yellow
            8+7, // Bright White
        };

        public void ApplyColorScheme(ColorScheme colorScheme, bool quietMode, bool compactColortable)
        {
            Program.DoInVTMode(() =>
            {
                if (colorScheme.ConsoleAttributes.Foreground.HasValue)
                {
                    Color color = colorScheme.Foreground;
                    string s = $"\x1b]10;rgb:{color.R:X2}/{color.G:X2}/{color.B:X2}\x1b\\";
                    Console.Write(s);
                }
                if (colorScheme.ConsoleAttributes.Background.HasValue)
                {
                    Color color = colorScheme.Background;
                    string s = $"\x1b]11;rgb:{color.R:X2}/{color.G:X2}/{color.B:X2}\x1b\\";
                    Console.Write(s);
                }
                if (colorScheme.ConsoleAttributes.Cursor.HasValue)
                {
                    Color color = colorScheme.Cursor;
                    string s = $"\x1b]12;rgb:{color.R:X2}/{color.G:X2}/{color.B:X2}\x1b\\";
                    Console.Write(s);
                }
                for (int i = 0; i < colorScheme.ColorTable.Length; i++)
                {
                    int vtIndex = VirtualTerminalIndices[i];
                    Color color = colorScheme[i];
                    string s = $"\x1b]4;{vtIndex};rgb:{color.R:X2}/{color.G:X2}/{color.B:X2}\x7";
                    Console.Write(s);
                }

                if (!quietMode)
                {
                    ColorTable.PrintTableWithVt(compactColortable);
                }
            });
        }
    }
}
