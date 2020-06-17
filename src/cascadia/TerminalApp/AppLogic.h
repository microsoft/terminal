// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppLogic.g.h"

#include "Tab.h"
#include "CascadiaSettings.h"
#include "TerminalPage.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct AppLogic : AppLogicT<AppLogic>
    {
    public:
        static AppLogic* Current() noexcept;

        AppLogic();
        ~AppLogic() = default;

        void Create();
        bool IsUwp() const noexcept;
        void RunAsUwp();
        bool IsElevated() const noexcept;
        void LoadSettings();
        [[nodiscard]] std::shared_ptr<::TerminalApp::CascadiaSettings> GetSettings() const noexcept;

        int32_t SetStartupCommandline(array_view<const winrt::hstring> actions);
        winrt::hstring ParseCommandlineMessage();
        bool ShouldExitEarly();

        winrt::hstring ApplicationDisplayName() const;
        winrt::hstring ApplicationVersion() const;

        Windows::Foundation::Point GetLaunchDimensions(uint32_t dpi);
        winrt::Windows::Foundation::Point GetLaunchInitialPositions(int32_t defaultInitialX, int32_t defaultInitialY);
        winrt::Windows::UI::Xaml::ElementTheme GetRequestedTheme();
        LaunchMode GetLaunchMode();
        bool GetShowTabsInTitlebar();
        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        hstring Title();
        void TitlebarClicked();
        bool OnDirectKeyEvent(const uint32_t vkey, const bool down);

        void WindowCloseButtonClicked();

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(RequestedThemeChanged, _requestedThemeChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::ElementTheme);

    private:
        bool _isUwp{ false };
        bool _isElevated{ false };

        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the AppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The root currently is _root.
        winrt::com_ptr<TerminalPage> _root{ nullptr };

        std::shared_ptr<::TerminalApp::CascadiaSettings> _settings{ nullptr };

        HRESULT _settingsLoadedResult;
        winrt::hstring _settingsLoadExceptionText{};

        bool _loadedInitialSettings;

        wil::unique_folder_change_reader_nothrow _reader;

        std::shared_mutex _dialogLock;

        std::atomic<bool> _settingsReloadQueued{ false };

        ::TerminalApp::AppCommandlineArgs _appArgs;
        int _ParseArgs(winrt::array_view<const hstring>& args);

        fire_and_forget _ShowDialog(const winrt::Windows::Foundation::IInspectable& sender, winrt::Windows::UI::Xaml::Controls::ContentDialog dialog);
        void _ShowLoadErrorsDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey, HRESULT settingsLoadedResult);
        void _ShowLoadWarningsDialog();

        fire_and_forget _LoadErrorsDialogRoutine();
        fire_and_forget _ShowLoadWarningsDialogRoutine();
        fire_and_forget _RefreshThemeRoutine();
        fire_and_forget _ApplyStartupTaskStateChange();

        void _OnLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        [[nodiscard]] HRESULT _TryLoadSettings() noexcept;
        void _RegisterSettingsChange();
        fire_and_forget _DispatchReloadSettings();
        void _ReloadSettings();

        void _ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme);

        // These are events that are handled by the TerminalPage, but are
        // exposed through the AppLogic. This macro is used to forward the event
        // directly to them.
        FORWARDED_TYPED_EVENT(SetTitleBarContent, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement, _root, SetTitleBarContent);
        FORWARDED_TYPED_EVENT(TitleChanged, winrt::Windows::Foundation::IInspectable, winrt::hstring, _root, TitleChanged);
        FORWARDED_TYPED_EVENT(LastTabClosed, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs, _root, LastTabClosed);
        FORWARDED_TYPED_EVENT(ToggleFullscreen, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::ToggleFullscreenEventArgs, _root, ToggleFullscreen);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct AppLogic : AppLogicT<AppLogic, implementation::AppLogic>
    {
    };
}
