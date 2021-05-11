//----------------------------------------------------------------------------------------------------------------------
// <copyright file="SmokeTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for verifying the basic opening/closing of the Terminal.</summary>
//----------------------------------------------------------------------------------------------------------------------

namespace WindowsTerminal.UIA.Tests
{
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

                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "T");
                Globals.WaitForTimeout();
                root.SendKeys(Keys.LeftAlt + Keys.LeftShift + "T");
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
    }
}
