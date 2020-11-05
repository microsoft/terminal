
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Utils.h"
#include "Profiles.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles : ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs e);

        fire_and_forget BackgroundImage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Commandline_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        // TODO GH#1564: Settings UI
        // This crashes on click, for some reason
        //fire_and_forget StartingDirectory_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::Profile, Profile);
        GETSET_BINDABLE_ENUM_SETTING(CursorShape, winrt::Microsoft::Terminal::TerminalControl::CursorStyle, Profile, CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, Profile, BackgroundImageStretchMode);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode, Profile, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, Profile, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, Profile, BellStyle);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, winrt::Microsoft::Terminal::TerminalControl::ScrollbarState, Profile, ScrollState);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
