//----------------------------------------------------------------------------------------------------------------------
// <copyright file="RegistryHelper.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Console UI Automation registry manipulation</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Common
{
    using System;
    using System.Diagnostics;
    using System.IO;

    using Microsoft.Win32;

    using WEX.Common.Managed;
    using WEX.TestExecution;

    public class RegistryHelper : IDisposable
    {
        private static readonly string regExePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "reg.exe");
        private static readonly string regSaveCommand = @"export {0} {1}"; // 0 = path in reg, 1 = file name
        private static readonly string regDelCommand = @"delete {0} /f"; // 0 = path in reg
        private static readonly string regLoadCommand = @"import {0}"; // 0 = file

        private static readonly string regNode = @"HKCU\Console";
        private static readonly string regNodeVerbose = @"HKEY_CURRENT_USER\Console";

        private string backupFile;

        public RegistryHelper()
        {

        }

        ~RegistryHelper()
        {
            this.Dispose(false);
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (!string.IsNullOrEmpty(this.backupFile))
                {
                    this.RestoreRegistry();
                }
            }
        }

        public void SetDefaultValue(string valueName, object valueValue)
        {
            AutoHelpers.LogInvariant("Setting registry key {0}'s value name {1} to value {2}", regNode, valueName, valueValue);
            Registry.SetValue(regNodeVerbose, valueName, valueValue);
        }

        public void BackupRegistry()
        {
            AutoHelpers.LogInvariant("Save existing registry key status.");
            this.backupFile = Path.Combine(Path.GetTempPath(), Path.GetRandomFileName());

            AutoHelpers.LogInvariant("Backing up registry to file: {0}", backupFile);

            Verify.IsFalse(File.Exists(backupFile));

            string backupCmd = AutoHelpers.FormatInvariant(regSaveCommand, regNode, backupFile);

            AutoHelpers.LogInvariant("Calling command: {0} {1}", regExePath, backupCmd);

            Process regProc = Process.Start(regExePath, backupCmd);
            regProc.WaitForExit();

            Verify.IsTrue(File.Exists(backupFile));
        }

        private void DeleteRegistry()
        {
            AutoHelpers.LogInvariant("Deleting registry node: {0}", regNode);
            string deleteCmd = AutoHelpers.FormatInvariant(regDelCommand, regNode);

            Process regProc = Process.Start(regExePath, deleteCmd);
            regProc.WaitForExit();
        }

        public void RestoreRegistry()
        {
            AutoHelpers.LogInvariant("Restore settings to pre-test status.");
            this.DeleteRegistry();

            AutoHelpers.LogInvariant("Restoring registry from file: {0}", backupFile);

            Verify.IsTrue(File.Exists(backupFile));

            string restoreCmd = AutoHelpers.FormatInvariant(regLoadCommand, backupFile);

            Process regProc = Process.Start(regExePath, restoreCmd);
            regProc.WaitForExit();

            File.Delete(backupFile);

            this.backupFile = null;
        }

        public RegistryKey GetMatchingKey(OpenTarget target)
        {
            switch (target)
            {
                case OpenTarget.Defaults:
                    return Registry.CurrentUser.OpenSubKey(@"Console");
                case OpenTarget.Specifics:
                    return Registry.CurrentUser.OpenSubKey(@"Console").OpenSubKey("%SystemRoot%_system32_cmd.exe");
                default:
                    throw new NotImplementedException(AutoHelpers.FormatInvariant("This type of registry key isn't implemented: {0}", target.ToString()));
            }
        }
    }
}
