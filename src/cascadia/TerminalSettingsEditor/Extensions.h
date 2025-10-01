// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Extensions.g.h"
#include "ExtensionsViewModel.g.h"
#include "ExtensionPackageViewModel.g.h"
#include "FragmentExtensionViewModel.g.h"
#include "FragmentProfileViewModel.g.h"
#include "FragmentColorSchemeViewModel.g.h"
#include "ExtensionPackageTemplateSelector.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Extensions : public HasScrollViewer<Extensions>, ExtensionsT<Extensions>
    {
    public:
        Windows::UI::Xaml::Thickness CalculateMargin(bool hidden);

        Extensions();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void ExtensionNavigator_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void NavigateToProfile_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void NavigateToColorScheme_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        WINRT_PROPERTY(Editor::ExtensionsViewModel, ViewModel, nullptr);

    private:
        Editor::ExtensionPackageTemplateSelector _extensionPackageIdentifierTemplateSelector;
    };

    struct ExtensionsViewModel : ExtensionsViewModelT<ExtensionsViewModel>, ViewModelHelper<ExtensionsViewModel>
    {
    public:
        ExtensionsViewModel(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM);

        // Properties
        Windows::UI::Xaml::DataTemplate CurrentExtensionPackageIdentifierTemplate() const;
        bool IsExtensionView() const noexcept { return _CurrentExtensionPackage != nullptr; }
        bool NoExtensionPackages() const noexcept { return _extensionPackages.Size() == 0; }
        bool NoProfilesModified() const noexcept { return _profilesModifiedView.Size() == 0; }
        bool NoProfilesAdded() const noexcept { return _profilesAddedView.Size() == 0; }
        bool NoSchemesAdded() const noexcept { return _colorSchemesAddedView.Size() == 0; }
        bool DisplayBadge() const noexcept;

        // Views
        Windows::Foundation::Collections::IObservableVector<Editor::ExtensionPackageViewModel> ExtensionPackages() const noexcept { return _extensionPackages; }
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> ProfilesModified() const noexcept { return _profilesModifiedView; }
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> ProfilesAdded() const noexcept { return _profilesAddedView; }
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentColorSchemeViewModel> ColorSchemesAdded() const noexcept { return _colorSchemesAddedView; }

        // Methods
        void LazyLoadExtensions();
        void UpdateSettings(const Model::CascadiaSettings& settings, const Editor::ColorSchemesPageViewModel& colorSchemesPageVM);
        void NavigateToProfile(const guid profileGuid);
        void NavigateToColorScheme(const Editor::ColorSchemeViewModel& schemeVM);
        void MarkAsVisited();

        static bool GetExtensionState(hstring extensionSource, const Model::CascadiaSettings& settings);
        static void SetExtensionState(hstring extensionSource, const Model::CascadiaSettings& settings, bool enableExt);

        til::typed_event<IInspectable, guid> NavigateToProfileRequested;
        til::typed_event<IInspectable, Editor::ColorSchemeViewModel> NavigateToColorSchemeRequested;

        VIEW_MODEL_OBSERVABLE_PROPERTY(Editor::ExtensionPackageViewModel, CurrentExtensionPackage, nullptr);
        WINRT_PROPERTY(Editor::ExtensionPackageTemplateSelector, ExtensionPackageIdentifierTemplateSelector, nullptr);

    private:
        Model::CascadiaSettings _settings;
        Editor::ColorSchemesPageViewModel _colorSchemesPageVM;
        Windows::Foundation::Collections::IObservableVector<Editor::ExtensionPackageViewModel> _extensionPackages;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> _profilesModifiedView;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentProfileViewModel> _profilesAddedView;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentColorSchemeViewModel> _colorSchemesAddedView;
        bool _extensionsLoaded;

        void _UpdateListViews(bool updateProfilesModified, bool updateProfilesAdded, bool updateColorSchemesAdded);
    };

    struct ExtensionPackageViewModel : ExtensionPackageViewModelT<ExtensionPackageViewModel>, ViewModelHelper<ExtensionPackageViewModel>
    {
    public:
        ExtensionPackageViewModel(const Model::ExtensionPackage& pkg, const Model::CascadiaSettings& settings) :
            _package{ pkg },
            _settings{ settings },
            _fragmentExtensions{ single_threaded_observable_vector<Editor::FragmentExtensionViewModel>() } {}

        static bool SortAscending(const Editor::ExtensionPackageViewModel& lhs, const Editor::ExtensionPackageViewModel& rhs);

        void UpdateSettings(const Model::CascadiaSettings& settings);

        Model::ExtensionPackage Package() const noexcept { return _package; }
        hstring Scope() const noexcept;
        bool Enabled() const;
        void Enabled(bool val);
        hstring AccessibleName() const noexcept;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentExtensionViewModel> FragmentExtensions() { return _fragmentExtensions; }

    private:
        Model::ExtensionPackage _package;
        Model::CascadiaSettings _settings;
        Windows::Foundation::Collections::IObservableVector<Editor::FragmentExtensionViewModel> _fragmentExtensions;
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

        static bool SortAscending(const Editor::FragmentProfileViewModel& lhs, const Editor::FragmentProfileViewModel& rhs);

        Model::Profile Profile() const { return _deducedProfile; };
        hstring SourceName() const { return _fragment.Source(); }
        hstring Json() const { return _entry.Json(); }
        hstring AccessibleName() const noexcept;

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

        static bool SortAscending(const Editor::FragmentColorSchemeViewModel& lhs, const Editor::FragmentColorSchemeViewModel& rhs);

        Editor::ColorSchemeViewModel ColorSchemeVM() const { return _deducedSchemeVM; };
        hstring SourceName() const { return _fragment.Source(); }
        hstring Json() const { return _entry.Json(); }
        hstring AccessibleName() const noexcept;

    private:
        Model::FragmentColorSchemeEntry _entry;
        Model::FragmentSettings _fragment;
        Editor::ColorSchemeViewModel _deducedSchemeVM;
    };

    struct ExtensionPackageTemplateSelector : public ExtensionPackageTemplateSelectorT<ExtensionPackageTemplateSelector>
    {
    public:
        ExtensionPackageTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const Windows::Foundation::IInspectable& item, const Windows::UI::Xaml::DependencyObject& container);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const Windows::Foundation::IInspectable& item);

        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, DefaultTemplate, nullptr);
        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, ComplexTemplate, nullptr);
        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, ComplexTemplateWithFontIcon, nullptr);
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Extensions);
    BASIC_FACTORY(ExtensionPackageTemplateSelector);
}
