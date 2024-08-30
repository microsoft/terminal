// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/conint.h"

#include <dwmapi.h>

using namespace Microsoft::Console::Internal;

[[nodiscard]] HRESULT ProcessPolicy::CheckAppModelPolicy(const HANDLE /*hToken*/,
                                                         bool& fIsWrongWayBlocked) noexcept
{
    fIsWrongWayBlocked = false;
    return S_OK;
}

[[nodiscard]] HRESULT ProcessPolicy::CheckIntegrityLevelPolicy(const HANDLE /*hOtherToken*/,
                                                               bool& fIsWrongWayBlocked) noexcept
{
    fIsWrongWayBlocked = false;
    return S_OK;
}

[[nodiscard]] HRESULT Theming::TrySetDarkMode(HWND hwnd) noexcept
{
    static const auto ShouldAppsUseDarkMode = []() {
        static const auto uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        return uxtheme ? reinterpret_cast<bool(WINAPI*)()>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(132))) : nullptr;
    }();
    static const auto IsHighContrastOn = []() {
        bool highContrast = false;
        HIGHCONTRAST hc{ sizeof(hc) };
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0))
        {
            highContrast = (HCF_HIGHCONTRASTON & hc.dwFlags) != 0;
        }
        return highContrast;
    };

    if (ShouldAppsUseDarkMode)
    {
        const BOOL useDarkMode = ShouldAppsUseDarkMode() && !IsHighContrastOn();
        SetWindowTheme(hwnd, useDarkMode ? L"DarkMode_Explorer" : L"", nullptr);
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    }

    return S_FALSE;
}
