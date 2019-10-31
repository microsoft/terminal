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
    HRESULT SetDwmImmersiveDarkMode(HWND hwnd, bool enabled) noexcept
    {
        constexpr const int useImmersiveDarkModeAttr = 19;

        const BOOL enabledBool = static_cast<BOOL>(enabled);
        return DwmSetWindowAttribute(hwnd, useImmersiveDarkModeAttr, &enabledBool, sizeof(enabledBool));
    }
}
