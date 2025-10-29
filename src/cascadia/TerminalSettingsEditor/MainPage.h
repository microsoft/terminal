// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MainPage.g.h"
#include "Breadcrumb.g.h"
#include "FilteredSearchResult.g.h"
#include "Utils.h"
#include "GeneratedSettingsIndex.g.h"
#include <til/generational.h>

class ScopedResourceLoader;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    const ScopedResourceLoader& EnglishOnlyResourceLoader() noexcept;

    struct Breadcrumb : BreadcrumbT<Breadcrumb>
    {
        Breadcrumb(IInspectable tag, winrt::hstring label, BreadcrumbSubPage subPage) :
            _Tag{ tag },
            _Label{ label },
            _SubPage{ subPage } {}

        hstring ToString() { return _Label; }

        WINRT_PROPERTY(IInspectable, Tag);
        WINRT_PROPERTY(winrt::hstring, Label);
        WINRT_PROPERTY(BreadcrumbSubPage, SubPage);
    };

    struct LocalizedIndexEntry
    {
        std::optional<winrt::hstring> DisplayTextNeutral = std::nullopt;
        std::optional<winrt::hstring> HelpTextNeutral = std::nullopt;
        const IndexEntry* Entry = nullptr;
    };

    struct FilteredSearchResult : FilteredSearchResultT<FilteredSearchResult>
    {
        FilteredSearchResult(const winrt::hstring& label) :
            _overrideLabel{ label } {}

        FilteredSearchResult(const LocalizedIndexEntry* entry, const Windows::Foundation::IInspectable& navigationArgOverride = nullptr) :
            _SearchIndexEntry{ entry },
            _NavigationArgOverride{ navigationArgOverride } {}

        hstring ToString() { return Label(); }

        winrt::hstring Label() const
        {
            if (_overrideLabel)
            {
                return _overrideLabel.value();
            }
            else if (_SearchIndexEntry)
            {
                if (_SearchIndexEntry->Entry)
                {
                    return _SearchIndexEntry->Entry->DisplayTextLocalized;
                }
                else if (_SearchIndexEntry->DisplayTextNeutral.has_value())
                {
                    return _SearchIndexEntry->DisplayTextNeutral.value();
                }
            }
            return {};
        }

        bool IsNoResultsPlaceholder() const
        {
            return _overrideLabel.has_value();
        }

        const LocalizedIndexEntry& SearchIndexEntry() const noexcept
        {
            return *_SearchIndexEntry;
        }

        Windows::Foundation::IInspectable NavigationArg() const
        {
            if (_NavigationArgOverride)
            {
                return _NavigationArgOverride;
            }
            else if (_SearchIndexEntry)
            {
                return _SearchIndexEntry->Entry->NavigationArg;
            }
            return nullptr;
        }

    private:
        const std::optional<winrt::hstring> _overrideLabel{ std::nullopt };
        const Windows::Foundation::IInspectable& _NavigationArgOverride{ nullptr };
        const LocalizedIndexEntry* _SearchIndexEntry{ nullptr };
    };

    struct MainPage : MainPageT<MainPage>
    {
        MainPage() = delete;
        MainPage(const Model::CascadiaSettings& settings);

        void UpdateSettings(const Model::CascadiaSettings& settings);

        safe_void_coroutine SettingsSearchBox_TextChanged(const Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs& args);
        void SettingsSearchBox_QuerySubmitted(const Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs& args);
        void SettingsSearchBox_SuggestionChosen(const Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxSuggestionChosenEventArgs& args);

        void SettingsNav_Loaded(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void SettingsNav_ItemInvoked(const Microsoft::UI::Xaml::Controls::NavigationView& sender, const Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs& args);
        void SaveButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void ResetButton_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void BreadcrumbBar_ItemClicked(const Microsoft::UI::Xaml::Controls::BreadcrumbBar& sender, const Microsoft::UI::Xaml::Controls::BreadcrumbBarItemClickedEventArgs& args);

        void SetHostingWindow(uint64_t hostingWindow) noexcept;
        bool TryPropagateHostingWindow(IInspectable object) noexcept;
        uint64_t GetHostingWindow() const noexcept;

        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        Windows::Foundation::Collections::IObservableVector<IInspectable> Breadcrumbs() noexcept;
        Editor::ExtensionsViewModel ExtensionsVM() const noexcept { return _extensionsVM; }

        til::typed_event<Windows::Foundation::IInspectable, Model::SettingsTarget> OpenJson;
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::Collections::IVectorView<Model::SettingsLoadWarnings>> ShowLoadWarningsDialog;

    private:
        Windows::Foundation::Collections::IObservableVector<IInspectable> _breadcrumbs;
        Windows::Foundation::Collections::IObservableVector<IInspectable> _menuItemSource;
        size_t _originalNumItems = 0u;

        Model::CascadiaSettings _settingsSource;
        Model::CascadiaSettings _settingsClone;

        std::optional<HWND> _hostingHwnd;

        void _InitializeProfilesList();
        void _CreateAndNavigateToNewProfile(const uint32_t index, const Model::Profile& profile);
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem _CreateProfileNavViewItem(const Editor::ProfileViewModel& profile);
        void _DeleteProfile(const Windows::Foundation::IInspectable sender, const Editor::DeleteProfileEventArgs& args);
        void _AddProfileHandler(const winrt::guid profileGuid);

        void _SetupProfileEventHandling(const winrt::Microsoft::Terminal::Settings::Editor::ProfileViewModel profile);

        void _PreNavigateHelper();
        void _Navigate(hstring clickedItemTag, BreadcrumbSubPage subPage, hstring elementToFocus = {});
        void _Navigate(const Editor::ProfileViewModel& profile, BreadcrumbSubPage subPage, hstring elementToFocus = {});
        void _Navigate(const Editor::NewTabMenuEntryViewModel& ntmEntryVM, BreadcrumbSubPage subPage, hstring elementToFocus = {});
        void _Navigate(const Editor::ExtensionPackageViewModel& extPkgVM, BreadcrumbSubPage subPage, hstring elementToFocus = {});
        void _NavigateToProfileHandler(const IInspectable& sender, winrt::guid profileGuid);
        void _NavigateToColorSchemeHandler(const IInspectable& sender, const IInspectable& args);

        void _UpdateBackgroundForMica();
        void _MoveXamlParsedNavItemsIntoItemSource();

        til::generation_t _QuerySearchIndex(const hstring& queryText);
        safe_void_coroutine _UpdateSearchIndex();

        Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Editor::ProfileViewModel> _profileVMs{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::ColorSchemesPageViewModel _colorSchemesPageVM{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::NewTabMenuViewModel _newTabMenuPageVM{ nullptr };
        winrt::Microsoft::Terminal::Settings::Editor::ExtensionsViewModel _extensionsVM{ nullptr };

        std::vector<LocalizedIndexEntry> _searchIndex;
        std::vector<LocalizedIndexEntry> _searchProfileIndex;
        std::vector<LocalizedIndexEntry> _searchNTMFolderIndex;
        std::vector<LocalizedIndexEntry> _searchColorSchemeIndex;
        til::generational<std::vector<const LocalizedIndexEntry*>> _filteredSearchIndex;
        std::vector<const LocalizedIndexEntry*> _filteredSearchProfileIndex;
        std::vector<const LocalizedIndexEntry*> _filteredSearchNTMFolderIndex;
        std::vector<const LocalizedIndexEntry*> _filteredSearchColorSchemeIndex;

        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _profileViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _colorSchemesPageViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ntmViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _extensionsViewModelChangedRevoker;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(MainPage);
}
