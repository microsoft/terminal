//----------------------------------------------------------------------------------------------------------------------
// <copyright file="SmokeTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for verifying the basic opening/closing of the Terminal.</summary>
//----------------------------------------------------------------------------------------------------------------------

namespace WindowsTerminal.UIA.Tests
{
    using WEX.TestExecution;
    using OpenQA.Selenium;
    using WEX.TestExecution.Markup;

    using WindowsTerminal.UIA.Tests.Common;
    using WindowsTerminal.UIA.Tests.Elements;

    [TestClass]
    public class SmokeTests
    {
        public TestContext TestContext { get; set; }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void StartTerminal()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();
                root.SendKeys("Hello smoke test!");
                Globals.WaitForLongTimeout();
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunBigTextPowershell()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();
                var contentPath = app.GetFullTestContentPath("big.txt");
                root.SendKeys($"cat \"{contentPath}\"");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(25000);
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunBigTextPowershellBulk ()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();
                var contentPath = app.GetFullTestContentPath("big.txt");
                root.SendKeys($"Get-Content -ReadCount 0 \"{contentPath}\" | Out-Default");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(25000);
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunBigTextCmd()
        {
            using (TerminalApp app = new TerminalApp(TestContext, "cmd.exe"))
            {
                var root = app.GetRoot();
                var contentPath = app.GetFullTestContentPath("big.txt");
                root.SendKeys($"type \"{contentPath}\"");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(25000);
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunCmatrixCmd()
        {
            using (TerminalApp app = new TerminalApp(TestContext, "cmd.exe"))
            {
                var root = app.GetRoot();
                root.SendKeys("chcp 65001" + Keys.Enter); // This output needs UTF-8
                var contentPath = app.GetFullTestContentPath("cmatrix.txt");
                root.SendKeys($"type \"{contentPath}\"");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(10000);
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunCacafireCmd()
        {
            using (TerminalApp app = new TerminalApp(TestContext, "cmd.exe"))
            {
                var root = app.GetRoot();
                root.SendKeys("chcp 65001" + Keys.Enter); // This output needs UTF-8
                var contentPath = app.GetFullTestContentPath("cacafire.txt");
                root.SendKeys($"type \"{contentPath}\"");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(25000);
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunChafaCmd()
        {
            using (TerminalApp app = new TerminalApp(TestContext, "cmd.exe"))
            {
                var root = app.GetRoot();
                root.SendKeys("chcp 65001" + Keys.Enter); // This output needs UTF-8
                var contentPath = app.GetFullTestContentPath("chafa.txt");
                root.SendKeys($"type \"{contentPath}\"");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(10000);
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunMakeKillPanes()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "+");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "+");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "-");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "-");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();

                Globals.WaitForLongTimeout();
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunMakeKillTabs()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftControl + Keys.LeftShift + "W");
                Globals.WaitForTimeout();

                Globals.WaitForLongTimeout();
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void RunOpenSettingsUI()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                root.SendKeys(Keys.LeftControl + ",");
                Globals.WaitForTimeout();

                Globals.WaitForLongTimeout();
            }
        }

        [TestMethod]
        [TestProperty("IsPGO", "true")]
        public void DragMultipleTabsAcrossWindows()
        {
            const string detachedTabTitle = "DragTabA";
            const string firstAttachedTabTitle = "DragTabB";
            const string secondAttachedTabTitle = "DragTabC";
            const string rootTitle = "WindowsTerminal.UIA.Tests";

            var launchArgs =
                $"new-tab --title \"{detachedTabTitle}\" --suppressApplicationTitle \"powershell.exe\" ; " +
                $"new-tab --title \"{firstAttachedTabTitle}\" --suppressApplicationTitle \"powershell.exe\" ; " +
                $"new-tab --title \"{secondAttachedTabTitle}\" --suppressApplicationTitle \"powershell.exe\" ; " +
                $"new-tab --title \"{rootTitle}\" --suppressApplicationTitle \"powershell.exe\"";

            using (TerminalApp app = new TerminalApp(TestContext, launchArgs: launchArgs, windowTitleToFind: rootTitle))
            {
                var windowA = app.WaitForTopLevelWindowByName(rootTitle);
                app.ArrangeWindowOnPrimaryMonitor(windowA, 0);
                app.ActivateWindow(windowA);
                var firstTab = app.FindTabElementByName(windowA, detachedTabTitle);
                app.LogWindowDetails("initialWindowA", windowA);
                app.LogElementDetails("firstTab", windowA, firstTab);

                app.DragByOffset(firstTab, windowA.Width + 80, 80);
                Globals.WaitForLongTimeout();

                var windowB = app.TryWaitForTopLevelWindowByName(detachedTabTitle, windowA, 5000) ??
                              app.WaitForTopLevelWindowByName(rootTitle, windowA);
                Verify.IsNotNull(windowB, "Expected dragging a tab outside the window to create a new top-level window.");
                Verify.IsTrue(windowA.Handle != windowB.Handle, "Expected dragging a tab outside the window to create a distinct top-level window.");
                Verify.IsTrue(app.HasElementByName(windowB, detachedTabTitle), "Expected the detached window to contain the dragged tab.");

                windowA = app.WaitForTopLevelWindowByName(rootTitle, windowB);
                app.ArrangeWindowOnPrimaryMonitor(windowA, 0);
                app.ArrangeWindowOnPrimaryMonitor(windowB, 1);
                app.ActivateWindow(windowA);

                var secondTab = app.FindTabElementByName(windowA, firstAttachedTabTitle);
                var thirdTab = app.FindTabElementByName(windowA, secondAttachedTabTitle);

                app.LogWindowDetails("windowA", windowA);
                app.LogWindowDetails("windowB", windowB);
                app.LogElementDetails("secondTab", windowA, secondTab);
                app.LogElementDetails("thirdTab", windowA, thirdTab);
                app.LogElementAncestors("secondTabAncestors", secondTab);
                app.LogElementAncestors("thirdTabAncestors", thirdTab);

                app.SelectTabRangeForTesting(windowA, 0, 1);
                Globals.WaitForTimeout();
                secondTab = app.FindTabElementByName(windowA, firstAttachedTabTitle);

                var existingWindowTab = app.FindTabElementByName(windowB, detachedTabTitle);
                app.LogElementDetails("existingWindowTab", windowB, existingWindowTab);
                app.LogElementAncestors("existingWindowTabAncestors", existingWindowTab);
                app.ActivateWindow(windowA);
                var dropOffset = System.Math.Max(24, existingWindowTab.Size.Width / 3);
                app.DragToElement(secondTab, existingWindowTab, dropOffset, 0);
                Globals.WaitForLongTimeout();

                Verify.IsNull(
                    app.TryWaitForTopLevelWindowByName(firstAttachedTabTitle, windowB, 1500),
                    "Expected dropping DragTabB + DragTabC onto DragTabA to move the tabs into the existing window, not create a new top-level window.");

                Verify.IsTrue(app.HasElementByName(windowB, detachedTabTitle), "Expected the destination window to keep its original tab.");
                Verify.IsTrue(app.HasElementByName(windowB, firstAttachedTabTitle), "Expected the destination window to receive the first dragged tab.");
                Verify.IsTrue(app.HasElementByName(windowB, secondAttachedTabTitle), "Expected the destination window to receive the second dragged tab.");

                var tabAInWindowB = app.FindTabElementByName(windowB, detachedTabTitle);
                var tabBInWindowB = app.FindTabElementByName(windowB, firstAttachedTabTitle);
                var tabCInWindowB = app.FindTabElementByName(windowB, secondAttachedTabTitle);

                tabCInWindowB.Click();
                Globals.WaitForTimeout();

                Verify.IsTrue(tabAInWindowB.Location.X < tabBInWindowB.Location.X, "Expected dragged tabs to be inserted after the drop target.");
                Verify.IsTrue(tabBInWindowB.Location.X < tabCInWindowB.Location.X, "Expected dragged tabs to preserve their source order.");

                windowA = app.WaitForTopLevelWindowByName(rootTitle, windowB);
                Verify.IsFalse(app.HasElementByName(windowA, firstAttachedTabTitle), "Expected the source window to no longer contain the first moved tab.");
                Verify.IsFalse(app.HasElementByName(windowA, secondAttachedTabTitle), "Expected the source window to no longer contain the second moved tab.");
            }
        }
    }
}
