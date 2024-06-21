using Microsoft.Win32.SafeHandles;
using Windows.Win32;
using Windows.Win32.System.Console;

namespace MiniTermCore
{
    /// <summary>
    /// Utility functions around the new Pseudo Console APIs
    /// </summary>
    internal sealed class PseudoConsole : IDisposable
    {
        public ClosePseudoConsoleSafeHandle Handle { get; }

        private PseudoConsole(ClosePseudoConsoleSafeHandle handle)
        {
            Handle = handle;
        }

        internal static PseudoConsole Create(SafeFileHandle inputReadSide, SafeFileHandle outputWriteSide, int width, int height)
        {
            var createResult = PInvoke.CreatePseudoConsole(
                new COORD { X = (short)width, Y = (short)height },
                inputReadSide, outputWriteSide,
                0, out ClosePseudoConsoleSafeHandle hPC);

            if (createResult != 0)
            {
                throw new InvalidOperationException("Could not create pseudo console. Error Code " + createResult);
            }

            return new PseudoConsole(hPC);
        }

        public void Dispose()
        {
            Handle.Close();
        }
    }
}
