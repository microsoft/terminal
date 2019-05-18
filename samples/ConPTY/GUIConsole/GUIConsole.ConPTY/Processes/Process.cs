using System;
using System.Runtime.InteropServices;
using static GUIConsole.ConPTY.Native.ProcessApi;

namespace GUIConsole.ConPTY.Processes
{
    /// <summary>
    /// Represents an instance of a process.
    /// </summary>
    internal sealed class Process : IDisposable
    {
        public Process(STARTUPINFOEX startupInfo, PROCESS_INFORMATION processInfo)
        {
            StartupInfo = startupInfo;
            ProcessInfo = processInfo;
        }

        public STARTUPINFOEX StartupInfo { get; }
        public PROCESS_INFORMATION ProcessInfo { get; }

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

        ~Process()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        #endregion
    }
}
