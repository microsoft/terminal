//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

namespace ColorTool.ConsoleTargets
{
    /// <summary>
    /// A console that can have a color scheme applied to it.
    /// </summary>
    interface IConsoleTarget
    {
        void ApplyColorScheme(ColorScheme colorScheme, bool quietMode);
    }
}
