// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "windowdpiapi.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

#pragma region Public Methods

#pragma region IHighDpiApi Members

[[nodiscard]] HRESULT WindowDpiApi::SetProcessPerMonitorDpiAwareness()
{
    return SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
}

BOOL WindowDpiApi::SetProcessDpiAwarenessContext()
{
    return SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

#pragma endregion

BOOL WindowDpiApi::SetProcessDpiAwarenessContext(_In_ DPI_AWARENESS_CONTEXT dpiContext)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef int(WINAPI * PfnSetProcessDpiAwarenessContexts)(DPI_AWARENESS_CONTEXT dpiContext);

        static auto fTried = false;
        static PfnSetProcessDpiAwarenessContexts pfn = nullptr;

        if (!fTried)
        {
            pfn = (PfnSetProcessDpiAwarenessContexts)GetProcAddress(_hUser32, "SetProcessDpiAwarenessContext");
        }

        fTried = true;

        if (pfn != nullptr)
        {
            return pfn(dpiContext);
        }
    }

    return FALSE;
#else
    return SetProcessDpiAwarenessContext(dpiContext);
#endif
}

BOOL WindowDpiApi::AdjustWindowRectExForDpi(_Inout_ LPRECT const lpRect,
                                            const DWORD dwStyle,
                                            const BOOL bMenu,
                                            const DWORD dwExStyle,
                                            const UINT dpi)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef BOOL(WINAPI * PfnAdjustWindowRectExForDpi)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, int dpi);

        static auto fTried = false;
        static PfnAdjustWindowRectExForDpi pfn = nullptr;

        if (!fTried)
        {
            pfn = (PfnAdjustWindowRectExForDpi)GetProcAddress(_hUser32, "AdjustWindowRectExForDpi");

            fTried = true;
        }

        if (pfn != nullptr)
        {
            return pfn(lpRect, dwStyle, bMenu, dwExStyle, dpi);
        }
    }

    return AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
#else
    return AdjustWindowRectExForDpi(lpRect, dwStyle, bMenu, dwExStyle, dpi);
#endif
}

int WindowDpiApi::GetDpiForWindow(const HWND hwnd)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef int(WINAPI * PfnGetDpiForWindow)(HWND hwnd);

        static auto fTried = false;
        static PfnGetDpiForWindow pfn = nullptr;

        if (!fTried)
        {
            pfn = (PfnGetDpiForWindow)GetProcAddress(_hUser32, "GetDpiForWindow");

            fTried = true;
        }

        if (pfn != nullptr)
        {
            return pfn(hwnd);
        }
    }

    return USER_DEFAULT_SCREEN_DPI;
#else
    return GetDpiForWindow(hwnd);
#endif
}

int WindowDpiApi::GetSystemMetricsForDpi(const int nIndex, const UINT dpi)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef int(WINAPI * PfnGetDpiMetrics)(int nIndex, int dpi);

        static auto fTried = false;
        static PfnGetDpiMetrics pfn = nullptr;

        if (!fTried)
        {
            pfn = (PfnGetDpiMetrics)GetProcAddress(_hUser32, "GetSystemMetricsForDpi");

            fTried = true;
        }

        if (pfn != nullptr)
        {
            return pfn(nIndex, dpi);
        }
    }

    return GetSystemMetrics(nIndex);
#else
    return GetSystemMetricsForDpi(nIndex, dpi);
#endif
}

#pragma endregion

#pragma region Private Methods

#ifdef CON_DPIAPI_INDIRECT
WindowDpiApi::WindowDpiApi()
{
    // NOTE: Use LoadLibraryExW with LOAD_LIBRARY_SEARCH_SYSTEM32 flag below to avoid unneeded directory traversal.
    //       This has triggered CPG boot IO warnings in the past.
    _hUser32 = LoadLibraryExW(L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}
#endif

WindowDpiApi::~WindowDpiApi()
{
    if (_hUser32 != nullptr)
    {
        FreeLibrary(_hUser32);
        _hUser32 = nullptr;
    }
}

#pragma endregion
