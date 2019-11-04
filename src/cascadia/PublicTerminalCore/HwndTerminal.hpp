// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"

using namespace Microsoft::Console::VirtualTerminal;

typedef struct _TerminalTheme
{
    COLORREF DefaultBackground;
    COLORREF DefaultForeground;
    DispatchTypes::CursorStyle CursorStyle;
    COLORREF ColorTable[16];
} TerminalTheme, *LPTerminalTheme;

extern "C" {
__declspec(dllexport) HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal);
__declspec(dllexport) void _stdcall TerminalSendOutput(void* terminal, LPCWSTR data);
__declspec(dllexport) void _stdcall TerminalRegisterScrollCallback(void* terminal, void __stdcall callback(int, int, int));
__declspec(dllexport) HRESULT _stdcall TerminalTriggerResize(void* terminal, double width, double height, _Out_ COORD* dimensions);
__declspec(dllexport) HRESULT _stdcall TerminalResize(void* terminal, COORD dimensions);
__declspec(dllexport) void _stdcall TerminalDpiChanged(void* terminal, int newDpi);
__declspec(dllexport) void _stdcall TerminalUserScroll(void* terminal, int viewTop);
__declspec(dllexport) HRESULT _stdcall TerminalStartSelection(void* terminal, COORD cursorPosition, bool altPressed);
__declspec(dllexport) HRESULT _stdcall TerminalMoveSelection(void* terminal, COORD cursorPosition);
__declspec(dllexport) void _stdcall TerminalClearSelection(void* terminal);
__declspec(dllexport) const wchar_t* _stdcall TerminalGetSelection(void* terminal);
__declspec(dllexport) bool _stdcall TerminalIsSelectionActive(void* terminal);
__declspec(dllexport) void _stdcall DestroyTerminal(void* terminal);
__declspec(dllexport) void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi);
__declspec(dllexport) void _stdcall TerminalRegisterWriteCallback(void* terminal, const void __stdcall callback(wchar_t*));
__declspec(dllexport) void _stdcall TerminalSendKeyEvent(void* terminal, WPARAM wParam);
__declspec(dllexport) void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch);
__declspec(dllexport) void _stdcall TerminalBlinkCursor(void* terminal);
__declspec(dllexport) void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible);
};

struct HwndTerminal
{
public:
    HwndTerminal(HWND hwnd);
    HRESULT Initialize();
    void SendOutput(std::wstring_view data);
    HRESULT Refresh(const SIZE windowSize, _Out_ COORD* dimensions);
    void RegisterScrollCallback(std::function<void(int, int, int)> callback);
    void RegisterWriteCallback(const void _stdcall callback(wchar_t*));

private:
    wil::unique_hwnd _hwnd;
    FontInfoDesired _desiredFont;
    FontInfo _actualFont;

    std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;

    std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
    std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;

    friend HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal);
    friend HRESULT _stdcall TerminalResize(void* terminal, COORD dimensions);
    friend void _stdcall TerminalDpiChanged(void* terminal, int newDpi);
    friend void _stdcall TerminalUserScroll(void* terminal, int viewTop);
    friend HRESULT _stdcall TerminalStartSelection(void* terminal, COORD cursorPosition, bool altPressed);
    friend HRESULT _stdcall TerminalMoveSelection(void* terminal, COORD cursorPosition);
    friend void _stdcall TerminalClearSelection(void* terminal);
    friend const wchar_t* _stdcall TerminalGetSelection(void* terminal);
    friend bool _stdcall TerminalIsSelectionActive(void* terminal);
    friend void _stdcall TerminalSendKeyEvent(void* terminal, WPARAM wParam);
    friend void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch);
    friend void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi);
    friend void _stdcall TerminalBlinkCursor(void* terminal);
    friend void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible);
    void _UpdateFont(int newDpi);
};
