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

            for (int i = 0; i < colorScheme.ColorTable.Length; i++)
            {
                int vtIndex = VirtualTerminalIndices[i];
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
