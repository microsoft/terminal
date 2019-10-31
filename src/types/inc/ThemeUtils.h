#pragma once

#include <Windows.h>

namespace Microsoft::Console::ThemeUtils
{
    HRESULT SetDwmImmersiveDarkMode(HWND hwnd, bool enabled) noexcept;
}
