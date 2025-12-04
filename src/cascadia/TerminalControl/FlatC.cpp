#include "pch.h"
#include "FlatC.h"
#include "winrt/Microsoft.Terminal.Control.h"
#include "winrt/Microsoft.Terminal.Core.h"
#include "../TerminalCore/ControlKeyStates.hpp"
#include "../types/inc/colorTable.hpp"
#include "../inc/DefaultSettings.h"
#include "../inc/cppwinrt_utils.h"

#include "ControlCore.h"
#include "ControlInteractivity.h"

#include <windowsx.h>

#pragma warning(disable : 4100)

#define HARDCODED_PROPERTY(type, name, ...) \
    type name() const                       \
    {                                       \
        return type{ __VA_ARGS__ };         \
    }                                       \
    void name(const type&)                  \
    {                                       \
    }

using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Core;
using CKS = ::Microsoft::Terminal::Core::ControlKeyStates;

static CKS getControlKeyState() noexcept
{
    struct KeyModifier
    {
        int vkey;
        CKS flags;
    };

    constexpr std::array<KeyModifier, 5> modifiers{ {
        { VK_RMENU, CKS::RightAltPressed },
        { VK_LMENU, CKS::LeftAltPressed },
        { VK_RCONTROL, CKS::RightCtrlPressed },
        { VK_LCONTROL, CKS::LeftCtrlPressed },
        { VK_SHIFT, CKS::ShiftPressed },
    } };

    CKS flags;

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

static MouseButtonState MouseButtonStateFromWParam(WPARAM wParam)
{
    MouseButtonState state{};
    WI_UpdateFlag(state, MouseButtonState::IsLeftButtonDown, WI_IsFlagSet(wParam, MK_LBUTTON));
    WI_UpdateFlag(state, MouseButtonState::IsMiddleButtonDown, WI_IsFlagSet(wParam, MK_MBUTTON));
    WI_UpdateFlag(state, MouseButtonState::IsRightButtonDown, WI_IsFlagSet(wParam, MK_RBUTTON));
    return state;
}

static Point PointFromLParam(LPARAM lParam)
{
    return { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
}

struct CsBridgeConnection : public winrt::implements<CsBridgeConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
{
    void Initialize(IInspectable x) {}
    void Start() {}
    void WriteInput(winrt::array_view<const char16_t> d)
    {
        if (_pfnWriteCallback)
        {
            _pfnWriteCallback(reinterpret_cast<const wchar_t*>(d.data()));
        }
    }
    void Resize(uint32_t r, uint32_t c) {}
    void Close() {}
    winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept { return winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected; }
    WINRT_CALLBACK(TerminalOutput, winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler);

    TYPED_EVENT(StateChanged, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable);

public:
    HARDCODED_PROPERTY(winrt::guid, SessionId, winrt::guid{});

public:
    PWRITECB _pfnWriteCallback{ nullptr };
    void OriginateOutputFromConnection(const wchar_t* data)
    {
        _TerminalOutputHandlers(winrt::to_hstring(data));
    }
};

struct CsBridgeTerminalSettings : winrt::implements<CsBridgeTerminalSettings, IControlSettings, ICoreSettings, IControlAppearance, ICoreAppearance>
{
    using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    CsBridgeTerminalSettings()
    {
        const auto campbellSpan = Microsoft::Console::Utils::CampbellColorTable();
        std::transform(campbellSpan.begin(), campbellSpan.end(), std::begin(_theme.ColorTable), [](auto&& color) {
            return color;
        });
    }
    ~CsBridgeTerminalSettings() = default;

    winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept
    {
        return til::color{ til::at(_theme.ColorTable, index) };
    }

    til::color DefaultForeground() const
    {
        return _theme.DefaultForeground;
    }
    til::color DefaultBackground() const
    {
        return _theme.DefaultBackground;
    }
    til::color SelectionBackground() const
    {
        return til::color{ _theme.DefaultSelectionBackground };
    }
    winrt::hstring FontFace() const
    {
        return _fontFace;
    }
    float FontSize() const
    {
        return _fontSize;
    }
    winrt::Microsoft::Terminal::Core::CursorStyle CursorShape() const
    {
        return static_cast<winrt::Microsoft::Terminal::Core::CursorStyle>(_theme.CursorStyle);
    }

    void DefaultForeground(const til::color&) {}
    void DefaultBackground(const til::color&) {}
    void SelectionBackground(const til::color&) {}
    void FontFace(const winrt::hstring&) {}
    void FontSize(const float&) {}
    void CursorShape(const winrt::Microsoft::Terminal::Core::CursorStyle&) {}

    HARDCODED_PROPERTY(int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
    HARDCODED_PROPERTY(int32_t, InitialRows, 30);
    HARDCODED_PROPERTY(int32_t, InitialCols, 80);
    HARDCODED_PROPERTY(bool, SnapOnInput, true);
    HARDCODED_PROPERTY(bool, AltGrAliasing, true);
    HARDCODED_PROPERTY(til::color, CursorColor, DEFAULT_CURSOR_COLOR);
    HARDCODED_PROPERTY(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
    HARDCODED_PROPERTY(winrt::hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
    HARDCODED_PROPERTY(bool, CopyOnSelect, false);
    HARDCODED_PROPERTY(bool, InputServiceWarning, true);
    HARDCODED_PROPERTY(bool, FocusFollowMouse, false);
    HARDCODED_PROPERTY(bool, TrimBlockSelection, false);
    HARDCODED_PROPERTY(bool, DetectURLs, true);
    HARDCODED_PROPERTY(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, TabColor, nullptr);
    HARDCODED_PROPERTY(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr);
    HARDCODED_PROPERTY(winrt::hstring, ProfileName);
    HARDCODED_PROPERTY(bool, UseAcrylic, false);
    HARDCODED_PROPERTY(float, Opacity, 1.0);
    HARDCODED_PROPERTY(winrt::hstring, Padding, DEFAULT_PADDING);
    HARDCODED_PROPERTY(winrt::Windows::UI::Text::FontWeight, FontWeight, winrt::Windows::UI::Text::FontWeight{ 400 });
    HARDCODED_PROPERTY(IFontAxesMap, FontAxes);
    HARDCODED_PROPERTY(IFontFeatureMap, FontFeatures);
    HARDCODED_PROPERTY(winrt::hstring, BackgroundImage);
    HARDCODED_PROPERTY(float, BackgroundImageOpacity, 1.0);
    HARDCODED_PROPERTY(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
    HARDCODED_PROPERTY(winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
    HARDCODED_PROPERTY(winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr);
    HARDCODED_PROPERTY(winrt::hstring, Commandline);
    HARDCODED_PROPERTY(winrt::hstring, StartingDirectory);
    HARDCODED_PROPERTY(winrt::hstring, StartingTitle);
    HARDCODED_PROPERTY(bool, SuppressApplicationTitle);
    HARDCODED_PROPERTY(winrt::hstring, EnvironmentVariables);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::ScrollbarState, ScrollState, winrt::Microsoft::Terminal::Control::ScrollbarState::Visible);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale);
    HARDCODED_PROPERTY(bool, RetroTerminalEffect, false);
    HARDCODED_PROPERTY(bool, ForceFullRepaintRendering, false);
    HARDCODED_PROPERTY(bool, SoftwareRendering, false);
    HARDCODED_PROPERTY(bool, ForceVTInput, false);
    HARDCODED_PROPERTY(winrt::hstring, PixelShaderPath);
    HARDCODED_PROPERTY(winrt::hstring, PixelShaderImagePath);
    HARDCODED_PROPERTY(bool, IntenseIsBright);
    HARDCODED_PROPERTY(bool, IntenseIsBold);
    HARDCODED_PROPERTY(bool, ShowMarks);
    HARDCODED_PROPERTY(bool, UseBackgroundImageForWindow);
    HARDCODED_PROPERTY(bool, AutoMarkPrompts);
    HARDCODED_PROPERTY(bool, VtPassthrough);
    HARDCODED_PROPERTY(bool, UseAtlasEngine, false);
    HARDCODED_PROPERTY(AdjustTextMode, AdjustIndistinguishableColors, AdjustTextMode::Never);
    HARDCODED_PROPERTY(bool, RightClickContextMenu, false);
    HARDCODED_PROPERTY(winrt::hstring, CellWidth, L"");
    HARDCODED_PROPERTY(winrt::hstring, CellHeight, L"");
    HARDCODED_PROPERTY(bool, RepositionCursorWithMouse, false);
    HARDCODED_PROPERTY(bool, EnableUnfocusedAcrylic, false);
    HARDCODED_PROPERTY(bool, RainbowSuggestions, false);
    HARDCODED_PROPERTY(bool, AllowVtClipboardWrite, true);
    HARDCODED_PROPERTY(bool, AllowVtChecksumReport, false);
    HARDCODED_PROPERTY(winrt::hstring, AnswerbackMessage, L"");
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle, winrt::Microsoft::Terminal::Control::PathTranslationStyle::None);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::DefaultInputScope, DefaultInputScope, winrt::Microsoft::Terminal::Control::DefaultInputScope::Default);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::TextMeasurement, TextMeasurement, winrt::Microsoft::Terminal::Control::TextMeasurement::Graphemes);
    HARDCODED_PROPERTY(bool, DisablePartialInvalidation, false);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::GraphicsAPI, GraphicsAPI, winrt::Microsoft::Terminal::Control::GraphicsAPI::Automatic);
    HARDCODED_PROPERTY(winrt::Microsoft::Terminal::Control::CopyFormat, CopyFormatting, winrt::Microsoft::Terminal::Control::CopyFormat::All);
    HARDCODED_PROPERTY(bool, EnableColorGlyphs, true);
    HARDCODED_PROPERTY(bool, EnableBuiltinGlyphs, true);
    HARDCODED_PROPERTY(winrt::guid, SessionId, winrt::guid{});

public:
    void SetTheme(TerminalTheme theme, LPCWSTR fontFamily, til::CoordType fontSize, int newDpi)
    {
        _theme = std::move(theme);
        _fontFace = fontFamily;
        _fontSize = static_cast<float>(fontSize);
    }

private:
    TerminalTheme _theme;
    winrt::hstring _fontFace{ L"Cascadia Mono" };
    float _fontSize = 12.0f;
};

struct HwndTerminal
{
    static constexpr LPCWSTR term_window_class = L"HwndTerminalClass";

    static LRESULT CALLBACK HwndTerminalWndProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam) noexcept
    try
    {
#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
        HwndTerminal* terminal = reinterpret_cast<HwndTerminal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (terminal)
        {
            return terminal->WindowProc(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
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

    LRESULT WindowProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam) noexcept
    {
        switch (uMsg)
        {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            SetCapture(_hwnd.get());
            _interactivity->PointerPressed(
                0, // Mouse
                MouseButtonStateFromWParam(wParam),
                uMsg,
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
                getControlKeyState(),
                PointFromLParam(lParam));
            return 0;
        case WM_MOUSEMOVE:
            _interactivity->PointerMoved(
                0, // Mouse
                MouseButtonStateFromWParam(wParam),
                WM_MOUSEMOVE,
                getControlKeyState(),
                PointFromLParam(lParam));
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            _interactivity->PointerReleased(
                0, // Mouse
                MouseButtonStateFromWParam(wParam),
                uMsg,
                getControlKeyState(),
                PointFromLParam(lParam));
            ReleaseCapture();
            return 0;
        case WM_POINTERDOWN:
            if (!IS_POINTER_INCONTACT_WPARAM(wParam))
            {
                break;
            }
            SetCapture(_hwnd.get());
            _interactivity->TouchPressed(PointFromLParam(lParam));
            return 0;
        case WM_POINTERUPDATE:
            if (!IS_POINTER_INCONTACT_WPARAM(wParam))
            {
                break;
            }
            _interactivity->TouchMoved(PointFromLParam(lParam));
            return 0;
        case WM_POINTERUP:
            if (!IS_POINTER_INCONTACT_WPARAM(wParam))
            {
                break;
            }
            _interactivity->TouchReleased();
            ReleaseCapture();
            return 0;
        case WM_MOUSEWHEEL:
            if (_interactivity->MouseWheel(getControlKeyState(), GET_WHEEL_DELTA_WPARAM(wParam), PointFromLParam(lParam), MouseButtonStateFromWParam(wParam)))
            {
                return 0;
            }
            break;
        case WM_SETFOCUS:
            _interactivity->GotFocus();
            _focused = true;
            _core->ApplyAppearance(_focused);
            break;
        case WM_KILLFOCUS:
            _interactivity->LostFocus();
            _focused = true;
            _core->ApplyAppearance(_focused);
            break;
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    wil::unique_hwnd _hwnd;

    HwndTerminal(HWND parentHwnd)
    {
        HINSTANCE hInstance = wil::GetModuleInstanceHandle();

        if (RegisterTermClass(hInstance))
        {
            _hwnd.reset(CreateWindowExW(
                0,
                term_window_class,
                nullptr,
                WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
                0,
                0,
                0,
                0,
                parentHwnd,
                nullptr,
                hInstance,
                nullptr));

#pragma warning(suppress : 26490) // Win32 APIs can only store void*, so we have to use reinterpret_cast
            SetWindowLongPtr(_hwnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }

        _settingsBridge = winrt::make_self<CsBridgeTerminalSettings>();
        _connection = winrt::make_self<CsBridgeConnection>();
        _interactivity = winrt::make_self<implementation::ControlInteractivity>(*_settingsBridge, nullptr, *_connection);
        _core.copy_from(winrt::get_self<implementation::ControlCore>(_interactivity->Core()));

        _core->ScrollPositionChanged({ this, &HwndTerminal::_scrollPositionChanged });
        _interactivity->ScrollPositionChanged({ this, &HwndTerminal::_scrollPositionChanged });
    }

    /*( PUBLIC API )*/
    HRESULT SendOutput(LPCWSTR data)
    {
        _connection->OriginateOutputFromConnection(data);
        return S_OK;
    }

    HRESULT RegisterScrollCallback(PSCROLLCB callback)
    {
        _scrollCallback = callback;
        return S_OK;
    }

    HRESULT TriggerResize(_In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions)
    {
        if (!_initialized)
            return S_FALSE;

        SetWindowPos(_hwnd.get(), nullptr, 0, 0, width, height, 0);

        // **NOTE** The sizes we get here are unscaled ...
        auto dpi = GetDpiForWindow(_hwnd.get());
        float w = static_cast<float>(width * USER_DEFAULT_SCREEN_DPI) / dpi;
        float h = static_cast<float>(height * USER_DEFAULT_SCREEN_DPI) / dpi;
        // ... but ControlCore expects scaled sizes.
        _core->SizeChanged(w, h);

        // TODO(DH): ControlCore has no API that returns the new size in cells
        //wil::assign_to_opt_param(dimensions, /*thing*/);

        return S_OK;
    }

    HRESULT TriggerResizeWithDimension(_In_ til::size dimensions, _Out_ til::size* dimensionsInPixels)
    {
        if (!_initialized)
            return S_FALSE;

        winrt::Windows::Foundation::Size outSizeInPixels;
        _core->ResizeToDimensions(dimensions.width, dimensions.height, outSizeInPixels);
        wil::assign_to_opt_param(dimensionsInPixels, til::size{ til::math::rounding, outSizeInPixels });
        return S_OK;
    }

    HRESULT CalculateResize(_In_ til::CoordType width, _In_ til::CoordType height, _Out_ til::size* dimensions)
    {
        // TODO(DH): It seems weird to have to do this manually.
        auto fontSizeInPx = _core->FontSize();
        wil::assign_to_opt_param(dimensions, til::size{
                                                 static_cast<til::CoordType>(width / fontSizeInPx.Width),
                                                 static_cast<til::CoordType>(height / fontSizeInPx.Height),
                                             });
        return S_OK;
    }

    HRESULT DpiChanged(int newDpi)
    {
        _core->ScaleChanged((float)newDpi / 96.0f);
        return S_OK;
    }

    HRESULT UserScroll(int viewTop)
    {
        _interactivity->UpdateScrollbar(static_cast<float>(viewTop) /* TODO(DH) */);
        return S_OK;
    }

    HRESULT GetSelection(const wchar_t** out)
    {
        auto strings = _core->SelectedText(true);
        auto concatenated = std::accumulate(std::begin(strings), std::end(strings), std::wstring{}, [](auto&& l, auto&& r) {
            return l + r;
        });
        auto returnText = wil::make_cotaskmem_string_nothrow(concatenated.c_str());
        *out = returnText.release();
        return S_OK;
    }

    HRESULT IsSelectionActive(bool* out)
    {
        *out = _core->HasSelection();
        return S_OK;
    }

    HRESULT SetTheme(TerminalTheme theme, LPCWSTR fontFamily, til::CoordType fontSize, int newDpi)
    {
        _settingsBridge->SetTheme(theme, fontFamily, fontSize, newDpi);
        _core->UpdateSettings(*_settingsBridge, nullptr);
        _interactivity->UpdateSettings();
        _core->ScaleChanged((static_cast<float>(newDpi) / USER_DEFAULT_SCREEN_DPI));
        _core->ApplyAppearance(_focused);
        return S_OK;
    }

    HRESULT RegisterWriteCallback(PWRITECB callback)
    {
        _connection->_pfnWriteCallback = callback;
        return S_OK;
    }

    HRESULT SendKeyEvent(WORD vkey, WORD scanCode, WORD flags, bool keyDown)
    {
        _core->TrySendKeyEvent(vkey, scanCode, getControlKeyState(), keyDown);
        return S_OK;
    }

    HRESULT SendCharEvent(wchar_t ch, WORD flags, WORD scanCode)
    {
        _core->SendCharEvent(ch, scanCode, getControlKeyState());
        return S_OK;
    }

    HRESULT SetCursorVisible(const bool visible)
    {
        _core->CursorOn(visible);
        return S_OK;
    }

    void Initialize()
    {
        RECT windowRect;
        GetWindowRect(_hwnd.get(), &windowRect);
        auto dpi = GetDpiForWindow(_hwnd.get());
        // BODGY: the +/-1 is because ControlCore will ignore an Initialize with zero size (oops)
        // because in the old days, TermControl would accidentally try to resize the Swap Chain to 0x0 (oops)
        // and therefore resize the connection to 0x0 (oops)
        _core->InitializeWithHwnd(
            gsl::narrow_cast<float>(windowRect.right - windowRect.left + 1),
            gsl::narrow_cast<float>(windowRect.bottom - windowRect.top + 1),
            (static_cast<float>(dpi) / USER_DEFAULT_SCREEN_DPI),
            reinterpret_cast<uint64_t>(_hwnd.get()));
        _interactivity->Initialize();
        _core->ApplyAppearance(_focused);

        int blinkTime = GetCaretBlinkTime();
        auto animationsEnabled = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0);
        _core->CursorBlinkTime(std::chrono::milliseconds(blinkTime == INFINITE ? 0 : blinkTime));
        _core->VtBlinkEnabled(animationsEnabled);

        _core->EnablePainting();

        _initialized = true;
    }

private:
    winrt::com_ptr<CsBridgeConnection> _connection;
    winrt::com_ptr<CsBridgeTerminalSettings> _settingsBridge;
    winrt::com_ptr<implementation::ControlInteractivity> _interactivity{ nullptr };
    winrt::com_ptr<implementation::ControlCore> _core{ nullptr };
    bool _initialized{ false };
    bool _focused{ false };
    PSCROLLCB _scrollCallback{};

    void _scrollPositionChanged(const winrt::Windows::Foundation::IInspectable& i, const ScrollPositionChangedArgs& update)
    {
        if (_scrollCallback)
        {
            _scrollCallback(update.ViewTop(), update.ViewHeight(), update.BufferSize());
        }
    }
};

__declspec(dllexport) HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ PTERM* terminal)
{
    auto inner = new HwndTerminal{ parentHwnd };
    *terminal = inner;
    *hwnd = inner->_hwnd.get();
    inner->Initialize();
    return S_OK;
}

__declspec(dllexport) void _stdcall DestroyTerminal(PTERM terminal)
{
    delete (HwndTerminal*)terminal;
}

// Generate all of the C->C++ bridge functions.
#define API_NAME(name) Terminal##name
#define GENERATOR_0(name)                                                 \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM terminal) \
    try                                                                   \
    {                                                                     \
        return ((HwndTerminal*)(terminal))->name();                       \
    }                                                                     \
    CATCH_RETURN()
#define GENERATOR_1(name, t1, a1)                                                \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM terminal, t1 a1) \
    try                                                                          \
    {                                                                            \
        return ((HwndTerminal*)(terminal))->name(a1);                            \
    }                                                                            \
    CATCH_RETURN()
#define GENERATOR_2(name, t1, a1, t2, a2)                                               \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM terminal, t1 a1, t2 a2) \
    try                                                                                 \
    {                                                                                   \
        return ((HwndTerminal*)(terminal))->name(a1, a2);                               \
    }                                                                                   \
    CATCH_RETURN()
#define GENERATOR_3(name, t1, a1, t2, a2, t3, a3)                                              \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM terminal, t1 a1, t2 a2, t3 a3) \
    try                                                                                        \
    {                                                                                          \
        return ((HwndTerminal*)(terminal))->name(a1, a2, a3);                                  \
    }                                                                                          \
    CATCH_RETURN()
#define GENERATOR_4(name, t1, a1, t2, a2, t3, a3, t4, a4)                                             \
    __declspec(dllexport) HRESULT _stdcall API_NAME(name)(PTERM terminal, t1 a1, t2 a2, t3 a3, t4 a4) \
    try                                                                                               \
    {                                                                                                 \
        return ((HwndTerminal*)(terminal))->name(a1, a2, a3, a4);                                     \
    }                                                                                                 \
    CATCH_RETURN()
#define GENERATOR_N(name, t1, a1, t2, a2, t3, a3, t4, a4, MACRO, ...) MACRO
#define GENERATOR(...)                                                                                                                            \
    GENERATOR_N(__VA_ARGS__, GENERATOR_4, GENERATOR_4, GENERATOR_3, GENERATOR_3, GENERATOR_2, GENERATOR_2, GENERATOR_1, GENERATOR_1, GENERATOR_0) \
    (__VA_ARGS__)
TERMINAL_API_TABLE(GENERATOR)
#undef GENERATOR_0
#undef GENERATOR_1
#undef GENERATOR_2
#undef GENERATOR_3
#undef GENERATOR_4
#undef GENERATOR_N
#undef GENERATOR

#undef API_NAME
