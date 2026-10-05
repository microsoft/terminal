// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MediaResourceList.h"
#include "SettingsTypes.h"
#include "PropertyDeclarations.h"
#include "ProfileSettings.h"

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Json
{
    class Value;
}

namespace Microsoft::Terminal::Settings::Model::Native
{
    class AppearanceConfig;
    class FontConfig;

    using MediaResourceResolver = std::function<void(OriginTag, const winrt::hstring&, const winrt::com_ptr<MediaResource>&)>;

    class Profile : public winrt::implements<Profile, winrt::Windows::Foundation::IInspectable>
    {
        class ConstructionToken
        {
        public:
            ConstructionToken(const ConstructionToken&) = default;

        private:
            ConstructionToken() = default;
            friend class Profile;
        };

    public:
        TSM_NATIVE_LIFETIME_DECLARATIONS(Profile);
        TSM_NATIVE_API static winrt::com_ptr<Profile> Create();
        TSM_NATIVE_API static winrt::com_ptr<Profile> Create(winrt::guid guid);
        Profile(ConstructionToken);
        TSM_NATIVE_API ~Profile() noexcept;

        TSM_NATIVE_API winrt::com_ptr<Profile> CreateChild() const;
        TSM_NATIVE_API void ClearParents();
        TSM_NATIVE_API void AddLeastImportantParent(winrt::com_ptr<Profile> parent);
        TSM_NATIVE_API void AddMostImportantParent(winrt::com_ptr<Profile> parent);
        TSM_NATIVE_API std::span<const winrt::com_ptr<Profile>> Parents() const noexcept;
        TSM_NATIVE_API void FinalizeInheritance();

        TSM_NATIVE_API void CreateUnfocusedAppearance();
        TSM_NATIVE_API void DeleteUnfocusedAppearance();
        TSM_NATIVE_API winrt::com_ptr<AppearanceConfig> DefaultAppearance() const;
        TSM_NATIVE_API winrt::com_ptr<FontConfig> FontInfo() const;

        TSM_NATIVE_API static void CopyInheritanceGraphs(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited, const std::vector<winrt::com_ptr<Profile>>& source, std::vector<winrt::com_ptr<Profile>>& target);
        TSM_NATIVE_API winrt::com_ptr<Profile>& CopyInheritanceGraph(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited) const;
        TSM_NATIVE_API winrt::com_ptr<Profile> CopySettings() const;
        TSM_NATIVE_API static winrt::com_ptr<Profile> FromJson(const Json::Value& json);
        TSM_NATIVE_API void LayerJson(const Json::Value& json);
        TSM_NATIVE_API Json::Value ToJson() const;
        TSM_NATIVE_API winrt::hstring EvaluatedStartingDirectory() const;
        TSM_NATIVE_API static std::wstring NormalizeCommandLine(const wchar_t* commandLine);
        TSM_NATIVE_API void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;
        TSM_NATIVE_API void ResolveMediaResources(const MediaResourceResolver& resolver);

        TSM_NATIVE_API bool Deleted() const noexcept;
        TSM_NATIVE_API void Deleted(bool value) noexcept;
        TSM_NATIVE_API bool Orphaned() const noexcept;
        TSM_NATIVE_API void Orphaned(bool value) noexcept;
        TSM_NATIVE_API OriginTag Origin() const noexcept;
        TSM_NATIVE_API void Origin(OriginTag value) noexcept;
        TSM_NATIVE_API winrt::guid Updates() const noexcept;
        TSM_NATIVE_API void Updates(winrt::guid value) noexcept;
        TSM_NATIVE_API winrt::hstring SourceBasePath() const;
        TSM_NATIVE_API void SourceBasePath(const winrt::hstring& value);
        TSM_NATIVE_API void Icon(const winrt::hstring& path);

        TSM_NATIVE_PROPERTY(Profile, std::optional<Native::Color>, TabColor);
        TSM_NATIVE_PROPERTY(Profile, winrt::com_ptr<Native::AppearanceConfig>, UnfocusedAppearance);
        TSM_NATIVE_PROPERTY(Profile, winrt::hstring, Name);
        TSM_NATIVE_PROPERTY(Profile, winrt::hstring, Source);
        TSM_NATIVE_PROPERTY(Profile, bool, Hidden);
        TSM_NATIVE_PROPERTY(Profile, winrt::guid, Guid);
        TSM_NATIVE_PROPERTY(Profile, winrt::hstring, Padding);

#define DECLARE_NATIVE_PROFILE_PROPERTY(type, name, ...) TSM_NATIVE_PROPERTY(Profile, type, name);
        NATIVE_PROFILE_SETTINGS(DECLARE_NATIVE_PROFILE_PROPERTY)
#undef DECLARE_NATIVE_PROFILE_PROPERTY

    private:
        using base_type = winrt::implements<Profile, winrt::Windows::Foundation::IInspectable>;

        static std::wstring EvaluateStartingDirectory(const std::wstring& directory);
        static winrt::guid _GenerateGuidForProfile(const std::wstring_view& name, const std::wstring_view& source) noexcept;
        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, bool isSet);

        std::vector<winrt::com_ptr<Profile>> _parents;
        winrt::com_ptr<AppearanceConfig> _DefaultAppearance;
        winrt::com_ptr<FontConfig> _FontInfo;
        std::set<std::string> _changeLog;
        winrt::hstring _SourceBasePath;
        winrt::guid _Updates{};
        OriginTag _Origin{ OriginTag::None };
        bool _Deleted = false;
        bool _Orphaned = false;
    };
}
