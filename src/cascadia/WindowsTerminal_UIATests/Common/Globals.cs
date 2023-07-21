//----------------------------------------------------------------------------------------------------------------------
// <copyright file="Globals.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Windows Terminal UI Automation global settings</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace WindowsTerminal.UIA.Tests.Common
{
    using System;
    using WEX.TestExecution.Markup;

    public static class Globals
    {
        public const int Timeout = 50; // in milliseconds
        public const int LongTimeout = 5000; // in milliseconds
        public const int AppCreateTimeout = 3000; // in milliseconds

        public static void WaitForTimeout()
        {
            System.Threading.Thread.Sleep(Globals.Timeout);
        }

        public static void WaitForLongTimeout()
        {
            System.Threading.Thread.Sleep(Globals.LongTimeout);
        }


        static string[] modules =
        {
            "WindowsTerminal.exe",
            "OpenConsole.exe",
            "Microsoft.Terminal.Control.dll",
            "Microsoft.Terminal.Remoting.dll",
            "Microsoft.Terminal.Settings.Editor.dll",
            "Microsoft.Terminal.Settings.Model.dll",
            "TerminalApp.dll",
            "TerminalConnection.dll"
        };

        public static void SweepAllModules(TestContext context)
        {
            foreach (var mod in modules)
            {
                PgoManager.PgoSweepIfInstrumented(context, mod);
            }
        }
    }
}
