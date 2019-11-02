#include "precomp.h"
#include "inc/ThemeUtils.h"

namespace Microsoft::Console::ThemeUtils
{
    // Routine Description:
    // - Attempts to enable/disable the DWM immersive dark mode on the given
    //   HWND.
    // - Enabling the DWM immersive dark mode on a window makes the title bar
    //   and the window's frame dark.
    // Arguments:
    // - hwnd - Window to the attribute to
    // Return Value:
    // - S_OK or suitable HRESULT from DWM engines.
    [[nodiscard]] HRESULT SetDwmImmersiveDarkMode(HWND hwnd, bool enabled) noexcept
    {
        constexpr const int useImmersiveDarkModeAttr = 19;

        // I have to be a big B BOOL or DwnSetWindowAttribute will be upset (E_INVALIDARG) when I am passed in.
        const BOOL enabledBool = enabled;

        return DwmSetWindowAttribute(hwnd, useImmersiveDarkModeAttr, &enabledBool, sizeof(enabledBool));
    }
}
