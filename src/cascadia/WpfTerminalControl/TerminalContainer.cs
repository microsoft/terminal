// <copyright file="TerminalContainer.cs" company="Microsoft Corporation">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>

namespace Microsoft.Terminal.Wpf
{
    using System;
    using System.Runtime.InteropServices;
    using System.Windows;
    using System.Windows.Interop;

    /// <summary>
    /// The container class that hosts the native hwnd terminal.
    /// </summary>
    /// <remarks>
    /// This class is only left public since xaml cannot work with internal classes.
    /// </remarks>
    public class TerminalContainer : HwndHost
    {
        private ITerminalConnection connection;
        private IntPtr hwnd;
        private IntPtr terminal;

        private NativeMethods.ScrollCallback scrollCallback;
        private NativeMethods.WriteCallback writeCallback;

        // We keep track of the DPI scale since we could get a DPI changed event before we are able to initialize the terminal.
        private DpiScale dpiScale = new DpiScale(NativeMethods.USER_DEFAULT_SCREEN_DPI, NativeMethods.USER_DEFAULT_SCREEN_DPI);

        /// <summary>
        /// Initializes a new instance of the <see cref="TerminalContainer"/> class.
        /// </summary>
        public TerminalContainer()
        {
            this.MessageHook += this.TerminalContainer_MessageHook;
            this.GotFocus += this.TerminalContainer_GotFocus;
            this.Focusable = true;
        }

        /// <summary>
        /// Event that is fired when the terminal buffer scrolls from text output.
        /// </summary>
        internal event EventHandler<(int viewTop, int viewHeight, int bufferSize)> TerminalScrolled;

        /// <summary>
        /// Event that is fired when the user engages in a mouse scroll over the terminal hwnd.
        /// </summary>
        internal event EventHandler<int> UserScrolled;

        /// <summary>
        /// Gets the character rows available to the terminal.
        /// </summary>
        internal int Rows { get; private set; }

        /// <summary>
        /// Gets the character columns available to the terminal.
        /// </summary>
        internal int Columns { get; private set; }

        /// <summary>
        /// Sets the connection to the terminal backend.
        /// </summary>
        internal ITerminalConnection Connection
        {
            private get
            {
                return this.connection;
            }

            set
            {
                if (this.connection != null)
                {
                    this.connection.TerminalOutput -= this.Connection_TerminalOutput;
                }

                this.connection = value;
                this.connection.TerminalOutput += this.Connection_TerminalOutput;
                this.connection.Start();
            }
        }

        /// <summary>
        /// Manually invoke a scroll of the terminal buffer.
        /// </summary>
        /// <param name="viewTop">The top line to show in the terminal.</param>
        internal void UserScroll(int viewTop)
        {
            NativeMethods.UserScroll(this.terminal, viewTop);
        }

        /// <summary>
        /// Sets the theme for the terminal. This includes font family, size, color, as well as background and foreground colors.
        /// </summary>
        /// <param name="theme">The color theme for the terminal to use.</param>
        /// <param name="fontFamily">The font family to use in the terminal.</param>
        /// <param name="fontSize">The font size to use in the terminal.</param>
        /// <param name="newDpi">The dpi that the terminal should be rendered at.</param>
        internal void SetTheme(TerminalTheme theme, string fontFamily, short fontSize, int newDpi)
        {
            NativeMethods.SetTheme(this.terminal, theme, fontFamily, fontSize, newDpi);
        }

        /// <summary>
        /// Triggers a refresh of the terminal with the given size.
        /// </summary>
        /// <param name="renderSize">Size of the rendering window.</param>
        /// <returns>Tuple with rows and columns.</returns>
        internal (int rows, int columns) TriggerResize(Size renderSize)
        {
            NativeMethods.COORD dimensions;
            NativeMethods.TriggerResize(this.terminal, renderSize.Width, renderSize.Height, out dimensions);

            this.Rows = dimensions.Y;
            this.Columns = dimensions.X;

            return (dimensions.Y, dimensions.X);
        }

        /// <summary>
        /// Resizes the terminal.
        /// </summary>
        /// <param name="rows">Number of rows to show.</param>
        /// <param name="columns">Number of columns to show.</param>
        internal void Resize(uint rows, uint columns)
        {
            NativeMethods.COORD dimensions = new NativeMethods.COORD
            {
                X = (short)columns,
                Y = (short)rows,
            };

            NativeMethods.Resize(this.terminal, dimensions);
        }

        /// <inheritdoc/>
        protected override void OnDpiChanged(DpiScale oldDpi, DpiScale newDpi)
        {
            // Save the DPI if the terminal hasn't been initialized.
            if (this.terminal == IntPtr.Zero)
            {
                this.dpiScale = newDpi;
            }
            else
            {
                NativeMethods.DpiChanged(this.terminal, (int)(NativeMethods.USER_DEFAULT_SCREEN_DPI * newDpi.DpiScaleX));
            }
        }

        /// <inheritdoc/>
        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            NativeMethods.CreateTerminal(hwndParent.Handle, out this.hwnd, out this.terminal);

            this.scrollCallback = this.OnScroll;
            this.writeCallback = this.OnWrite;

            NativeMethods.RegisterScrollCallback(this.terminal, this.scrollCallback);
            NativeMethods.RegisterWriteCallback(this.terminal, this.writeCallback);

            // If the saved DPI scale isn't the default scale, we push it to the terminal.
            if (this.dpiScale.DpiScaleX != NativeMethods.USER_DEFAULT_SCREEN_DPI)
            {
                NativeMethods.DpiChanged(this.terminal, (int)(NativeMethods.USER_DEFAULT_SCREEN_DPI * this.dpiScale.DpiScaleX));
            }

            return new HandleRef(this, this.hwnd);
        }

        /// <inheritdoc/>
        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            NativeMethods.DestroyTerminal(this.terminal);
            this.terminal = IntPtr.Zero;
        }

        private void TerminalContainer_GotFocus(object sender, RoutedEventArgs e)
        {
            NativeMethods.SetFocus(this.hwnd);
        }

        private IntPtr TerminalContainer_MessageHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (hwnd == this.hwnd)
            {
                switch ((NativeMethods.WindowMessage)msg)
                {
                    case NativeMethods.WindowMessage.WM_MOUSEACTIVATE:
                        this.Focus();
                        NativeMethods.SetFocus(this.hwnd);
                        break;
                    case NativeMethods.WindowMessage.WM_LBUTTONDOWN:
                        this.LeftClickHandler((int)lParam);
                        break;
                    case NativeMethods.WindowMessage.WM_RBUTTONDOWN:
                        if (NativeMethods.IsSelectionActive(this.terminal))
                        {
                            Clipboard.SetText(NativeMethods.GetSelection(this.terminal));
                        }
                        else
                        {
                            this.connection.WriteInput(Clipboard.GetText());
                        }

                        break;
                    case NativeMethods.WindowMessage.WM_MOUSEMOVE:
                        this.MouseMoveHandler((int)wParam, (int)lParam);
                        break;
                    case NativeMethods.WindowMessage.WM_KEYDOWN:
                        NativeMethods.ClearSelection(this.terminal);
                        NativeMethods.SendKeyEvent(this.terminal, wParam);
                        break;
                    case NativeMethods.WindowMessage.WM_CHAR:
                        NativeMethods.SendCharEvent(this.terminal, (char)wParam);
                        break;
                    case NativeMethods.WindowMessage.WM_WINDOWPOSCHANGED:
                        var windowpos = (NativeMethods.WINDOWPOS)Marshal.PtrToStructure(lParam, typeof(NativeMethods.WINDOWPOS));
                        if (((NativeMethods.SetWindowPosFlags)windowpos.flags).HasFlag(NativeMethods.SetWindowPosFlags.SWP_NOSIZE))
                        {
                            break;
                        }

                        NativeMethods.TriggerResize(this.terminal, windowpos.cx, windowpos.cy, out var dimensions);

                        this.connection?.Resize((uint)dimensions.Y, (uint)dimensions.X);
                        this.Columns = dimensions.X;
                        this.Rows = dimensions.Y;

                        break;
                    case NativeMethods.WindowMessage.WM_MOUSEWHEEL:
                        var delta = ((int)wParam) >> 16;
                        this.UserScrolled?.Invoke(this, delta);
                        break;
                }
            }

            return IntPtr.Zero;
        }

        private void LeftClickHandler(int lParam)
        {
            var altPressed = NativeMethods.GetKeyState((int)NativeMethods.VirtualKey.VK_MENU) < 0;
            var x = (short)(((int)lParam << 16) >> 16);
            var y = (short)((int)lParam >> 16);
            NativeMethods.COORD cursorPosition = new NativeMethods.COORD()
            {
                X = x,
                Y = y,
            };

            NativeMethods.StartSelection(this.terminal, cursorPosition, altPressed);
        }

        private void MouseMoveHandler(int wParam, int lParam)
        {
            if (((int)wParam & 0x0001) == 1)
            {
                var x = (short)(((int)lParam << 16) >> 16);
                var y = (short)((int)lParam >> 16);
                NativeMethods.COORD cursorPosition = new NativeMethods.COORD()
                {
                    X = x,
                    Y = y,
                };
                NativeMethods.MoveSelection(this.terminal, cursorPosition);
            }
        }

        private void Connection_TerminalOutput(object sender, TerminalOutputEventArgs e)
        {
            if (this.terminal != IntPtr.Zero)
            {
                NativeMethods.SendTerminalOutput(this.terminal, e.Data);
            }
        }

        private void OnScroll(int viewTop, int viewHeight, int bufferSize)
        {
            this.TerminalScrolled?.Invoke(this, (viewTop, viewHeight, bufferSize));
        }

        private void OnWrite(string data)
        {
            this.connection?.WriteInput(data);
        }
    }
}
