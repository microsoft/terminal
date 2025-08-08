// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalWindow.g.h"
#include "SystemMenuChangeArgs.g.h"
#include "WindowProperties.g.h"

#include "Remoting.h"
#include "TerminalPage.h"

#include <cppwinrt_utils.h>

#ifdef UNIT_TESTING
// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class CommandlineTest;
};
#endif

namespace winrt::TerminalApp::implementation
{
    struct SystemMenuChangeArgs : SystemMenuChangeArgsT<SystemMenuChangeArgs>
    {
        WINRT_PROPERTY(winrt::hstring, Name, L"");
        WINRT_PROPERTY(SystemMenuChangeAction, Action, SystemMenuChangeAction::Add);
        WINRT_PROPERTY(SystemMenuItemHandler, Handler, nullptr);

    public:
        SystemMenuChangeArgs(const winrt::hstring& name, SystemMenuChangeAction action, SystemMenuItemHandler handler = nullptr) :
            _Name{ name }, _Action{ action }, _Handler{ handler } {};
    };

    struct WindowProperties : WindowPropertiesT<WindowProperties>
    {
        // Normally, WindowName and WindowId would be
        // WINRT_OBSERVABLE_PROPERTY's, but we want them to raise
        // WindowNameForDisplay and WindowIdForDisplay instead
        winrt::hstring WindowName() const noexcept;
        void WindowName(const winrt::hstring& value);
        uint64_t WindowId() const noexcept;
        void WindowId(const uint64_t& value);
        winrt::hstring WindowIdForDisplay() const noexcept;
        winrt::hstring WindowNameForDisplay() const noexcept;
        bool IsQuakeWindow() const noexcept;

        til::property_changed_event PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, VirtualWorkingDirectory, PropertyChanged.raise, L"");

    public:
        // Used for setting the initial CWD, before we have XAML set up for property change notifications.
        void SetInitialCwd(winrt::hstring cwd) { _VirtualWorkingDirectory = std::move(cwd); };

        til::property<winrt::hstring> VirtualEnvVars;

    private:
        winrt::hstring _WindowName{};
        uint64_t _WindowId{ 0 };
    };

    struct TerminalWindow : TerminalWindowT<TerminalWindow, IInitializeWithWindow>
    {
    public:
        TerminalWindow(const TerminalApp::SettingsLoadEventArgs& settingsLoadedResult, const TerminalApp::ContentManager& manager);
        ~TerminalWindow() = default;

        STDMETHODIMP Initialize(HWND hwnd);

        void Create();

        void PersistState(bool serializeBuffer);

        void UpdateSettings(winrt::TerminalApp::SettingsLoadEventArgs args);

        bool HasCommandlineArguments() const noexcept;

        int32_t SetStartupCommandline(TerminalApp::CommandlineArgs args);
        void SetStartupContent(const winrt::hstring& content, const Windows::Foundation::IReference<Windows::Foundation::Rect>& contentBounds);
        int32_t ExecuteCommandline(TerminalApp::CommandlineArgs args);
        void SetSettingsStartupArgs(const std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions);

        bool ShouldImmediatelyHandoffToElevated();
        void HandoffToElevated();

        bool FocusMode() const;
        bool Fullscreen() const;
        void Maximized(bool newMaximized);
        bool AlwaysOnTop() const;
        bool ShowTabsFullscreen() const;
        bool AutoHideWindow();
        void IdentifyWindow();

        std::optional<uint32_t> LoadPersistedLayoutIdx() const;
        winrt::Microsoft::Terminal::Settings::Model::WindowLayout LoadPersistedLayout();

        void SetPersistedLayoutIdx(const uint32_t idx);

        void RequestExitFullscreen();

        Windows::Foundation::Size GetLaunchDimensions(uint32_t dpi);
        bool CenterOnLaunch();
        TerminalApp::InitialPosition GetInitialPosition(int64_t defaultInitialX, int64_t defaultInitialY);
        winrt::Windows::UI::Xaml::ElementTheme GetRequestedTheme();
        Microsoft::Terminal::Settings::Model::LaunchMode GetLaunchMode();
        bool GetShowTabsInTitlebar();
        bool GetInitialAlwaysOnTop();
        bool GetInitialShowTabsFullscreen();
        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        hstring Title();
        void TitlebarClicked();
        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void CloseWindow();
        void WindowVisibilityChanged(const bool showOrHide);

        winrt::TerminalApp::TaskbarState TaskbarState();
        winrt::Windows::UI::Xaml::Media::Brush TitlebarBrush();
        winrt::Windows::UI::Xaml::Media::Brush FrameBrush();
        void WindowActivated(const bool activated);

        bool GetMinimizeToNotificationArea();
        bool GetAlwaysShowNotificationIcon();

        bool GetShowTitleInTitlebar();

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Xaml::Controls::ContentDialogResult> ShowDialog(winrt::Windows::UI::Xaml::Controls::ContentDialog dialog);
        void DismissDialog();

        Microsoft::Terminal::Settings::Model::Theme Theme();
        void UpdateSettingsHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::TerminalApp::SettingsLoadEventArgs& arg);

        void WindowName(const winrt::hstring& value);
        void WindowId(const uint64_t& value);

        bool IsQuakeWindow() const noexcept { return _WindowProperties->IsQuakeWindow(); }
        TerminalApp::WindowProperties WindowProperties() { return *_WindowProperties; }

        void AttachContent(winrt::hstring content, uint32_t tabIndex);
        void SendContentToOther(winrt::TerminalApp::RequestReceiveContentArgs args);

        // -------------------------------- WinRT Events ---------------------------------
        // PropertyChanged is surprisingly not a typed event, so we'll define that one manually.
        // Usually we'd just do
        //    til::property_changed_event PropertyChanged;
        //
        // But what we're doing here is exposing the Page's PropertyChanged _as
        // our own event_. It's a FORWARDED_CALLBACK, essentially.
        winrt::event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return _root->PropertyChanged(handler); }
        void PropertyChanged(winrt::event_token const& token) { _root->PropertyChanged(token); }

        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Settings::Model::Theme> RequestedThemeChanged;
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> IsQuakeWindowChanged;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::SystemMenuChangeArgs> SystemMenuChangeRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::SettingsLoadEventArgs> SettingsChanged;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Control::WindowSizeChangedEventArgs> WindowSizeChanged;

    private:
        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the AppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The root currently is _root.
        winrt::com_ptr<TerminalPage> _root{ nullptr };

        wil::com_ptr<CommandlineArgs> _appArgs{ nullptr };
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _startupConnection{ nullptr };
        bool _hasCommandLineArguments{ false };
        bool _gotSettingsStartupActions{ false };
        std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> _settingsStartupArgs{};
        Windows::Foundation::IReference<Windows::Foundation::Rect> _contentBounds{ nullptr };

        winrt::com_ptr<TerminalApp::implementation::WindowProperties> _WindowProperties{ nullptr };

        std::optional<uint32_t> _loadFromPersistedLayoutIdx{};
        std::optional<winrt::Microsoft::Terminal::Settings::Model::WindowLayout> _cachedLayout{ std::nullopt };

        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };
        TerminalApp::SettingsLoadEventArgs _initialLoadResult{ nullptr };

        TerminalApp::ContentManager _manager{ nullptr };
        std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> _initialContentArgs;

        void _ShowLoadErrorsDialog(const winrt::hstring& titleKey,
                                   const winrt::hstring& contentKey,
                                   HRESULT settingsLoadedResult,
                                   const winrt::hstring& exceptionText);
        void _ShowLoadWarningsDialog(const IInspectable& sender, const Windows::Foundation::Collections::IVectorView<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings>& warnings);

        bool _IsKeyboardServiceEnabled();

        void _RefreshThemeRoutine();
        void _OnLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _pageInitialized(const IInspectable& sender, const IInspectable& eventArgs);
        void _OpenSettingsUI();
        void _WindowSizeChanged(const IInspectable& sender, winrt::Microsoft::Terminal::Control::WindowSizeChangedEventArgs args);
        void _RenameWindowRequested(const IInspectable& sender, const winrt::TerminalApp::RenameWindowRequestedArgs args);

        winrt::Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::ActionAndArgs> _contentStringToActions(const winrt::hstring& content,
                                                                                                                                      const bool replaceFirstWithNewTab);

        // These are events that are handled by the TerminalPage, but are
        // exposed through the AppLogic. This macro is used to forward the event
        // directly to them.
        FORWARDED_TYPED_EVENT(Initialized, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, Initialized);

        FORWARDED_TYPED_EVENT(SetTitleBarContent, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement, _root, SetTitleBarContent);
        FORWARDED_TYPED_EVENT(TitleChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, TitleChanged);
        FORWARDED_TYPED_EVENT(CloseWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, CloseWindowRequested);
        FORWARDED_TYPED_EVENT(FocusModeChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FocusModeChanged);
        FORWARDED_TYPED_EVENT(FullscreenChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FullscreenChanged);
        FORWARDED_TYPED_EVENT(ChangeMaximizeRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, ChangeMaximizeRequested);
        FORWARDED_TYPED_EVENT(AlwaysOnTopChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, AlwaysOnTopChanged);
        FORWARDED_TYPED_EVENT(RaiseVisualBell, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, RaiseVisualBell);
        FORWARDED_TYPED_EVENT(SetTaskbarProgress, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, SetTaskbarProgress);
        FORWARDED_TYPED_EVENT(IdentifyWindowsRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, IdentifyWindowsRequested);
        FORWARDED_TYPED_EVENT(SummonWindowRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, SummonWindowRequested);
        FORWARDED_TYPED_EVENT(OpenSystemMenu, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, OpenSystemMenu);
        FORWARDED_TYPED_EVENT(QuitRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, QuitRequested);
        FORWARDED_TYPED_EVENT(ShowWindowChanged, Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Control::ShowWindowArgs, _root, ShowWindowChanged);

        FORWARDED_TYPED_EVENT(RequestMoveContent, Windows::Foundation::IInspectable, winrt::TerminalApp::RequestMoveContentArgs, _root, RequestMoveContent);
        FORWARDED_TYPED_EVENT(RequestReceiveContent, Windows::Foundation::IInspectable, winrt::TerminalApp::RequestReceiveContentArgs, _root, RequestReceiveContent);

        FORWARDED_TYPED_EVENT(RequestLaunchPosition, Windows::Foundation::IInspectable, winrt::TerminalApp::LaunchPositionRequest, _root, RequestLaunchPosition);

#ifdef UNIT_TESTING
        friend class TerminalAppLocalTests::CommandlineTest;
#endif
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalWindow);
}
