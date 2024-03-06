// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "SettingsTab.h"
#include "SettingsTab.g.cpp"
#include "Utils.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Editor;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

#define ASSERT_UI_THREAD() assert(TabViewItem().Dispatcher().HasThreadAccess())

namespace winrt::TerminalApp::implementation
{
    SettingsTab::SettingsTab(MainPage settingsUI,
                             winrt::Windows::UI::Xaml::ElementTheme requestedTheme)
    {
        Content(settingsUI);
        _requestedTheme = requestedTheme;

        _MakeTabViewItem();
        _CreateContextMenu();
        _CreateIcon();
    }

    void SettingsTab::UpdateSettings(CascadiaSettings settings)
    {
        ASSERT_UI_THREAD();

        auto settingsUI{ Content().as<MainPage>() };
        settingsUI.UpdateSettings(settings);

        // Stash away the current requested theme of the app. We'll need that in
        // _BackgroundBrush() to do a theme-aware resource lookup
        _requestedTheme = settings.GlobalSettings().CurrentTheme().RequestedTheme();
    }

    // Method Description:
    // - Creates a list of actions that can be run to recreate the state of this tab
    // Arguments:
    // - asContent: unused. There's nothing different we need to do when
    //   serializing the settings tab for moving to another window. If we ever
    //   really want to support opening the SUI to a specific page, we can
    //   re-evaluate including that arg in this action then.
    //  Return Value:
    // - The list of actions.
    std::vector<ActionAndArgs> SettingsTab::BuildStartupActions(const bool /*asContent*/) const
    {
        ASSERT_UI_THREAD();

        ActionAndArgs action;
        action.Action(ShortcutAction::OpenSettings);
        OpenSettingsArgs args{ SettingsTarget::SettingsUI };
        action.Args(args);

        return std::vector{ std::move(action) };
    }

    // Method Description:
    // - Focus the settings UI
    // Arguments:
    // - focusState: The FocusState mode by which focus is to be obtained.
    // Return Value:
    // - <none>
    void SettingsTab::Focus(WUX::FocusState focusState)
    {
        ASSERT_UI_THREAD();

        _focusState = focusState;

        if (_focusState != FocusState::Unfocused)
        {
            Content().as<WUX::Controls::Page>().Focus(focusState);
        }
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_MakeTabViewItem()
    {
        TabBase::_MakeTabViewItem();

        Title(RS_(L"SettingsTab"));
        TabViewItem().Header(winrt::box_value(Title()));
    }

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SettingsTab::_CreateIcon()
    {
        // This is the Setting icon (looks like a gear)
        static constexpr std::wstring_view glyph{ L"\xE713" };

        // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
        Icon(winrt::hstring{ glyph });
        TabViewItem().IconSource(Microsoft::Terminal::UI::IconPathConverter::IconSourceMUX(glyph, false));
    }

    winrt::Windows::UI::Xaml::Media::Brush SettingsTab::_BackgroundBrush()
    {
        // Look up the color we should use for the settings tab item from our
        // resources. This should only be used for when "terminalBackground" is
        // requested.
        static const auto key = winrt::box_value(L"SettingsUiTabBrush");
        // You can't just do a Application::Current().Resources().TryLookup
        // lookup, cause the app theme never changes! Do the hacky version
        // instead.
        return ThemeLookup(Application::Current().Resources(), _requestedTheme, key).try_as<winrt::Windows::UI::Xaml::Media::Brush>();
    }
}
