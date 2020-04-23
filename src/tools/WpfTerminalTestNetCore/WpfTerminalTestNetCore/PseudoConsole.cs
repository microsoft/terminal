using Microsoft.Win32.SafeHandles;
using System;
using static WpfTerminalTestNetCore.Native.PseudoConsoleApi;

namespace WpfTerminalTestNetCore
{
    /// <summary>
    /// Utility functions around the new Pseudo Console APIs
    /// </summary>
    internal sealed class PseudoConsole : IDisposable
    {
        public static readonly IntPtr PseudoConsoleThreadAttribute = (IntPtr)PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE;

        public IntPtr Handle { get; }

        private PseudoConsole(IntPtr handle)
        {
            this.Handle = handle;
        }

        internal static PseudoConsole Create(SafeFileHandle inputReadSide, SafeFileHandle outputWriteSide, int width, int height)
        {
            var createResult = CreatePseudoConsole(
                new COORD { X = (short)width, Y = (short)height },
                inputReadSide, outputWriteSide,
                0, out IntPtr hPC);
            if (createResult != 0)
            {
                throw new InvalidOperationException("Could not create psuedo console. Error Code " + createResult);
            }
            return new PseudoConsole(hPC);
        }

        public void Resize(short w, short h)
        {
            ResizePseudoConsole(Handle, new COORD { X = w, Y = h });
        }

        public void Dispose()
        {
            ClosePseudoConsole(Handle);
        }
    }
}
