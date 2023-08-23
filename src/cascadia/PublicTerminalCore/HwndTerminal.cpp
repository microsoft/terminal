// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HwndTerminal.hpp"
#include <windowsx.h>
#include <DefaultSettings.h>
#include "../../types/viewport.cpp"
#include "../../types/inc/GlyphWidth.hpp"

using namespace ::Microsoft::Terminal::Core;

static LPCWSTR term_window_class = L"HwndTerminalClass";

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };

static constexpr bool _IsMouseMessage(UINT uMsg)
{
    return uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK ||
           uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP || uMsg == WM_MBUTTONDBLCLK ||
           uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP || uMsg == WM_RBUTTONDBLCLK ||
           uMsg == WM_MOUSEMOVE || uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL;
}

LRESULT CALLBACK HwndTerminal::HwndTerminalWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam) noexcept
try
{
    if (WM_NCCREATE == uMsg)
    {
#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        HwndTerminal* that = static_cast<HwndTerminal*>(cs->lpCreateParams);
        that->_hwnd = wil::unique_hwnd(hwnd);

#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
        return DefWindowProc(hwnd, WM_NCCREATE, wParam, lParam);
    }
#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
    auto terminal = reinterpret_cast<HwndTerminal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (terminal)
    {
        if (_IsMouseMessage(uMsg))
        {
            if (terminal->_CanSendVTMouseInput() && terminal->_SendMouseEvent(uMsg, wParam, lParam))
            {
                // GH#6401: Capturing the mouse ensures that we get drag/release events
                // even if the user moves outside the window.
                // _SendMouseEvent returns false if the terminal's not in VT mode, so we'll
                // fall through to release the capture.
                switch (uMsg)
                {
                case WM_LBUTTONDOWN:
                case WM_MBUTTONDOWN:
                case WM_RBUTTONDOWN:
                    SetCapture(hwnd);
                    break;
                case WM_LBUTTONUP:
                case WM_MBUTTONUP:
                case WM_RBUTTONUP:
                    ReleaseCapture();
                    break;
                default:
                    break;
                }

                // Suppress all mouse events that made it into the terminal.
                return 0;
            }
        }

        switch (uMsg)
        {
        case WM_GETOBJECT:
            if (lParam == UiaRootObjectId)
            {
                return UiaReturnRawElementProvider(hwnd, wParam, lParam, terminal->_GetUiaProvider());
            }
            break;
        case WM_LBUTTONDOWN:
            LOG_IF_FAILED(terminal->_StartSelection(lParam));
            return 0;
        case WM_LBUTTONUP:
            terminal->_singleClickTouchdownPos = std::nullopt;
            [[fallthrough]];
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            ReleaseCapture();
            break;
        case WM_MOUSEMOVE:
            if (WI_IsFlagSet(wParam, MK_LBUTTON))
            {
                LOG_IF_FAILED(terminal->_MoveSelection(lParam));
                return 0;
            }
            break;
        case WM_RBUTTONDOWN:
            if (const auto& termCore{ terminal->_terminal }; termCore && termCore->IsSelectionActive())
            {
                try
                {
                    const auto bufferData = termCore->RetrieveSelectedTextFromBuffer(false);
                    LOG_IF_FAILED(terminal->_CopyTextToSystemClipboard(bufferData, true));
                    TerminalClearSelection(terminal);
                }
                CATCH_LOG();
            }
            else
            {
                terminal->_PasteTextFromClipboard();
            }
            return 0;
        case WM_DESTROY:
            // Release Terminal's hwnd so Teardown doesn't try to destroy it again
            terminal->_hwnd.release();
            terminal->Teardown();
            return 0;
        default:
            break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return 0;
}

static bool RegisterTermClass(HINSTANCE hInstance) noexcept
{
    WNDCLASSW wc;
    if (GetClassInfoW(hInstance, term_window_class, &wc))
    {
        return true;
    }

    wc.style = 0;
    wc.lpfnWndProc = HwndTerminal::HwndTerminalWndProc;
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

HwndTerminal::HwndTerminal(HWND parentHwnd) noexcept :
    _desiredFont{ L"Consolas", 0, DEFAULT_FONT_WEIGHT, 14, CP_UTF8 },
    _actualFont{ L"Consolas", 0, DEFAULT_FONT_WEIGHT, { 0, 14 }, CP_UTF8, false },
    _uiaProvider{ nullptr },
    _currentDpi{ USER_DEFAULT_SCREEN_DPI },
    _pfnWriteCallback{ nullptr },
    _multiClickTime{ 500 } // this will be overwritten by the windows system double-click time
{
    auto hInstance = wil::GetModuleInstanceHandle();

    if (RegisterTermClass(hInstance))
    {
        CreateWindowExW(
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
            this);
    }
}

HwndTerminal::~HwndTerminal()
{
    Teardown();
}

HRESULT HwndTerminal::Initialize()
{
    _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();
    auto renderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
    auto* const localPointerToThread = renderThread.get();
    auto& renderSettings = _terminal->GetRenderSettings();
    renderSettings.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, RGB(12, 12, 12));
    renderSettings.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, RGB(204, 204, 204));
    _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(renderSettings, _terminal.get(), nullptr, 0, std::move(renderThread));
    RETURN_HR_IF_NULL(E_POINTER, localPointerToThread);
    RETURN_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

    auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
    RETURN_IF_FAILED(dxEngine->SetHwnd(_hwnd.get()));
    RETURN_IF_FAILED(dxEngine->Enable());
    _renderer->AddRenderEngine(dxEngine.get());

    _UpdateFont(USER_DEFAULT_SCREEN_DPI);
    RECT windowRect;
    GetWindowRect(_hwnd.get(), &windowRect);

    const til::size windowSize{ windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };

    // Fist set up the dx engine with the window size in pixels.
    // Then, using the font, get the number of characters that can fit.
    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
    RETURN_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

    _renderEngine = std::move(dxEngine);

    _terminal->Create({ 80, 25 }, 9001, *_renderer);
    _terminal->SetWriteInputCallback([=](std::wstring_view input) noexcept { _WriteTextToConnection(input); });
    localPointerToThread->EnablePainting();

    _multiClickTime = std::chrono::milliseconds{ GetDoubleClickTime() };

    return S_OK;
}

void HwndTerminal::Teardown() noexcept
try
{
    // As a rule, detach resources from the Terminal before shutting them down.
    // This ensures that teardown is reentrant.

    // Shut down the renderer (and therefore the thread) before we implode
    if (auto localRenderEngine{ std::exchange(_renderEngine, nullptr) })
    {
        if (auto localRenderer{ std::exchange(_renderer, nullptr) })
        {
            localRenderer->TriggerTeardown();
            // renderer is destroyed
        }
        // renderEngine is destroyed
    }

    if (auto localHwnd{ _hwnd.release() })
    {
        // If we're being called through WM_DESTROY, we won't get here (hwnd is already released)
        // If we're not, we may end up in Teardown _again_... but by the time we do, all other
        // resources have been released and will not be released again.
        DestroyWindow(localHwnd);
    }
}
CATCH_LOG();

void HwndTerminal::RegisterScrollCallback(std::function<void(int, int, int)> callback)
{
    if (!_terminal)
    {
        return;
    }
    _terminal->SetScrollPositionChangedCallback(callback);
}

void HwndTerminal::_WriteTextToConnection(const std::wstring_view input) noexcept
{
    if (!_pfnWriteCallback)
    {
        return;
    }

    try
    {
        auto callingText{ wil::make_cotaskmem_string(input.data(), input.size()) };
        _pfnWriteCallback(callingText.release());
    }
    CATCH_LOG();
}

void HwndTerminal::RegisterWriteCallback(const void _stdcall callback(wchar_t*))
{
    _pfnWriteCallback = callback;
}

::Microsoft::Console::Render::IRenderData* HwndTerminal::GetRenderData() const noexcept
{
    return _terminal.get();
}

HWND HwndTerminal::GetHwnd() const noexcept
{
    return _hwnd.get();
}

void HwndTerminal::_UpdateFont(int newDpi)
{
    if (!_terminal)
    {
        return;
    }
    _currentDpi = newDpi;
    auto lock = _terminal->LockForWriting();

    // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
    //      actually fail. We need a way to gracefully fallback.
    _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);
}

IRawElementProviderSimple* HwndTerminal::_GetUiaProvider() noexcept
{
    // If TermControlUiaProvider throws during construction,
    // we don't want to try constructing an instance again and again.
    if (!_uiaProvider)
    {
        try
        {
            if (!_terminal)
            {
                return nullptr;
            }
            auto lock = _terminal->LockForWriting();
            LOG_IF_FAILED(::Microsoft::WRL::MakeAndInitialize<HwndTerminalAutomationPeer>(&_uiaProvider, this->GetRenderData(), this));
            _uiaEngine = std::make_unique<::Microsoft::Console::Render::UiaEngine>(_uiaProvider.Get());
            LOG_IF_FAILED(_uiaEngine->Enable());
            _renderer->AddRenderEngine(_uiaEngine.get());
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
            _uiaProvider = nullptr;
        }
    }

    return _uiaProvider.Get();
}

HRESULT HwndTerminal::Refresh(const til::size windowSize, _Out_ til::size* dimensions)
{
    RETURN_HR_IF_NULL(E_NOT_VALID_STATE, _terminal);
    RETURN_HR_IF_NULL(E_INVALIDARG, dimensions);

    auto lock = _terminal->LockForWriting();

    _terminal->ClearSelection();

    RETURN_IF_FAILED(_renderEngine->SetWindowSize(windowSize));

    // Invalidate everything
    _renderer->TriggerRedrawAll();

    // Convert our new dimensions to characters
    const auto viewInPixels = Viewport::FromDimensions(windowSize);
    const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

    // Guard against resizing the window to 0 columns/rows, which the text buffer classes don't really support.
    auto size = vp.Dimensions();
    size.width = std::max(size.width, 1);
    size.height = std::max(size.height, 1);

    // If this function succeeds with S_FALSE, then the terminal didn't
    //      actually change size. No need to notify the connection of this
    //      no-op.
    // TODO: MSFT:20642295 Resizing the buffer will corrupt it
    // I believe we'll need support for CSI 2J, and additionally I think
    //      we're resetting the viewport to the top
    RETURN_IF_FAILED(_terminal->UserResize(size));
    dimensions->width = size.width;
    dimensions->height = size.height;

    return S_OK;
}

void HwndTerminal::SendOutput(std::wstring_view data)
{
    if (!_terminal)
    {
        return;
    }
    _terminal->Write(data);
}

HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal)
{
    auto _terminal = std::make_unique<HwndTerminal>(parentHwnd);
    RETURN_IF_FAILED(_terminal->Initialize());

    *hwnd = _terminal->GetHwnd();
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

/// <summary>
/// Triggers a terminal resize using the new width and height in pixel.
/// </summary>
/// <param name="terminal">Terminal pointer.</param>
/// <param name="width">New width of the terminal in pixels.</param>
/// <param name="height">New height of the terminal in pixels</param>
/// <param name="dimensions">Out parameter containing the columns and rows that fit the new size.</param>
/// <returns>HRESULT of the attempted resize.</returns>
HRESULT _stdcall TerminalTriggerResize(_In_ void* terminal, _In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);

    LOG_IF_WIN32_BOOL_FALSE(SetWindowPos(
        publicTerminal->GetHwnd(),
        nullptr,
        0,
        0,
        static_cast<int>(width),
        static_cast<int>(height),
        0));

    const til::size windowSize{ width, height };
    return publicTerminal->Refresh(windowSize, dimensions);
}

/// <summary>
/// Helper method for resizing the terminal using character column and row counts
/// </summary>
/// <param name="terminal">Pointer to the terminal object.</param>
/// <param name="dimensionsInCharacters">New terminal size in row and column count.</param>
/// <param name="dimensionsInPixels">Out parameter with the new size of the renderer.</param>
/// <returns>HRESULT of the attempted resize.</returns>
HRESULT _stdcall TerminalTriggerResizeWithDimension(_In_ void* terminal, _In_ til::size dimensionsInCharacters, _Out_ til::size* dimensionsInPixels)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, dimensionsInPixels);

    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);

    const auto viewInCharacters = Viewport::FromDimensions(dimensionsInCharacters);
    const auto viewInPixels = publicTerminal->_renderEngine->GetViewportInPixels(viewInCharacters);

    dimensionsInPixels->width = viewInPixels.Width();
    dimensionsInPixels->height = viewInPixels.Height();

    til::size unused;

    return TerminalTriggerResize(terminal, viewInPixels.Width(), viewInPixels.Height(), &unused);
}

/// <summary>
/// Calculates the amount of rows and columns that fit in the provided width and height.
/// </summary>
/// <param name="terminal">Terminal pointer</param>
/// <param name="width">Width of the terminal area to calculate.</param>
/// <param name="height">Height of the terminal area to calculate.</param>
/// <param name="dimensions">Out parameter containing the columns and rows that fit the new size.</param>
/// <returns>HRESULT of the calculation.</returns>
HRESULT _stdcall TerminalCalculateResize(_In_ void* terminal, _In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);

    const auto viewInPixels = Viewport::FromDimensions({ width, height });
    const auto viewInCharacters = publicTerminal->_renderEngine->GetViewportInCharacters(viewInPixels);

    dimensions->width = viewInCharacters.Width();
    dimensions->height = viewInCharacters.Height();

    return S_OK;
}

void _stdcall TerminalDpiChanged(void* terminal, int newDpi)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_UpdateFont(newDpi);
}

void _stdcall TerminalUserScroll(void* terminal, int viewTop)
{
    if (const auto publicTerminal = static_cast<const HwndTerminal*>(terminal); publicTerminal && publicTerminal->_terminal)
    {
        publicTerminal->_terminal->UserScrollViewport(viewTop);
    }
}

const unsigned int HwndTerminal::_NumberOfClicks(til::point point, std::chrono::steady_clock::time_point timestamp) noexcept
{
    // if click occurred at a different location or past the multiClickTimer...
    const auto delta{ timestamp - _lastMouseClickTimestamp };
    if (point != _lastMouseClickPos || delta > _multiClickTime)
    {
        // exit early. This is a single click.
        _multiClickCounter = 1;
    }
    else
    {
        _multiClickCounter++;
    }
    return _multiClickCounter;
}

HRESULT HwndTerminal::_StartSelection(LPARAM lParam) noexcept
try
{
    RETURN_HR_IF_NULL(E_NOT_VALID_STATE, _terminal);
    const til::point cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    auto lock = _terminal->LockForWriting();
    const auto altPressed = GetKeyState(VK_MENU) < 0;
    const til::size fontSize{ this->_actualFont.GetSize() };

    this->_terminal->SetBlockSelection(altPressed);

    const auto clickCount{ _NumberOfClicks(cursorPosition, std::chrono::steady_clock::now()) };

    // This formula enables the number of clicks to cycle properly between single-, double-, and triple-click.
    // To increase the number of acceptable click states, simply increment MAX_CLICK_COUNT and add another if-statement
    const unsigned int MAX_CLICK_COUNT = 3;
    const auto multiClickMapper = clickCount > MAX_CLICK_COUNT ? ((clickCount + MAX_CLICK_COUNT - 1) % MAX_CLICK_COUNT) + 1 : clickCount;

    if (multiClickMapper == 3)
    {
        _terminal->MultiClickSelection(cursorPosition / fontSize, ::Terminal::SelectionExpansion::Line);
    }
    else if (multiClickMapper == 2)
    {
        _terminal->MultiClickSelection(cursorPosition / fontSize, ::Terminal::SelectionExpansion::Word);
    }
    else
    {
        this->_terminal->ClearSelection();
        _singleClickTouchdownPos = cursorPosition;

        _lastMouseClickTimestamp = std::chrono::steady_clock::now();
        _lastMouseClickPos = cursorPosition;
    }
    this->_renderer->TriggerSelection();

    return S_OK;
}
CATCH_RETURN();

HRESULT HwndTerminal::_MoveSelection(LPARAM lParam) noexcept
try
{
    RETURN_HR_IF_NULL(E_NOT_VALID_STATE, _terminal);
    const til::point cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    auto lock = _terminal->LockForWriting();
    const til::size fontSize{ this->_actualFont.GetSize() };

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.area() == 0); // either dimension = 0, area == 0

    // This is a copy of ControlInteractivity::PointerMoved
    if (_singleClickTouchdownPos)
    {
        const auto touchdownPoint = *_singleClickTouchdownPos;
        const auto dx = cursorPosition.x - touchdownPoint.x;
        const auto dy = cursorPosition.y - touchdownPoint.y;
        const auto w = fontSize.width;
        const auto distanceSquared = dx * dx + dy * dy;
        const auto maxDistanceSquared = w * w / 16; // (w / 4)^2

        if (distanceSquared >= maxDistanceSquared)
        {
            _terminal->SetSelectionAnchor(touchdownPoint / fontSize);
            // stop tracking the touchdown point
            _singleClickTouchdownPos = std::nullopt;
        }
    }

    this->_terminal->SetSelectionEnd(cursorPosition / fontSize);
    this->_renderer->TriggerSelection();

    return S_OK;
}
CATCH_RETURN();

void HwndTerminal::_ClearSelection() noexcept
try
{
    if (!_terminal)
    {
        return;
    }
    auto lock{ _terminal->LockForWriting() };
    _terminal->ClearSelection();
    _renderer->TriggerSelection();
}
CATCH_LOG();

void _stdcall TerminalClearSelection(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_ClearSelection();
}

bool _stdcall TerminalIsSelectionActive(void* terminal)
{
    if (const auto publicTerminal = static_cast<const HwndTerminal*>(terminal); publicTerminal && publicTerminal->_terminal)
    {
        return publicTerminal->_terminal->IsSelectionActive();
    }
    return false;
}

// Returns the selected text in the terminal.
const wchar_t* _stdcall TerminalGetSelection(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    if (!publicTerminal || !publicTerminal->_terminal)
    {
        return nullptr;
    }

    const auto bufferData = publicTerminal->_terminal->RetrieveSelectedTextFromBuffer(false);
    publicTerminal->_ClearSelection();

    // convert text: vector<string> --> string
    std::wstring selectedText;
    for (const auto& text : bufferData.text)
    {
        selectedText += text;
    }

    auto returnText = wil::make_cotaskmem_string_nothrow(selectedText.c_str());
    return returnText.release();
}

static ControlKeyStates getControlKeyState() noexcept
{
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

    return flags;
}

bool HwndTerminal::_CanSendVTMouseInput() const noexcept
{
    // Only allow the transit of mouse events if shift isn't pressed.
    const auto shiftPressed = GetKeyState(VK_SHIFT) < 0;
    return !shiftPressed && _focused && _terminal && _terminal->IsTrackingMouseInput();
}

bool HwndTerminal::_SendMouseEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept
try
{
    if (!_terminal)
    {
        return false;
    }

    til::point cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    const til::size fontSize{ this->_actualFont.GetSize() };
    short wheelDelta{ 0 };
    if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL)
    {
        wheelDelta = HIWORD(wParam);

        // If it's a *WHEEL event, it's in screen coordinates, not window (?!)
        ScreenToClient(_hwnd.get(), cursorPosition.as_win32_point());
    }

    const TerminalInput::MouseButtonState state{
        WI_IsFlagSet(GetKeyState(VK_LBUTTON), KeyPressed),
        WI_IsFlagSet(GetKeyState(VK_MBUTTON), KeyPressed),
        WI_IsFlagSet(GetKeyState(VK_RBUTTON), KeyPressed)
    };

    return _terminal->SendMouseEvent(cursorPosition / fontSize, uMsg, getControlKeyState(), wheelDelta, state);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

void HwndTerminal::_SendKeyEvent(WORD vkey, WORD scanCode, WORD flags, bool keyDown) noexcept
try
{
    if (!_terminal)
    {
        return;
    }

    auto modifiers = getControlKeyState();
    if (WI_IsFlagSet(flags, ENHANCED_KEY))
    {
        modifiers |= ControlKeyStates::EnhancedKey;
    }
    if (vkey && keyDown && _uiaProvider)
    {
        _uiaProvider->RecordKeyEvent(vkey);
    }
    _terminal->SendKeyEvent(vkey, scanCode, modifiers, keyDown);
}
CATCH_LOG();

void HwndTerminal::_SendCharEvent(wchar_t ch, WORD scanCode, WORD flags) noexcept
try
{
    if (!_terminal)
    {
        return;
    }
    else if (_terminal->IsSelectionActive())
    {
        _ClearSelection();
        if (ch == UNICODE_ESC)
        {
            // ESC should clear any selection before it triggers input.
            // Other characters pass through.
            return;
        }
    }

    if (ch == UNICODE_TAB)
    {
        // TAB was handled as a keydown event (cf. Terminal::SendKeyEvent)
        return;
    }

    auto modifiers = getControlKeyState();
    if (WI_IsFlagSet(flags, ENHANCED_KEY))
    {
        modifiers |= ControlKeyStates::EnhancedKey;
    }
    _terminal->SendCharEvent(ch, scanCode, modifiers);
}
CATCH_LOG();

void _stdcall TerminalSendKeyEvent(void* terminal, WORD vkey, WORD scanCode, WORD flags, bool keyDown)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_SendKeyEvent(vkey, scanCode, flags, keyDown);
}

void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch, WORD scanCode, WORD flags)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_SendCharEvent(ch, scanCode, flags);
}

void _stdcall DestroyTerminal(void* terminal)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    delete publicTerminal;
}

// Updates the terminal font type, size, color, as well as the background/foreground colors to a specified theme.
void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, til::CoordType fontSize, int newDpi)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    if (!publicTerminal || !publicTerminal->_terminal)
    {
        return;
    }
    {
        auto lock = publicTerminal->_terminal->LockForWriting();

        auto& renderSettings = publicTerminal->_terminal->GetRenderSettings();
        renderSettings.SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, theme.DefaultForeground);
        renderSettings.SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, theme.DefaultBackground);

        publicTerminal->_renderEngine->SetSelectionBackground(theme.DefaultSelectionBackground, theme.SelectionBackgroundAlpha);

        // Set the font colors
        for (size_t tableIndex = 0; tableIndex < 16; tableIndex++)
        {
            // It's using gsl::at to check the index is in bounds, but the analyzer still calls this array-to-pointer-decay
            [[gsl::suppress(bounds .3)]] renderSettings.SetColorTableEntry(tableIndex, gsl::at(theme.ColorTable, tableIndex));
        }
    }

    publicTerminal->_terminal->SetCursorStyle(static_cast<DispatchTypes::CursorStyle>(theme.CursorStyle));

    publicTerminal->_desiredFont = { fontFamily, 0, DEFAULT_FONT_WEIGHT, static_cast<float>(fontSize), CP_UTF8 };
    publicTerminal->_UpdateFont(newDpi);

    // When the font changes the terminal dimensions need to be recalculated since the available row and column
    // space will have changed.
    RECT windowRect;
    GetWindowRect(publicTerminal->_hwnd.get(), &windowRect);

    til::size dimensions;
    const til::size windowSize{ windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };
    publicTerminal->Refresh(windowSize, &dimensions);
}

void _stdcall TerminalBlinkCursor(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    if (!publicTerminal || !publicTerminal->_terminal || (!publicTerminal->_terminal->IsCursorBlinkingAllowed() && publicTerminal->_terminal->IsCursorVisible()))
    {
        return;
    }

    publicTerminal->_terminal->SetCursorOn(!publicTerminal->_terminal->IsCursorOn());
}

void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    if (!publicTerminal || !publicTerminal->_terminal)
    {
        return;
    }
    publicTerminal->_terminal->SetCursorOn(visible);
}

void __stdcall TerminalSetFocus(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_focused = true;
    if (auto uiaEngine = publicTerminal->_uiaEngine.get())
    {
        LOG_IF_FAILED(uiaEngine->Enable());
    }
}

void __stdcall TerminalKillFocus(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_focused = false;
    if (auto uiaEngine = publicTerminal->_uiaEngine.get())
    {
        LOG_IF_FAILED(uiaEngine->Disable());
    }
}

// Routine Description:
// - Copies the text given onto the global system clipboard.
// Arguments:
// - rows - Rows of text data to copy
// - fAlsoCopyFormatting - true if the color and formatting should also be copied, false otherwise
HRESULT HwndTerminal::_CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, const bool fAlsoCopyFormatting)
try
{
    RETURN_HR_IF_NULL(E_NOT_VALID_STATE, _terminal);
    std::wstring finalString;

    // Concatenate strings into one giant string to put onto the clipboard.
    for (const auto& str : rows.text)
    {
        finalString += str;
    }

    // allocate the final clipboard data
    const auto cchNeeded = finalString.size() + 1;
    const auto cbNeeded = sizeof(wchar_t) * cchNeeded;
    wil::unique_hglobal globalHandle(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbNeeded));
    RETURN_LAST_ERROR_IF_NULL(globalHandle.get());

    auto pwszClipboard = static_cast<PWSTR>(GlobalLock(globalHandle.get()));
    RETURN_LAST_ERROR_IF_NULL(pwszClipboard);

    // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
    // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
    const auto hr = StringCchCopyW(pwszClipboard, cchNeeded, finalString.data());
    GlobalUnlock(globalHandle.get());
    RETURN_IF_FAILED(hr);

    // Set global data to clipboard
    RETURN_LAST_ERROR_IF(!OpenClipboard(_hwnd.get()));

    { // Clipboard Scope
        auto clipboardCloser = wil::scope_exit([]() {
            LOG_LAST_ERROR_IF(!CloseClipboard());
        });

        RETURN_LAST_ERROR_IF(!EmptyClipboard());
        RETURN_LAST_ERROR_IF_NULL(SetClipboardData(CF_UNICODETEXT, globalHandle.get()));

        if (fAlsoCopyFormatting)
        {
            const auto& fontData = _actualFont;
            const int iFontHeightPoints = fontData.GetUnscaledSize().height; // this renderer uses points already
            const auto bgColor = _terminal->GetAttributeColors({}).second;

            auto HTMLToPlaceOnClip = TextBuffer::GenHTML(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            _CopyToSystemClipboard(HTMLToPlaceOnClip, L"HTML Format");

            auto RTFToPlaceOnClip = TextBuffer::GenRTF(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            _CopyToSystemClipboard(RTFToPlaceOnClip, L"Rich Text Format");
        }
    }

    // only free if we failed.
    // the memory has to remain allocated if we successfully placed it on the clipboard.
    // Releasing the smart pointer will leave it allocated as we exit scope.
    globalHandle.release();

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Copies the given string onto the global system clipboard in the specified format
// Arguments:
// - stringToCopy - The string to copy
// - lpszFormat - the name of the format
HRESULT HwndTerminal::_CopyToSystemClipboard(std::string stringToCopy, LPCWSTR lpszFormat)
{
    const auto cbData = stringToCopy.size() + 1; // +1 for '\0'
    if (cbData)
    {
        wil::unique_hglobal globalHandleData(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbData));
        RETURN_LAST_ERROR_IF_NULL(globalHandleData.get());

        auto pszClipboardHTML = static_cast<PSTR>(GlobalLock(globalHandleData.get()));
        RETURN_LAST_ERROR_IF_NULL(pszClipboardHTML);

        // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
        // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
        const auto hr2 = StringCchCopyA(pszClipboardHTML, cbData, stringToCopy.data());
        GlobalUnlock(globalHandleData.get());
        RETURN_IF_FAILED(hr2);

        const auto CF_FORMAT = RegisterClipboardFormatW(lpszFormat);
        RETURN_LAST_ERROR_IF(0 == CF_FORMAT);

        RETURN_LAST_ERROR_IF_NULL(SetClipboardData(CF_FORMAT, globalHandleData.get()));

        // only free if we failed.
        // the memory has to remain allocated if we successfully placed it on the clipboard.
        // Releasing the smart pointer will leave it allocated as we exit scope.
        globalHandleData.release();
    }

    return S_OK;
}

void HwndTerminal::_PasteTextFromClipboard() noexcept
{
    // Get paste data from clipboard
    if (!OpenClipboard(_hwnd.get()))
    {
        return;
    }

    auto ClipboardDataHandle = GetClipboardData(CF_UNICODETEXT);
    if (ClipboardDataHandle == nullptr)
    {
        CloseClipboard();
        return;
    }

    auto pwstr = static_cast<PCWCH>(GlobalLock(ClipboardDataHandle));

    _StringPaste(pwstr);

    GlobalUnlock(ClipboardDataHandle);

    CloseClipboard();
}

void HwndTerminal::_StringPaste(const wchar_t* const pData) noexcept
{
    if (pData == nullptr)
    {
        return;
    }

    try
    {
        std::wstring text(pData);
        _WriteTextToConnection(text);
    }
    CATCH_LOG();
}

til::size HwndTerminal::GetFontSize() const noexcept
{
    return _actualFont.GetSize();
}

til::rect HwndTerminal::GetBounds() const noexcept
{
    til::rect windowRect;
    GetWindowRect(_hwnd.get(), windowRect.as_win32_rect());
    return windowRect;
}

til::rect HwndTerminal::GetPadding() const noexcept
{
    return {};
}

double HwndTerminal::GetScaleFactor() const noexcept
{
    return static_cast<double>(_currentDpi) / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
}

void HwndTerminal::ChangeViewport(const til::inclusive_rect& NewWindow)
{
    if (!_terminal)
    {
        return;
    }
    _terminal->UserScrollViewport(NewWindow.top);
}

HRESULT HwndTerminal::GetHostUiaProvider(IRawElementProviderSimple** provider) noexcept
{
    return UiaHostProviderFromHwnd(_hwnd.get(), provider);
}
