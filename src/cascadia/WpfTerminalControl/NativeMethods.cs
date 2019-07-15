using System;
using System.Runtime.InteropServices;

namespace WpfTerminalControl
{
    internal static class NativeMethods
    {
        public delegate void ScrollCallback(int viewTop, int viewHeight, int bufferSize);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint CreateTerminal(IntPtr parent, out IntPtr hwnd, out IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void SendTerminalOutput(IntPtr terminal, string lpdata);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint TriggerResize(IntPtr terminal, double width, double height, out int columns, out int rows);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void DpiChanged(IntPtr terminal, int newDpi);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void RegisterScrollCallback(IntPtr terminal, [MarshalAs(UnmanagedType.FunctionPtr)]ScrollCallback callback);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void UserScroll(IntPtr terminal, int viewTop);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void StartSelection(IntPtr terminal, PInvoke.COORD cursorPosition, bool altPressed);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void MoveSelection(IntPtr terminal, PInvoke.COORD cursorPosition);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void ClearSelection(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.LPWStr)]
        public static extern string GetSelection(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool IsSelectionActive(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void DestroyTerminal(IntPtr terminal);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr SetFocus(IntPtr hWnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern short GetKeyState(int keyCode);

        [StructLayout(LayoutKind.Sequential)]
        public struct WINDOWPOS
        {
            public IntPtr hwnd;
            public IntPtr hwndInsertAfter;
            public int x;
            public int y;
            public int cx;
            public int cy;
            public uint flags;
        }
    }
}
