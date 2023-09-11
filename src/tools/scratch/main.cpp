// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <string>

// Used by window structures to place our special frozen-console painting data
#define GWL_CONSOLE_WNDALLOC (3 * sizeof(DWORD))

HANDLE g_stdOut{ nullptr };
LRESULT CALLBACK s_ExtensionWindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

HWND g_parent;
HWND g_child;
HWND g_extension;

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

        // Resize the g_extension to match
        SetWindowPos(g_extension, nullptr, 0, 0, width, height, SWP_NOZORDER);
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
void resizeChildToMatchParent(HWND parent, HWND child)
{
    // get the position of the parent window
    RECT rect;
    GetWindowRect(parent, &rect);

    // resize the child window to match
    // (leave a 48px margin around the child window, for debug)
    SetWindowPos(child,
                 nullptr,
                 rect.left + 48,
                 rect.top + 48,
                 rect.right - rect.left - 96,
                 rect.bottom - rect.top - 96,
                 /* SWP_NOZORDER |  */ SWP_NOACTIVATE);
}

void setupHooks(HWND parent, HWND child)
{
    // get pid and tid for parent window
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(parent, &pid);
    g_parent = parent;
    g_child = child;

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
            // wprintf(L"Got a location change\n");
            resizeChildToMatchParent(g_parent, g_child);
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
           HWND hwnd,
           LONG /* idObject */,
           LONG /* idChild */,
           DWORD /* dwEventThread */,
           DWORD /* dwmsEventTime */) {
            // n.b. not sure this is ever actually called

            if (hwnd == g_parent)
            {
                wprintf(L"Got a reorder change\n");
                // just bring us to the top
                SetWindowPos(g_child, g_parent /* HWND_TOP */, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
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
    // const auto style = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
    const auto style = 0; // WS_VISIBLE | WS_OVERLAPPED; // no resize border, no caption, etc.

    HWND hwnd = CreateWindowExW(0, // WS_EX_LAYERED,
                                reinterpret_cast<LPCWSTR>(windowClassAtom),
                                L"Hello World",
                                style,
                                200, // CW_USEDEFAULT,
                                200, // CW_USEDEFAULT,
                                200, // CW_USEDEFAULT,
                                200, // CW_USEDEFAULT,
                                0, // owner
                                nullptr,
                                nullptr,
                                nullptr); // gwl
    if (hwnd == nullptr)
    {
        const auto gle = GetLastError();
        wprintf(L"Failed to create window: %d\n", gle);

        return gle;
    }

    SetWindowLong(hwnd, GWL_STYLE, 0); //remove all window styles, cause it's created with WS_CAPTION even if we didn't ask for it
    ShowWindow(hwnd, SW_SHOW); //display window

    // Add hooks so that we can be informed when the child process gets resized.
    setupHooks(reinterpret_cast<HWND>(ownerHandle), hwnd);
    // Immediately resize to the right size
    resizeChildToMatchParent(g_parent, g_child);
    // Set our HWND as owned by the parent's. We're always on top of them, but not explicitly a _child_.
    ::SetWindowLongPtrW(g_child, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(g_parent));

    // Now, create a child window of this first hwnd.
    // This is the one that will be the actual extension's HWND.

    static const auto EXTN_WINDOW_CLASS = L"my_extension_window_class";
    WNDCLASSEXW extClass{ 0 };
    extClass.cbSize = sizeof(WNDCLASSEXW);
    extClass.style = CS_HREDRAW | CS_VREDRAW | /*CS_PARENTDC |*/ CS_DBLCLKS;
    extClass.lpszClassName = EXTN_WINDOW_CLASS;
    extClass.lpfnWndProc = s_ExtensionWindowProc;
    extClass.cbWndExtra = GWL_CONSOLE_WNDALLOC; // this is required to store the owning thread/process override in NTUSER
    auto extnClassAtom{ RegisterClassExW(&extClass) };
    const auto extensionStyle = WS_VISIBLE | WS_CHILD;
    const auto extensionStyleEx = 0;
    g_extension = CreateWindowExW(extensionStyleEx, // WS_EX_LAYERED,
                                  reinterpret_cast<LPCWSTR>(extnClassAtom),
                                  L"Hello Extension",
                                  extensionStyle,
                                  0, // CW_USEDEFAULT,
                                  0, // CW_USEDEFAULT,
                                  50, // CW_USEDEFAULT,
                                  50, // CW_USEDEFAULT,
                                  hwnd, // owner
                                  nullptr,
                                  nullptr,
                                  nullptr);

    // pump messages
    MSG msg = { 0 };
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

//TODO! much later:
// * The Terminal doesn't think it's active, cause it's HWND... isn't. The extension's is

HBRUSH g_magentaBrush;

LRESULT CALLBACK s_ExtensionWindowProc(HWND hWnd,
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

        // Create a magenta brush for later
        g_magentaBrush = CreateSolidBrush(RGB(255, 0, 255));
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
        // FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        FillRect(hdc, &ps.rcPaint, g_magentaBrush);

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
