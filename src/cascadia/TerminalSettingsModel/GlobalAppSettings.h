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
#include "MTSMSettings.h"

#include "ActionMap.h"
#include "Command.h"
#include "ColorScheme.h"
#include "Theme.h"
#include "NewTabMenuEntry.h"
#include "RemainingProfilesEntry.h"

// fwdecl unittest classes
namespace SettingsModelUnitTests
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
        Model::ColorScheme DuplicateColorScheme(const Model::ColorScheme& scheme);

        Model::ActionMap ActionMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);
        void LayerActionsFrom(const Json::Value& json, const bool withKeybindings = true);

        Json::Value ToJson() const;

        const std::vector<SettingsLoadWarnings>& KeybindingsWarnings() const;

        // This DefaultProfile() setter is called by CascadiaSettings,
        // when it parses UnparsedDefaultProfile in _finalizeSettings().
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::Theme> Themes() noexcept;
        void AddTheme(const Model::Theme& theme);
        Model::Theme CurrentTheme() noexcept;
        bool ShouldUsePersistedLayout() const;

        void ExpandCommands(const Windows::Foundation::Collections::IVectorView<Model::Profile>& profiles,
                            const Windows::Foundation::Collections::IMapView<winrt::hstring, Model::ColorScheme>& schemes);

        bool LegacyReloadEnvironmentVariables() const noexcept { return _legacyReloadEnvironmentVariables; }

        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, UnparsedDefaultProfile, L"");

#define GLOBAL_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::GlobalAppSettings, type, name, ##__VA_ARGS__)
        MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_INITIALIZE)
#undef GLOBAL_SETTINGS_INITIALIZE

    private:
#ifdef NDEBUG
        static constexpr bool debugFeaturesDefault{ false };
#else
        static constexpr bool debugFeaturesDefault{ true };
#endif

        winrt::guid _defaultProfile{};
        bool _legacyReloadEnvironmentVariables{ true };
        winrt::com_ptr<implementation::ActionMap> _actionMap{ winrt::make_self<implementation::ActionMap>() };

        std::vector<SettingsLoadWarnings> _keybindingsWarnings;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::ColorScheme> _colorSchemes{ winrt::single_threaded_map<winrt::hstring, Model::ColorScheme>() };
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::Theme> _themes{ winrt::single_threaded_map<winrt::hstring, Model::Theme>() };
    };
}
