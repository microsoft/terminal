#pragma once

#include <Windows.h>

namespace Microsoft::Console::ThemeUtils
{
    [[nodiscard]] HRESULT SetWindowFrameDarkMode(HWND hwnd, bool enabled) noexcept;
}
