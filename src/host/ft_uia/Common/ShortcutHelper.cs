//----------------------------------------------------------------------------------------------------------------------
// <copyright file="ShortcutHelper.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Console UI Automation shortcut file manipulation</summary>
//----------------------------------------------------------------------------------------------------------------------
namespace Conhost.UIA.Tests.Common
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Runtime.InteropServices;

    using WEX.Common.Managed;
    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;

    using Conhost.UIA.Tests.Common.NativeMethods;

    public class ShortcutHelper : IDisposable
    {
        private bool isDisposed = false;

        public string ShortcutPath { get; private set; }

        public ShortcutHelper()
        {
            this.ShortcutPath = null;
        }

        ~ShortcutHelper()
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
            if (!this.isDisposed)
            {
                this.CleanupTempCmdShortcut();
            }

            this.isDisposed = true;
        }

        public void CreateTempCmdShortcut()
        {
            string tempPath = Path.Combine(Path.GetTempPath(), AutoHelpers.FormatInvariant("{0}.lnk", Path.GetRandomFileName()));
            AutoHelpers.LogInvariant("Creating temporary shortcut: {0}", tempPath);

            Shell32.IShellLinkW link = (Shell32.IShellLinkW)new Shell32.ShellLink();
            link.SetDescription("Created by Conhost.UIA.Tests.Common.ShortcutHelper");
            link.SetPath(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "cmd.exe"));

            Shell32.IPersistFile persist = (Shell32.IPersistFile)link; // performs QI
            persist.Save(tempPath, false);

            this.ShortcutPath = tempPath;
        }

        public void CleanupTempCmdShortcut()
        {
            if (!string.IsNullOrEmpty(this.ShortcutPath))
            {
                File.Delete(this.ShortcutPath);
                this.ShortcutPath = null;
            }
        }

        public WinConP.NT_CONSOLE_PROPS GetConsoleProps()
        {
            Shell32.IPersistFile persist = (Shell32.IPersistFile)new Shell32.ShellLink();
            persist.Load(this.ShortcutPath, (uint)ObjBase.STGM.STGM_READ);

            Shell32.IShellLinkDataList sldl = (Shell32.IShellLinkDataList)persist;

            WinConP.NT_CONSOLE_PROPS props = new WinConP.NT_CONSOLE_PROPS();
            IntPtr ppDataBlock = Marshal.AllocHGlobal(Marshal.SizeOf(props));

            try
            {
                sldl.CopyDataBlock(WinConP.NT_CONSOLE_PROPS_SIG, out ppDataBlock);

                // The marshaler doesn't like using the existing instance that we made above because it's a value type and
                // there are potential string pointers here. Give it the type instead and it can handle setting everything up.
                props = (WinConP.NT_CONSOLE_PROPS)Marshal.PtrToStructure(ppDataBlock, typeof(WinConP.NT_CONSOLE_PROPS));
            }
            finally
            {
                Marshal.FreeHGlobal(ppDataBlock);
            }

            return props;
        }

        public void SetConsoleProps(WinConP.NT_CONSOLE_PROPS props)
        {
            Shell32.IPersistFile persist = (Shell32.IPersistFile)new Shell32.ShellLink();
            persist.Load(this.ShortcutPath, (uint)ObjBase.STGM.STGM_READWRITE);

            Shell32.IShellLinkDataList sldl = (Shell32.IShellLinkDataList)persist;

            sldl.RemoveDataBlock(WinConP.NT_CONSOLE_PROPS_SIG);

            IntPtr ppDataBlock = Marshal.AllocHGlobal(Marshal.SizeOf(props));
            try
            {
                Marshal.StructureToPtr(props, ppDataBlock, false);

                // we are assuming that the signature is set properly on the NT_CONSOLE_PROPS structure
                Verify.AreEqual(props.dbh.dwSignature, (int)WinConP.NT_CONSOLE_PROPS_SIG);
                sldl.AddDataBlock(ppDataBlock);

                persist.Save(null, true); // 2nd var is ignored when 1st is null
            }
            finally
            {
                Marshal.FreeHGlobal(ppDataBlock);
            }
        }

        public IDictionary<Wtypes.PROPERTYKEY, object> GetFromPropertyStore(IEnumerable<Wtypes.PROPERTYKEY> keys)
        {
            if (keys == null)
            {
                throw new NotSupportedException(AutoHelpers.FormatInvariant("Keys passed cannot be null"));
            }

            Shell32.IPersistFile persist = (Shell32.IPersistFile)new Shell32.ShellLink();
            persist.Load(this.ShortcutPath, (uint)ObjBase.STGM.STGM_READ);

            Shell32.IPropertyStore store = (Shell32.IPropertyStore)persist;

            Dictionary<Wtypes.PROPERTYKEY, object> results = new Dictionary<Wtypes.PROPERTYKEY, object>();

            foreach (Wtypes.PROPERTYKEY key in keys)
            {
                Wtypes.PROPERTYKEY pkey = key; // iteration variables are read-only and we need to pass by ref
                object pv;
                store.GetValue(ref pkey, out pv);

                results.Add(key, pv);
            }

            return results;
        }

        public void SetToPropertyStore(IDictionary<Wtypes.PROPERTYKEY, object> properties)
        {
            if (properties == null)
            {
                throw new NotSupportedException(AutoHelpers.FormatInvariant("Properties passed cannot be null."));
            }

            Shell32.IPersistFile persist = (Shell32.IPersistFile)new Shell32.ShellLink();
            persist.Load(this.ShortcutPath, (uint)ObjBase.STGM.STGM_READWRITE);

            Shell32.IPropertyStore store = (Shell32.IPropertyStore)persist;

            foreach (Wtypes.PROPERTYKEY key in properties.Keys)
            {
                Wtypes.PROPERTYKEY pkey = key; // iteration variables are read-only and we need to pass by ref
                object pv = properties[key];

                store.SetValue(ref pkey, ref pv);
            }

            persist.Save(null, true);
        }
    }
}
