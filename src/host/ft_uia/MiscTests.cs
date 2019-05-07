//----------------------------------------------------------------------------------------------------------------------
// <copyright file="MiscTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>Miscellaneous UI Automation tests (like things from bugs)</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.Drawing;
    using System.IO;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.Threading;

    using Microsoft.Win32;

    using WEX.Common.Managed;
    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using Conhost.UIA.Tests.Common;
    using Conhost.UIA.Tests.Common.NativeMethods;
    using Conhost.UIA.Tests.Elements;
    using OpenQA.Selenium.Appium;

    [TestClass]
    public class MiscTests
    {
        public TestContext TestContext { get; set; }

        [TestMethod]
        public void DotNetSetWindowPosition()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                VersionSelector.SetConsoleVersion(reg, ConsoleVersion.V2);

                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    // maximize the window
                    AppiumWebElement titleBar = app.GetTitleBar();
                    app.Actions.DoubleClick(titleBar);

                    // wait for double click action to react.
                    Globals.WaitForTimeout();

                    // do the thing that makes it upset.
                    // MSFT:2490828 called Console.SetWindowPosition, so let's just copy the .NET source for that here...
                    // see: http://referencesource.microsoft.com/#mscorlib/system/console.cs,fcb364a853d81c57
                    SetWindowPosition(0, 0);
                }
            }
        }

        // Adapted from .NET source code...
        // See: http://referencesource.microsoft.com/#mscorlib/system/console.cs,fcb364a853d81c57
        private static void SetWindowPosition(int left, int top)
        {
            AutoHelpers.LogInvariant("Attempt to set console viewport buffer to Left: {0} and Top: {1}", left, top);

            IntPtr hConsole = WinCon.GetStdHandle(WinCon.CONSOLE_STD_HANDLE.STD_OUTPUT_HANDLE);
            // Get the size of the current console window
            WinCon.CONSOLE_SCREEN_BUFFER_INFO csbi = new WinCon.CONSOLE_SCREEN_BUFFER_INFO();
            NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfo(hConsole, out csbi), "Get console screen buffer for viewport size information.");

            WinCon.SMALL_RECT srWindow = csbi.srWindow;
            AutoHelpers.LogInvariant("Initial viewport position: {0}", srWindow);

            // Check for arithmetic underflows & overflows.
            int newRight = left + srWindow.Right - srWindow.Left + 1;
            if (left < 0 || newRight > csbi.dwSize.X || newRight < 0)
                throw new ArgumentOutOfRangeException("left");
            int newBottom = top + srWindow.Bottom - srWindow.Top + 1;
            if (top < 0 || newBottom > csbi.dwSize.Y || newBottom < 0)
                throw new ArgumentOutOfRangeException("top");

            // Preserve the size, but move the position.
            srWindow.Bottom -= (short)(srWindow.Top - top);
            srWindow.Right -= (short)(srWindow.Left - left);
            srWindow.Left = (short)left;
            srWindow.Top = (short)top;

            NativeMethods.Win32BoolHelper(WinCon.SetConsoleWindowInfo(hConsole, true, ref srWindow), string.Format("Attempt to update viewport position to {0}.", srWindow));
        }

    }
}
