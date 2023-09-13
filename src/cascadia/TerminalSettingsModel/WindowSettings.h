/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WindowSettings.h

Abstract:
- This class encapsulates all of the settings that are specific to a single
  window. Broader than Profile settings (which are more like, per-pane
  settings). Different windows can have different settings for things like
  Theme, default profile, launch mode, etc.

Author(s):
- Mike Griese - Sept 2023

--*/
#pragma once

#include "WindowSettings.g.h"
#include "IInheritable.h"
#include "MTSMSettings.h"

#include "TerminalSettingsSerializationHelpers.h"
#include "ColorScheme.h"
#include "Theme.h"
#include "NewTabMenuEntry.h"
#include "RemainingProfilesEntry.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class ColorSchemeTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct WindowSettings : WindowSettingsT<WindowSettings>, IInheritable<WindowSettings>
    {
    public:
        void _FinalizeInheritance() override;
        com_ptr<WindowSettings> Copy() const;

        static com_ptr<WindowSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        Json::Value ToJson() const;

        // This DefaultProfile() setter is called by CascadiaSettings,
        // when it parses UnparsedDefaultProfile in _finalizeSettings().
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;

        til::property<winrt::hstring> Name;

        INHERITABLE_SETTING(Model::WindowSettings, hstring, UnparsedDefaultProfile, L"");

#define WINDOW_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::WindowSettings, type, name, ##__VA_ARGS__)
        MTSM_WINDOW_SETTINGS(WINDOW_SETTINGS_INITIALIZE)
#undef WINDOW_SETTINGS_INITIALIZE

    private:
        winrt::guid _defaultProfile;
    };
}
