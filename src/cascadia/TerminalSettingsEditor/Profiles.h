// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles.g.h"
#include "ProfilePageNavigationState.g.h"
#include "ProfileViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfileViewModel : ProfileViewModelT<ProfileViewModel>, ViewModelHelper<ProfileViewModel>
    {
    public:
        ProfileViewModel(const Model::Profile& profile) :
            _profile{ profile } {}

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, Guid);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_profile, ConnectionType);
        OBSERVABLE_PROJECTED_SETTING(_profile, Name);
        OBSERVABLE_PROJECTED_SETTING(_profile, Source);
        OBSERVABLE_PROJECTED_SETTING(_profile, Hidden);
        OBSERVABLE_PROJECTED_SETTING(_profile, Icon);
        OBSERVABLE_PROJECTED_SETTING(_profile, CloseOnExit);
        OBSERVABLE_PROJECTED_SETTING(_profile, TabTitle);
        OBSERVABLE_PROJECTED_SETTING(_profile, TabColor);
        OBSERVABLE_PROJECTED_SETTING(_profile, SuppressApplicationTitle);
        OBSERVABLE_PROJECTED_SETTING(_profile, UseAcrylic);
        OBSERVABLE_PROJECTED_SETTING(_profile, AcrylicOpacity);
        OBSERVABLE_PROJECTED_SETTING(_profile, ScrollState);
        OBSERVABLE_PROJECTED_SETTING(_profile, FontFace);
        OBSERVABLE_PROJECTED_SETTING(_profile, FontSize);
        OBSERVABLE_PROJECTED_SETTING(_profile, FontWeight);
        OBSERVABLE_PROJECTED_SETTING(_profile, Padding);
        OBSERVABLE_PROJECTED_SETTING(_profile, Commandline);
        OBSERVABLE_PROJECTED_SETTING(_profile, StartingDirectory);
        OBSERVABLE_PROJECTED_SETTING(_profile, BackgroundImagePath);
        OBSERVABLE_PROJECTED_SETTING(_profile, BackgroundImageOpacity);
        OBSERVABLE_PROJECTED_SETTING(_profile, BackgroundImageStretchMode);
        OBSERVABLE_PROJECTED_SETTING(_profile, BackgroundImageAlignment);
        OBSERVABLE_PROJECTED_SETTING(_profile, AntialiasingMode);
        OBSERVABLE_PROJECTED_SETTING(_profile, RetroTerminalEffect);
        OBSERVABLE_PROJECTED_SETTING(_profile, ForceFullRepaintRendering);
        OBSERVABLE_PROJECTED_SETTING(_profile, SoftwareRendering);
        OBSERVABLE_PROJECTED_SETTING(_profile, ColorSchemeName);
        OBSERVABLE_PROJECTED_SETTING(_profile, Foreground);
        OBSERVABLE_PROJECTED_SETTING(_profile, Background);
        OBSERVABLE_PROJECTED_SETTING(_profile, SelectionBackground);
        OBSERVABLE_PROJECTED_SETTING(_profile, CursorColor);
        OBSERVABLE_PROJECTED_SETTING(_profile, HistorySize);
        OBSERVABLE_PROJECTED_SETTING(_profile, SnapOnInput);
        OBSERVABLE_PROJECTED_SETTING(_profile, AltGrAliasing);
        OBSERVABLE_PROJECTED_SETTING(_profile, CursorShape);
        OBSERVABLE_PROJECTED_SETTING(_profile, CursorHeight);
        OBSERVABLE_PROJECTED_SETTING(_profile, BellStyle);

    private:
        Model::Profile _profile;
    };

    struct ProfilePageNavigationState : ProfilePageNavigationStateT<ProfilePageNavigationState>
    {
    public:
        ProfilePageNavigationState(const Editor::ProfileViewModel& viewModel, const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes, const IHostedInWindow& windowRoot) :
            _Profile{ viewModel },
            _Schemes{ schemes },
            _WindowRoot{ windowRoot }
        {
        }

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> Schemes() { return _Schemes; }
        void Schemes(const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& val) { _Schemes = val; }

        GETSET_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        GETSET_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

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
        fire_and_forget Icon_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void BIAlignment_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        // CursorShape visibility logic
        void CursorShape_Changed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        bool IsVintageCursor() const;

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
        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
