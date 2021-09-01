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

namespace winrt::TerminalApp::implementation
{
    SettingsTab::SettingsTab(MainPage settingsUI)
    {
        Content(settingsUI);

        _MakeTabViewItem();
        _CreateContextMenu();
        _CreateIcon();
    }

    void SettingsTab::UpdateSettings(CascadiaSettings settings)
    {
        auto settingsUI{ Content().as<MainPage>() };
        settingsUI.UpdateSettings(settings);
    }

    // Method Description:
    // - Focus the settings UI
    // Arguments:
    // - focusState: The FocusState mode by which focus is to be obtained.
    // Return Value:
    // - <none>
    void SettingsTab::Focus(WUX::FocusState focusState)
    {
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
    winrt::fire_and_forget SettingsTab::_CreateIcon()
    {
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(TabViewItem().Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            auto fontFamily = winrt::WUX::Media::FontFamily(L"Segoe MDL2 Assets");
            auto glyph = L"\xE713"; // This is the Setting icon (looks like a gear)

            // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
            Icon(glyph);
            TabViewItem().IconSource(IconPathConverter::IconSourceMUX(glyph));
        }
    }
}
