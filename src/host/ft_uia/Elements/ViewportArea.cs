//----------------------------------------------------------------------------------------------------------------------
// <copyright file="ViewportArea.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Wrapper and helper for instantiating and interacting with the main text region (viewport area) of the console.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Elements
{
    using System;
    using System.Collections.Generic;
    using System.Drawing;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.Text;

    using OpenQA.Selenium.Appium;

    using WEX.Logging.Interop;
    using WEX.TestExecution;

    using Conhost.UIA.Tests.Common;
    using Conhost.UIA.Tests.Common.NativeMethods;
    using OpenQA.Selenium;

    public class ViewportArea : IDisposable
    {
        private CmdApp app;
        private Point clientTopLeft;
        private Size sizeFont;

        private ViewportStates state;

        public enum ViewportStates
        {
            Normal,
            Mark, // keyboard selection
            Select, // mouse selection
            Scroll
        }

        public ViewportArea(CmdApp app)
        {
            this.app = app;

            this.state = ViewportStates.Normal;

            this.InitializeFont();
            this.InitializeWindow();
        }

        ~ViewportArea()
        {
            this.Dispose(false);
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            // Don't need to dispose of anything now, but this helps maintain the pattern used by other controls.
        }

        private void InitializeFont()
        {
            AutoHelpers.LogInvariant("Initializing font data for viewport area...");
            this.sizeFont = new Size();

            IntPtr hCon = app.GetStdOutHandle();
            Verify.IsNotNull(hCon, "Check that we obtained the console output handle.");

            WinCon.CONSOLE_FONT_INFO cfi = new WinCon.CONSOLE_FONT_INFO();
            NativeMethods.Win32BoolHelper(WinCon.GetCurrentConsoleFont(hCon, false, out cfi), "Attempt to get current console font.");

            this.sizeFont.Width = cfi.dwFontSize.X;
            this.sizeFont.Height = cfi.dwFontSize.Y;
            AutoHelpers.LogInvariant("Font size is X:{0} Y:{1}", this.sizeFont.Width, this.sizeFont.Height);
        }

        private void InitializeWindow()
        {
            AutoHelpers.LogInvariant("Initializing window data for viewport area...");

            IntPtr hWnd = app.GetWindowHandle();

            User32.RECT lpRect;
            User32.GetClientRect(hWnd, out lpRect);

            int style = User32.GetWindowLong(hWnd, User32.GWL_STYLE);
            int exStyle = User32.GetWindowLong(hWnd, User32.GWL_EXSTYLE);

            Verify.IsTrue(User32.AdjustWindowRectEx(ref lpRect, style, false, exStyle));

            this.clientTopLeft = new Point();
            this.clientTopLeft.X = Math.Abs(lpRect.left);
            this.clientTopLeft.Y = Math.Abs(lpRect.top);
            AutoHelpers.LogInvariant("Top left corner of client area is at X:{0} Y:{1}", this.clientTopLeft.X, this.clientTopLeft.Y);
        }

        public void ExitModes()
        {
            app.UIRoot.SendKeys(Keys.Escape);
            this.state = ViewportStates.Normal;
        }
        
        public void EnterMode(ViewportStates state)
        {
            if (state == ViewportStates.Normal)
            {
                ExitModes();
                return;
            }

            var titleBar = app.UIRoot.FindElementByAccessibilityId("TitleBar");
            app.Session.Mouse.ContextClick(titleBar.Coordinates);

            Globals.WaitForTimeout();
            var contextMenu = app.Session.FindElementByClassName(Globals.PopupMenuClassId);

            var editButton = contextMenu.FindElementByName("Edit");

            editButton.Click();
            Globals.WaitForTimeout();

            Globals.WaitForTimeout();

            AppiumWebElement subMenuButton;
            switch (state)
            {
                case ViewportStates.Mark:
                    subMenuButton = app.Session.FindElementByName("Mark");
                    break;
                default:
                    throw new NotImplementedException(AutoHelpers.FormatInvariant("Set Mode doesn't yet support type of '{0}'", state.ToString()));
            }

            subMenuButton.Click();
            Globals.WaitForTimeout();

            this.state = state;
        }


        // Accepts Point in characters. Will convert to pixels and move to the right location relative to this viewport.
        public void MouseMove(Point pt)
        {
            Log.Comment($"Character position {pt.X}, {pt.Y}");

            Point modPoint = pt;
            ConvertCharacterOffsetToPixelPosition(ref modPoint);

            Log.Comment($"Pixel position {modPoint.X}, {modPoint.Y}");

            app.Session.Mouse.MouseMove(app.UIRoot.Coordinates, modPoint.X, modPoint.Y);
        }

        public void MouseDown()
        {
            app.Session.Mouse.MouseDown(null);
        }

        public void MouseUp()
        {
            app.Session.Mouse.MouseUp(null);
        }

        private void ConvertCharacterOffsetToPixelPosition(ref Point pt)
        {
            // Scale by pixel count per character
            pt.X *= this.sizeFont.Width;
            pt.Y *= this.sizeFont.Height;

            // Move it to center of character
            pt.X += this.sizeFont.Width / 2;
            pt.Y += this.sizeFont.Height / 2;

            // Adjust to the top left corner of the client rectangle.
            pt.X += this.clientTopLeft.X;
            pt.Y += this.clientTopLeft.Y;
        }

        public WinCon.CHAR_INFO GetCharInfoAt(IntPtr handle, Point pt)
        {
            Size size = new Size(1, 1);
            Rectangle rect = new Rectangle(pt, size);

            WinCon.CHAR_INFO[,] data = GetCharInfoInRectangle(handle, rect);

            return data[0, 0];
        }

        public WinCon.CHAR_INFO[,] GetCharInfoInRectangle(IntPtr handle, Rectangle rect)
        {
            WinCon.SMALL_RECT readRectangle = new WinCon.SMALL_RECT();
            readRectangle.Top = (short)rect.Top;
            readRectangle.Bottom = (short)(rect.Bottom - 1);
            readRectangle.Left = (short)rect.Left;
            readRectangle.Right = (short)(rect.Right - 1);

            WinCon.COORD dataBufferSize = new WinCon.COORD();
            dataBufferSize.X = (short)rect.Width;
            dataBufferSize.Y = (short)rect.Height;

            WinCon.COORD dataBufferPos = new WinCon.COORD();
            dataBufferPos.X = 0;
            dataBufferPos.Y = 0;

            WinCon.CHAR_INFO[,] data = new WinCon.CHAR_INFO[dataBufferSize.Y, dataBufferSize.X];

            NativeMethods.Win32BoolHelper(WinCon.ReadConsoleOutput(handle, data, dataBufferSize, dataBufferPos, ref readRectangle), string.Format("Attempting to read rectangle (L: {0}, T: {1}, R: {2}, B: {3}) from output buffer.", readRectangle.Left, readRectangle.Top, readRectangle.Right, readRectangle.Bottom));

            return data;
        }

        public IEnumerable<string> GetLinesInRectangle(IntPtr handle, Rectangle rect)
        {
            WinCon.CHAR_INFO[,] data = GetCharInfoInRectangle(handle, rect);
            List<string> lines = new List<string>();

            for (int row = 0; row < data.GetLength(0); row++)
            {
                StringBuilder builder = new StringBuilder();

                for (int col = 0; col < data.GetLength(1); col++)
                {
                    char z = data[row, col].UnicodeChar;
                    builder.Append(z);
                }

                lines.Add(builder.ToString());
            }

            return lines;
        }
    }
}