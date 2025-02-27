// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Extensions.h"
#include "Extensions.g.cpp"
#include "ExtensionPackageViewModel.g.cpp"
#include "ExtensionsViewModel.g.cpp"
#include "FragmentProfileViewModel.g.cpp"
#include "ExtensionPackageTemplateSelector.g.cpp"

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

        _extensionPackageIdentifierTemplateSelector = Resources().Lookup(box_value(L"ExtensionPackageIdentifierTemplateSelector")).as<Editor::ExtensionPackageTemplateSelector>();

        Automation::AutomationProperties::SetName(ActiveExtensionsList(), RS_(L"Extensions_ActiveExtensionsHeader/Text"));
        Automation::AutomationProperties::SetName(ModifiedProfilesList(), RS_(L"Extensions_ModifiedProfilesHeader/Text"));
        Automation::AutomationProperties::SetName(AddedProfilesList(), RS_(L"Extensions_AddedProfilesHeader/Text"));
        Automation::AutomationProperties::SetName(AddedColorSchemesList(), RS_(L"Extensions_AddedColorSchemesHeader/Text"));
    }

    void Extensions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ExtensionsViewModel>();
        get_self<ExtensionsViewModel>(_ViewModel)->ExtensionPackageIdentifierTemplateSelector(_extensionPackageIdentifierTemplateSelector);
    }

    void Extensions::ExtensionNavigator_Click(const IInspectable& sender, const RoutedEventArgs& /*args*/)
    {
        const auto extPkgVM = sender.as<Controls::Button>().Tag().as<Editor::ExtensionPackageViewModel>();
        _ViewModel.CurrentExtensionPackage(extPkgVM);
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
            if (viewModelProperty == L"CurrentExtensionPackage")
            {
                // Update the views to reflect the current extension package, if one is selected.
                // Otherwise, show components from all extensions
                _profilesModifiedView.Clear();
                _profilesAddedView.Clear();
                _colorSchemesAddedView.Clear();

                // Helper lambda to add the contents of an extension package to the current view
                auto addPackageContentsToView = [&](const Editor::ExtensionPackageViewModel& extPkg) {
                    auto extPkgVM = get_self<ExtensionPackageViewModel>(extPkg);
                    for (const auto& ext : extPkgVM->FragmentExtensions())
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
                };

                if (const auto currentExtensionPackage = CurrentExtensionPackage())
                {
                    addPackageContentsToView(currentExtensionPackage);
                }
                else
                {
                    for (const auto& extPkg : _extensionPackages)
                    {
                        addPackageContentsToView(extPkg);
                    }
                }

                _NotifyChanges(L"IsExtensionView", L"CurrentExtensionPackageIdentifierTemplate");
            }
        });
    }

    void ExtensionsViewModel::UpdateSettings(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM)
    {
        _settings = settings;
        _colorSchemesPageVM = colorSchemesPageVM;
        _CurrentExtensionPackage = nullptr;

        std::vector<Model::ExtensionPackage> extensions = wil::to_vector(settings.Extensions());

        // these vectors track components all extensions successfully added
        std::vector<Editor::ExtensionPackageViewModel> extensionPackages;
        std::vector<Editor::FragmentProfileViewModel> profilesModifiedTotal;
        std::vector<Editor::FragmentProfileViewModel> profilesAddedTotal;
        std::vector<Editor::FragmentColorSchemeViewModel> colorSchemesAddedTotal;
        for (const auto& extPkg : extensions)
        {
            auto extPkgVM = winrt::make_self<ExtensionPackageViewModel>(extPkg, settings);
            extensionPackages.push_back(*extPkgVM);
            for (const auto& fragExt : extPkg.FragmentsView())
            {
                const auto extensionEnabled = GetExtensionState(fragExt.Source(), _settings);

                // these vectors track everything the current extension attempted to bring in
                std::vector<Editor::FragmentProfileViewModel> currentProfilesModified;
                std::vector<Editor::FragmentProfileViewModel> currentProfilesAdded;
                std::vector<Editor::FragmentColorSchemeViewModel> currentColorSchemesAdded;

                if (fragExt.ModifiedProfilesView())
                {
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
                }

                if (fragExt.NewProfilesView())
                {
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
                }

                if (fragExt.ColorSchemesView())
                {
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
                }
                extPkgVM->FragmentExtensions().Append(winrt::make<FragmentExtensionViewModel>(fragExt, currentProfilesModified, currentProfilesAdded, currentColorSchemesAdded));
            }
        }

        _extensionPackages = single_threaded_observable_vector<Editor::ExtensionPackageViewModel>(std::move(extensionPackages));
        _profilesModifiedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesModifiedTotal));
        _profilesAddedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesAddedTotal));
        _colorSchemesAddedView = single_threaded_observable_vector<Editor::FragmentColorSchemeViewModel>(std::move(colorSchemesAddedTotal));
    }

    Windows::UI::Xaml::DataTemplate ExtensionsViewModel::CurrentExtensionPackageIdentifierTemplate() const
    {
        return _ExtensionPackageIdentifierTemplateSelector.SelectTemplate(CurrentExtensionPackage());
    }

    // Returns true if the extension is enabled, false otherwise
    bool ExtensionsViewModel::GetExtensionState(hstring extensionSource, const Model::CascadiaSettings& settings)
    {
        if (const auto& disabledExtensions = settings.GlobalSettings().DisabledProfileSources())
        {
            uint32_t ignored;
            return !disabledExtensions.IndexOf(extensionSource, ignored);
        }
        // "disabledProfileSources" not defined --> all extensions are enabled
        return true;
    }

    // Enable/Disable an extension
    void ExtensionsViewModel::SetExtensionState(hstring extensionSource, const Model::CascadiaSettings& settings, bool enableExt)
    {
        // get the current status of the extension
        uint32_t idx;
        bool currentlyEnabled = true;
        const auto& disabledExtensions = settings.GlobalSettings().DisabledProfileSources();
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
                settings.GlobalSettings().DisabledProfileSources(single_threaded_vector<hstring>(std::move(disabledProfileSources)));
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

    hstring ExtensionPackageViewModel::Scope() const noexcept
    {
        return _package.Scope() == Model::FragmentScope::User ? RS_(L"Extensions_ScopeUser") : RS_(L"Extensions_ScopeSystem");
    }

    bool ExtensionPackageViewModel::Enabled() const
    {
        return ExtensionsViewModel::GetExtensionState(_package.Source(), _settings);
    }

    void ExtensionPackageViewModel::Enabled(bool val)
    {
        if (Enabled() != val)
        {
            ExtensionsViewModel::SetExtensionState(_package.Source(), _settings, val);
            _NotifyChanges(L"Enabled");
        }
    }

    // Returns the accessible name for the extension package in the following format:
    //   "<DisplayName?>, <Source>"
    hstring ExtensionPackageViewModel::AccessibleName() const noexcept
    {
        hstring name;
        const auto source = _package.Source();
        if (const auto displayName = _package.DisplayName(); !displayName.empty())
        {
            return hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), displayName, source) };
        }
        return source;
    }

    // Returns the accessible name for the extension package with the disabled state (if disabled) in the following format:
    //   "<DisplayName?>, <Source>: <Disabled?>"
    hstring ExtensionPackageViewModel::AccessibleNameWithStatus() const noexcept
    {
        if (Enabled())
        {
            return AccessibleName();
        }
        return hstring{ fmt::format(FMT_COMPILE(L"{}: {}"), AccessibleName(), RS_(L"Extension_StateDisabled/Text")) };
    }

    hstring FragmentProfileViewModel::AccessibleName() const noexcept
    {
        return hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), Profile().Name(), SourceName()) };
    }

    hstring FragmentColorSchemeViewModel::AccessibleName() const noexcept
    {
        return hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), ColorSchemeVM().Name(), SourceName()) };
    }

    DataTemplate ExtensionPackageTemplateSelector::SelectTemplateCore(const IInspectable& item, const DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    DataTemplate ExtensionPackageTemplateSelector::SelectTemplateCore(const IInspectable& item)
    {
        if (const auto extPkgVM = item.try_as<Editor::ExtensionPackageViewModel>())
        {
            if (!extPkgVM.Package().DisplayName().empty())
            {
                return ComplexTemplate();
            }
            return DefaultTemplate();
        }
        return nullptr;
    }
}
