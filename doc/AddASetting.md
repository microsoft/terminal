# Adding a Settings Proper
#ifndef MICA_ALT_SUPPORT_H
#define MICA_ALT_SUPPORT_H

#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// DWM attributes
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#define DWMWA_MICA_EFFECT 1029

// Backdrop types
enum DWM_SYSTEMBACKDROP_TYPE
{
    DWMSBT_AUTO = 0,
    DWMSBT_NONE = 1,
    DWMSBT_MAINWINDOW = 2,
    DWMSBT_TRANSIENTWINDOW = 3,
    DWMSBT_TABBEDWINDOW = 4
};

class MicaAltSupport
{
public:
    // Apply Mica Alt backdrop
    static bool ApplyMicaAlt(HWND hwnd)
    {
        if (!IsWindows11OrGreater())
            return false;

        if (hwnd == NULL)
            return false;

        // Try modern method first (Windows 11 22H2+)
        int backdropType = DWMSBT_TRANSIENTWINDOW;
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, 
                                           &backdropType, sizeof(int));

        if (SUCCEEDED(hr))
            return true;

        // Fallback for earlier Windows 11
        int micaEnabled = 1;
        hr = DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT, 
                                   &micaEnabled, sizeof(int));

        return SUCCEEDED(hr);
    }

    // Apply standard Mica backdrop
    static bool ApplyMica(HWND hwnd)
    {
        if (!IsWindows11OrGreater())
            return false;

        if (hwnd == NULL)
            return false;

        int backdropType = DWMSBT_MAINWINDOW;
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, 
                                           &backdropType, sizeof(int));

        return SUCCEEDED(hr);
    }

    // Remove backdrop effect
    static bool RemoveBackdrop(HWND hwnd)
    {
        if (hwnd == NULL)
            return false;

        int backdropType = DWMSBT_NONE;
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, 
                                           &backdropType, sizeof(int));

        return SUCCEEDED(hr);
    }

    // Apply tabbed window backdrop
    static bool ApplyTabbedBackdrop(HWND hwnd)
    {
        if (!IsWindows11OrGreater())
            return false;

        if (hwnd == NULL)
            return false;

        int backdropType = DWMSBT_TABBEDWINDOW;
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, 
                                           &backdropType, sizeof(int));

        return SUCCEEDED(hr);
    }

    // Extend frame into client area
    static bool ExtendFrame(HWND hwnd)
    {
        if (hwnd == NULL)
            return false;

        MARGINS margins = { -1, -1, -1, -1 };
        HRESULT hr = DwmExtendFrameIntoClientArea(hwnd, &margins);

        return SUCCEEDED(hr);
    }

private:
    // Check if running Windows 11 or later
    static bool IsWindows11OrGreater()
    {
        OSVERSIONINFOEXW osvi = { sizeof(osvi) };
        DWORDLONG const dwlConditionMask = 
            VerSetConditionMask(
                VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL),
                VER_BUILDNUMBER, VER_GREATER_EQUAL);

        osvi.dwMajorVersion = 10;
        osvi.dwBuildNumber = 22000;

        return VerifyVersionInfoW(&osvi, 
                                  VER_MAJORVERSION | VER_BUILDNUMBER, 
                                  dwlConditionMask) != FALSE;
    }
};

#endif // MICA_ALT_SUPPORT_H

// Example usage in WinMain
#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"MicaAltWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Mica Alt Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL)
        return 0;

    // Apply Mica Alt backdrop
    MicaAltSupport::ApplyMicaAlt(hwnd);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        // Apply Mica Alt when window is created
        MicaAltSupport::ApplyMicaAlt(hwnd);
        return 0;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case '1': // Press 1 for Mica Alt
            MicaAltSupport::ApplyMicaAlt(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        case '2': // Press 2 for standard Mica
            MicaAltSupport::ApplyMica(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        case '3': // Press 3 to remove backdrop
            MicaAltSupport::RemoveBackdrop(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        case '4': // Press 4 for tabbed backdrop
            MicaAltSupport::ApplyTabbedBackdrop(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Draw some text
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        
        DrawTextW(hdc, L"Press 1: Mica Alt\nPress 2: Standard Mica\nPress 3: Remove\nPress 4: Tabbed", 
                  -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}