// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// This application allows you to monitor the text buffer contents of ConPTY.
// All you need to do is run this application in Windows Terminal and it will pop up a window.

#include "pch.h"

#include <wil/win32_helpers.h>

#pragma warning(disable : 4100) // '...': unreferenced formal parameter

// WS_OVERLAPPEDWINDOW without WS_THICKFRAME, which disables resize by the user.
static constexpr DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

static CONSOLE_SCREEN_BUFFER_INFOEX g_info{ .cbSize = sizeof(g_info) };
static std::vector<CHAR_INFO> g_buffer;
static COORD g_bufferSize;
static COORD g_cellCount;

static wil::unique_hfont g_font;
static SIZE g_cellSize;
static LONG g_dpi;

static std::vector<wchar_t> g_text;
static std::vector<INT> g_textAdvance;

static bool equalCoord(const COORD& a, const COORD& b) noexcept
{
    return memcmp(&a, &b, sizeof(COORD)) == 0;
}

static void updateFont(HWND hwnd, LONG dpi)
{
    const LOGFONTW lf{
        .lfHeight = -MulDiv(10, dpi, 72),
        .lfWeight = FW_REGULAR,
        .lfCharSet = DEFAULT_CHARSET,
        .lfQuality = PROOF_QUALITY,
        .lfPitchAndFamily = FIXED_PITCH | FF_MODERN,
        .lfFaceName = L"Consolas",
    };
    g_font = wil::unique_hfont{ CreateFontIndirectW(&lf) };
    g_dpi = dpi;

    const auto dc = wil::GetDC(hwnd);
    const auto restoreFont = wil::SelectObject(dc.get(), g_font.get());
    GetTextExtentPoint32W(dc.get(), L"0", 1, &g_cellSize);
}

static void updateWindowSize(HWND hwnd)
{
    RECT windowArea{ 0, 0, g_cellSize.cx * g_cellCount.X, g_cellSize.cy * g_cellCount.Y };
    AdjustWindowRectExForDpi(&windowArea, windowStyle, FALSE, 0, g_dpi);
    SetWindowPos(hwnd, nullptr, 0, 0, windowArea.right - windowArea.left, windowArea.bottom - windowArea.top, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);
}

static void updateConsoleState(HWND hwnd)
{
    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleScreenBufferInfoEx(out, &g_info))
    {
        PostQuitMessage(0);
        return;
    }

    // Add some extra columns/rows just in case the window is being resized
    // in-between GetConsoleScreenBufferInfoEx and ReadConsoleOutputW.
    const COORD bufferSize{ g_info.dwSize.X + 10, g_info.dwSize.Y + 10 };
    if (!equalCoord(g_bufferSize, bufferSize))
    {
        g_bufferSize = bufferSize;
        g_buffer.resize(static_cast<size_t>(g_bufferSize.X) * g_bufferSize.Y);
    }

    SMALL_RECT readArea{ .Right = bufferSize.X, .Bottom = bufferSize.Y };
    if (!ReadConsoleOutputW(out, g_buffer.data(), bufferSize, {}, &readArea))
    {
        PostQuitMessage(0);
        return;
    }

    const COORD cellCount{ readArea.Right + 1, readArea.Bottom + 1 };
    if (!equalCoord(g_cellCount, cellCount))
    {
        g_cellCount = cellCount;
        updateWindowSize(hwnd);
    }
}

static void paintConsole(HWND hwnd)
{
    const auto dc = wil::BeginPaint(hwnd);
    const auto restoreFont = wil::SelectObject(dc.get(), g_font.get());

    if (g_buffer.empty())
    {
        return;
    }

    for (SHORT y = 0; y < g_cellCount.Y; y++)
    {
        const auto ciOffset = g_bufferSize.X * y;

        for (SHORT x = 0, x2; x < g_cellCount.X; x = x2)
        {
            const auto& ci = g_buffer[ciOffset + x];
            const auto fg = g_info.ColorTable[(ci.Attributes >> 0) & 15];
            const auto bg = g_info.ColorTable[(ci.Attributes >> 4) & 15];

            g_text.clear();
            g_textAdvance.clear();
            g_text.push_back(ci.Char.UnicodeChar);
            g_textAdvance.push_back(g_cellSize.cx);

            // Accumulate characters and advance widths until either the foreground or background
            // color changes. It also handles joining wide glyphs in a somewhat poor manner.
            for (x2 = x + 1; x2 < g_cellCount.X; x2++)
            {
                const auto& ci2 = g_buffer[ciOffset + x2];
                const auto fg2 = g_info.ColorTable[(ci2.Attributes >> 0) & 15];
                const auto bg2 = g_info.ColorTable[(ci2.Attributes >> 4) & 15];

                if (fg2 != fg || bg2 != bg)
                {
                    break;
                }

                if (ci2.Attributes & COMMON_LVB_TRAILING_BYTE)
                {
                    g_textAdvance.back() *= 2;
                }
                else
                {
                    g_text.push_back(ci2.Char.UnicodeChar);
                    g_textAdvance.push_back(g_cellSize.cx);
                }
            }

            RECT r;
            r.left = g_cellSize.cx * x;
            r.top = g_cellSize.cy * y;
            r.right = g_cellSize.cx * x2;
            r.bottom = r.top + g_cellSize.cy;

            SetTextColor(dc.get(), fg);
            SetBkColor(dc.get(), bg);
            ExtTextOutW(dc.get(), r.left, r.top, ETO_CLIPPED, &r, g_text.data(), static_cast<UINT>(g_text.size()), g_textAdvance.data());
        }
    }

    RECT cursorRect;
    cursorRect.left = g_info.dwCursorPosition.X * g_cellSize.cx;
    cursorRect.top = g_info.dwCursorPosition.Y * g_cellSize.cy;
    cursorRect.right = cursorRect.left + g_dpi / 96;
    cursorRect.bottom = cursorRect.top + g_cellSize.cy;
    FillRect(dc.get(), &cursorRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
}

LRESULT WndProc(HWND hwnd, UINT message, size_t wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DPICHANGED:
        updateFont(hwnd, LOWORD(wParam));
        updateWindowSize(hwnd);
        return 0;
    case WM_PAINT:
        paintConsole(hwnd);
        return 0;
    case WM_TIMER:
        updateConsoleState(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

static void winMainImpl(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        MessageBoxW(nullptr, L"This application needs to be spawned from within a console session.", L"Failure", MB_ICONWARNING | MB_OK);
        return;
    }

    const WNDCLASSEXW wc{
        .cbSize = sizeof(wc),
        .style = CS_OWNDC,
        .lpfnWndProc = WndProc,
        .hInstance = hInstance,
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .lpszClassName = L"ConsoleMonitor",
    };
    THROW_LAST_ERROR_IF(!RegisterClassExW(&wc));

    const wil::unique_hwnd hwnd{
        THROW_LAST_ERROR_IF_NULL(CreateWindowExW(
            0,
            wc.lpszClassName,
            L"ConsoleMonitor",
            windowStyle,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            nullptr,
            nullptr,
            wc.hInstance,
            nullptr))
    };

    updateFont(hwnd.get(), static_cast<LONG>(GetDpiForWindow(hwnd.get())));
    updateConsoleState(hwnd.get());

    ShowWindow(hwnd.get(), SW_SHOWNORMAL);
    UpdateWindow(hwnd.get());

    SetTimer(hwnd.get(), 0, 30, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    try
    {
        winMainImpl(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
    }
    catch (const wil::ResultException& e)
    {
        wchar_t message[2048];
        wil::GetFailureLogString(&message[0], 2048, e.GetFailureInfo());
        MessageBoxW(nullptr, &message[0], L"Exception", MB_ICONERROR | MB_OK);
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }

    return 0;
}
