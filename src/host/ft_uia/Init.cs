//----------------------------------------------------------------------------------------------------------------------
// <copyright file="ExperimentalTabTests.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// </copyright>
//----------------------------------------------------------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;
using WEX.Logging.Interop;
using WEX.TestExecution;
using WEX.TestExecution.Markup;

namespace Host.Tests.UIA
{
    [TestClass]
    class Init
    {
        static Process appDriver;

        [AssemblyInitialize]
        public static void SetupAll(TestContext context)
        {
            Log.Comment("Searching for WinAppDriver in the same directory where this test was launched from...");
            string winAppDriver = Path.Combine(context.TestDeploymentDir, "WinAppDriver.exe");

            Log.Comment($"Attempting to launch WinAppDriver at: {winAppDriver}");
            Log.Comment($"Working directory: {Environment.CurrentDirectory}");

            appDriver = Process.Start(winAppDriver);
        }

        [AssemblyCleanup]
        public static void CleanupAll()
        {
            try
            {
                appDriver.Kill();
            }
            catch
            {

            }
        }
    }
}
