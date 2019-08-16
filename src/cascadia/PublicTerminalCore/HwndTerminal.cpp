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

LRESULT CALLBACK HwndTerminalWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HwndTerminal::HwndTerminal(HWND parentHwnd) :
    _desiredFont{ DEFAULT_FONT_FACE.c_str(), 0, 10, { 0, 14 }, CP_UTF8 },
    _actualFont{ DEFAULT_FONT_FACE.c_str(), 0, 10, { 0, 14 }, CP_UTF8, false }
{
    WNDCLASSW wc;
    HINSTANCE hInstance = wil::GetModuleInstanceHandle();

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

    bool registered = RegisterClassW(&wc) != 0;

    if (registered)
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
            50,
            50,
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

    RETURN_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

    auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
    RETURN_IF_FAILED(dxEngine->SetHwnd(_hwnd.get()));
    RETURN_IF_FAILED(dxEngine->Enable());
    _renderer->AddRenderEngine(dxEngine.get());

    auto pfn = std::bind(&::Microsoft::Console::Render::Renderer::IsGlyphWideByFont, _renderer.get(), std::placeholders::_1);
    SetGlyphWidthFallback(pfn);

    _UpdateFont(USER_DEFAULT_SCREEN_DPI);
    RECT windowRect;
    GetWindowRect(_hwnd.get(), &windowRect);

    const COORD windowSize{ static_cast<short>(windowRect.right - windowRect.left), static_cast<short>(windowRect.bottom - windowRect.top) };

    // Fist set up the dx engine with the window size in pixels.
    // Then, using the font, get the number of characters that can fit.
    // Resize our terminal connection to match that size, and initialize the terminal with that size.
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

void HwndTerminal::RegisterWriteCallback(void _stdcall callback(wchar_t*))
{
    _terminal->SetWriteInputCallback([=](std::wstring& input) {
        const wchar_t* text = input.c_str();
        size_t textChars = wcslen(text) + 1;
        size_t textBytes = textChars * sizeof(wchar_t);
        wchar_t* callingText = NULL;

        callingText = (wchar_t*)::CoTaskMemAlloc(textBytes);

        if (callingText == nullptr)
        {
            callback(nullptr);
        }

        wcscpy_s(callingText, textChars, text);

        callback(callingText);
    });
}

void HwndTerminal::_UpdateFont(int newDpi)
{
    auto lock = _terminal->LockForWriting();

    // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
    //      actually fail. We need a way to gracefully fallback.
    _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);
}

HRESULT HwndTerminal::Refresh(double width, double height, _Out_ int* charColumns, _Out_ int* charRows)
{
    auto lock = _terminal->LockForWriting();
    const SIZE windowSize{ static_cast<short>(width), static_cast<short>(height) };
    RETURN_IF_FAILED(_renderEngine->SetWindowSize(windowSize));

    // Invalidate everything
    _renderer->TriggerRedrawAll();

    // Convert our new dimensions to characters
    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 },
                                                       { static_cast<short>(windowSize.cx), static_cast<short>(windowSize.cy) });
    const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

    // If this function succeeds with S_FALSE, then the terminal didn't
    //      actually change size. No need to notify the connection of this
    //      no-op.
    // TODO: MSFT:20642295 Resizing the buffer will corrupt it
    // I believe we'll need support for CSI 2J, and additionally I think
    //      we're resetting the viewport to the top
    RETURN_IF_FAILED(_terminal->UserResize({ vp.Width(), vp.Height() }));
    *charColumns = vp.Width();
    *charRows = vp.Height();

    return S_OK;
}

void HwndTerminal::SendOutput(std::wstring_view data)
{
    _terminal->Write(data);
}

HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal)
{
    auto _terminal = new HwndTerminal(parentHwnd);
    RETURN_IF_FAILED(_terminal->Initialize());

    *terminal = _terminal;
    *hwnd = _terminal->_hwnd.get();

    return S_OK;
}

void _stdcall RegisterScrollCallback(void* terminal, void __stdcall callback(int, int, int))
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->RegisterScrollCallback(callback);
}

void _stdcall RegisterWriteCallback(void* terminal, void __stdcall callback(wchar_t*))
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->RegisterWriteCallback(callback);
}

void _stdcall SendTerminalOutput(void* terminal, LPCWSTR data)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->SendOutput(data);
}

HRESULT _stdcall TriggerResize(void* terminal, double width, double height, _Out_ int* charColumns, _Out_ int* charRows)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    return publicTerminal->Refresh(width, height, charColumns, charRows);
}

void _stdcall DpiChanged(void* terminal, int newDpi)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->_UpdateFont(newDpi);
}

void _stdcall UserScroll(void* terminal, int viewTop)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->_terminal->UserScrollViewport(viewTop);
}

HRESULT _stdcall StartSelection(void* terminal, COORD cursorPosition, bool altPressed)
{
    COORD terminalPosition = {
        cursorPosition.X,
        cursorPosition.Y
    };

    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
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

HRESULT _stdcall MoveSelection(void* terminal, COORD cursorPosition)
{
    COORD terminalPosition = {
        cursorPosition.X,
        cursorPosition.Y
    };

    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    const auto fontSize = publicTerminal->_actualFont.GetSize();

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.X == 0);
    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.Y == 0);

    terminalPosition.X /= fontSize.X;
    terminalPosition.Y /= fontSize.Y;

    publicTerminal->_terminal->SetEndSelectionPosition(terminalPosition);
    publicTerminal->_renderer->TriggerSelection();

    return S_OK;
}

void _stdcall ClearSelection(void* terminal)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->_terminal->ClearSelection();
}
bool _stdcall IsSelectionActive(void* terminal)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    bool selectionActive = publicTerminal->_terminal->IsSelectionActive();
    return selectionActive;
}

// Copies the selected text into the clipboard.
const wchar_t* _stdcall GetSelection(void* terminal)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);

    std::wstring selectedText = publicTerminal->_terminal->RetrieveSelectedTextFromBuffer(false);

    const wchar_t* text = selectedText.c_str();
    size_t textChars = wcslen(text) + 1;
    size_t textBytes = textChars * sizeof(wchar_t);
    wchar_t* returnText = NULL;

    returnText = (wchar_t*)::CoTaskMemAlloc(textBytes);

    if (returnText == nullptr)
    {
        return nullptr;
    }

    try
    {
        wcscpy_s(returnText, textChars, text);
        ClearSelection(terminal);
    }
    catch (...)
    {
        ::CoTaskMemFree(returnText);
        return nullptr;
    }

    return returnText;
}

void _stdcall SendKeyEvent(void* terminal, WPARAM wParam)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);

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

    publicTerminal->_terminal->SendKeyEvent((WORD)wParam, flags);
}

void _stdcall SendCharEvent(void* terminal, char16_t ch)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    publicTerminal->_terminal->SendCharEvent(ch);
}

void _stdcall DestroyTerminal(void* terminal)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    delete publicTerminal;
}

// Updates the terminal font type, size, color, as well as the background/foreground colors to a specified theme.
void _stdcall SetTheme(void* terminal, LPTerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi)
{
    auto publicTerminal = reinterpret_cast<HwndTerminal*>(terminal);
    {
        auto lock = publicTerminal->_terminal->LockForWriting();

        publicTerminal->_terminal->SetDefaultForeground(theme->DefaultForeground);
        publicTerminal->_terminal->SetDefaultBackground(theme->DefaultBackground);

        // Set the font colors
        for (size_t tableIndex = 0; tableIndex < 16; tableIndex++)
        {
            publicTerminal->_terminal->SetColorTableEntry(tableIndex, theme->ColorTable[tableIndex]);
        }
    }

    publicTerminal->_desiredFont = { fontFamily, 0, 10, { 0, fontSize }, CP_UTF8 };
    publicTerminal->_UpdateFont(newDpi);
}
