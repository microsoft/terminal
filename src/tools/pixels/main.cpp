// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>

#include <windowsinternalstring.h>

using namespace wil;
using namespace wistd;
using namespace std;
using namespace Windows::Internal;

#define CONSOLE_WINDOW_FLAGS (WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL)
#define CONSOLE_WINDOW_EX_FLAGS (WS_EX_WINDOWEDGE | WS_EX_ACCEPTFILES | WS_EX_APPWINDOW)

void PrintRect(LPCWSTR pwszLabel, RECT& rc)
{
    LocalMemNativeString foo;
    foo.InitializeFormat(L" L: %5d R: %5d T: %5d B: %5d (W: %5d H: %5d)", rc.left, rc.right, rc.top, rc.bottom, rc.right - rc.left, rc.bottom - rc.top);

    wcout << pwszLabel << " (exclusive rect)" << endl;
    wcout << foo.Get() << endl;
}

void PrintRect(LPCWSTR pwszLabel, SMALL_RECT& rc)
{
    LocalMemNativeString foo;
    foo.InitializeFormat(L" L: %5d R: %5d T: %5d B: %5d (W: %5d H: %5d)", rc.Left, rc.Right, rc.Top, rc.Bottom, rc.Right - rc.Left + 1, rc.Bottom - rc.Top + 1);

    wcout << pwszLabel << " (inclusive rect)" << endl;
    wcout << foo.Get() << endl;
}

void PrintSize(LPCWSTR pwszLabel, COORD& sz)
{
    LocalMemNativeString foo;
    foo.InitializeFormat(L"%37s(W: %5d H: %5d)", L"", sz.X, sz.Y);

    wcout << pwszLabel << endl;
    wcout << foo.Get() << endl;
}

void PrintSize(LPCWSTR pwszLabel, SIZE& sz)
{
    LocalMemNativeString foo;
    foo.InitializeFormat(L"%37s(W: %5d H: %5d)", L"", sz.cx, sz.cy);

    wcout << pwszLabel << endl;
    wcout << foo.Get() << endl;
}

HRESULT PrintMonitorInfo(LPCWSTR pwszLabel, HMONITOR hmon)
{
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    RETURN_IF_WIN32_BOOL_FALSE(GetMonitorInfoW(hmon, &mi));

    bool const IsPrimary = mi.dwFlags & MONITORINFOF_PRIMARY;

    wcout << pwszLabel << endl;
    wcout << "- Monitor Name: " << mi.szDevice << endl;
    wcout << "- Is Primary? " << IsPrimary << endl;
    PrintRect(L"- Monitor Rect:", mi.rcMonitor);
    PrintRect(L"- Work Rect:", mi.rcWork);

    SIZE sz;
    RETURN_IF_FAILED(GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, (UINT*)&sz.cx, (UINT*)&sz.cy));
    PrintSize(L"Effective DPI:", sz);

    return S_OK;
}

BOOL CALLBACK MonitorEnumProc(
    _In_ HMONITOR hMonitor,
    _In_ HDC /*hdcMonitor*/,
    _In_ LPRECT /*lprcMonitor*/,
    _In_ LPARAM /*dwData*/
)
{
    PrintMonitorInfo(L"--- Monitor ---", hMonitor);
    wcout << endl;
    return TRUE;
}

BOOL s_AdjustWindowRectEx(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle)
{
    return AdjustWindowRectEx(prc, dwStyle, fMenu, dwExStyle);
}

BOOL s_UnadjustWindowRectEx(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle)
{
    RECT rc;
    SetRectEmpty(&rc);
    BOOL fRc = s_AdjustWindowRectEx(&rc, dwStyle, fMenu, dwExStyle);
    if (fRc)
    {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }
    return fRc;
}

BOOL s_AdjustWindowRectExForDpi(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle, _In_ UINT dpi)
{
    return AdjustWindowRectExForDpi(prc, dwStyle, fMenu, dwExStyle, dpi);
}

BOOL s_UnadjustWindowRectExForDpi(_Inout_ LPRECT prc, const DWORD dwStyle, const BOOL fMenu, const DWORD dwExStyle, _In_ UINT dpi)
{
    RECT rc;
    SetRectEmpty(&rc);
    BOOL fRc = s_AdjustWindowRectExForDpi(&rc, dwStyle, fMenu, dwExStyle, dpi);
    if (fRc)
    {
        prc->left -= rc.left;
        prc->top -= rc.top;
        prc->right -= rc.right;
        prc->bottom -= rc.bottom;
    }
    return fRc;
}

int __cdecl wmain(int /*argc*/, WCHAR* /*argv*/[])
{
    // set this or we'll get false values when asking for the DPI.
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    HANDLE hOut = CreateFileW(L"CONOUT$",
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_WRITE,
                              0,
                              OPEN_EXISTING,
                              0,
                              0);

    RETURN_IF_HANDLE_INVALID(hOut);

    HWND hwnd = GetConsoleWindow();
    wcout << "Console Window Handle: " << hwnd << endl;

    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    RETURN_IF_HANDLE_NULL(hmon);

    UINT dpix;
    UINT dpiy;
    RETURN_IF_FAILED(GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy));

    RECT rc = { 0 };
    RETURN_IF_WIN32_BOOL_FALSE(GetWindowRect(hwnd, &rc));

    PrintRect(L"Window Rect:", rc);

    RECT rcWindow = rc;

    RETURN_IF_WIN32_BOOL_FALSE(s_UnadjustWindowRectEx(&rc, CONSOLE_WINDOW_FLAGS, FALSE, CONSOLE_WINDOW_EX_FLAGS));
    PrintRect(L"Adjusted Window Rect (unscaled):", rc);

    rc = rcWindow;
    RETURN_IF_WIN32_BOOL_FALSE(s_UnadjustWindowRectExForDpi(&rc, CONSOLE_WINDOW_FLAGS, FALSE, CONSOLE_WINDOW_EX_FLAGS, dpix));
    PrintRect(L"Adjusted Window Rect (scaled):", rc);

    SIZE szClient;
    szClient.cx = rc.right - rc.left;
    szClient.cy = rc.bottom - rc.top;

    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd, &rc));

    PrintRect(L"Client Rect:", rc);

    SIZE sz = { 0 };
    sz.cx = GetSystemMetrics(SM_CXHSCROLL);
    sz.cy = GetSystemMetrics(SM_CYVSCROLL);

    PrintSize(L"Scroll Bar Reservations (unscaled):", sz);

    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    typedef int (*PfnGetDpiMetrics)(int nIndex, int dpi);
    bool fGotMetrics = false;

    if (hUser32 != nullptr)
    {
        // First try the TH1/TH2 name of the function.
        PfnGetDpiMetrics pfn = (PfnGetDpiMetrics)GetProcAddress(hUser32, "GetDpiMetrics");

        if (pfn == nullptr)
        {
            // If not, then try the RS1 name of the function.
            pfn = (PfnGetDpiMetrics)GetProcAddress(hUser32, "GetSystemMetricsForDpi");
        }

        if (pfn != nullptr)
        {
            sz.cx = (SHORT)pfn(SM_CXVSCROLL, dpix);
            sz.cy = (SHORT)pfn(SM_CYHSCROLL, dpiy);
            fGotMetrics = true;
        }
    }

    if (!fGotMetrics)
    {
        sz.cx = (SHORT)GetSystemMetrics(SM_CXVSCROLL);
        sz.cy = (SHORT)GetSystemMetrics(SM_CYHSCROLL);
    }

    PrintSize(L"Scroll Bar Reservations   (scaled):", sz);

    SIZE szScrollScaled = sz;

    COORD coordFont = GetConsoleFontSize(hOut, 0);

    PrintSize(L"Font Size              (unscaled):", coordFont);

    sz.cx = MulDiv(coordFont.X, dpix, 96);
    sz.cy = MulDiv(coordFont.Y, dpiy, 96);

    PrintSize(L"Font Size                (scaled):", sz);

    SIZE szFontScaled = sz;

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(csbiex);

    BOOL b = GetConsoleScreenBufferInfoEx(hOut, &csbiex);

    if (b == FALSE)
    {
        wcout << GetLastError() << endl;
    }

    PrintRect(L"Viewport (chars):", csbiex.srWindow);
    PrintSize(L"Max Window Size (chars):", csbiex.dwMaximumWindowSize);
    PrintSize(L"Cursor Pos (chars):", csbiex.dwCursorPosition);
    PrintSize(L"Buffer Size (chars):", csbiex.dwSize);

    PrintMonitorInfo(L"Primary Monitor Data:", hmon);

    wcout << endl;

    wcout << "All monitors data:" << endl;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, NULL);

    wcout << "------ MATH ------" << endl;

    if (szFontScaled.cx != 0 && szFontScaled.cy != 0)
    {
        SIZE szCharFit;
        szCharFit.cx = szClient.cx / szFontScaled.cx;
        szCharFit.cy = szClient.cy / szFontScaled.cy;
        SIZE szCharLeftover;
        szCharLeftover.cx = szClient.cx % szFontScaled.cx;
        szCharLeftover.cy = szClient.cy % szFontScaled.cy;
        bool fHorizScroll = (csbiex.dwSize.X > (szClient.cx / szFontScaled.cx));
        bool fVertScroll = (csbiex.dwSize.Y > (szClient.cy / szFontScaled.cy));

        wcout << "Start with adjusted window dimensions (scaled for DPI). We take the outer window rect and ask the system to scale it down to what we could use for a client." << endl
              << endl;
        wcout << "Width: " << endl;
        wcout << " Window Adjusted: " << szClient.cx << endl;
        wcout << " / Font         : " << szFontScaled.cx << endl;
        wcout << "                = " << szCharFit.cx << " chars";
        wcout << " with " << szCharLeftover.cx << " pixels leftover" << endl;
        wcout << "This is the number of characters we could fit in the window if Vertical doesn't need its scroll bar." << endl;
        wcout << "Now check if we will need to steal some of Vertical's space for our Horizontal scroll bar." << endl;
        wcout << " Is < buffer of : " << csbiex.dwSize.X << endl;
        wcout << " H-scroll needed= " << fHorizScroll << endl;
        wcout << endl;
        wcout << "Height: " << endl;
        wcout << " Window Adjusted: " << szClient.cy << endl;
        wcout << " / Font         : " << szFontScaled.cy << endl;
        wcout << "                = " << szCharFit.cy << " chars";
        wcout << " with " << szCharLeftover.cy << " pixels leftover" << endl;
        wcout << "This is the number of characters we could fit in the window if Horizontal doesn't need its scroll bar." << endl;
        wcout << "Now check if we will need to steal some of Horizontal's space for our Vertical scroll bar." << endl;
        wcout << " Is < buffer of : " << csbiex.dwSize.Y << endl;
        wcout << " V-scroll needed= " << fVertScroll << endl;
        wcout << endl;

        SIZE szAvailableClient;
        szAvailableClient = szClient;

        SIZE szRemoveBars;
        szRemoveBars.cx = fVertScroll ? szScrollScaled.cx : 0;
        szRemoveBars.cy = fHorizScroll ? szScrollScaled.cy : 0;

        szAvailableClient.cx -= szRemoveBars.cx;
        szAvailableClient.cy -= szRemoveBars.cy;

        SIZE szCharFinal;
        szCharFinal.cx = szAvailableClient.cx / szFontScaled.cx;
        szCharFinal.cy = szAvailableClient.cy / szFontScaled.cy;

        SIZE szCharLeftoverFinal;
        szCharLeftoverFinal.cx = szAvailableClient.cx % szFontScaled.cx;
        szCharLeftoverFinal.cy = szAvailableClient.cy % szFontScaled.cy;

        wcout << "Now math out the space we actually have for the viewport with scroll bars if necessary." << endl
              << endl;
        wcout << "Width: " << endl;
        wcout << " Window Adjusted: " << szClient.cx << endl;
        wcout << " - Vert Scroll  : " << szRemoveBars.cx << endl;
        wcout << "                = " << szAvailableClient.cx << endl;
        wcout << " / Font         : " << szFontScaled.cx << endl;
        wcout << "                = " << szCharFinal.cx << " chars";
        wcout << " with " << szCharLeftoverFinal.cx << " pixels leftover" << endl;
        wcout << endl;
        wcout << "Height: " << endl;
        wcout << " Window Adjusted: " << szClient.cy << endl;
        wcout << " - Horiz Scroll : " << szRemoveBars.cy << endl;
        wcout << "                = " << szAvailableClient.cy << endl;
        wcout << " / Font         : " << szFontScaled.cy << endl;
        wcout << "                = " << szCharFinal.cy << " chars";
        wcout << " with " << szCharLeftoverFinal.cy << " pixels leftover" << endl;

        wcout << endl;
        wcout << "------ TEST PATTERN ------" << endl;
        sz = szCharFinal;

        for (LONG rows = 0; rows < sz.cy; rows++)
        {
            LONG foo = 0;

            for (LONG cols = 0; cols < sz.cx; cols++)
            {
                wcout << foo % 10;
                foo++;
            }
            wcout << endl;
        }
    }
    else
    {
        wcout << "Your font has a 0 size in it. That's sad. No more math for me." << endl;
    }

    return 0;
}
