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

#include "winrt/Microsoft.Terminal.Settings.Model.h"
#include "winrt/TerminalApp.h"

namespace winrt::Microsoft::Terminal::Settings
{
    struct TerminalSettingsCreateResult;
}

namespace winrt::TerminalApp::implementation
{
    class TerminalSettingsPair
    {
    public:
        TerminalSettingsPair(const winrt::Microsoft::Terminal::Settings::TerminalSettingsCreateResult& result);

        winrt::Microsoft::Terminal::Control::IControlSettings DefaultSettings() const { return _defaultSettings; };
        winrt::Microsoft::Terminal::Control::IControlSettings UnfocusedSettings() const { return _unfocusedSettings; };

    private:
        winrt::Microsoft::Terminal::Control::IControlSettings _defaultSettings{ nullptr };
        winrt::Microsoft::Terminal::Control::IControlSettings _unfocusedSettings{ nullptr };
    };

    struct TerminalSettingsCache
    {
        TerminalSettingsCache(const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);
        std::optional<TerminalSettingsPair> TryLookup(const Microsoft::Terminal::Settings::Model::Profile& profile);
        void Reset(const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

    private:
        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };
        std::unordered_map<winrt::guid, std::pair<Microsoft::Terminal::Settings::Model::Profile, std::optional<winrt::Microsoft::Terminal::Settings::TerminalSettingsCreateResult>>> profileGuidSettingsMap;
    };
}
