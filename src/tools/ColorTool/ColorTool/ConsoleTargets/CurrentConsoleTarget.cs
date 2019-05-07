//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using static ColorTool.ConsoleAPI;

namespace ColorTool.ConsoleTargets
{
    /// <summary>
    /// A console target that writes to the currently open console.
    /// </summary>
    class CurrentConsoleTarget : IConsoleTarget
    {
        public void ApplyColorScheme(ColorScheme colorScheme, bool quietMode)
        {
            CONSOLE_SCREEN_BUFFER_INFO_EX csbiex = CONSOLE_SCREEN_BUFFER_INFO_EX.Create();
            IntPtr hOut = GetStdOutputHandle();
            bool success = GetConsoleScreenBufferInfoEx(hOut, ref csbiex);
            if (!success)
            {
                throw new InvalidOperationException("Could not obtain console screen buffer");
            }

            csbiex.srWindow.Bottom++;
            for (int i = 0; i < 16; i++)
            {
                csbiex.ColorTable[i] = colorScheme.ColorTable[i];
            }
            if(colorScheme.ScreenColorAttributes is ushort wAttrs)
            {
                csbiex.wAttributes = wAttrs;
            }
            if(colorScheme.PopupColorAttributes is ushort wPopupAttrs)
            {
                csbiex.wPopupAttributes = wPopupAttrs;
            }
            SetConsoleScreenBufferInfoEx(hOut, ref csbiex);

            if (!quietMode)
            {
                ColorTable.PrintTable();
            }
        }
    }
}
