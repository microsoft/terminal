// <copyright file="TerminalTheme.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System;
    using System.Runtime.InteropServices;

    /// <summary>
    /// Enum for the style of cursor to display in the terminal.
    /// </summary>
    public enum CursorStyle : uint
    {
        /// <summary>
        /// Cursor will be rendered as a blinking block.
        /// </summary>
        BlinkingBlock = 0,

        /// <summary>
        /// Currently identical to <see cref="CursorStyle.BlinkingBlock"/>
        /// </summary>
        BlinkingBlockDefault = 1,

        /// <summary>
        /// Cursor will be rendered as a block that does not blink.
        /// </summary>
        SteadyBlock = 2,

        /// <summary>
        /// Cursor will be rendered as a blinking underline.
        /// </summary>
        BlinkingUnderline = 3,

        /// <summary>
        /// Cursor will be rendered as an underline that does not blink.
        /// </summary>
        SteadyUnderline = 4,

        /// <summary>
        /// Cursor will be rendered as a vertical blinking bar.
        /// </summary>
        BlinkingBar = 5,

        /// <summary>
        /// Cursor will be rendered as a vertical bar that does not blink.
        /// </summary>
        SteadyBar = 6,
    }

    /// <summary>
    /// Structure for color handling in the terminal.
    /// </summary>
    /// <remarks>Pack = 1 removes the padding added by some compilers/processors for optimization purposes.</remarks>
    [StructLayout(LayoutKind.Sequential)]
    public struct TerminalTheme
    {
        /// <summary>
        /// The default background color of the terminal, represented in Win32 COLORREF format.
        /// </summary>
        public uint DefaultBackground;

        /// <summary>
        /// The default foreground color of the terminal, represented in Win32 COLORREF format.
        /// </summary>
        public uint DefaultForeground;

        /// <summary>
        /// The style of cursor to use in the terminal.
        /// </summary>
        public CursorStyle CursorStyle;

        /// <summary>
        /// The color array to use for the terminal, filling the standard vt100 16 color table, represented in Win32 COLORREF format.
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U4, SizeConst = 16)]
        public uint[] ColorTable;
    }
}
