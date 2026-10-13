// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Reproduces a three-step console resize sequence some clients use to
// safely resize a console window/buffer pair in either direction (a window
// can never be made bigger than the current buffer, and a buffer can never
// be made smaller than the current window, so the safe order is: shrink the
// window first if needed, resize the buffer, then grow the window back up
// if the requested size is bigger than what it was shrunk to):
//
//   1. SetConsoleWindowInfo()       - shrink the window if it's currently
//                                     bigger than the requested size
//   2. SetConsoleScreenBufferSize() - resize the buffer to the requested size
//   3. SetConsoleWindowInfo()       - grow the window back up to the
//                                     requested size, but only if step 1
//                                     didn't already get it there
//
// Step 3 is skipped whenever the requested size doesn't exceed the window's
// size from before this sequence started. That's exactly the case that
// exposes the bug this tool validates: after step 2, the window's outer
// pixel rect can be left holding pixel space that was reserved for a scroll
// bar which is no longer needed, and without step 3 nothing else
// re-evaluates that rect (see ResizeScreenBuffer's PostUpdateWindowSize()
// call in src/host/screenInfo.cpp).
//
// Usage: resizewindowtest.exe [width height]   (defaults to 80x25)
//
// After resizing, the whole buffer is filled with a solid color and the
// resulting buffer/window/max-window sizes (from GetConsoleScreenBufferInfo)
// are written to the window title and to resizewindowtest.log next to the
// executable. Watch the right and bottom edges of the window: any strip of
// unpainted background left between the filled content and the window
// frame indicates the bug - even though the log will show the buffer and
// window character dimensions matching exactly.

#include <windows.h>
#include <conio.h>
#include <cstdio>
#include <cstdlib>

namespace
{
    HANDLE g_hOut;
    FILE* g_log;

    void WindowSize(int w, int h, int top, int left)
    {
        SMALL_RECT rect;
        rect.Top = static_cast<SHORT>(top);
        rect.Left = static_cast<SHORT>(left);
        rect.Bottom = static_cast<SHORT>(top + h - 1);
        rect.Right = static_cast<SHORT>(left + w - 1);

        if (!SetConsoleWindowInfo(g_hOut, TRUE, &rect))
        {
            fprintf(g_log, "SetConsoleWindowInfo(%d,%d @ %d,%d) FAILED: %lu\n", w, h, top, left, GetLastError());
        }
        else
        {
            fprintf(g_log, "SetConsoleWindowInfo(%d,%d @ %d,%d) OK\n", w, h, top, left);
        }
    }

    void BufferSize(int w, int h)
    {
        COORD size;
        size.X = static_cast<SHORT>(w);
        size.Y = static_cast<SHORT>(h);

        if (!SetConsoleScreenBufferSize(g_hOut, size))
        {
            fprintf(g_log, "SetConsoleScreenBufferSize(%d,%d) FAILED: %lu\n", w, h, GetLastError());
        }
        else
        {
            fprintf(g_log, "SetConsoleScreenBufferSize(%d,%d) OK\n", w, h);
        }
    }

    // The shrink/resize/grow sequence described at the top of this file.
    void ResizeWindow(int w, int h)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(g_hOut, &info))
        {
            fprintf(g_log, "GetConsoleScreenBufferInfo (1) FAILED: %lu\n", GetLastError());
            return;
        }

        const auto oldTop = info.srWindow.Top;
        const auto oldLeft = info.srWindow.Left;
        const auto oldWindowWidth = info.srWindow.Right - info.srWindow.Left + 1;
        const auto oldWindowHeight = info.srWindow.Bottom - info.srWindow.Top + 1;

        fprintf(g_log, "ResizeWindow(%d,%d): oldWindow=%dx%d oldBuffer=%dx%d\n",
                w, h, oldWindowWidth, oldWindowHeight, info.dwSize.X, info.dwSize.Y);

        // 1. Shrink the window first if it's bigger than the target in
        //    either dimension - a buffer can never be resized smaller than
        //    the window currently displaying it.
        if (info.srWindow.Bottom >= h || info.srWindow.Right >= w)
        {
            WindowSize(min(w, oldWindowWidth), min(h, oldWindowHeight), 0, 0);
        }

        // 2. Resize the buffer to the target size.
        BufferSize(w, h);

        if (!GetConsoleScreenBufferInfo(g_hOut, &info))
        {
            fprintf(g_log, "GetConsoleScreenBufferInfo (2) FAILED: %lu\n", GetLastError());
            return;
        }
        fprintf(g_log, "  dwMaximumWindowSize = %dx%d\n", info.dwMaximumWindowSize.X, info.dwMaximumWindowSize.Y);

        auto newLeft = w - info.dwMaximumWindowSize.X;
        if (newLeft > 0)
        {
            w = info.dwMaximumWindowSize.X;
        }
        else
        {
            newLeft = 0;
        }

        auto newTop = h - info.dwMaximumWindowSize.Y;
        if (newTop > 0)
        {
            h = info.dwMaximumWindowSize.Y;
        }
        else
        {
            newTop = 0;
        }

        // 3. Grow the window back up to the target size, only if step 1
        //    didn't already get it there. This is the step that's skipped
        //    when the requested size doesn't exceed the window's original
        //    size - the case this tool is meant to exercise.
        if (w > oldWindowWidth || h > oldWindowHeight)
        {
            WindowSize(w, h, min(newTop, oldTop), min(newLeft, oldLeft));
        }
    }

    void FillAndReport(int requestedWidth, int requestedHeight)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(g_hOut, &info);

        const COORD origin{ 0, 0 };
        const DWORD cellCount = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
        const WORD attr = BACKGROUND_BLUE | BACKGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        DWORD written;

        FillConsoleOutputCharacterW(g_hOut, L' ', cellCount, origin, &written);
        FillConsoleOutputAttribute(g_hOut, attr, cellCount, origin, &written);

        char title[256];
        sprintf_s(title,
                  "req=%dx%d buffer=%dx%d window=%dx%d (col %d-%d, row %d-%d) maxWin=%dx%d",
                  requestedWidth, requestedHeight,
                  info.dwSize.X, info.dwSize.Y,
                  info.srWindow.Right - info.srWindow.Left + 1,
                  info.srWindow.Bottom - info.srWindow.Top + 1,
                  info.srWindow.Left, info.srWindow.Right,
                  info.srWindow.Top, info.srWindow.Bottom,
                  info.dwMaximumWindowSize.X, info.dwMaximumWindowSize.Y);
        SetConsoleTitleA(title);
        fprintf(g_log, "%s\n", title);

        const char* msg = "resizewindowtest - see title bar / resizewindowtest.log - press any key";
        WriteConsoleOutputCharacterA(g_hOut, msg, static_cast<DWORD>(strlen(msg)), origin, &written);
    }
}

int __cdecl wmain(int argc, WCHAR* argv[])
{
    int width = 80;
    int height = 25;
    if (argc >= 3)
    {
        width = _wtoi(argv[1]);
        height = _wtoi(argv[2]);
    }

    g_log = fopen("resizewindowtest.log", "w");
    if (!g_log)
    {
        g_log = stderr;
    }

    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hOut == INVALID_HANDLE_VALUE || g_hOut == nullptr)
    {
        fprintf(g_log, "GetStdHandle failed: %lu\n", GetLastError());
        return 1;
    }

    fprintf(g_log, "Resizing to %d x %d...\n", width, height);
    ResizeWindow(width, height);

    FillAndReport(width, height);
    fflush(g_log);

    _getch();
    return 0;
}
