// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Extensions.h"
#include "Extensions.g.cpp"
#include "ExtensionPackageViewModel.g.cpp"
#include "ExtensionsViewModel.g.cpp"
#include "FragmentProfileViewModel.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
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

    void Extensions::ExtensionLoaded(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto& toggleSwitch = sender.as<Controls::ToggleSwitch>();
        const auto& extensionSource = toggleSwitch.Tag().as<hstring>();
        toggleSwitch.IsOn(_ViewModel.GetExtensionState(extensionSource));
    }

    void Extensions::ExtensionToggled(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto& toggleSwitch = sender.as<Controls::ToggleSwitch>();
        const auto& extensionSource = toggleSwitch.Tag().as<hstring>();
        _ViewModel.SetExtensionState(extensionSource, toggleSwitch.IsOn());
    }

    void Extensions::ExtensionNavigator_Click(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto source = sender.as<Controls::Button>().Tag().as<hstring>();
        _ViewModel.CurrentExtensionSource(source);
    }

    void Extensions::NavigateToProfile_Click(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto& profileGuid = sender.as<Controls::Button>().Tag().as<guid>();
        get_self<ExtensionsViewModel>(_ViewModel)->NavigateToProfile(profileGuid);
    }

    void Extensions::NavigateToColorScheme_Click(const IInspectable& sender, const RoutedEventArgs& /*args*/)
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
            if (viewModelProperty == L"CurrentExtensionSource")
            {
                // Update the views to reflect the current extension source, if one is selected.
                // Otherwise, show components from all extensions
                _profilesModifiedView.Clear();
                _profilesAddedView.Clear();
                _colorSchemesAddedView.Clear();

                const auto currentExtensionSource = CurrentExtensionSource();
                for (const auto& ext : _fragmentExtensions)
                {
                    // No extension selected --> show all enabled extension components
                    // Otherwise, only show the ones for the selected extension
                    if (const auto extSrc = ext.Fragment().Source(); (currentExtensionSource.empty() && GetExtensionState(extSrc)) || extSrc == currentExtensionSource)
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

                _NotifyChanges(L"IsExtensionView", L"CurrentExtensionFragments");
            }
        });
    }

    void ExtensionsViewModel::UpdateSettings(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM)
    {
        _settings = settings;
        _colorSchemesPageVM = colorSchemesPageVM;
        _extensionSources.clear();
        _CurrentExtensionSource.clear();

        std::vector<Editor::FragmentExtensionViewModel> fragmentExtensions;
        fragmentExtensions.reserve(settings.FragmentExtensions().Size());

        // these vectors track components all extensions successfully added
        std::vector<Editor::FragmentProfileViewModel> profilesModifiedTotal;
        std::vector<Editor::FragmentProfileViewModel> profilesAddedTotal;
        std::vector<Editor::FragmentColorSchemeViewModel> colorSchemesAddedTotal;
        for (const auto&& fragExt : settings.FragmentExtensions())
        {
            const auto extensionEnabled = GetExtensionState(fragExt.Source());

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
                    currentProfilesModified.push_back(vm);
                    if (extensionEnabled)
                    {
                        profilesModifiedTotal.push_back(vm);
                    }
                }
            }

            for (const auto&& entry : fragExt.NewProfilesView())
            {
                // Ensure entry successfully points to a profile before creating and registering the object.
                // The profile may have been removed by the user.
                if (const auto& deducedProfile = _settings.FindProfile(entry.ProfileGuid()))
                {
                    auto vm = winrt::make<FragmentProfileViewModel>(entry, fragExt, deducedProfile);
                    currentProfilesAdded.push_back(vm);
                    if (extensionEnabled)
                    {
                        profilesAddedTotal.push_back(vm);
                    }
                }
            }

            for (const auto&& entry : fragExt.ColorSchemesView())
            {
                for (const auto& schemeVM : _colorSchemesPageVM.AllColorSchemes())
                {
                    if (schemeVM.Name() == entry.ColorSchemeName())
                    {
                        auto vm = winrt::make<FragmentColorSchemeViewModel>(entry, fragExt, schemeVM);
                        currentColorSchemesAdded.push_back(vm);
                        if (extensionEnabled)
                        {
                            colorSchemesAddedTotal.push_back(vm);
                        }
                    }
                }
            }

            _extensionSources.insert(fragExt.Source());
            fragmentExtensions.push_back(winrt::make<FragmentExtensionViewModel>(fragExt, currentProfilesModified, currentProfilesAdded, currentColorSchemesAdded));
        }

        _fragmentExtensions = single_threaded_observable_vector<Editor::FragmentExtensionViewModel>(std::move(fragmentExtensions));
        _profilesModifiedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesModifiedTotal));
        _profilesAddedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesAddedTotal));
        _colorSchemesAddedView = single_threaded_observable_vector<Editor::FragmentColorSchemeViewModel>(std::move(colorSchemesAddedTotal));
    }

    IVector<IInspectable> ExtensionsViewModel::CurrentExtensionFragments() const noexcept
    {
        std::vector<IInspectable> fragmentExtensionVMs;
        for (auto&& extVM : _fragmentExtensions)
        {
            if (_CurrentExtensionSource.empty() || extVM.Fragment().Source() == _CurrentExtensionSource)
            {
                fragmentExtensionVMs.push_back(extVM);
            }
        }
        return winrt::single_threaded_vector<IInspectable>(std::move(fragmentExtensionVMs));
    }

    hstring ExtensionsViewModel::CurrentExtensionScope() const noexcept
    {
        if (!_CurrentExtensionSource.empty())
        {
            for (auto&& extVM : _fragmentExtensions)
            {
                const auto& fragExt = extVM.Fragment();
                if (fragExt.Source() == _CurrentExtensionSource)
                {
                    return fragExt.Scope() == Model::FragmentScope::User ? RS_(L"Extensions_ScopeUser") : RS_(L"Extensions_ScopeSystem");
                }
            }
        }
        return hstring{};
    }

    IObservableVector<Editor::ExtensionPackageViewModel> ExtensionsViewModel::ExtensionPackages() const noexcept
    {
        std::vector<Editor::ExtensionPackageViewModel> extensionPackages;
        for (auto&& extSrc : _extensionSources)
        {
            extensionPackages.push_back(winrt::make<ExtensionPackageViewModel>(extSrc, GetExtensionState(extSrc)));
        }
        return winrt::single_threaded_observable_vector<Editor::ExtensionPackageViewModel>(std::move(extensionPackages));
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

    hstring ExtensionPackageViewModel::AccessibleName() const noexcept
    {
        if (_enabled)
        {
            return _source;
        }
        return hstring{ fmt::format(L"{}: {}", _source, RS_(L"Extension_StateDisabled/Text")) };
    }
}
