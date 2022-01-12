//
// Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

namespace ColorTool
{
    /// <summary>
    /// Keeps track of the color table indices for the background/foreground in a color scheme.
    /// </summary>
    public readonly struct ConsoleAttributes
    {
        public ConsoleAttributes(uint? background, uint? foreground, uint? popupBackground, uint? popupForeground, uint? cursor)
        {
            Background = background;
            Foreground = foreground;
            PopupBackground = popupBackground;
            PopupForeground = popupForeground;
            Cursor = cursor;
        }

        public uint? Foreground { get; }
        public uint? Background { get; }
        public uint? PopupForeground { get; }
        public uint? PopupBackground { get; }
        public uint? Cursor { get; }
    }
}
