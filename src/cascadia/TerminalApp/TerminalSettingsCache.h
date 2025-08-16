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

namespace winrt::TerminalApp::implementation
{
    struct TerminalSettingsCache
    {
        TerminalSettingsCache(const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings, const TerminalApp::AppKeyBindings& bindings);
        Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult TryLookup(const Microsoft::Terminal::Settings::Model::Profile& profile);
        void Reset(const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings, const TerminalApp::AppKeyBindings& bindings);

    private:
        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };
        TerminalApp::AppKeyBindings _bindings{ nullptr };
        std::unordered_map<winrt::guid, std::pair<Microsoft::Terminal::Settings::Model::Profile, Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult>> profileGuidSettingsMap;
    };
}
