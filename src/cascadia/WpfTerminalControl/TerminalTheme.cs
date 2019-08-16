using System;
using System.Runtime.InteropServices;

namespace Microsoft.Terminal.Wpf
{
    /// <summary>
    /// Structure for color handling in the terminal.
    /// </summary>
    /// <remarks>Pack = 1 removes the padding added by some compilers/processors for optimization purposes.</remarks>
    [StructLayout(LayoutKind.Sequential)]
    public struct TerminalTheme
    {
        public uint DefaultBackground;
        public uint DefaultForeground;
        public CursorStyle CursorStyle;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U4, SizeConst = 16)]
        public uint[] ColorTable;
    }


    public enum CursorStyle : UInt32
    {
        BlinkingBlock = 0,
        BlinkingBlockDefault = 1,
        SteadyBlock = 2,
        BlinkingUnderline = 3,
        SteadyUnderline = 4,
        BlinkingBar = 5,
        SteadyBar = 6
    };
}
