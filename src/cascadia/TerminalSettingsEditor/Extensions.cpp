// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Extensions.h"
#include "Extensions.g.cpp"
#include "ExtensionsViewModel.g.cpp"
#include "FragmentProfileViewModel.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Extensions::Extensions()
    {
        InitializeComponent();
    }

    void Extensions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ExtensionsViewModel>();
    }

    void Extensions::ExtensionLoaded(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto& toggleSwitch = sender.as<Controls::ToggleSwitch>();
        const auto& extensionSource = toggleSwitch.Tag().as<hstring>();
        toggleSwitch.IsOn(_ViewModel.GetExtensionState(extensionSource));
    }

    void Extensions::ExtensionToggled(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto& toggleSwitch = sender.as<Controls::ToggleSwitch>();
        const auto& extensionSource = toggleSwitch.Tag().as<hstring>();
        _ViewModel.SetExtensionState(extensionSource, toggleSwitch.IsOn());
    }

    void Extensions::FragmentNavigator_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto& fragExtVM = sender.as<Controls::Button>().Tag().as<Editor::FragmentExtensionViewModel>();
        get_self<ExtensionsViewModel>(_ViewModel)->CurrentExtension(fragExtVM);
    }

    void Extensions::NavigateToProfile_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto& profileGuid = sender.as<Controls::Button>().Tag().as<guid>();
        get_self<ExtensionsViewModel>(_ViewModel)->NavigateToProfile(profileGuid);
    }

    void Extensions::NavigateToColorScheme_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto& schemeVM = sender.as<Controls::Button>().Tag().as<Editor::ColorSchemeViewModel>();
        get_self<ExtensionsViewModel>(_ViewModel)->NavigateToColorScheme(schemeVM);
    }

    ExtensionsViewModel::ExtensionsViewModel(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM) :
        _settings{ settings },
        _colorSchemesPageVM{ colorSchemesPageVM }
    {
        UpdateSettings(settings, colorSchemesPageVM);

        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"CurrentExtension")
            {
                // Update the views to reflect the current extension, if one is selected.
                // Otherwise, show components from all extensions
                _profilesModifiedView.Clear();
                _profilesAddedView.Clear();
                _colorSchemesAddedView.Clear();

                if (const auto& ext = CurrentExtension())
                {
                    for (const auto& profile : ext.ProfilesModified())
                    {
                        _profilesModifiedView.Append(profile);
                    }
                    for (const auto& profile : ext.ProfilesAdded())
                    {
                        _profilesAddedView.Append(profile);
                    }
                    for (const auto& scheme : ext.ColorSchemesAdded())
                    {
                        _colorSchemesAddedView.Append(scheme);
                    }
                }
                else
                {
                    for (const auto& ext : _fragmentExtensions)
                    {
                        for (const auto& profile : ext.ProfilesModified())
                        {
                            _profilesModifiedView.Append(profile);
                        }
                        for (const auto& profile : ext.ProfilesAdded())
                        {
                            _profilesAddedView.Append(profile);
                        }
                        for (const auto& scheme : ext.ColorSchemesAdded())
                        {
                            _colorSchemesAddedView.Append(scheme);
                        }
                    }
                }

                _NotifyChanges(L"IsExtensionView", L"CurrentExtensionSource", L"CurrentExtensionJson");
            }
        });
    }

    void ExtensionsViewModel::UpdateSettings(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM)
    {
        _settings = settings;
        _colorSchemesPageVM = colorSchemesPageVM;

        std::vector<Editor::FragmentExtensionViewModel> fragmentExtensions;
        fragmentExtensions.reserve(settings.FragmentExtensions().Size());

        // these vectors track components all extensions successfully added
        std::vector<Editor::FragmentProfileViewModel> profilesModifiedTotal;
        std::vector<Editor::FragmentProfileViewModel> profilesAddedTotal;
        std::vector<Editor::FragmentColorSchemeViewModel> colorSchemesAddedTotal;
        for (const auto&& fragExt : settings.FragmentExtensions())
        {
            // these vectors track everything the current extension attempted to bring in
            std::vector<Editor::FragmentProfileViewModel> currentProfilesModified;
            std::vector<Editor::FragmentProfileViewModel> currentProfilesAdded;
            std::vector<Editor::FragmentColorSchemeViewModel> currentColorSchemesAdded;

            for (const auto&& entry : fragExt.ModifiedProfilesView())
            {
                // Ensure entry successfully modifies a profile before creating and registering the object
                if (const auto& deducedProfile = _settings.FindProfile(entry.ProfileGuid()))
                {
                    auto vm = winrt::make<FragmentProfileViewModel>(entry, fragExt, deducedProfile);
                    profilesModifiedTotal.push_back(vm);
                    currentProfilesModified.push_back(vm);
                }
                // TODO CARLOS: if this fails, we should probably still add it to currentProfilesModified
            }

            for (const auto&& entry : fragExt.NewProfilesView())
            {
                // Ensure entry successfully points to a profile before creating and registering the object.
                // The profile may have been removed by the user.
                if (const auto& deducedProfile = _settings.FindProfile(entry.ProfileGuid()))
                {
                    auto vm = winrt::make<FragmentProfileViewModel>(entry, fragExt, deducedProfile);
                    profilesAddedTotal.push_back(vm);
                    currentProfilesAdded.push_back(vm);
                }
                // TODO CARLOS: if this fails, we should probably still display it, but just say it was removed
                //                possibly introduce a way to re-add it too?
            }

            for (const auto&& entry : fragExt.ColorSchemesView())
            {
                for (const auto& schemeVM : _colorSchemesPageVM.AllColorSchemes())
                {
                    if (schemeVM.Name() == entry.ColorSchemeName())
                    {
                        auto vm = winrt::make<FragmentColorSchemeViewModel>(entry, fragExt, schemeVM);
                        colorSchemesAddedTotal.push_back(vm);
                        currentColorSchemesAdded.push_back(vm);
                    }
                }
                // TODO CARLOS: if this fails, we should probably still display it, but just say it was removed
                //                possibly introduce a way to re-add it too?
            }

            fragmentExtensions.push_back(winrt::make<FragmentExtensionViewModel>(fragExt, currentProfilesModified, currentProfilesAdded, currentColorSchemesAdded));
        }

        _fragmentExtensions = single_threaded_observable_vector<Editor::FragmentExtensionViewModel>(std::move(fragmentExtensions));
        _profilesModifiedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesModifiedTotal));
        _profilesAddedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesAddedTotal));
        _colorSchemesAddedView = single_threaded_observable_vector<Editor::FragmentColorSchemeViewModel>(std::move(colorSchemesAddedTotal));
    }

    hstring ExtensionsViewModel::CurrentExtensionScope() const noexcept
    {
        if (_CurrentExtension)
        {
            return _CurrentExtension.Fragment().Scope() == Model::FragmentScope::User ? RS_(L"Extensions_ScopeUser") : RS_(L"Extensions_ScopeSystem");
        }
        return hstring{};
    }

    // Returns true if the extension is enabled, false otherwise
    bool ExtensionsViewModel::GetExtensionState(hstring extensionSource) const
    {
        if (const auto& disabledExtensions = _DisabledProfileSources())
        {
            uint32_t ignored;
            return !disabledExtensions.IndexOf(extensionSource, ignored);
        }
        // "disabledProfileSources" not defined --> all extensions are enabled
        return true;
    }

    // Enable/Disable an extension
    void ExtensionsViewModel::SetExtensionState(hstring extensionSource, bool enableExt)
    {
        // get the current status of the extension
        uint32_t idx;
        bool currentlyEnabled = true;
        const auto& disabledExtensions = _DisabledProfileSources();
        if (disabledExtensions)
        {
            currentlyEnabled = !disabledExtensions.IndexOf(extensionSource, idx);
        }

        // current status mismatches the desired status,
        // update the list of disabled extensions
        if (currentlyEnabled != enableExt)
        {
            // If we're disabling an extension and we don't have "disabledProfileSources" defined,
            // create it in the model directly
            if (!disabledExtensions && !enableExt)
            {
                std::vector<hstring> disabledProfileSources{ extensionSource };
                _settings.GlobalSettings().DisabledProfileSources(single_threaded_vector<hstring>(std::move(disabledProfileSources)));
                return;
            }

            // Update the list of disabled extensions
            if (enableExt)
            {
                disabledExtensions.RemoveAt(idx);
            }
            else
            {
                disabledExtensions.Append(extensionSource);
            }
        }
    }

    void ExtensionsViewModel::NavigateToProfile(const guid profileGuid)
    {
        NavigateToProfileRequested.raise(*this, profileGuid);
    }

    void ExtensionsViewModel::NavigateToColorScheme(const Editor::ColorSchemeViewModel& schemeVM)
    {
        _colorSchemesPageVM.CurrentScheme(schemeVM);
        NavigateToColorSchemeRequested.raise(*this, nullptr);
    }
}
