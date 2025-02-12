// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Extensions.g.h"
#include "ExtensionsViewModel.g.h"
#include "FragmentProfileViewModel.g.h"
#include "FragmentColorSchemeViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Extensions : public HasScrollViewer<Extensions>, ExtensionsT<Extensions>
    {
    public:
        Extensions();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void ExtensionLoaded(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ExtensionToggled(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        WINRT_PROPERTY(Editor::ExtensionsViewModel, ViewModel, nullptr);
    };

    struct ExtensionsViewModel : ExtensionsViewModelT<ExtensionsViewModel>, ViewModelHelper<ExtensionsViewModel>
    {
    public:
        ExtensionsViewModel(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM);

        // Properties
        bool NoActiveExtensions() const noexcept { return _fragmentExtensions.Size() == 0; }
        bool NoProfilesModified() const noexcept { return _profilesModified.Size() == 0; }
        bool NoProfilesAdded() const noexcept { return _profilesAdded.Size() == 0; }
        bool NoSchemesAdded() const noexcept { return _colorSchemesAdded.Size() == 0; }

        // Views
        Windows::Foundation::Collections::IVectorView<IInspectable> FragmentExtensions() const noexcept { return _fragmentExtensions.GetView(); }
        Windows::Foundation::Collections::IVectorView<IInspectable> ProfilesModified() const noexcept { return _profilesModified.GetView(); }
        Windows::Foundation::Collections::IVectorView<IInspectable> ProfilesAdded() const noexcept { return _profilesAdded.GetView(); }
        Windows::Foundation::Collections::IVectorView<IInspectable> ColorSchemesAdded() const noexcept { return _colorSchemesAdded.GetView(); }

        // Methods
        bool GetExtensionState(hstring extensionSource) const;
        void SetExtensionState(hstring extensionSource, bool enableExt);

        til::typed_event<IInspectable, guid> NavigateToProfileRequested;
        til::typed_event<IInspectable, Editor::ColorSchemeViewModel> NavigateToColorSchemeRequested;

    private:
        Model::CascadiaSettings _settings;
        Editor::ColorSchemesPageViewModel _colorSchemesPageVM;
        Windows::Foundation::Collections::IVector<IInspectable> _fragmentExtensions;
        Windows::Foundation::Collections::IVector<IInspectable> _profilesModified;
        Windows::Foundation::Collections::IVector<IInspectable> _profilesAdded;
        Windows::Foundation::Collections::IVector<IInspectable> _colorSchemesAdded;

        Windows::Foundation::Collections::IVector<hstring> _DisabledProfileSources() const noexcept { return _settings.GlobalSettings().DisabledProfileSources(); }
        void _NavigateToProfileHandler(const IInspectable& sender, const guid profileGuid);
        void _NavigateToColorSchemeHandler(const IInspectable& sender, const Editor::ColorSchemeViewModel& schemeVM);
    };

    struct FragmentProfileViewModel : FragmentProfileViewModelT<FragmentProfileViewModel>, ViewModelHelper<FragmentProfileViewModel>
    {
    public:
        FragmentProfileViewModel(const Model::FragmentProfileEntry& entry, const Model::FragmentSettings& fragment, const Model::Profile& deducedProfile) :
            _entry{ entry },
            _fragment{ fragment },
            _deducedProfile{ deducedProfile } {}

        Model::Profile Profile() const { return _deducedProfile; };
        hstring SourceName() const { return _fragment.Source(); }
        hstring Json() const { return _entry.Json(); }

        void AttemptNavigateToProfile();

        til::typed_event<IInspectable, guid> NavigateToProfileRequested;

    private:
        Model::FragmentProfileEntry _entry;
        Model::FragmentSettings _fragment;
        Model::Profile _deducedProfile;
    };

    struct FragmentColorSchemeViewModel : FragmentColorSchemeViewModelT<FragmentColorSchemeViewModel>, ViewModelHelper<FragmentColorSchemeViewModel>
    {
    public:
        FragmentColorSchemeViewModel(const Model::FragmentColorSchemeEntry& entry, const Model::FragmentSettings& fragment, const Editor::ColorSchemeViewModel& deducedSchemeVM) :
            _entry{ entry },
            _fragment{ fragment },
            _deducedSchemeVM{ deducedSchemeVM } {}

        Editor::ColorSchemeViewModel ColorSchemeVM() const { return _deducedSchemeVM; };
        hstring SourceName() const { return _fragment.Source(); }
        hstring Json() const { return _entry.Json(); }

        void AttemptNavigateToColorScheme();

        til::typed_event<IInspectable, Editor::ColorSchemeViewModel> NavigateToColorSchemeRequested;

    private:
        Model::FragmentColorSchemeEntry _entry;
        Model::FragmentSettings _fragment;
        Editor::ColorSchemeViewModel _deducedSchemeVM;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Extensions);
}
