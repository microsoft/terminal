using System;
using System.Runtime.InteropServices;
using WpfTerminalTestNetCore.Native;
using static WpfTerminalTestNetCore.Native.ProcessApi;

namespace WpfTerminalTestNetCore
{
    /// <summary>
    /// Support for starting and configuring processes.
    /// </summary>
    /// <remarks>
    /// Possible to replace with managed code? The key is being able to provide the PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE attribute
    /// </remarks>
    static class ProcessFactory
    {
        /// <summary>
        /// Start and configure a process. The return value represents the process and should be disposed.
        /// </summary>
        internal static Process Start(string command, IntPtr attributes, IntPtr hPC)
        {
            var startupInfo = new STARTUPINFOEX();
            PseudoConsoleApi.ConptyCreateStartupInfo(hPC, out startupInfo);
            var processInfo = RunProcess(ref startupInfo, command);
            return new Process(startupInfo, processInfo);
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
                bInheritHandles: true,
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
    }
}
