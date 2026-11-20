// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <Windows.h>
#include <string_view>

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // WinUI hosts its TSF context in an InputSite child window. Limit the search
    // to this control's owner: other windows can share the same UI thread.
    inline HWND FindInputSiteWindow(const HWND owner) noexcept
    {
        HWND inputSite = nullptr;
        if (owner)
        {
            EnumChildWindows(
                owner,
                [](HWND hwnd, LPARAM param) noexcept -> BOOL {
                    wchar_t className[64];
                    const auto length = GetClassNameW(hwnd, className, ARRAYSIZE(className));
                    if (length && std::wstring_view{ className, static_cast<size_t>(length) } == L"Windows.UI.Input.InputSite.WindowClass")
                    {
                        *reinterpret_cast<HWND*>(param) = hwnd;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&inputSite));
        }

        // Returns nullptr if the InputSite doesn't exist (yet).
        return inputSite;
    }
}
