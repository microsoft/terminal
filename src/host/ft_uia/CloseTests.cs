//----------------------------------------------------------------------------------------------------------------------
// <copyright file="CloseTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
// <summary>UI Automation tests for verifying the mechanism of closing processes attached to the console host window.</summary>
//----------------------------------------------------------------------------------------------------------------------

namespace Conhost.UIA.Tests
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.IO.Pipes;
    using System.Threading;

    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using Conhost.UIA.Tests.Common;
    using Conhost.UIA.Tests.Common.NativeMethods;
    using Conhost.UIA.Tests.Elements;
    using System.Threading.Tasks;

    [TestClass]
    public class CloseTests
    {
        public static Queue<string> messages = new Queue<string>();

        public TestContext TestContext { get; set; }

        private static string closeTestBinaryLocation;
        private static readonly string testPipeName = "ConsoleUIACloseTests";
        private static readonly uint processCount = 4;

        private static readonly string attachPattern = "closetest: child {0}: attached to console";
        private static readonly string spacerStartsWith = "closetest: attached process list:";
        private static readonly string pausingPattern = "closetest: child {0}: CTRL_CLOSE_EVENT received, pausing...";
        private static readonly string exitingPattern = "closetest: child {0}: CTRL_CLOSE_EVENT received, exiting...";

        [ClassInitialize]
        public static void ClassSetup(TestContext context)
        {
            Log.Comment("Searching for CloseTest.exe in the same directory where this test was launched from...");
            closeTestBinaryLocation = Path.Combine(context.TestDeploymentDir, "CloseTest.exe");
            Verify.IsFalse(string.IsNullOrEmpty(closeTestBinaryLocation));
            Verify.IsTrue(File.Exists(closeTestBinaryLocation));
        }

        public void ListenToPipe(NamedPipeServerStream stream, CancellationToken token)
        {
            StreamReader reader = new StreamReader(stream);
            while (!reader.EndOfStream && !token.IsCancellationRequested)
            {
                string line = reader.ReadLine();
                if (!string.IsNullOrWhiteSpace(line))
                {
                    messages.Enqueue(line);
                }
            }
        }

        public void MakePipeServer(CancellationToken token)
        {
            while (!token.IsCancellationRequested)
            {
                NamedPipeServerStream str = new NamedPipeServerStream(testPipeName, PipeDirection.In, 250);
                str.WaitForConnection();

                Task.Run(() => ListenToPipe(str, token), token);
            }
        }

        [TestMethod]
        [TestProperty("IsolationLevel", "Method")]
        public void CheckClose()
        {
            string closeTestCmdLine = $"{closeTestBinaryLocation} -n {processCount} --log {testPipeName} --delay 1000 --no-realloc";

            using (var tokenSource = new CancellationTokenSource())
            {
                var token = tokenSource.Token;

                Task.Run(() => MakePipeServer(token), token);

                Log.Comment("Connect a test console window to the close test binary and wait for a few seconds.");
                CmdApp app = new CmdApp(CreateType.ProcessOnly, TestContext, closeTestCmdLine);
                Thread.Sleep(TimeSpan.FromSeconds(2));
                NativeMethods.Win32BoolHelper(WinCon.FreeConsole(), "Free console bindings so we aren't shut down when we kill the window.");
                Log.Comment("Click the close button on the window then wait a few seconds for it to cleanup.");
                app.GetCloseButton().Click();

                Thread.Sleep(TimeSpan.FromSeconds(5));
                tokenSource.Cancel();

                Log.Comment("Compare the output we received on our pipes to what we expected to get in terms of ordering and process count.");

                for (uint i = 1; i <= processCount; i++)
                {
                    string expected = string.Format(attachPattern, i);
                    Verify.AreEqual(expected, messages.Dequeue());
                }

                Verify.IsTrue(messages.Dequeue().StartsWith(spacerStartsWith));

                for (uint i = processCount; i >= 1; i--)
                {
                    string expected;
                    expected = string.Format(pausingPattern, i);
                    Verify.AreEqual(expected, messages.Dequeue());
                    expected = string.Format(exitingPattern, i);
                    Verify.AreEqual(expected, messages.Dequeue());
                }
            }
        }
    }
}
