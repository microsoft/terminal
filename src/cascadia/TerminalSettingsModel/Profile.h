/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Profile.hpp

Abstract:
- A profile acts as a single set of terminal settings. Many tabs or panes could
  exist side-by-side with different profiles simultaneously.
- Profiles could also specify their appearance when unfocused, this is what
  the inheritance tree looks like for unfocused settings:

                +-------------------+
                |                   |
                |Profile.defaults   |
                |                   |
                |DefaultAppearance  |
                |                   |
                +-------------------+
                   ^             ^
                   |             |
+------------------++           ++------------------+
|                   |           |                   |
|MyProfile          |           |Profile.defaults   |
|                   |           |                   |
|DefaultAppearance  |           |UnfocusedAppearance|
|                   |           |                   |
+-------------------+           +-------------------+
                   ^
                   |
+------------------++
|                   |
|MyProfile          |
|                   |
|UnfocusedAppearance|
|                   |
+-------------------+


Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "Profile.g.h"
#include "IInheritable.h"
#include "MTSMSettings.h"

#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"
#include <DefaultSettings.h>
#include "MediaResourceSupport.h"
#include "AppearanceConfig.h"
#include "FontConfig.h"

// fwdecl unittest classes
namespace SettingsModelUnitTests
{
    class DeserializationTests;
    class ProfileTests;
    class ColorSchemeTests;
    class KeyBindingsTests;
};
namespace TerminalAppUnitTests
{
    class DynamicProfileTests;
    class JsonTests;
};

using IEnvironmentVariableMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>;

// GUID used for generating GUIDs at runtime, for profiles that did not have a
// GUID specified manually.
constexpr GUID RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID = { 0xf65ddb7e, 0x706b, 0x4499, { 0x8a, 0x50, 0x40, 0x31, 0x3c, 0xaf, 0x51, 0x0a } };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Profile : ProfileT<Profile, IMediaResourceContainer>, IInheritable<Profile>
    {
    public:
        Profile() noexcept = default;
        // Not noexcept: Guid(guid) writes to _json, which allocates.
        // The old backing-field constructor was noexcept (POD copy into std::optional),
        // but JSON-backed storage requires heap allocation.
        Profile(guid guid);

        void CreateUnfocusedAppearance();
        void DeleteUnfocusedAppearance();

        hstring ToString()
        {
            return Name();
        }

        static void CopyInheritanceGraphs(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited, const std::vector<winrt::com_ptr<Profile>>& source, std::vector<winrt::com_ptr<Profile>>& target);
        winrt::com_ptr<Profile>& CopyInheritanceGraph(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited) const;
        winrt::com_ptr<Profile> CopySettings() const;

        static com_ptr<Profile> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        // Generic setting access via SettingKey
        bool HasSetting(ProfileSettingKey key) const;
        void ClearSetting(ProfileSettingKey key);
        std::vector<ProfileSettingKey> CurrentSettings() const;

        hstring EvaluatedStartingDirectory() const;

        Model::IAppearanceConfig DefaultAppearance();
        Model::FontConfig FontInfo();

        static std::wstring NormalizeCommandLine(LPCWSTR commandLine);

        void _FinalizeInheritance() override;
        void _ValidateThisLayer() const override;

        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;

        void ResolveMediaResources(const Model::MediaResourceResolver& resolver);

        void Icon(const winrt::hstring& path)
        {
            // Internal Helper (overload version)
            Icon(MediaResource::FromString(path));
        }

        WINRT_PROPERTY(bool, Deleted, false);
        WINRT_PROPERTY(bool, Orphaned, false);
        WINRT_PROPERTY(OriginTag, Origin, OriginTag::None);
        WINRT_PROPERTY(guid, Updates);

        // Nullable/optional settings (JSON-backed)
        INHERITABLE_NULLABLE_SETTING(Model::Profile, Microsoft::Terminal::Core::Color, TabColor, "tabColor", nullptr)
        INHERITABLE_MUTABLE_SETTING(Model::Profile, Model::IAppearanceConfig, UnfocusedAppearance, nullptr);

        // Settings that are JSON-backed but need custom handling in ToJson/LayerJson
        INHERITABLE_SETTING(Model::Profile, hstring, Name, "name", L"Default")
        INHERITABLE_SETTING(Model::Profile, hstring, Source, "source")
        INHERITABLE_SETTING(Model::Profile, bool, Hidden, "hidden", false)
        INHERITABLE_SETTING(Model::Profile, hstring, Padding, "padding", DEFAULT_PADDING)

        // Guid: hand-written JSON-backed (dynamic default)
        _BASE_INHERITABLE_SETTING(Model::Profile, guid, Guid, "guid")
    public:
        guid Guid() const
        {
            const auto val{ _getGuidImpl() };
            return val ? *val : _GenerateGuidForProfile(Name(), Source());
        }
        void Guid(const guid& value)
        {
            ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(
                _json, "guid", value);
        }

        winrt::hstring SourceBasePath;

    public:
#define PROFILE_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING_WITH_LOGGING(Model::Profile, type, name, jsonKey, ##__VA_ARGS__)
        MTSM_PROFILE_SETTINGS(PROFILE_SETTINGS_INITIALIZE)
#undef PROFILE_SETTINGS_INITIALIZE

        // IMediaResource settings with backing fields for resolution lifecycle.
        // _json is the source of truth for serialization. Setters dual-write to both
        // the backing field (for runtime resolution) and _json (for auto-save).
        INHERITABLE_MEDIA_RESOURCE_SETTING(Model::Profile, Icon, "icon", implementation::MediaResource::FromString(L"\uE756"))

        // BellSound: IVector<IMediaResource> with backing field + dual-write setter.
        _BASE_INHERITABLE_MUTABLE_SETTING(Model::Profile, std::optional<Windows::Foundation::Collections::IVector<IMediaResource>>, BellSound, nullptr)
    public:
        Windows::Foundation::Collections::IVector<IMediaResource> BellSound() const
        {
            const auto val{ _getBellSoundImpl() };
            return val ? *val : nullptr;
        }
        void BellSound(const Windows::Foundation::Collections::IVector<IMediaResource>& value)
        {
            _BellSound = value;
            ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(_json, "bellSound", value);
        }

    private:
        Model::IAppearanceConfig _DefaultAppearance{ winrt::make<AppearanceConfig>(weak_ref<Model::Profile>(*this)) };
        Model::FontConfig _FontInfo{ winrt::make<FontConfig>(weak_ref<Model::Profile>(*this)) };

        // Raw JSON for this layer. Populated by LayerJson(), will become the
        // source of truth for settings once the JSON-backed refactor is complete.
        Json::Value _json{ Json::ValueType::objectValue };

        std::set<std::string> _changeLog;

        static std::wstring EvaluateStartingDirectory(const std::wstring& directory);

        static guid _GenerateGuidForProfile(const std::wstring_view& name, const std::wstring_view& source) noexcept;

        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, const bool isSet);

        friend class SettingsModelUnitTests::DeserializationTests;
        friend class SettingsModelUnitTests::ProfileTests;
        friend class SettingsModelUnitTests::ColorSchemeTests;
        friend class SettingsModelUnitTests::KeyBindingsTests;
        friend class TerminalAppUnitTests::DynamicProfileTests;
        friend class TerminalAppUnitTests::JsonTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(Profile);
}
