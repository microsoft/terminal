// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Validates CONSOLE_GRAPHICS_BUFFER support (see #246, "Bringing back the
// console graphics screen-buffers?") end to end from a plain client, with
// no NTVDM involved: creates a palette-indexed (8bpp) and a true-color
// (32bpp) graphics buffer, draws directly into the shared bitmap memory
// CreateConsoleScreenBuffer hands back, and exercises
// SetConsoleActiveScreenBuffer / InvalidateConsoleDIBits / SetConsolePalette
// the way any client could - CONSOLE_GRAPHICS_BUFFER isn't gated to NTVDM;
// #246 itself mentions e.g. Far Manager plugins wanting to show image
// thumbnails in-console as another use case.
//
// CreateConsoleScreenBuffer's CONSOLE_GRAPHICS_BUFFER flag and the
// CONSOLE_GRAPHICS_BUFFER_INFO structure it takes are still supported by
// kernel32.dll but aren't declared in the public Windows SDK headers this
// tool otherwise builds against, so they're declared locally below.
// InvalidateConsoleDIBits/SetConsolePalette are likewise still exported by
// kernel32.dll by name, but not present in the public import library, so
// they're resolved dynamically via GetProcAddress instead of being linked
// against directly - which doubles as a reasonably realistic example of how
// a third-party client would actually have to consume this today.
//
// Usage: graphicsbuffertest.exe

#include <windows.h>
#include <conio.h>
#include <cstdio>
#include <vector>

namespace
{
    constexpr DWORD CONSOLE_GRAPHICS_BUFFER = 2;

    struct CONSOLE_GRAPHICS_BUFFER_INFO
    {
        DWORD dwBitMapInfoLength;
        LPBITMAPINFO lpBitMapInfo;
        DWORD dwUsage;
        HANDLE hMutex;
        PVOID lpBitMap;
    };

    using PFN_InvalidateConsoleDIBits = BOOL(WINAPI*)(HANDLE, PSMALL_RECT);
    using PFN_SetConsolePalette = BOOL(WINAPI*)(HANDLE, HPALETTE, UINT);

    PFN_InvalidateConsoleDIBits pInvalidateConsoleDIBits;
    PFN_SetConsolePalette pSetConsolePalette;

    HANDLE g_hText;

    [[noreturn]] void Fail(const wchar_t* what)
    {
        SetConsoleActiveScreenBuffer(g_hText);
        wprintf(L"\n%s failed: %lu\n", what, GetLastError());
        _getch();
        ExitProcess(1);
    }

    // Switches to the text buffer to show a prompt, then waits for a key -
    // for use before/between graphics buffers exist, or once one is done
    // with and about to be torn down.
    void PromptOnText(const wchar_t* prompt)
    {
        SetConsoleActiveScreenBuffer(g_hText);
        wprintf(L"\n%s\n", prompt);
        _getch();
    }

    // Waits for a key without switching away from whichever buffer is
    // currently active, so a just-drawn graphics buffer stays visible while
    // waiting - printing the prompt would corrupt the pixel content, so it
    // goes to the window title instead.
    void WaitOnGraphics(const wchar_t* prompt)
    {
        SetConsoleTitleW(prompt);
        _getch();
    }

    // Builds a top-down, uncompressed BITMAPINFO for the given dimensions
    // and bit depth. For bitCount <= 8 the color table is a DIB_PAL_COLORS
    // index array (one WORD per slot, mapping slot N to palette entry N).
    std::vector<BYTE> MakeBitmapInfo(LONG width, LONG height, WORD bitCount)
    {
        const DWORD colorTableEntries = (bitCount <= 8) ? (1u << bitCount) : 0u;
        const DWORD stride = ((static_cast<DWORD>(width) * bitCount + 31) / 32) * 4;

        std::vector<BYTE> buffer(sizeof(BITMAPINFOHEADER) + colorTableEntries * sizeof(WORD));
        auto& header = *reinterpret_cast<BITMAPINFOHEADER*>(buffer.data());
        header.biSize = sizeof(BITMAPINFOHEADER);
        header.biWidth = width;
        header.biHeight = -height;
        header.biPlanes = 1;
        header.biBitCount = bitCount;
        header.biCompression = BI_RGB;
        header.biSizeImage = stride * static_cast<DWORD>(height);

        if (colorTableEntries)
        {
            auto* indices = reinterpret_cast<WORD*>(buffer.data() + sizeof(BITMAPINFOHEADER));
            for (DWORD i = 0; i < colorTableEntries; i++)
            {
                indices[i] = static_cast<WORD>(i);
            }
        }

        return buffer;
    }

    // A 256-entry rainbow ramp, one entry per possible 8bpp pixel value.
    HPALETTE CreateRainbowPalette()
    {
        struct
        {
            LOGPALETTE header;
            PALETTEENTRY moreEntries[255];
        } buffer;

        buffer.header.palVersion = 0x300;
        buffer.header.palNumEntries = 256;

        const auto entries = buffer.header.palPalEntry;
        for (int i = 0; i < 256; i++)
        {
            const auto hue = i * 6.0 / 256.0;
            const auto sector = static_cast<int>(hue) % 6;
            const auto frac = hue - static_cast<int>(hue);
            BYTE r = 0, g = 0, b = 0;
            switch (sector)
            {
            case 0: r = 255; g = static_cast<BYTE>(frac * 255); b = 0; break;
            case 1: r = static_cast<BYTE>((1 - frac) * 255); g = 255; b = 0; break;
            case 2: r = 0; g = 255; b = static_cast<BYTE>(frac * 255); break;
            case 3: r = 0; g = static_cast<BYTE>((1 - frac) * 255); b = 255; break;
            case 4: r = static_cast<BYTE>(frac * 255); g = 0; b = 255; break;
            case 5: r = 255; g = 0; b = static_cast<BYTE>((1 - frac) * 255); break;
            }
            entries[i] = PALETTEENTRY{ r, g, b, PC_NOCOLLAPSE };
        }

        return CreatePalette(&buffer.header);
    }

    void TestPalettizedGraphicsBuffer()
    {
        PromptOnText(L"Press any key to test an 8bpp, palette-indexed CONSOLE_GRAPHICS_BUFFER (diagonal color-bar pattern)...");

        constexpr LONG width = 320, height = 200;
        auto bitmapInfo = MakeBitmapInfo(width, height, 8);
        const DWORD stride = ((static_cast<DWORD>(width) * 8 + 31) / 32) * 4;

        CONSOLE_GRAPHICS_BUFFER_INFO info{};
        info.dwBitMapInfoLength = static_cast<DWORD>(bitmapInfo.size());
        info.lpBitMapInfo = reinterpret_cast<LPBITMAPINFO>(bitmapInfo.data());
        info.dwUsage = DIB_PAL_COLORS;

        const auto hBuf = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                     nullptr,
                                                     CONSOLE_GRAPHICS_BUFFER,
                                                     &info);
        if (hBuf == INVALID_HANDLE_VALUE)
        {
            Fail(L"CreateConsoleScreenBuffer(CONSOLE_GRAPHICS_BUFFER, 8bpp)");
        }

        auto* pixels = static_cast<BYTE*>(info.lpBitMap);
        for (LONG y = 0; y < height; y++)
        {
            for (LONG x = 0; x < width; x++)
            {
                pixels[y * stride + x] = static_cast<BYTE>((x + y) & 0xFF);
            }
        }

        const auto hPalette = CreateRainbowPalette();
        if (!hPalette || !pSetConsolePalette(hBuf, hPalette, SYSPAL_STATIC))
        {
            Fail(L"SetConsolePalette");
        }

        if (!SetConsoleActiveScreenBuffer(hBuf))
        {
            Fail(L"SetConsoleActiveScreenBuffer(8bpp buffer)");
        }

        SMALL_RECT full{ 0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1) };
        if (!pInvalidateConsoleDIBits(hBuf, &full))
        {
            Fail(L"InvalidateConsoleDIBits(full rect)");
        }

        WaitOnGraphics(L"8bpp color bars showing - press any key to redraw the top-left quadrant only");

        for (LONG y = 0; y < height / 2; y++)
        {
            for (LONG x = 0; x < width / 2; x++)
            {
                pixels[y * stride + x] = static_cast<BYTE>(255 - ((x + y) & 0xFF));
            }
        }
        SMALL_RECT quadrant{ 0, 0, static_cast<SHORT>(width / 2 - 1), static_cast<SHORT>(height / 2 - 1) };
        pInvalidateConsoleDIBits(hBuf, &quadrant);

        WaitOnGraphics(L"Top-left quadrant redrawn (inverted) - rest unchanged if dirty-rect works - press any key to continue");

        PromptOnText(L"8bpp palette test done - press any key to move on to the 32bpp true-color test.");

        DeleteObject(hPalette);
        CloseHandle(hBuf);
    }

    void TestTrueColorGraphicsBuffer()
    {
        PromptOnText(L"Press any key to test a 32bpp true-color CONSOLE_GRAPHICS_BUFFER (RGB gradient, no palette)...");

        constexpr LONG width = 320, height = 200;
        auto bitmapInfo = MakeBitmapInfo(width, height, 32);

        CONSOLE_GRAPHICS_BUFFER_INFO info{};
        info.dwBitMapInfoLength = static_cast<DWORD>(bitmapInfo.size());
        info.lpBitMapInfo = reinterpret_cast<LPBITMAPINFO>(bitmapInfo.data());
        info.dwUsage = DIB_RGB_COLORS;

        const auto hBuf = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                     nullptr,
                                                     CONSOLE_GRAPHICS_BUFFER,
                                                     &info);
        if (hBuf == INVALID_HANDLE_VALUE)
        {
            Fail(L"CreateConsoleScreenBuffer(CONSOLE_GRAPHICS_BUFFER, 32bpp)");
        }

        // 32bpp is always naturally 4-byte-per-pixel with no row padding, so
        // a DWORD* stride of exactly `width` needs no separate stride calc.
        auto* pixels = static_cast<DWORD*>(info.lpBitMap);
        for (LONG y = 0; y < height; y++)
        {
            for (LONG x = 0; x < width; x++)
            {
                const auto r = static_cast<BYTE>(x * 255 / width);
                const auto g = static_cast<BYTE>(y * 255 / height);
                const auto b = static_cast<BYTE>(255 - (x * 255 / width));
                pixels[y * width + x] = (static_cast<DWORD>(r) << 16) | (static_cast<DWORD>(g) << 8) | b;
            }
        }

        if (!SetConsoleActiveScreenBuffer(hBuf))
        {
            Fail(L"SetConsoleActiveScreenBuffer(32bpp buffer)");
        }

        SMALL_RECT full{ 0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1) };
        if (!pInvalidateConsoleDIBits(hBuf, &full))
        {
            Fail(L"InvalidateConsoleDIBits(full rect)");
        }

        WaitOnGraphics(L"32bpp RGB gradient showing - press any key to finish");

        SetConsoleActiveScreenBuffer(g_hText);
        CloseHandle(hBuf);
    }
}

int __cdecl wmain()
{
    const auto hKernel32 = GetModuleHandleW(L"kernel32.dll");
    pInvalidateConsoleDIBits = reinterpret_cast<PFN_InvalidateConsoleDIBits>(GetProcAddress(hKernel32, "InvalidateConsoleDIBits"));
    pSetConsolePalette = reinterpret_cast<PFN_SetConsolePalette>(GetProcAddress(hKernel32, "SetConsolePalette"));
    if (!pInvalidateConsoleDIBits || !pSetConsolePalette)
    {
        wprintf(L"kernel32.dll doesn't export InvalidateConsoleDIBits/SetConsolePalette on this system.\n");
        _getch();
        return 1;
    }

    g_hText = GetStdHandle(STD_OUTPUT_HANDLE);

    wprintf(L"CONSOLE_GRAPHICS_BUFFER validation tool\n"
            L"========================================\n"
            L"Exercises CreateConsoleScreenBuffer/SetConsoleActiveScreenBuffer/\n"
            L"InvalidateConsoleDIBits/SetConsolePalette directly, without NTVDM.\n");

    TestPalettizedGraphicsBuffer();
    TestTrueColorGraphicsBuffer();

    SetConsoleActiveScreenBuffer(g_hText);
    wprintf(L"\nAll tests completed.\n");
    _getch();
    return 0;
}
