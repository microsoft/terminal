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

        Automation::AutomationProperties::SetHelpText(DefaultsNavigator(), RS_(L"Profiles_DefaultsNavigator/Description"));
        Automation::AutomationProperties::SetHelpText(ColorSchemesNavigator(), RS_(L"Profiles_ColorSchemesNavigator/Description"));
        Automation::AutomationProperties::SetName(AddProfileButton(), RS_(L"Profiles_AddProfileButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
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

    // The primary "+" half of the SplitButton — adds either a new empty
    // profile or a duplicate of the currently selected source profile,
    // depending on what the user picked from the flyout.
    void Profiles::AddProfile_Click(const winrt::Microsoft::UI::Xaml::Controls::SplitButton& /*sender*/,
                                    const winrt::Microsoft::UI::Xaml::Controls::SplitButtonClickEventArgs& /*args*/)
    {
        const auto selected = _ViewModel.SelectedSourceProfile();
        const auto sourceGuid = selected ? selected.OriginalProfileGuid() : winrt::guid{};

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "AddNewProfile",
            TraceLoggingDescription("Event emitted when the user adds a new profile"),
            TraceLoggingValue(selected ? "Duplicate" : "EmptyProfile", "Type", "The type of the creation method (i.e. empty profile, duplicate)"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        _ViewModel.RequestAddProfile(sourceGuid);
    }

    // Rebuilds the MenuFlyout each time it opens so newly added/removed
    // profiles are reflected without having to subscribe to VectorChanged.
    // Selecting a flyout item updates which profile the SplitButton's primary
    // action will use as its source — it does NOT add a profile on its own.
    void Profiles::AddProfileFlyout_Opening(const IInspectable& sender, const IInspectable& /*args*/)
    {
        const auto flyout = sender.try_as<MenuFlyout>();
        if (!flyout)
        {
            return;
        }

        auto items = flyout.Items();
        items.Clear();

        constexpr std::wstring_view groupName{ L"ProfilesAddProfileSource" };
        const auto selected = _ViewModel.SelectedSourceProfile();
        const auto weakViewModel = winrt::make_weak(_ViewModel);

        // "New empty profile" item — picking this resets the SplitButton to its
        // default state where the primary action creates a brand-new profile.
        {
            winrt::Microsoft::UI::Xaml::Controls::RadioMenuFlyoutItem newEmptyItem;
            newEmptyItem.Text(RS_(L"Profiles_AddProfileMenu_NewEmptyProfile/Text"));
            newEmptyItem.GroupName(groupName);
            newEmptyItem.IsChecked(!selected);

            Controls::FontIcon plusIcon;
            plusIcon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            plusIcon.Glyph(L"\xE710");
            newEmptyItem.Icon(plusIcon);

            newEmptyItem.Click([weakViewModel](const auto&, const auto&) {
                if (const auto vm = weakViewModel.get())
                {
                    vm.SelectSourceProfile(nullptr);
                }
            });

            items.Append(newEmptyItem);
        }

        items.Append(MenuFlyoutSeparator{});

        // One RadioMenuFlyoutItem per existing profile — picking one swaps the
        // SplitButton's primary action over to "duplicate this profile".
        for (const auto& profileVM : _ViewModel.Profiles())
        {
            winrt::Microsoft::UI::Xaml::Controls::RadioMenuFlyoutItem profileItem;
            profileItem.Text(profileVM.Name());
            profileItem.Icon(profileVM.IconPreview());
            profileItem.GroupName(groupName);
            profileItem.IsChecked(selected && selected.OriginalProfileGuid() == profileVM.OriginalProfileGuid());

            const auto weakProfile = winrt::make_weak(profileVM);
            profileItem.Click([weakViewModel, weakProfile](const auto&, const auto&) {
                const auto vm = weakViewModel.get();
                const auto profile = weakProfile.get();
                if (vm && profile)
                {
                    vm.SelectSourceProfile(profile);
                }
            });

            items.Append(profileItem);
        }
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

    void ProfilesPageViewModel::RequestAddProfile(const winrt::guid& sourceProfile)
    {
        AddProfileRequested.raise(*this, sourceProfile);
    }

    void ProfilesPageViewModel::SelectSourceProfile(const Editor::ProfileViewModel& profile)
    {
        if (_SelectedSourceProfile == profile)
        {
            return;
        }

        _SelectedSourceProfile = profile;
        _NotifyChanges(L"SelectedSourceProfile",
                       L"HasSelectedSourceProfile",
                       L"SelectedSourceProfileLabel",
                       L"SelectedSourceProfileIcon");
    }

    winrt::hstring ProfilesPageViewModel::SelectedSourceProfileLabel() const
    {
        if (_SelectedSourceProfile)
        {
            return _SelectedSourceProfile.Name();
        }
        return RS_(L"Profiles_AddProfileButton_DefaultLabel/Text");
    }

    Windows::UI::Xaml::Controls::IconElement ProfilesPageViewModel::SelectedSourceProfileIcon() const
    {
        if (_SelectedSourceProfile)
        {
            return _SelectedSourceProfile.IconPreview();
        }
        return nullptr;
    }

    void ProfilesPageViewModel::RequestOpenProfile(const Editor::ProfileViewModel& profile)
    {
        OpenProfileRequested.raise(*this, profile);
    }

}
