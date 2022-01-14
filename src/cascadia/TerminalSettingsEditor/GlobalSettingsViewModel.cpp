// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalSettingsViewModel.h"
#include "GlobalSettingsViewModel.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include <WtExeUtils.h>

namespace winrt
{
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // For ComboBox an empty SelectedItem string denotes no selection.
    // What we want instead is for "Use system language" to be selected by default.
    // --> "und" is synonymous for "Use system language".
    constexpr std::wstring_view systemLanguageTag{ L"und" };

    GlobalSettingsViewModel::GlobalSettingsViewModel(const Model::GlobalAppSettings& globalSettings, const Model::CascadiaSettings& appSettings) :
        _globals{ globalSettings },
        _appSettings{ appSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(FirstWindowPreference, FirstWindowPreference, Model::FirstWindowPreference, L"Globals_FirstWindowPreference", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(LaunchMode, LaunchMode, Model::LaunchMode, L"Globals_LaunchMode", L"Content");
        // More options were added to the JSON mapper when the enum was made into [Flags]
        // but we want to preserve the previous set of options in the UI.
        _LaunchModeList.RemoveAt(7); // maximizedFullscreenFocus
        _LaunchModeList.RemoveAt(6); // fullscreenFocus
        _LaunchModeList.RemoveAt(3); // maximizedFullscreen
        INITIALIZE_BINDABLE_ENUM_SETTING(WindowingBehavior, WindowingMode, Model::WindowingMode, L"Globals_WindowingBehavior", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(Theme, ElementTheme, winrt::Windows::UI::Xaml::ElementTheme, L"Globals_Theme", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(TabWidthMode, TabViewWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, L"Globals_TabWidthMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(TabSwitcherMode, TabSwitcherMode, Model::TabSwitcherMode, L"Globals_TabSwitcherMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CopyFormat, CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, L"Globals_CopyFormat", L"Content");
    }

    void GlobalSettingsViewModel::SetInvertedDisableAnimationsValue(bool invertedDisableAnimationsValue)
    {
        _globals.SetInvertedDisableAnimationsValue(invertedDisableAnimationsValue);
        _NotifyChanges(L"DisableAnimations");
    }

    IInspectable GlobalSettingsViewModel::CurrentDefaultProfile()
    {
        const auto defaultProfileGuid{ _globals.DefaultProfile() };
        return winrt::box_value(_appSettings.FindProfile(defaultProfileGuid));
    }

    void GlobalSettingsViewModel::CurrentDefaultProfile(const IInspectable& value)
    {
        const auto profile{ winrt::unbox_value<Model::Profile>(value) };
        _globals.DefaultProfile(profile.Guid());
    }

    winrt::Windows::Foundation::Collections::IObservableVector<IInspectable> GlobalSettingsViewModel::DefaultProfiles() const
    {
        const auto allProfiles = _appSettings.AllProfiles();

        std::vector<IInspectable> profiles;
        profiles.reserve(allProfiles.Size());

        // Remove profiles from the selection which have been explicitly deleted.
        // We do want to show hidden profiles though, as they are just hidden
        // from menus, but still work as the startup profile for instance.
        for (const auto& profile : allProfiles)
        {
            if (!profile.Deleted())
            {
                profiles.emplace_back(profile);
            }
        }

        return winrt::single_threaded_observable_vector(std::move(profiles));
    }

    bool GlobalSettingsViewModel::ShowFirstWindowPreference() const noexcept
    {
        return Feature_PersistedWindowLayout::IsEnabled();
    }

    winrt::hstring GlobalSettingsViewModel::LanguageDisplayConverter(const winrt::hstring& tag)
    {
        if (tag == systemLanguageTag)
        {
            return RS_(L"Globals_LanguageDefault");
        }

        winrt::Windows::Globalization::Language language{ tag };
        return language.NativeName();
    }

    // Returns whether the language selector is available/shown.
    //
    // winrt::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride()
    // doesn't work for unpackaged applications. The corresponding code in TerminalApp is disabled.
    // It would be confusing for our users if we presented a dysfunctional language selector.
    bool GlobalSettingsViewModel::LanguageSelectorAvailable()
    {
        return IsPackaged();
    }

    // Returns the list of languages the user may override the application language with.
    // The returned list are BCP 47 language tags like {"und", "en-US", "de-DE", "es-ES", ...}.
    // "und" is short for "undefined" and is synonymous for "Use system language" in this code.
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> GlobalSettingsViewModel::LanguageList()
    {
        if (_languageList)
        {
            return _languageList;
        }

        if (!LanguageSelectorAvailable())
        {
            _languageList = {};
            return _languageList;
        }

        // In order to return the language list this code does the following:
        // [1] Get all possible languages we want to allow the user to choose.
        //     We have to acquire languages from multiple sources, creating duplicates. See below at [1].
        // [2] Sort languages by their ASCII tags, forcing the UI in a consistent/stable order.
        //     I wanted to sort the localized language names initially, but it turned out to be complex.
        // [3] Remove potential duplicates in our language list from [1].
        //     We don't want to have en-US twice in the list, do we?
        // [4] Optionally remove unwanted language tags (like pseudo-localizations).

        std::vector<winrt::hstring> tags;

        // [1]:
        {
            // ManifestLanguages contains languages the app ships with.
            //
            // Languages is a computed list that merges the ManifestLanguages with the
            // user's ranked list of preferred languages taken from the system settings.
            // As is tradition the API documentation is incomplete though, as it can also
            // contain regional language variants. If our app supports en-US, but the user
            // has en-GB or en-DE in their system's preferred language list, Languages will
            // contain those as well, as they're variants from a supported language. We should
            // allow a user to select those, as regional formattings can vary significantly.
            const std::array tagSources{
                winrt::Windows::Globalization::ApplicationLanguages::ManifestLanguages(),
                winrt::Windows::Globalization::ApplicationLanguages::Languages()
            };

            // tags will hold all the flattened results from tagSources.
            // We resize() the vector to the proper size in order to efficiently GetMany() all items.
            tags.resize(std::accumulate(
                tagSources.begin(),
                tagSources.end(),
                // tags[0] will be "und" - the "Use system language" item
                // tags[1..n] will contain tags from tagSources.
                // --> totalTags is offset by 1
                1,
                [](uint32_t sum, const auto& v) -> uint32_t {
                    return sum + v.Size();
                }));

            // As per the function definition, the first item
            // is always "Use system language" ("und").
            auto data = tags.data();
            *data++ = systemLanguageTag;

            // Finally GetMany() all the tags from tagSources.
            for (const auto& v : tagSources)
            {
                const auto size = v.Size();
                v.GetMany(0, winrt::array_view(data, size));
                data += size;
            }
        }

        // NOTE: The size of tags is always >0, due to tags[0] being hardcoded to "und".
        const auto tagsBegin = ++tags.begin();
        const auto tagsEnd = tags.end();

        // [2]:
        std::sort(tagsBegin, tagsEnd);

        // I'd love for both, std::unique and std::remove_if, to occur in a single loop,
        // but the code turned out to be complex and even less maintainable, so I gave up.
        {
            // [3] part 1:
            auto it = std::unique(tagsBegin, tagsEnd);

            // The qps- languages are useful for testing ("pseudo-localization").
            // --> Leave them in if debug features are enabled.
            if (!_globals.DebugFeaturesEnabled())
            {
                // [4] part 1:
                it = std::remove_if(tagsBegin, it, [](const winrt::hstring& tag) -> bool {
                    return til::starts_with(tag, L"qps-");
                });
            }

            // [3], [4] part 2 (completing the so called "erase-remove idiom"):
            tags.erase(it, tagsEnd);
        }

        _languageList = winrt::single_threaded_observable_vector(std::move(tags));
        return _languageList;
    }

    winrt::Windows::Foundation::IInspectable GlobalSettingsViewModel::CurrentLanguage()
    {
        if (_currentLanguage)
        {
            return _currentLanguage;
        }

        if (!LanguageSelectorAvailable())
        {
            _currentLanguage = {};
            return _currentLanguage;
        }

        // NOTE: PrimaryLanguageOverride throws if this instance is unpackaged.
        auto currentLanguage = winrt::Windows::Globalization::ApplicationLanguages::PrimaryLanguageOverride();
        if (currentLanguage.empty())
        {
            currentLanguage = systemLanguageTag;
        }

        _currentLanguage = winrt::box_value(currentLanguage);
        return _currentLanguage;
    }

    void GlobalSettingsViewModel::CurrentLanguage(const winrt::Windows::Foundation::IInspectable& tag)
    {
        _currentLanguage = tag;

        const auto currentLanguage = winrt::unbox_value<winrt::hstring>(_currentLanguage);
        if (currentLanguage == systemLanguageTag)
        {
            _globals.ClearLanguage();
        }
        else
        {
            _globals.Language(currentLanguage);
        }
    }

    bool GlobalSettingsViewModel::FeatureNotificationIconEnabled() const noexcept
    {
        return Feature_NotificationIcon::IsEnabled();
    }
}
