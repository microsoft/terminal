// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SearchIndex.h"
#include "FilteredSearchResult.g.cpp"
#include "SearchResultTemplateSelector.g.cpp"

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <ScopedResourceLoader.h>
#include <til/static_map.h>

static constexpr std::wstring_view openJsonTag{ L"OpenJson_Nav" };
static constexpr std::wstring_view launchTag{ L"Launch_Nav" };
static constexpr std::wstring_view interactionTag{ L"Interaction_Nav" };
static constexpr std::wstring_view renderingTag{ L"Rendering_Nav" };
static constexpr std::wstring_view compatibilityTag{ L"Compatibility_Nav" };
static constexpr std::wstring_view actionsTag{ L"Actions_Nav" };
static constexpr std::wstring_view newTabMenuTag{ L"NewTabMenu_Nav" };
static constexpr std::wstring_view extensionsTag{ L"Extensions_Nav" };
static constexpr std::wstring_view globalProfileTag{ L"GlobalProfile_Nav" };
static constexpr std::wstring_view addProfileTag{ L"AddProfile" };
static constexpr std::wstring_view colorSchemesTag{ L"ColorSchemes_Nav" };
static constexpr std::wstring_view globalAppearanceTag{ L"GlobalAppearance_Nav" };

static constexpr til::static_map NavTagIconMap{
    std::pair{ launchTag, L"\xE7B5" }, /* Set Lock Screen */
    std::pair{ interactionTag, L"\xE7C9" }, /* Touch Pointer */
    std::pair{ globalAppearanceTag, L"\xE771" }, /* Personalize */
    std::pair{ colorSchemesTag, L"\xE790" }, /* Color */
    std::pair{ renderingTag, L"\xE7F8" }, /* Device Laptop No Pic */
    std::pair{ compatibilityTag, L"\xEC7A" }, /* Developer Tools */
    std::pair{ actionsTag, L"\xE765" }, /* Keyboard Classic */
    std::pair{ newTabMenuTag, L"\xE71D" }, /* All Apps */
    std::pair{ extensionsTag, L"\xEA86" }, /* Puzzle */
    std::pair{ globalProfileTag, L"\xE81E" }, /* Map Layers */
    std::pair{ addProfileTag, L"\xE710" }, /* Add */
    std::pair{ openJsonTag, L"\xE713" }, /* Settings */
};

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    const ScopedResourceLoader& EnglishOnlyResourceLoader() noexcept
    {
        static ScopedResourceLoader loader{ GetLibraryResourceLoader().WithQualifier(L"language", L"en-US") };
        return loader;
    }

    DataTemplate SearchResultTemplateSelector::SelectTemplateCore(const IInspectable& item, const DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    DataTemplate SearchResultTemplateSelector::SelectTemplateCore(const IInspectable& item)
    {
        if (const auto searchResultItem = item.try_as<FilteredSearchResult>())
        {
            if (!searchResultItem->SecondaryLabel().empty())
            {
                return ComplexTemplate();
            }
            return BasicTemplate();
        }
        return nullptr;
    }

    Editor::FilteredSearchResult FilteredSearchResult::CreateNoResultsItem(const winrt::hstring& query)
    {
        return winrt::make<FilteredSearchResult>(nullptr, nullptr, hstring{ fmt::format(fmt::runtime(std::wstring{ RS_(L"Search_NoResults") }), query) });
    }

    // Creates a FilteredSearchResult with the given search index entry and runtime object.
    // The resulting search result will have a label like "<ProfileName>: <display text>" or "<ProfileName>".
    // This is so that we can reuse the display text from the search index, but also add additional context to which runtime object this search result maps to.
    Editor::FilteredSearchResult FilteredSearchResult::CreateRuntimeObjectItem(const LocalizedIndexEntry* searchIndexEntry, const Windows::Foundation::IInspectable& runtimeObj)
    {
        hstring runtimeObjLabel{};
        hstring runtimeObjContext{};
        if (const auto profileVM = runtimeObj.try_as<Editor::ProfileViewModel>())
        {
            // No runtimeObjContext: profile name and icon should be enough
            runtimeObjLabel = profileVM.Name();
        }
        else if (const auto colorSchemeVM = runtimeObj.try_as<Editor::ColorSchemeViewModel>())
        {
            // No runtimeObjContext: scheme name and generic icon should be enough
            runtimeObjLabel = colorSchemeVM.Name();
        }
        else if (const auto ntmFolderEntryVM = runtimeObj.try_as<Editor::FolderEntryViewModel>())
        {
            runtimeObjLabel = ntmFolderEntryVM.Name();
            runtimeObjContext = RS_(L"Nav_NewTabMenu/Content");
        }
        else if (const auto extensionPackageVM = runtimeObj.try_as<Editor::ExtensionPackageViewModel>())
        {
            runtimeObjLabel = extensionPackageVM.Package().DisplayName();
            runtimeObjContext = RS_(L"Nav_Extensions/Content");
        }

        hstring displayText{};
        if (searchIndexEntry)
        {
            if (searchIndexEntry->Entry)
            {
                displayText = searchIndexEntry->Entry->DisplayTextLocalized;
            }
            else if (searchIndexEntry->DisplayTextNeutral)
            {
                displayText = searchIndexEntry->DisplayTextNeutral.value();
            }
        }

        if (displayText.empty())
        {
            // primaryText: <runtimeObjLabel>
            // secondaryText: <runtimeObjContext>
            // "PowerShell" --> navigates to main runtime object page (i.e. Profiles_Base)
            // "SSH" | "Extension" --> navigates to main runtime object page (i.e. Extensions > SSH)
            return winrt::make<FilteredSearchResult>(searchIndexEntry, runtimeObj, runtimeObjLabel, runtimeObjContext);
        }
        // primaryText: <displayText>
        // secondaryText: <runtimeObjLabel>
        // navigates to setting container
        return winrt::make<FilteredSearchResult>(searchIndexEntry, runtimeObj, displayText, runtimeObjLabel);
    }

    winrt::hstring FilteredSearchResult::Label() const
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

    bool FilteredSearchResult::IsNoResultsPlaceholder() const
    {
        return _overrideLabel.has_value() && !_NavigationArgOverride;
    }

    Windows::Foundation::IInspectable FilteredSearchResult::NavigationArg() const
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

    Windows::UI::Xaml::Controls::IconElement FilteredSearchResult::Icon() const
    {
        // We need to set the icon size here as opposed to in the XAML.
        // Setting it in the XAML just crops the icon.
        static constexpr double iconSize = 16;
        if (auto navigationArg = NavigationArg())
        {
            if (const auto profileVM = navigationArg.try_as<Editor::ProfileViewModel>())
            {
                auto icon = UI::IconPathConverter::IconWUX(profileVM.EvaluatedIcon());
                icon.Width(iconSize);
                icon.Height(iconSize);
                return icon;
            }
            else if (const auto colorSchemeVM = navigationArg.try_as<Editor::ColorSchemeViewModel>())
            {
                WUX::Controls::FontIcon icon{};
                icon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                icon.FontSize(iconSize);
                icon.Glyph(NavTagIconMap[colorSchemesTag]);
                return icon;
            }
            else if (const auto ntmFolderEntryVM = navigationArg.try_as<Editor::FolderEntryViewModel>())
            {
                auto icon = UI::IconPathConverter::IconWUX(ntmFolderEntryVM.Icon());
                icon.Width(iconSize);
                icon.Height(iconSize);
                return icon;
            }
            else if (const auto extensionPackageVM = navigationArg.try_as<Editor::ExtensionPackageViewModel>())
            {
                auto icon = UI::IconPathConverter::IconWUX(extensionPackageVM.Package().Icon());
                icon.Width(iconSize);
                icon.Height(iconSize);
                return icon;
            }
            else if (const auto stringNavArg = navigationArg.try_as<hstring>())
            {
                WUX::Controls::FontIcon icon{};
                icon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                icon.FontSize(iconSize);

                if (_SearchIndexEntry->Entry->SubPage == BreadcrumbSubPage::ColorSchemes_Edit)
                {
                    // If we're editing a color scheme, stringNavArg is the color scheme name.
                    // Use the color scheme icon.
                    icon.Glyph(NavTagIconMap[colorSchemesTag]);
                    return icon;
                }
                else if (const auto it = NavTagIconMap.find(*stringNavArg); it != NavTagIconMap.end())
                {
                    // Use the font icon used by the navigation view item
                    icon.Glyph(it->second);
                    return icon;
                }
            }
        }
        return nullptr;
    }

    // Reset the search index to the build-time data from GeneratedSettingsIndex.g.h
    void SearchIndex::Reset()
    {
        // copied from CommandPaletteItems.h
        static bool shouldIncludeLanguageNeutralResources = [] {
            try
            {
                const auto context{ winrt::Windows::ApplicationModel::Resources::Core::ResourceContext::GetForViewIndependentUse() };
                const auto qualifiers{ context.QualifierValues() };
                if (const auto language{ qualifiers.TryLookup(L"language") })
                {
                    return !til::starts_with_insensitive_ascii(*language, L"en-");
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
            return false;
        }();

        // Creates the LocalizedIndexEntry wrapper objects around the given index entries
        // and adds them to the given storage vector.
        auto loadLocalizedIndex = [&](const auto& indexRef) {
            std::vector<LocalizedIndexEntry> localizedIndex;
            localizedIndex.reserve(indexRef.size());
            for (const auto& entry : indexRef)
            {
                LocalizedIndexEntry localizedEntry;
                localizedEntry.Entry = &entry;
                if (shouldIncludeLanguageNeutralResources)
                {
                    localizedEntry.DisplayTextNeutral = EnglishOnlyResourceLoader().GetLocalizedString(entry.DisplayTextUid);
                    if (entry.HelpTextUid)
                    {
                        localizedEntry.HelpTextNeutral = EnglishOnlyResourceLoader().GetLocalizedString(entry.HelpTextUid.value());
                    }
                }
                localizedIndex.emplace_back(std::move(localizedEntry));
            }
            return localizedIndex;
        };

        IndexData indexData{
            .mainIndex = loadLocalizedIndex(LoadBuildTimeIndex()),
            .profileIndex = loadLocalizedIndex(LoadProfileIndex()),
            .ntmFolderIndex = loadLocalizedIndex(LoadNTMFolderIndex()),
            .colorSchemeIndex = loadLocalizedIndex(LoadColorSchemeIndex())
        };

        // Load partial entries for runtime object main pages
        //   (i.e. PowerShell profile --> PartialProfileIndexEntry() --> Profile page w/ PowerShell profile VM as context)
        indexData.profileIndexEntry.Entry = &PartialProfileIndexEntry();
        indexData.ntmFolderIndexEntry.Entry = &PartialNTMFolderIndexEntry();
        indexData.extensionIndexEntry.Entry = &PartialExtensionIndexEntry();
        indexData.colorSchemeIndexEntry.Entry = &PartialColorSchemeIndexEntry();

        _index = std::move(indexData);
    }

    // Method Description:
    // - Gets the search results based on the given query string. Can be cancelled.
    // - Some results (i.e. profiles) are dynamically generated at runtime, so they need to be passed in here so that we can include them.
    // - LOAD-BEARING: vector views CANNOT be passed by reference. We need the AddRef to avoid lifetime issues with the async operation.
    // Arguments:
    // - query - the search query string
    // - profileVMs - the list of profile view models to search
    // - ntmFolderVMs - the list of New Tab Menu folder entry view models to search
    // - colorSchemeVMs - the list of color scheme view models to search
    // - extensionPkgVMs - the list of extension package view models to search
    // Return value:
    // - The results are sorted by score (best matches first).
    // - If no results are found, a "no results" placeholder item is returned.
    IAsyncOperation<IObservableVector<Windows::Foundation::IInspectable>> SearchIndex::SearchAsync(const hstring& query,
                                                                                                   const IVectorView<Editor::ProfileViewModel> profileVMs,
                                                                                                   const IVectorView<Editor::FolderEntryViewModel> ntmFolderVMs,
                                                                                                   const IVectorView<Editor::ColorSchemeViewModel> colorSchemeVMs,
                                                                                                   const IVectorView<Editor::ExtensionPackageViewModel> extensionPkgVMs)
    {
        co_await winrt::resume_background();

        // The search can be cancelled at any time by the caller.
        // Get a token to check for cancellation as we go.
        auto cancellationToken = co_await get_cancellation_token();
        const auto filter = fzf::matcher::ParsePattern(std::wstring_view{ query });

        // 1st pass: filter and score build-time index, this reduces the search space for the 2nd pass
        auto findMatchingResults = [&](const std::vector<LocalizedIndexEntry>& searchIndex) {
            std::vector<std::pair<int, const LocalizedIndexEntry*>> filteredIndex;
            for (const auto& entry : searchIndex)
            {
                if (cancellationToken())
                {
                    break;
                }

                // Iterate through all searchable fields and find the best weighted score
                int32_t bestScore = 0;
                for (const auto& [searchTextOpt, weight] : entry.GetSearchableFields())
                {
                    if (searchTextOpt.has_value())
                    {
                        if (const auto match = fzf::matcher::Match(searchTextOpt.value(), filter))
                        {
                            const auto weightedScore = match->Score * weight;
                            bestScore = std::max(bestScore, weightedScore);
                        }
                    }
                }

                if (bestScore > 0)
                {
                    filteredIndex.emplace_back(bestScore, &entry);
                }
            }
            return filteredIndex;
        };

        std::vector<std::pair<int, Editor::FilteredSearchResult>> scoredResults;
        for (const auto& [score, entry] : findMatchingResults(_index.mainIndex))
        {
            scoredResults.emplace_back(score, winrt::make<FilteredSearchResult>(entry));
        }

        // 2nd pass: filter and score runtime objects (i.e. profiles)
        //   appends results to scoredResults
        auto appendRuntimeObjectResults = [&](const auto& runtimeObjectList, const auto& filteredSearchIndex, const auto& partialSearchIndexEntry) {
            for (const auto& runtimeObj : runtimeObjectList)
            {
                if (cancellationToken())
                {
                    break;
                }

                const auto& objName = runtimeObj.Name();
                if (const auto match = fzf::matcher::Match(objName, filter))
                {
                    // navigates to runtime object main page (i.e. "PowerShell" Profiles_Base page)
                    scoredResults.emplace_back(match->Score, FilteredSearchResult::CreateRuntimeObjectItem(&partialSearchIndexEntry, runtimeObj));
                }
                for (const auto& scoredResult : filteredSearchIndex)
                {
                    // navigates to runtime object's setting (i.e. "PowerShell: Command line" )
                    scoredResults.emplace_back(scoredResult.first, FilteredSearchResult::CreateRuntimeObjectItem(scoredResult.second, runtimeObj));
                }
            }
        };

        appendRuntimeObjectResults(profileVMs, findMatchingResults(_index.profileIndex), _index.profileIndexEntry);
        appendRuntimeObjectResults(ntmFolderVMs, findMatchingResults(_index.ntmFolderIndex), _index.ntmFolderIndexEntry);
        appendRuntimeObjectResults(colorSchemeVMs, findMatchingResults(_index.colorSchemeIndex), _index.colorSchemeIndexEntry);

        // Extensions
        for (const auto& extension : extensionPkgVMs)
        {
            if (const auto match = fzf::matcher::Match(extension.Package().DisplayName(), filter))
            {
                scoredResults.emplace_back(match->Score, FilteredSearchResult::CreateRuntimeObjectItem(&_index.extensionIndexEntry, extension));
            }
        }

        // must be IInspectable to be used as ItemsSource in XAML
        std::vector<Windows::Foundation::IInspectable> results;
        if (scoredResults.empty())
        {
            // Explicitly show "no results"
            results.reserve(1);
            results.emplace_back(FilteredSearchResult::CreateNoResultsItem(query));
        }
        else
        {
            // Sort results by score (descending)
            results.reserve(scoredResults.size());
            std::sort(scoredResults.begin(), scoredResults.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            std::transform(scoredResults.begin(), scoredResults.end(), std::back_inserter(results), [](const auto& pair) { return pair.second; });
        }

        if (cancellationToken())
        {
            // Search was cancelled; do not return any results
            co_return single_threaded_observable_vector<Windows::Foundation::IInspectable>();
        }
        co_return single_threaded_observable_vector(std::move(results));
    }
}
