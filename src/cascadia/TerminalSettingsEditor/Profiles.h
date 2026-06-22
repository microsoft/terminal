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
        void RequestAddProfile(const winrt::guid& sourceProfile);
        void RequestOpenProfile(const Editor::ProfileViewModel& profile);

        Editor::ProfileViewModel SelectedSourceProfile() const noexcept { return _SelectedSourceProfile; }
        void SelectSourceProfile(const Editor::ProfileViewModel& profile);
        bool HasSelectedSourceProfile() const noexcept { return static_cast<bool>(_SelectedSourceProfile); }
        winrt::hstring SelectedSourceProfileLabel() const;
        Windows::UI::Xaml::Controls::IconElement SelectedSourceProfileIcon() const;

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<ProfilesPageViewModel>::PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ProfileViewModel>, Profiles, _propertyChangedHandlers, nullptr);

    public:
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> OpenDefaultsRequested;
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> OpenColorSchemesRequested;
        til::typed_event<Windows::Foundation::IInspectable, winrt::guid> AddProfileRequested;
        til::typed_event<Windows::Foundation::IInspectable, Editor::ProfileViewModel> OpenProfileRequested;

    private:
        Editor::ProfileViewModel _SelectedSourceProfile{ nullptr };
    };

    struct Profiles : public HasScrollViewer<Profiles>, ProfilesT<Profiles>
    {
    public:
        Profiles();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void AddProfile_Click(const Microsoft::UI::Xaml::Controls::SplitButton& sender, const Microsoft::UI::Xaml::Controls::SplitButtonClickEventArgs& args);
        void AddProfileFlyout_Opening(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& args);
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
