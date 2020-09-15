/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.h

Abstract:
- This class acts as the container for all app settings. It's composed of two
        parts: Globals, which are app-wide settings, and Profiles, which contain
        a set of settings that apply to a single instance of the terminal.
  Also contains the logic for serializing and deserializing this object.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "CascadiaSettings.g.h"

#include "GlobalAppSettings.h"
#include "TerminalWarnings.h"
#include "IDynamicProfileGenerator.h"

#include "Profile.h"
#include "ColorScheme.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ProfileTests;
    class ColorSchemeTests;
    class KeyBindingsTests;
    class TabTests;
};
namespace TerminalAppUnitTests
{
    class DynamicProfileTests;
    class JsonTests;
};

namespace Microsoft::Terminal::Settings::Model
{
    class SettingsTypedDeserializationException;
};

class Microsoft::Terminal::Settings::Model::SettingsTypedDeserializationException final : public std::runtime_error
{
public:
    SettingsTypedDeserializationException(const std::string_view description) :
        runtime_error(description.data()) {}
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct CascadiaSettings : CascadiaSettingsT<CascadiaSettings>
    {
    public:
        CascadiaSettings();
        explicit CascadiaSettings(const bool addDynamicProfiles);

        static Microsoft::Terminal::Settings::Model::CascadiaSettings LoadDefaults();
        static Microsoft::Terminal::Settings::Model::CascadiaSettings LoadAll();
        static Microsoft::Terminal::Settings::Model::CascadiaSettings LoadUniversal();

        Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings() const;

        Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Model::Profile> Profiles() const noexcept;

        Microsoft::Terminal::Settings::Model::KeyMapping KeyMap() const noexcept;

        static com_ptr<CascadiaSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        static hstring SettingsPath();
        static hstring DefaultSettingsPath();

        static winrt::hstring ApplicationDisplayName();
        static winrt::hstring ApplicationVersion();

        Microsoft::Terminal::Settings::Model::Profile FindProfile(guid profileGuid) const noexcept;
        Microsoft::Terminal::Settings::Model::ColorScheme GetColorSchemeForProfile(const guid profileGuid) const;

        Windows::Foundation::Collections::IVectorView<SettingsLoadWarnings> Warnings();
        Windows::Foundation::IReference<SettingsLoadErrors> GetLoadingError();
        hstring GetSerializationErrorMessage();

        winrt::guid GetProfileForArgs(const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs) const;

    private:
        com_ptr<GlobalAppSettings> _globals;
        Windows::Foundation::Collections::IObservableVector<Microsoft::Terminal::Settings::Model::Profile> _profiles;
        Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> _warnings;
        Windows::Foundation::IReference<SettingsLoadErrors> _loadError;
        hstring _deserializationErrorMessage;

        std::vector<std::unique_ptr<::Microsoft::Terminal::Settings::Model::IDynamicProfileGenerator>> _profileGenerators;

        std::string _userSettingsString;
        Json::Value _userSettings;
        Json::Value _defaultSettings;
        Json::Value _userDefaultProfileSettings{ Json::Value::null };

        void _LayerOrCreateProfile(const Json::Value& profileJson);
        winrt::com_ptr<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile> _FindMatchingProfile(const Json::Value& profileJson);
        void _LayerOrCreateColorScheme(const Json::Value& schemeJson);
        winrt::com_ptr<winrt::Microsoft::Terminal::Settings::Model::implementation::ColorScheme> _FindMatchingColorScheme(const Json::Value& schemeJson);
        void _ParseJsonString(std::string_view fileData, const bool isDefaultSettings);
        static const Json::Value& _GetProfilesJsonObject(const Json::Value& json);
        static const Json::Value& _GetDisabledProfileSourcesJsonObject(const Json::Value& json);
        bool _PrependSchemaDirective();
        bool _AppendDynamicProfilesToUserSettings();
        std::string _ApplyFirstRunChangesToSettingsTemplate(std::string_view settingsTemplate) const;

        void _ApplyDefaultsFromUserSettings();

        void _LoadDynamicProfiles();

        static bool _IsPackaged();
        static void _WriteSettings(const std::string_view content);
        static std::optional<std::string> _ReadUserSettings();
        static std::optional<std::string> _ReadFile(HANDLE hFile);

        std::optional<guid> _GetProfileGuidByName(const hstring) const;
        std::optional<guid> _GetProfileGuidByIndex(std::optional<int> index) const;

        void _ValidateSettings();
        void _ValidateProfilesExist();
        void _ValidateProfilesHaveGuid();
        void _ValidateDefaultProfileExists();
        void _ValidateNoDuplicateProfiles();
        void _ResolveDefaultProfile();
        void _ReorderProfilesToMatchUserSettingsOrder();
        void _RemoveHiddenProfiles();
        void _ValidateAllSchemesExist();
        void _ValidateMediaResources();
        void _ValidateKeybindings();
        void _ValidateNoGlobalsKey();

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::ProfileTests;
        friend class TerminalAppLocalTests::ColorSchemeTests;
        friend class TerminalAppLocalTests::KeyBindingsTests;
        friend class TerminalAppLocalTests::TabTests;
        friend class TerminalAppUnitTests::DynamicProfileTests;
        friend class TerminalAppUnitTests::JsonTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(CascadiaSettings);
}
