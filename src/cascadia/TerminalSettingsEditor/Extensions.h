// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Extensions.g.h"
#include "ExtensionsViewModel.g.h"
#include "FragmentExtensionViewModel.g.h"
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

        void FragmentNavigator_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void NavigateToProfile_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void NavigateToColorScheme_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        WINRT_PROPERTY(Editor::ExtensionsViewModel, ViewModel, nullptr);
    };

    struct ExtensionsViewModel : ExtensionsViewModelT<ExtensionsViewModel>, ViewModelHelper<ExtensionsViewModel>
    {
    public:
        ExtensionsViewModel(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM);

        // Properties
        bool IsExtensionView() const noexcept { return _CurrentExtension != nullptr; }
        hstring CurrentExtensionSource() const noexcept { return _CurrentExtension ? _CurrentExtension.Fragment().Source() : hstring{}; }
        hstring CurrentExtensionJson() const noexcept { return _CurrentExtension ? _CurrentExtension.Fragment().Json() : hstring{}; }
        hstring CurrentExtensionScope() const noexcept;
        bool NoActiveExtensions() const noexcept { return _fragmentExtensions.Size() == 0; }
        bool NoProfilesModified() const noexcept { return _profilesModifiedView.Size() == 0; }
        bool NoProfilesAdded() const noexcept { return _profilesAddedView.Size() == 0; }
        bool NoSchemesAdded() const noexcept { return _colorSchemesAddedView.Size() == 0; }

        // Views
        Windows::Foundation::Collections::IVectorView<Editor::FragmentExtensionViewModel> FragmentExtensions() const noexcept { return _fragmentExtensions.GetView(); }
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> ProfilesModified() const noexcept { return _profilesModifiedView; }
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> ProfilesAdded() const noexcept { return _profilesAddedView; }
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentColorSchemeViewModel> ColorSchemesAdded() const noexcept { return _colorSchemesAddedView; }

        // Methods
        void UpdateSettings(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM);
        bool GetExtensionState(hstring extensionSource) const;
        void SetExtensionState(hstring extensionSource, bool enableExt);
        void NavigateToProfile(const guid profileGuid);
        void NavigateToColorScheme(const Editor::ColorSchemeViewModel& schemeVM);

        til::typed_event<IInspectable, guid> NavigateToProfileRequested;
        til::typed_event<IInspectable, Editor::ColorSchemeViewModel> NavigateToColorSchemeRequested;

        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::FragmentExtensionViewModel, CurrentExtension, nullptr);

    private:
        Model::CascadiaSettings _settings;
        Editor::ColorSchemesPageViewModel _colorSchemesPageVM;
        Windows::Foundation::Collections::IVector<Editor::FragmentExtensionViewModel> _fragmentExtensions;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> _profilesModifiedView;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> _profilesAddedView;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentColorSchemeViewModel> _colorSchemesAddedView;

        Windows::Foundation::Collections::IVector<hstring> _DisabledProfileSources() const noexcept { return _settings.GlobalSettings().DisabledProfileSources(); }
    };

    struct FragmentExtensionViewModel : FragmentExtensionViewModelT<FragmentExtensionViewModel>, ViewModelHelper<FragmentExtensionViewModel>
    {
    public:
        FragmentExtensionViewModel(const Model::FragmentSettings& fragment,
                                   std::vector<FragmentProfileViewModel>& profilesModified,
                                   std::vector<FragmentProfileViewModel>& profilesAdded,
                                   std::vector<FragmentColorSchemeViewModel>& colorSchemesAdded) :
            _fragment{ fragment },
            _profilesModified{ single_threaded_vector(std::move(profilesModified)) },
            _profilesAdded{ single_threaded_vector(std::move(profilesAdded)) },
            _colorSchemesAdded{ single_threaded_vector(std::move(colorSchemesAdded)) } {}

        Model::FragmentSettings Fragment() const noexcept { return _fragment; }
        Windows::Foundation::Collections::IVectorView<FragmentProfileViewModel> ProfilesModified() const noexcept { return _profilesModified.GetView(); }
        Windows::Foundation::Collections::IVectorView<FragmentProfileViewModel> ProfilesAdded() const noexcept { return _profilesAdded.GetView(); }
        Windows::Foundation::Collections::IVectorView<FragmentColorSchemeViewModel> ColorSchemesAdded() const noexcept { return _colorSchemesAdded.GetView(); }
        hstring Json() const noexcept { return _fragment.Json(); }

    private:
        Model::FragmentSettings _fragment;
        Windows::Foundation::Collections::IVector<FragmentProfileViewModel> _profilesModified;
        Windows::Foundation::Collections::IVector<FragmentProfileViewModel> _profilesAdded;
        Windows::Foundation::Collections::IVector<FragmentColorSchemeViewModel> _colorSchemesAdded;
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
