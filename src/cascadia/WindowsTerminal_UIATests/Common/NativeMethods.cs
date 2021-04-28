//----------------------------------------------------------------------------------------------------------------------
// <copyright file="NativeMethods.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper class for storing P/Invoke and COM Interop definitions.</summary>
//----------------------------------------------------------------------------------------------------------------------

namespace WindowsTerminal.UIA.Tests.Common.NativeMethods
{
    using System;
    using System.Drawing;
    using System.Runtime.CompilerServices;
    using System.Runtime.InteropServices;
    using System.Text;

    using Microsoft.Win32;
    using WEX.TestExecution;
    using WEX.Logging.Interop;

    // Small extension method helpers to make C# feel closer to native.
    public static class NativeExtensions
    {
        public static int LoWord(this int val)
        {
            return val & 0xffff;
        }

        public static int HiWord(this int val)
        {
            return (val >> 16) & 0xffff;
        }
    }

    public static class NativeMethods
    {
        public static void Win32BoolHelper(bool result, string actionMessage)
        {
            if (!result)
            {
                string errorMsg = string.Format("Win32 error occurred: 0x{0:X}", Marshal.GetLastWin32Error());
                Log.Comment(errorMsg);
            }

            Verify.IsTrue(result, actionMessage);
        }

        public static void Win32NullHelper(IntPtr result, string actionMessage)
        {
            if (result == IntPtr.Zero)
            {
                string errorMsg = string.Format("Win32 error occurred: 0x{0:X}", Marshal.GetLastWin32Error());
                Log.Comment(errorMsg);
            }

            Verify.IsNotNull(result, actionMessage);
        }
    }

    public static class WinCon
    {
        [Flags()]
        public enum CONSOLE_SELECTION_INFO_FLAGS : uint
        {
            CONSOLE_NO_SELECTION = 0x0,
            CONSOLE_SELECTION_IN_PROGRESS = 0x1,
            CONSOLE_SELECTION_NOT_EMPTY = 0x2,
            CONSOLE_MOUSE_SELECTION = 0x4,
            CONSOLE_MOUSE_DOWN = 0x8
        }

        public enum CONSOLE_STD_HANDLE : int
        {
            STD_INPUT_HANDLE = -10,
            STD_OUTPUT_HANDLE = -11,
            STD_ERROR_HANDLE = -12
        }

        public enum CONSOLE_ATTRIBUTES : ushort
        {
            FOREGROUND_BLUE = 0x1,
            FOREGROUND_GREEN = 0x2,
            FOREGROUND_RED = 0x4,
            FOREGROUND_INTENSITY = 0x8,
            FOREGROUND_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN,
            FOREGROUND_CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE,
            FOREGROUND_MAGENTA = FOREGROUND_RED | FOREGROUND_BLUE,
            FOREGROUND_COLORS = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN,
            FOREGROUND_ALL = FOREGROUND_COLORS | FOREGROUND_INTENSITY,
            BACKGROUND_BLUE = 0x10,
            BACKGROUND_GREEN = 0x20,
            BACKGROUND_RED = 0x40,
            BACKGROUND_INTENSITY = 0x80,
            BACKGROUND_YELLOW = BACKGROUND_RED | BACKGROUND_GREEN,
            BACKGROUND_CYAN = BACKGROUND_GREEN | BACKGROUND_BLUE,
            BACKGROUND_MAGENTA = BACKGROUND_RED | BACKGROUND_BLUE,
            BACKGROUND_COLORS = BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_GREEN,
            BACKGROUND_ALL = BACKGROUND_COLORS | BACKGROUND_INTENSITY,
            COMMON_LVB_LEADING_BYTE = 0x100,
            COMMON_LVB_TRAILING_BYTE = 0x200,
            COMMON_LVB_GRID_HORIZONTAL = 0x400,
            COMMON_LVB_GRID_LVERTICAL = 0x800,
            COMMON_LVB_GRID_RVERTICAL = 0x1000,
            COMMON_LVB_REVERSE_VIDEO = 0x4000,
            COMMON_LVB_UNDERSCORE = 0x8000
        }

        public enum CONSOLE_OUTPUT_MODES : uint
        {
            ENABLE_PROCESSED_OUTPUT = 0x1,
            ENABLE_WRAP_AT_EOL_OUTPUT = 0x2,
            ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x4
        }

        //CHAR_INFO struct, which was a union in the old days
        // so we want to use LayoutKind.Explicit to mimic it as closely
        // as we can
        [StructLayout(LayoutKind.Explicit)]
        public struct CHAR_INFO
        {
            [FieldOffset(0)]
            internal char UnicodeChar;
            [FieldOffset(0)]
            internal char AsciiChar;
            [FieldOffset(2)] //2 bytes seems to work properly
            internal CONSOLE_ATTRIBUTES Attributes;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct COORD
        {
            public short X;
            public short Y;

            public override string ToString()
            {
                return string.Format("(X:{0} Y:{1})", X, Y);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SMALL_RECT
        {
            public short Left;
            public short Top;
            public short Right;
            public short Bottom;

            public short Width
            {
                get
                {
                    // The API returns bottom/right as the inclusive lower-right
                    // corner, so we need +1 for the true width
                    return (short)(this.Right - this.Left + 1);
                }
            }

            public short Height
            {
                get
                {
                    // The API returns bottom/right as the inclusive lower-right
                    // corner, so we need +1 for the true height
                    return (short)(this.Bottom - this.Top + 1);
                }
            }

            public override string ToString()
            {
                return string.Format("(L:{0} T:{1} R:{2} B:{3})", Left, Top, Right, Bottom);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CONSOLE_CURSOR_INFO
        {
            public uint dwSize;
            public bool bVisible;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CONSOLE_FONT_INFO
        {
            public int nFont;
            public COORD dwFontSize;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CONSOLE_SELECTION_INFO
        {
            public CONSOLE_SELECTION_INFO_FLAGS Flags;
            public COORD SelectionAnchor;
            public SMALL_RECT Selection;

            public override string ToString()
            {
                return string.Format("Flags:{0:X} Anchor:{1} Selection:{2}", Flags, SelectionAnchor, Selection);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CONSOLE_SCREEN_BUFFER_INFO
        {
            public COORD dwSize;
            public COORD dwCursorPosition;
            public CONSOLE_ATTRIBUTES wAttributes;
            public SMALL_RECT srWindow;
            public COORD dwMaximumWindowSize;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct CONSOLE_SCREEN_BUFFER_INFO_EX
        {
            public uint cbSize;
            public COORD dwSize;
            public COORD dwCursorPosition;
            public CONSOLE_ATTRIBUTES wAttributes;
            public SMALL_RECT srWindow;
            public COORD dwMaximumWindowSize;

            public CONSOLE_ATTRIBUTES wPopupAttributes;
            public bool bFullscreenSupported;

            internal COLORREF black;
            internal COLORREF darkBlue;
            internal COLORREF darkGreen;
            internal COLORREF darkCyan;
            internal COLORREF darkRed;
            internal COLORREF darkMagenta;
            internal COLORREF darkYellow;
            internal COLORREF gray;
            internal COLORREF darkGray;
            internal COLORREF blue;
            internal COLORREF green;
            internal COLORREF cyan;
            internal COLORREF red;
            internal COLORREF magenta;
            internal COLORREF yellow;
            internal COLORREF white;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct COLORREF
        {
            internal uint ColorDWORD;

            public COLORREF(Color color)
            {
                ColorDWORD = (uint)color.R + (((uint)color.G) << 8) + (((uint)color.B) << 16);
            }

            public COLORREF(uint r, uint g, uint b)
            {
                ColorDWORD = r + (g << 8) + (b << 16);
            }

            public Color GetColor()
            {
                return Color.FromArgb((int)(0x000000FFU & ColorDWORD),
                   (int)(0x0000FF00U & ColorDWORD) >> 8, (int)(0x00FF0000U & ColorDWORD) >> 16);
            }

            public void SetColor(Color color)
            {
                ColorDWORD = (uint)color.R + (((uint)color.G) << 8) + (((uint)color.B) << 16);
            }
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr GetStdHandle(CONSOLE_STD_HANDLE nStdHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool AttachConsole(UInt32 dwProcessId);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool FreeConsole();

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern bool SetConsoleTitle(string ConsoleTitle);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetConsoleMode(IntPtr hConsoleOutputHandle, out CONSOLE_OUTPUT_MODES lpMode);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern uint GetConsoleTitle(StringBuilder lpConsoleTitle, int nSize);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr GetConsoleWindow();

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetConsoleSelectionInfo(out CONSOLE_SELECTION_INFO lpConsoleSelectionInfo);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool SetConsoleWindowInfo(IntPtr hConsoleOutput, bool bAbsolute, [In] ref SMALL_RECT lpConsoleWindow);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetConsoleCursorInfo(IntPtr hConsoleOutput, out CONSOLE_CURSOR_INFO lpConsoleCursorInfo);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetConsoleScreenBufferInfo(IntPtr hConsoleOutput, out CONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetConsoleScreenBufferInfoEx(IntPtr hConsoleOutput, ref CONSOLE_SCREEN_BUFFER_INFO_EX ConsoleScreenBufferInfo);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool SetConsoleScreenBufferInfoEx(IntPtr ConsoleOutput, ref CONSOLE_SCREEN_BUFFER_INFO_EX ConsoleScreenBufferInfoEx);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool GetCurrentConsoleFont(IntPtr hConsoleOutput, bool bMaximumWindow, out CONSOLE_FONT_INFO lpConsoleCurrentFont);

        [DllImport("kernel32.dll", EntryPoint = "ReadConsoleOutputW", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool ReadConsoleOutput(
            IntPtr hConsoleOutput,
            /* This pointer is treated as the origin of a two-dimensional array of CHAR_INFO structures
            whose size is specified by the dwBufferSize parameter.*/
            [MarshalAs(UnmanagedType.LPArray), Out] CHAR_INFO[,] lpBuffer,
            COORD dwBufferSize,
            COORD dwBufferCoord,
            ref SMALL_RECT lpReadRegion);

        [DllImport("kernel32.dll", EntryPoint = "WriteConsoleOutputCharacterW", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool WriteConsoleOutputCharacter(
            IntPtr hConsoleOutput,
            string lpCharacter,
            UInt32 nLength,
            COORD dwWriteCoord,
            ref UInt32 lpNumberOfCharsWritten);

    }

    /// <summary>
    /// The definitions within this file match the winconp.h file that is generated from wincon.w
    /// Please see /windows/published/main/wincon.w
    /// </summary>
    public static class WinConP
    {
        private static readonly Guid PKEY_Console_FormatId = new Guid(0x0C570607, 0x0396, 0x43DE, new byte[] { 0x9D, 0x61, 0xE3, 0x21, 0xD7, 0xDF, 0x50, 0x26 });

        public static readonly Wtypes.PROPERTYKEY PKEY_Console_ForceV2 = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 1 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_WrapText = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 2 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_FilterOnPaste = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 3 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_CtrlKeysDisabled = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 4 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_LineSelection = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 5 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_WindowTransparency = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 6 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_TrimZeros = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 7 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_CursorType = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 8 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_CursorColor = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 9 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_InterceptCopyPaste = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 10 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_DefaultForeground = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 11 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_DefaultBackground = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 12 };
        public static readonly Wtypes.PROPERTYKEY PKEY_Console_TerminalScrolling = new Wtypes.PROPERTYKEY() { fmtid = PKEY_Console_FormatId, pid = 13 };

        public static readonly uint NT_CONSOLE_PROPS_SIG = 0xA0000002;
        public static readonly uint NT_FE_CONSOLE_PROPS_SIG = 0xA0000004;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct NT_CONSOLE_PROPS
        {
            public Shell32.DATABLOCK_HEADER dbh;
            public short wFillAttribute;
            public short wPopupFillAttribute;
            public WinCon.COORD dwScreenBufferSize;
            public WinCon.COORD dwWindowSize;
            public WinCon.COORD dwWindowOrigin;
            public int nFont;
            public int nInputBufferSize;
            public WinCon.COORD dwFontSize;
            public uint uFontFamily;
            public uint uFontWeight;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
            public string FaceName;
            public uint uCursorSize;
            public int bFullScreen;
            public int bQuickEdit;
            public int bInsertMode;
            public int bAutoPosition;
            public uint uHistoryBufferSize;
            public uint uNumberOfHistoryBuffers;
            public int bHistoryNoDup;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
            public int[] ColorTable;
            public uint CursorType;
            public WinCon.COLORREF CursorColor;
            public bool InterceptCopyPaste;
            public WinCon.COLORREF DefaultForeground;
            public WinCon.COLORREF DefaultBackground;
            public bool TerminalScrolling;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NT_FE_CONSOLE_PROPS
        {
            Shell32.DATABLOCK_HEADER dbh;
            uint uCodePage;
        }
    }

    public static class User32
    {
        // http://msdn.microsoft.com/en-us/library/windows/desktop/dd162897(v=vs.85).aspx
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public Int32 left;
            public Int32 top;
            public Int32 right;
            public Int32 bottom;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct POINT
        {
            public Int32 x;
            public Int32 y;
        }

        public const int WHEEL_DELTA = 120;

        [DllImport("user32.dll")]
        public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

        public const int GWL_STYLE = (-16);
        public const int GWL_EXSTYLE = (-20);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        public static extern bool AdjustWindowRectEx(ref RECT lpRect, int dwStyle, bool bMenu, int dwExStyle);

        [DllImport("user32.dll")]
        public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

        public enum WindowMessages : UInt32
        {
            WM_KEYDOWN = 0x0100,
            WM_KEYUP = 0x0101,
            WM_CHAR = 0x0102,
            WM_MOUSEWHEEL = 0x020A,
            WM_MOUSEHWHEEL = 0x020E,
            WM_USER = 0x0400,
            CM_SET_KEY_STATE = WM_USER + 18
        }

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        public static extern IntPtr SendMessage(IntPtr hWnd, WindowMessages Msg, Int32 wParam, IntPtr lParam);

        public enum SPI : uint
        {
            SPI_GETWHEELSCROLLLINES = 0x0068,
            SPI_GETWHEELSCROLLCHARACTERS = 0x006C
        }

        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        public static extern bool SystemParametersInfo(SPI uiAction, uint uiParam, ref uint pvParam, uint fWinIni);


        public enum WinEventId : uint
        {
            EVENT_CONSOLE_CARET = 0x4001,
            EVENT_CONSOLE_UPDATE_REGION = 0x4002,
            EVENT_CONSOLE_UPDATE_SIMPLE = 0x4003,
            EVENT_CONSOLE_UPDATE_SCROLL = 0x4004,
            EVENT_CONSOLE_LAYOUT = 0x4005,
            EVENT_CONSOLE_START_APPLICATION = 0x4006,
            EVENT_CONSOLE_END_APPLICATION = 0x4007
        }

        [Flags]
        public enum WinEventFlags : uint
        {
            WINEVENT_OUTOFCONTEXT = 0x0000,  // Events are ASYNC
            WINEVENT_SKIPOWNTHREAD = 0x0001,  // Don't call back for events on installer's thread
            WINEVENT_SKIPOWNPROCESS = 0x0002,  // Don't call back for events on installer's process
            WINEVENT_INCONTEXT = 0x0004,  // Events are SYNC, this causes your dll to be injected into every process
        }

        public delegate void WinEventDelegate(IntPtr hWinEventHook, WinEventId eventType, IntPtr hwnd, int idObject, int idChild, uint dwEventThread, uint dwmsEventTime);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr SetWinEventHook(WinEventId eventMin, WinEventId eventMax, IntPtr hmodWinEventProc, WinEventDelegate lpfnWinEventProc, uint idProcess, uint idThread, WinEventFlags dwFlags);

        [DllImport("user32.dll")]
        public static extern bool UnhookWinEvent(IntPtr hWinEventHook);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

        public struct MSG
        {
            public IntPtr hwnd;
            public uint message;
            public IntPtr wParam;
            public IntPtr lParam;
            public uint time;
            public POINT pt;
        }

        public enum PM : uint
        {
            PM_NOREMOVE = 0x0000,
            PM_REMOVE = 0x0001,
            PM_NOYIELD = 0x0002,
        }

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool PeekMessage(out MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax, PM wRemoveMsg);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern int GetMessage(out MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr DispatchMessage(ref MSG lpmsg);
    }

    public static class Shell32
    {
        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb773249(v=vs.85).aspx
        [StructLayout(LayoutKind.Sequential)]
        public struct DATABLOCK_HEADER
        {
            public int cbSize;
            public int dwSignature;
        }

        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb774944(v=vs.85).aspx
        // http://pinvoke.net/default.aspx/Enums/SLGP_FLAGS.html
        /// <summary>IShellLink.GetPath fFlags: Flags that specify the type of path information to retrieve</summary>
        [Flags()]
        public enum SLGP_FLAGS
        {
            /// <summary>Retrieves the standard short (8.3 format) file name</summary>
            SLGP_SHORTPATH = 0x1,
            /// <summary>Retrieves the Universal Naming Convention (UNC) path name of the file</summary>
            SLGP_UNCPRIORITY = 0x2,
            /// <summary>Retrieves the raw path name. A raw path is something that might not exist and may include environment variables that need to be expanded</summary>
            SLGP_RAWPATH = 0x4
        }

        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb774952(v=vs.85).aspx
        // http://pinvoke.net/default.aspx/Enums/SLR_FLAGS.html
        /// <summary>IShellLink.Resolve fFlags</summary>
        [Flags()]
        public enum SLR_FLAGS
        {
            /// <summary>
            /// Do not display a dialog box if the link cannot be resolved. When SLR_NO_UI is set,
            /// the high-order word of fFlags can be set to a time-out value that specifies the
            /// maximum amount of time to be spent resolving the link. The function returns if the
            /// link cannot be resolved within the time-out duration. If the high-order word is set
            /// to zero, the time-out duration will be set to the default value of 3,000 milliseconds
            /// (3 seconds). To specify a value, set the high word of fFlags to the desired time-out
            /// duration, in milliseconds.
            /// </summary>
            SLR_NO_UI = 0x1,
            /// <summary>Obsolete and no longer used</summary>
            SLR_ANY_MATCH = 0x2,
            /// <summary>If the link object has changed, update its path and list of identifiers.
            /// If SLR_UPDATE is set, you do not need to call IPersistFile::IsDirty to determine
            /// whether or not the link object has changed.</summary>
            SLR_UPDATE = 0x4,
            /// <summary>Do not update the link information</summary>
            SLR_NOUPDATE = 0x8,
            /// <summary>Do not execute the search heuristics</summary>
            SLR_NOSEARCH = 0x10,
            /// <summary>Do not use distributed link tracking</summary>
            SLR_NOTRACK = 0x20,
            /// <summary>Disable distributed link tracking. By default, distributed link tracking tracks
            /// removable media across multiple devices based on the volume name. It also uses the
            /// Universal Naming Convention (UNC) path to track remote file systems whose drive letter
            /// has changed. Setting SLR_NOLINKINFO disables both types of tracking.</summary>
            SLR_NOLINKINFO = 0x40,
            /// <summary>Call the Microsoft Windows Installer</summary>
            SLR_INVOKE_MSI = 0x80
        }

        [ComImport, Guid("00021401-0000-0000-C000-000000000046")]
        public class ShellLink
        {
            // Making new of this class will call CoCreate e.g. new ShellLink();
            // Cast to one of the interfaces below will QueryInterface. e.g. (IPersistFile)new ShellLink();
        }

        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb774950(v=vs.85).aspx
        // http://pinvoke.net/default.aspx/Interfaces/IShellLinkW.html
        /// <summary>The IShellLink interface allows Shell links to be created, modified, and resolved</summary>
        [ComImport(), InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("000214F9-0000-0000-C000-000000000046")]
        public interface IShellLinkW
        {
            /// <summary>Retrieves the path and file name of a Shell link object</summary>
            void GetPath([Out(), MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszFile, int cchMaxPath, out WinBase.WIN32_FIND_DATAW pfd, SLGP_FLAGS fFlags);
            /// <summary>Retrieves the list of item identifiers for a Shell link object</summary>
            void GetIDList(out IntPtr ppidl);
            /// <summary>Sets the pointer to an item identifier list (PIDL) for a Shell link object.</summary>
            void SetIDList(IntPtr pidl);
            /// <summary>Retrieves the description string for a Shell link object</summary>
            void GetDescription([Out(), MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszName, int cchMaxName);
            /// <summary>Sets the description for a Shell link object. The description can be any application-defined string</summary>
            void SetDescription([MarshalAs(UnmanagedType.LPWStr)] string pszName);
            /// <summary>Retrieves the name of the working directory for a Shell link object</summary>
            void GetWorkingDirectory([Out(), MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszDir, int cchMaxPath);
            /// <summary>Sets the name of the working directory for a Shell link object</summary>
            void SetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)] string pszDir);
            /// <summary>Retrieves the command-line arguments associated with a Shell link object</summary>
            void GetArguments([Out(), MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszArgs, int cchMaxPath);
            /// <summary>Sets the command-line arguments for a Shell link object</summary>
            void SetArguments([MarshalAs(UnmanagedType.LPWStr)] string pszArgs);
            /// <summary>Retrieves the hot key for a Shell link object</summary>
            void GetHotkey(out short pwHotkey);
            /// <summary>Sets a hot key for a Shell link object</summary>
            void SetHotkey(short wHotkey);
            /// <summary>Retrieves the show command for a Shell link object</summary>
            void GetShowCmd(out int piShowCmd);
            /// <summary>Sets the show command for a Shell link object. The show command sets the initial show state of the window.</summary>
            void SetShowCmd(int iShowCmd);
            /// <summary>Retrieves the location (path and index) of the icon for a Shell link object</summary>
            void GetIconLocation([Out(), MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszIconPath,
                int cchIconPath, out int piIcon);
            /// <summary>Sets the location (path and index) of the icon for a Shell link object</summary>
            void SetIconLocation([MarshalAs(UnmanagedType.LPWStr)] string pszIconPath, int iIcon);
            /// <summary>Sets the relative path to the Shell link object</summary>
            void SetRelativePath([MarshalAs(UnmanagedType.LPWStr)] string pszPathRel, int dwReserved);
            /// <summary>Attempts to find the target of a Shell link, even if it has been moved or renamed</summary>
            void Resolve(IntPtr hwnd, SLR_FLAGS fFlags);
            /// <summary>Sets the path and file name of a Shell link object</summary>
            void SetPath([MarshalAs(UnmanagedType.LPWStr)] string pszFile);
        }

        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb774916(v=vs.85).aspx
        // http://pinvoke.net/default.aspx/Interfaces/IShellLonkDataList.html
        [ComImport(), InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("45e2b4ae-b1c3-11d0-b92f-00a0c90312e1")]
        public interface IShellLinkDataList
        {
            void AddDataBlock(IntPtr pDataBlock);
            void CopyDataBlock(uint dwSig, out IntPtr ppDataBlock);
            void RemoveDataBlock(uint dwSig);
            void GetFlags(out uint pdwFlags);
            void SetFlags(uint dwFlags);
        }

        // http://msdn.microsoft.com/en-us/library/windows/desktop/ms688695(v=vs.85).aspx
        // http://pinvoke.net/default.aspx/Interfaces/IPersist.html
        [ComImport, Guid("0000010c-0000-0000-c000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        public interface IPersist
        {
            [PreserveSig]
            void GetClassID(out Guid pClassID);
        }


        // http://msdn.microsoft.com/en-us/library/windows/desktop/ms687223(v=vs.85).aspx
        // http://www.pinvoke.net/default.aspx/Interfaces/IPersistFile.html
        [ComImport, Guid("0000010b-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        public interface IPersistFile : IPersist
        {
            new void GetClassID(out Guid pClassID);

            [PreserveSig]
            int IsDirty();

            [PreserveSig]
            void Load([In, MarshalAs(UnmanagedType.LPWStr)] string pszFileName, uint dwMode);

            [PreserveSig]
            void Save([In, MarshalAs(UnmanagedType.LPWStr)] string pszFileName,
                      [In, MarshalAs(UnmanagedType.Bool)] bool fRemember);

            [PreserveSig]
            void SaveCompleted([In, MarshalAs(UnmanagedType.LPWStr)] string pszFileName);

            [PreserveSig]
            void GetCurFile([In, MarshalAs(UnmanagedType.LPWStr)] string ppszFileName);
        }

        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb761474(v=vs.85).aspx
        // http://www.pinvoke.net/default.aspx/Interfaces/IPropertyStore.html
        [ComImport, Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        public interface IPropertyStore
        {
            [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
            void GetCount([Out] out uint cProps);

            [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
            void GetAt([In] uint iProp, out Wtypes.PROPERTYKEY pkey);

            [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
            void GetValue([In] ref Wtypes.PROPERTYKEY key, out object pv);

            [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
            void SetValue([In] ref Wtypes.PROPERTYKEY key, [In] ref object pv);

            [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
            void Commit();
        }
    }

    public static class WinBase
    {
        // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365740(v=vs.85).aspx
        // http://www.pinvoke.net/default.aspx/Structures/WIN32_FIND_DATA.html
        // The CharSet must match the CharSet of the corresponding PInvoke signature
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct WIN32_FIND_DATAW
        {
            public uint dwFileAttributes;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftCreationTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftLastAccessTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftLastWriteTime;
            public uint nFileSizeHigh;
            public uint nFileSizeLow;
            public uint dwReserved0;
            public uint dwReserved1;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string cFileName;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)]
            public string cAlternateFileName;
        }

        public enum STARTF : Int32
        {
            STARTF_TITLEISLINKNAME = 0x00000800
        }


        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct STARTUPINFO
        {
            public Int32 cb;
            public string lpReserved;
            public string lpDesktop;
            public string lpTitle;
            public Int32 dwX;
            public Int32 dwY;
            public Int32 dwXSize;
            public Int32 dwYSize;
            public Int32 dwXCountChars;
            public Int32 dwYCountChars;
            public Int32 dwFillAttribute;
            public STARTF dwFlags;
            public Int16 wShowWindow;
            public Int16 cbReserved2;
            public IntPtr lpReserved2;
            public IntPtr hStdInput;
            public IntPtr hStdOutput;
            public IntPtr hStdError;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PROCESS_INFORMATION
        {
            public IntPtr hProcess;
            public IntPtr hThread;
            public int dwProcessId;
            public int dwThreadId;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct STARTUPINFOEX
        {
            public STARTUPINFO StartupInfo;
            public IntPtr lpAttributeList;
        }

        [Flags]
        public enum CP_CreationFlags : uint
        {
            CREATE_SUSPENDED = 0x4,
            CREATE_NEW_CONSOLE = 0x10,
        }

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        public static extern bool CreateProcess(string lpApplicationName,
                                                string lpCommandLine,
                                                IntPtr lpProcessAttributes,
                                                IntPtr lpThreadAttributes,
                                                bool bInheritHandles,
                                                CP_CreationFlags dwCreationFlags,
                                                IntPtr lpEnvironment,
                                                string lpCurrentDirectory,
                                                [In] ref STARTUPINFO lpStartupInfo,
                                                out PROCESS_INFORMATION lpProcessInformation);


        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        public static extern IntPtr CreateJobObject(IntPtr lpJobAttributes, IntPtr lpName);

        [DllImport("kernel32.dll")]
        public static extern bool TerminateJobObject(IntPtr hJob, uint uExitCode);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool AssignProcessToJobObject(IntPtr hJob, IntPtr hProcess);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern int ResumeThread(IntPtr hThread);

        public enum JOBOBJECTINFOCLASS : uint
        {
            JobObjectBasicProcessIdList = 3
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct JOBOBJECT_BASIC_PROCESS_ID_LIST
        {
            public uint NumberOfAssignedProcesses;
            public uint NumberOfProcessIdsInList;
            public IntPtr ProcessId;
            public IntPtr ProcessId2;
        }

        [DllImport("kernel32.dll")]
        public static extern bool QueryInformationJobObject(IntPtr hJob,
                                                            JOBOBJECTINFOCLASS JobObjectInformationClass,
                                                            IntPtr lpJobObjectInfo,
                                                            int cbJobObjectInfoLength,
                                                            IntPtr lpReturnLength);
    }

    public static class Wtypes
    {
        // http://msdn.microsoft.com/en-us/library/windows/desktop/bb773381(v=vs.85).aspx
        // http://pinvoke.net/default.aspx/Structures/PROPERTYKEY.html
        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        public struct PROPERTYKEY
        {
            public Guid fmtid;
            public uint pid;
        }
    }

    public static class ObjBase
    {
        // http://msdn.microsoft.com/en-us/library/windows/desktop/aa380337(v=vs.85).aspx
        // http://www.pinvoke.net/default.aspx/Enums/StgmConstants.html
        [Flags]
        public enum STGM
        {
            STGM_READ = 0x0,
            STGM_WRITE = 0x1,
            STGM_READWRITE = 0x2,
            STGM_SHARE_DENY_NONE = 0x40,
            STGM_SHARE_DENY_READ = 0x30,
            STGM_SHARE_DENY_WRITE = 0x20,
            STGM_SHARE_EXCLUSIVE = 0x10,
            STGM_PRIORITY = 0x40000,
            STGM_CREATE = 0x1000,
            STGM_CONVERT = 0x20000,
            STGM_FAILIFTHERE = 0x0,
            STGM_DIRECT = 0x0,
            STGM_TRANSACTED = 0x10000,
            STGM_NOSCRATCH = 0x100000,
            STGM_NOSNAPSHOT = 0x200000,
            STGM_SIMPLE = 0x8000000,
            STGM_DIRECT_SWMR = 0x400000,
            STGM_DELETEONRELEASE = 0x4000000
        }
    }
}
