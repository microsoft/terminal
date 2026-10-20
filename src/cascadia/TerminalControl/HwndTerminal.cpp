// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "HwndTerminal.h"
#include "HwndTerminalAutomationPeer.hpp"

#include "winrt/Microsoft.Terminal.Control.h"
#include "winrt/Microsoft.Terminal.Core.h"

#include "../inc/ControlProperties.h"
#include "../inc/cppwinrt_utils.h"
#include "../inc/DefaultSettings.h"
#include "../TerminalCore/ControlKeyStates.hpp"
#include "../types/inc/colorTable.hpp"
#include "../../tsf/Handle.h"

#include "ControlCore.h"
#include "ControlInteractivity.h"

#include <windows.system.h>
#include <windowsx.h>
#include <UIAutomationCoreApi.h>

#pragma warning(disable : 4100)

namespace MT
{
    namespace Control = winrt::Microsoft::Terminal::Control;
    namespace Core = winrt::Microsoft::Terminal::Core;
}

static auto getControlKeyState() noexcept
{
    using ControlKeyStates = ::Microsoft::Terminal::Core::ControlKeyStates;
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

static MT::Control::MouseButtonState MouseButtonStateFromWParam(WPARAM wParam)
{
    MT::Control::MouseButtonState state{};
    WI_UpdateFlag(state, MT::Control::MouseButtonState::IsLeftButtonDown, WI_IsFlagSet(wParam, MK_LBUTTON));
    WI_UpdateFlag(state, MT::Control::MouseButtonState::IsMiddleButtonDown, WI_IsFlagSet(wParam, MK_MBUTTON));
    WI_UpdateFlag(state, MT::Control::MouseButtonState::IsRightButtonDown, WI_IsFlagSet(wParam, MK_RBUTTON));
    return state;
}

static MT::Core::Point PointFromLParam(LPARAM lParam)
{
    return { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
}

struct CallbackBasedConnection : public winrt::implements<CallbackBasedConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
{
    void WriteInput(winrt::array_view<const char16_t> d)
    {
        if (_pfnWriteCallback)
        {
            _pfnWriteCallback(reinterpret_cast<const wchar_t*>(d.data()));
        }
    }

    // Stubs
    void Initialize(const IInspectable&) {}
    void Start() {}
    void Resize(uint32_t r, uint32_t c) {}
    void Close() {}
    winrt::guid SessionId() { return {}; }
    void SessionId(const winrt::guid&) {}

    winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept { return winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected; }

    til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
    til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> StateChanged;

public:
    PWRITECB _pfnWriteCallback{ nullptr };
    void OriginateOutputFromConnection(const wchar_t* data)
    {
        std::wstring_view str{ data };
        TerminalOutput.raise(winrt::array_view<const char16_t>(reinterpret_cast<const char16_t*>(str.data()), static_cast<uint32_t>(str.size())));
    }
};

struct BasicTerminalSettings : winrt::implements<BasicTerminalSettings, MT::Control::IControlSettings, MT::Core::ICoreSettings, MT::Control::IControlAppearance, MT::Core::ICoreAppearance, MT::Core::ICoreScheme>
{
    using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

    BasicTerminalSettings()
    {
        const auto campbellSpan = Microsoft::Console::Utils::CampbellColorTable();
        std::transform(campbellSpan.begin(), campbellSpan.end(), std::begin(_theme.ColorTable), [](auto&& color) {
            return color;
        });
    }

    ~BasicTerminalSettings() = default;

    void GetColorTable(winrt::com_array<::winrt::Microsoft::Terminal::Core::Color>& table) noexcept
    {
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> colorTable{};
        std::transform(&_theme.ColorTable[0], &_theme.ColorTable[16], colorTable.begin(), [](auto&& color) {
            return static_cast<winrt::Microsoft::Terminal::Core::Color>(til::color{ color });
        });

        table = winrt::com_array(colorTable.begin(), colorTable.end());
    }

    til::color DynamicDefaultForeground() const
    {
        return _theme.DefaultForeground;
    }

    til::color DynamicDefaultBackground() const
    {
        return _theme.DefaultBackground;
    }

    til::color DynamicSelectionBackground() const
    {
        return til::color{ _theme.DefaultSelectionBackground };
    }

    winrt::hstring DynamicFontFace() const
    {
        return _fontFace;
    }

    float DynamicFontSize() const
    {
        return _fontSize;
    }

    winrt::Microsoft::Terminal::Core::CursorStyle DynamicCursorShape() const
    {
        return static_cast<winrt::Microsoft::Terminal::Core::CursorStyle>(_theme.CursorStyle);
    }

#define DYNAMIC_PROPERTY(type, name, ...)                                              \
    auto name()                                                                        \
    {                                                                                  \
        return [](auto t) {                                                            \
            if constexpr (std::bool_constant<requires { t->Dynamic##name(); }>::value) \
            {                                                                          \
                return t->Dynamic##name();                                             \
            }                                                                          \
            return type{ __VA_ARGS__ };                                                \
        }(this);                                                                       \
    }

    CONTROL_SETTINGS(DYNAMIC_PROPERTY);
    CORE_SETTINGS(DYNAMIC_PROPERTY);
    CONTROL_APPEARANCE_SETTINGS(DYNAMIC_PROPERTY);
    CORE_APPEARANCE_SETTINGS(DYNAMIC_PROPERTY);
#undef DYNAMIC_PROPERTY

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

struct HwndTerminal : public Microsoft::Console::Types::IControlAccessibilityInfo
{
    static constexpr LPCWSTR term_window_class = L"HwndTerminalClass";

    static LRESULT CALLBACK HwndTerminalWndProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam) noexcept
    try
    {
#pragma warning(push)
#pragma warning(disable : 26490) // Win32 APIs can only store void*/LONG_PTR, have to use reinterpret_cast
        if (uMsg == WM_NCCREATE)
        {
            // Set the GWLP_USERDATA pointer based on the param passed into CreateWindowEx
            const auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            HwndTerminal* that = static_cast<HwndTerminal*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
        }
        else if (HwndTerminal* terminal = reinterpret_cast<HwndTerminal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
        {
            return terminal->WindowProc(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
#pragma warning(pop)
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
        case WM_GETOBJECT:
            if (lParam == UiaRootObjectId)
            {
                return UiaReturnRawElementProvider(hwnd, wParam, lParam, _getUiaProvider());
            }
            break;
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
        case WM_MOUSEHWHEEL:
        {
            winrt::Microsoft::Terminal::Core::Point delta{ 0, GET_WHEEL_DELTA_WPARAM(wParam) };
            if (uMsg == WM_MOUSEHWHEEL)
            {
                std::swap(delta.X, delta.Y);
            }

            if (_interactivity->MouseWheel(getControlKeyState(), delta, PointFromLParam(lParam), MouseButtonStateFromWParam(wParam)))
            {
                return 0;
            }
            break;
        }
        case WM_SETFOCUS:
            if (!_tsfHandle)
            {
                _tsfHandle = Microsoft::Console::TSF::Handle::Create();
                _tsfHandle.AssociateFocus(&_tsfDataProvider);
            }
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

    HwndTerminal(HWND parentHwnd, winrt::Windows::System::DispatcherQueue dispatcher)
    {
        HINSTANCE hInstance = wil::GetModuleInstanceHandle();

        if (RegisterTermClass(hInstance))
        {
            _hwnd.reset(CreateWindowExW(
                WS_EX_NOREDIRECTIONBITMAP,
                term_window_class,
                nullptr,
                WS_CHILD | WS_VISIBLE,
                0,
                0,
                0,
                0,
                parentHwnd,
                nullptr,
                hInstance,
                this));
        }

        _settingsBridge = winrt::make_self<BasicTerminalSettings>();
        _connection = winrt::make_self<CallbackBasedConnection>();
        _interactivity = winrt::make_self<MT::Control::implementation::ControlInteractivity>(*_settingsBridge, nullptr, *_connection, dispatcher);
        _core = _interactivity->Core();

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

        wil::assign_to_opt_param(dimensions, til::size{ _core->ViewportSize() });

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
        auto selection = _core->SelectedText(true);
        auto returnText = wil::make_cotaskmem_string_nothrow(selection.c_str(), selection.size());
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
        if (_tsfHandle)
        {
            // If a composition just ended, its text is still queued up inside TSF. Hand it to
            // the connection now, so that a key the IME passed on to us can't reach the
            // connection before the text it finalized - neither by being forwarded to the PTY,
            // nor by triggering an action bound to it. GH#20244
            _tsfHandle.FlushPendingComposition();

            if (_tsfHandle.HasActiveComposition())
            {
                // While TSF has an active composition, key events must not be forwarded
                // to the PTY directly. The IME re-injects navigation/confirmation keys
                // after composition ends, at which point HasActiveComposition() == false
                // and they are forwarded normally. This mirrors conhost v1 behavior in
                // src/interactivity/win32/windowio.cpp.
                return S_OK;
            }
        }

        _core->TrySendKeyEvent(vkey, scanCode, getControlKeyState(), keyDown);

        if (vkey && keyDown && _uiaProvider)
        {
            _uiaProvider->RecordKeyEvent(vkey);
        }

        return S_OK;
    }

    HRESULT SendCharEvent(wchar_t ch, WORD flags, WORD scanCode)
    {
        _core->SendCharEvent(ch, scanCode, getControlKeyState());
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

        _core->EnablePainting();

        _initialized = true;
    }

#pragma region HwndTerminal Accessibility
    til::size GetFontSize() const noexcept
    {
        if (_core)
        {
            return { til::math::rounding, _core->FontSize() };
        }
        return {};
    }

    til::rect GetBounds() const noexcept
    {
        til::rect windowRect;
        GetWindowRect(_hwnd.get(), windowRect.as_win32_rect());
        return windowRect;
    }

    til::rect GetPadding() const noexcept
    {
        return {};
    }

    void ChangeViewport(const til::inclusive_rect& NewWindow)
    {
        if (!_interactivity)
        {
            return;
        }
        _interactivity->UpdateScrollbar(static_cast<float>(NewWindow.top));
    }

    HRESULT GetHostUiaProvider(IRawElementProviderSimple** provider) noexcept
    {
        return UiaHostProviderFromHwnd(_hwnd.get(), provider);
    }
#pragma endregion

private:
    struct TsfDataProvider : public Microsoft::Console::TSF::IDataProvider
    {
        TsfDataProvider(HwndTerminal* t) :
            _terminal(t) {}
        virtual ~TsfDataProvider() = default;
        STDMETHODIMP QueryInterface(REFIID, void**) noexcept override;
        ULONG STDMETHODCALLTYPE AddRef() noexcept override;
        ULONG STDMETHODCALLTYPE Release() noexcept override;
        HWND GetHwnd() override;
        RECT GetViewport() override;
        RECT GetCursorPosition() override;
        void HandleOutput(std::wstring_view text) override;
        Microsoft::Console::Render::Renderer* GetRenderer() override;
        HwndTerminal* _terminal;
    };

    winrt::com_ptr<CallbackBasedConnection> _connection;
    winrt::com_ptr<BasicTerminalSettings> _settingsBridge;
    PSCROLLCB _scrollCallback{};

    // Keep these ordered as-is for proper destruction.
    winrt::com_ptr<HwndTerminalAutomationPeer> _uiaProvider;
    winrt::com_ptr<MT::Control::implementation::ControlInteractivity> _interactivity{ nullptr };
    winrt::com_ptr<MT::Control::implementation::ControlCore> _core{ nullptr };
    TsfDataProvider _tsfDataProvider{ this };
    Microsoft::Console::TSF::Handle _tsfHandle;

    bool _initialized{ false };
    bool _focused{ false };

    void _scrollPositionChanged(const winrt::Windows::Foundation::IInspectable& i, const MT::Control::ScrollPositionChangedArgs& update)
    {
        if (_scrollCallback)
        {
            _scrollCallback(update.ViewTop(), update.ViewHeight(), update.BufferSize());
        }
    }

    IRawElementProviderSimple* _getUiaProvider() noexcept
    {
        // If TermControlUiaProvider throws during construction,
        // we don't want to try constructing an instance again and again.
        if (!_uiaProvider)
        {
            try
            {
                if (!_core)
                {
                    return nullptr;
                }
                LOG_IF_FAILED(::Microsoft::WRL::MakeAndInitialize<HwndTerminalAutomationPeer>(_uiaProvider.put(), _core->GetRenderData(), this));
                _interactivity->SetUiaEventDispatcher(_uiaProvider.get());
            }
            catch (...)
            {
                LOG_HR(wil::ResultFromCaughtException());
                _uiaProvider = nullptr;
            }
        }

        return _uiaProvider.get();
    }
};

STDMETHODIMP HwndTerminal::TsfDataProvider::QueryInterface(REFIID, void**) noexcept
{
    return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE HwndTerminal::TsfDataProvider::AddRef() noexcept
{
    return 1;
}

ULONG STDMETHODCALLTYPE HwndTerminal::TsfDataProvider::Release() noexcept
{
    return 1;
}

HWND HwndTerminal::TsfDataProvider::GetHwnd()
{
    return _terminal->_hwnd.get();
}

RECT HwndTerminal::TsfDataProvider::GetViewport()
{
    const auto hwnd = GetHwnd();

    RECT rc;
    GetClientRect(hwnd, &rc);

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
    // > The left and top members are zero. The right and bottom members contain the width and height of the window.
    // --> We can turn the client rect into a screen-relative rect by adding the left/top position.
    ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rc));
    rc.right += rc.left;
    rc.bottom += rc.top;

    return rc;
}

RECT HwndTerminal::TsfDataProvider::GetCursorPosition()
{
    // Convert from columns/rows to pixels.
    til::point cursorPos;
    til::size fontSize;
    {
        auto rawCursorPos = _terminal->_core->CursorPosition();
        cursorPos = til::point{
            static_cast<til::CoordType>(rawCursorPos.X),
            static_cast<til::CoordType>(rawCursorPos.Y),
        };
        fontSize = { til::math::rounding, _terminal->_core->FontSize() }; // measured in pixels, not DIP
    }
    POINT ptSuggestion = {
        .x = cursorPos.x * fontSize.width,
        .y = cursorPos.y * fontSize.height,
    };

    ClientToScreen(GetHwnd(), &ptSuggestion);

    // Final measurement should be in pixels
    return {
        .left = ptSuggestion.x,
        .top = ptSuggestion.y,
        .right = ptSuggestion.x + fontSize.width,
        .bottom = ptSuggestion.y + fontSize.height,
    };
}

void HwndTerminal::TsfDataProvider::HandleOutput(std::wstring_view text)
{
    _terminal->_core->SendInput(text);
}

Microsoft::Console::Render::Renderer* HwndTerminal::TsfDataProvider::GetRenderer()
{
    return _terminal->_core->GetRenderer();
}

#pragma region Implementation of IDispatcherQueue
struct CalloutBasedDispatcherQueue : public winrt::implements<CalloutBasedDispatcherQueue, ::ABI::Windows::System::IDispatcherQueue, ::ABI::Windows::System::IDispatcherQueue2>
{
    CalloutBasedDispatcherQueue(TerminalDispatcherCallouts callouts) : _callouts{ std::move(callouts) } {}

    IFACEMETHODIMP CreateTimer(::ABI::Windows::System::IDispatcherQueueTimer** result)
    {
        return E_FAIL;
    }

    IFACEMETHODIMP TryEnqueue(::ABI::Windows::System::IDispatcherQueueHandler* callback, boolean* result)
    {
        return TryEnqueueWithPriority(::ABI::Windows::System::DispatcherQueuePriority::DispatcherQueuePriority_Normal, callback, result);
    }

    IFACEMETHODIMP TryEnqueueWithPriority(
        ::ABI::Windows::System::DispatcherQueuePriority priority,
        ::ABI::Windows::System::IDispatcherQueueHandler* callback,
        boolean* result)
    {
        RETURN_HR_IF(E_INVALIDARG, !result || !callback);
        callback->AddRef(); // Retain on transition to managed
        *result = _callouts.pfnDispatcherTryEnqueueWork(_callouts.context, static_cast<int>(priority), callback);
        return S_OK;
    }

    IFACEMETHODIMP add_ShutdownStarting(
        ::ABI::Windows::Foundation::ITypedEventHandler<::ABI::Windows::System::DispatcherQueue*, ::ABI::Windows::System::DispatcherQueueShutdownStartingEventArgs*>*,
        EventRegistrationToken*)
    {
        return E_FAIL;
    }

    IFACEMETHODIMP remove_ShutdownStarting(
        EventRegistrationToken)
    {
        return E_FAIL;
    }

    IFACEMETHODIMP add_ShutdownCompleted(
        ::ABI::Windows::Foundation::ITypedEventHandler<::ABI::Windows::System::DispatcherQueue*, ::IInspectable*>*,
        EventRegistrationToken*)
    {
        return E_FAIL;
    }

    IFACEMETHODIMP remove_ShutdownCompleted(
        EventRegistrationToken)
    {
        return E_FAIL;
    }

    IFACEMETHODIMP get_HasThreadAccess(boolean* result)
    {
        RETURN_HR_IF(E_INVALIDARG, !result);
        *result = _callouts.pfnDispatcherHasThreadAccess(_callouts.context);
        return S_OK;
    }

private:
    TerminalDispatcherCallouts _callouts;
};

__declspec(dllexport) HRESULT __stdcall InteropQueueHandlerInvoke(void* ptr)
{
    auto handler = static_cast<::ABI::Windows::System::IDispatcherQueueHandler*>(ptr);
    const auto hr = handler->Invoke();
    handler->Release();
    return hr;
}
#pragma endregion

extern "C" void __stdcall AvoidBuggyTSFConsoleFlags()
{
    Microsoft::Console::TSF::Handle::AvoidBuggyTSFConsoleFlags();
}

__declspec(dllexport) HRESULT __stdcall CreateTerminal(HWND parentHwnd, _In_ TerminalDispatcherCallouts dispatcherCallouts, _Out_ void** hwnd, _Out_ PTERM* terminal)
{
    winrt::Windows::System::DispatcherQueue localDispatcherQueue{ nullptr };
    if (dispatcherCallouts.pfnDispatcherTryEnqueueWork != nullptr)
    {
        auto dispatcher = winrt::make<CalloutBasedDispatcherQueue>(std::move(dispatcherCallouts));
        winrt::copy_from_abi(localDispatcherQueue, winrt::get_abi(dispatcher));
    }
    auto inner = new HwndTerminal{ parentHwnd, localDispatcherQueue };
    *terminal = inner;
    *hwnd = inner->_hwnd.get();
    inner->Initialize();
    return S_OK;
}

__declspec(dllexport) void __stdcall DestroyTerminal(PTERM terminal)
{
    delete (HwndTerminal*)terminal;
}

// Generate all of the C->C++ bridge functions.
#define API_NAME(name) Terminal##name
#define GENERATOR_0(name)                                                  \
    __declspec(dllexport) HRESULT __stdcall API_NAME(name)(PTERM terminal) \
    try                                                                    \
    {                                                                      \
        return ((HwndTerminal*)(terminal))->name();                        \
    }                                                                      \
    CATCH_RETURN()
#define GENERATOR_1(name, t1, a1)                                                 \
    __declspec(dllexport) HRESULT __stdcall API_NAME(name)(PTERM terminal, t1 a1) \
    try                                                                           \
    {                                                                             \
        return ((HwndTerminal*)(terminal))->name(a1);                             \
    }                                                                             \
    CATCH_RETURN()
#define GENERATOR_2(name, t1, a1, t2, a2)                                                \
    __declspec(dllexport) HRESULT __stdcall API_NAME(name)(PTERM terminal, t1 a1, t2 a2) \
    try                                                                                  \
    {                                                                                    \
        return ((HwndTerminal*)(terminal))->name(a1, a2);                                \
    }                                                                                    \
    CATCH_RETURN()
#define GENERATOR_3(name, t1, a1, t2, a2, t3, a3)                                               \
    __declspec(dllexport) HRESULT __stdcall API_NAME(name)(PTERM terminal, t1 a1, t2 a2, t3 a3) \
    try                                                                                         \
    {                                                                                           \
        return ((HwndTerminal*)(terminal))->name(a1, a2, a3);                                   \
    }                                                                                           \
    CATCH_RETURN()
#define GENERATOR_4(name, t1, a1, t2, a2, t3, a3, t4, a4)                                              \
    __declspec(dllexport) HRESULT __stdcall API_NAME(name)(PTERM terminal, t1 a1, t2 a2, t3 a3, t4 a4) \
    try                                                                                                \
    {                                                                                                  \
        return ((HwndTerminal*)(terminal))->name(a1, a2, a3, a4);                                      \
    }                                                                                                  \
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
