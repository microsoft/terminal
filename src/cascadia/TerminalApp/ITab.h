// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "inc/cppwinrt_utils.h"
#include "ITab.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct ITab : ITabT<ITab>
    {
    public:
        void Focus(winrt::Windows::UI::Xaml::FocusState focusState);
        winrt::Windows::UI::Xaml::FocusState FocusState() const noexcept;

        void Shutdown();
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        void UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs);

        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        // This is needed since Tab is going to be managing its own SwitchToTab command.
        GETSET_PROPERTY(uint32_t, TabViewIndex, 0);
        // The TabViewNumTabs is the number of Tab objects in TerminalPage's _tabs vector.
        GETSET_PROPERTY(uint32_t, TabViewNumTabs, 0);

        GETSET_PROPERTY(winrt::hstring, Title);
        GETSET_PROPERTY(winrt::hstring, Icon);
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::Command, SwitchToTabCommand, nullptr);
        GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewItem, TabViewItem, nullptr);
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::Page, Content, nullptr);

    protected:
        winrt::Windows::UI::Xaml::FocusState _focusState{ winrt::Windows::UI::Xaml::FocusState::Unfocused };
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeOtherTabsMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeTabsAfterMenuItem{};
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        void _CreateContextMenu();
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem _CreateCloseSubMenu();
        void _EnableCloseMenuItems();
        void _CloseTabsAfter();
        void _CloseOtherTabs();

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
