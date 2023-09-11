#include <windows.h>
#include <string>
#include "ExtensionInterface.h"

static HBRUSH s_magentaBrush;

LRESULT CALLBACK ExtensionWindowProc(HWND hWnd,
                                     UINT Message,
                                     WPARAM wParam,
                                     LPARAM lParam)
{
    wParam;
    lParam;
    switch (Message)
    {
    case WM_CREATE:
    {
        wprintf(L"WM_CREATE\n");
        // Create a magenta brush for later
        s_magentaBrush = CreateSolidBrush(RGB(255, 0, 255));
        break;
    }
    // case WM_SIZE:
    // {
    //     const auto width = LOWORD(lParam);
    //     const auto height = HIWORD(lParam);
    //     wprintf(L"resized to: %d, %d\n", width, height);
    //     break;
    // }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // All painting occurs here, between BeginPaint and EndPaint.
        FillRect(hdc, &ps.rcPaint, s_magentaBrush);

        EndPaint(hWnd, &ps);
        break;
    }
    }

    return 0;
}

// TODO! hax for now
void StartExtension()
{
    (void)SetExtensionWindowProc(ExtensionWindowProc);
}
