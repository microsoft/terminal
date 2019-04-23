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
            //TODO
            RegistryKey consoleKey = Registry.CurrentUser.OpenSubKey("Console", true);
            for (int i = 0; i < colorScheme.colorTable.Length; i++)
            {
                string valueName = "ColorTable" + (i < 10 ? "0" : "") + i;
                consoleKey.SetValue(valueName, colorScheme.colorTable[i], RegistryValueKind.DWord);
            }
            Console.WriteLine(Resources.WroteToDefaults);
        }
    }
}
