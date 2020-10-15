// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppLogic.g.h"

#include "Tab.h"
#include "TerminalPage.h"
#include "Jumplist.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct AppLogic : AppLogicT<AppLogic>
    {
    public:
        static AppLogic* Current() noexcept;
        static const Microsoft::Terminal::Settings::Model::CascadiaSettings CurrentAppSettings();

        AppLogic();
        ~AppLogic() = default;

        void Create();
        bool IsUwp() const noexcept;
        void RunAsUwp();
        bool IsElevated() const noexcept;
        void LoadSettings();
        [[nodiscard]] Microsoft::Terminal::Settings::Model::CascadiaSettings GetSettings() const noexcept;

        int32_t SetStartupCommandline(array_view<const winrt::hstring> actions);
        winrt::hstring ParseCommandlineMessage();
        bool ShouldExitEarly();

        bool FocusMode() const;
        bool Fullscreen() const;
        bool AlwaysOnTop() const;

        Windows::Foundation::Size GetLaunchDimensions(uint32_t dpi);
        TerminalApp::InitialPosition GetInitialPosition(int64_t defaultInitialX, int64_t defaultInitialY);
        winrt::Windows::UI::Xaml::ElementTheme GetRequestedTheme();
        Microsoft::Terminal::Settings::Model::LaunchMode GetLaunchMode();
        bool GetShowTabsInTitlebar();
        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        hstring Title();
        void TitlebarClicked();
        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void WindowCloseButtonClicked();

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Xaml::Controls::ContentDialogResult> ShowDialog(winrt::Windows::UI::Xaml::Controls::ContentDialog dialog);

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

        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };

        HRESULT _settingsLoadedResult;
        winrt::hstring _settingsLoadExceptionText{};

        bool _loadedInitialSettings;

        wil::unique_folder_change_reader_nothrow _reader;

        std::shared_mutex _dialogLock;

        std::atomic<bool> _settingsReloadQueued{ false };

        ::TerminalApp::AppCommandlineArgs _appArgs;
        int _ParseArgs(winrt::array_view<const hstring>& args);

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
        FORWARDED_TYPED_EVENT(FocusModeChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FocusModeChanged);
        FORWARDED_TYPED_EVENT(FullscreenChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FullscreenChanged);
        FORWARDED_TYPED_EVENT(AlwaysOnTopChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, AlwaysOnTopChanged);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct AppLogic : AppLogicT<AppLogic, implementation::AppLogic>
    {
    };
}
