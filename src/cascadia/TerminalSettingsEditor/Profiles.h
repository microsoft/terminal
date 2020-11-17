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
        ProfilePageNavigationState(const Model::Profile& profile, const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes) :
            _Profile{ profile },
            _Schemes{ schemes } {}

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> Schemes() { return _Schemes; }
        void Schemes(const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& val) { _Schemes = val; }

        GETSET_PROPERTY(Model::Profile, Profile, nullptr);

    private:
        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> _Schemes;
    };

    struct Profiles : ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        fire_and_forget BackgroundImage_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Commandline_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        // TODO GH#1564: Settings UI
        // This crashes on click, for some reason
        //fire_and_forget StartingDirectory_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        GETSET_PROPERTY(Editor::ProfilePageNavigationState, State, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(CursorShape, winrt::Microsoft::Terminal::TerminalControl::CursorStyle, State().Profile, CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, State().Profile, BackgroundImageStretchMode);
        //GETSET_BINDABLE_ENUM_SETTING(BackgroundImageAlignment, winrt::Microsoft::Terminal::Settings::Model::Alignment, State().Profile, BackgroundImageAlignment);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode, State().Profile, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, State().Profile, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, State().Profile, BellStyle);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, winrt::Microsoft::Terminal::TerminalControl::ScrollbarState, State().Profile, ScrollState);

    public:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> BackgroundImageAlignmentList()
        {
            return _BackgroundImageAlignmentList;
        }

        winrt::Windows::Foundation::IInspectable CurrentBackgroundImageAlignment()
        {
            const auto val{ State().Profile().BackgroundImageAlignment() };
            const auto res{ _BackgroundImageAlignmentMap.Lookup(val) };
            return winrt::box_value<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(res);
            //return winrt::box_value<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(_BackgroundImageAlignmentMap.Lookup(State().Profile().BackgroundImageAlignment()));
        }

        void CurrentBackgroundImageAlignment(const winrt::Windows::Foundation::IInspectable& enumEntry)
        {
            if (auto ee = enumEntry.try_as<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>())
            {
                auto setting = winrt::unbox_value<Model::Alignment>(ee.EnumValue());
                State().Profile().BackgroundImageAlignment(setting);
            }
        }

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _BackgroundImageAlignmentList;
        winrt::Windows::Foundation::Collections::IMap<Model::Alignment, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _BackgroundImageAlignmentMap;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
