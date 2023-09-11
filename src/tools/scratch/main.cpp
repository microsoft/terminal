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

        // // Make the window a topmost window cause islands are weird
        // SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        // Nope, that didn't work
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
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_HIGHLIGHT + 1));

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
HWND g_parent;
void setupHooks(HWND parent, HWND /*child*/)
{
    // get pid and tid for parent window
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(parent, &pid);
    g_parent = parent;

    // Ala
    // https://devblogs.microsoft.com/oldnewthing/20210104-00/?p=104656
    // circa 2021

    WINEVENTPROC sizeChange = [](HWINEVENTHOOK /* hWinEventHook */,
                                 DWORD /* event */,
                                 HWND hwnd,
                                 LONG /* idObject */,
                                 LONG /* idChild */,
                                 DWORD /* dwEventThread */,
                                 DWORD /* dwmsEventTime */) {
        if (hwnd == g_parent)
        {
            wprintf(L"Got a location change\n");
        }
    };

    // Listen for changes in size and pos of the ownerhandle
    SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE,
        EVENT_OBJECT_LOCATIONCHANGE,
        nullptr,
        reinterpret_cast<WINEVENTPROC>(sizeChange),
        pid,
        tid,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Listen for changes in the ownerhandle's visibility
    SetWinEventHook(
        EVENT_OBJECT_SHOW,
        EVENT_OBJECT_HIDE,
        nullptr,
        [](HWINEVENTHOOK /* hWinEventHook */,
           DWORD /* event */,
           HWND /* hwnd */,
           LONG /* idObject */,
           LONG /* idChild */,
           DWORD /* dwEventThread */,
           DWORD /* dwmsEventTime */) {
            // if (hwnd == g_hWnd)
            // {
            //     wprintf(L"Got a visibility change\n");
            // }
        },
        pid,
        tid,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    SetWinEventHook(
        EVENT_OBJECT_REORDER,
        EVENT_OBJECT_REORDER,
        nullptr,
        [](HWINEVENTHOOK /* hWinEventHook */,
           DWORD /* event */,
           HWND /* hwnd */,
           LONG /* idObject */,
           LONG /* idChild */,
           DWORD /* dwEventThread */,
           DWORD /* dwmsEventTime */) {
            // if (hwnd == g_hWnd)
            // {
            //     wprintf(L"Got a reorder change\n");
            // }
        },
        pid,
        tid,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    SetWinEventHook(
        EVENT_OBJECT_DESTROY,
        EVENT_OBJECT_DESTROY,
        nullptr,
        [](HWINEVENTHOOK /* hWinEventHook */,
           DWORD /* event */,
           HWND /* hwnd */,
           LONG /* idObject */,
           LONG /* idChild */,
           DWORD /* dwEventThread */,
           DWORD /* dwmsEventTime */) {
            // if (hwnd == g_hWnd)
            // {
            //     wprintf(L"Got a destroy change\n");
            // }
        },
        pid,
        tid,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int argc, WCHAR* argv[])
{
    g_stdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    // Parse out the handle provided on the command line. It's in hexadecimal.
    const auto ownerHandle = argc < 2 ?
                                 0 :
                                 std::stoul(argv[1], nullptr, 16);
    wprintf(L"handle: %d\n", ownerHandle);

    static const auto SCRATCH_WINDOW_CLASS = L"scratch_window_class";
    WNDCLASSEXW scratchClass{ 0 };
    scratchClass.cbSize = sizeof(WNDCLASSEXW);
    scratchClass.style = CS_HREDRAW | CS_VREDRAW | /*CS_PARENTDC |*/ CS_DBLCLKS;
    scratchClass.lpszClassName = SCRATCH_WINDOW_CLASS;
    scratchClass.lpfnWndProc = s_ScratchWindowProc;
    scratchClass.cbWndExtra = GWL_CONSOLE_WNDALLOC; // this is required to store the owning thread/process override in NTUSER
    auto windowClassAtom{ RegisterClassExW(&scratchClass) };

    // Create a window
    const auto style = WS_VISIBLE |
                       (ownerHandle == 0 ? WS_OVERLAPPEDWINDOW : WS_THICKFRAME | WS_CAPTION | WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

    HWND hwnd = CreateWindowExW(0, // WS_EX_LAYERED,
                                reinterpret_cast<LPCWSTR>(windowClassAtom),
                                L"Hello World",
                                style,
                                100, // CW_USEDEFAULT,
                                100, // CW_USEDEFAULT,
                                100, // CW_USEDEFAULT,
                                100, // CW_USEDEFAULT,
                                reinterpret_cast<HWND>(ownerHandle), // owner
                                nullptr,
                                nullptr,
                                nullptr); // gwl
    if (hwnd == nullptr)
    {
        const auto gle = GetLastError();
        wprintf(L"Failed to create window: %d\n", gle);

        return gle;
    }

    setupHooks(reinterpret_cast<HWND>(ownerHandle), hwnd);

    // You know, if you just resize the Terminal window here, you actually do
    // get a hole in the Terminal where this window should be. It just doesn't
    // actually paint the child window.

    // pump messages
    MSG msg = { 0 };
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
