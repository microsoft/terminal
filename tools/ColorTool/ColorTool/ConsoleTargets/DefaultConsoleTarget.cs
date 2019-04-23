//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using Microsoft.Win32;
using System;

namespace ColorTool.ConsoleTargets
{
    /// <summary>
    /// A console target that writes to the Windows registry to modify system defaults
    /// </summary>
    class DefaultConsoleTarget : IConsoleTarget
    {
        public void ApplyColorScheme(ColorScheme colorScheme, bool quietMode)
        {
            RegistryKey consoleKey = Registry.CurrentUser.OpenSubKey("Console", true);
            for (int i = 0; i < colorScheme.ColorTable.Length; i++)
            {
                string valueName = "ColorTable" + (i < 10 ? "0" : "") + i;
                consoleKey.SetValue(valueName, colorScheme.ColorTable[i], RegistryValueKind.DWord);
            }
            if(colorScheme.ScreenColorAttributes is ushort screenColors)
            {
                consoleKey.SetValue("ScreenColors", screenColors, RegistryValueKind.DWord);
            }
            if(colorScheme.PopupColorAttributes is ushort popupColors)
            {
                consoleKey.SetValue("PopupColors", popupColors, RegistryValueKind.DWord);
            }

            Console.WriteLine(Resources.WroteToDefaults);
        }
    }
}
