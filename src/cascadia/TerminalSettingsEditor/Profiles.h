// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles.g.h"
#include "ProfilePageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfilePageNavigationState : ProfilePageNavigationStateT<ProfilePageNavigationState>
    {
    public:
        ProfilePageNavigationState(const Model::Profile& profile, const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes, const IHostedInWindow& windowRoot) :
            _Profile{ profile },
            _Schemes{ schemes },
            _WindowRoot{ windowRoot }
        {
        }

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> Schemes() { return _Schemes; }
        void Schemes(const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& val) { _Schemes = val; }

        GETSET_PROPERTY(Model::Profile, Profile, nullptr);
        GETSET_PROPERTY(IHostedInWindow, WindowRoot, nullptr);

    private:
        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> _Schemes;
    };

    struct Profiles : ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        Model::ColorScheme CurrentColorScheme();
        void CurrentColorScheme(const Model::ColorScheme& val);

        fire_and_forget BackgroundImage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Commandline_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget StartingDirectory_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        // manually bind FontWeight
        winrt::Windows::Foundation::IInspectable CurrentFontWeight() const;
        void CurrentFontWeight(const winrt::Windows::Foundation::IInspectable& enumEntry);
        bool IsCustomFontWeight();
        GETSET_PROPERTY(winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>, FontWeightList);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        GETSET_PROPERTY(Editor::ProfilePageNavigationState, State, nullptr);
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::ColorScheme>, ColorSchemeList, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(CursorShape, winrt::Microsoft::Terminal::TerminalControl::CursorStyle, State().Profile, CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, State().Profile, BackgroundImageStretchMode);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode, State().Profile, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, State().Profile, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, State().Profile, BellStyle);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, winrt::Microsoft::Terminal::TerminalControl::ScrollbarState, State().Profile, ScrollState);

    private:
        winrt::Windows::Foundation::Collections::IMap<uint16_t, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _FontWeightMap;
        Editor::EnumEntry _CustomFontWeight{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
