// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <string>

// Used by window structures to place our special frozen-console painting data
#define GWL_CONSOLE_WNDALLOC (3 * sizeof(DWORD))

HANDLE g_stdOut{ nullptr };

LRESULT CALLBACK s_ScratchWindowProc(HWND hWnd,
                                     UINT Message,
                                     WPARAM wParam,
                                     LPARAM lParam)
{
    switch (Message)
    {
    case WM_CREATE:
    {
        // Show the window
        ShowWindow(hWnd, SW_SHOWDEFAULT);
        break;
    }
    case WM_SIZE:
    {
        const auto width = LOWORD(lParam);
        const auto height = HIWORD(lParam);
        wprintf(L"resized to: %d, %d\n", width, height);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // All painting occurs here, between BeginPaint and EndPaint.
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    }
    // If we get this far, call the default window proc
    return DefWindowProcW(hWnd, Message, wParam, lParam);
}

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    g_stdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    static const auto SCRATCH_WINDOW_CLASS = L"scratch_window_class";
    WNDCLASSEXW scratchClass{ 0 };
    scratchClass.cbSize = sizeof(WNDCLASSEXW);
    scratchClass.lpszClassName = SCRATCH_WINDOW_CLASS;
    scratchClass.lpfnWndProc = s_ScratchWindowProc;
    scratchClass.cbWndExtra = GWL_CONSOLE_WNDALLOC; // this is required to store the owning thread/process override in NTUSER
    auto windowClassAtom{ RegisterClassExW(&scratchClass) };

    // Create a window
    HWND hwnd = CreateWindowExW(0,
                                reinterpret_cast<LPCWSTR>(windowClassAtom),
                                L"Hello World",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                nullptr, // owner
                                nullptr,
                                nullptr,
                                nullptr); // gwl
    if (hwnd == nullptr)
    {
        const auto gle = GetLastError();
        wprintf(L"Failed to create window: %d\n", gle);

        return gle;
    }

    // pump messages
    MSG msg = { 0 };
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
