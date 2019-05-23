//----------------------------------------------------------------------------------------------------------------------
// <copyright file="KeyPressTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for the certain key presses.</summary>
//----------------------------------------------------------------------------------------------------------------------
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;

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
    class KeyPressTests
    {
        public TestContext TestContext { get; set; }

        [TestMethod]
        public void VerifyCtrlKeysBash()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (ViewportArea area = new ViewportArea(app))
                    {
                        IntPtr hConsole = app.GetStdOutHandle();
                        Verify.IsNotNull(hConsole, "Ensure the STDOUT handle is valid.");
                        // start up bash, run cat, type some keys with ctrl held down
                        app.UIRoot.SendKeys("bash");
                        app.UIRoot.SendKeys(Keys.Enter);
                        Globals.WaitForTimeout();
                        app.UIRoot.SendKeys("cat");
                        app.UIRoot.SendKeys(Keys.Enter);
                        Globals.WaitForTimeout();
                        app.UIRoot.SendKeys(Keys.Control + "ahz" + Keys.Control);
                        Globals.WaitForTimeout();
                        // make sure "^A^H^Z" showed up in the output
                        Rectangle rect = new Rectangle(0, 0, 10, 10);
                        IEnumerable<string> text = area.GetLinesInRectangle(hConsole, rect);
                        bool foundCtrlChars = false;
                        foreach (string line in text)
                        {
                            if (line.Contains("^A^H^Z"))
                            {
                                foundCtrlChars = true;
                                break;
                            }
                        }
                        Verify.IsTrue(foundCtrlChars);
                    }
                }
            }
        }

        [TestMethod]
        public void VerifyCtrlCBash()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (ViewportArea area = new ViewportArea(app))
                    {
                        IntPtr hConsole = app.GetStdOutHandle();
                        Verify.IsNotNull(hConsole, "Ensure the STDOUT handle is valid.");
                        // start up bash, run cat, type ctrl+c 
                        app.UIRoot.SendKeys("bash");
                        app.UIRoot.SendKeys(Keys.Enter);
                        Globals.WaitForTimeout();
                        app.UIRoot.SendKeys("cat");
                        app.UIRoot.SendKeys(Keys.Enter);
                        Globals.WaitForTimeout();
                        app.UIRoot.SendKeys(Keys.Control + "c" + Keys.Control);
                        Globals.WaitForTimeout();
                        // make sure "^C" showed up in the output
                        Rectangle rect = new Rectangle(0, 0, 10, 10);
                        IEnumerable<string> text = area.GetLinesInRectangle(hConsole, rect);
                        bool foundCtrlC = false;
                        foreach (string line in text)
                        {
                            if (line.Contains("^C"))
                            {
                                foundCtrlC = true;
                                break;
                            }
                        }
                        Verify.IsTrue(foundCtrlC);
                    }
                }
            }
        }

        [TestMethod]
        public void VerifyCtrlZCmd()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (ViewportArea area = new ViewportArea(app))
                    {
                        IntPtr hConsole = app.GetStdOutHandle();
                        Verify.IsNotNull(hConsole, "Ensure the handle is valid.");
                        // get cursor location
                        WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX screenBufferInfo = new WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX();
                        WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref screenBufferInfo);
                        // send ^Z
                        app.UIRoot.SendKeys(Keys.Control + "z" + Keys.Control);
                        Globals.WaitForTimeout();
                        // test that "^Z" exists on the screen
                        Rectangle rect = new Rectangle(0, 0, 200, 20);
                        IEnumerable<string> text = area.GetLinesInRectangle(hConsole, rect);
                        bool foundCtrlZ = false;
                        foreach (string line in text)
                        {
                            if (line.Contains("^Z"))
                            {
                                foundCtrlZ = true;
                                break;
                            }
                        }
                        Verify.IsTrue(foundCtrlZ);
                    }
                }
            }
        }

        [TestMethod]
        public void VerifyCtrlHCmd()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (ViewportArea area = new ViewportArea(app))
                    {
                        string testText = "1234blah5678";
                        IntPtr hConsole = app.GetStdOutHandle();
                        Verify.IsNotNull(hConsole, "Ensure the handle is valid.");
                        // get cursor location
                        WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX screenBufferInfo = new WinCon.CONSOLE_SCREEN_BUFFER_INFO_EX();
                        WinCon.GetConsoleScreenBufferInfoEx(hConsole, ref screenBufferInfo);
                        // send some text and a ^H to remove the last character
                        app.UIRoot.SendKeys(testText + Keys.Control + "h" + Keys.Control);
                        Globals.WaitForTimeout();
                        // test that we're missing the last character of testText on the line
                        Rectangle rect = new Rectangle(0, 0, 200, 20);
                        IEnumerable<string> text = area.GetLinesInRectangle(hConsole, rect);
                        bool foundCtrlH = false;
                        foreach (string line in text)
                        {
                            if (line.Contains(testText.Substring(0, testText.Length - 1)) && !line.Contains(testText))
                            {
                                foundCtrlH = true;
                                break;
                            }
                        }
                        Verify.IsTrue(foundCtrlH);
                    }
                }
            }
        }

        [TestMethod]
        public void VerifyCtrlCCmd()
        {
            using (RegistryHelper reg = new RegistryHelper())
            {
                reg.BackupRegistry();
                using (CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext))
                {
                    using (ViewportArea area = new ViewportArea(app))
                    {
                        IntPtr hConsole = app.GetStdOutHandle();
                        Verify.IsNotNull(hConsole, "Ensure the handle is valid.");
                        Globals.WaitForTimeout();
                        // send ctrl-c sequence
                        const int keypressCount = 10;
                        for (int i = 0; i < keypressCount; ++i)
                        {
                            app.UIRoot.SendKeys(Keys.Control + "c" + Keys.Control);
                        }
                        Globals.WaitForTimeout();
                        // fetch the text
                        Rectangle rect = new Rectangle(0, 0, 50, 50);
                        IEnumerable<string> text = area.GetLinesInRectangle(hConsole, rect);
                        // filter out the blank lines
                        List<string> possiblePromptLines = new List<string>();
                        for (int i = 0; i < text.Count(); ++i)
                        {
                            string line = text.ElementAt(i);
                            line = line.Trim(' ');
                            if (!line.Equals(""))
                            {
                                possiblePromptLines.Add(line);
                            }
                        }
                        // make sure that the prompt line shows up for each ^C key press
                        Verify.IsTrue(possiblePromptLines.Count() >= keypressCount);
                        possiblePromptLines.Reverse();
                        for (int i = 0; i < keypressCount; ++i)
                        {
                            Verify.AreEqual(possiblePromptLines[0], possiblePromptLines[1]);
                            possiblePromptLines.RemoveAt(0);
                        }
                    }
                }
            }
        }
    }
}
