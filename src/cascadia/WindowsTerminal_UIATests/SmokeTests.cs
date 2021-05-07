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
        public void RunBigText()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();
                // Relative to the test deployment root, this is where the content file is stored.
                // Override necessary to run locally
                root.SendKeys(@"cat content\big.txt");
                root.SendKeys(Keys.Enter);
                System.Threading.Thread.Sleep(25000);
            }
        }
    }
}
