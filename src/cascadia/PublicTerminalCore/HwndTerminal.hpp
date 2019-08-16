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

extern "C"
{
	__declspec(dllexport) HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal);
	__declspec(dllexport) void _stdcall SendTerminalOutput(void* terminal, LPCWSTR data);
	__declspec(dllexport) void _stdcall RegisterScrollCallback(void* terminal, void __stdcall callback(int, int, int));
	__declspec(dllexport) HRESULT _stdcall TriggerResize(void* terminal, double width, double height, _Out_ int* charColumns, _Out_ int* charRows);
	__declspec(dllexport) void _stdcall DpiChanged(void* terminal, int newDpi);
	__declspec(dllexport) void _stdcall UserScroll(void* terminal, int viewTop);
	__declspec(dllexport) HRESULT _stdcall StartSelection(void* terminal, COORD cursorPosition, bool altPressed);
	__declspec(dllexport) HRESULT _stdcall MoveSelection(void* terminal, COORD cursorPosition);
	__declspec(dllexport) void _stdcall ClearSelection(void* terminal);
	__declspec(dllexport) const wchar_t* _stdcall GetSelection(void* terminal);
	__declspec(dllexport) bool _stdcall IsSelectionActive(void* terminal);
	__declspec(dllexport) void _stdcall DestroyTerminal(void* terminal);
	__declspec(dllexport) void _stdcall SetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi);
	__declspec(dllexport) void _stdcall RegisterWriteCallback(void* terminal, void __stdcall callback(wchar_t*));
	__declspec(dllexport) void _stdcall SendKeyEvent(void* terminal, WPARAM wParam);
	__declspec(dllexport) void _stdcall SendCharEvent(void* terminal, char16_t ch);

};

struct HwndTerminal {
public:
    HwndTerminal(HWND hwnd);
    HRESULT Initialize();
    void SendOutput(std::wstring_view data);
    HRESULT Refresh(double width, double height, _Out_ int* charColumns, _Out_ int* charRows);
    void RegisterScrollCallback(std::function<void (int, int, int)> callback);
    void RegisterWriteCallback(void _stdcall callback(wchar_t*));

private:
    wil::unique_hwnd _hwnd;
    FontInfoDesired _desiredFont;
    FontInfo _actualFont;

    std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;

    std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
    std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;

    friend HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal);
    friend void _stdcall DpiChanged(void* terminal, int newDpi);
    friend void _stdcall UserScroll(void* terminal, int viewTop);
    friend HRESULT _stdcall StartSelection(void* terminal, COORD cursorPosition, bool altPressed);
    friend HRESULT _stdcall MoveSelection(void* terminal, COORD cursorPosition);
    friend void _stdcall ClearSelection(void* terminal);
    friend const wchar_t* _stdcall GetSelection(void* terminal);
    friend bool _stdcall IsSelectionActive(void* terminal);
    friend void _stdcall SendKeyEvent(void* terminal, WPARAM wParam);
    friend void _stdcall SendCharEvent(void* terminal, char16_t ch);
    friend void _stdcall SetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi);
    void _UpdateFont(int newDpi);
};
