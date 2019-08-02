// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TerminalPage.g.h"
#include "Tab.h"
#include "CascadiaSettings.h"
#include "Profile.h"

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

        ~TerminalPage();

        void SetSettings(::TerminalApp::CascadiaSettings* settings);

        void Create();

        hstring GetTitle();

        void RefreshUIAfterSettingsReloaded();

        void ShowOkDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey);

        TerminalApp::TabRowControl* GetTabRow(); // This is a work-around because the winrt events are not working in Page

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT(TitleChanged, _titleChangeHandlers, winrt::Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
        DECLARE_EVENT(LastTabClosed, _lastTabClosedHandlers, winrt::TerminalApp::LastTabClosedEventArgs);

    private:
        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        TerminalApp::TabRowControl _tabRow{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabContent{ nullptr };
        Windows::UI::Xaml::Controls::SplitButton _newTabButton{ nullptr };

        ::TerminalApp::CascadiaSettings* _settings;

        std::vector<std::shared_ptr<Tab>> _tabs;

        std::shared_mutex _dialogLock;

        void _HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept;

        void _CreateNewTabFromSettings(GUID profileGuid, winrt::Microsoft::Terminal::Settings::TerminalSettings settings);
        void _OpenNewTab(std::optional<int> profileIndex);
        void _CreateNewTabFlyout();

        void _RegisterTerminalEvents(Microsoft::Terminal::TerminalControl::TermControl term, std::shared_ptr<Tab> hostingTab);

        // Todo: add more event implementations here
        // MSFT:20641986: Add keybindings for New Window
        void _UpdateTitle(std::shared_ptr<Tab> tab);
        void _UpdateTabIcon(std::shared_ptr<Tab> tab);
        void _UpdateTabView();
        void _DuplicateTabViewItem();
        void _SelectNextTab(const bool bMoveRight);
        void _MoveFocus(const Direction& direction);
        void _CloseFocusedTab();
        void _CloseFocusedPane();
        void _Scroll(int delta);
        void _SplitVertical(const std::optional<GUID>& profileGuid);
        void _SplitHorizontal(const std::optional<GUID>& profileGuid);
        void _SplitPane(const Pane::SplitState splitType, const std::optional<GUID>& profileGuid);
        void _ResizePane(const Direction& direction);
        void _ScrollPage(int delta);
        void _SelectTab(const int tabIndex);
        void _OpenSettings();
        fire_and_forget LaunchSettings();
        fire_and_forget _ShowDialog(const IInspectable& titleElement,
                                    const IInspectable& contentElement,
                                    const winrt::hstring& closeButtonText);
        void _ShowAboutDialog();
        static Windows::UI::Xaml::Controls::IconElement _GetIconFromProfile(const ::TerminalApp::Profile& profile);
        void _SetAcceleratorForMenuItem(Windows::UI::Xaml::Controls::MenuFlyoutItem& menuItem, const winrt::Microsoft::Terminal::Settings::KeyChord& keyChord);

        void _CopyToClipboardHandler(const winrt::hstring& copiedData);
        void _PasteFromClipboardHandler(const IInspectable& sender,
                                        const Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs& eventArgs);
        void _CopyText(const bool trimTrailingWhitespace);
        void _PasteText();
        static fire_and_forget PasteFromClipboard(winrt::Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs eventArgs);

        void _RemoveTabViewItem(const IInspectable& tabViewItem);

        winrt::Microsoft::Terminal::TerminalControl::TermControl _GetFocusedControl();
        int _GetFocusedTabIndex() const;
        void _SetFocusedTabIndex(int tabIndex);

        void _OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
        void _OnTabSelectionChanged(const IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& eventArgs);
        void _OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs);
        void _OnContentSizeChanged(const IInspectable& /*sender*/, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _OnTabClosing(const IInspectable& sender, const Microsoft::UI::Xaml::Controls::TabViewTabClosingEventArgs& eventArgs);
        void _SettingsButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _FeedbackButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _AboutButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage, implementation::TerminalPage>
    {
    };
}
