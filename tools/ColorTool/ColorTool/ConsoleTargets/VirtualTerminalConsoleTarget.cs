//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
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
        static readonly int[] VT_INDICIES = {
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

        public void ApplyColorScheme(ColorScheme colorScheme, bool quietMode)
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
                Color color = colorScheme[i];
                string s = $"\x1b]4;{vtIndex};rgb:{color.R:X}/{color.G:X}/{color.B:X}\x7";
                Console.Write(s);
            }
            if (!quietMode)
            {
                ColorTable.PrintTableWithVt();
            }

            if (succeeded)
            {
                SetConsoleMode(hOut, originalMode);
            }
        }
    }
}
