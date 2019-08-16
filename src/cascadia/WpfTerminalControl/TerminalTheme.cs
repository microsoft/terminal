using System;
using System.Runtime.InteropServices;

namespace Microsoft.Terminal.Wpf
{
    /// <summary>
    /// Structure for color handling in the terminal.
    /// </summary>
    /// <remarks>Pack = 1 removes the padding added by some compilers/processors for optimization purposes.</remarks>
    [StructLayout(LayoutKind.Sequential)]
    public class TerminalTheme
    {
        public uint DefaultBackground;
        public uint DefaultForeground;
        public CursorStyle CursorStyle;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U8, SizeConst = 16)]
        public uint[] ColorTable;
    }

    public enum CursorStyle
    {
        Vintage = 0,
        Bar = 1,
        Underscore = 2,
        FilledBox = 3,
        EmptyBox = 4,
    }
}
