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
#include <DefaultSettings.h>
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
    struct Profile : ProfileT<Profile>, IInheritable<Profile>
    {
    public:
        Profile() noexcept = default;
        Profile(guid guid) noexcept;

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

        hstring EvaluatedStartingDirectory() const;

        Model::IAppearanceConfig DefaultAppearance();
        Model::FontConfig FontInfo();

        winrt::hstring EvaluatedIcon();

        static std::wstring NormalizeCommandLine(LPCWSTR commandLine);

        void _FinalizeInheritance() override;

        // Special fields
        hstring Icon() const;
        void Icon(const hstring& value);

        WINRT_PROPERTY(bool, Deleted, false);
        WINRT_PROPERTY(OriginTag, Origin, OriginTag::None);
        WINRT_PROPERTY(guid, Updates);

        // Nullable/optional settings
        INHERITABLE_NULLABLE_SETTING(Model::Profile, Microsoft::Terminal::Core::Color, TabColor, nullptr);
        INHERITABLE_SETTING(Model::Profile, Model::IAppearanceConfig, UnfocusedAppearance, nullptr);

        // Settings that cannot be put in the macro because of how they are handled in ToJson/LayerJson
        INHERITABLE_SETTING(Model::Profile, hstring, Name, L"Default");
        INHERITABLE_SETTING(Model::Profile, hstring, Source);
        INHERITABLE_SETTING(Model::Profile, bool, Hidden, false);
        INHERITABLE_SETTING(Model::Profile, guid, Guid, _GenerateGuidForProfile(Name(), Source()));
        INHERITABLE_SETTING(Model::Profile, hstring, Padding, DEFAULT_PADDING);
        // Icon is _very special_ because we want to customize its setter
        _BASE_INHERITABLE_SETTING(Model::Profile, std::optional<hstring>, Icon, L"\uE756");

    public:
#define PROFILE_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::Profile, type, name, ##__VA_ARGS__)
        MTSM_PROFILE_SETTINGS(PROFILE_SETTINGS_INITIALIZE)
#undef PROFILE_SETTINGS_INITIALIZE

    private:
        Model::IAppearanceConfig _DefaultAppearance{ winrt::make<AppearanceConfig>(weak_ref<Model::Profile>(*this)) };
        Model::FontConfig _FontInfo{ winrt::make<FontConfig>(weak_ref<Model::Profile>(*this)) };

        std::optional<winrt::hstring> _evaluatedIcon{ std::nullopt };

        static std::wstring EvaluateStartingDirectory(const std::wstring& directory);

        static guid _GenerateGuidForProfile(const std::wstring_view& name, const std::wstring_view& source) noexcept;

        winrt::hstring _evaluateIcon() const;

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
