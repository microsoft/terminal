// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "windowtheme.hpp"

#include <dwmapi.h>

using namespace Microsoft::Console::Interactivity::Win32;

#define DWMWA_USE_IMMERSIVE_DARK_MODE 19
#define DARK_MODE_STRING_NAME L"DarkMode_Explorer"
#define UXTHEME_DLL_NAME L"uxtheme.dll"
#define UXTHEME_SHOULDAPPSUSEDARKMODE_ORDINAL 132

// Routine Description:
// - Constructs window theme class (holding module references for function lookups)
WindowTheme::WindowTheme()
{
    // NOTE: Use LoadLibraryExW with LOAD_LIBRARY_SEARCH_SYSTEM32 flag below to avoid unneeded directory traversal.
    //       This has triggered CPG boot IO warnings in the past.
    _module.reset(LoadLibraryExW(UXTHEME_DLL_NAME, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
}

// Routine Description:
// - Attempts to set the dark mode on the given HWND.
// - Will check the system for user preferences and high contrast to see if it's a good idea
//   before setting it.
// Arguments:
// - hwnd - Window to apply dark mode to
// Return Value:
// - S_OK or suitable HRESULT from theming or DWM engines.
[[nodiscard]] HRESULT WindowTheme::TrySetDarkMode(HWND hwnd) const noexcept
{
    // I have to be a big B BOOL or DwnSetWindowAttribute will be upset (E_INVALIDARG) when I am passed in.
    const BOOL isDarkMode = !!_IsDarkMode();

    if (isDarkMode)
    {
        RETURN_IF_FAILED(SetWindowTheme(hwnd, DARK_MODE_STRING_NAME, nullptr));
        RETURN_IF_FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDarkMode, sizeof(isDarkMode)));
    }
    else
    {
        RETURN_IF_FAILED(SetWindowTheme(hwnd, L"", nullptr));
        RETURN_IF_FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDarkMode, sizeof(isDarkMode)));
    }

    return S_OK;
}

// Routine Description:
// - Logical determination of if we should use the dark mode or not.
// - Combines user preferences and high contrast accessibility settings.
// Arguments:
// - <none>
// Return Value:
// - TRUE if dark mode is allowed. FALSE if it is not.
bool WindowTheme::_IsDarkMode() const noexcept
{
    if (_ShouldAppsUseDarkMode() && !_IsHighContrast())
    {
        return true;
    }
    else
    {
        return false;
    }
}

// Routine Description:
// - Looks up the high contrast state of the system.
// Arguments:
// - <none>
// Return Value:
// - True if the system is in high contrast (shouldn't change theme further.) False otherwise.
bool WindowTheme::_IsHighContrast() const noexcept
{
    BOOL fHighContrast = FALSE;
    HIGHCONTRAST hc = { sizeof(hc) };
    if (SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0))
    {
        fHighContrast = (HCF_HIGHCONTRASTON & hc.dwFlags);
    }
    return fHighContrast;
}

// Routine Description:
// - Looks up the user preference for dark mode.
// Arguments:
// - <none>
// Return Value:
// - True if the user chose dark mode in settings. False otherwise.
bool WindowTheme::_ShouldAppsUseDarkMode() const noexcept
{
    if (_module.get() != nullptr)
    {
        typedef bool(WINAPI * PfnShouldAppsUseDarkMode)();

        static bool tried = false;
        static PfnShouldAppsUseDarkMode pfn = nullptr;

        if (!tried)
        {
            pfn = (PfnShouldAppsUseDarkMode)GetProcAddress(_module.get(), MAKEINTRESOURCEA(UXTHEME_SHOULDAPPSUSEDARKMODE_ORDINAL));
        }

        tried = true;

        if (pfn != nullptr)
        {
            return pfn();
        }
    }

    return false;
}
