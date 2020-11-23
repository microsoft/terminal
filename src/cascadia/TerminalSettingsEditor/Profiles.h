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

        GETSET_PROPERTY(Editor::ProfilePageNavigationState, State, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(CursorShape, winrt::Microsoft::Terminal::TerminalControl::CursorStyle, State().Profile, CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, State().Profile, BackgroundImageStretchMode);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode, State().Profile, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, State().Profile, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, State().Profile, BellStyle);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, winrt::Microsoft::Terminal::TerminalControl::ScrollbarState, State().Profile, ScrollState);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
