/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Profiles.h

Abstract:
- The Profiles landing page in the Settings UI. The page hosts entry points to
  the list of individual profiles among other profile-related settings.

--*/

#pragma once

#include "Profiles.g.h"
#include "ProfilesPageViewModel.g.h"
#include "ProfileViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfilesPageViewModel : ProfilesPageViewModelT<ProfilesPageViewModel>, ViewModelHelper<ProfilesPageViewModel>
    {
    public:
        ProfilesPageViewModel();

        void RequestOpenDefaults();
        void RequestOpenColorSchemes();
        void RequestAddProfile();
        void RequestOpenProfile(const Editor::ProfileViewModel& profile);

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<ProfilesPageViewModel>::PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ProfileViewModel>, Profiles, _propertyChangedHandlers, nullptr);

    public:
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> OpenDefaultsRequested;
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> OpenColorSchemesRequested;
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> AddProfileRequested;
        til::typed_event<Windows::Foundation::IInspectable, Editor::ProfileViewModel> OpenProfileRequested;
    };

    struct Profiles : public HasScrollViewer<Profiles>, ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void Defaults_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ColorSchemes_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void AddProfile_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void Profile_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        til::property_changed_event PropertyChanged;
        WINRT_PROPERTY(Editor::ProfilesPageViewModel, ViewModel, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles);
    BASIC_FACTORY(ProfilesPageViewModel);
}
