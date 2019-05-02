//----------------------------------------------------------------------------------------------------------------------
// <copyright file="AutoHelpers.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Console UI Automation test helpers</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Common
{
    using System;
    using System.Globalization;

    using WEX.Logging.Interop;

    public static class AutoHelpers
    {
        public static string FormatInvariant(string format, params object[] args)
        {
            return string.Format(CultureInfo.InvariantCulture, format, args);
        }

        public static void LogInvariant(string format, params object[] args)
        {
            Log.Comment(CultureInfo.InvariantCulture, format, args);
        }

        public static bool DwordToBool(this int value)
        {
            return value != 0;
        }
    }
}
