// <copyright file="TerminalTheme.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System.Runtime.InteropServices;

    /// <summary>
    /// Enum for the style of cursor to display in the terminal.
    /// </summary>
    public enum CursorStyle : uint
    {
        /// <summary>
        /// Cursor will be rendered as a quarter-block, like the original Windows Console.
        /// </summary>
        Vintage = 0,

        /// <summary>
        /// Cursor will be rendered as a vertical bar.
        /// </summary>
        Bar = 1,

        /// <summary>
        /// Cursor will be rendered as a one-pixel underline.
        /// </summary>
        Underscore = 2,

        /// <summary>
        /// Cursor will be rendered as a double underline.
        /// </summary>
        DoubleUnderscore = 3,

        /// <summary>
        /// Cursor will be rendered as a filled rectangle covering a full cell.
        /// </summary>
        FilledBox = 4,

        /// <summary>
        /// Cursor will be rendered as an empty rectangle outlining a full cell.
        /// </summary>
        EmptyBox = 5,

        /// <summary>
        /// This definition is included for backwards compatibility.
        /// </summary>
        BlinkingBar = Bar,

        /// <summary>
        /// This definition is included for backwards compatibility.
        /// </summary>
        BlinkingUnderline = Underscore,

        /// <summary>
        /// This definition is included for backwards compatibility.
        /// </summary>
        BlinkingDoubleUnderline = DoubleUnderscore,

        /// <summary>
        /// This definition is included for backwards compatibility.
        /// </summary>
        BlinkingBlock = FilledBox,

        /// <summary>
        /// This definition is included for backwards compatibility.
        /// </summary>
        BlinkingBox = EmptyBox,

        /// <summary>
        /// Currently identical to <see cref="CursorStyle.BlinkingBlock"/>
        /// </summary>
        BlinkingBlockDefault = BlinkingBlock,

        /// <summary>
        /// Cursor will be rendered as a vertical bar that does not blink.
        /// Note: Steady cursors are not currently supported.
        /// </summary>
        SteadyBar = BlinkingBar,

        /// <summary>
        /// Cursor will be rendered as an underline that does not blink.
        /// Note: Steady cursors are not currently supported.
        /// </summary>
        SteadyUnderline = BlinkingUnderline,

        /// <summary>
        /// Cursor will be rendered as a block that does not blink.
        /// Note: Steady cursors are not currently supported.
        /// </summary>
        SteadyBlock = BlinkingBlock,
    }

    /// <summary>
    /// Structure for color handling in the terminal.
    /// </summary>
    /// <remarks>Keep in sync with HwndTerminal.hpp.</remarks>
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
        /// The default selection background color of the terminal, represented in Win32 COLORREF format.
        /// </summary>
        public uint DefaultSelectionBackground;

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
