// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Tab.h"
#include "CascadiaSettings.h"
#include "App.g.h"
#include "App.base.h"
#include "ScopedResourceLoader.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace winrt::TerminalApp::implementation
{
    struct App : AppT2<App>
    {
    public:
        App();

        Windows::UI::Xaml::UIElement GetRoot() noexcept;

        void Create();
        void LoadSettings();

        Windows::Foundation::Point GetLaunchDimensions(uint32_t dpi);
        bool GetShowTabsInTitlebar();

        hstring GetTitle();
        void TitlebarClicked();

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(SetTitleBarContent, _setTitleBarContentHandlers, TerminalApp::App, winrt::Windows::UI::Xaml::UIElement);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(RequestedThemeChanged, _requestedThemeChangedHandlers, TerminalApp::App, winrt::Windows::UI::Xaml::ElementTheme);

    private:
        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the app.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The two roots currently are _root and _tabRow
        // (which is a root when the tabs are in the titlebar.)
        Windows::UI::Xaml::Controls::Control _root{ nullptr };

        std::unique_ptr<::TerminalApp::CascadiaSettings> _settings;

        HRESULT _settingsLoadedResult;
        winrt::hstring _settingsLoadExceptionText{};

        bool _loadedInitialSettings;
        std::shared_mutex _dialogLock;

        ScopedResourceLoader _resourceLoader;

        wil::unique_folder_change_reader_nothrow _reader;

        std::atomic<bool> _settingsReloadQueued{ false };

        [[nodiscard]] HRESULT _TryLoadSettings(const bool saveOnLoad) noexcept;
        void _LoadSettings();
        void _OpenSettings();
        void _RegisterSettingsChange();
        fire_and_forget _DispatchReloadSettings();
        void _ReloadSettings();

        void _ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme);

        void _OnLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
