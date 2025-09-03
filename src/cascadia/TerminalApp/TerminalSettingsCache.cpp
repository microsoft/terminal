// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettingsCache.h"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"

namespace winrt
{
    namespace MTSM = Microsoft::Terminal::Settings::Model;
}

namespace winrt::TerminalApp::implementation
{
    TerminalSettingsPair::TerminalSettingsPair(const winrt::Microsoft::Terminal::Settings::TerminalSettingsCreateResult& result)
    {
        result.DefaultSettings().try_as(_defaultSettings);
        result.UnfocusedSettings().try_as(_unfocusedSettings);
    }

    TerminalSettingsCache::TerminalSettingsCache(const MTSM::CascadiaSettings& settings)
    {
        Reset(settings);
    }

    std::optional<TerminalSettingsPair> TerminalSettingsCache::TryLookup(const MTSM::Profile& profile)
    {
        const auto found{ profileGuidSettingsMap.find(profile.Guid()) };
        // GH#2455: If there are any panes with controls that had been
        // initialized with a Profile that no longer exists in our list of
        // profiles, we'll leave it unmodified. The profile doesn't exist
        // anymore, so we can't possibly update its settings.
        if (found != profileGuidSettingsMap.cend())
        {
            auto& pair{ found->second };
            if (!pair.second)
            {
                pair.second = winrt::Microsoft::Terminal::Settings::TerminalSettings::CreateWithProfile(_settings, pair.first);
            }
            return std::optional{ TerminalSettingsPair{ *pair.second } };
        }

        return std::nullopt;
    }

    void TerminalSettingsCache::Reset(const MTSM::CascadiaSettings& settings)
    {
        _settings = settings;

        // Mapping by GUID isn't _excellent_ because the defaults profile doesn't have a stable GUID; however,
        // when we stabilize its guid this will become fully safe.
        const auto profileDefaults{ _settings.ProfileDefaults() };
        const auto allProfiles{ _settings.AllProfiles() };
        profileGuidSettingsMap.clear();
        profileGuidSettingsMap.reserve(allProfiles.Size() + 1);

        // Include the Defaults profile for consideration
        profileGuidSettingsMap.insert_or_assign(profileDefaults.Guid(), std::pair{ profileDefaults, std::nullopt });
        for (const auto& newProfile : allProfiles)
        {
            // Avoid creating a TerminalSettings right now. They're not totally cheap, and we suspect that users with many
            // panes may not be using all of their profiles at the same time. Lazy evaluation is king!
            profileGuidSettingsMap.insert_or_assign(newProfile.Guid(), std::pair{ newProfile, std::nullopt });
        }
    }
}
