/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- TerminalSettingsCache.h

Abstract:
- This is a helper class used as we update the settings for panes. This class
  contains a single map of guid -> TerminalSettings, so that as we update all
  the panes during a settings reload, we only need to create a TerminalSettings
  once per profile.
--*/
#pragma once

#include "TerminalSettingsCache.g.h"
#include "TerminalSettingsPair.g.h"
#include "../TerminalSettingsAppAdapterLib//TerminalSettings.h"
#include <inc/cppwinrt_utils.h>

namespace winrt::TerminalApp::implementation
{
    class TerminalSettingsPair : public TerminalSettingsPairT<TerminalSettingsPair>
    {
    public:
        TerminalSettingsPair(const winrt::Microsoft::Terminal::Settings::TerminalSettingsCreateResult& result)
        {
            result.DefaultSettings().try_as(_defaultSettings);
            result.UnfocusedSettings().try_as(_unfocusedSettings);
        }

        winrt::Microsoft::Terminal::Control::IControlSettings DefaultSettings() { return _defaultSettings; };
        winrt::Microsoft::Terminal::Control::IControlSettings UnfocusedSettings() { return _unfocusedSettings; };

    private:
        winrt::Microsoft::Terminal::Control::IControlSettings _defaultSettings{ nullptr };
        winrt::Microsoft::Terminal::Control::IControlSettings _unfocusedSettings{ nullptr };
    };
    class TerminalSettingsCache : public TerminalSettingsCacheT<TerminalSettingsCache>
    {
    public:
        TerminalSettingsCache(const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings, const TerminalApp::AppKeyBindings& bindings);
        TerminalApp::TerminalSettingsPair TryLookup(const Microsoft::Terminal::Settings::Model::Profile& profile);
        void Reset(const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings, const TerminalApp::AppKeyBindings& bindings);

    private:
        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };
        TerminalApp::AppKeyBindings _bindings{ nullptr };
        std::unordered_map<winrt::guid, std::pair<Microsoft::Terminal::Settings::Model::Profile, std::optional<winrt::Microsoft::Terminal::Settings::TerminalSettingsCreateResult>>> profileGuidSettingsMap;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalSettingsCache);
}
