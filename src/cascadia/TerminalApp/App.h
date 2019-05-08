// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Tab.h"
#include "CascadiaSettings.h"
#include "App.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

#include <wil/filesystem.h>

#include <winrt/Microsoft.Terminal.TerminalControl.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace winrt::TerminalApp::implementation
{

    // We dont use AppT as it does not provide access to protected constructors
    template <typename D, typename... I>
    using AppT_Override = App_base<D, I...>;

    struct App : AppT_Override<App>
    {
    public:
        App();

        Windows::UI::Xaml::UIElement GetRoot() noexcept;
        Windows::UI::Xaml::UIElement GetTabs() noexcept;
        void Create();
        void LoadSettings();

        Windows::Foundation::Point GetLaunchDimensions(uint32_t dpi);
        bool GetShowTabsInTitlebar();

        ~App();

        hstring GetTitle();

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT(TitleChanged, _titleChangeHandlers, winrt::Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);

    private:
        App(Windows::UI::Xaml::Markup::IXamlMetadataProvider const& parentProvider);

        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the app.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The two roots currently are _root and _tabRow
        // (which is a root when the tabs are in the titlebar.)
        Windows::UI::Xaml::Controls::Grid _root{ nullptr };
        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabRow{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabContent{ nullptr };
        Windows::UI::Xaml::Controls::SplitButton _newTabButton{ nullptr };

        std::vector<std::shared_ptr<Tab>> _tabs;

        std::unique_ptr<::TerminalApp::CascadiaSettings> _settings;

        HRESULT _settingsLoadedResult;

        bool _loadedInitialSettings;
        std::shared_mutex _dialogLock;

        wil::unique_folder_change_reader_nothrow _reader;

        void _Create();
        void _CreateNewTabFlyout();

        fire_and_forget _ShowOkDialog(const winrt::hstring& titleKey, const winrt::hstring& textKey);

        void _LoadSettings();
        void _HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept;

        void _RegisterSettingsChange();
        void _ReloadSettings();

        void _SettingsButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _FeedbackButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        void _UpdateTabView();

        void _CreateNewTabFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings);

        void _OpenNewTab(std::optional<int> profileIndex);
        void _CloseFocusedTab();
        void _SelectNextTab(const bool bMoveRight);

        void _SetFocusedTabIndex(int tabIndex);
        int _GetFocusedTabIndex() const;

        void _DoScroll(int delta);
        void _CopyText(const bool trimTrailingWhitespace);
        // Todo: add more event implementations here
        // MSFT:20641986: Add keybindings for New Window

        void _OnLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _OnTabSelectionChanged(const IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& eventArgs);
        void _OnTabClosing(const IInspectable& sender, const Microsoft::UI::Xaml::Controls::TabViewTabClosingEventArgs& eventArgs);
        void _OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs);
        void _OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);

        void _RemoveTabViewItem(const IInspectable& tabViewItem);

        void _ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme);

        static Windows::UI::Xaml::Controls::IconElement _GetIconFromProfile(const ::TerminalApp::Profile& profile);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
