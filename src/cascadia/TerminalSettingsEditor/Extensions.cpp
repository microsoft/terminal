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
        auto vmImpl = get_self<ExtensionsViewModel>(_ViewModel);
        vmImpl->ExtensionPackageIdentifierTemplateSelector(_extensionPackageIdentifierTemplateSelector);
        vmImpl->LazyLoadExtensions();
        vmImpl->MarkAsVisited();

        if (vmImpl->IsExtensionView())
        {
            const auto currentPkgVM = vmImpl->CurrentExtensionPackage();
            const auto currentPkg = currentPkgVM.Package();
            TraceLoggingWrite(
                g_hTerminalSettingsEditorProvider,
                "NavigatedToPage",
                TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
                TraceLoggingValue("extensions.extensionView", "PageId", "The identifier of the page that was navigated to"),
                TraceLoggingValue(currentPkg.Source().c_str(), "FragmentSource", "The source of the fragment included in this extension package"),
                TraceLoggingValue(currentPkgVM.FragmentExtensions().Size(), "FragmentCount", "The number of fragments included in this extension package"),
                TraceLoggingValue(currentPkgVM.Enabled(), "Enabled", "The enabled status of the extension"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
        else
        {
            TraceLoggingWrite(
                g_hTerminalSettingsEditorProvider,
                "NavigatedToPage",
                TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
                TraceLoggingValue("extensions", "PageId", "The identifier of the page that was navigated to"),
                TraceLoggingValue(vmImpl->ExtensionPackages().Size(), "ExtensionPackageCount", "The number of extension packages displayed"),
                TraceLoggingValue(vmImpl->ProfilesModified().Size(), "ProfilesModifiedCount", "The number of profiles modified by enabled extensions"),
                TraceLoggingValue(vmImpl->ProfilesAdded().Size(), "ProfilesAddedCount", "The number of profiles added by enabled extensions"),
                TraceLoggingValue(vmImpl->ColorSchemesAdded().Size(), "ColorSchemesAddedCount", "The number of color schemes added by enabled extensions"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
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
        _colorSchemesPageVM{ colorSchemesPageVM },
        _extensionsLoaded{ false }
    {
        UpdateSettings(settings, colorSchemesPageVM);

        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };

            const bool extensionPackageChanged = viewModelProperty == L"CurrentExtensionPackage";
            const bool profilesModifiedChanged = viewModelProperty == L"ProfilesModified";
            const bool profilesAddedChanged = viewModelProperty == L"ProfilesAdded";
            const bool colorSchemesAddedChanged = viewModelProperty == L"ColorSchemesAdded";
            if (extensionPackageChanged || (!IsExtensionView() && (profilesModifiedChanged || profilesAddedChanged || colorSchemesAddedChanged)))
            {
                // Use these booleans to track which of our observable vectors need to be refreshed.
                // This prevents a full refresh of the UI when enabling/disabling extensions.
                // If the CurrentExtensionPackage changed, we want to update all components.
                // Otherwise, just update the ones that we were notified about.
                const bool updateProfilesModified = extensionPackageChanged || profilesModifiedChanged;
                const bool updateProfilesAdded = extensionPackageChanged || profilesAddedChanged;
                const bool updateColorSchemesAdded = extensionPackageChanged || colorSchemesAddedChanged;
                _UpdateListViews(updateProfilesModified, updateProfilesAdded, updateColorSchemesAdded);

                if (extensionPackageChanged)
                {
                    _NotifyChanges(L"IsExtensionView", L"CurrentExtensionPackageIdentifierTemplate");
                }
                else if (profilesModifiedChanged)
                {
                    _NotifyChanges(L"NoProfilesModified");
                }
                else if (profilesAddedChanged)
                {
                    _NotifyChanges(L"NoProfilesAdded");
                }
                else if (colorSchemesAddedChanged)
                {
                    _NotifyChanges(L"NoSchemesAdded");
                }
            }
        });
    }

    void ExtensionsViewModel::_UpdateListViews(bool updateProfilesModified, bool updateProfilesAdded, bool updateColorSchemesAdded)
    {
        // STL vectors to track relevant components for extensions to display in UI
        std::vector<Editor::FragmentProfileViewModel> profilesModifiedTotal;
        std::vector<Editor::FragmentProfileViewModel> profilesAddedTotal;
        std::vector<Editor::FragmentColorSchemeViewModel> colorSchemesAddedTotal;

        // Helper lambda to add the contents of an extension package to the current view.
        auto addPackageContentsToView = [&](const Editor::ExtensionPackageViewModel& extPkg) {
            auto extPkgVM = get_self<ExtensionPackageViewModel>(extPkg);
            for (const auto& ext : extPkgVM->FragmentExtensions())
            {
                if (updateProfilesModified)
                {
                    for (const auto& profile : ext.ProfilesModified())
                    {
                        profilesModifiedTotal.push_back(profile);
                    }
                }
                if (updateProfilesAdded)
                {
                    for (const auto& profile : ext.ProfilesAdded())
                    {
                        profilesAddedTotal.push_back(profile);
                    }
                }
                if (updateColorSchemesAdded)
                {
                    for (const auto& scheme : ext.ColorSchemesAdded())
                    {
                        colorSchemesAddedTotal.push_back(scheme);
                    }
                }
            }
        };

        // Populate the STL vectors that we want to update
        if (const auto currentExtensionPackage = CurrentExtensionPackage())
        {
            // Update all of the views to reflect the current extension package, if one is selected.
            addPackageContentsToView(currentExtensionPackage);
        }
        else
        {
            // Only populate the views with components from enabled extensions
            for (const auto& extPkg : _extensionPackages)
            {
                if (extPkg.Enabled())
                {
                    addPackageContentsToView(extPkg);
                }
            }
        }

        // Sort the lists linguistically for nicer presentation.
        // Update the WinRT lists bound to UI.
        if (updateProfilesModified)
        {
            std::sort(profilesModifiedTotal.begin(), profilesModifiedTotal.end(), FragmentProfileViewModel::SortAscending);
            _profilesModifiedView = winrt::single_threaded_observable_vector(std::move(profilesModifiedTotal));
        }
        if (updateProfilesAdded)
        {
            std::sort(profilesAddedTotal.begin(), profilesAddedTotal.end(), FragmentProfileViewModel::SortAscending);
            _profilesAddedView = winrt::single_threaded_observable_vector(std::move(profilesAddedTotal));
        }
        if (updateColorSchemesAdded)
        {
            std::sort(colorSchemesAddedTotal.begin(), colorSchemesAddedTotal.end(), FragmentColorSchemeViewModel::SortAscending);
            _colorSchemesAddedView = winrt::single_threaded_observable_vector(std::move(colorSchemesAddedTotal));
        }
    }

    void ExtensionsViewModel::UpdateSettings(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM)
    {
        _settings = settings;
        _colorSchemesPageVM = colorSchemesPageVM;
        _CurrentExtensionPackage = nullptr;

        // The extension packages may not be loaded yet because we want to wait until we actually navigate to the page to do so.
        // In that case, omit "updating" them. They'll get the proper references when we lazy load them.
        if (_extensionPackages)
        {
            for (const auto& extPkg : _extensionPackages)
            {
                get_self<ExtensionPackageViewModel>(extPkg)->UpdateSettings(_settings);
            }
        }
    }

    void ExtensionsViewModel::LazyLoadExtensions()
    {
        if (_extensionsLoaded)
        {
            return;
        }
        std::vector<Model::ExtensionPackage> extensions = wil::to_vector(_settings.Extensions());

        // these vectors track components all extensions successfully added
        std::vector<Editor::ExtensionPackageViewModel> extensionPackages;
        std::vector<Editor::FragmentProfileViewModel> profilesModifiedTotal;
        std::vector<Editor::FragmentProfileViewModel> profilesAddedTotal;
        std::vector<Editor::FragmentColorSchemeViewModel> colorSchemesAddedTotal;
        for (const auto& extPkg : extensions)
        {
            auto extPkgVM = winrt::make_self<ExtensionPackageViewModel>(extPkg, _settings);
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

                // sort the lists linguistically for nicer presentation
                std::sort(currentProfilesModified.begin(), currentProfilesModified.end(), FragmentProfileViewModel::SortAscending);
                std::sort(currentProfilesAdded.begin(), currentProfilesAdded.end(), FragmentProfileViewModel::SortAscending);
                std::sort(currentColorSchemesAdded.begin(), currentColorSchemesAdded.end(), FragmentColorSchemeViewModel::SortAscending);

                extPkgVM->FragmentExtensions().Append(winrt::make<FragmentExtensionViewModel>(fragExt, currentProfilesModified, currentProfilesAdded, currentColorSchemesAdded));
                extPkgVM->PropertyChanged([&](const IInspectable& sender, const PropertyChangedEventArgs& args) {
                    const auto viewModelProperty{ args.PropertyName() };
                    if (viewModelProperty == L"Enabled")
                    {
                        // If the extension was enabled/disabled,
                        // check if any of its fragments modified profiles, added profiles, or added color schemes.
                        // Only notify what was affected!
                        bool hasModifiedProfiles = false;
                        bool hasAddedProfiles = false;
                        bool hasAddedColorSchemes = false;
                        for (const auto& fragExtVM : sender.as<ExtensionPackageViewModel>()->FragmentExtensions())
                        {
                            const auto profilesModified = fragExtVM.ProfilesModified();
                            const auto profilesAdded = fragExtVM.ProfilesAdded();
                            const auto colorSchemesAdded = fragExtVM.ColorSchemesAdded();
                            hasModifiedProfiles |= profilesModified && profilesModified.Size() > 0;
                            hasAddedProfiles |= profilesAdded && profilesAdded.Size() > 0;
                            hasAddedColorSchemes |= colorSchemesAdded && colorSchemesAdded.Size() > 0;
                        }
                        if (hasModifiedProfiles)
                        {
                            _NotifyChanges(L"ProfilesModified");
                        }
                        if (hasAddedProfiles)
                        {
                            _NotifyChanges(L"ProfilesAdded");
                        }
                        if (hasAddedColorSchemes)
                        {
                            _NotifyChanges(L"ColorSchemesAdded");
                        }
                    }
                });
            }
            extensionPackages.push_back(*extPkgVM);
        }

        // sort the lists linguistically for nicer presentation
        std::sort(extensionPackages.begin(), extensionPackages.end(), ExtensionPackageViewModel::SortAscending);
        std::sort(profilesModifiedTotal.begin(), profilesModifiedTotal.end(), FragmentProfileViewModel::SortAscending);
        std::sort(profilesAddedTotal.begin(), profilesAddedTotal.end(), FragmentProfileViewModel::SortAscending);
        std::sort(colorSchemesAddedTotal.begin(), colorSchemesAddedTotal.end(), FragmentColorSchemeViewModel::SortAscending);

        _extensionPackages = single_threaded_observable_vector<Editor::ExtensionPackageViewModel>(std::move(extensionPackages));
        _profilesModifiedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesModifiedTotal));
        _profilesAddedView = single_threaded_observable_vector<Editor::FragmentProfileViewModel>(std::move(profilesAddedTotal));
        _colorSchemesAddedView = single_threaded_observable_vector<Editor::FragmentColorSchemeViewModel>(std::move(colorSchemesAddedTotal));
        _extensionsLoaded = true;
    }

    Windows::UI::Xaml::DataTemplate ExtensionsViewModel::CurrentExtensionPackageIdentifierTemplate() const
    {
        return _ExtensionPackageIdentifierTemplateSelector.SelectTemplate(CurrentExtensionPackage());
    }

    bool ExtensionsViewModel::DisplayBadge() const noexcept
    {
        return !Model::ApplicationState::SharedInstance().BadgeDismissed(L"page.extensions");
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

    Thickness Extensions::CalculateMargin(bool hidden)
    {
        return ThicknessHelper::FromLengths(/*left*/ 0,
                                            /*top*/ hidden ? 0 : 20,
                                            /*right*/ 0,
                                            /*bottom*/ 0);
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

    void ExtensionsViewModel::MarkAsVisited()
    {
        Model::ApplicationState::SharedInstance().DismissBadge(L"page.extensions");
        _NotifyChanges(L"DisplayBadge");
    }

    bool ExtensionPackageViewModel::SortAscending(const Editor::ExtensionPackageViewModel& lhs, const Editor::ExtensionPackageViewModel& rhs)
    {
        auto getKey = [&](const Editor::ExtensionPackageViewModel& pkgVM) {
            const auto pkg = pkgVM.Package();
            const auto displayName = pkg.DisplayName();
            return displayName.empty() ? pkg.Source() : displayName;
        };

        return til::compare_linguistic_insensitive(getKey(lhs), getKey(rhs)) < 0;
    }

    void ExtensionPackageViewModel::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        const auto oldEnabled = Enabled();
        _settings = settings;
        if (oldEnabled != Enabled())
        {
            // The enabled state of the extension has changed, notify the UI
            _NotifyChanges(L"Enabled");
        }
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

    bool FragmentProfileViewModel::SortAscending(const Editor::FragmentProfileViewModel& lhs, const Editor::FragmentProfileViewModel& rhs)
    {
        return til::compare_linguistic_insensitive(lhs.Profile().Name(), rhs.Profile().Name()) < 0;
    }

    hstring FragmentProfileViewModel::AccessibleName() const noexcept
    {
        return hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), Profile().Name(), SourceName()) };
    }

    bool FragmentColorSchemeViewModel::SortAscending(const Editor::FragmentColorSchemeViewModel& lhs, const Editor::FragmentColorSchemeViewModel& rhs)
    {
        return til::compare_linguistic_insensitive(lhs.ColorSchemeVM().Name(), rhs.ColorSchemeVM().Name()) < 0;
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
                // Check if the first char of the icon is in the Segoe MDL2 Icons list
                const auto ch = til::at(extPkgVM.Package().Icon(), 0);
                if (ch >= L'\uE700' && ch <= L'\uF8FF')
                {
                    return ComplexTemplateWithFontIcon();
                }
                return ComplexTemplate();
            }
            return DefaultTemplate();
        }
        return nullptr;
    }
}
