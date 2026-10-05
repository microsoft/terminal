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
#include "ProfileAdapter.h"
#include "IInheritable.h"
#include "MTSMSettings.h"

#include "JsonUtils.h"
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
    struct Profile : ProfileT<Profile, IMediaResourceContainer>
    {
    public:
        using CopyMap = std::unordered_map<const Native::Profile*, winrt::com_ptr<Native::Profile>>;

        Profile() noexcept;
        Profile(guid guid) noexcept;
        explicit Profile(winrt::com_ptr<Native::Profile> native);
        ~Profile();

        static winrt::com_ptr<Profile> FromNative(const winrt::com_ptr<Native::Profile>& native);
        const winrt::com_ptr<Native::Profile>& NativeModel() const noexcept { return _native; }

        void CreateUnfocusedAppearance() { _native->CreateUnfocusedAppearance(); }
        void DeleteUnfocusedAppearance() { _native->DeleteUnfocusedAppearance(); }

        hstring ToString()
        {
            return Name();
        }

        static void CopyInheritanceGraphs(CopyMap& visited, const std::vector<winrt::com_ptr<Profile>>& source, std::vector<winrt::com_ptr<Profile>>& target);
        winrt::com_ptr<Profile> CopyInheritanceGraph(CopyMap& visited) const;
        winrt::com_ptr<Profile> CopySettings() const { return FromNative(_native->CopySettings()); }
        winrt::com_ptr<Profile> CreateChild() const { return FromNative(_native->CreateChild()); }
        void ClearParents() { _native->ClearParents(); }
        void AddLeastImportantParent(const winrt::com_ptr<Profile>& parent) { _native->AddLeastImportantParent(parent->_native); }
        void AddMostImportantParent(const winrt::com_ptr<Profile>& parent) { _native->AddMostImportantParent(parent->_native); }
        std::vector<winrt::com_ptr<Profile>> Parents() const;

        static com_ptr<Profile> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        hstring EvaluatedStartingDirectory() const { return _native->EvaluatedStartingDirectory(); }

        Model::IAppearanceConfig DefaultAppearance() const { return Adapters::ToProjected(_native->DefaultAppearance()); }
        Model::FontConfig FontInfo() const { return Adapters::ToProjected(_native->FontInfo()); }

        static std::wstring NormalizeCommandLine(LPCWSTR commandLine) { return Native::Profile::NormalizeCommandLine(commandLine); }

        void _FinalizeInheritance() { _native->FinalizeInheritance(); }

        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const { _native->LogSettingChanges(changes, context); }

        void ResolveMediaResources(const Model::MediaResourceResolver& resolver);

        void Icon(const winrt::hstring& path)
        {
            // Internal Helper (overload version)
            Icon(MediaResource::FromString(path));
        }

        bool Deleted() const noexcept { return _native->Deleted(); }
        void Deleted(bool value) noexcept { _native->Deleted(value); }
        bool Orphaned() const noexcept { return _native->Orphaned(); }
        void Orphaned(bool value) noexcept { _native->Orphaned(value); }
        OriginTag Origin() const noexcept { return Adapters::ToProjected(_native->Origin()); }
        void Origin(OriginTag value) noexcept { _native->Origin(Adapters::ToNative(value)); }
        guid Updates() const noexcept { return _native->Updates(); }
        void Updates(guid value) noexcept { _native->Updates(value); }

        TSM_ADAPTER_PROPERTY(std::optional<Native::Color>, TabColor);
        TSM_ADAPTER_PROPERTY(winrt::com_ptr<Native::AppearanceConfig>, UnfocusedAppearance);

        TSM_ADAPTER_PROPERTY(winrt::hstring, Name);
        TSM_ADAPTER_PROPERTY(winrt::hstring, Source);
        TSM_ADAPTER_PROPERTY(bool, Hidden);
        TSM_ADAPTER_PROPERTY(winrt::guid, Guid);
        TSM_ADAPTER_PROPERTY(winrt::hstring, Padding);

        winrt::hstring SourceBasePath() const { return _native->SourceBasePath(); }
        void SourceBasePath(const winrt::hstring& value) { _native->SourceBasePath(value); }

    public:
#define PROFILE_SETTINGS_INITIALIZE(type, name, ...) TSM_ADAPTER_PROPERTY(type, name);
        NATIVE_PROFILE_SETTINGS(PROFILE_SETTINGS_INITIALIZE)
#undef PROFILE_SETTINGS_INITIALIZE

    private:
        winrt::com_ptr<Native::Profile> _native;

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
