// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <dwmapi.h>
#include <uxtheme.h>

#include "../inc/conint.h"

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
    static constexpr auto subKey = LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize)";
    static constexpr auto get = [](HKEY key, const wchar_t* value, DWORD& data) {
        DWORD dataType = 0;
        DWORD size = sizeof(data);
        return RegGetValueW(key, subKey, value, RRF_RT_REG_DWORD, &dataType, &data, &size);
    };

    // This is the approach that WinUI3 used in its initial source code release at the time of writing.
    DWORD useLightTheme = 0;
    if (get(HKEY_CURRENT_USER, L"AppsUseLightTheme", useLightTheme) != ERROR_SUCCESS)
    {
        if (get(HKEY_LOCAL_MACHINE, L"SystemUsesLightTheme", useLightTheme) != ERROR_SUCCESS)
        {
            useLightTheme = 0;
        }
    }

    const BOOL useDarkMode = useLightTheme == 0;
    LOG_IF_FAILED(SetWindowTheme(hwnd, useLightTheme ? L"" : L"DarkMode_Explorer", nullptr));
    LOG_IF_FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode)));
    return S_OK;
}
