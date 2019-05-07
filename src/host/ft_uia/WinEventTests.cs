//----------------------------------------------------------------------------------------------------------------------
// <copyright file="WinEventTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for the Accessibility WinEvents feature.</summary>
//----------------------------------------------------------------------------------------------------------------------

namespace Conhost.UIA.Tests
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;

    using WEX.Common.Managed;
    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using Conhost.UIA.Tests.Common;
    using Conhost.UIA.Tests.Common.NativeMethods;
    using Conhost.UIA.Tests.Elements;
    using OpenQA.Selenium;
    using System.Threading;
    using System.Runtime.InteropServices;
    using System.Drawing;

    // Test hooks adapted from C++ WinEvent accessibility sample at https://msdn.microsoft.com/en-us/library/ms971319.aspx#atg_consoleaccessibility_topic05
    [TestClass]
    public class WinEventTests : IWinEventCallbacks
    {
        public TestContext TestContext { get; set; }

        enum EventType
        {
            CaretSelection,
            CaretVisible,
            EndApplication,
            Layout,
            StartApplication,
            UpdateRegion,
            UpdateScroll,
            UpdateSimple
        }

        class EventData
        {
            EventType type;
            // Of all the WinEvents that we return in the console, the max number of parameters returned is 4.
            int[] data = new int[4];

            public EventData(EventType type, int a = 0, int b = 0, int c = 0, int d = 0)
            {
                this.type = type;
                data[0] = a;
                data[1] = b;
                data[2] = c;
                data[3] = d;
            }

            public override bool Equals(object obj)
            {
                if (typeof(EventData) == obj.GetType())
                {
                    return Equals((EventData)obj);
                }
                else
                {
                    return base.Equals(obj);
                }
            }

            public bool Equals(EventData other)
            {
                return type == other.type &&
                    data[0] == other.data[0] &&
                    data[1] == other.data[1] &&
                    data[2] == other.data[2] &&
                    data[3] == other.data[3];
            }

            public override string ToString()
            {
                return $"\r\nType: {type} Data[0]: {data[0]} Data[1]: {data[1]} Data[2]: {data[2]} Data[3]: {data[3]}\r\n";
            }

            public override int GetHashCode()
            {
                return type.GetHashCode() ^ data[0].GetHashCode() ^ data[1].GetHashCode() ^ data[2].GetHashCode() ^ data[3].GetHashCode();
            }
        }

        Queue<EventData> received = new Queue<EventData>();

        public void CaretSelection(int x, int y)
        {
            Log.Comment($"Event Console CARET SELECTION! X: {x} Y: {y}");
            received.Enqueue(new EventData(EventType.CaretSelection, x, y));
        }

        public void CaretVisible(int x, int y)
        {
            Log.Comment($"Event Console CARET VISIBLE! X: {x} Y: {y}");
            received.Enqueue(new EventData(EventType.CaretVisible, x, y));
        }

        public void EndApplication(int processId, int childId)
        {
            Log.Comment($"Event Console END APPLICATION! Process ID: {processId} - Child ID: {childId}");
            received.Enqueue(new EventData(EventType.EndApplication)); // we don't have a great way of detecting pids now so don't store it for comparison
        }

        public void Layout()
        {
            Log.Comment("Event Console LAYOUT!");
            received.Enqueue(new EventData(EventType.Layout));
        }

        public void StartApplication(int processId, int childId)
        {
            Log.Comment($"Event Console START APPLICATION! Process ID: {processId} - Child ID: {childId}");
            received.Enqueue(new EventData(EventType.StartApplication)); // we don't have a great way of detecting pids now so don't store it for comparison
        }

        public void UpdateRegion(int left, int top, int right, int bottom)
        {
            Log.Comment($"Event Console UPDATE REGION! Left: {left} - Top: {top} - Right: {right} - Bottom: {bottom}");
            received.Enqueue(new EventData(EventType.UpdateRegion, left, top, right, bottom));
        }

        public void UpdateScroll(int deltaX, int deltaY)
        {
            Log.Comment($"Event Console UPDATE SCROLL! dx: {deltaX} - dy: {deltaY}");
            received.Enqueue(new EventData(EventType.UpdateScroll, deltaX, deltaY));
        }

        public void UpdateSimple(int x, int y, int character, int attribute)
        {
            Log.Comment($"Event Console UPDATE SIMPLE! X: {x} - Y: {y} - Char: {character} - Attr: {attribute}");
            received.Enqueue(new EventData(EventType.UpdateSimple, x, y, character, attribute));
        }

        void VerifyQueue(Queue<EventData> testQueue)
        {
            while (testQueue.Count > 0)
            {
                Verify.AreEqual(testQueue.Dequeue(), received.Dequeue());
            }
        }

        public void TestTypeStringHelper(string str, CmdApp app, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex)
        {
            Queue<EventData> expected = new Queue<EventData>();

            foreach (char c in str)
            {
                // For each character, we expect a pair of messages.
                // 1. A Update Simple where the cursor was to represent the letter inserted where the cursor was.
                expected.Enqueue(new EventData(EventType.UpdateSimple, sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y, c, (int)sbiex.wAttributes));
                // Move cursor right by 1.
                sbiex.dwCursorPosition.X++;
                // 2. A caret visible to represent where the cursor moved after the letter was inserted.
                expected.Enqueue(new EventData(EventType.CaretVisible, sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y));

                // Type in the letter
                app.UIRoot.SendKeys($"{c}");

                Globals.WaitForTimeout();
            }

            // Verify that we saw the right messages
            this.VerifyQueue(expected);
        }

        delegate void AccessibilityTest(CmdApp app, ViewportArea area, IntPtr hConsole, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex, Queue<EventData> expected, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiexOriginal);


        void RunTest(AccessibilityTest test)
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (WinEventSystem sys = app.AttachWinEventSystem(this))
                    {
                        using (ViewportArea area = new ViewportArea(app))
                        {
                            Globals.WaitForTimeout(); // wait for everything to settle with winevents
                            IntPtr hConsole = app.GetStdOutHandle();

                            // Prep structures to hold cursor and size of buffer.
                            WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex = new WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX();
                            sbiex.cbSize = (uint)Marshal.SizeOf(sbiex);

                            // this is where we will hold our expected messages
                            Queue<EventData> expected = new Queue<EventData>();

                            NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Get initial console data.");
                            WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiexOriginal = sbiex; // keep a copy of the original data for later.

                            // Run the test
                            test(app, area, hConsole, sbiex, expected, sbiexOriginal);
                        }
                    }
                }
            }
        }

        [TestMethod]
        public void TestSelection()
        {
            RunTest(TestSelectionImpl);  
        }

        private void TestSelectionImpl(CmdApp app, ViewportArea area, IntPtr hConsole, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex, Queue<EventData> expected, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiexOriginal)
        {
            // A. Test single area click
            {
                // Move mouse pointer to where the cursor is
                NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Update console data.");
                Point pt = new Point(sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y);
                area.MouseMove(pt);

                // Click on this area.
                expected.Enqueue(new EventData(EventType.CaretSelection, sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y));
                area.MouseDown();
                area.MouseUp();

                Globals.WaitForTimeout();

                VerifyQueue(expected);

                // We may receive more than one caret and that's OK. Clear it out.
                this.received.Clear();

                // End selection with escape
                app.UIRoot.SendKeys(Keys.Escape);
                Globals.WaitForTimeout();

                // Expect to see the caret again after leaving selection mode
                expected.Enqueue(new EventData(EventType.CaretVisible, sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y));
                VerifyQueue(expected);
            }

            // B. Drag area click
            {
                // Move mouse pointer to where the cursor is
                NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Update console data.");
                Point pt = new Point(sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y);
                area.MouseMove(pt);

                // Click on this area.
                expected.Enqueue(new EventData(EventType.CaretSelection, sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y));
                area.MouseDown();

                Globals.WaitForTimeout();

                Point ptDrag = pt;
                // Drag down and right for "some" distance. 10 isn't for a specific reason, it's just "some".
                ptDrag.X += 10;
                ptDrag.Y += 10;

                area.MouseMove(ptDrag);

                Globals.WaitForTimeout();

                area.MouseUp();

                Globals.WaitForTimeout();

                // Verify that the first one in the queue starts with where we put the mouse down.
                VerifyQueue(expected);

                // Now we have to take the final message in the queue and make sure it is where we released the mouse
                EventData expectedLast = new EventData(EventType.CaretSelection, ptDrag.X, ptDrag.Y);
                EventData actualLast = received.Last();
                Verify.AreEqual(expectedLast, actualLast);

                // Empty the received queue.
                received.Clear();

                // End selection with escape
                app.UIRoot.SendKeys(Keys.Escape);
                Globals.WaitForTimeout();

                // Expect to see the caret again after leaving selection mode
                expected.Enqueue(new EventData(EventType.CaretVisible, sbiex.dwCursorPosition.X, sbiex.dwCursorPosition.Y));
                VerifyQueue(expected);
            }
        }

        [TestMethod]
        public void TestLaunchAndExitChild()
        {
            RunTest(TestLaunchAndExitChildImpl);   
        }

        private void TestLaunchAndExitChildImpl(CmdApp app, ViewportArea area, IntPtr hConsole, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex, Queue<EventData> expected, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiexOriginal)
        {
            // A. We're going to type "cmd" into the prompt to start a command prompt.
            {
                Log.Comment("Type 'cmd' to get ready to start nested prompt.");
                NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Update console data.");
                TestTypeStringHelper("cmd", app, sbiex);
            }

            // B. Now we're going to press enter to launch the CMD application
            {
                Log.Comment("Press enter to launch and observe launch events.");
                NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Update console data.");
                expected.Enqueue(new EventData(EventType.StartApplication));
                expected.Enqueue(new EventData(EventType.UpdateRegion, 0, 0, sbiex.dwSize.X - 1, sbiex.dwSize.Y - 1));
                sbiex.dwCursorPosition.Y++;
                expected.Enqueue(new EventData(EventType.UpdateRegion, 0, sbiex.dwCursorPosition.Y, "Microsoft Windows [Version 10.0.14974.1001]".Length - 1, sbiex.dwCursorPosition.Y));
                sbiex.dwCursorPosition.Y++;
                expected.Enqueue(new EventData(EventType.UpdateRegion, 0, sbiex.dwCursorPosition.Y, "(c) 2016 Microsoft Corporation. All rights reserved.".Length - 1, sbiex.dwCursorPosition.Y));
                sbiex.dwCursorPosition.Y++;
                sbiex.dwCursorPosition.Y++;
                expected.Enqueue(new EventData(EventType.UpdateRegion, 0, sbiex.dwCursorPosition.Y, sbiexOriginal.dwCursorPosition.X - 1, sbiex.dwCursorPosition.Y));
                expected.Enqueue(new EventData(EventType.CaretVisible, sbiexOriginal.dwCursorPosition.X, sbiex.dwCursorPosition.Y));

                app.UIRoot.SendKeys(Keys.Enter);
                Globals.WaitForTimeout();
                Globals.WaitForTimeout();
                VerifyQueue(expected);
            }

            // C. Now we're going to type exit to leave the nested CMD application
            {
                Log.Comment("Type 'exit' to get ready to exit nested prompt.");
                NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Update console data.");
                TestTypeStringHelper("exit", app, sbiex);
            }

            // D. Now we're going to press enter to exit the CMD application
            {
                Log.Comment("Press enter to launch and observe exit events.");
                NativeMethods.Win32BoolHelper(WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref sbiex), "Update console data.");
                expected.Enqueue(new EventData(EventType.EndApplication));
                sbiex.dwCursorPosition.Y++;
                sbiex.dwCursorPosition.Y++;
                expected.Enqueue(new EventData(EventType.UpdateRegion, 0, sbiex.dwCursorPosition.Y, sbiexOriginal.dwCursorPosition.X - 1, sbiex.dwCursorPosition.Y));
                expected.Enqueue(new EventData(EventType.CaretVisible, sbiexOriginal.dwCursorPosition.X, sbiex.dwCursorPosition.Y));

                app.UIRoot.SendKeys(Keys.Enter);
                Globals.WaitForTimeout();
                Globals.WaitForTimeout();
                VerifyQueue(expected);
            }
        }

        [TestMethod]
        public void TestScrollByWheel()
        {
            RunTest(TestScrollByWheelImpl);
        }
        
        private void TestScrollByWheelImpl(CmdApp app, ViewportArea area, IntPtr hConsole, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex, Queue<EventData> expected, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiexOriginal)
        {
            int rowsPerScroll = app.GetRowsPerScroll();
            int scrollDelta; 

            // A. Scroll down.
            {
                scrollDelta = -1;
                expected.Enqueue(new EventData(EventType.UpdateScroll, 0, scrollDelta * rowsPerScroll));
                expected.Enqueue(new EventData(EventType.Layout));

                app.ScrollWindow(scrollDelta);

                Globals.WaitForTimeout();
                VerifyQueue(expected);
            }

            // B. Scroll up.
            {
                scrollDelta = 1;
                expected.Enqueue(new EventData(EventType.UpdateScroll, 0, scrollDelta * rowsPerScroll));
                expected.Enqueue(new EventData(EventType.Layout));

                app.ScrollWindow(scrollDelta);

                Globals.WaitForTimeout();
                VerifyQueue(expected);
            }
        }

        [TestMethod]
        public void TestScrollByOverflow()
        {
            RunTest(TestScrollByOverflowImpl);
        }

        private void TestScrollByOverflowImpl(CmdApp app, ViewportArea area, IntPtr hConsole, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiex, Queue<EventData> expected, WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX sbiexOriginal)
        {
            // Get original screen information
            sbiexOriginal = app.GetScreenBufferInfo();
            short promptLineEnd = sbiexOriginal.dwCursorPosition.X;
            promptLineEnd--; // prompt line ended one position left of cursor

            // Resize the window to only have two lines left at the bottom to test overflow when we echo some text
            sbiex = sbiexOriginal;
            sbiex.srWindow.Bottom = sbiex.dwCursorPosition.Y;
            sbiex.srWindow.Bottom += 3;
            app.SetScreenBufferInfo(sbiex);

            string echoText = "foo";
            string echoCommand = "echo";

            int echoLine = sbiexOriginal.dwCursorPosition.Y + 1;
            expected.Enqueue(new EventData(EventType.UpdateRegion, 0, echoLine, echoText.Length - 1, echoLine));
            expected.Enqueue(new EventData(EventType.UpdateScroll, 0, -1));
            int newPromptLine = echoLine + 2;
            expected.Enqueue(new EventData(EventType.UpdateRegion, 0, newPromptLine, promptLineEnd, newPromptLine));
            expected.Enqueue(new EventData(EventType.Layout));
            expected.Enqueue(new EventData(EventType.CaretVisible, promptLineEnd + 1, newPromptLine));

            // type command to echo foo and press enter
            app.UIRoot.SendKeys($"{echoCommand} {echoText}");
            Globals.WaitForTimeout();
            received.Clear();
            app.UIRoot.SendKeys(Keys.Enter);
            Globals.WaitForTimeout();

            VerifyQueue(expected);
        }
    }
}
