//----------------------------------------------------------------------------------------------------------------------
// <copyright file="InputBufferTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for input buffer.</summary>
//----------------------------------------------------------------------------------------------------------------------
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;
using System.IO;

using WEX.TestExecution;
using WEX.TestExecution.Markup;
using WEX.Logging.Interop;

using Conhost.UIA.Tests.Common;
using Conhost.UIA.Tests.Common.NativeMethods;
using Conhost.UIA.Tests.Elements;
using OpenQA.Selenium;


namespace Conhost.UIA.Tests
{
    [TestClass]
    class InputBufferTests
    {
        public TestContext TestContext { get; set; }

        private bool IsProgramInPath(string name)
        {
            string[] pathDirs = Environment.GetEnvironmentVariable("PATH").Split(';');
            foreach (string path in pathDirs)
            {
                string joined = Path.Combine(path, name);
                if (File.Exists(joined))
                {
                    return true;
                }
            }
            return false;
        }

        [TestMethod]
        public void VerifyVimInput()
        {
            if (!IsProgramInPath("vim.exe"))
            {
                Log.Comment("vim can't be found in path, skipping test.");
                return;
            }
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (ViewportArea area = new ViewportArea(app))
                    {
                        IntPtr hConsole = app.GetStdOutHandle();
                        string testText = "hello world";
                        Verify.IsNotNull(hConsole, "ensure the stdout handle is valid.");
                        // start up vim
                        app.UIRoot.SendKeys("vim");
                        app.UIRoot.SendKeys(Keys.Enter);
                        Globals.WaitForTimeout();
                        app.UIRoot.SendKeys(Keys.Enter);
                        // go to insert mode
                        app.UIRoot.SendKeys("i");
                        // write some text
                        app.UIRoot.SendKeys(testText);
                        Globals.WaitForTimeout();
                        // make sure text showed up in the output
                        Rectangle rect = new Rectangle(0, 0, 20, 20);
                        IEnumerable<string> text = area.GetLinesInRectangle(hConsole, rect);
                        bool foundText = false;
                        foreach (string line in text)
                        {
                            if (line.Contains(testText))
                            {
                                foundText = true;
                                break;
                            }
                        }
                        Verify.IsTrue(foundText);
                    }
                }
            }
        }
    }
}