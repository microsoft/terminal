//----------------------------------------------------------------------------------------------------------------------
// <copyright file="MouseWheelTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for ensuring mouse wheel functionality.</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests
{
    using System;

    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using Conhost.UIA.Tests.Common;
    using Conhost.UIA.Tests.Common.NativeMethods;
    using Conhost.UIA.Tests.Elements;

    [TestClass]
    public class MouseWheelTests
    {
        public TestContext TestContext { get; set; }

        [TestMethod]
        public void TestMouseWheel()
        {
            // Use a registry helper to backup and restore registry state before/after test
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();

                // Start our application to test
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    Log.Comment("First ensure that word wrap is off so we can get scroll bars in both directions.");
                    // Make sure wrap is off
                    app.SetWrapState(false);

                    IntPtr handle = app.GetStdOutHandle();
                    Verify.IsNotNull(handle, "Ensure we have the output handle.");

                    Log.Comment("Set up the window so the buffer is larger than the window. Retreive existing properties then set the viewport to smaller than the buffer.");

                    WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX info = app.GetScreenBufferInfo(handle);

                    info.srWindow.Left = 0;
                    info.srWindow.Right = 30;
                    info.srWindow.Top = 0;
                    info.srWindow.Bottom = 30;

                    info.dwSize.X = 100;
                    info.dwSize.Y = 100;
                    app.SetScreenBufferInfo(handle, info);

                    Log.Comment("Now retrieve the starting position of the window viewport.");

                    Log.Comment("Scroll down one.");
                    VerifyScroll(app, ScrollDir.Vertical, -1);

                    Log.Comment("Scroll right one.");
                    VerifyScroll(app, ScrollDir.Horizontal, 1);

                    Log.Comment("Scroll left one.");
                    VerifyScroll(app, ScrollDir.Horizontal, -1);

                    Log.Comment("Scroll up one.");
                    VerifyScroll(app, ScrollDir.Vertical, 1);
                }
            }
        }

        enum ScrollDir
        {
            Vertical,
            Horizontal
        }

        void VerifyScroll(CmdApp app, ScrollDir dir, int clicks)
        {
            WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX beforeScroll;
            WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX afterScroll;
            int deltaActual;
            int deltaExpected;

            beforeScroll = app.GetScreenBufferInfo();

            switch (dir)
            {
                case ScrollDir.Vertical:
                    app.ScrollWindow(clicks);
                    break;
                case ScrollDir.Horizontal:
                    app.HScrollWindow(clicks);
                    break;
                default:
                    throw new NotSupportedException();
            }

            // Give the window message a moment to take effect.
            Globals.WaitForTimeout();

            switch (dir)
            {
                case ScrollDir.Vertical:
                    deltaExpected = clicks * app.GetRowsPerScroll();
                    break;
                case ScrollDir.Horizontal:
                    deltaExpected = clicks * app.GetColsPerScroll();
                    break;
                default:
                    throw new NotSupportedException();
            }
            
            afterScroll = app.GetScreenBufferInfo();

            switch (dir)
            {
                case ScrollDir.Vertical:
                    // Scrolling "negative" vertically is pulling the wheel downward which makes the lines move down.
                    // This means that if you scroll down from the top, before = 0 and after = 3. 0 - 3 = -3.
                    // The - sign of the delta here then aligns with the down = negative rule.
                    deltaActual = beforeScroll.srWindow.Top - afterScroll.srWindow.Top;
                    break;
                case ScrollDir.Horizontal:
                    // Scrolling "negative" horizontally is pushing the wheel left which makes lines move left.
                    // This means that if you scroll left, before = 3 and after = 0. 0 - 3 = -3.
                    // The - sign of the detal here then alighs with the left = negative rule.
                    deltaActual = afterScroll.srWindow.Left - beforeScroll.srWindow.Left;
                    break;
                default:
                    throw new NotSupportedException();
            }

            Verify.AreEqual(deltaExpected, deltaActual);
        }
    }
}
