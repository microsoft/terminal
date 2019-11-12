// <copyright file="NativeMethods.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System;
    using System.Runtime.InteropServices;

#pragma warning disable SA1600 // Elements should be documented
    internal static class NativeMethods
    {
        public const int USER_DEFAULT_SCREEN_DPI = 96;

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void ScrollCallback(int viewTop, int viewHeight, int bufferSize);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void WriteCallback([In, MarshalAs(UnmanagedType.LPWStr)] string data);

        public enum WindowMessage : int
        {
            /// <summary>
            /// The WM_SETFOCUS message is sent to a window after it has gained the keyboard focus.
            /// </summary>
            WM_SETFOCUS = 0x0007,

            /// <summary>
            /// The WM_KILLFOCUS message is sent to a window immediately before it loses the keyboard focus.
            /// </summary>
            WM_KILLFOCUS = 0x0008,

            /// <summary>
            /// The WM_MOUSEACTIVATE message is sent when the cursor is in an inactive window and the user presses a mouse button. The parent window receives this message only if the child window passes it to the DefWindowProc function.
            /// </summary>
            WM_MOUSEACTIVATE = 0x0021,

            /// <summary>
            /// The WM_WINDOWPOSCHANGED message is sent to a window whose size, position, or place in the Z order has changed as a result of a call to the SetWindowPos function or another window-management function.
            /// </summary>
            WM_WINDOWPOSCHANGED = 0x0047,

            /// <summary>
            /// The WM_KEYDOWN message is posted to the window with the keyboard focus when a nonsystem key is pressed. A nonsystem key is a key that is pressed when the ALT key is not pressed.
            /// </summary>
            WM_KEYDOWN = 0x0100,

            /// <summary>
            /// The WM_CHAR message is posted to the window with the keyboard focus when a WM_KEYDOWN message is translated by the TranslateMessage function. The WM_CHAR message contains the character code of the key that was pressed.
            /// </summary>
            WM_CHAR = 0x0102,

            /// <summary>
            /// The WM_MOUSEMOVE message is posted to a window when the cursor moves. If the mouse is not captured, the message is posted to the window that contains the cursor. Otherwise, the message is posted to the window that has captured the mouse.
            /// </summary>
            WM_MOUSEMOVE = 0x200,

            /// <summary>
            /// The WM_LBUTTONDOWN message is posted when the user presses the left mouse button while the cursor is in the client area of a window. If the mouse is not captured, the message is posted to the window beneath the cursor. Otherwise, the message is posted to the window that has captured the mouse.
            /// </summary>
            WM_LBUTTONDOWN = 0x0201,

            /// <summary>
            /// The WM_RBUTTONDOWN message is posted when the user presses the right mouse button while the cursor is in the client area of a window. If the mouse is not captured, the message is posted to the window beneath the cursor. Otherwise, the message is posted to the window that has captured the mouse.
            /// </summary>
            WM_RBUTTONDOWN = 0x0204,

            /// <summary>
            /// The WM_MOUSEWHEEL message is sent to the focus window when the mouse wheel is rotated. The DefWindowProc function propagates the message to the window's parent. There should be no internal forwarding of the message, since DefWindowProc propagates it up the parent chain until it finds a window that processes it.
            /// </summary>
            WM_MOUSEWHEEL = 0x020A,
        }

        public enum VirtualKey : ushort
        {
            /// <summary>
            /// ALT key
            /// </summary>
            VK_MENU = 0x12,
        }

        [Flags]
        public enum SetWindowPosFlags : uint
        {
            /// <summary>
            ///     If the calling thread and the thread that owns the window are attached to different input queues, the system posts the request to the thread that owns the window. This prevents the calling thread from blocking its execution while other threads process the request.
            /// </summary>
            SWP_ASYNCWINDOWPOS = 0x4000,

            /// <summary>
            ///     Prevents generation of the WM_SYNCPAINT message.
            /// </summary>
            SWP_DEFERERASE = 0x2000,

            /// <summary>
            ///     Draws a frame (defined in the window's class description) around the window.
            /// </summary>
            SWP_DRAWFRAME = 0x0020,

            /// <summary>
            ///     Applies new frame styles set using the SetWindowLong function. Sends a WM_NCCALCSIZE message to the window, even if the window's size is not being changed. If this flag is not specified, WM_NCCALCSIZE is sent only when the window's size is being changed.
            /// </summary>
            SWP_FRAMECHANGED = 0x0020,

            /// <summary>
            ///     Hides the window.
            /// </summary>
            SWP_HIDEWINDOW = 0x0080,

            /// <summary>
            ///     Does not activate the window. If this flag is not set, the window is activated and moved to the top of either the topmost or non-topmost group (depending on the setting of the hWndInsertAfter parameter).
            /// </summary>
            SWP_NOACTIVATE = 0x0010,

            /// <summary>
            ///     Discards the entire contents of the client area. If this flag is not specified, the valid contents of the client area are saved and copied back into the client area after the window is sized or repositioned.
            /// </summary>
            SWP_NOCOPYBITS = 0x0100,

            /// <summary>
            ///     Retains the current position (ignores X and Y parameters).
            /// </summary>
            SWP_NOMOVE = 0x0002,

            /// <summary>
            ///     Does not change the owner window's position in the Z order.
            /// </summary>
            SWP_NOOWNERZORDER = 0x0200,

            /// <summary>
            ///     Does not redraw changes. If this flag is set, no repainting of any kind occurs. This applies to the client area, the nonclient area (including the title bar and scroll bars), and any part of the parent window uncovered as a result of the window being moved. When this flag is set, the application must explicitly invalidate or redraw any parts of the window and parent window that need redrawing.
            /// </summary>
            SWP_NOREDRAW = 0x0008,

            /// <summary>
            ///     Same as the SWP_NOOWNERZORDER flag.
            /// </summary>
            SWP_NOREPOSITION = 0x0200,

            /// <summary>
            ///     Prevents the window from receiving the WM_WINDOWPOSCHANGING message.
            /// </summary>
            SWP_NOSENDCHANGING = 0x0400,

            /// <summary>
            ///     Retains the current size (ignores the cx and cy parameters).
            /// </summary>
            SWP_NOSIZE = 0x0001,

            /// <summary>
            ///     Retains the current Z order (ignores the hWndInsertAfter parameter).
            /// </summary>
            SWP_NOZORDER = 0x0004,

            /// <summary>
            ///     Displays the window.
            /// </summary>
            SWP_SHOWWINDOW = 0x0040,
        }

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint CreateTerminal(IntPtr parent, out IntPtr hwnd, out IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalSendOutput(IntPtr terminal, string lpdata);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint TerminalTriggerResize(IntPtr terminal, double width, double height, out COORD dimensions);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint TerminalResize(IntPtr terminal, COORD dimensions);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalDpiChanged(IntPtr terminal, int newDpi);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalRegisterScrollCallback(IntPtr terminal, [MarshalAs(UnmanagedType.FunctionPtr)]ScrollCallback callback);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalRegisterWriteCallback(IntPtr terminal, [MarshalAs(UnmanagedType.FunctionPtr)]WriteCallback callback);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalUserScroll(IntPtr terminal, int viewTop);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint TerminalStartSelection(IntPtr terminal, NativeMethods.COORD cursorPosition, bool altPressed);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern uint TerminalMoveSelection(IntPtr terminal, NativeMethods.COORD cursorPosition);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalClearSelection(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.LPWStr)]
        public static extern string TerminalGetSelection(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool TerminalIsSelectionActive(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void DestroyTerminal(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalSendKeyEvent(IntPtr terminal, IntPtr wParam);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalSendCharEvent(IntPtr terminal, char ch);

        [DllImport("PublicTerminalCore.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalSetTheme(IntPtr terminal, [MarshalAs(UnmanagedType.Struct)] TerminalTheme theme, string fontFamily, short fontSize, int newDpi);

        [DllImport("PublicTerminalCore.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalBlinkCursor(IntPtr terminal);

        [DllImport("PublicTerminalCore.dll", CallingConvention = CallingConvention.StdCall)]
        public static extern void TerminalSetCursorVisible(IntPtr terminal, bool visible);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr SetFocus(IntPtr hWnd);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr GetFocus();

        [DllImport("user32.dll", SetLastError = true)]
        public static extern short GetKeyState(int keyCode);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint GetCaretBlinkTime();

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

        [StructLayout(LayoutKind.Sequential)]
        public struct COORD
        {
            /// <summary>
            ///  The x-coordinate of the point.
            /// </summary>
            public short X;

            /// <summary>
            /// The x-coordinate of the point.
            /// </summary>
            public short Y;
        }
    }
#pragma warning restore SA1600 // Elements should be documented
}
