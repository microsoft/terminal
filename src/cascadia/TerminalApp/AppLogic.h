// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppLogic.g.h"
#include "FindTargetWindowResult.g.h"
#include "SystemMenuChangeArgs.g.h"
#include "Jumplist.h"
#include "LanguageProfileNotifier.h"
#include "TerminalPage.h"

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
    struct FindTargetWindowResult : FindTargetWindowResultT<FindTargetWindowResult>
    {
        WINRT_PROPERTY(int32_t, WindowId, -1);
        WINRT_PROPERTY(winrt::hstring, WindowName, L"");

    public:
        FindTargetWindowResult(const int32_t id, const winrt::hstring& name) :
            _WindowId{ id }, _WindowName{ name } {};

        FindTargetWindowResult(const int32_t id) :
            FindTargetWindowResult(id, L""){};
    };

    struct SystemMenuChangeArgs : SystemMenuChangeArgsT<SystemMenuChangeArgs>
    {
        WINRT_PROPERTY(winrt::hstring, Name, L"");
        WINRT_PROPERTY(SystemMenuChangeAction, Action, SystemMenuChangeAction::Add);
        WINRT_PROPERTY(SystemMenuItemHandler, Handler, nullptr);

    public:
        SystemMenuChangeArgs(const winrt::hstring& name, SystemMenuChangeAction action, SystemMenuItemHandler handler = nullptr) :
            _Name{ name }, _Action{ action }, _Handler{ handler } {};
    };

    struct AppLogic : AppLogicT<AppLogic, IInitializeWithWindow>
    {
    public:
        static AppLogic* Current() noexcept;
        static const MTSM::CascadiaSettings CurrentAppSettings();

        AppLogic();
        ~AppLogic() = default;

        STDMETHODIMP Initialize(HWND hwnd);

        void Create();
        bool IsUwp() const noexcept;
        void RunAsUwp();
        bool IsElevated() const noexcept;
        void ReloadSettings();

        [[nodiscard]] MTSM::CascadiaSettings GetSettings() const noexcept;

        void Quit();

        bool HasCommandlineArguments() const noexcept;
        bool HasSettingsStartupActions() const noexcept;
        int32_t SetStartupCommandline(array_view<const winrt::hstring> actions);
        int32_t ExecuteCommandline(array_view<const winrt::hstring> actions, const winrt::hstring& cwd);
        MTApp::FindTargetWindowResult FindTargetWindow(array_view<const winrt::hstring> actions);
        winrt::hstring ParseCommandlineMessage();
        bool ShouldExitEarly();

        bool FocusMode() const;
        bool Fullscreen() const;
        void Maximized(bool newMaximized);
        bool AlwaysOnTop() const;
        bool AutoHideWindow();

        bool ShouldUsePersistedLayout();
        bool ShouldImmediatelyHandoffToElevated();
        void HandoffToElevated();
        hstring GetWindowLayoutJson(Microsoft::Terminal::Settings::Model::LaunchPosition position);
        void SaveWindowLayoutJsons(const WFC::IVector<hstring>& layouts);
        void IdentifyWindow();
        void RenameFailed();
        winrt::hstring WindowName();
        void WindowName(const winrt::hstring& name);
        uint64_t WindowId();
        void WindowId(const uint64_t& id);
        void SetPersistedLayoutIdx(const uint32_t idx);
        void SetNumberOfOpenWindows(const uint64_t num);
        bool IsQuakeWindow() const noexcept;
        void RequestExitFullscreen();

        WF::Size GetLaunchDimensions(uint32_t dpi);
        bool CenterOnLaunch();
        TerminalApp::InitialPosition GetInitialPosition(int64_t defaultInitialX, int64_t defaultInitialY);
        WUX::ElementTheme GetRequestedTheme();
        Microsoft::Terminal::Settings::Model::LaunchMode GetLaunchMode();
        bool GetShowTabsInTitlebar();
        bool GetInitialAlwaysOnTop();
        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        WUX::UIElement GetRoot() noexcept;

        void SetInboundListener();

        hstring Title();
        void TitlebarClicked();
        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void CloseWindow(Microsoft::Terminal::Settings::Model::LaunchPosition position);
        void WindowVisibilityChanged(const bool showOrHide);

        MTApp::TaskbarState TaskbarState();
        WUXMedia::Brush TitlebarBrush();
        void WindowActivated(const bool activated);

        bool GetMinimizeToNotificationArea();
        bool GetAlwaysShowNotificationIcon();
        bool GetShowTitleInTitlebar();

        WF::IAsyncOperation<WUXC::ContentDialogResult> ShowDialog(WUXC::ContentDialog dialog);
        void DismissDialog();

        WFC::IMapView<MTControl::KeyChord, MTSM::Command> GlobalHotkeys();

        MTSM::Theme Theme();

        // -------------------------------- WinRT Events ---------------------------------
        // PropertyChanged is surprisingly not a typed event, so we'll define that one manually.
        // Usually we'd just do
        //    WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        //
        // But what we're doing here is exposing the Page's PropertyChanged _as
        // our own event_. It's a FORWARDED_CALLBACK, essentially.
        winrt::event_token PropertyChanged(WUX::Data::PropertyChangedEventHandler const& handler) { return _root->PropertyChanged(handler); }
        void PropertyChanged(winrt::event_token const& token) { _root->PropertyChanged(token); }

        TYPED_EVENT(RequestedThemeChanged, WF::IInspectable, MTSM::Theme);
        TYPED_EVENT(SettingsChanged, WF::IInspectable, WF::IInspectable);
        TYPED_EVENT(SystemMenuChangeRequested, WF::IInspectable, MTApp::SystemMenuChangeArgs);

    private:
        bool _isUwp{ false };
        bool _isElevated{ false };

        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the AppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The root currently is _root.
        winrt::com_ptr<TerminalPage> _root{ nullptr };
        MTSM::CascadiaSettings _settings{ nullptr };

        winrt::hstring _settingsLoadExceptionText;
        HRESULT _settingsLoadedResult = S_OK;
        bool _loadedInitialSettings = false;

        uint64_t _numOpenWindows{ 0 };

        std::shared_mutex _dialogLock;
        WUXC::ContentDialog _dialog;

        ::MTApp::AppCommandlineArgs _appArgs;
        ::MTApp::AppCommandlineArgs _settingsAppArgs;

        std::shared_ptr<ThrottledFuncTrailing<>> _reloadSettings;
        til::throttled_func_trailing<> _reloadState;

        // These fields invoke _reloadSettings and must be destroyed before _reloadSettings.
        // (C++ destroys members in reverse-declaration-order.)
        winrt::com_ptr<LanguageProfileNotifier> _languageProfileNotifier;
        wil::unique_folder_change_reader_nothrow _reader;

        static MTApp::FindTargetWindowResult _doFindTargetWindow(winrt::array_view<const hstring> args,
                                                                 const Microsoft::Terminal::Settings::Model::WindowingMode& windowingBehavior);

        void _ShowLoadErrorsDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey, HRESULT settingsLoadedResult);
        void _ShowLoadWarningsDialog();
        bool _IsKeyboardServiceEnabled();

        void _ApplyLanguageSettingChange() noexcept;
        void _RefreshThemeRoutine();
        fire_and_forget _ApplyStartupTaskStateChange();

        void _OnLoaded(const IInspectable& sender, const WUX::RoutedEventArgs& eventArgs);

        [[nodiscard]] HRESULT _TryLoadSettings() noexcept;
        void _ProcessLazySettingsChanges();
        void _RegisterSettingsChange();
        fire_and_forget _DispatchReloadSettings();
        void _OpenSettingsUI();

        bool _hasCommandLineArguments{ false };
        bool _hasSettingsStartupActions{ false };
        std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> _warnings;

        // These are events that are handled by the TerminalPage, but are
        // exposed through the AppLogic. This macro is used to forward the event
        // directly to them.
        FORWARDED_TYPED_EVENT(SetTitleBarContent, WF::IInspectable, WUX::UIElement, _root, SetTitleBarContent);
        FORWARDED_TYPED_EVENT(TitleChanged, WF::IInspectable, winrt::hstring, _root, TitleChanged);
        FORWARDED_TYPED_EVENT(LastTabClosed, WF::IInspectable, MTApp::LastTabClosedEventArgs, _root, LastTabClosed);
        FORWARDED_TYPED_EVENT(FocusModeChanged, WF::IInspectable, WF::IInspectable, _root, FocusModeChanged);
        FORWARDED_TYPED_EVENT(FullscreenChanged, WF::IInspectable, WF::IInspectable, _root, FullscreenChanged);
        FORWARDED_TYPED_EVENT(ChangeMaximizeRequested, WF::IInspectable, WF::IInspectable, _root, ChangeMaximizeRequested);
        FORWARDED_TYPED_EVENT(AlwaysOnTopChanged, WF::IInspectable, WF::IInspectable, _root, AlwaysOnTopChanged);
        FORWARDED_TYPED_EVENT(RaiseVisualBell, WF::IInspectable, WF::IInspectable, _root, RaiseVisualBell);
        FORWARDED_TYPED_EVENT(SetTaskbarProgress, WF::IInspectable, WF::IInspectable, _root, SetTaskbarProgress);
        FORWARDED_TYPED_EVENT(IdentifyWindowsRequested, WF::IInspectable, WF::IInspectable, _root, IdentifyWindowsRequested);
        FORWARDED_TYPED_EVENT(RenameWindowRequested, WF::IInspectable, MTApp::RenameWindowRequestedArgs, _root, RenameWindowRequested);
        FORWARDED_TYPED_EVENT(IsQuakeWindowChanged, WF::IInspectable, WF::IInspectable, _root, IsQuakeWindowChanged);
        FORWARDED_TYPED_EVENT(SummonWindowRequested, WF::IInspectable, WF::IInspectable, _root, SummonWindowRequested);
        FORWARDED_TYPED_EVENT(CloseRequested, WF::IInspectable, WF::IInspectable, _root, CloseRequested);
        FORWARDED_TYPED_EVENT(OpenSystemMenu, WF::IInspectable, WF::IInspectable, _root, OpenSystemMenu);
        FORWARDED_TYPED_EVENT(QuitRequested, WF::IInspectable, WF::IInspectable, _root, QuitRequested);
        FORWARDED_TYPED_EVENT(ShowWindowChanged, WF::IInspectable, MTControl::ShowWindowArgs, _root, ShowWindowChanged);

#ifdef UNIT_TESTING
        friend class TerminalAppLocalTests::CommandlineTest;
#endif
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppLogic);
}
