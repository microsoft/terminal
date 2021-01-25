// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles.g.h"
#include "ProfilePageNavigationState.g.h"
#include "DeleteProfileEventArgs.g.h"
#include "ProfileViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfileViewModel : ProfileViewModelT<ProfileViewModel>, ViewModelHelper<ProfileViewModel>
    {
    public:
        ProfileViewModel(const Model::Profile& profile);

        bool CanDeleteProfile() const;

        bool UseDesktopBGImage();
        void UseDesktopBGImage(const bool useDesktop);
        bool UseParentProcessDirectory();
        void UseParentProcessDirectory(const bool useParent);
        bool UseCustomStartingDirectory();
        bool BackgroundImageSettingsVisible();

        GETSET_PROPERTY(bool, IsBaseLayer, false);

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
        winrt::hstring _lastBgImagePath;
        winrt::hstring _lastStartingDirectoryPath;
    };

    struct DeleteProfileEventArgs :
        public DeleteProfileEventArgsT<DeleteProfileEventArgs>
    {
    public:
        DeleteProfileEventArgs(guid profileGuid) :
            _ProfileGuid(profileGuid) {}

        guid ProfileGuid() const noexcept { return _ProfileGuid; }

    private:
        guid _ProfileGuid{};
    };

    struct ProfilePageNavigationState : ProfilePageNavigationStateT<ProfilePageNavigationState>
    {
    public:
        ProfilePageNavigationState(const Editor::ProfileViewModel& viewModel,
                                   const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes,
                                   const Editor::ProfilePageNavigationState& lastState,
                                   const IHostedInWindow& windowRoot) :
            _Profile{ viewModel },
            _Schemes{ schemes },
            _WindowRoot{ windowRoot }
        {
            // If there was a previous nav state, and it was for the same
            // profile, then copy the selected pivot from it.
            if (lastState)
            {
                const auto& oldGuid = lastState.Profile().Guid();
                const auto& newGuid = _Profile.Guid();
                if (oldGuid == newGuid)
                {
                    _LastActivePivot = lastState.LastActivePivot();
                }
            }
        }

        void DeleteProfile();

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> Schemes() { return _Schemes; }
        void Schemes(const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& val) { _Schemes = val; }

        TYPED_EVENT(DeleteProfile, Editor::ProfilePageNavigationState, Editor::DeleteProfileEventArgs);
        GETSET_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        GETSET_PROPERTY(Editor::ProfilesPivots, LastActivePivot, Editor::ProfilesPivots::General);
        GETSET_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> _Schemes;
    };

    struct Profiles : ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        Model::ColorScheme CurrentColorScheme();
        void CurrentColorScheme(const Model::ColorScheme& val);

        fire_and_forget BackgroundImage_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Commandline_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget StartingDirectory_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Icon_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void BIAlignment_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void DeleteConfirmation_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void UseParentProcessDirectory_Check(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void UseParentProcessDirectory_Uncheck(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void Pivot_SelectionChanged(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);

        // CursorShape visibility logic
        void CursorShape_Changed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        bool IsVintageCursor() const;

        // manually bind FontWeight
        Windows::Foundation::IInspectable CurrentFontWeight() const;
        void CurrentFontWeight(const Windows::Foundation::IInspectable& enumEntry);
        bool IsCustomFontWeight();
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Editor::EnumEntry>, FontWeightList);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        GETSET_PROPERTY(Editor::ProfilePageNavigationState, State, nullptr);
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::ColorScheme>, ColorSchemeList, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle, State().Profile, CursorShape);
        GETSET_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch, State().Profile, BackgroundImageStretchMode);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, Microsoft::Terminal::TerminalControl::TextAntialiasingMode, State().Profile, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, Microsoft::Terminal::Settings::Model::CloseOnExitMode, State().Profile, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(BellStyle, Microsoft::Terminal::Settings::Model::BellStyle, State().Profile, BellStyle);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, Microsoft::Terminal::TerminalControl::ScrollbarState, State().Profile, ScrollState);

    private:
        Windows::Foundation::Collections::IMap<uint16_t, Microsoft::Terminal::Settings::Editor::EnumEntry> _FontWeightMap;
        Editor::EnumEntry _CustomFontWeight{ nullptr };
        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
