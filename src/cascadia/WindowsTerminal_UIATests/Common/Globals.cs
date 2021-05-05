//----------------------------------------------------------------------------------------------------------------------
// <copyright file="Globals.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Windows Terminal UI Automation global settings</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace WindowsTerminal.UIA.Tests.Common
{
    using System;

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
    }
}
