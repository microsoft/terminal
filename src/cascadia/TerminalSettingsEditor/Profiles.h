// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles.g.h"
#include "ProfilePageNavigationState.g.h"
#include "ProfileViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "Appearances.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfilePageNavigationState : ProfilePageNavigationStateT<ProfilePageNavigationState>
    {
    public:
        ProfilePageNavigationState(const Editor::ProfileViewModel& viewModel,
                                   const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes,
                                   const IHostedInWindow& windowRoot) :
            _Profile{ viewModel }
        {
            auto profile{ winrt::get_self<ProfileViewModel>(viewModel) };
            profile->Schemes(schemes);
            profile->WindowRoot(windowRoot);

            viewModel.DefaultAppearance().Schemes(schemes);
            viewModel.DefaultAppearance().WindowRoot(windowRoot);

            if (viewModel.UnfocusedAppearance())
            {
                viewModel.UnfocusedAppearance().Schemes(schemes);
                viewModel.UnfocusedAppearance().WindowRoot(windowRoot);
            }
        }
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);
    };

    struct Profiles : public HasScrollViewer<Profiles>, ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(Editor::ProfilePageNavigationState, State, nullptr);

    private:
        void _UpdateBIAlignmentControl(const int32_t val);

        std::array<Windows::UI::Xaml::Controls::Primitives::ToggleButton, 9> _BIAlignmentButtons;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _AppearanceViewModelChangedRevoker;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
}
