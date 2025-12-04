// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Keep in sync with TerminalTheme.cs
typedef struct _TerminalTheme
{
    COLORREF DefaultBackground;
    COLORREF DefaultForeground;
    COLORREF DefaultSelectionBackground;
    uint32_t CursorStyle; // This will be converted to DispatchTypes::CursorStyle (size_t), but C# cannot marshal an enum type and have it fit in a size_t.
    COLORREF ColorTable[16];
} TerminalTheme;

using PTERM = void*;
using PSCROLLCB = void(_stdcall*)(int, int, int);
using PWRITECB = void(_stdcall*)(const wchar_t*);

#define TERMINAL_API_TABLE(XX)                                                                                 \
    XX(SendOutput, LPCWSTR, data)                                                                              \
    XX(RegisterScrollCallback, PSCROLLCB, callback)                                                            \
    XX(TriggerResize, _In_ til::CoordType, width, _In_ til::CoordType, height, _Out_ til::size*, dimensions)   \
    XX(TriggerResizeWithDimension, _In_ til::size, dimensions, _Out_ til::size*, dimensionsInPixels)           \
    XX(CalculateResize, _In_ til::CoordType, width, _In_ til::CoordType, height, _Out_ til::size*, dimensions) \
    XX(DpiChanged, int, newDpi)                                                                                \
    XX(UserScroll, int, viewTop)                                                                               \
    XX(GetSelection, const wchar_t**, out)                                                                     \
    XX(IsSelectionActive, bool*, out)                                                                          \
    XX(SetTheme, TerminalTheme, theme, LPCWSTR, fontFamily, til::CoordType, fontSize, int, newDpi)             \
    XX(RegisterWriteCallback, PWRITECB, callback)                                                              \
    XX(SendKeyEvent, WORD, vkey, WORD, scanCode, WORD, flags, bool, keyDown)                                   \
    XX(SendCharEvent, wchar_t, ch, WORD, flags, WORD, scanCode)                                                \
    XX(SetCursorVisible, const bool, visible)

extern "C" {
#define API_NAME(name) Terminal##name
#define GENERATOR_0(name) \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM);
#define GENERATOR_1(name, t1, a1) \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM, t1);
#define GENERATOR_2(name, t1, a1, t2, a2) \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM, t1, t2);
#define GENERATOR_3(name, t1, a1, t2, a2, t3, a3) \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM, t1, t2, t3);
#define GENERATOR_4(name, t1, a1, t2, a2, t3, a3, t4, a4) \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM, t1, t2, t3, t4);
#define GENERATOR_N(name, t1, a1, t2, a2, t3, a3, t4, a4, MACRO, ...) MACRO
#define GENERATOR(...)                                                                                                                            \
    GENERATOR_N(__VA_ARGS__, GENERATOR_4, GENERATOR_4, GENERATOR_3, GENERATOR_3, GENERATOR_2, GENERATOR_2, GENERATOR_1, GENERATOR_1, GENERATOR_0) \
    (__VA_ARGS__)
TERMINAL_API_TABLE(GENERATOR)
#undef GENERATOR
#undef GENERATOR_0
#undef GENERATOR_1
#undef GENERATOR_2
#undef GENERATOR_3
#undef GENERATOR_4
#undef GENERATOR_N
#undef API_NAME

__declspec(dllexport) HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ PTERM* terminal);
__declspec(dllexport) void _stdcall DestroyTerminal(PTERM terminal);
};
