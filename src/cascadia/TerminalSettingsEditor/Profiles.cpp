// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"
#include "ProfilesPageViewModel.g.cpp"
#include "ProfileViewModel.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles::Profiles()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(DefaultsNavigator(), RS_(L"Profiles_DefaultsNavigator_Title/Text"));
        Automation::AutomationProperties::SetName(ColorSchemesNavigator(), RS_(L"Profiles_ColorSchemesNavigator_Title/Text"));
        Automation::AutomationProperties::SetName(AddProfileButton(), RS_(L"Profiles_AddProfileButton/Text"));
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _ViewModel = args.ViewModel().as<Editor::ProfilesPageViewModel>();
        BringIntoViewWhenLoaded(args.ElementToFocus());

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("profilesLanding", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Profiles::Defaults_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _ViewModel.RequestOpenDefaults();
    }

    void Profiles::ColorSchemes_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _ViewModel.RequestOpenColorSchemes();
    }

    void Profiles::AddProfile_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _ViewModel.RequestAddProfile();
    }

    void Profiles::Profile_Click(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        // Profile navigators are buttons whose DataContext is the bound ProfileViewModel.
        if (const auto element = sender.try_as<FrameworkElement>())
        {
            if (const auto profile = element.DataContext().try_as<Editor::ProfileViewModel>())
            {
                _ViewModel.RequestOpenProfile(profile);
            }
        }
    }

    ProfilesPageViewModel::ProfilesPageViewModel()
    {
        _setProfiles(single_threaded_observable_vector<Editor::ProfileViewModel>());
    }

    void ProfilesPageViewModel::RequestOpenDefaults()
    {
        OpenDefaultsRequested.raise(*this, nullptr);
    }

    void ProfilesPageViewModel::RequestOpenColorSchemes()
    {
        OpenColorSchemesRequested.raise(*this, nullptr);
    }

    void ProfilesPageViewModel::RequestAddProfile()
    {
        AddProfileRequested.raise(*this, nullptr);
    }

    void ProfilesPageViewModel::RequestOpenProfile(const Editor::ProfileViewModel& profile)
    {
        OpenProfileRequested.raise(*this, profile);
    }

}
