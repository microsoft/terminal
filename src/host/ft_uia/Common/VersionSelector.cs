//----------------------------------------------------------------------------------------------------------------------
// <copyright file="VersionSelector.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Console UI Automation Version selection</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Common
{
    using System;

    using WEX.Logging.Interop;

    public static class VersionSelector
    {
        private static string v1v2ValueName = "ForceV2";

        public static void SetConsoleVersion(RegistryHelper reg, ConsoleVersion version)
        {
            switch (version)
            {
                case ConsoleVersion.V1:
                    Log.Comment("Setting console to v1 mode.");
                    reg.SetDefaultValue(v1v2ValueName, 0);
                    break;
                case ConsoleVersion.V2:
                    Log.Comment("Setting console to v2 mode.");
                    reg.SetDefaultValue(v1v2ValueName, 1);
                    break;
                default:
                    throw new NotImplementedException();
            }
        }
    }

    public enum ConsoleVersion
    {
        V1, // legacy 1990s console
        V2 // revision from 2014/2015.
    }
}
