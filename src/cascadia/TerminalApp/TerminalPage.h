// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TerminalPage.g.h"
#include "Tab.h"
#include "CascadiaSettings.h"
#include "Profile.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage>
    {
    public:
        TerminalPage();

        void SetSettings(std::shared_ptr<::TerminalApp::CascadiaSettings> settings, bool needRefreshUI);

        void Create();

        hstring Title();

        void ShowOkDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey);

        void TitlebarClicked();

        void CloseWindow();

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(TitleChanged, _titleChangeHandlers, winrt::Windows::Foundation::IInspectable, winrt::hstring);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(LastTabClosed, _lastTabClosedHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(SetTitleBarContent, _setTitleBarContentHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(ShowDialog, _showDialogHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::Controls::ContentDialog);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(ToggleFullscreen, _toggleFullscreenHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::ToggleFullscreenEventArgs);

    private:
        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the app.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in App::_ApplyTheme. The roots currently is _tabRow
        // (which is a root when the tabs are in the titlebar.)
        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        TerminalApp::TabRowControl _tabRow{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabContent{ nullptr };
        Microsoft::UI::Xaml::Controls::SplitButton _newTabButton{ nullptr };

        std::shared_ptr<::TerminalApp::CascadiaSettings> _settings{ nullptr };

        std::vector<std::shared_ptr<Tab>> _tabs;

        bool _isFullscreen{ false };

        bool _rearranging;
        std::optional<int> _rearrangeFrom;
        std::optional<int> _rearrangeTo;

        void _ShowAboutDialog();
        void _ShowCloseWarningDialog();

        void _CreateNewTabFlyout();
        void _OpenNewTabDropdown();
        void _OpenNewTab(std::optional<int> profileIndex);
        void _CreateNewTabFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _CreateConnectionFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings);

        void _SettingsButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _FeedbackButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _AboutButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _CloseWarningPrimaryButtonOnClick(Windows::UI::Xaml::Controls::ContentDialog sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs eventArgs);

        void _HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept;

        void _UpdateTitle(std::shared_ptr<Tab> tab);
        void _UpdateTabIcon(std::shared_ptr<Tab> tab);
        void _UpdateTabView();
        void _DuplicateTabViewItem();
        void _RemoveTabViewItem(const Microsoft::UI::Xaml::Controls::TabViewItem& tabViewItem);
        void _RemoveTabViewItemByIndex(uint32_t tabIndex);

        void _RegisterTerminalEvents(Microsoft::Terminal::TerminalControl::TermControl term, std::shared_ptr<Tab> hostingTab);

        void _SelectNextTab(const bool bMoveRight);
        bool _SelectTab(const int tabIndex);
        void _MoveFocus(const Direction& direction);

        winrt::Microsoft::Terminal::TerminalControl::TermControl _GetActiveControl();
        int _GetFocusedTabIndex() const;
        void _SetFocusedTabIndex(int tabIndex);
        void _CloseFocusedTab();
        void _CloseFocusedPane();
        void _CloseAllTabs();

        // Todo: add more event implementations here
        // MSFT:20641986: Add keybindings for New Window
        void _Scroll(int delta);
        void _SplitVertical(const std::optional<GUID>& profileGuid);
        void _SplitHorizontal(const std::optional<GUID>& profileGuid);
        void _SplitPane(const Pane::SplitState splitType, const std::optional<GUID>& profileGuid);
        void _ResizePane(const Direction& direction);
        void _ScrollPage(int delta);
        void _SetAcceleratorForMenuItem(Windows::UI::Xaml::Controls::MenuFlyoutItem& menuItem, const winrt::Microsoft::Terminal::Settings::KeyChord& keyChord);

        void _CopyToClipboardHandler(const IInspectable& sender, const winrt::Microsoft::Terminal::TerminalControl::CopyToClipboardEventArgs& copiedData);
        void _PasteFromClipboardHandler(const IInspectable& sender,
                                        const Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs& eventArgs);
        bool _CopyText(const bool trimTrailingWhitespace);
        void _PasteText();
        static fire_and_forget PasteFromClipboard(winrt::Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs eventArgs);

        fire_and_forget _LaunchSettings(const bool openDefaults);

        void _OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
        void _OnTabSelectionChanged(const IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& eventArgs);
        void _OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs);
        void _OnContentSizeChanged(const IInspectable& /*sender*/, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _OnTabCloseRequested(const IInspectable& sender, const Microsoft::UI::Xaml::Controls::TabViewTabCloseRequestedEventArgs& eventArgs);

        void _RefreshUIForSettingsReload();

        void _ToggleFullscreen();

#pragma region ActionHandlers
        // These are all defined in AppActionHandlers.cpp
        void _HandleOpenNewTabDropdown(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleDuplicateTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleCloseTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleClosePane(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleScrollUp(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleScrollDown(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleNextTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandlePrevTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleSplitVertical(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleSplitHorizontal(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleScrollUpPage(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleScrollDownPage(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleOpenSettings(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandlePasteText(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleNewTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleSwitchToTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleResizePane(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleMoveFocus(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleCopyText(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleCloseWindow(const IInspectable&, const TerminalApp::ActionEventArgs& args);
        void _HandleAdjustFontSize(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleToggleFullscreen(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
#pragma endregion
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage, implementation::TerminalPage>
    {
    };
}
