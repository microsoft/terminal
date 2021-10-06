/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- GlobalAppSettings.h

Abstract:
- This class encapsulates all of the settings that are global to the app, and
    not a part of any particular profile.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "GlobalAppSettings.g.h"
#include "IInheritable.h"
#include "SettingsUtils.h"

#include "ActionMap.h"
#include "Command.h"
#include "ColorScheme.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class ColorSchemeTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct GlobalAppSettings : GlobalAppSettingsT<GlobalAppSettings>, IInheritable<GlobalAppSettings>
    {
    public:
        void _FinalizeInheritance() override;
        com_ptr<GlobalAppSettings> Copy() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> ColorSchemes() noexcept;
        void AddColorScheme(const Model::ColorScheme& scheme);
        void RemoveColorScheme(hstring schemeName);

        Model::ActionMap ActionMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        Json::Value ToJson() const;

        const std::vector<SettingsLoadWarnings>& KeybindingsWarnings() const;

        // This DefaultProfile() setter is called by CascadiaSettings,
        // when it parses UnparsedDefaultProfile in _finalizeSettings().
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;

        // TODO GH#9207: Remove this once we have a GlobalAppSettingsViewModel in TerminalSettingsEditor
        void SetInvertedDisableAnimationsValue(bool invertedDisableAnimationsValue)
        {
            DisableAnimations(!invertedDisableAnimationsValue);
        }

        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, UnparsedDefaultProfile, L"");

#define GLOBAL_APP_SETTINGS_INITIALIZE(type, name, ...) \
    INHERITABLE_SETTING(Model::GlobalAppSettings, type, name, ##__VA_ARGS__)
        GLOBAL_APP_SETTINGS(GLOBAL_APP_SETTINGS_INITIALIZE)
#undef GLOBAL_APP_SETTINGS_INITIALIZE

#define GLOBAL_CONTROL_SETTINGS_INITIALIZE(type, name, ...) \
    INHERITABLE_SETTING(Model::GlobalAppSettings, type, name, ##__VA_ARGS__)
        GLOBAL_CONTROL_SETTINGS(GLOBAL_CONTROL_SETTINGS_INITIALIZE)
#undef GLOBAL_CONTROL_SETTINGS_INITIALIZE

    private:
#ifdef NDEBUG
        static constexpr bool debugFeaturesDefault{ false };
#else
        static constexpr bool debugFeaturesDefault{ true };
#endif

        winrt::guid _defaultProfile;
        winrt::com_ptr<implementation::ActionMap> _actionMap{ winrt::make_self<implementation::ActionMap>() };
        std::vector<SettingsLoadWarnings> _keybindingsWarnings;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::ColorScheme> _colorSchemes{ winrt::single_threaded_map<winrt::hstring, Model::ColorScheme>() };
    };
}
