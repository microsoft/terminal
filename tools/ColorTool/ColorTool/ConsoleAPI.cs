//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

using System;
using System.Runtime.InteropServices;

namespace ColorTool
{
    class ConsoleAPI
    {
        ////////////////////////////////////////////////////////////////////////
        [StructLayout(LayoutKind.Sequential)]
        public struct COORD
        {
            public short X;
            public short Y;
        }

        public struct SMALL_RECT
        {
            public short Left;
            public short Top;
            public short Right;
            public short Bottom;
        }

        [Flags]
        public enum ConsoleOutputModes : uint
        {
            ENABLE_PROCESSED_OUTPUT = 0x0001,
            ENABLE_WRAP_AT_EOL_OUTPUT = 0x0002,
            ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004,
            DISABLE_NEWLINE_AUTO_RETURN = 0x0008,
            ENABLE_LVB_GRID_WORLDWIDE = 0x0010,
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CONSOLE_SCREEN_BUFFER_INFO_EX
        {
            public uint cbSize;
            public COORD dwSize;
            public COORD dwCursorPosition;
            public ushort wAttributes;
            public SMALL_RECT srWindow;
            public COORD dwMaximumWindowSize;

            public ushort wPopupAttributes;
            public bool bFullscreenSupported;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
            public uint[] ColorTable;

            public static CONSOLE_SCREEN_BUFFER_INFO_EX Create()
            {
                return new CONSOLE_SCREEN_BUFFER_INFO_EX { cbSize = 96 };
            }
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr GetStdHandle(int nStdHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetConsoleScreenBufferInfoEx(IntPtr hConsoleOutput, ref CONSOLE_SCREEN_BUFFER_INFO_EX ConsoleScreenBufferInfo);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool SetConsoleScreenBufferInfoEx(IntPtr ConsoleOutput, ref CONSOLE_SCREEN_BUFFER_INFO_EX ConsoleScreenBufferInfoEx);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool WriteConsole(
            IntPtr hConsoleOutput,
            string lpBuffer,
            uint nNumberOfCharsToWrite,
            out uint lpNumberOfCharsWritten,
            IntPtr lpReserved
            );

        [DllImport("kernel32.dll")]
        public static extern bool GetConsoleMode(IntPtr hConsoleHandle, out uint lpMode);

        [DllImport("kernel32.dll")]
        public static extern bool SetConsoleMode(IntPtr hConsoleHandle, uint dwMode);
        ////////////////////////////////////////////////////////////////////////

        public static uint RGB(int r, int g, int b)
        {
            return (uint)r + (((uint)g) << 8) + (((uint)b) << 16);
        }
        public static uint Rvalue(uint rgb)
        {
            return rgb & 0x000000ff;
        }
        public static uint Gvalue(uint rgb)
        {
            return (rgb & 0x0000ff00) >> 8;
        }
        public static uint Bvalue(uint rgb)
        {
            return (rgb & 0x00ff0000) >> 16;
        }

        public const int COLOR_TABLE_SIZE = 16;
    }
}
