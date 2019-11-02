#pragma once

#include <Windows.h>

namespace Microsoft::Console::ThemeUtils
{
    [[nodiscard]] HRESULT SetDwmImmersiveDarkMode(HWND hwnd, bool enabled) noexcept;
}
