// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalWindow.g.h"
#include "SystemMenuChangeArgs.g.h"
#include "WindowProperties.g.h"

#include "SettingsLoadEventArgs.h"
#include "TerminalPage.h"
#include "SettingsLoadEventArgs.h"

#include <inc/cppwinrt_utils.h>
#include <ThrottledFunc.h>

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

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, VirtualWorkingDirectory, _PropertyChangedHandlers, L"");

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

    public:
        // Used for setting the initial CWD, before we have XAML set up for property change notifications.
        void SetInitialCwd(winrt::hstring cwd) { _VirtualWorkingDirectory = std::move(cwd); };

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

        void Quit();

        winrt::fire_and_forget UpdateSettings(winrt::TerminalApp::SettingsLoadEventArgs args);

        bool HasCommandlineArguments() const noexcept;

        int32_t SetStartupCommandline(array_view<const winrt::hstring> actions, winrt::hstring cwd);
        void SetStartupContent(const winrt::hstring& content, const Windows::Foundation::IReference<Windows::Foundation::Rect>& contentBounds);
        int32_t ExecuteCommandline(array_view<const winrt::hstring> actions, const winrt::hstring& cwd);
        void SetSettingsStartupArgs(const std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions);
        winrt::hstring ParseCommandlineMessage();
        bool ShouldExitEarly();

        bool ShouldImmediatelyHandoffToElevated();
        void HandoffToElevated();

        bool FocusMode() const;
        bool Fullscreen() const;
        void Maximized(bool newMaximized);
        bool AlwaysOnTop() const;
        bool AutoHideWindow();

        hstring GetWindowLayoutJson(Microsoft::Terminal::Settings::Model::LaunchPosition position);

        void IdentifyWindow();
        void RenameFailed();

        std::optional<uint32_t> LoadPersistedLayoutIdx() const;
        winrt::Microsoft::Terminal::Settings::Model::WindowLayout LoadPersistedLayout();

        void SetPersistedLayoutIdx(const uint32_t idx);
        void SetNumberOfOpenWindows(const uint64_t num);
        bool ShouldUsePersistedLayout() const;
        void ClearPersistedWindowState();

        void RequestExitFullscreen();

        Windows::Foundation::Size GetLaunchDimensions(uint32_t dpi);
        bool CenterOnLaunch();
        TerminalApp::InitialPosition GetInitialPosition(int64_t defaultInitialX, int64_t defaultInitialY);
        winrt::Windows::UI::Xaml::ElementTheme GetRequestedTheme();
        Microsoft::Terminal::Settings::Model::LaunchMode GetLaunchMode();
        bool GetShowTabsInTitlebar();
        bool GetInitialAlwaysOnTop();
        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        hstring Title();
        void TitlebarClicked();
        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void CloseWindow(Microsoft::Terminal::Settings::Model::LaunchPosition position, const bool isLastWindow);
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
        //    WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        //
        // But what we're doing here is exposing the Page's PropertyChanged _as
        // our own event_. It's a FORWARDED_CALLBACK, essentially.
        winrt::event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler) { return _root->PropertyChanged(handler); }
        void PropertyChanged(winrt::event_token const& token) { _root->PropertyChanged(token); }

        TYPED_EVENT(RequestedThemeChanged, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Settings::Model::Theme);

    private:
        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the AppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The root currently is _root.
        winrt::com_ptr<TerminalPage> _root{ nullptr };
        winrt::Windows::UI::Xaml::Controls::ContentDialog _dialog{ nullptr };
        std::shared_mutex _dialogLock;

        bool _hasCommandLineArguments{ false };
        ::TerminalApp::AppCommandlineArgs _appArgs;
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
        void _ShowLoadWarningsDialog(const Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings>& warnings);

        bool _IsKeyboardServiceEnabled();

        void _RefreshThemeRoutine();
        void _OnLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _OpenSettingsUI();

        winrt::Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::ActionAndArgs> _contentStringToActions(const winrt::hstring& content,
                                                                                                                                      const bool replaceFirstWithNewTab);

        // These are events that are handled by the TerminalPage, but are
        // exposed through the AppLogic. This macro is used to forward the event
        // directly to them.
        FORWARDED_TYPED_EVENT(Initialized, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, Initialized);

        FORWARDED_TYPED_EVENT(SetTitleBarContent, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement, _root, SetTitleBarContent);
        FORWARDED_TYPED_EVENT(TitleChanged, winrt::Windows::Foundation::IInspectable, winrt::hstring, _root, TitleChanged);
        FORWARDED_TYPED_EVENT(LastTabClosed, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs, _root, LastTabClosed);
        FORWARDED_TYPED_EVENT(FocusModeChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FocusModeChanged);
        FORWARDED_TYPED_EVENT(FullscreenChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FullscreenChanged);
        FORWARDED_TYPED_EVENT(ChangeMaximizeRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, ChangeMaximizeRequested);
        FORWARDED_TYPED_EVENT(AlwaysOnTopChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, AlwaysOnTopChanged);
        FORWARDED_TYPED_EVENT(RaiseVisualBell, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, RaiseVisualBell);
        FORWARDED_TYPED_EVENT(SetTaskbarProgress, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, SetTaskbarProgress);
        FORWARDED_TYPED_EVENT(IdentifyWindowsRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, IdentifyWindowsRequested);
        FORWARDED_TYPED_EVENT(RenameWindowRequested, Windows::Foundation::IInspectable, winrt::TerminalApp::RenameWindowRequestedArgs, _root, RenameWindowRequested);
        FORWARDED_TYPED_EVENT(SummonWindowRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, SummonWindowRequested);
        FORWARDED_TYPED_EVENT(CloseRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, CloseRequested);
        FORWARDED_TYPED_EVENT(OpenSystemMenu, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, OpenSystemMenu);
        FORWARDED_TYPED_EVENT(QuitRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, QuitRequested);
        FORWARDED_TYPED_EVENT(ShowWindowChanged, Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Control::ShowWindowArgs, _root, ShowWindowChanged);

        TYPED_EVENT(IsQuakeWindowChanged, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable);

        TYPED_EVENT(SystemMenuChangeRequested, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::SystemMenuChangeArgs);

        TYPED_EVENT(SettingsChanged, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::SettingsLoadEventArgs);

        FORWARDED_TYPED_EVENT(RequestMoveContent, Windows::Foundation::IInspectable, winrt::TerminalApp::RequestMoveContentArgs, _root, RequestMoveContent);
        FORWARDED_TYPED_EVENT(RequestReceiveContent, Windows::Foundation::IInspectable, winrt::TerminalApp::RequestReceiveContentArgs, _root, RequestReceiveContent);

#ifdef UNIT_TESTING
        friend class TerminalAppLocalTests::CommandlineTest;
#endif
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalWindow);
}
