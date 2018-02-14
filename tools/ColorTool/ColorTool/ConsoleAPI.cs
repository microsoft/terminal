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

        ////////////////////////////////////////////////////////////////////////

        public static uint RGB(int r, int g, int b)
        {
            return (uint)r + (((uint)g) << 8) + (((uint)b) << 16);
        }

        public const int COLOR_TABLE_SIZE = 16;
    }
}
