using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Samples.Terminal
{
    internal static class NativeMethods
    {
        private static int MakeHRFromErrorCode(int errorCode)
        {
            // Don't convert it if it is already an HRESULT
            if ((0xFFFF0000 & errorCode) != 0)
            {
                Debug.Assert(false, "errorCode is already HRESULT");
                return errorCode;
            }

            return unchecked(((int)0x80070000) | errorCode);
        }

        internal static Exception GetExceptionForWin32Error(int errorCode)
        {
            return Marshal.GetExceptionForHR(MakeHRFromErrorCode(errorCode));
        }

        internal static Exception GetExceptionForLastWin32Error()
        {
            return GetExceptionForWin32Error(Marshal.GetLastWin32Error());
        }
    }
}