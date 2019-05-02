//----------------------------------------------------------------------------------------------------------------------
// <copyright file="AccessibilityTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for the certain key presses.</summary>
//----------------------------------------------------------------------------------------------------------------------
using System;
using System.Windows;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Automation;
using System.Windows.Automation.Text;

using WEX.TestExecution;
using WEX.TestExecution.Markup;
using WEX.Logging.Interop;

using Conhost.UIA.Tests.Common;
using Conhost.UIA.Tests.Common.NativeMethods;
using Conhost.UIA.Tests.Elements;
using OpenQA.Selenium;
using System.Drawing;

using System.Runtime.InteropServices;

namespace Conhost.UIA.Tests
{

    class InvalidElementException : Exception
    {

    }

    [TestClass]
    class AccessibilityTests
    {
        public TestContext TestContext { get; set; }

        private AutomationElement GetWindowUiaElement(CmdApp app)
        {
            IntPtr handle = app.GetWindowHandle();
            AutomationElement windowUiaElement = AutomationElement.FromHandle(handle);
            return windowUiaElement;
        }

        private AutomationElement GetTextAreaUiaElement(CmdApp app)
        {
            AutomationElement windowUiaElement = GetWindowUiaElement(app);
            AutomationElementCollection descendants = windowUiaElement.FindAll(TreeScope.Descendants, Condition.TrueCondition);
            for (int i = 0; i < descendants.Count; ++i)
            {
                AutomationElement poss = descendants[i];
                if (poss.Current.AutomationId.Equals("Text Area"))
                {
                    return poss;
                }
            }
            throw new InvalidElementException();
        }

        private int _GetTotalRows(CmdApp app)
        {
            WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX screenBufferInfo = app.GetScreenBufferInfo();
            return screenBufferInfo.dwSize.Y;
        }

        private void _ClearScreenBuffer(CmdApp app)
        {
            IntPtr outHandle = app.GetStdOutHandle();
            WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX screenInfo = new WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX();
            screenInfo.cbSize = (uint)Marshal.SizeOf(screenInfo);
            WinCon.GetConsoleScreenBufferInfoEx(outHandle, ref screenInfo);
            int charCount = screenInfo.dwSize.X * screenInfo.dwSize.Y;
            string writeString = new string(' ', charCount);
            WinCon.COORD coord = new WinCon.COORD();
            coord.X = 0;
            coord.Y = 0;
            UInt32 charsWritten = 0;
            WinCon.WriteConsoleOutputCharacter(outHandle,
                                               writeString,
                                               (uint)charCount,
                                               coord,
                                               ref charsWritten);
            Verify.AreEqual((UInt32)charCount, charsWritten);
        }

        private void _WriteCharTestText(CmdApp app)
        {
            IntPtr outHandle = app.GetStdOutHandle();
            WinCon.COORD coord = new WinCon.COORD();
            coord.X = 0;
            coord.Y = 0;
            string row1 = "1234567890";
            string row2 = "   abcdefghijk";
            UInt32 charsWritten = 0;
            WinCon.WriteConsoleOutputCharacter(outHandle,
                                               row1,
                                               (uint)row1.Length,
                                               coord,
                                               ref charsWritten);

            coord.Y = 1;
            WinCon.WriteConsoleOutputCharacter(outHandle,
                                               row2,
                                               (uint)row2.Length,
                                               coord,
                                               ref charsWritten);

        }

        private void _FillOutputBufferWithData(CmdApp app)
        {
            for (int i = 0; i < _GetTotalRows(app) * 2 / 3; ++i)
            {
                // each echo command uses up 3 lines in the buffer:
                // 1. output text
                // 2. newline
                // 3. new prompt line
                app.UIRoot.SendKeys("echo ");
                app.UIRoot.SendKeys(i.ToString());
                app.UIRoot.SendKeys(Keys.Enter);
            }
        }

        [TestMethod]
        public void CanAccessAccessibilityTree()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement windowUiaElement = GetWindowUiaElement(app);
                Verify.IsTrue(windowUiaElement.Current.AutomationId.Equals("Console Window"));
            }
        }

        [TestMethod]
        public void CanAccessTextAreaUiaElement()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                Verify.IsTrue(textAreaUiaElement != null);
            }
        }

        [TestMethod]
        public void CanGetDocumentRangeText()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                // get the text from uia api
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange documentRange = textPattern.DocumentRange;
                string allText = documentRange.GetText(-1);
                // get text from console api
                IntPtr hConsole = app.GetStdOutHandle();
                using (ViewportArea area = new ViewportArea(app))
                {
                    WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX screenInfo = app.GetScreenBufferInfo();
                    Rectangle rect = new Rectangle(0, 0, screenInfo.dwSize.X, screenInfo.dwSize.Y);
                    IEnumerable<string> viewportText = area.GetLinesInRectangle(hConsole, rect);

                    // the uia api does not return spaces beyond the last
                    // non -whitespace character so we need to trim those from
                    // the viewportText. The uia api also inserts \r\n to indicate
                    // a new linen so we need to add those back in after trimming.
                    string consoleText = "";
                    for (int i = 0; i < viewportText.Count(); ++i)
                    {
                        consoleText += viewportText.ElementAt(i).Trim() + "\r\n";
                    }
                    consoleText = consoleText.Trim();
                    allText = allText.Trim();
                    // compare
                    Verify.IsTrue(consoleText.Equals(allText));
                }
            }
        }

        [TestMethod]
        public void CanGetTextAtCharacterLevel()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                const int noMaxLength = -1;
                const string row1 = "1234567890";
                const string row2 = "   abcdefghijk";
                _ClearScreenBuffer(app);
                _WriteCharTestText(app);
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] ranges = textPattern.GetVisibleRanges();
                TextPatternRange range = ranges[0].Clone();

                // should be able to get each char in row1
                range.ExpandToEnclosingUnit(TextUnit.Character);
                foreach (char ch in row1)
                {
                    string text = range.GetText(noMaxLength);
                    Verify.AreEqual(ch.ToString(), text);
                    range.Move(TextUnit.Character, 1);
                }

                // should be able to get each char in row2, including starting spaces
                range = ranges[1].Clone();
                range.ExpandToEnclosingUnit(TextUnit.Character);
                foreach (char ch in row2)
                {
                    string text = range.GetText(noMaxLength);
                    Verify.AreEqual(ch.ToString(), text);
                    range.Move(TextUnit.Character, 1);
                }

                // taking half of each row should return correct text with
                // spaces if they appear before the last non-whitespace char
                range = ranges[0].Clone();
                range.MoveEndpointByUnit(TextPatternRangeEndpoint.Start, TextUnit.Character, 8);
                range.MoveEndpointByUnit(TextPatternRangeEndpoint.End, TextUnit.Character, 8);
                Verify.AreEqual("90\r\n   abcde", range.GetText(noMaxLength));
            }
        }

        [TestMethod]
        public void CanGetVisibleRange()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                // get the ranges from uia api
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] ranges = textPattern.GetVisibleRanges();

                // get the ranges from the console api
                WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX screenInfo = app.GetScreenBufferInfo();
                int viewportHeight = screenInfo.srWindow.Bottom - screenInfo.srWindow.Top + 1;

                // we should have one range per line in the viewport
                Verify.AreEqual(ranges.GetLength(0), viewportHeight);

                // each line should have the same text
                ViewportArea viewport = new ViewportArea(app);
                IntPtr hConsole = app.GetStdOutHandle();
                for (int i = 0; i < viewportHeight; ++i)
                {
                    Rectangle rect = new Rectangle(0, i, screenInfo.dwSize.X, 1);
                    IEnumerable<string> text = viewport.GetLinesInRectangle(hConsole, rect);
                    Verify.AreEqual(text.ElementAt(0).Trim(), ranges[i].GetText(-1).Trim());
                }
            }
        }

        /*
        // TODO this is commented out because it will fail. It fails because the c# api of RangeFromPoint
        // throws an exception when passed a point that is outside the bounds of the window, which is
        // allowed in the c++ version and exactly what we want to test. Another way to test this case needs
        // to be found that doesn't go through the c# api.
        [TestMethod]
        public void CanGetRangeFromPoint()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    // RangeFromPoint returns the range closest to the point provided
                    // so we have three cases along the y dimension:
                    // - point above the console window
                    // - point in the console window
                    // - point below the console window

                    // get the window dimensions and pick the point locations
                    IntPtr handle = app.GetWindowHandle();
                    User32.RECT windowRect;
                    User32.GetWindowRect(handle, out windowRect);

                    List<System.Windows.Point> points = new List<System.Windows.Point>();
                    int middleOfWindow = (windowRect.bottom + windowRect.top) / 2;
                    const int windowEdgeOffset = 10;
                    // x doesn't matter until we support more ranges than lines
                    points.Add(new System.Windows.Point(windowRect.left, windowRect.top - windowEdgeOffset));
                    points.Add(new System.Windows.Point(windowRect.left, middleOfWindow));
                    points.Add(new System.Windows.Point(windowRect.left, windowRect.bottom + windowEdgeOffset));


                    // get the ranges from uia api
                    AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                    TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                    List<TextPatternRange> textPatternRanges = new List<TextPatternRange>();
                    foreach (System.Windows.Point p in points)
                    {
                        textPatternRanges.Add(textPattern.RangeFromPoint(p));
                    }

                    // ranges should be in correct order (starting at top and
                    // going down screen)
                    Rect lastBoundingRect = textPatternRanges.ElementAt(0).GetBoundingRectangles()[0];
                    foreach (TextPatternRange range in textPatternRanges)
                    {
                        Rect[] boundingRects = range.GetBoundingRectangles();
                        // since the ranges returned by RangeFromPoint are supposed to be degenerate,
                        // there should be only one bounding rect per TextPatternRange
                        Verify.AreEqual(boundingRects.GetLength(0), 1);

                        Verify.IsTrue(boundingRects[0].Top >= lastBoundingRect.Top);
                        lastBoundingRect = boundingRects[0];
                    }


                }
            }

        }
        */

        [TestMethod]
        public void CanCloneTextRangeProvider()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange textPatternRange = textPattern.DocumentRange;
                // clone it
                TextPatternRange copyRange = textPatternRange.Clone();
                Verify.IsTrue(copyRange.Compare(textPatternRange));
                // change the copy and make sure the compare fails
                copyRange.MoveEndpointByRange(TextPatternRangeEndpoint.End, copyRange, TextPatternRangeEndpoint.Start);
                Verify.IsFalse(copyRange.Compare(textPatternRange));
            }
        }

        [TestMethod]
        public void CanCompareTextRangeProviderEndpoints()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange textPatternRange = textPattern.DocumentRange;
                // comparing an endpoint to itself should be the same
                Verify.AreEqual(0, textPatternRange.CompareEndpoints(TextPatternRangeEndpoint.Start,
                                                                        textPatternRange,
                                                                        TextPatternRangeEndpoint.Start));
                // comparing an earlier endpoint to a later one should be negative
                Verify.IsGreaterThan(0, textPatternRange.CompareEndpoints(TextPatternRangeEndpoint.Start,
                                                                            textPatternRange,
                                                                            TextPatternRangeEndpoint.End));
                // comparing a later endpoint to an earlier one should be positive
                Verify.IsLessThan(0, textPatternRange.CompareEndpoints(TextPatternRangeEndpoint.End,
                                                                        textPatternRange,
                                                                        TextPatternRangeEndpoint.Start));
            }
        }

        [TestMethod]
        public void CanExpandToEnclosingUnitTextRangeProvider()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] visibleRanges = textPattern.GetVisibleRanges();
                TextPatternRange testRange = visibleRanges.First().Clone();

                // change testRange to a degenerate range and then expand to a line
                testRange.MoveEndpointByRange(TextPatternRangeEndpoint.End, testRange, TextPatternRangeEndpoint.Start);
                Verify.AreEqual(0, testRange.CompareEndpoints(TextPatternRangeEndpoint.Start,
                                                              testRange,
                                                              TextPatternRangeEndpoint.End));
                testRange.ExpandToEnclosingUnit(TextUnit.Line);
                Verify.IsTrue(testRange.Compare(visibleRanges[0]));

                // expand to document size
                testRange.ExpandToEnclosingUnit(TextUnit.Document);
                Verify.IsTrue(testRange.Compare(textPattern.DocumentRange));

                // shrink back to a line
                testRange.ExpandToEnclosingUnit(TextUnit.Line);
                Verify.IsTrue(testRange.Compare(visibleRanges[0]));

                // make the text buffer start to cycle its buffer
                _FillOutputBufferWithData(app);

                // expand to document range again
                testRange.ExpandToEnclosingUnit(TextUnit.Document);
                Verify.IsTrue(testRange.Compare(textPattern.DocumentRange));
            }
        }

        [TestMethod]
        public void CanMoveRange()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] visibleRanges = textPattern.GetVisibleRanges();
                TextPatternRange testRange = visibleRanges.First().Clone();


                // assumes range is at the top of the screen buffer
                Action<TextPatternRange> testMovement = delegate (TextPatternRange range)
                {
                    // the range is at the top of the screen
                    // buffer, we shouldn't be able to move up.
                    int moveAmount = range.Move(TextUnit.Line, -1);
                    Verify.AreEqual(0, moveAmount);

                    // move to the bottom of the screen
                    // - 1 because we're already on the 0th row
                    int rowsToMove = _GetTotalRows(app) - 1;
                    moveAmount = range.Move(TextUnit.Line, rowsToMove);
                    Verify.AreEqual(rowsToMove, moveAmount);

                    // try to move one more row down, we should not be able to
                    moveAmount = range.Move(TextUnit.Line, 1);
                    Verify.AreEqual(0, moveAmount);

                    // move the range up to the top again, one row at a time,
                    // making sure that we have only one line being encompassed
                    // by the range. We check this by counting the number of
                    // bounding rectangles that represent the range.
                    for (int i = 0; i < rowsToMove; ++i)
                    {
                        moveAmount = range.Move(TextUnit.Line, -1);
                        // we need to scroll into view or getting the boundary
                        // rectangles might return 0
                        Verify.AreEqual(-1, moveAmount);
                        range.ScrollIntoView(true);
                        Rect[] boundingRects = range.GetBoundingRectangles();
                        Verify.AreEqual(1, boundingRects.GetLength(0));
                    }

                    // and back down to the bottom, one row at a time
                    for (int i = 0; i < rowsToMove; ++i)
                    {
                        moveAmount = range.Move(TextUnit.Line, 1);
                        // we need to scroll into view or getting the boundary
                        // rectangles might return 0
                        Verify.AreEqual(1, moveAmount);
                        range.ScrollIntoView(true);
                        Rect[] boundingRects = range.GetBoundingRectangles();
                        Verify.AreEqual(1, boundingRects.GetLength(0));
                    }

                };

                testMovement(testRange);

                // test again with unaligned text buffer and screen buffer
                _FillOutputBufferWithData(app);
                Globals.WaitForTimeout();

                visibleRanges = textPattern.GetVisibleRanges();
                testRange = visibleRanges.First().Clone();
                // move range back to the top
                while (true)
                {
                    int moveCount = testRange.Move(TextUnit.Line, -1);
                    if (moveCount == 0)
                    {
                        break;
                    }
                }

                testMovement(testRange);
            }
        }

        [TestMethod]
        public void CanMoveEndpointByUnitNearTopBoundary()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] visibleRanges = textPattern.GetVisibleRanges();
                TextPatternRange testRange = visibleRanges.First().Clone();

                // assumes that range is a line range at the top of the screen buffer
                Action<TextPatternRange> testTopBoundary = delegate(TextPatternRange range)
                {
                    // the first visible range is at the top of the screen
                    // buffer, we shouldn't be able to move the starting endpoint up
                    int moveAmount = range.MoveEndpointByUnit(TextPatternRangeEndpoint.Start, TextUnit.Line, -1);
                    Verify.AreEqual(0, moveAmount);

                    // we should be able to move the ending endpoint back, creating a degenerate range
                    moveAmount = range.MoveEndpointByUnit(TextPatternRangeEndpoint.End, TextUnit.Line, -1);
                    Verify.AreEqual(-1, moveAmount);

                    // the range should now be degenerate and the ending
                    // endpoint should not be able to be moved back again
                    string rangeText = range.GetText(-1);
                    Verify.AreEqual("", rangeText);
                    moveAmount = range.MoveEndpointByUnit(TextPatternRangeEndpoint.End, TextUnit.Line, -1);
                    Verify.AreEqual(-1, moveAmount);
                };

                testTopBoundary(testRange);

                // we want to test that the boundaries are still observed
                // when the screen buffer index and text buffer index don't align.
                // write a bunch of text to the screen to fill up the text
                // buffer and make it start to reuse its buffer
                _FillOutputBufferWithData(app);
                Globals.WaitForTimeout();

                // move all the way to the bottom
                visibleRanges = textPattern.GetVisibleRanges();
                testRange = visibleRanges.Last().Clone();
                while (true)
                {
                    int moved = testRange.Move(TextUnit.Line, 1);
                    if (moved == 0)
                    {
                        break;
                    }
                }
                // we're at the bottom of the screen buffer, so move back to the top
                // so we can test
                int rowsToMove = -1 * (_GetTotalRows(app) - 1);
                int moveCount = testRange.Move(TextUnit.Line, rowsToMove);
                Verify.AreEqual(rowsToMove, moveCount);
                testRange.ScrollIntoView(true);

                testTopBoundary(testRange);
            }
        }

        [TestMethod]
        public void CanMoveEndpointByUnitNearBottomBoundary()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] visibleRanges = textPattern.GetVisibleRanges();
                TextPatternRange testRange = visibleRanges.First().Clone();

                // assumes that range is a line range at the bottom of the screen buffer
                Action<TextPatternRange> testBottomBoundary = delegate (TextPatternRange range)
                {
                    // the range is at the bottom of the screen buffer, we
                    // shouldn't be able to move the endpoint endpoint down
                    int moveAmount = range.MoveEndpointByUnit(TextPatternRangeEndpoint.End, TextUnit.Line, 1);
                    Verify.AreEqual(0, moveAmount);

                    // we shouldn't be able to move the starting endpoint down either
                    moveAmount = range.MoveEndpointByUnit(TextPatternRangeEndpoint.Start, TextUnit.Line, 1);
                    Verify.AreEqual(0, moveAmount);
                };

                // move the range to the bottom of the screen
                int rowsToMove = _GetTotalRows(app) - 1;
                int moveCount = testRange.Move(TextUnit.Line, rowsToMove);
                Verify.AreEqual(rowsToMove, moveCount);

                testBottomBoundary(testRange);

                // we want to test that the boundaries are still observed
                // when the screen buffer index and text buffer index don't align.
                // write a bunch of text to the screen to fill up the text
                // buffer and make it start to reuse its buffer
                _FillOutputBufferWithData(app);
                Globals.WaitForTimeout();

                // move all the way to the top
                visibleRanges = textPattern.GetVisibleRanges();
                testRange = visibleRanges.First().Clone();
                while (true)
                {
                    int moved = testRange.Move(TextUnit.Line, -1);
                    if (moved == 0)
                    {
                        break;
                    }
                }

                // we're at the top of the screen buffer, so move back to the bottom
                // so we can test
                rowsToMove = _GetTotalRows(app) - 1;
                moveCount = testRange.Move(TextUnit.Line, rowsToMove);
                Verify.AreEqual(rowsToMove, moveCount);
                testRange.ScrollIntoView(true);

                testBottomBoundary(testRange);
            }
        }

        [TestMethod]
        public void CanGetBoundingRectangles()
        {
            using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
            {
                AutomationElement textAreaUiaElement = GetTextAreaUiaElement(app);
                TextPattern textPattern = textAreaUiaElement.GetCurrentPattern(TextPattern.Pattern) as TextPattern;
                TextPatternRange[] visibleRanges = textPattern.GetVisibleRanges();
                // copy the first range
                TextPatternRange firstRange = visibleRanges[0].Clone();
                // only one bounding rect should be returned for the one line
                Rect[] boundingRects = firstRange.GetBoundingRectangles();
                Verify.AreEqual(1, boundingRects.GetLength(0));
                // expand to two lines, verify we get a bounding rect per line
                firstRange.MoveEndpointByRange(TextPatternRangeEndpoint.End, visibleRanges[1], TextPatternRangeEndpoint.End);
                boundingRects = firstRange.GetBoundingRectangles();
                Verify.AreEqual(2, boundingRects.GetLength(0));
            }
        }
    }
}
