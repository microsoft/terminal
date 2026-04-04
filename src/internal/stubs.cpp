// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/conint.h"
#include <dwmapi.h>
#include <uxtheme.h>

// DWMWA_USE_IMMERSIVE_DARK_MODE is defined in newer SDKs, but we define it here
// for compatibility with older SDKs.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

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

// Helper function to check if the system is using a dark theme.
// This checks the registry for the AppsUseLightTheme value.
static bool _IsSystemInDarkTheme() noexcept
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"SystemUsesLightTheme",
            RRF_RT_DWORD,
            nullptr,
            &value,
            &size) == ERROR_SUCCESS)
    {
        // value is 0 for dark theme, 1 for light theme
        return value == 0;
    }
    // Default to light theme if we can't read the registry
    return false;
}

[[nodiscard]] HRESULT Theming::TrySetDarkMode(HWND hwnd) noexcept
{
    if (hwnd == nullptr)
    {
        return E_INVALIDARG;
    }

    // Check if system is in dark mode
    const BOOL isDarkMode = _IsSystemInDarkTheme() ? TRUE : FALSE;

    // Set the window attribute for dark mode
    // This will make the title bar follow the system theme
    const HRESULT hr = DwmSetWindowAttribute(
        hwnd,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &isDarkMode,
        sizeof(isDarkMode));

    // Set the scroll bar theme to match dark/light mode
    // "DarkMode_Explorer" is the theme name for dark scrollbars
    // "Explorer" is the default light theme
    if (isDarkMode)
    {
        // Try to set dark mode theme for scrollbars
        // This uses the undocumented but widely used DarkMode_Explorer theme
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
    else
    {
        // Reset to default theme for light mode
        SetWindowTheme(hwnd, L"Explorer", nullptr);
    }

    return hr;
}
