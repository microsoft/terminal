using System;
using System.Runtime.InteropServices;
using static MiniTerm.Native.ProcessApi;
using static MiniTerm.Native.PseudoConsoleApi;

namespace MiniTerm
{
    /// <summary>
    /// Support for starting and configuring processes.
    /// </summary>
    /// <remarks>
    /// Possible to replace with managed code? The key is being able to provide the PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE attribute
    /// </remarks>
    static class Process
    {
        /// <summary>
        /// Start and configure a process. The return value should be considered opaque, and eventually disposed.
        /// </summary>
        internal static ProcessResources Start(string command, IntPtr attributes, IntPtr hPC)
        {
            var startupInfo = ConfigureProcessThread(hPC, attributes);
            var processInfo = RunProcess(ref startupInfo, "cmd.exe");
            return new ProcessResources(startupInfo, processInfo);
        }

        private static STARTUPINFOEX ConfigureProcessThread(IntPtr hPC, IntPtr attributes)
        {
            var lpSize = IntPtr.Zero;
            var success = InitializeProcThreadAttributeList(
                lpAttributeList: IntPtr.Zero,
                dwAttributeCount: 1,
                dwFlags: 0,
                lpSize: ref lpSize
            );
            if (success || lpSize == IntPtr.Zero) // we're not expecting `success` here, we just want to get the calculated lpSize
            {
                throw new InvalidOperationException("Could not calculate the number of bytes for the attribute list. " + Marshal.GetLastWin32Error());
            }

            var startupInfo = new STARTUPINFOEX();
            startupInfo.StartupInfo.cb = Marshal.SizeOf<STARTUPINFOEX>();
            startupInfo.lpAttributeList = Marshal.AllocHGlobal(lpSize);

            success = InitializeProcThreadAttributeList(
                lpAttributeList: startupInfo.lpAttributeList,
                dwAttributeCount: 1,
                dwFlags: 0,
                lpSize: ref lpSize
            );
            if (!success)
            {
                throw new InvalidOperationException("Could not set up attribute list. " + Marshal.GetLastWin32Error());
            }

            success = UpdateProcThreadAttribute(
                lpAttributeList: startupInfo.lpAttributeList,
                dwFlags: 0,
                attribute: attributes,
                lpValue: hPC,
                cbSize: (IntPtr)IntPtr.Size,
                lpPreviousValue: IntPtr.Zero,
                lpReturnSize: IntPtr.Zero
            );
            if (!success)
            {
                throw new InvalidOperationException("Could not set pseudoconsole thread attribute. " + Marshal.GetLastWin32Error());
            }

            return startupInfo;
        }

        private static PROCESS_INFORMATION RunProcess(ref STARTUPINFOEX sInfoEx, string commandLine)
        {
            int securityAttributeSize = Marshal.SizeOf<SECURITY_ATTRIBUTES>();
            var pSec = new SECURITY_ATTRIBUTES { nLength = securityAttributeSize };
            var tSec = new SECURITY_ATTRIBUTES { nLength = securityAttributeSize };
            var success = CreateProcess(
                lpApplicationName: null,
                lpCommandLine: commandLine,
                lpProcessAttributes: ref pSec,
                lpThreadAttributes: ref tSec,
                bInheritHandles: false,
                dwCreationFlags: EXTENDED_STARTUPINFO_PRESENT,
                lpEnvironment: IntPtr.Zero,
                lpCurrentDirectory: null,
                lpStartupInfo: ref sInfoEx,
                lpProcessInformation: out PROCESS_INFORMATION pInfo
            );
            if (!success)
            {
                throw new InvalidOperationException("Could not create process. " + Marshal.GetLastWin32Error());
            }

            return pInfo;
        }

        internal sealed class ProcessResources : IDisposable
        {
            public ProcessResources(STARTUPINFOEX startupInfo, PROCESS_INFORMATION processInfo)
            {
                StartupInfo = startupInfo;
                ProcessInfo = processInfo;
            }

            STARTUPINFOEX StartupInfo { get; }
            PROCESS_INFORMATION ProcessInfo { get; }

            #region IDisposable Support

            private bool disposedValue = false; // To detect redundant calls

            void Dispose(bool disposing)
            {
                if (!disposedValue)
                {
                    if (disposing)
                    {
                        // dispose managed state (managed objects).
                    }

                    // dispose unmanaged state

                    // Free the attribute list
                    if (StartupInfo.lpAttributeList != IntPtr.Zero)
                    {
                        DeleteProcThreadAttributeList(StartupInfo.lpAttributeList);
                        Marshal.FreeHGlobal(StartupInfo.lpAttributeList);
                    }

                    // Close process and thread handles
                    if (ProcessInfo.hProcess != IntPtr.Zero)
                    {
                        CloseHandle(ProcessInfo.hProcess);
                    }
                    if (ProcessInfo.hThread != IntPtr.Zero)
                    {
                        CloseHandle(ProcessInfo.hThread);
                    }

                    disposedValue = true;
                }
            }

            ~ProcessResources()
            {
                // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
                Dispose(false);
            }

            // This code added to correctly implement the disposable pattern.
            public void Dispose()
            {
                // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
                Dispose(true);
                // use the following line if the finalizer is overridden above.
                GC.SuppressFinalize(this);
            }

            #endregion
        }
    }
}
