// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../types/IControlAccessibilityInfo.h"
#include "HwndTerminalAutomationPeer.hpp"

using namespace Microsoft::Console::VirtualTerminal;

// Keep in sync with TerminalTheme.cs
typedef struct _TerminalTheme
{
    COLORREF DefaultBackground;
    COLORREF DefaultForeground;
    COLORREF DefaultSelectionBackground;
    float SelectionBackgroundAlpha;
    uint32_t CursorStyle; // This will be converted to DispatchTypes::CursorStyle (size_t), but C# cannot marshal an enum type and have it fit in a size_t.
    COLORREF ColorTable[16];
} TerminalTheme, *LPTerminalTheme;

extern "C" {
__declspec(dllexport) HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal);
__declspec(dllexport) void _stdcall TerminalSendOutput(void* terminal, LPCWSTR data);
__declspec(dllexport) void _stdcall TerminalRegisterScrollCallback(void* terminal, void __stdcall callback(int, int, int));
__declspec(dllexport) HRESULT _stdcall TerminalTriggerResize(_In_ void* terminal, _In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions);
__declspec(dllexport) HRESULT _stdcall TerminalTriggerResizeWithDimension(_In_ void* terminal, _In_ til::size dimensions, _Out_ til::size* dimensionsInPixels);
__declspec(dllexport) HRESULT _stdcall TerminalCalculateResize(_In_ void* terminal, _In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions);
__declspec(dllexport) void _stdcall TerminalDpiChanged(void* terminal, int newDpi);
__declspec(dllexport) void _stdcall TerminalUserScroll(void* terminal, int viewTop);
__declspec(dllexport) void _stdcall TerminalClearSelection(void* terminal);
__declspec(dllexport) const wchar_t* _stdcall TerminalGetSelection(void* terminal);
__declspec(dllexport) bool _stdcall TerminalIsSelectionActive(void* terminal);
__declspec(dllexport) void _stdcall DestroyTerminal(void* terminal);
__declspec(dllexport) void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, til::CoordType fontSize, int newDpi);
__declspec(dllexport) void _stdcall TerminalRegisterWriteCallback(void* terminal, const void __stdcall callback(wchar_t*));
__declspec(dllexport) void _stdcall TerminalSendKeyEvent(void* terminal, WORD vkey, WORD scanCode, WORD flags, bool keyDown);
__declspec(dllexport) void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch, WORD flags, WORD scanCode);
__declspec(dllexport) void _stdcall TerminalBlinkCursor(void* terminal);
__declspec(dllexport) void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible);
__declspec(dllexport) void _stdcall TerminalSetFocus(void* terminal);
__declspec(dllexport) void _stdcall TerminalKillFocus(void* terminal);
};

struct HwndTerminal : ::Microsoft::Console::Types::IControlAccessibilityInfo
{
public:
    HwndTerminal(HWND hwnd) noexcept;

    HwndTerminal(const HwndTerminal&) = default;
    HwndTerminal(HwndTerminal&&) = default;
    HwndTerminal& operator=(const HwndTerminal&) = default;
    HwndTerminal& operator=(HwndTerminal&&) = default;
    ~HwndTerminal();

    HRESULT Initialize();
    void Teardown() noexcept;
    void SendOutput(std::wstring_view data);
    HRESULT Refresh(const til::size windowSize, _Out_ til::size* dimensions);
    void RegisterScrollCallback(std::function<void(int, int, int)> callback);
    void RegisterWriteCallback(const void _stdcall callback(wchar_t*));
    ::Microsoft::Console::Render::IRenderData* GetRenderData() const noexcept;
    HWND GetHwnd() const noexcept;

    static LRESULT CALLBACK HwndTerminalWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept;

private:
    wil::unique_hwnd _hwnd;
    FontInfoDesired _desiredFont;
    FontInfo _actualFont;
    int _currentDpi;
    std::function<void(wchar_t*)> _pfnWriteCallback;
    ::Microsoft::WRL::ComPtr<HwndTerminalAutomationPeer> _uiaProvider;

    std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;

    std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
    std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;
    std::unique_ptr<::Microsoft::Console::Render::UiaEngine> _uiaEngine;

    bool _focused{ false };

    std::chrono::milliseconds _multiClickTime;
    unsigned int _multiClickCounter{};
    std::chrono::steady_clock::time_point _lastMouseClickTimestamp{};
    std::optional<til::point> _lastMouseClickPos;
    std::optional<til::point> _singleClickTouchdownPos;

    friend HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal);
    friend HRESULT _stdcall TerminalTriggerResize(_In_ void* terminal, _In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions);
    friend HRESULT _stdcall TerminalTriggerResizeWithDimension(_In_ void* terminal, _In_ til::size dimensions, _Out_ til::size* dimensionsInPixels);
    friend HRESULT _stdcall TerminalCalculateResize(_In_ void* terminal, _In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions);
    friend void _stdcall TerminalDpiChanged(void* terminal, int newDpi);
    friend void _stdcall TerminalUserScroll(void* terminal, int viewTop);
    friend void _stdcall TerminalClearSelection(void* terminal);
    friend const wchar_t* _stdcall TerminalGetSelection(void* terminal);
    friend bool _stdcall TerminalIsSelectionActive(void* terminal);
    friend void _stdcall TerminalSendKeyEvent(void* terminal, WORD vkey, WORD scanCode, WORD flags, bool keyDown);
    friend void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch, WORD scanCode, WORD flags);
    friend void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, til::CoordType fontSize, int newDpi);
    friend void _stdcall TerminalBlinkCursor(void* terminal);
    friend void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible);
    friend void _stdcall TerminalSetFocus(void* terminal);
    friend void _stdcall TerminalKillFocus(void* terminal);

    void _UpdateFont(int newDpi);
    void _WriteTextToConnection(const std::wstring_view text) noexcept;
    HRESULT _CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, const bool fAlsoCopyFormatting);
    HRESULT _CopyToSystemClipboard(std::string stringToCopy, LPCWSTR lpszFormat);
    void _PasteTextFromClipboard() noexcept;
    void _StringPaste(const wchar_t* const pData) noexcept;

    const unsigned int _NumberOfClicks(til::point clickPos, std::chrono::steady_clock::time_point clickTime) noexcept;
    HRESULT _StartSelection(LPARAM lParam) noexcept;
    HRESULT _MoveSelection(LPARAM lParam) noexcept;
    IRawElementProviderSimple* _GetUiaProvider() noexcept;

    void _ClearSelection() noexcept;

    bool _CanSendVTMouseInput() const noexcept;
    bool _SendMouseEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept;

    void _SendKeyEvent(WORD vkey, WORD scanCode, WORD flags, bool keyDown) noexcept;
    void _SendCharEvent(wchar_t ch, WORD scanCode, WORD flags) noexcept;

    // Inherited via IControlAccessibilityInfo
    til::size GetFontSize() const noexcept override;
    til::rect GetBounds() const noexcept override;
    double GetScaleFactor() const noexcept override;
    void ChangeViewport(const til::inclusive_rect& NewWindow) override;
    HRESULT GetHostUiaProvider(IRawElementProviderSimple** provider) noexcept override;
    til::rect GetPadding() const noexcept override;
};
