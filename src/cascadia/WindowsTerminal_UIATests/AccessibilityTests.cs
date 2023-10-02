//----------------------------------------------------------------------------------------------------------------------
// <copyright file="AccessibilityTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for Axe.Windows to perform an accessibility pass on different scenarios.</summary>
//----------------------------------------------------------------------------------------------------------------------

namespace WindowsTerminal.UIA.Tests
{
    using Axe.Windows.Automation;
    using OpenQA.Selenium;
    using OpenQA.Selenium.Appium;
    using System.Linq;
    using WEX.TestExecution.Markup;
    using WEX.Logging.Interop;

    using WindowsTerminal.UIA.Tests.Common;
    using WindowsTerminal.UIA.Tests.Elements;
    using WEX.TestExecution;
    using OpenQA.Selenium.Interactions;
    using System.Runtime.CompilerServices;

    [TestClass]
    public class AccessibilityTests
    {
        public TestContext TestContext { get; set; }

        private AppiumWebElement OpenSUIPage(TerminalApp app, string elementName)
        {
            var root = app.GetRoot();

            root.SendKeys(Keys.LeftControl + ",");
            Globals.WaitForTimeout();

            // Navigate to the settings page
            root.FindElementByName(elementName).Click();
            Globals.WaitForTimeout();

            return root;
        }

        private AppiumWebElement OpenTabContextMenu(TerminalApp app)
        {
            var root = app.GetRoot();

            // Open tab context menu
            var elem = root.FindElementByClassName("ListViewItem");
            new Actions(app.Session).ContextClick(elem).Perform();
            Globals.WaitForTimeout();

            return root;
        }


        private IScanner BuildScanner(int processId)
        {
            // Output Directory: bin\x64\Debug\AxeWindowsOutputFiles
            var builder = Config.Builder.ForProcessId(processId);
            builder.WithOutputFileFormat(OutputFileFormat.A11yTest);
            builder.WithOutputDirectory(TestContext.TestRunDirectory);
            var config = builder.Build();
            return ScannerFactory.CreateScanner(config);
        }

        private int ScanForErrors(TerminalApp app, IScanner scanner = null)
        {
            // Use the provided scanner if one is provided
            scanner = scanner == null ? BuildScanner(app.ProcessId) : scanner;

            var scanOutput = scanner.Scan(null);
            var errorCount = scanOutput.WindowScanOutputs.First().ErrorCount;
            Log.Comment($"Errors Found: {errorCount}");

            return errorCount;
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIStartup()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                OpenSUIPage(app, "Startup");
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIInteraction()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                OpenSUIPage(app, "Interaction");
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIAppearance()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                OpenSUIPage(app, "Appearance");
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIColorSchemes()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                int totalErrorCount = 0;
                var root = OpenSUIPage(app, "Color Schemes");

                var scanner = BuildScanner(app.ProcessId);
                totalErrorCount += ScanForErrors(app, scanner);

                // Navigate to the Campbell scheme and scan it
                root.FindElementByName("Campbell (default)").Click();
                Globals.WaitForTimeout();
                totalErrorCount += ScanForErrors(app, scanner);

                Globals.WaitForLongTimeout();
                Verify.AreEqual(totalErrorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIRendering()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                OpenSUIPage(app, "Rendering");
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIActions()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                OpenSUIPage(app, "Actions");
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIBaseLayer()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                int totalErrorCount = 0;
                var root = OpenSUIPage(app, "Defaults");

                var scanner = BuildScanner(app.ProcessId);
                totalErrorCount += ScanForErrors(app, scanner);

                // Navigate to the Appearance page and scan it
                root.FindElementByName("Appearance").Click();
                Globals.WaitForTimeout();
                totalErrorCount += ScanForErrors(app, scanner);

                // Navigate back to the Advanced page and scan it
                root.FindElementByName("Defaults").Click();
                Globals.WaitForTimeout();
                root.FindElementByName("Advanced").Click();
                Globals.WaitForTimeout();
                totalErrorCount += ScanForErrors(app, scanner);

                // Error expected: default icon is a unicode symbol that is not readable by a screen reader
                Verify.AreEqual(totalErrorCount, 1);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunSUIAddProfile()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                OpenSUIPage(app, "Add a new profile");
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunNewTabFlyout()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open dropdown
                root.SendKeys(Keys.Control + Keys.Shift + Keys.Space);
                Globals.WaitForTimeout();

                // Error expected: the flyout menu itself has no name
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 1);
            }
        }

        public void RunAboutMenu()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open dropdown
                root.FindElementByName("More Options").Click();
                Globals.WaitForTimeout();

                // Open About menu
                root.FindElementByName("About").Click();
                Globals.WaitForTimeout();

                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunCommandPalette()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open Command Palette
                root.SendKeys(Keys.LeftControl + Keys.Shift + "P");

                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunTabContextMenu()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open a new tab so that the "close" nested menu items are enabled
                root.SendKeys(Keys.Control + Keys.Shift + "T");

                root = OpenTabContextMenu(app);

                // Expand the "Close..." nested menu
                root.FindElementByName("Close...").Click();
                Globals.WaitForTimeout();

                // Errors expected: the flyout menu itself (and the nested menu) has no name
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 2);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunColorTabMenu()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open color tab menu
                root = OpenTabContextMenu(app);
                root.FindElementByName("Color...").Click();
                Globals.WaitForTimeout();

                // Open custom sub menu
                root.FindElementByName("Custom...").Click();
                Globals.WaitForTimeout();

                // Expand more options in Custom sub menu
                root.FindElementByName("More").Click();
                Globals.WaitForTimeout();

                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunRenameTab()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Begin renaming tab
                root = OpenTabContextMenu(app);
                root.FindElementByName("Rename Tab").Click();
                Globals.WaitForTimeout();

                // Error expected: the text box itself has no name
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 1);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunFind()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open search box
                root.SendKeys(Keys.Control + Keys.Shift + "F");
                Globals.WaitForTimeout();

                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }

        [TestMethod]
        [TestProperty("A11yInsights", "true")]
        public void RunTerminalContextMenu()
        {
            using (TerminalApp app = new TerminalApp(TestContext))
            {
                var root = app.GetRoot();

                // Open command palette and find the context menu action
                root.SendKeys(Keys.Control + Keys.Shift + "P");
                root.SendKeys("Show context menu");
                root.FindElementByName("Show context menu").Click();
                Globals.WaitForTimeout();
                
                var errorCount = ScanForErrors(app);
                Verify.AreEqual(errorCount, 0);
            }
        }
    }
}
