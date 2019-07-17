using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace Microsoft.Terminal.Wpf
{
    /// <summary>
    /// Follow steps 1a or 1b and then 2 to use this custom control in a XAML file.
    ///
    /// Step 1a) Using this custom control in a XAML file that exists in the current project.
    /// Add this XmlNamespace attribute to the root element of the markup file where it is 
    /// to be used:
    ///
    ///     xmlns:MyNamespace="clr-namespace:WpfTerminalControl"
    ///
    ///
    /// Step 1b) Using this custom control in a XAML file that exists in a different project.
    /// Add this XmlNamespace attribute to the root element of the markup file where it is 
    /// to be used:
    ///
    ///     xmlns:MyNamespace="clr-namespace:WpfTerminalControl;assembly=WpfTerminalControl"
    ///
    /// You will also need to add a project reference from the project where the XAML file lives
    /// to this project and Rebuild to avoid compilation errors:
    ///
    ///     Right click on the target project in the Solution Explorer and
    ///     "Add Reference"->"Projects"->[Browse to and select this project]
    ///
    ///
    /// Step 2)
    /// Go ahead and use your control in the XAML file.
    ///
    ///     <MyNamespace:TerminalControl/>
    ///
    /// </summary>
    public class TerminalContainer : HwndHost
    {
        private ITerminalConnection connection;
        private IntPtr hwnd;
        private IntPtr terminal;
        private char? highSurrogate;

        private static NativeMethods.ScrollCallback callback;

        public event EventHandler<(int viewTop, int viewHeight, int bufferSize)> TerminalScrolled;

        public event EventHandler<int> UserScrolled;

        public TerminalContainer()
        {
            this.MessageHook += TerminalContainer_MessageHook;

            this.GotFocus += TerminalContainer_GotFocus;
            this.Focusable = true;
        }

        public void UserScroll(int viewTop)
        {
            NativeMethods.UserScroll(this.terminal, viewTop);
        }

        protected override void OnDpiChanged(DpiScale oldDpi, DpiScale newDpi)
        {
            NativeMethods.DpiChanged(this.terminal, (int)(96 * newDpi.DpiScaleX));
        }

        private void TerminalContainer_GotFocus(object sender, RoutedEventArgs e)
        {
            NativeMethods.SetFocus(this.hwnd);
        }

        private IntPtr TerminalContainer_MessageHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (hwnd == this.hwnd)
            {
                switch ((PInvoke.User32.WindowMessage)msg)
                {
                    case PInvoke.User32.WindowMessage.WM_MOUSEACTIVATE:
                        this.Focus();
                        NativeMethods.SetFocus(this.hwnd);
                        break;
                    case PInvoke.User32.WindowMessage.WM_LBUTTONDOWN:
                        this.LeftClickHandler((int)lParam);
                        break;
                    case PInvoke.User32.WindowMessage.WM_RBUTTONDOWN:
                        if (NativeMethods.IsSelectionActive(this.terminal))
                        {
                            Clipboard.SetText(NativeMethods.GetSelection(this.terminal));
                        }
                        else
                        {
                            NativeMethods.SendTerminalOutput(this.terminal, Clipboard.GetText());
                        }
                        break;
                    case PInvoke.User32.WindowMessage.WM_MOUSEMOVE:
                        this.MouseMoveHandler((int)wParam, (int)lParam);
                        break;
                    case PInvoke.User32.WindowMessage.WM_KEYDOWN:
                        NativeMethods.ClearSelection(this.terminal);
                        break;
                    case PInvoke.User32.WindowMessage.WM_CHAR:
                        this.HandleChar((char)wParam);
                        break;
                    case PInvoke.User32.WindowMessage.WM_WINDOWPOSCHANGED:
                        var windowpos = (NativeMethods.WINDOWPOS)Marshal.PtrToStructure(lParam, typeof(NativeMethods.WINDOWPOS));
                        if (((PInvoke.User32.SetWindowPosFlags)windowpos.flags).HasFlag(PInvoke.User32.SetWindowPosFlags.SWP_NOSIZE))
                        {
                            break;
                        }

                        NativeMethods.TriggerResize(this.terminal, windowpos.cx, windowpos.cy, out int columns, out int rows);
                        this.connection?.Resize((uint)rows, (uint)columns);
                        break;
                    case PInvoke.User32.WindowMessage.WM_MOUSEWHEEL:
                        var delta = (((int)wParam) >> 16);
                        this.UserScrolled?.Invoke(this, delta);
                        break;
                }
            }

            return IntPtr.Zero;
        }

        private void LeftClickHandler(int lParam)
        {
            var altPressed = NativeMethods.GetKeyState((int)PInvoke.User32.VirtualKey.VK_MENU) < 0;
            var x = (short)(((int)lParam << 16) >> 16);
            var y = (short)((int)lParam >> 16);
            PInvoke.COORD cursorPosition = new PInvoke.COORD()
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
                PInvoke.COORD cursorPosition = new PInvoke.COORD()
                {
                    X = x,
                    Y = y,
                };
                NativeMethods.MoveSelection(this.terminal, cursorPosition);
            }
        }

        private bool HandleChar(char character)
        {
            if (this.connection == null)
            {
                return false;
            }

            if (char.IsHighSurrogate(character))
            {
                if (this.highSurrogate.HasValue)
                {
                    // Invalid unicode sequence is being sent, write out the unicode replacement character '�' instead.
                    this.connection.WriteInput("�");
                }

                this.highSurrogate = character;
                return true;
            }
            else if (char.IsLowSurrogate(character) && this.highSurrogate.HasValue)
            {
                var possiblyValid = $"{this.highSurrogate.Value}{character}";
                this.highSurrogate = null;
                this.connection.WriteInput(possiblyValid);
                return true;
            }
            else if (char.IsLowSurrogate(character) && this.highSurrogate == null)
            {
                this.connection.WriteInput("�");
                return true;
            }
            else
            {
                this.connection.WriteInput(character.ToString());
                return true;
            }
        }

        public ITerminalConnection Connection
        {
            private get
            {
                return this.connection;
            }
            set
            {
                if (this.connection != null)
                {
                    this.connection.TerminalOutput -= Connection_TerminalOutput;
                    this.connection.TerminalDisconnected -= Connection_TerminalDisconnected;
                }

                this.connection = value;
                this.connection.TerminalOutput += Connection_TerminalOutput;
                this.connection.TerminalDisconnected += Connection_TerminalDisconnected;
                this.connection.Start();
            }
        }

        private void Connection_TerminalDisconnected(object sender, EventArgs e)
        {
            // TODO
        }

        private void Connection_TerminalOutput(object sender, TerminalOutputEventArgs e)
        {
            if (this.terminal != IntPtr.Zero)
            {
                NativeMethods.SendTerminalOutput(this.terminal, e.Data);
            }
        }

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {

            NativeMethods.CreateTerminal(hwndParent.Handle, out this.hwnd, out this.terminal);

            callback = this.OnScroll;
            NativeMethods.RegisterScrollCallback(this.terminal, callback);

            return new HandleRef(this, this.hwnd);
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            NativeMethods.DestroyTerminal(terminal);
            terminal = IntPtr.Zero;
        }

        private void OnScroll(int viewTop, int viewHeight, int bufferSize)
        {
            this.TerminalScrolled?.Invoke(this, (viewTop, viewHeight, bufferSize));
        }
    }
}
