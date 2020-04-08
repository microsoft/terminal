// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HwndTerminal.hpp"
#include <windowsx.h>
#include "../../types/TermControlUiaProvider.hpp"
#include <DefaultSettings.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../types/viewport.cpp"
#include "../../types/inc/GlyphWidth.hpp"

using namespace ::Microsoft::Terminal::Core;

static LPCWSTR term_window_class = L"HwndTerminalClass";

LRESULT CALLBACK HwndTerminal::HwndTerminalWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam) noexcept
{
#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
    HwndTerminal* terminal = reinterpret_cast<HwndTerminal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (terminal)
    {
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
        case WM_MOUSEMOVE:
            if (WI_IsFlagSet(wParam, MK_LBUTTON))
            {
                LOG_IF_FAILED(terminal->_MoveSelection(lParam));
                return 0;
            }
            break;
        case WM_RBUTTONDOWN:
            if (terminal->_terminal->IsSelectionActive())
            {
                try
                {
                    const auto bufferData = terminal->_terminal->RetrieveSelectedTextFromBuffer(false);
                    LOG_IF_FAILED(terminal->_CopyTextToSystemClipboard(bufferData, true));
                    terminal->_terminal->ClearSelection();
                }
                CATCH_LOG();
            }
            else
            {
                terminal->_PasteTextFromClipboard();
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

HwndTerminal::HwndTerminal(HWND parentHwnd) :
    _desiredFont{ L"Consolas", 0, 10, { 0, 14 }, CP_UTF8 },
    _actualFont{ L"Consolas", 0, 10, { 0, 14 }, CP_UTF8, false },
    _uiaProvider{ nullptr },
    _uiaProviderInitialized{ false },
    _currentDpi{ USER_DEFAULT_SCREEN_DPI },
    _pfnWriteCallback{ nullptr }
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

#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
        SetWindowLongPtr(_hwnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
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
    _terminal->SetWriteInputCallback([=](std::wstring & input) noexcept { _WriteTextToConnection(input); });
    localPointerToThread->EnablePainting();

    return S_OK;
}

void HwndTerminal::RegisterScrollCallback(std::function<void(int, int, int)> callback)
{
    _terminal->SetScrollPositionChangedCallback(callback);
}

void HwndTerminal::_WriteTextToConnection(const std::wstring& input) noexcept
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

::Microsoft::Console::Types::IUiaData* HwndTerminal::GetUiaData() const noexcept
{
    return _terminal.get();
}

HWND HwndTerminal::GetHwnd() const noexcept
{
    return _hwnd.get();
}

void HwndTerminal::_UpdateFont(int newDpi)
{
    _currentDpi = newDpi;
    auto lock = _terminal->LockForWriting();

    // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
    //      actually fail. We need a way to gracefully fallback.
    _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);
}

IRawElementProviderSimple* HwndTerminal::_GetUiaProvider() noexcept
{
    if (nullptr == _uiaProvider && !_uiaProviderInitialized)
    {
        std::unique_lock<std::shared_mutex> lock;
        try
        {
#pragma warning(suppress : 26441) // The lock is named, this appears to be a false positive
            lock = _terminal->LockForWriting();
            if (_uiaProviderInitialized)
            {
                return _uiaProvider.Get();
            }

            LOG_IF_FAILED(::Microsoft::WRL::MakeAndInitialize<::Microsoft::Terminal::TermControlUiaProvider>(&_uiaProvider, this->GetUiaData(), this));
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
            _uiaProvider = nullptr;
        }
        _uiaProviderInitialized = true;
    }

    return _uiaProvider.Get();
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
    // In order for UIA to hook up properly there needs to be a "static" window hosting the
    // inner win32 control. If the static window is not present then WM_GETOBJECT messages
    // will not reach the child control, and the uia element will not be present in the tree.
    auto _hostWindow = CreateWindowEx(
        0,
        L"static",
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
        nullptr,
        nullptr);
    auto _terminal = std::make_unique<HwndTerminal>(_hostWindow);
    RETURN_IF_FAILED(_terminal->Initialize());

    *hwnd = _hostWindow;
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

    LOG_IF_WIN32_BOOL_FALSE(SetWindowPos(
        publicTerminal->GetHwnd(),
        nullptr,
        0,
        0,
        static_cast<int>(width),
        static_cast<int>(height),
        0));

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

HRESULT HwndTerminal::_StartSelection(LPARAM lParam) noexcept
try
{
    const bool altPressed = GetKeyState(VK_MENU) < 0;
    COORD cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    const auto fontSize = this->_actualFont.GetSize();

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.X == 0);
    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.Y == 0);

    cursorPosition.X /= fontSize.X;
    cursorPosition.Y /= fontSize.Y;

    this->_terminal->SetSelectionAnchor(cursorPosition);
    this->_terminal->SetBlockSelection(altPressed);

    this->_renderer->TriggerSelection();

    return S_OK;
}
CATCH_RETURN();

HRESULT HwndTerminal::_MoveSelection(LPARAM lParam) noexcept
try
{
    COORD cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    const auto fontSize = this->_actualFont.GetSize();

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.X == 0);
    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.Y == 0);

    cursorPosition.X /= fontSize.X;
    cursorPosition.Y /= fontSize.Y;

    this->_terminal->SetSelectionEnd(cursorPosition);
    this->_renderer->TriggerSelection();

    return S_OK;
}
CATCH_RETURN();

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

void _stdcall TerminalSendKeyEvent(void* terminal, WORD vkey, WORD scanCode)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const auto flags = getControlKeyState();
    publicTerminal->_terminal->SendKeyEvent(vkey, scanCode, flags);
}

void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch, WORD scanCode)
{
    if (ch == '\t')
    {
        return;
    }

    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const auto flags = getControlKeyState();
    publicTerminal->_terminal->SendCharEvent(ch, scanCode, flags);
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

    publicTerminal->_terminal->SetCursorOn(!publicTerminal->_terminal->IsCursorOn());
}

void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->SetCursorOn(visible);
}

// Routine Description:
// - Copies the text given onto the global system clipboard.
// Arguments:
// - rows - Rows of text data to copy
// - fAlsoCopyFormatting - true if the color and formatting should also be copied, false otherwise
HRESULT HwndTerminal::_CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, bool const fAlsoCopyFormatting)
{
    std::wstring finalString;

    // Concatenate strings into one giant string to put onto the clipboard.
    for (const auto& str : rows.text)
    {
        finalString += str;
    }

    // allocate the final clipboard data
    const size_t cchNeeded = finalString.size() + 1;
    const size_t cbNeeded = sizeof(wchar_t) * cchNeeded;
    wil::unique_hglobal globalHandle(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbNeeded));
    RETURN_LAST_ERROR_IF_NULL(globalHandle.get());

    PWSTR pwszClipboard = static_cast<PWSTR>(GlobalLock(globalHandle.get()));
    RETURN_LAST_ERROR_IF_NULL(pwszClipboard);

    // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
    // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
    const HRESULT hr = StringCchCopyW(pwszClipboard, cchNeeded, finalString.data());
    GlobalUnlock(globalHandle.get());
    RETURN_IF_FAILED(hr);

    // Set global data to clipboard
    RETURN_LAST_ERROR_IF(!OpenClipboard(_hwnd.get()));

    { // Clipboard Scope
        auto clipboardCloser = wil::scope_exit([]() noexcept {
            LOG_LAST_ERROR_IF(!CloseClipboard());
        });

        RETURN_LAST_ERROR_IF(!EmptyClipboard());
        RETURN_LAST_ERROR_IF_NULL(SetClipboardData(CF_UNICODETEXT, globalHandle.get()));

        if (fAlsoCopyFormatting)
        {
            const auto& fontData = _actualFont;
            int const iFontHeightPoints = fontData.GetUnscaledSize().Y * 72 / this->_currentDpi;
            const COLORREF bgColor = _terminal->GetBackgroundColor(_terminal->GetDefaultBrushColors());

            std::string HTMLToPlaceOnClip = TextBuffer::GenHTML(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor, "Hwnd Console Host");
            _CopyToSystemClipboard(HTMLToPlaceOnClip, L"HTML Format");

            std::string RTFToPlaceOnClip = TextBuffer::GenRTF(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            _CopyToSystemClipboard(RTFToPlaceOnClip, L"Rich Text Format");
        }
    }

    // only free if we failed.
    // the memory has to remain allocated if we successfully placed it on the clipboard.
    // Releasing the smart pointer will leave it allocated as we exit scope.
    globalHandle.release();

    return S_OK;
}

// Routine Description:
// - Copies the given string onto the global system clipboard in the specified format
// Arguments:
// - stringToCopy - The string to copy
// - lpszFormat - the name of the format
HRESULT HwndTerminal::_CopyToSystemClipboard(std::string stringToCopy, LPCWSTR lpszFormat)
{
    const size_t cbData = stringToCopy.size() + 1; // +1 for '\0'
    if (cbData)
    {
        wil::unique_hglobal globalHandleData(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbData));
        RETURN_LAST_ERROR_IF_NULL(globalHandleData.get());

        PSTR pszClipboardHTML = static_cast<PSTR>(GlobalLock(globalHandleData.get()));
        RETURN_LAST_ERROR_IF_NULL(pszClipboardHTML);

        // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
        // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
        const HRESULT hr2 = StringCchCopyA(pszClipboardHTML, cbData, stringToCopy.data());
        GlobalUnlock(globalHandleData.get());
        RETURN_IF_FAILED(hr2);

        UINT const CF_FORMAT = RegisterClipboardFormatW(lpszFormat);
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

    HANDLE ClipboardDataHandle = GetClipboardData(CF_UNICODETEXT);
    if (ClipboardDataHandle == nullptr)
    {
        CloseClipboard();
        return;
    }

    PCWCH pwstr = static_cast<PCWCH>(GlobalLock(ClipboardDataHandle));

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

COORD HwndTerminal::GetFontSize() const
{
    return _actualFont.GetSize();
}

RECT HwndTerminal::GetBounds() const noexcept
{
    RECT windowRect;
    GetWindowRect(_hwnd.get(), &windowRect);
    return windowRect;
}

RECT HwndTerminal::GetPadding() const noexcept
{
    return { 0 };
}

double HwndTerminal::GetScaleFactor() const noexcept
{
    return static_cast<double>(_currentDpi) / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
}

void HwndTerminal::ChangeViewport(const SMALL_RECT NewWindow)
{
    _terminal->UserScrollViewport(NewWindow.Top);
}

HRESULT HwndTerminal::GetHostUiaProvider(IRawElementProviderSimple** provider) noexcept
{
    return UiaHostProviderFromHwnd(_hwnd.get(), provider);
}
