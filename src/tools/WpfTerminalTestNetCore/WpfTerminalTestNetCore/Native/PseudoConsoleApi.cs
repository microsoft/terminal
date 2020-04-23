using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.InteropServices;
using static WpfTerminalTestNetCore.Native.ProcessApi;

namespace WpfTerminalTestNetCore.Native
{
    /// <summary>
    /// PInvoke signatures for win32 pseudo console api
    /// </summary>
    static class PseudoConsoleApi
    {
        internal const uint PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE = 0x00020016;
        internal const uint PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE = 0x0002000A;

        [StructLayout(LayoutKind.Sequential)]
        internal struct COORD
        {
            public short X;
            public short Y;
        }

        [DllImport("conpty.dll", SetLastError = true)]
        internal static extern int CreatePseudoConsole(COORD size, SafeFileHandle hInput, SafeFileHandle hOutput, uint dwFlags, out IntPtr phPC);

        [DllImport("conpty.dll", SetLastError = true)]
        internal static extern int ConptyCreateStartupInfo(IntPtr hPC, out STARTUPINFOEX pSiExW);

        [DllImport("conpty.dll", SetLastError = true)]
        internal static extern int ConptyDestroyStartupInfo(ref STARTUPINFOEX pSiExW);

        [DllImport("conpty.dll", SetLastError = true)]
        internal static extern int ResizePseudoConsole(IntPtr hPC, COORD size);

        [DllImport("conpty.dll", SetLastError = true)]
        internal static extern int ClosePseudoConsole(IntPtr hPC);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        internal static extern bool CreatePipe(out SafeFileHandle hReadPipe, out SafeFileHandle hWritePipe, IntPtr lpPipeAttributes, int nSize);
    }
}
