// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles.g.h"
#include "ProfilePageNavigationState.g.h"
#include "DeleteProfileEventArgs.g.h"
#include "ProfileViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "Appearances.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfileViewModel : ProfileViewModelT<ProfileViewModel>, ViewModelHelper<ProfileViewModel>
    {
    public:
        ProfileViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings);

        Model::TerminalSettings TermSettings() const;

        // starting directory
        bool UseParentProcessDirectory();
        void UseParentProcessDirectory(const bool useParent);
        bool UseCustomStartingDirectory();

        // font face
        static void UpdateFontList() noexcept;
        Windows::Foundation::Collections::IObservableVector<Editor::Font> CompleteFontList() const noexcept;
        Windows::Foundation::Collections::IObservableVector<Editor::Font> MonospaceFontList() const noexcept;

        // general profile knowledge
        winrt::guid OriginalProfileGuid() const noexcept;
        bool CanDeleteProfile() const;
        Editor::AppearanceViewModel DefaultAppearance();
        Editor::AppearanceViewModel UnfocusedAppearance();
        bool HasUnfocusedAppearance();
        void CreateUnfocusedAppearance(const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes,
                                       const IHostedInWindow& windowRoot);

        WINRT_PROPERTY(bool, IsBaseLayer, false);

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
        OBSERVABLE_PROJECTED_SETTING(_profile, Padding);
        OBSERVABLE_PROJECTED_SETTING(_profile, Commandline);
        OBSERVABLE_PROJECTED_SETTING(_profile, StartingDirectory);
        OBSERVABLE_PROJECTED_SETTING(_profile, AntialiasingMode);
        OBSERVABLE_PROJECTED_SETTING(_profile, ForceFullRepaintRendering);
        OBSERVABLE_PROJECTED_SETTING(_profile, SoftwareRendering);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), Foreground);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), Background);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), SelectionBackground);
        OBSERVABLE_PROJECTED_SETTING(_profile.DefaultAppearance(), CursorColor);
        OBSERVABLE_PROJECTED_SETTING(_profile, HistorySize);
        OBSERVABLE_PROJECTED_SETTING(_profile, SnapOnInput);
        OBSERVABLE_PROJECTED_SETTING(_profile, AltGrAliasing);
        OBSERVABLE_PROJECTED_SETTING(_profile, BellStyle);

    private:
        Model::Profile _profile;
        winrt::guid _originalProfileGuid;
        winrt::hstring _lastBgImagePath;
        winrt::hstring _lastStartingDirectoryPath;
        Editor::AppearanceViewModel _defaultAppearanceViewModel;

        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _MonospaceFontList;
        static Windows::Foundation::Collections::IObservableVector<Editor::Font> _FontList;

        static Editor::Font _GetFont(com_ptr<IDWriteLocalizedStrings> localizedFamilyNames);

        Model::CascadiaSettings _appSettings;
        Editor::AppearanceViewModel _unfocusedAppearanceViewModel;
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
            // If there was a previous nav state copy the selected pivot from it.
            if (lastState)
            {
                _LastActivePivot = lastState.LastActivePivot();
            }
            viewModel.DefaultAppearance().Schemes(schemes);
            viewModel.DefaultAppearance().WindowRoot(windowRoot);

            if (viewModel.UnfocusedAppearance())
            {
                viewModel.UnfocusedAppearance().Schemes(schemes);
                viewModel.UnfocusedAppearance().WindowRoot(windowRoot);
            }

            const auto revoker = viewModel.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
                const auto settingName{ args.PropertyName() };
                if (settingName == L"UnfocusedAppearance")
                {
                    viewModel.UnfocusedAppearance().Schemes(schemes);
                    viewModel.UnfocusedAppearance().WindowRoot(windowRoot);
                }
            });
        }

        void DeleteProfile();
        void CreateUnfocusedAppearance();

        TYPED_EVENT(DeleteProfile, Editor::ProfilePageNavigationState, Editor::DeleteProfileEventArgs);
        WINRT_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        WINRT_PROPERTY(Editor::ProfilesPivots, LastActivePivot, Editor::ProfilesPivots::General);
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> _Schemes;
    };

    struct Profiles : ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        // bell style bits
        bool IsBellStyleFlagSet(const uint32_t flag);
        void SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on);
        void SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on);

        fire_and_forget Commandline_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget StartingDirectory_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        fire_and_forget Icon_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void DeleteConfirmation_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void Pivot_SelectionChanged(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void Expander_Expanded(Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::ExpanderExpandingEventArgs const& e);
        void Expander_Collapsed(Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::ExpanderCollapsedEventArgs const& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(Editor::ProfilePageNavigationState, State, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(AntiAliasingMode, Microsoft::Terminal::Control::TextAntialiasingMode, State().Profile, AntialiasingMode);
        GETSET_BINDABLE_ENUM_SETTING(CloseOnExitMode, Microsoft::Terminal::Settings::Model::CloseOnExitMode, State().Profile, CloseOnExit);
        GETSET_BINDABLE_ENUM_SETTING(ScrollState, Microsoft::Terminal::Control::ScrollbarState, State().Profile, ScrollState);

    private:
        void _UpdateBIAlignmentControl(const int32_t val);

        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _AppearanceViewModelChangedRevoker;

        Microsoft::Terminal::Control::TermControl _previewControl;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
