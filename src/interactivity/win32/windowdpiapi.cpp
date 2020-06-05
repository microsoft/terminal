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

BOOL WindowDpiApi::EnablePerMonitorDialogScaling()
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        static bool fTried = false;
        static FARPROC pfnFunc = nullptr;

        if (!fTried)
        {
            pfnFunc = GetProcAddress(_hUser32, "EnablePerMonitorDialogScaling");

            if (pfnFunc == nullptr)
            {
                // If retrieve by name fails, we have to get it by the ordinal because it was made a secret.
                pfnFunc = GetProcAddress(_hUser32, MAKEINTRESOURCEA(2577));
            }

            fTried = true;
        }

        if (pfnFunc != nullptr)
        {
            return (BOOL)pfnFunc();
        }
    }

    return FALSE;
#else
    return EnablePerMonitorDialogScaling();
#endif
}

#pragma endregion

BOOL WindowDpiApi::SetProcessDpiAwarenessContext(_In_ DPI_AWARENESS_CONTEXT dpiContext)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef int(WINAPI * PfnSetProcessDpiAwarenessContexts)(DPI_AWARENESS_CONTEXT dpiContext);

        static bool fTried = false;
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
    return EnableChildWindowDpiMessage(dpiContext);
#endif
}

BOOL WindowDpiApi::EnableChildWindowDpiMessage(const HWND hwnd, const BOOL fEnable)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef BOOL(WINAPI * PfnEnableChildWindowDpiMessage)(HWND hwnd, BOOL fEnable);

        static bool fTried = false;
        static PfnEnableChildWindowDpiMessage pfn = nullptr;

        if (!fTried)
        {
            pfn = (PfnEnableChildWindowDpiMessage)GetProcAddress(_hUser32, "EnableChildWindowDpiMessage");

            if (pfn == nullptr)
            {
                // If that fails, we have to get it by the ordinal because it was made a secret in RS1.
                pfn = (PfnEnableChildWindowDpiMessage)GetProcAddress(_hUser32, MAKEINTRESOURCEA(2704));
            }

            fTried = true;
        }

        if (pfn != nullptr)
        {
            return pfn(hwnd, fEnable);
        }
    }

    return FALSE;
#else
    return EnableChildWindowDpiMessage(hwnd, fEnable);
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

        static bool fTried = false;
        static PfnAdjustWindowRectExForDpi pfn = nullptr;

        if (!fTried)
        {
            // Try to retrieve it the RS1 way first
            pfn = (PfnAdjustWindowRectExForDpi)GetProcAddress(_hUser32, "AdjustWindowRectExForDpi");

            if (pfn == nullptr)
            {
                // If that fails, we have to get it by the ordinal because it was made a secret in TH/TH2.
                pfn = (PfnAdjustWindowRectExForDpi)GetProcAddress(_hUser32, MAKEINTRESOURCEA(2580));
            }

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

int WindowDpiApi::GetWindowDPI(const HWND hwnd)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef int(WINAPI * PfnGetWindowDPI)(HWND hwnd);

        static bool fTried = false;
        static PfnGetWindowDPI pfn = nullptr;

        if (!fTried)
        {
            pfn = (PfnGetWindowDPI)GetProcAddress(_hUser32, "GetWindowDPI");

            if (pfn == nullptr)
            {
                // If that fails, we have to get it by the ordinal because it was made a secret in RS1.
                pfn = (PfnGetWindowDPI)GetProcAddress(_hUser32, MAKEINTRESOURCEA(2707));
            }

            fTried = true;
        }

        if (pfn != nullptr)
        {
            return pfn(hwnd);
        }
    }

    return USER_DEFAULT_SCREEN_DPI;
#else
    // GetDpiForWindow is the public API version (as of RS1) of GetWindowDPI
    return GetDpiForWindow(hwnd);
#endif
}

int WindowDpiApi::GetSystemMetricsForDpi(const int nIndex, const UINT dpi)
{
#ifdef CON_DPIAPI_INDIRECT
    if (_hUser32 != nullptr)
    {
        typedef int(WINAPI * PfnGetDpiMetrics)(int nIndex, int dpi);

        static bool fTried = false;
        static PfnGetDpiMetrics pfn = nullptr;

        if (!fTried)
        {
            // First try the TH1/TH2 name of the function.
            pfn = (PfnGetDpiMetrics)GetProcAddress(_hUser32, "GetDpiMetrics");

            if (pfn == nullptr)
            {
                // If not, then try the RS1 name of the function.
                pfn = (PfnGetDpiMetrics)GetProcAddress(_hUser32, "GetSystemMetricsForDpi");
            }

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
