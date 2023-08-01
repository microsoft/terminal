// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "SettingsPaneContent.g.h"
#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    struct SettingsPaneContent : SettingsPaneContentT<SettingsPaneContent>
    {
        SettingsPaneContent(winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings settings);

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();
        winrt::Microsoft::Terminal::Settings::Editor::MainPage SettingsUI() { return _sui; }

        winrt::Windows::Foundation::Size MinSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool asContent) const;

        winrt::hstring Title() { return RS_(L"SettingsTab"); }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }

        til::typed_event<> CloseRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<> TitleChanged;
        til::typed_event<> TabColorChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<> FocusRequested;

    private:
        winrt::Microsoft::Terminal::Settings::Editor::MainPage _sui{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SettingsPaneContent);
}
