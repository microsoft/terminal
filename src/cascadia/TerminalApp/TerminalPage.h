// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TerminalPage.g.h"
#include "Tab.h"
#include "CascadiaSettings.h"
#include "Profile.h"
#include "ScopedResourceLoader.h"

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

        void SetSettings(::TerminalApp::CascadiaSettings* settings, bool needRefreshUI);
        void SetSettingsLoadExceptionText(hstring exceptionText);
        void SetSettingsLoadExceptionText(::TerminalApp::SettingsLoadErrors error);

        void Create();

        hstring GetTitle();

        void ShowOkDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey);
        void ShowLoadWarningsDialog();
        void ShowLoadErrorsDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey, HRESULT settingsLoadedResult);

        void TitlebarClicked();

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(TitleChanged, _titleChangeHandlers, winrt::Windows::Foundation::IInspectable, winrt::hstring);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(LastTabClosed, _lastTabClosedHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(SetTitleBarContent, _setTitleBarContentHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(ShowDialog, _showDialogHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::Controls::ContentDialog);

    private:
        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        TerminalApp::TabRowControl _tabRow{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabContent{ nullptr };
        Windows::UI::Xaml::Controls::SplitButton _newTabButton{ nullptr };

        ::TerminalApp::CascadiaSettings* _settings{ nullptr };

        std::vector<std::shared_ptr<Tab>> _tabs;

        ScopedResourceLoader _resourceLoader;

        winrt::hstring _settingsLoadExceptionText{};

        void _ShowAboutDialog();

        void _CreateNewTabFlyout();
        void _OpenNewTabDropdown();
        void _OpenNewTab(std::optional<int> profileIndex);
        void _CreateNewTabFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _CreateConnectionFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings);

        void _SettingsButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _FeedbackButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _AboutButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        void _HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept;

        void _UpdateTitle(std::shared_ptr<Tab> tab);
        void _UpdateTabIcon(std::shared_ptr<Tab> tab);
        void _UpdateTabView();
        void _DuplicateTabViewItem();
        void _RemoveTabViewItem(const IInspectable& tabViewItem);

        void _RegisterTerminalEvents(Microsoft::Terminal::TerminalControl::TermControl term, std::shared_ptr<Tab> hostingTab);

        void _SelectNextTab(const bool bMoveRight);
        bool _SelectTab(const int tabIndex);
        void _MoveFocus(const Direction& direction);

        winrt::Microsoft::Terminal::TerminalControl::TermControl _GetFocusedControl();
        int _GetFocusedTabIndex() const;
        void _SetFocusedTabIndex(int tabIndex);
        void _CloseFocusedTab();
        void _CloseFocusedPane();

        // Todo: add more event implementations here
        // MSFT:20641986: Add keybindings for New Window
        void _Scroll(int delta);
        void _SplitVertical(const std::optional<GUID>& profileGuid);
        void _SplitHorizontal(const std::optional<GUID>& profileGuid);
        void _SplitPane(const Pane::SplitState splitType, const std::optional<GUID>& profileGuid);
        void _ResizePane(const Direction& direction);
        void _ScrollPage(int delta);
        static Windows::UI::Xaml::Controls::IconElement _GetIconFromProfile(const ::TerminalApp::Profile& profile);
        void _SetAcceleratorForMenuItem(Windows::UI::Xaml::Controls::MenuFlyoutItem& menuItem, const winrt::Microsoft::Terminal::Settings::KeyChord& keyChord);

        void _CopyToClipboardHandler(const winrt::hstring& copiedData);
        void _PasteFromClipboardHandler(const IInspectable& sender,
                                        const Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs& eventArgs);
        bool _CopyText(const bool trimTrailingWhitespace);
        void _PasteText();
        static fire_and_forget PasteFromClipboard(winrt::Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs eventArgs);

        void _OpenSettings();
        fire_and_forget LaunchSettings();

        void _OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
        void _OnTabSelectionChanged(const IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& eventArgs);
        void _OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs);
        void _OnContentSizeChanged(const IInspectable& /*sender*/, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _OnTabClosing(const IInspectable& sender, const Microsoft::UI::Xaml::Controls::TabViewTabClosingEventArgs& eventArgs);

        void RefreshUIForSettingsReload();

#pragma region ActionHandlers
        // These are all defined in AppActionHandlers.cpp
        void _HandleNewTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
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
        void _HandleNewTabWithProfile(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleSwitchToTab(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleResizePane(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleMoveFocus(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
        void _HandleCopyText(const IInspectable& sender, const TerminalApp::ActionEventArgs& args);
#pragma endregion
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage, implementation::TerminalPage>
    {
    };
}
