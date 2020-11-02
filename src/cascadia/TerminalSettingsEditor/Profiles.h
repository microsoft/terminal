
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Utils.h"
#include "Profiles.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles : ProfilesT<Profiles>
    {
   
        Profiles();
        Profiles(winrt::Microsoft::Terminal::Settings::Model::Profile profile);

        void ColorSchemeSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> ColorSchemeList();

        fire_and_forget BackgroundImage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Commandline_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        // TODO GH#1564: Settings UI
        // This crashes on click, for some reason
        //fire_and_forget StartingDirectory_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::Profile, Profile);

    private:
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _ColorSchemeList{ nullptr };
        void _UpdateColorSchemeList();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
