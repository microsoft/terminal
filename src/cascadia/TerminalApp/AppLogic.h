// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppLogic.g.h"

#include "Tab.h"
#include "CascadiaSettings.h"
#include "TerminalPage.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace winrt::TerminalApp::implementation
{
    struct AppLogic : AppLogicT<AppLogic>
    {
    public:
        AppLogic();
        ~AppLogic() = default;

        void Create();
        void LoadSettings();

        Windows::Foundation::Point GetLaunchDimensions(uint32_t dpi);
        winrt::Windows::Foundation::Point GetLaunchInitialPositions(int32_t defaultInitialX, int32_t defaultInitialY);
        winrt::Windows::UI::Xaml::ElementTheme GetRequestedTheme();
        LaunchMode GetLaunchMode();
        bool GetShowTabsInTitlebar();

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        hstring Title();
        void TitlebarClicked();

        void WindowCloseButtonClicked();

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(RequestedThemeChanged, _requestedThemeChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::ElementTheme);

    private:
        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the AppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _AppLogiclyTheme. The root currently is _root.
        winrt::com_ptr<TerminalPage> _root{ nullptr };

        std::shared_ptr<::TerminalApp::CascadiaSettings> _settings{ nullptr };

        HRESULT _settingsLoadedResult;
        winrt::hstring _settingsLoadExceptionText{};

        bool _loadedInitialSettings;

        wil::unique_folder_change_reader_nothrow _reader;

        std::shared_mutex _dialogLock;

        std::atomic<bool> _settingsReloadQueued{ false };

        fire_and_forget _ShowDialog(const winrt::Windows::Foundation::IInspectable& sender, winrt::Windows::UI::Xaml::Controls::ContentDialog dialog);
        void _ShowLoadErrorsDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey, HRESULT settingsLoadedResult);
        void _ShowLoadWarningsDialog();

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
