#include "precomp.h"
#include "inc/ThemeUtils.h"

namespace Microsoft::Console::ThemeUtils
{
    // Routine Description:
    // - Attempts to enable/disable the dark mode on the frame of a window.
    // Arguments:
    // - hwnd: handle to the window to change
    // - enabled: whether to enable or not the dark mode on the window's frame
    // Return Value:
    // - S_OK or suitable HRESULT from DWM engines.
    [[nodiscard]] HRESULT SetWindowFrameDarkMode(HWND /* hwnd */, bool /* enabled */) noexcept
    {
        // TODO:GH #3425 implement the new DWM API and change
        //  src/interactivity/win32/windowtheme.cpp to use it.
        return S_OK;
    }
}
