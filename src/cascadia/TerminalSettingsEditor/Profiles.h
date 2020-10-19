
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
        Profiles(winrt::Microsoft::Terminal::Settings::Model::Profile profile);

        fire_and_forget BackgroundImage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Commandline_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        // TODO GH#1564: Settings UI
        // This crashes on click, for some reason
        //fire_and_forget StartingDirectory_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::Profile, Profile);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
