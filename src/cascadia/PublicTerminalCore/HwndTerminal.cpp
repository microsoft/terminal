// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HwndTerminal.hpp"
#include <DefaultSettings.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../types/viewport.cpp"
#include "../../types/inc/GlyphWidth.hpp"

using namespace ::Microsoft::Terminal::Core;

static LPCWSTR term_window_class = L"HwndTerminalClass";

static LRESULT CALLBACK HwndTerminalWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam) noexcept
{
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static bool RegisterTermClass(HINSTANCE hInstance) noexcept
{
    WNDCLASSW wc;
    if (GetClassInfoW(hInstance, term_window_class, &wc))
    {
        return true;
    }

    wc.style = 0;
    wc.lpfnWndProc = HwndTerminalWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = term_window_class;

    return RegisterClassW(&wc) != 0;
}

HwndTerminal::HwndTerminal(HWND parentHwnd) :
    _desiredFont{ DEFAULT_FONT_FACE.c_str(), 0, 10, { 0, 14 }, CP_UTF8 },
    _actualFont{ DEFAULT_FONT_FACE.c_str(), 0, 10, { 0, 14 }, CP_UTF8, false }
{
    HINSTANCE hInstance = wil::GetModuleInstanceHandle();

    if (RegisterTermClass(hInstance))
    {
        _hwnd = wil::unique_hwnd(CreateWindowExW(
            0,
            term_window_class,
            nullptr,
            WS_CHILD |
                WS_CLIPCHILDREN |
                WS_CLIPSIBLINGS |
                WS_VISIBLE,
            0,
            0,
            0,
            0,
            parentHwnd,
            nullptr,
            hInstance,
            nullptr));
    }
}

HRESULT HwndTerminal::Initialize()
{
    _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();
    auto renderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
    auto* const localPointerToThread = renderThread.get();
    _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(_terminal.get(), nullptr, 0, std::move(renderThread));
    RETURN_HR_IF_NULL(E_POINTER, localPointerToThread);
    RETURN_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

    auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
    RETURN_IF_FAILED(dxEngine->SetHwnd(_hwnd.get()));
    RETURN_IF_FAILED(dxEngine->Enable());
    _renderer->AddRenderEngine(dxEngine.get());

    const auto pfn = std::bind(&::Microsoft::Console::Render::Renderer::IsGlyphWideByFont, _renderer.get(), std::placeholders::_1);
    SetGlyphWidthFallback(pfn);

    _UpdateFont(USER_DEFAULT_SCREEN_DPI);
    RECT windowRect;
    GetWindowRect(_hwnd.get(), &windowRect);

    const COORD windowSize{ gsl::narrow<short>(windowRect.right - windowRect.left), gsl::narrow<short>(windowRect.bottom - windowRect.top) };

    // Fist set up the dx engine with the window size in pixels.
    // Then, using the font, get the number of characters that can fit.
    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
    RETURN_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

    _renderEngine = std::move(dxEngine);

    _terminal->SetBackgroundCallback([](auto) {});

    _terminal->Create(COORD{ 80, 25 }, 1000, *_renderer);
    _terminal->SetDefaultBackground(RGB(5, 27, 80));
    _terminal->SetDefaultForeground(RGB(255, 255, 255));

    localPointerToThread->EnablePainting();

    return S_OK;
}

void HwndTerminal::RegisterScrollCallback(std::function<void(int, int, int)> callback)
{
    _terminal->SetScrollPositionChangedCallback(callback);
}

void HwndTerminal::RegisterWriteCallback(const void _stdcall callback(wchar_t*))
{
    _terminal->SetWriteInputCallback([=](std::wstring & input) noexcept {
        const wchar_t* text = input.c_str();
        const size_t textChars = wcslen(text) + 1;
        const size_t textBytes = textChars * sizeof(wchar_t);
        wchar_t* callingText = nullptr;

        callingText = static_cast<wchar_t*>(::CoTaskMemAlloc(textBytes));

        if (callingText == nullptr)
        {
            callback(nullptr);
        }
        else
        {
            wcscpy_s(callingText, textChars, text);

            callback(callingText);
        }
    });
}

void HwndTerminal::_UpdateFont(int newDpi)
{
    auto lock = _terminal->LockForWriting();

    // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
    //      actually fail. We need a way to gracefully fallback.
    _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);
}

HRESULT HwndTerminal::Refresh(const SIZE windowSize, _Out_ COORD* dimensions)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, dimensions);

    auto lock = _terminal->LockForWriting();

    RETURN_IF_FAILED(_renderEngine->SetWindowSize(windowSize));

    // Invalidate everything
    _renderer->TriggerRedrawAll();

    // Convert our new dimensions to characters
    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 },
                                                       { gsl::narrow<short>(windowSize.cx), gsl::narrow<short>(windowSize.cy) });
    const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

    // If this function succeeds with S_FALSE, then the terminal didn't
    //      actually change size. No need to notify the connection of this
    //      no-op.
    // TODO: MSFT:20642295 Resizing the buffer will corrupt it
    // I believe we'll need support for CSI 2J, and additionally I think
    //      we're resetting the viewport to the top
    RETURN_IF_FAILED(_terminal->UserResize({ vp.Width(), vp.Height() }));
    dimensions->X = vp.Width();
    dimensions->Y = vp.Height();

    return S_OK;
}

void HwndTerminal::SendOutput(std::wstring_view data)
{
    _terminal->Write(data);
}

HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal)
{
    auto _terminal = std::make_unique<HwndTerminal>(parentHwnd);
    RETURN_IF_FAILED(_terminal->Initialize());

    *hwnd = _terminal->_hwnd.get();
    *terminal = _terminal.release();

    return S_OK;
}

void _stdcall TerminalRegisterScrollCallback(void* terminal, void __stdcall callback(int, int, int))
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->RegisterScrollCallback(callback);
}

void _stdcall TerminalRegisterWriteCallback(void* terminal, const void __stdcall callback(wchar_t*))
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->RegisterWriteCallback(callback);
}

void _stdcall TerminalSendOutput(void* terminal, LPCWSTR data)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->SendOutput(data);
}

HRESULT _stdcall TerminalTriggerResize(void* terminal, double width, double height, _Out_ COORD* dimensions)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);

    const SIZE windowSize{ static_cast<short>(width), static_cast<short>(height) };
    return publicTerminal->Refresh(windowSize, dimensions);
}

void _stdcall TerminalDpiChanged(void* terminal, int newDpi)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_UpdateFont(newDpi);
}

void _stdcall TerminalUserScroll(void* terminal, int viewTop)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->UserScrollViewport(viewTop);
}

HRESULT _stdcall TerminalStartSelection(void* terminal, COORD cursorPosition, bool altPressed)
{
    COORD terminalPosition = { cursorPosition };

    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const auto fontSize = publicTerminal->_actualFont.GetSize();

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.X == 0);
    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.Y == 0);

    terminalPosition.X /= fontSize.X;
    terminalPosition.Y /= fontSize.Y;

    publicTerminal->_terminal->SetSelectionAnchor(terminalPosition);
    publicTerminal->_terminal->SetBoxSelection(altPressed);

    publicTerminal->_renderer->TriggerSelection();

    return S_OK;
}

HRESULT _stdcall TerminalMoveSelection(void* terminal, COORD cursorPosition)
{
    COORD terminalPosition = { cursorPosition };

    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const auto fontSize = publicTerminal->_actualFont.GetSize();

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.X == 0);
    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.Y == 0);

    terminalPosition.X /= fontSize.X;
    terminalPosition.Y /= fontSize.Y;

    publicTerminal->_terminal->SetEndSelectionPosition(terminalPosition);
    publicTerminal->_renderer->TriggerSelection();

    return S_OK;
}

void _stdcall TerminalClearSelection(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->ClearSelection();
}
bool _stdcall TerminalIsSelectionActive(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const bool selectionActive = publicTerminal->_terminal->IsSelectionActive();
    return selectionActive;
}

// Returns the selected text in the terminal.
const wchar_t* _stdcall TerminalGetSelection(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);

    const auto bufferData = publicTerminal->_terminal->RetrieveSelectedTextFromBuffer(false);

    // convert text: vector<string> --> string
    std::wstring selectedText;
    for (const auto& text : bufferData.text)
    {
        selectedText += text;
    }

    auto returnText = wil::make_cotaskmem_string_nothrow(selectedText.c_str());
    TerminalClearSelection(terminal);

    return returnText.release();
}

void _stdcall TerminalSendKeyEvent(void* terminal, WPARAM wParam)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const auto scanCode = MapVirtualKeyW((UINT)wParam, MAPVK_VK_TO_VSC);
    struct KeyModifier
    {
        int vkey;
        ControlKeyStates flags;
    };

    constexpr std::array<KeyModifier, 5> modifiers{ {
        { VK_RMENU, ControlKeyStates::RightAltPressed },
        { VK_LMENU, ControlKeyStates::LeftAltPressed },
        { VK_RCONTROL, ControlKeyStates::RightCtrlPressed },
        { VK_LCONTROL, ControlKeyStates::LeftCtrlPressed },
        { VK_SHIFT, ControlKeyStates::ShiftPressed },
    } };

    ControlKeyStates flags;

    for (const auto& mod : modifiers)
    {
        const auto state = GetKeyState(mod.vkey);
        const auto isDown = state < 0;

        if (isDown)
        {
            flags |= mod.flags;
        }
    }

    publicTerminal->_terminal->SendKeyEvent((WORD)wParam, (WORD)scanCode, flags);
}

void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch)
{
    if (ch == '\t')
    {
        return;
    }

    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->SendCharEvent(ch);
}

void _stdcall DestroyTerminal(void* terminal)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    delete publicTerminal;
}

// Updates the terminal font type, size, color, as well as the background/foreground colors to a specified theme.
void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    {
        auto lock = publicTerminal->_terminal->LockForWriting();

        publicTerminal->_terminal->SetDefaultForeground(theme.DefaultForeground);
        publicTerminal->_terminal->SetDefaultBackground(theme.DefaultBackground);

        // Set the font colors
        for (size_t tableIndex = 0; tableIndex < 16; tableIndex++)
        {
            // It's using gsl::at to check the index is in bounds, but the analyzer still calls this array-to-pointer-decay
            [[gsl::suppress(bounds .3)]] publicTerminal->_terminal->SetColorTableEntry(tableIndex, gsl::at(theme.ColorTable, tableIndex));
        }
    }

    publicTerminal->_terminal->SetCursorStyle(theme.CursorStyle);

    publicTerminal->_desiredFont = { fontFamily, 0, 10, { 0, fontSize }, CP_UTF8 };
    publicTerminal->_UpdateFont(newDpi);

    // When the font changes the terminal dimensions need to be recalculated since the available row and column
    // space will have changed.
    RECT windowRect;
    GetWindowRect(publicTerminal->_hwnd.get(), &windowRect);

    COORD dimensions = {};
    const SIZE windowSize{ windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };
    publicTerminal->Refresh(windowSize, &dimensions);
}

// Resizes the terminal to the specified rows and columns.
HRESULT _stdcall TerminalResize(void* terminal, COORD dimensions)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);

    return publicTerminal->_terminal->UserResize(dimensions);
}

void _stdcall TerminalBlinkCursor(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    if (!publicTerminal->_terminal->IsCursorBlinkingAllowed() && publicTerminal->_terminal->IsCursorVisible())
    {
        return;
    }

    publicTerminal->_terminal->SetCursorVisible(!publicTerminal->_terminal->IsCursorVisible());
}

void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->SetCursorVisible(visible);
}
