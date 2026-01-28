// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SearchIndex.h"
#include "FilteredSearchResult.g.cpp"
#include "SearchResultTemplateSelector.g.cpp"
#include "NavConstants.h"

#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <ScopedResourceLoader.h>

// Weight multipliers for search result scoring.
// Higher values prioritize certain types of matches over others.
static constexpr int WeightRuntimeObjectMatch = 6; // Direct runtime object name match (e.g., "PowerShell")
static constexpr int WeightProfileDefaults = 6; // Profile Defaults setting
static constexpr int WeightRuntimeObjectSetting = 5; // Setting with runtime object context (e.g., "PowerShell: Command line")
static constexpr int WeightDisplayTextLocalized = 5; // Display text in current locale
static constexpr int WeightDisplayTextNeutral = 2; // Display text in English (fallback)

// Minimum fzf score threshold to filter out low-quality fuzzy matches
static constexpr int MinimumMatchScore = 100;

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
    // Retrieves the searchable fields from the LocalizedIndexEntry and their associated weight bonus.
    // This allows us to prioritize certain fields over others when scoring search results.
    std::array<std::pair<std::optional<winrt::hstring>, int>, 2> LocalizedIndexEntry::GetSearchableFields() const
    {
        // Profile Defaults entries (DisplayTextUid starts with "Profile_") get a higher weight
        const auto weight = til::starts_with(std::wstring_view{ Entry->DisplayTextUid }, L"Profile_") ? WeightProfileDefaults : WeightDisplayTextLocalized;
        return { { { std::optional<winrt::hstring>{ Entry->DisplayTextLocalized }, weight },
                   { DisplayTextNeutral, WeightDisplayTextNeutral } } };
    }

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

        if (const auto& displayText = searchIndexEntry->Entry->DisplayTextLocalized; !displayText.empty())
        {
            // Full index entry (for settings within runtime objects)
            // - primaryText: <displayText>
            // - secondaryText: <runtimeObjLabel>
            // navigates to setting container
            return winrt::make<FilteredSearchResult>(searchIndexEntry, runtimeObj, displayText, runtimeObjLabel);
        }
        // Partial index entry (for runtime object main pages)
        // - primaryText: <runtimeObjLabel>
        // - secondaryText: <runtimeObjContext>
        // "PowerShell" --> navigates to main runtime object page (i.e. Profiles_Base)
        // "SSH" | "Extension" --> navigates to main runtime object page (i.e. Extensions > SSH)
        return winrt::make<FilteredSearchResult>(searchIndexEntry, runtimeObj, runtimeObjLabel, runtimeObjContext);
    }

    winrt::hstring FilteredSearchResult::Label() const
    {
        if (_overrideLabel)
        {
            return *_overrideLabel;
        }
        return _SearchIndexEntry->Entry->DisplayTextLocalized;
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

        std::vector<std::pair<int, Editor::FilteredSearchResult>> scoredResults;
        for (const auto& entry : _index.mainIndex)
        {
            if (cancellationToken())
            {
                break;
            }

            // Calculate best score across all searchable fields for entry
            int bestScore = 0;
            for (const auto& [searchTextOpt, weight] : entry.GetSearchableFields())
            {
                if (searchTextOpt.has_value())
                {
                    if (const auto match = fzf::matcher::Match(searchTextOpt.value(), filter))
                    {
                        bestScore = std::max(bestScore, match->Score * weight);
                    }
                }
            }

            if (bestScore >= MinimumMatchScore)
            {
                scoredResults.emplace_back(bestScore, winrt::make<FilteredSearchResult>(&entry));
            }
        }

        // Method Description:
        // - Filter and score index dependent on runtime objects (i.e. profiles)
        // - Matches against combined text: "<searchable field> <runtime object name>"
        // - This allows queries like "font size powershell" to find "PowerShell: Font size"
        // Arguments:
        // - runtimeObjectList: the list of runtime objects to search (i.e. profiles, ntm folders, extensions, etc...)
        // - searchIndex: the corresponding localized search index for the runtime object type (i.e. profile -> _index.profileIndex)
        // - partialSearchIndexEntry: index entry that is populated with the runtime object (i.e. profile -> _index.profileIndexEntry)
        auto appendRuntimeObjectResults = [&](const auto& runtimeObjectList, const std::vector<LocalizedIndexEntry>& searchIndex, const auto& partialSearchIndexEntry) {
            for (const auto& runtimeObj : runtimeObjectList)
            {
                if (cancellationToken())
                {
                    break;
                }

                // Check for direct runtime object name match first
                const auto& objName = runtimeObj.Name();
                const auto objNameMatch = fzf::matcher::Match(objName, filter);
                if (objNameMatch && objNameMatch->Score >= MinimumMatchScore)
                {
                    // navigates to runtime object main page (i.e. "PowerShell" Profiles_Base page)
                    scoredResults.emplace_back(objNameMatch->Score * WeightRuntimeObjectMatch,
                                               FilteredSearchResult::CreateRuntimeObjectItem(&partialSearchIndexEntry, runtimeObj));
                }

                for (const auto& entry : searchIndex)
                {
                    if (cancellationToken())
                    {
                        break;
                    }

                    // Calculate best score across all searchable fields for entry
                    int bestScore = 0;
                    for (const auto& [searchTextOpt, _] : entry.GetSearchableFields())
                    {
                        if (searchTextOpt.has_value())
                        {
                            // Score for combined text: "<searchable field> <runtime object name>"
                            const auto combinedText = fmt::format(L"{} {}", std::wstring_view{ searchTextOpt.value() }, std::wstring_view{ objName });
                            const auto combinedMatch = fzf::matcher::Match(combinedText, filter);
                            if (!combinedMatch)
                            {
                                continue;
                            }

                            const auto settingMatch = fzf::matcher::Match(searchTextOpt.value(), filter);

                            // Scoring:
                            // 1. require EITHER the runtime object OR the setting to match the query independently,
                            //    OR the combined match scores very well (handles "font size powershell" where neither matches alone)
                            // 2. only include if query matches combined text (i.e. "font size PowerShell" matches "<setting name> <profile name>")
                            // 3. combined match scores higher than runtime object alone (setting must contribute to the match)
                            //   NOTE: don't compare to settingMatch! This allows "font size" to show results for all profiles
                            const auto hasIndependentMatch = objNameMatch || settingMatch;
                            const auto hasStrongCombinedMatch = combinedMatch->Score >= MinimumMatchScore * 3;
                            if (!hasIndependentMatch && !hasStrongCombinedMatch)
                            {
                                continue;
                            }

                            if (combinedMatch->Score > (objNameMatch ? objNameMatch->Score : 0))
                            {
                                bestScore = std::max(combinedMatch->Score,
                                                     settingMatch ? settingMatch->Score : 0);
                            }
                        }
                    }

                    if (bestScore >= MinimumMatchScore)
                    {
                        // navigates to runtime object's setting (i.e. "PowerShell: Command line")
                        scoredResults.emplace_back(bestScore * WeightRuntimeObjectSetting,
                                                   FilteredSearchResult::CreateRuntimeObjectItem(&entry, runtimeObj));
                    }
                }
            }
        };

        appendRuntimeObjectResults(profileVMs, _index.profileIndex, _index.profileIndexEntry);
        appendRuntimeObjectResults(ntmFolderVMs, _index.ntmFolderIndex, _index.ntmFolderIndexEntry);
        appendRuntimeObjectResults(colorSchemeVMs, _index.colorSchemeIndex, _index.colorSchemeIndexEntry);

        // Extensions
        for (const auto& extension : extensionPkgVMs)
        {
            if (const auto match = fzf::matcher::Match(extension.Package().DisplayName(), filter); match && match->Score >= MinimumMatchScore)
            {
                scoredResults.emplace_back(match->Score * WeightRuntimeObjectMatch, FilteredSearchResult::CreateRuntimeObjectItem(&_index.extensionIndexEntry, extension));
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
